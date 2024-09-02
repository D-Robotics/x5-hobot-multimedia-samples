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

#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "communicate/sdk_communicate.h"

#include "utils/nalu_utils.h"
#include "utils/utils_log.h"
#include "utils/cqueue.h"
#include "utils/common_utils.h"
#include "utils/stream_define.h"
#include "utils/stream_manager.h"
#include "utils/mthread.h"
#include "utils/mqueue.h"
#include "utils/time_utils.h"

#include "bpu_wrap.h"
#include "vp_wrap.h"
#include "vp_codec.h"
#include "vp_sensors.h"
#include "vp_display.h"

#include "vp_gdc.h"

#include "solution_handle.h"
#include "solution_config.h"

#include "vpp_preparam.h"
#include "vpp_camera_impl.h"

#define VPP_CAM_MAX_CHANNELS 32

typedef struct
{
	int pipline_id;
	int drm_init_succesed;
	vp_drm_context_t *drm_context;
	vp_vflow_contex_t vp_vflow_contex;
	media_codec_context_t m_encode_context;

	bpu_handle_t	m_bpu_handle;

	shm_stream_t 	*venc_shm; /* H264 H265 码流，最大可能是32路 */
	tsThread 		m_vse_thread; /* 从vse获取图像，送入编码 */
	tsThread 		m_venc_thread; /*从编码器获取图像，送入共享内存 */
	tsQueue			m_vse_to_enc_queue;
	tsQueue			m_enc_to_vse_queue;

	tsThread		m_bpu_thread;
} vpp_camera_t;

static vp_drm_context_t g_drm_context;
static vpp_camera_t g_vpp_camera[VPP_CAM_MAX_CHANNELS];

static void vpp_camera_push_stream(vpp_camera_t *vpp_camera, ImageFrame *stream)
{
	int32_t frame_rate = 0;
	int32_t venc_ist_id = 0;
	media_codec_id_t codec_type;
	media_codec_context_t *codec_context = &vpp_camera->m_encode_context;

	media_codec_buffer_t *buffer = NULL;

	if(codec_context == NULL || stream == NULL) {
		SC_LOGE("Param is NULL");
		return;
	}

	venc_ist_id = codec_context->instance_index;
	codec_type = codec_context->codec_id;
	frame_rate = vpp_camera->m_encode_context.video_enc_params.rc_params.h264_cbr_params.frame_rate;

	buffer = (media_codec_buffer_t *)(stream->frame_buffer);

	frame_info info;
	info.type		= codec_type;
	info.key		= venc_ist_id;
	info.seq		= buffer->vstream_buf.src_idx;
	info.pts		= buffer->vstream_buf.pts;
	info.length		= buffer->vstream_buf.size;
	info.t_time		= (unsigned int)time(0);
	info.framerate	= frame_rate;
	info.width		= vpp_camera->m_encode_context.video_enc_params.width;
	info.height		= vpp_camera->m_encode_context.video_enc_params.height;

	// SC_LOGI("codec put size %lld", buffer->vstream_buf.size);
	shm_stream_put(vpp_camera->venc_shm, info, (unsigned char*)buffer->vstream_buf.vir_ptr, buffer->vstream_buf.size);
}
static void update_osd_info(vp_vflow_contex_t* vp_vflow_contex, uint64_t *next_update_time_ms){
	uint64_t current_time_ms = get_timestamp_ms();

	if(current_time_ms > *next_update_time_ms){
		char world_time_string[100];
		get_world_time_string(world_time_string, sizeof(world_time_string));
		vp_osd_draw_world(vp_vflow_contex, 0, world_time_string);
		*next_update_time_ms = (current_time_ms / 1000) * 1000 + 1000;
	}
}
static void* venc_get_stream_proc(void *ptr)
{
	int32_t ret = 0;
	tsThread *privThread = (tsThread*)ptr;
	vpp_camera_t *vpp_camera = (vpp_camera_t *)privThread->pvThreadData;

	ImageFrame encode_stream = {0};
	if (vp_allocate_image_frame(&encode_stream) == NULL) {
		SC_LOGE("vp_allocate_image_frame for encode_stream failed, so exit program.");
		exit(-1);
	}
	teQueueStatus status = E_QUEUE_OK;
	ImageFrame vse_frame = {0};
	hbn_vnode_image_t *hbn_vnode_image = NULL;

	int dequeue_enc_count = 0;
	int enqueue_vse_count = 0;
	//线程退出时，保证hbn_vnode_image 已经处理完： 放到 m_enc_to_vse_queue
	while (privThread->eState == E_THREAD_RUNNING){
		status = mQueueDequeueTimed(&vpp_camera->m_vse_to_enc_queue, 2000, (void **)&hbn_vnode_image);
		if(status != E_QUEUE_OK){
			SC_LOGE("channel %d dequeue from enc_to_vse_queue failed:%d\n", vpp_camera->pipline_id ,status);
			continue;
		}
		dequeue_enc_count++;

		// 送进编码器
		vse_frame.hbn_vnode_image = hbn_vnode_image;
		ret = vp_codec_encoder_set_input(&vpp_camera->m_encode_context, &vse_frame);
		if(ret != 0){
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("vp_codec_set_input failed.");
			}
			break;
		}

		// 从编码器获取码流
		ret = vp_codec_get_output(&vpp_camera->m_encode_context, &encode_stream, 2000);
		if(ret != 0){
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("vp_codec_get_output failed.");
			}
			break;
		}

		// 编码器用完VSE的数据 就释放
		ret = vp_vse_release_frame(&vpp_camera->vp_vflow_contex, 0, &vse_frame);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed.");
			break;
		}

		// 编码器用完VnodeBuffer,就归还给 VSE
		while(privThread->eState == E_THREAD_RUNNING){
			status = mQueueEnqueueEx(&vpp_camera->m_enc_to_vse_queue, hbn_vnode_image);
			if (status != E_QUEUE_OK){
				SC_LOGE("channel %d enqueue from enc_to_vse_queue failed:%d\n", vpp_camera->pipline_id ,status);
				sleep(1);
				continue;
			}
			hbn_vnode_image = NULL;
			enqueue_vse_count++;
			break;
		}


		vpp_camera_push_stream(vpp_camera, &encode_stream);
		ret = vp_codec_release_output(&vpp_camera->m_encode_context, &encode_stream);
		if (ret != 0) {
			SC_LOGE("vp_codec_release_output failed.");
			break;
		}
	}

	int free_dissociate_count = 0;
	if(hbn_vnode_image != NULL){
		free(hbn_vnode_image);
		free_dissociate_count = 1;
	}
	SC_LOGI("channel %d dequeue enc %d = enqueue vse %d + free_dissociate_count %d.\n",
		vpp_camera->pipline_id , dequeue_enc_count, enqueue_vse_count, free_dissociate_count);


	vp_free_image_frame(&encode_stream);
	mThreadFinish(privThread);
	return NULL;
}

