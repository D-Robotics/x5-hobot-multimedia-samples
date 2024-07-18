#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "communicate/sdk_communicate.h"

#include "utils/utils_log.h"
#include "utils/mthread.h"
#include "utils/time_utils.h"

#include "bpu_wrap.h"
#include "yolov5_post_process.h"
#include "fcos_post_process.h"

/**
 * Align by 16
 */
#define ALIGN_16(v) ((v + (16 - 1)) / 16 * 16)

#define HB_CHECK_SUCCESS(value, errmsg)                                \
	do {                                                               \
		/*value can be call of function*/                                  \
		int32_t ret_code = value;                                           \
		if (ret_code != 0) {                                             \
			SC_LOGE("[BPU ERROR] %s, error code:%d", errmsg, ret_code); \
			return (ret_code);                                               \
		}                                                                \
	} while (0);

void print_bpu_buffer_info(const bpu_buffer_info_t *buffer_info)
{
	printf("=== Bpu Buffer Info ===");
	if (!buffer_info) {
		printf("Buffer info pointer is NULL\n");
		return;
	}

	printf("Plane Count: %d\n", buffer_info->plane_cnt);
	printf("Width: %d\n", buffer_info->width);
	printf("Height: %d\n", buffer_info->height);
	printf("Width Stride: %d\n", buffer_info->w_stride);
	printf("tv: %ld.%06ld\n", buffer_info->tv.tv_sec, buffer_info->tv.tv_usec);

	for (int i = 0; i < buffer_info->plane_cnt; ++i) {
		printf("Address of Plane %d: %p\n", i, (void *)buffer_info->addr[i]);
		printf("Physical Address of Plane %d: %lu\n", i, buffer_info->paddr[i]);
	}
}

static int32_t print_model_info(hbPackedDNNHandle_t *packed_dnn_handle)
{
	int32_t i = 0, j = 0;
	hbDNNHandle_t dnn_handle;
	const char **model_name_list;
	int32_t model_count = 0;
	hbDNNTensorProperties properties;

	HB_CHECK_SUCCESS(hbDNNGetModelNameList(
		&model_name_list, &model_count, packed_dnn_handle),
		"hbDNNGetModelNameList failed");
	if (model_count <= 0) {
		printf("Modle count <= 0\n");
		return -1;
	}
	HB_CHECK_SUCCESS(
		hbDNNGetModelHandle(&dnn_handle, packed_dnn_handle, model_name_list[0]),
		"hbDNNGetModelHandle failed");

	printf("Model info:\nmodel_name: %s\n", model_name_list[0]);

	int32_t input_count = 0;
	int32_t output_count = 0;
	HB_CHECK_SUCCESS(hbDNNGetInputCount(&input_count, dnn_handle),
					 "hbDNNGetInputCount failed");
	HB_CHECK_SUCCESS(hbDNNGetOutputCount(&output_count, dnn_handle),
					 "hbDNNGetInputCount failed");

	printf("Input count: %d\n", input_count);
	for (i = 0; i < input_count; i++) {
		HB_CHECK_SUCCESS(
			hbDNNGetInputTensorProperties(&properties, dnn_handle, i),
			"hbDNNGetInputTensorProperties failed");

		printf("input[%d]: tensorLayout: %d tensorType: %d validShape:(",
			i, properties.tensorLayout, properties.tensorType);
		for (j = 0; j < properties.validShape.numDimensions; j++)
			printf("%d, ", properties.validShape.dimensionSize[j]);
		printf("), ");
		printf("alignedShape:(");
		for (j = 0; j < properties.alignedShape.numDimensions; j++)
			printf("%d, ", properties.alignedShape.dimensionSize[j]);
		printf(")\n");
	}

	printf("Output count: %d\n", output_count);
	for (i = 0; i < output_count; i++) {
		HB_CHECK_SUCCESS(
			hbDNNGetOutputTensorProperties(&properties, dnn_handle, i),
			"hbDNNGetOutputTensorProperties failed");
		printf("Output[%d]: tensorLayout: %d tensorType: %d validShape:(",
			i, properties.tensorLayout, properties.tensorType);
		for (j = 0; j < properties.validShape.numDimensions; j++)
			printf("%d, ", properties.validShape.dimensionSize[j]);
		printf("), ");
		printf("alignedShape:(");
		for (j = 0; j < properties.alignedShape.numDimensions; j++)
			printf("%d, ", properties.alignedShape.dimensionSize[j]);
		printf(")\n");
	}

	return 0;
}

static int32_t prepare_output_tensor(hbDNNTensor *output_tensor,
						   hbDNNHandle_t dnn_handle) {
	int32_t ret = 0;

	int32_t i = 0;
	int32_t output_count = 0;
	// hbDNNTensorProperties properties;
	hbDNNTensor *output = output_tensor;

	hbDNNGetOutputCount(&output_count, dnn_handle);
	for (i = 0; i < output_count; ++i) {
	HB_CHECK_SUCCESS(
			hbDNNGetOutputTensorProperties(&output[i].properties, dnn_handle, i),
			"hbDNNGetOutputTensorProperties failed");
	HB_CHECK_SUCCESS(hbSysAllocCachedMem(&output[i].sysMem[0], output[i].properties.alignedByteSize),
			"hbSysAllocCachedMem failed");
	}

  return ret;
}

static int32_t release_output_tensor(hbDNNTensor *output, int32_t len)
{
	for (int32_t i = 0; i < len; i++) {
		HB_CHECK_SUCCESS(hbSysFreeMem(&(output[i].sysMem[0])),
					   "hbSysFreeMem failed");
	}
	return 0;
}

static void *post_process_yolov5s(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	Yolov5PostProcessInfo_t *post_info;

	int count = 0;
	mThreadSetName(privThread, __func__);

	SC_LOGI("thread [post_process_yolov5s] start .");
	struct TimeStatistics time_statistics;
	bpu_handle_t *bpu_handle = (bpu_handle_t *)privThread->pvThreadData;
	while (privThread->eState == E_THREAD_RUNNING) {
		if (mQueueDequeueTimed(&bpu_handle->m_output_queue, 100, (void**)&post_info) != E_QUEUE_OK){
			SC_LOGI("post_process_yolov5s wait queue time out.");
			continue;
		}

		time_statistics_at_beginning_of_loop(&time_statistics);
		char *results = Yolov5PostProcess(post_info);
		time_statistics_at_ending_of_loop(&time_statistics);
		time_statistics_info_show(&time_statistics, "yolov5 post process", false);
		if (results) {
			if (NULL != bpu_handle->callback) {

				{
					int pipeline_id = -1;
					if (bpu_handle->m_userdata)
						pipeline_id = *(int*)bpu_handle->m_userdata;

					if(count % 3300 == 0){
						SC_LOGD("[%d] inference:[%s]", pipeline_id, results);
					}
					count++;
				}
				bpu_handle->callback(results, bpu_handle->m_userdata);
			} else {
				SC_LOGI("%s", results);
			}
			free(results);
		}
		if (post_info) {
			free(post_info);
			post_info = NULL;
		}
	}
	SC_LOGI("thread [post_process_yolov5s] stop .");
	mThreadFinish(privThread);
	return NULL;
}


static void *inference_yolov5s(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	int32_t i = 0, ret = 0;
	bpu_tensor_info_t *input_tensor;
	int32_t output_count = 0;

	bpu_handle_t *bpu_handle = (bpu_handle_t *)privThread->pvThreadData;

	mThreadSetName(privThread, __func__);

	if (bpu_handle == NULL)
		goto exit;

	hbPackedDNNHandle_t packed_dnn_handle = bpu_handle->m_packed_dnn_handle;
	hbDNNHandle_t dnn_handle = bpu_handle->m_dnn_handle;

	SC_LOGI("packed_dnn_handle: %p, dnn_handle: %p", packed_dnn_handle, dnn_handle);

	hbDNNGetOutputCount(&output_count, dnn_handle);

	// 准备模型输出节点tensor，5组输出buff轮转，简单处理，理论上后处理的速度是要比算法推理更快的
	hbDNNTensor output_tensors[5][3];
	int32_t cur_ouput_buf_idx = 0;
	for (i = 0; i < 5; i++) {
		ret = prepare_output_tensor(output_tensors[i], dnn_handle);
		if (ret) {
			SC_LOGE("prepare model output tensor failed");
			goto exit;
		}
	}

	hbDNNTaskHandle_t task_handle = NULL;

	struct TimeStatistics time_statistics;
	char time_sts_tag[32];
	sprintf(time_sts_tag, "yolov5 infer process:%d", bpu_handle->m_vpp_id);

	while (privThread->eState == E_THREAD_RUNNING) {

		if (mQueueDequeueTimed(&bpu_handle->m_input_queue, 100, (void**)&input_tensor) != E_QUEUE_OK)
			continue;

		time_statistics_at_beginning_of_loop(&time_statistics);
		// make sure memory data is flushed to DDR before inference
		hbSysFlushMem(&input_tensor->m_dnn_tensor.sysMem[0], HB_SYS_MEM_CACHE_CLEAN);

		hbDNNTensor *output = &output_tensors[cur_ouput_buf_idx][0];

		// 模型推理infer
		hbDNNInferCtrlParam infer_ctrl_param;
		HB_DNN_INITIALIZE_INFER_CTRL_PARAM(&infer_ctrl_param);
		ret = hbDNNInfer(&task_handle,
				&output,
				&input_tensor->m_dnn_tensor,
				dnn_handle,
				&infer_ctrl_param);
		if (ret) {
			SC_LOGE("hbDNNInfer failed");
			break;
		}
		// wait task done
		ret = hbDNNWaitTaskDone(task_handle, 0);
		if (ret) {
			SC_LOGE("hbDNNWaitTaskDone failed");
			break;
		}

		// make sure CPU read data from DDR before using output tensor data
		for (int32_t i = 0; i < output_count; i++) {
			hbSysFlushMem(&output_tensors[cur_ouput_buf_idx][i].sysMem[0], HB_SYS_MEM_CACHE_INVALIDATE);
		}

		// release task handle
		ret = hbDNNReleaseTask(task_handle);
		if (ret) {
			SC_LOGE("hbDNNReleaseTask failed");
			break;
		}
		task_handle = NULL;
		time_statistics_at_ending_of_loop(&time_statistics);
		time_statistics_info_show(&time_statistics, time_sts_tag, false);

		// 如果后处理队列满的，直接返回
		if (mQueueIsFull(&bpu_handle->m_output_queue)) {
			SC_LOGI("post process queue full, skip it, queue length is %d",
				bpu_handle->m_output_queue.u32Length);
			cur_ouput_buf_idx++;
			cur_ouput_buf_idx %= 5;
			continue;
		}

		// 后处理数据
		Yolov5PostProcessInfo_t *post_info;
		post_info = (Yolov5PostProcessInfo_t *)malloc(sizeof(Yolov5PostProcessInfo_t));
		if (NULL == post_info) {
			SC_LOGE("Failed to allocate memory for post_info");
			continue;
		}
		post_info->is_pad_resize = 0;
		post_info->score_threshold = 0.3;
		post_info->nms_threshold = 0.45;
		post_info->nms_top_k = 500;
		post_info->width = bpu_handle->m_image_info.m_model_w;
		post_info->height = bpu_handle->m_image_info.m_model_h;
		post_info->ori_width = bpu_handle->m_image_info.m_ori_width;
		post_info->ori_height = bpu_handle->m_image_info.m_ori_height;
		post_info->tv = input_tensor->tv;
		post_info->output_tensor = output_tensors[cur_ouput_buf_idx];
		mQueueEnqueue(&bpu_handle->m_output_queue, post_info);
		cur_ouput_buf_idx++;
		cur_ouput_buf_idx %= 5;


	}

	for (i = 0; i < 5; i++)
		release_output_tensor(output_tensors[i], 3);	// 释放模型输出资源
exit:
	mThreadFinish(privThread);
	return NULL;
}