/******************************************************************************
 * funciton : get stream from each channels
 ******************************************************************************/
static void* vse_get_stream_proc(void *ptr)
{
	int32_t ret = 0;

	tsThread *privThread = (tsThread*)ptr;
	vpp_camera_t *vpp_camera = (vpp_camera_t *)privThread->pvThreadData;
	mThreadSetNameWidthIndex(privThread, __func__, vpp_camera->pipline_id);

	teQueueStatus status = E_QUEUE_OK;
	ImageFrame vse_frame = {0};
	hbn_vnode_image_t *hbn_vnode_image = NULL;

	uint64_t next_update_time_ms = ((get_timestamp_ms() + 999) / 1000) * 1000;
	struct TimeStatistics time_statistics;

	int dequeue_vse_count = 0;
	int enqueue_enc_count = 0;

	while (privThread->eState == E_THREAD_RUNNING){
		time_statistics_at_beginning_of_loop(&time_statistics);
		status = mQueueDequeueTimed(&vpp_camera->m_enc_to_vse_queue, 2000, (void **)&hbn_vnode_image);
		if (status != E_QUEUE_OK){
			SC_LOGW("channel %d dequeue from enc_to_vse_queue failed:%d\n", vpp_camera->pipline_id ,status);
			continue;
		}
		dequeue_vse_count++;

		vse_frame.hbn_vnode_image = hbn_vnode_image;
		ret = vp_vse_get_frame(&vpp_camera->vp_vflow_contex, 0, &vse_frame);
		if (ret != 0) {
			// 当线程接收到退出信号时，getframe 接口会立即报超时退出
			// 所以只有当线程是正常运行状态下的异常才属于真异常
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("vp_vse_get_frame chn 0 failed(%d).", ret);
			}
			break;
		}
		while(privThread->eState == E_THREAD_RUNNING){
			status = mQueueEnqueueEx(&vpp_camera->m_vse_to_enc_queue, hbn_vnode_image);
			if (status != E_QUEUE_OK){
				printf("channel %d enqueue from enc_to_vse_queue failed:%d\n", vpp_camera->pipline_id ,status);
				sleep(1);
				continue;
			}
			hbn_vnode_image = NULL;
			enqueue_enc_count++;
			break;
		}

		update_osd_info(&vpp_camera->vp_vflow_contex, &next_update_time_ms);
		if((vpp_camera->drm_context != NULL) && (vpp_camera->drm_init_succesed != 0)){
			ret = vp_display_set_frame(vpp_camera->drm_context, vse_frame.hbn_vnode_image);
			if(ret != 0){
				SC_LOGW("vp_display_set_frame chn failed(%d).", ret);
			}
		}

		time_statistics_at_ending_of_loop(&time_statistics);
		time_statistics_info_show(&time_statistics, "read_camera", false);
	}
	int free_dissociate_count = 0;
	if(hbn_vnode_image != NULL){
		free(hbn_vnode_image);
		free_dissociate_count = 1;
	}

	SC_LOGI("channel %d dequeue vse %d = enqueue enc %d + free_dissociate_count %d.\n",
		vpp_camera->pipline_id , dequeue_vse_count, enqueue_enc_count, free_dissociate_count);
	mThreadFinish(privThread);
	return NULL;
}