static void *post_process_fcos(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	FcosPostProcessInfo_t *post_info;

	mThreadSetName(privThread, __func__);

	bpu_handle_t *bpu_handle = (bpu_handle_t *)privThread->pvThreadData;
	while (privThread->eState == E_THREAD_RUNNING) {
		if (mQueueDequeueTimed(&bpu_handle->m_output_queue, 100, (void**)&post_info) != E_QUEUE_OK)
			continue;

		char *results = FcosPostProcess(post_info);

		if (results) {
			if (NULL != bpu_handle->callback) {
				bpu_handle->callback(results, bpu_handle->m_userdata);
			} else {
				SC_LOGI("%s", results);
			}
			free(results);
		}

		if (post_info) {
			free(post_info);
			post_info = NULL;
		}
	}
	mThreadFinish(privThread);
	return NULL;
}

static void *inference_fcos(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	int32_t i = 0, ret = 0;
	bpu_tensor_info_t *input_tensor;
	int32_t output_count = 0;

	bpu_handle_t *bpu_handle = (bpu_handle_t *)privThread->pvThreadData;

	mThreadSetName(privThread, __func__);

	if (bpu_handle == NULL)
		goto exit;

	hbPackedDNNHandle_t packed_dnn_handle = bpu_handle->m_packed_dnn_handle;
	hbDNNHandle_t dnn_handle = bpu_handle->m_dnn_handle;

	hbDNNGetOutputCount(&output_count, dnn_handle);

	SC_LOGI("packed_dnn_handle: %p, dnn_handle: %p output count:%d.", packed_dnn_handle, dnn_handle, output_count);
	// 准备模型输出节点tensor，5组输出buff轮转，简单处理，理论上后处理的速度是要比算法推理更快的
	hbDNNTensor output_tensors[5][15];
	int32_t cur_ouput_buf_idx = 0;
	for (i = 0; i < 5; i++) {
		ret = prepare_output_tensor(output_tensors[i], dnn_handle);
		if (ret) {
			SC_LOGE("prepare model output tensor failed");
			goto exit;
		}
	}

	hbDNNTaskHandle_t task_handle = NULL;

	while (privThread->eState == E_THREAD_RUNNING) {
		if (mQueueDequeueTimed(&bpu_handle->m_input_queue, 100, (void**)&input_tensor) != E_QUEUE_OK)
			continue;

		// make sure memory data is flushed to DDR before inference
		hbSysFlushMem(&input_tensor->m_dnn_tensor.sysMem[0], HB_SYS_MEM_CACHE_CLEAN);

		hbDNNTensor *output = &output_tensors[cur_ouput_buf_idx][0];

		// 模型推理infer
		hbDNNInferCtrlParam infer_ctrl_param;
		HB_DNN_INITIALIZE_INFER_CTRL_PARAM(&infer_ctrl_param);
		ret = hbDNNInfer(&task_handle,
				&output,
				&input_tensor->m_dnn_tensor,
				dnn_handle,
				&infer_ctrl_param);
		if (ret) {
			SC_LOGE("hbDNNInfer failed");
			break;
		}
		// wait task done
		ret = hbDNNWaitTaskDone(task_handle, 0);
		if (ret) {
			SC_LOGE("hbDNNWaitTaskDone failed");
			break;
		}

		// make sure CPU read data from DDR before using output tensor data
		for (int32_t i = 0; i < output_count; i++) {
			hbSysFlushMem(&output_tensors[cur_ouput_buf_idx][i].sysMem[0], HB_SYS_MEM_CACHE_INVALIDATE);
		}

		// release task handle
		ret = hbDNNReleaseTask(task_handle);
		if (ret) {
			SC_LOGE("hbDNNReleaseTask failed");
			break;
		}
		task_handle = NULL;

		// 如果后处理队列满的，直接返回
		if (mQueueIsFull(&bpu_handle->m_output_queue)) {
			SC_LOGI("post process queue full, skip it");
			cur_ouput_buf_idx++;
			cur_ouput_buf_idx %= 5;
			continue;
		}

		// 后处理数据
		FcosPostProcessInfo_t *post_info;
		post_info = (FcosPostProcessInfo_t *)malloc(sizeof(FcosPostProcessInfo_t));
		if (NULL == post_info) {
			SC_LOGE("Failed to allocate memory for post_info");
			continue;
		}
		post_info->is_pad_resize = 0;
		post_info->score_threshold = 0.5;
		post_info->nms_threshold = 0.6;
		post_info->nms_top_k = 500;
		post_info->width = bpu_handle->m_image_info.m_model_w;
		post_info->height = bpu_handle->m_image_info.m_model_h;
		post_info->ori_width = bpu_handle->m_image_info.m_ori_width;
		post_info->ori_height = bpu_handle->m_image_info.m_ori_height;
		post_info->tv = input_tensor->tv;
		post_info->output_tensor = output_tensors[cur_ouput_buf_idx];
		mQueueEnqueue(&bpu_handle->m_output_queue, post_info);
		cur_ouput_buf_idx++;
		cur_ouput_buf_idx %= 5;
	}

	for (i = 0; i < 5; i++)
		release_output_tensor(output_tensors[i], output_count);	// 释放模型输出资源
exit:
	mThreadFinish(privThread);
	return NULL;
}

// 解析分类结果
static void parse_classification_result(
		hbDNNTensor *tensor,
		int32_t *idx,
		float *score_top1) {

	float *scores = (float *) (tensor->sysMem[0].virAddr);
	int32_t *shape = tensor->properties.validShape.dimensionSize;
	for (int32_t i = 0; i < shape[1] * shape[2] * shape[3]; i++) {
		float score = scores[i];
		if (score > *score_top1){
			*idx =  i;
			*score_top1 = score;
		}
	}
}

static void *inference_mobilenetv2(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	int32_t ret = 0;
	bpu_tensor_info_t *input_tensor;
	int32_t output_count = 0;

	bpu_handle_t *bpu_handle = (bpu_handle_t *)privThread->pvThreadData;

	mThreadSetName(privThread, __func__);

	if (bpu_handle == NULL)
		goto exit;

	hbPackedDNNHandle_t packed_dnn_handle = bpu_handle->m_packed_dnn_handle;
	hbDNNHandle_t dnn_handle = bpu_handle->m_dnn_handle;

	SC_LOGI("packed_dnn_handle: %p, dnn_handle: %p", packed_dnn_handle, dnn_handle);

	hbDNNGetOutputCount(&output_count, dnn_handle);

	// 准备模型输出节点tensor
	hbDNNTensor output_tensors[1];
	ret = prepare_output_tensor(output_tensors, dnn_handle);
	if (ret) {
		printf("prepare model output tensor failed\n");
		goto exit;
	}

	hbDNNTaskHandle_t task_handle = NULL;
	hbDNNTensor *output = &output_tensors[0];

	while (privThread->eState == E_THREAD_RUNNING) {
		if (mQueueDequeueTimed(&bpu_handle->m_input_queue, 100, (void**)&input_tensor) != E_QUEUE_OK)
			continue;

		// make sure memory data is flushed to DDR before inference
		hbSysFlushMem(&input_tensor->m_dnn_tensor.sysMem[0], HB_SYS_MEM_CACHE_CLEAN);

		// 模型推理infer
		hbDNNInferCtrlParam infer_ctrl_param;
		HB_DNN_INITIALIZE_INFER_CTRL_PARAM(&infer_ctrl_param);
		ret = hbDNNInfer(&task_handle,
				&output,
				&input_tensor->m_dnn_tensor,
				dnn_handle,
				&infer_ctrl_param);
		if (ret) {
			SC_LOGE("hbDNNInfer failed");
			break;
		}
		// wait task done
		ret = hbDNNWaitTaskDone(task_handle, 0);
		if (ret) {
			SC_LOGE("hbDNNWaitTaskDone failed");
			break;
		}

		// make sure CPU read data from DDR before using output tensor data
		for (int32_t i = 0; i < output_count; i++) {
			hbSysFlushMem(&output_tensors[i].sysMem[0], HB_SYS_MEM_CACHE_INVALIDATE);
		}

		// release task handle
		ret = hbDNNReleaseTask(task_handle);
		if (ret) {
			SC_LOGE("hbDNNReleaseTask failed");
			break;
		}
		task_handle = NULL;

		// 同步模式下的后处理, 测试用，每个模型都要一份独立的后处理接口
		float score_top1 = 0.0;
		int32_t idx = 0;
		char result[128] = {0};
		parse_classification_result(
				&output_tensors[0], &idx, &score_top1);

		// 通过websocket把算法结果发送给web页面
		sprintf(result, "\"timestamp\": %ld,\"classification_result\": \"id=%d, score=%.3f\"",
			input_tensor->tv.tv_sec * 1000000 + input_tensor->tv.tv_usec,
			idx, score_top1);

		if (NULL != bpu_handle->callback) {
			bpu_handle->callback(result, bpu_handle->m_userdata);
		} else {
			SC_LOGI("%s", result);
		}
	}

	ret = hbSysFreeMem(&(output_tensors[0].sysMem[0]));			  // 释放模型输出资源
	if (ret)
		printf("output data free failed\n");
exit:
	mThreadFinish(privThread);
	return NULL;
}