// 从vse的chn1获取yuv数据送给bpu进行算法运行
static void *send_yuv_to_bpu(void *ptr) {
	tsThread *privThread = (tsThread*)ptr;
	int ret = 0;
	ImageFrame vse_frame = {0};
	hbn_vnode_image_t *hbn_vnode_image = NULL;
	bpu_buffer_info_t bpu_input_buffer;

	vpp_camera_t *vpp_camera = (vpp_camera_t *)privThread->pvThreadData;

	if (vp_allocate_image_frame(&vse_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for vse_frame failed, so exit program.");
		exit(-1);
	};
	mThreadSetNameWidthIndex(privThread, __func__, vpp_camera->pipline_id);

	while(privThread->eState == E_THREAD_RUNNING) {
		ret = vp_vse_get_frame(&vpp_camera->vp_vflow_contex, 1, &vse_frame);
		if (ret != 0) {
			// 当线程接收到退出信号时，getframe 接口会立即报超时退出
			// 所以只有当线程是正常运行状态下的异常才属于真异常
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("vp_vse_get_frame chn 1 failed(%d).", ret);
			}
			break;
		}

		hbn_vnode_image = (hbn_vnode_image_t *)vse_frame.hbn_vnode_image;
		// vp_vin_print_hbn_vnode_image_t(hbn_vnode_image);

		// 把yuv数据送进bpu进行算法运算
		memset(&bpu_input_buffer, 0, sizeof(bpu_buffer_info_t));
		vpp_graphic_buf_to_bpu_buffer_info(hbn_vnode_image, &bpu_input_buffer);
		// print_bpu_buffer_info(&bpu_input_buffer);

		bpu_wrap_send_frame(&vpp_camera->m_bpu_handle, &bpu_input_buffer);
		ret = vp_vse_release_frame(&vpp_camera->vp_vflow_contex, 1, &vse_frame);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed");
			break;
		}
	}

	vp_free_image_frame(&vse_frame);

	mThreadFinish(privThread);
	return NULL;
}

int32_t vpp_camera_init_param(void)
{
	int32_t i = 0, ret = 0;

	camera_config_t *camera_config = NULL;
	isp_ichn_attr_t *isp_ichn_attr = NULL;
	vse_config_t *vse_config = NULL;
	int32_t input_width = 0, input_height = 0;
	int32_t model_width = 0, model_height = 0;

	memset(&g_vpp_camera, 0, sizeof(g_vpp_camera));

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		g_vpp_camera[i].m_encode_context.codec_id = MEDIA_CODEC_ID_NONE;
	}
	int vpp_camera_index = 0;
	int hdmi_display_channel = -1;
	int pipeline_count = g_solution_config.cam_solution.pipeline_count;
	int hdmi_is_connected = vp_display_check_hdmi_is_connected();
	if(hdmi_is_connected){
		SC_LOGI("hdmi is connected");
	}else{
		SC_LOGI("hdmi is not connected");
	}

	// 根据camera solution的配置设置vin、vse、venc、bpu模块的使能和参数
	for (i = 0; i < g_solution_config.cam_solution.max_pipeline_count; i++) {
		// 1. 配置 vin
		if(g_solution_config.cam_solution.cam_vpp[i].is_valid == 0){
			continue;
		}
		char* sensor_name = g_solution_config.cam_solution.cam_vpp[i].sensor;
		if(g_solution_config.cam_solution.cam_vpp[i].is_enable == 0){
			SC_LOGI("Ignore camera sensor [%s] [%d/%d].", sensor_name, i, pipeline_count);
			continue;
		}
		g_vpp_camera[i].vp_vflow_contex.mipi_csi_rx_index = g_solution_config.cam_solution.cam_vpp[i].csi_index;
		g_vpp_camera[i].vp_vflow_contex.sensor_config = vp_get_sensor_config_by_name(sensor_name);
		g_vpp_camera[i].vp_vflow_contex.mclk_is_not_configed = g_solution_config.cam_solution.cam_vpp[i].mclk_is_not_configed;

		SC_LOGI("Enable camera sensor [%s] [%d/%d] mclk_is_not_configed:[%d]", sensor_name, i, pipeline_count,
			g_vpp_camera[i].vp_vflow_contex.mclk_is_not_configed);
		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL) {
			SC_LOGE("sensor name not found(%s)", sensor_name);
			return -1;
		}

		// 2. 配置算法模型
		if (strlen(g_solution_config.cam_solution.cam_vpp[i].model) > 1
			&& strcmp(g_solution_config.cam_solution.cam_vpp[i].model, "null") != 0) {
			//g_vpp_camera 与插入的摄像头的顺序一一对应
			//m_vpp_id 与使能的摄像头一一对应
			//比如插入了两个摄像头,只使能第二个: g_vpp_camera[0] 是空 g_vpp_camera[1]是有效的
			// 					  			  m_vpp_id 为0 (决定了算法上报结果的通道号)
			g_vpp_camera[i].m_bpu_handle.m_vpp_id = vpp_camera_index;
			strncpy(g_vpp_camera[i].m_bpu_handle.m_model_name,
				g_solution_config.cam_solution.cam_vpp[i].model,
				sizeof(g_vpp_camera[i].m_bpu_handle.m_model_name) - 1);
			g_vpp_camera[i].m_bpu_handle.m_model_name[sizeof(g_vpp_camera[i].m_bpu_handle.m_model_name) - 1] = '\0';
		}

		// 3. 配置 vse
		vse_config = &g_vpp_camera[i].vp_vflow_contex.vse_config;
		isp_ichn_attr = g_vpp_camera[i].vp_vflow_contex.sensor_config->isp_ichn_attr;

		input_width = isp_ichn_attr->width;
		input_height = isp_ichn_attr->height;
		SC_LOGD("VSE %d: input_width: %d input_height: %d",
			i, input_width, input_height);

		vse_config->vse_ichn_attr.width = input_width;
		vse_config->vse_ichn_attr.height = input_height;
		vse_config->vse_ichn_attr.fmt = FRM_FMT_NV12;
		vse_config->vse_ichn_attr.bit_width = 8;

		// 第一个通道给编码器使用
		// 设置VSE通道0输出属性，ROI为原图大小，保持原始输入大小
		vse_config->vse_ochn_attr[0].chn_en = CAM_TRUE;
		vse_config->vse_ochn_attr[0].roi.x = 0;
		vse_config->vse_ochn_attr[0].roi.y = 0;
		vse_config->vse_ochn_attr[0].roi.w = input_width;
		vse_config->vse_ochn_attr[0].roi.h = input_height;
		vse_config->vse_ochn_attr[0].target_w = input_width;
		vse_config->vse_ochn_attr[0].target_h = input_height;
		vse_config->vse_ochn_attr[0].fmt = FRM_FMT_NV12;
		vse_config->vse_ochn_attr[0].bit_width = 8;

		// 第二个通道的数据给BPU使用
		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) > 1
			&& strcmp(g_vpp_camera[i].m_bpu_handle.m_model_name, "null") != 0) {
			ret = bpu_wrap_get_model_hw(g_vpp_camera[i].m_bpu_handle.m_model_name, &model_width, &model_height);
			vse_config->vse_ochn_attr[1].chn_en = CAM_TRUE;
			vse_config->vse_ochn_attr[1].roi.x = 0;
			vse_config->vse_ochn_attr[1].roi.y = 0;
			vse_config->vse_ochn_attr[1].roi.w = input_width;
			vse_config->vse_ochn_attr[1].roi.h = input_height;
			vse_config->vse_ochn_attr[1].target_w = model_width;
			vse_config->vse_ochn_attr[1].target_h = model_height;
			vse_config->vse_ochn_attr[1].fmt = FRM_FMT_NV12;
			vse_config->vse_ochn_attr[1].bit_width = 8;
		}

		// 第三个通道的数据给显示器使用，默认 1080P
		// 需要根据输入分辨率来设置不同的通道，待实现

		// 配置编码通道
		camera_config = g_vpp_camera[i].vp_vflow_contex.sensor_config->camera_config;
		ret = vp_encode_config_param(&g_vpp_camera[i].m_encode_context,
			VP_GET_MD_CODEC_TYPE(g_solution_config.cam_solution.cam_vpp[i].encode_type),
			input_width, input_height, camera_config->fps,
			g_solution_config.cam_solution.cam_vpp[i].encode_bitrate,
			true);
		if (ret != 0)
		{
			SC_LOGE("Encode config param error");
		}
		g_vpp_camera[i].vp_vflow_contex.gdc_info.input_width = input_width;
		g_vpp_camera[i].vp_vflow_contex.gdc_info.input_height = input_height;
		strcpy(g_vpp_camera[i].vp_vflow_contex.gdc_info.sensor_name, sensor_name);
		g_vpp_camera[i].vp_vflow_contex.gdc_info.status = g_solution_config.cam_solution.cam_vpp[i].gdc_status;

		if((hdmi_is_connected) && (hdmi_display_channel == -1)){

			g_vpp_camera[i].drm_context = &g_drm_context;
			hdmi_display_channel = g_vpp_camera[i].pipline_id;
			SC_LOGI("channel %d enable hdmi display", hdmi_display_channel);
		}

		vpp_camera_index++;
	}

	return ret;
}