// 定义模型描述符数组
bpu_model_descriptor bpu_models[] = {
	{
		.model_name = "null",
		.model_path = "",
		.inference_func = NULL,
		.post_proc_func = NULL
	},
	{
		.model_name = "mobilenetv2",
		.model_path = "../model_zoom/mobilenetv2_224x224_nv12.bin",
		.inference_func = inference_mobilenetv2,
		.post_proc_func = NULL
	},
	{
		.model_name = "yolov5s",
		.model_path = "../model_zoom/yolov5s_672x672_nv12.bin",
		.inference_func = inference_yolov5s,
		.post_proc_func = post_process_yolov5s
	},
	{
		.model_name = "fcos",
		.model_path = "../model_zoom/fcos_efficientnetb0_512x512_nv12.bin",
		.inference_func = inference_fcos,
		.post_proc_func = post_process_fcos
	},
};

int32_t bpu_wrap_get_model_list(char *model_list)
{
	// 初始化模型列表字符串
	// 将每个模型的名称添加到模型列表字符串中
	for (int i = 0; i < sizeof(bpu_models) / sizeof(bpu_models[0]); ++i) {
		strcat(model_list, bpu_models[i].model_name);
		if (i < sizeof(bpu_models) / sizeof(bpu_models[0]) - 1) {
			strcat(model_list, "/");
		}
	}
	SC_LOGI("model list [%s]", model_list);
	return 0;
}

static int32_t bpu_wrap_get_modle_prop(char *model_file_name, hbDNNTensorProperties *properties)
{
	int32_t ret = 0;
	const char **model_name_list;
	int32_t model_count = 0;
	hbPackedDNNHandle_t packed_dnn_handle;
	hbDNNHandle_t dnn_handle;

	// 加载模型
	HB_CHECK_SUCCESS(
		hbDNNInitializeFromFiles(&packed_dnn_handle, (char const **)&model_file_name, 1),
		"hbDNNInitializeFromFiles failed"); // 从本地文件加载模型

	HB_CHECK_SUCCESS(hbDNNGetModelNameList(
		&model_name_list, &model_count, packed_dnn_handle),
		"hbDNNGetModelNameList failed");

	if (model_count <= 0) {
		printf("Modle count <= 0\n");
		return -1;
	}

	HB_CHECK_SUCCESS(
		hbDNNGetModelHandle(&dnn_handle, packed_dnn_handle, model_name_list[0]),
		"hbDNNGetModelHandle failed");

	// 获取模型属性信息
	HB_CHECK_SUCCESS(
			hbDNNGetInputTensorProperties(properties, dnn_handle, 0),
			"hbDNNGetInputTensorProperties failed");

	HB_CHECK_SUCCESS(hbDNNRelease(packed_dnn_handle), "hbDNNRelease failed");

	return ret;
}

int32_t bpu_wrap_init(bpu_handle_t *bpu_handle, char *model_file_name, char *model_name)
{
	int32_t ret = 0, i = 0;
	const char **model_name_list;
	int32_t model_count = 0;
	hbDNNTensorProperties properties;
	hbPackedDNNHandle_t packed_dnn_handle;
	hbDNNHandle_t dnn_handle;

	if (NULL == bpu_handle) {
		SC_LOGE("bpu_handle is NULL");
		return -1;
	}

	SC_LOGI("model_file_name[%s]  model_name [%s] %d\n", model_file_name, model_name, strlen(model_name));
	if(strlen(model_name) < (sizeof(bpu_handle->m_model_name) - 1)){
		strcpy(bpu_handle->m_model_name, model_name);
	}else{
		SC_LOGE("model_name [%s] is too long :%d\n", strlen(model_name) + 1);
		exit(-1);
	}

	// 加载模型
	HB_CHECK_SUCCESS(
		hbDNNInitializeFromFiles(&packed_dnn_handle, (char const **)&model_file_name, 1),
		"hbDNNInitializeFromFiles failed"); // 从本地文件加载模型

	// 打印模型信息
	print_model_info(packed_dnn_handle);

	HB_CHECK_SUCCESS(hbDNNGetModelNameList(
		&model_name_list, &model_count, packed_dnn_handle),
		"hbDNNGetModelNameList failed");

	if (model_count <= 0) {
		printf("Modle count <= 0\n");
		return -1;
	}
	SC_LOGI("model_name_list[0]:%s", model_name_list[0]);

	HB_CHECK_SUCCESS(
		hbDNNGetModelHandle(&dnn_handle, packed_dnn_handle, model_name_list[0]),
		"hbDNNGetModelHandle failed");

	bpu_handle->m_packed_dnn_handle = packed_dnn_handle;
	bpu_handle->m_dnn_handle = dnn_handle;
	SC_LOGI("packed_dnn_handle: %p, dnn_handle: %p", packed_dnn_handle, dnn_handle);

	SC_LOGI("Model info:\nmodel_name: %s", model_name_list[0]);

	// 获取模型相关信息
	// 目前模型输入的yuv都按照nv12格式处理，其他格式先不做考虑
	HB_CHECK_SUCCESS(
			hbDNNGetInputTensorProperties(&properties, dnn_handle, 0),
			"hbDNNGetInputTensorProperties failed");
	hbDNNTensorShape  *input_tensor_shape = &properties.validShape;	 // 获取模型输入shape
	bpu_handle->m_image_info.m_model_h = (input_tensor_shape->dimensionSize)[2];
	bpu_handle->m_image_info.m_model_w = (input_tensor_shape->dimensionSize)[3];
	SC_LOGI("get model input_tensor_shape ok, NCHW = (1, 3, %d, %d)",
		bpu_handle->m_image_info.m_model_h, bpu_handle->m_image_info.m_model_w);		// 打印从模型中读取的宽高

	// 设置默认的原始图像宽高为 1920 * 1080
	// 如果原始图像时4K/2K或者其他分辨率，请调用bpu_wrap_set_ori_hw接口重新设置
	bpu_handle->m_image_info.m_ori_height = 1080;
	bpu_handle->m_image_info.m_ori_width = 1920;

	// 队列中存2个，解决算法结果延迟较大的问题
	mQueueCreate(&bpu_handle->m_input_queue, 2);//the length of queue is 2
	mQueueCreate(&bpu_handle->m_output_queue, 3);//the length of queue is 3

	// 分配 bpu input buffer 使用的内存
	bpu_handle->m_cur_input_tensor = 0;
	for (i = 0; i < BPU_INPUT_BUFFER_NUM; i++) {
		HB_CHECK_SUCCESS(hbSysAllocCachedMem(&bpu_handle->m_input_tensors[i].m_dnn_tensor.sysMem[0],
			bpu_handle->m_image_info.m_model_h * bpu_handle->m_image_info.m_model_w),
			"hbSysAllocCachedMem failed");
		HB_CHECK_SUCCESS(hbSysAllocCachedMem(&bpu_handle->m_input_tensors[i].m_dnn_tensor.sysMem[1],
			bpu_handle->m_image_info.m_model_h * bpu_handle->m_image_info.m_model_w / 2),
			"hbSysAllocCachedMem failed");
	}

	return ret;
}