int32_t vpp_camera_init(void)
{
	int32_t ret = 0;
	int32_t i = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	hb_mem_module_open();

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		g_vpp_camera[i].venc_shm = NULL;

		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL)
			continue;

		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;

		ret = vp_vin_init(vp_vflow_contex);
		ret |= vp_isp_init(vp_vflow_contex);
		ret |= vp_vse_init(vp_vflow_contex);
		ret |= vp_osd_init(vp_vflow_contex);
		ret |= vp_gdc_init(vp_vflow_contex);
		ret |= vp_vflow_init(vp_vflow_contex);
		if (ret != 0){
			SC_LOGE("pipeline init failed for channel %d error", i);
			continue;
		}

		g_vpp_camera[i].drm_init_succesed = 0;
		if(g_vpp_camera[i].drm_context != NULL){
			vp_vse_output_info_t vp_vse_output_info;
			int vse_ret = vp_vse_get_output_info(vp_vflow_contex, 0, &vp_vse_output_info);
			if(vse_ret != 0){
				SC_LOGE("vp_vse_get_output_info failed for channel %d error", i);
			}else{
				int width = vp_vse_output_info.width;
				int height = vp_vse_output_info.height;
				SC_LOGI("channel %d init hdmi display width:%d height %d",
					g_vpp_camera[i].pipline_id, width, height);
				//g_vpp_camera[i].vp_vflow_contex.sensor_config->camera_config
				vse_ret = vp_display_init(g_vpp_camera[i].drm_context, width, height);
				if(vse_ret != 0){
					SC_LOGW("channel %d init hdmi display width:%d height %d failed.",
					g_vpp_camera[i].pipline_id, width, height);
				}else{
					g_vpp_camera[i].drm_init_succesed = 1;
				}
			}
		}



		ret = vp_codec_init(&g_vpp_camera[i].m_encode_context);
		if (ret != 0){
			SC_LOGE("Encode vp_codec_init error for channel %d", i);
			continue;
		}
		SC_LOGI("Init video encode instance %d successful", g_vpp_camera[i].m_encode_context.instance_index);

		// 初始化算法模块， 从vse的chn1通道get yuv数据
		// 初始化bpu
		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_model_init(&g_vpp_camera[i].m_bpu_handle, g_vpp_camera[i].m_bpu_handle.m_model_name);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_model_init failed");
			continue;
		}
		// 注册算法结果回调函数
		bpu_wrap_callback_register(&g_vpp_camera[i].m_bpu_handle,
			bpu_wrap_general_result_handle, &g_vpp_camera[i].m_bpu_handle.m_vpp_id);
	}

	SC_LOGD("successful");
	return 0;
}

int32_t vpp_camera_uninit(void)
{
	int32_t ret = 0, i = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL)
			continue;

		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;

		ret = vp_codec_deinit(&g_vpp_camera[i].m_encode_context);
		ret |= vp_vflow_deinit(vp_vflow_contex);
		ret |= vp_osd_deinit(vp_vflow_contex);
		ret |= vp_vse_deinit(vp_vflow_contex);
		ret |= vp_gdc_deinit(vp_vflow_contex);
		ret |= vp_isp_deinit(vp_vflow_contex);
		ret |= vp_vin_deinit(vp_vflow_contex);

		if(g_vpp_camera[i].drm_context != NULL){
			if(g_vpp_camera[i].drm_init_succesed){
				SC_LOGI("channel %d deinit hdmi display", g_vpp_camera[i].pipline_id);
				ret |= vp_display_deinit(g_vpp_camera[i].drm_context);
			}

		}
		SC_ERR_CON_EQ(ret, 0, "vpp_camera_uninit");

		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_deinit(&g_vpp_camera[i].m_bpu_handle);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_model_init failed");
			return -1;
		}
	}

	hb_mem_module_close();

	vp_print_debug_infos();
	return ret;
}

int32_t vpp_camera_start(void)
{
	int32_t ret = 0;
	int32_t i = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL)
			continue;
		g_vpp_camera[i].pipline_id = i;
		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;

		//队列的个数根据 vse 输出buffer的个数设置
		int codec_buffer_count = 6;
		teQueueStatus status = mQueueCreate(&g_vpp_camera[i].m_vse_to_enc_queue, codec_buffer_count + 1); //必须是加1的
		if(status != E_QUEUE_OK){
			SC_LOGE("mqueue create failed %d, for channle:%d.", status, i);
			continue;
		}
		int vse_buffer_count = 6;
		status = mQueueCreate(&g_vpp_camera[i].m_enc_to_vse_queue, vse_buffer_count + 1);
		if(status != E_QUEUE_OK){
			SC_LOGE("mqueue create failed %d, for channle:%d.", status, i);
			continue;
		}

		for (size_t j = 0; j < vse_buffer_count; j++){
			hbn_vnode_image_t *hbn_vnode_image = (hbn_vnode_image_t *)malloc(sizeof(hbn_vnode_image_t));
			if (hbn_vnode_image == NULL){
				SC_LOGE("malloc failed\n");
				exit(-1);
			}
			memset(hbn_vnode_image, 0, sizeof(hbn_vnode_image_t));

			teQueueStatus status = mQueueEnqueue(&g_vpp_camera[i].m_enc_to_vse_queue, (void *)hbn_vnode_image);
			if (status != E_QUEUE_OK){
				printf("mqueue enqueue failed:%d\n", status);
				return -1;
			}
		}

		ret = vp_codec_start(&g_vpp_camera[i].m_encode_context);
		if (ret != 0)
		{
			SC_LOGE("Encode vp_codec_start error");
			return -1;
		}
		SC_LOGI("Start video encode instance %d successful", g_vpp_camera[i].m_encode_context.instance_index);

		ret = vp_vin_start(vp_vflow_contex);
		ret |= vp_isp_start(vp_vflow_contex);
		ret |= vp_vse_start(vp_vflow_contex);
		ret |= vp_osd_start(vp_vflow_contex);
		ret |= vp_gdc_start(vp_vflow_contex);
		ret |= vp_vflow_start(vp_vflow_contex);
		SC_ERR_CON_EQ(ret, 0, "vpp_camera_start");

		if(g_vpp_camera[i].venc_shm == NULL) {
			T_SDK_VENC_INFO venc_chn_info;

			media_codec_context_t *codec_context = &g_vpp_camera[i].m_encode_context;
			media_codec_id_t codec_type = codec_context->codec_id;
			int32_t venc_ist_id = codec_context->instance_index;
			venc_chn_info.channel = venc_ist_id;
			ret = SDK_Cmd_Impl(SDK_CMD_VPP_VENC_CHN_PARAM_GET, (void*)&venc_chn_info);

			char shm_id[32] = {0}, shm_name[32] = {0};
			sprintf(shm_id, "cam_id_%s_chn%d", codec_type == MEDIA_CODEC_ID_H264 ? "h264" :
					(codec_type == MEDIA_CODEC_ID_H265 ? "h265" :
					(codec_type == MEDIA_CODEC_ID_JPEG) ? "jpeg" : "other"), venc_ist_id);
			sprintf(shm_name, "name_%s_chn%d", codec_type == MEDIA_CODEC_ID_H264 ? "h264" :
					(codec_type == MEDIA_CODEC_ID_H265 ? "h265" :
					(codec_type == MEDIA_CODEC_ID_JPEG) ? "jpeg" : "other"), venc_ist_id);
			g_vpp_camera[i].venc_shm = shm_stream_create(shm_id, shm_name,
					STREAM_MAX_USER, venc_chn_info.suggest_buffer_item_count,
					venc_chn_info.suggest_buffer_region_size,
					SHM_STREAM_WRITE, SHM_STREAM_MALLOC);

			SC_LOGI("video_stream_create => shm_id: %s, shm_name: %s, max user: %d, framerate: %d, stream_buf_size: %d bitrate:%d region size:%d, item count %d.",
				shm_id, shm_name, STREAM_MAX_USER,
				venc_chn_info.framerate, venc_chn_info.stream_buf_size, venc_chn_info.bitrate,
				venc_chn_info.suggest_buffer_region_size, venc_chn_info.suggest_buffer_item_count);
		}else{
			SC_LOGE("channel %d's venc_shm is not null, exit(-1)", i);
			exit(-1);
		}

		g_vpp_camera[i].m_vse_thread.pvThreadData = (void*)&g_vpp_camera[i];
		mThreadStart(vse_get_stream_proc, &g_vpp_camera[i].m_vse_thread, E_THREAD_JOINABLE);

		g_vpp_camera[i].m_venc_thread.pvThreadData = (void*)&g_vpp_camera[i];
		mThreadStart(venc_get_stream_proc, &g_vpp_camera[i].m_venc_thread, E_THREAD_JOINABLE);

		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		// 设置bpu后处理的原始图像大小为推流图像大小
		bpu_wrap_set_ori_hw(&g_vpp_camera[i].m_bpu_handle,
			g_vpp_camera[i].m_encode_context.video_enc_params.width,
			g_vpp_camera[i].m_encode_context.video_enc_params.height);

		ret = bpu_wrap_start(&g_vpp_camera[i].m_bpu_handle);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_start failed");
			return -1;
		}

		// 启动一个线程从 vse 获取 yuv 数据给 bpu 进行算法运算
		g_vpp_camera[i].m_bpu_thread.pvThreadData = (void*)&g_vpp_camera[i];
		mThreadStart(send_yuv_to_bpu, &g_vpp_camera[i].m_bpu_thread, E_THREAD_JOINABLE);
		SC_LOGI("Start BPU %d process successful, %s", i, g_vpp_camera[i].m_bpu_handle.m_model_name);
	}
	vp_print_debug_infos();
	return ret;
}