int32_t bpu_wrap_deinit(bpu_handle_t *handle)
{
	int32_t ret = 0;
	int32_t i = 0;

	if (handle == NULL)
		return 0;

	for (i = 0; i < BPU_INPUT_BUFFER_NUM; i++) {
		ret = hbSysFreeMem(&handle->m_input_tensors[i].m_dnn_tensor.sysMem[0]);	   // 释放模型输入资源
		ret |= hbSysFreeMem(&handle->m_input_tensors[i].m_dnn_tensor.sysMem[1]);
		if (ret)
			SC_LOGE("input data free failed");
	}
	// 销毁队列
	mQueueDestroy(&handle->m_output_queue);
	mQueueDestroy(&handle->m_input_queue);

	// 释放模型资源
	HB_CHECK_SUCCESS(hbDNNRelease(handle->m_packed_dnn_handle), "hbDNNRelease failed");

	SC_LOGI("successful");

	return ret;
}

// 初始化算法模型
int32_t bpu_wrap_model_init(bpu_handle_t *bpu_handle, char *model_name)
{
	// 遍历模型描述符数组
	for (int i = 0; i < sizeof(bpu_models) / sizeof(bpu_models[0]); ++i) {
		// 检查是否找到匹配的模型名称
		if (strcmp(model_name, bpu_models[i].model_name) == 0) {
			// 找到匹配的模型名称，使用对应的模型文件路径进行初始化
			return bpu_wrap_init(bpu_handle, bpu_models[i].model_path, model_name);
		}
	}
	// 如果未找到匹配的模型名称，打印错误信息并返回错误码
	SC_LOGE("Unsupported model name: %s", model_name);
	return -1;
}

void bpu_wrap_set_ori_hw(bpu_handle_t *handle, int32_t width, int32_t height)
{
	if (handle == NULL) return;

	handle->m_image_info.m_ori_height = height;
	handle->m_image_info.m_ori_width = width;
}

int32_t bpu_wrap_get_model_hw(char *model_name, int32_t *width, int32_t *height)
{
	hbDNNTensorProperties properties = {0};

	// 遍历模型描述符数组
	for (int i = 0; i < sizeof(bpu_models) / sizeof(bpu_models[0]); ++i) {
		// 检查是否找到匹配的模型名称
		if (strcmp(model_name, bpu_models[i].model_name) == 0) {
			bpu_wrap_get_modle_prop(bpu_models[i].model_path, &properties);
			hbDNNTensorShape  *input_tensor_shape = &properties.validShape;	 // 获取模型输入shape
			*width = (input_tensor_shape->dimensionSize)[2];
			*height = (input_tensor_shape->dimensionSize)[3];
			SC_LOGI("get model input_tensor_shape shape, NCHW = (1, 3, %d, %d)",
				*height, *width);
			return 0;
		}
	}
	// 如果未找到匹配的模型名称，打印错误信息并返回错误码
	SC_LOGE("Unsupported model name: %s", model_name);
	return -1;
}

int32_t bpu_wrap_start(bpu_handle_t *handle)
{
	if (handle == NULL)
		return -1;

	// 遍历模型描述符数组
	for (int i = 0; i < sizeof(bpu_models) / sizeof(bpu_models[0]); ++i) {
		// 检查是否找到匹配的模型名称
		if (strcmp(handle->m_model_name, bpu_models[i].model_name) == 0) {
			if (bpu_models[i].inference_func != NULL) {
				handle->m_run_model_thread.pvThreadData = (void *)handle;
				mThreadStart(bpu_models[i].inference_func, &handle->m_run_model_thread, E_THREAD_JOINABLE);
			}
			if (bpu_models[i].post_proc_func != NULL) {
				handle->m_post_process_thread.pvThreadData = (void *)handle;
				mThreadStart(bpu_models[i].post_proc_func, &handle->m_post_process_thread, E_THREAD_JOINABLE);
			}
			return 0;
		}
	}
	// 如果未找到匹配的模型名称，打印错误信息并返回错误码
	SC_LOGE("Unsupported model name: %s", handle->m_model_name);
	return -1;
}

int32_t bpu_wrap_stop(bpu_handle_t *handle)
{
	SC_LOGI("bpu_wrap_stop start .");
	if (handle == NULL){
		SC_LOGE("bpu_wrap_stop failed, handle is null.");
		return 0;
	}

	mThreadStop(&handle->m_post_process_thread);
	mThreadStop(&handle->m_run_model_thread);
	SC_LOGI("bpu_wrap_stop complete .");

	return 0;
}