int32_t vpp_camera_stop(void)
{
	int32_t ret = 0, i = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	// 先把所有线程停掉
	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL)
			continue;

		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;
		mThreadStop(&g_vpp_camera[i].m_venc_thread);
		mThreadStop(&g_vpp_camera[i].m_vse_thread);
		if(g_vpp_camera[i].venc_shm != NULL){
			shm_stream_destory(g_vpp_camera[i].venc_shm);
			g_vpp_camera[i].venc_shm = NULL;
		}

		int enc_remain_count = 0;
		int vse_remain_count = 0;
		teQueueStatus status = E_QUEUE_OK;
		while(!mQueueIsEmpty(&g_vpp_camera[i].m_vse_to_enc_queue)){
			hbn_vnode_image_t *hbn_vnode_image = NULL;
			status = mQueueDequeueTimed(&g_vpp_camera[i].m_vse_to_enc_queue, 0, (void **)&hbn_vnode_image);
			if(status != E_QUEUE_OK){
				SC_LOGE("mqueue clear failed %d, for channle:%d.", status, i);
				break;
			}
			free(hbn_vnode_image);
			enc_remain_count++;
		}
		status = mQueueDestroy(&g_vpp_camera[i].m_vse_to_enc_queue);
		if(status != E_QUEUE_OK){
			SC_LOGE("mqueue destroy failed %d, for channle:%d.", status, i);
		}

		while(!mQueueIsEmpty(&g_vpp_camera[i].m_enc_to_vse_queue)){
			hbn_vnode_image_t *hbn_vnode_image = NULL;
			status = mQueueDequeueTimed(&g_vpp_camera[i].m_enc_to_vse_queue, 0, (void **)&hbn_vnode_image);
			if(status != E_QUEUE_OK){
				SC_LOGE("mqueue clear failed %d, for channle:%d.", status, i);
				break;
			}
			free(hbn_vnode_image);
			vse_remain_count++;
		}
		SC_LOGI("channel %d enc queue remain %d, vse queue remain %d .\n",
				&g_vpp_camera[i].pipline_id , enc_remain_count, vse_remain_count);

		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		mThreadStop(&g_vpp_camera[i].m_bpu_thread);
	}

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL)
			continue;

		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;

		ret = vp_codec_stop(&g_vpp_camera[i].m_encode_context);
		ret |= vp_vflow_stop(vp_vflow_contex);
		ret |= vp_vin_stop(vp_vflow_contex);
		ret |= vp_isp_stop(vp_vflow_contex);
		ret |= vp_osd_stop(vp_vflow_contex);
		ret |= vp_gdc_stop(vp_vflow_contex);
		ret |= vp_vse_stop(vp_vflow_contex);
		SC_ERR_CON_EQ(ret, 0, "vpp_camera_stop");

		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_stop(&g_vpp_camera[i].m_bpu_handle);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_start failed");
			return -1;
		}
	}

	return ret;
}

static int32_t get_pipeline_id_by_video_id(int32_t video_id)
{
	int32_t i = 0;
	int32_t enable_pipeline_count = 0;
	// 遍历所有 pipeline
	// 用 enable_pipeline_count 记录使能的pipeline的编号，这个编号理论上与 web 上的video编号相等
	// 当 enable_pipeline_count == video_id时就说明找到了对应的pipeline
	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			enable_pipeline_count++;
			if (enable_pipeline_count == video_id) {
				return i;
			}
		}
	}
	return 0;
}

int32_t vpp_camera_param_set(SOLUTION_PARAM_E type, char* val, uint32_t length)
{
	int32_t ret = 0;
	return ret;
}

int32_t vpp_camera_param_get(SOLUTION_PARAM_E type, char* val, uint32_t* length)
{
	int32_t i = 0, ret = 0;
	mc_video_codec_enc_params_t *enc_params;
	ImageFrame image_frame = {0};
	char file_name[256] = {0};
	hbn_vnode_image_t *hbn_vnode_image = NULL;

	switch(type)
	{
	case SOLUTION_VENC_CHN_PARAM_GET: // 获取某个编码通道的配置
		{
			venc_info_t* param = (venc_info_t*)val;
			param->enable = 0;
			SC_LOGI("param->channel: %d", param->channel);
			for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
				if (g_vpp_camera[i].m_encode_context.codec_id == MEDIA_CODEC_ID_NONE) {
					continue;
				}
				if (g_vpp_camera[i].m_encode_context.instance_index == param->channel) {
					// 填充对外的信息
					enc_params = &g_vpp_camera[i].m_encode_context.video_enc_params;
					param->enable = 1;
					param->width = enc_params->width;
					param->height = enc_params->height;
					param->stream_buf_size = enc_params->bitstream_buf_size;
					if (g_vpp_camera[i].m_encode_context.codec_id == MEDIA_CODEC_ID_H264) {
						param->type = 96;
						param->bitrate = enc_params->rc_params.h264_cbr_params.bit_rate;
						param->framerate = enc_params->rc_params.h264_cbr_params.frame_rate;
					} else if (g_vpp_camera[i].m_encode_context.codec_id == MEDIA_CODEC_ID_H265) {
						param->type = 265;
						param->bitrate = enc_params->rc_params.h265_cbr_params.bit_rate;
						param->framerate = enc_params->rc_params.h265_cbr_params.frame_rate;
					}else{
						SC_LOGE("unsupport codec id %d.", g_vpp_camera[i].m_encode_context.codec_id);
						exit(-1);
					}
					vp_codec_get_user_buffer_param(enc_params, &param->suggest_buffer_region_size,
						&param->suggest_buffer_item_count);
				}
			}
			break;
		}
	case SOLUTION_GET_VENC_CHN_STATUS: // 获取哪些编码通道被使能了
		{
			// 32位的整形，每个通道的状态占其中一个bit
			// 注： 64bit的值位与会有异常，待查
			unsigned int *status = (unsigned int *)val;
			*status = 0;
			int valid_index = 0;
			for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
				if (g_vpp_camera[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
					*status |= (1 << valid_index);
					valid_index++;
				}
			}
			SC_LOGI("venc status: 0x%x", *status);
			break;
		}
	case SOLUTION_GET_RAW_FRAME:
		{
			// video_id 代表web上的第几个 video 控件，从1开始计数
			// 需要结合当前使能了多少路pipeline来获取到对应的 pipeline id
			int32_t video_id = *(int32_t *)val;
			int32_t pipeline_id = get_pipeline_id_by_video_id(video_id);
			if (vp_allocate_image_frame(&image_frame) == NULL) {
				SC_LOGE("vp_allocate_image_frame failed");
				return -1;
			}
			ret = vp_vin_get_frame(&g_vpp_camera[pipeline_id].vp_vflow_contex, &image_frame);
			if (ret != 0) {
				SC_LOGE("vp_vin_get_frame failed (%d)", ret);
				vp_free_image_frame(&image_frame);
				return -1;
			}

			hbn_vnode_image = (hbn_vnode_image_t *)image_frame.hbn_vnode_image;

			snprintf(file_name, sizeof(file_name),
				"/tmp/pipeline_%d_vin_chn0_%dx%d_stride_%d_frameid_%d_ts_%ld.raw",
				pipeline_id,
				hbn_vnode_image->buffer.width,
				hbn_vnode_image->buffer.height,
				hbn_vnode_image->buffer.stride,
				hbn_vnode_image->info.frame_id,
				hbn_vnode_image->info.timestamps);

			SC_LOGI("pipeline %d vin dump raw %dx%d(stride:%d), buffer size: %ld frame id: %d,"
				" timestamp: %ld",
				pipeline_id,
				hbn_vnode_image->buffer.width, hbn_vnode_image->buffer.height,
				hbn_vnode_image->buffer.stride,
				hbn_vnode_image->buffer.size[0],
				hbn_vnode_image->info.frame_id,
				hbn_vnode_image->info.timestamps);

			delete_files_with_extension("/tmp", ".raw");
			vp_dump_1plane_image_to_file(file_name, hbn_vnode_image->buffer.virt_addr[0],
				hbn_vnode_image->buffer.size[0]);

			vp_vin_release_frame(&g_vpp_camera[pipeline_id].vp_vflow_contex, &image_frame);
			if (ret != 0) {
				SC_LOGE("vp_vin_release_frame failed.");
				vp_free_image_frame(&image_frame);
				return -1;
			}
			vp_free_image_frame(&image_frame);
			// 通知浏览器下载文件
			SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_UPLOAD_FILE, (void*)file_name);
			break;
		}
	case SOLUTION_GET_ISP_FRAME:
		{
			// video_id 代表web上的第几个 video 控件，从1开始计数
			// 需要结合当前使能了多少路pipeline来获取到对应的 pipeline id
			int32_t video_id = *(int32_t *)val;
			int32_t pipeline_id = get_pipeline_id_by_video_id(video_id);
			if (vp_allocate_image_frame(&image_frame) == NULL) {
				SC_LOGE("vp_allocate_image_frame failed");
				return -1;
			}
			ret = vp_isp_get_frame(&g_vpp_camera[pipeline_id].vp_vflow_contex, &image_frame);
			if (ret != 0) {
				SC_LOGE("vp_isp_get_frame failed (%d)", ret);
				vp_free_image_frame(&image_frame);
				return -1;
			}

			hbn_vnode_image = (hbn_vnode_image_t *)image_frame.hbn_vnode_image;

			snprintf(file_name, sizeof(file_name),
				"/tmp/pipeline_%d_isp_chn0_%dx%d_stride_%d_frameid_%d_ts_%ld.yuv",
				pipeline_id,
				hbn_vnode_image->buffer.width,
				hbn_vnode_image->buffer.height,
				hbn_vnode_image->buffer.stride,
				hbn_vnode_image->info.frame_id,
				hbn_vnode_image->info.timestamps);

			SC_LOGI("pipeline %d isp dump yuv %dx%d(stride:%d), buffer size: %ld frame id: %d,"
				" timestamp: %ld",
				pipeline_id,
				hbn_vnode_image->buffer.width, hbn_vnode_image->buffer.height,
				hbn_vnode_image->buffer.stride,
				hbn_vnode_image->buffer.size[0],
				hbn_vnode_image->info.frame_id,
				hbn_vnode_image->info.timestamps);

			delete_files_with_extension("/tmp", ".yuv");
			vp_dump_2plane_yuv_to_file(file_name,
				hbn_vnode_image->buffer.virt_addr[0],
				hbn_vnode_image->buffer.virt_addr[1],
				hbn_vnode_image->buffer.size[0],
				hbn_vnode_image->buffer.size[1]);

			vp_isp_release_frame(&g_vpp_camera[pipeline_id].vp_vflow_contex, &image_frame);
			if (ret != 0) {
				SC_LOGE("vp_isp_release_frame failed.");
				vp_free_image_frame(&image_frame);
				return -1;
			}
			vp_free_image_frame(&image_frame);
			// 通知浏览器下载文件
			SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_UPLOAD_FILE, (void*)file_name);
			break;
		}
	case SOLUTION_GET_VSE_FRAME:
		{
			// video_id 代表web上的第几个 video 控件，从1开始计数
			// 需要结合当前使能了多少路pipeline来获取到对应的 pipeline id
			int32_t video_id = *(int32_t *)val;
			int32_t pipeline_id = get_pipeline_id_by_video_id(video_id);
			if (vp_allocate_image_frame(&image_frame) == NULL) {
				SC_LOGE("vp_allocate_image_frame failed");
				return -1;
			}
			ret = vp_vse_get_frame(&g_vpp_camera[pipeline_id].vp_vflow_contex, 0, &image_frame);
			if (ret != 0) {
				SC_LOGE("vp_vse_get_frame failed (%d)", ret);
				vp_free_image_frame(&image_frame);
				return -1;
			}

			hbn_vnode_image = (hbn_vnode_image_t *)image_frame.hbn_vnode_image;

			snprintf(file_name, sizeof(file_name),
				"/tmp/pipeline_%d_vse_ochn0_%dx%d_stride_%d_frameid_%d_ts_%ld.yuv",
				pipeline_id,
				hbn_vnode_image->buffer.width,
				hbn_vnode_image->buffer.height,
				hbn_vnode_image->buffer.stride,
				hbn_vnode_image->info.frame_id,
				hbn_vnode_image->info.timestamps);

			SC_LOGI("pipeline %d vse dump yuv %dx%d(stride:%d), buffer size: %ld frame id: %d,"
				" timestamp: %ld",
				pipeline_id,
				hbn_vnode_image->buffer.width, hbn_vnode_image->buffer.height,
				hbn_vnode_image->buffer.stride,
				hbn_vnode_image->buffer.size[0],
				hbn_vnode_image->info.frame_id,
				hbn_vnode_image->info.timestamps);

			delete_files_with_extension("/tmp", ".yuv");
			vp_dump_2plane_yuv_to_file(file_name,
				hbn_vnode_image->buffer.virt_addr[0],
				hbn_vnode_image->buffer.virt_addr[1],
				hbn_vnode_image->buffer.size[0],
				hbn_vnode_image->buffer.size[1]);

			vp_vse_release_frame(&g_vpp_camera[pipeline_id].vp_vflow_contex, 0, &image_frame);
			if (ret != 0) {
				SC_LOGE("vp_vse_release_frame failed.");
				vp_free_image_frame(&image_frame);
				return -1;
			}
			vp_free_image_frame(&image_frame);
			// 通知浏览器下载文件
			SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_UPLOAD_FILE, (void*)file_name);
			break;
		}
	default:
		{
			ret= -1;
			break;
		}
	}
	return ret;
}