// 完成输入数据前处理
// 处理完的数据推入算法推理队列
// #define BPU_DEBUG 1
int32_t bpu_wrap_send_frame(bpu_handle_t *handle, bpu_buffer_info_t *input_buffer)
{
#ifdef BPU_DEBUG // test
	char nv12_file_name[128];
	static int32_t nv12_index = 0;
#endif

	if (handle == NULL || input_buffer == NULL)
		return 0;

	// 如果队列满的，直接返回
	if (mQueueIsFull(&handle->m_input_queue)) {
		SC_LOGW("input queue full, skip it");
		return 0;
	}

	// print_bpu_buffer_info(input_buffer);

#ifdef BPU_DEBUG
extern int32_t vp_dump_2plane_yuv_to_file(char *filename, uint8_t *src_buffer, uint8_t *src_buffer1,
		uint32_t size, uint32_t size1);
	if (nv12_index++ % 100 == 0) {
		sprintf(nv12_file_name, "/tmp/bpu_%dx%d_nv12_input_%04d.yuv",
			input_buffer->width, input_buffer->height,
			nv12_index++);
		vp_dump_2plane_yuv_to_file(nv12_file_name,
			input_buffer->addr[0], input_buffer->addr[1],
			input_buffer->width * input_buffer->height,
			input_buffer->width * input_buffer->height / 2);
	}
#endif

	// 准备输入数据（用于存放yuv数据）
	hbDNNTensor *input_tensor = &handle->m_input_tensors[handle->m_cur_input_tensor].m_dnn_tensor;
	handle->m_input_tensors[handle->m_cur_input_tensor].tv = input_buffer->tv;

	input_tensor->properties.tensorLayout = HB_DNN_LAYOUT_NCHW;
	// 张量类型为Y通道及UV通道为输入的图片, 方便直接使用 vpu出来的y和uv分离的数据
	input_tensor->properties.tensorType = HB_DNN_IMG_TYPE_NV12_SEPARATE; // 用于Y和UV分离的场景，主要为我们摄像头数据通路场景
	// 填充 input_tensor->sysMem 成员变量 Y 分量
	hbSysWriteMem(&input_tensor->sysMem[0],
		(char *)input_buffer->addr[0],
		input_buffer->w_stride * input_buffer->height);
	input_tensor->sysMem[0].memSize = input_buffer->w_stride * input_buffer->height;
	// 填充 input_tensor->data_ext 成员变量， UV 分量
	hbSysWriteMem(&input_tensor->sysMem[1],
		(char *)input_buffer->addr[1],
		(input_buffer->w_stride * input_buffer->height) / 2);
	input_tensor->sysMem[1].memSize = (input_buffer->w_stride * input_buffer->height) / 2;

	// HB_DNN_IMG_TYPE_NV12_SEPARATE 类型的 layout 为 (1, 3, h, w)
	input_tensor->properties.validShape.numDimensions = 4;
	input_tensor->properties.validShape.dimensionSize[0] = 1;						// N
	input_tensor->properties.validShape.dimensionSize[1] = 3;						// C
	input_tensor->properties.validShape.dimensionSize[2] = input_buffer->height;		// H
	input_tensor->properties.validShape.dimensionSize[3] = ALIGN_16(input_buffer->w_stride); 	// W
	input_tensor->properties.alignedShape = input_tensor->properties.validShape;		// 已满足跨距对齐要求，直接赋值

#ifdef BPU_DEBUG
	SC_LOGI("input_tensor(%p): tensorLayout: %d tensorType: %d validShape:(", input_tensor,
			input_tensor->properties.tensorLayout, input_tensor->properties.tensorType);
		int32_t j = 0;
		for (j = 0; j < input_tensor->properties.validShape.numDimensions; j++)
			printf("%d, ", input_tensor->properties.validShape.dimensionSize[j]);
		printf(")\n");
#endif

	// 把图像数据推送进BPU处理队列
	if (mQueueEnqueueEx(&handle->m_input_queue, &handle->m_input_tensors[handle->m_cur_input_tensor]) == E_QUEUE_OK) {
		handle->m_cur_input_tensor++;
		handle->m_cur_input_tensor %= BPU_INPUT_BUFFER_NUM;
	} else {
		SC_LOGI("m_input_queue full, skip it");
	}

	return 0;
}

void bpu_wrap_callback_register(bpu_handle_t* handle, bpu_post_process_callback callback, void *userdata)
{
	if (handle == NULL)
		return;
	handle->callback = callback;
	handle->m_userdata = userdata;
}

void bpu_wrap_callback_unregister(bpu_handle_t* handle)
{
	if (handle == NULL)
		return;
	handle->callback = NULL;
	handle->m_userdata = NULL;
	SC_LOGI("ok!");
}


// 通用的算法回调函数，目前都是通过websocket想web上发送
int32_t bpu_wrap_general_result_handle(char *result, void *userdata)
{
	int32_t ret = 0;
	int32_t pipeline_id = 0;
	char *ws_msg = NULL;

	if (userdata)
		pipeline_id = *(int*)userdata;

	// json 算法结果添加标志信息
	// 分配内存
	ws_msg = malloc(strlen(result) + 32);
	if (NULL == ws_msg) {
		SC_LOGE("Failed to allocate memory for ws_msg");
		return -1;
	}
	sprintf(ws_msg, "{\"kind\":10, \"pipeline\":%d,", pipeline_id + 1);
	strcat(ws_msg, result);
	strcat(ws_msg, "}");

	ret = SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_SEND_MSG, (void*)ws_msg);
	free(ws_msg);
	return ret;
}
