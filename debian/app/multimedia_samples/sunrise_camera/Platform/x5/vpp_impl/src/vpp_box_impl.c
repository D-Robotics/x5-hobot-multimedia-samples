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

#include "utils/utils_log.h"
#include "utils/cqueue.h"
#include "utils/common_utils.h"
#include "utils/stream_define.h"
#include "utils/stream_manager.h"
#include "utils/mthread.h"
#include "utils/mqueue.h"

#include "bpu_wrap.h"
#include "vp_wrap.h"
#include "vp_codec.h"

#include "solution_handle.h"
#include "solution_config.h"

#include "vpp_preparam.h"
#include "vpp_box_impl.h"

#define VPP_BOX_MAX_CHANNELS 8

typedef struct
{
	int pipline_id;
	char			m_stream_path[128];
	media_codec_context_t m_decode_context;
	vp_decode_param_t m_decode_param;

	media_codec_context_t m_encode_context;

	// 使能vse, 功能：
	// 1. 把解码出来的图像缩放到编码通道的分辨率大小
	// 2. 输出BPU 算法模型需要的图像
	vp_vflow_contex_t vp_vflow_contex;

	bpu_handle_t	m_bpu_handle;

	shm_stream_t 	*venc_shm; /* H264 H265 码流，最大支持32路 */
	tsThread 		m_venc_thread; /* 图像编码、输出给vo、算法图像前处理 */
	tsThread 		m_vdec_thread; /* 读取h264视频文件解码 */
	tsThread		m_bpu_thread;
} vpp_box_t;

static vpp_box_t g_vpp_box[VPP_BOX_MAX_CHANNELS];

static void vpp_box_push_stream(vpp_box_t *vpp_box, ImageFrame *stream, int pipline_id)
{
	int32_t frame_rate = 0;
	media_codec_id_t codec_type;
	media_codec_context_t *codec_context = &vpp_box->m_encode_context;

	media_codec_buffer_t *buffer = NULL;

	if(codec_context == NULL || stream == NULL) {
		SC_LOGE("Param is NULL");
		return;
	}

	codec_type = codec_context->codec_id;
	frame_rate = vpp_box->m_encode_context.video_enc_params.rc_params.h264_cbr_params.frame_rate;

	buffer = (media_codec_buffer_t *)(stream->frame_buffer);

	frame_info info;
	info.type		= codec_type;
	info.key		= pipline_id;
	info.seq		= buffer->vstream_buf.src_idx;
	info.pts		= buffer->vstream_buf.pts;
	info.length		= buffer->vstream_buf.size;
	info.t_time		= (unsigned int)time(0);
	info.framerate	= frame_rate;
	info.width		= vpp_box->m_encode_context.video_enc_params.width;
	info.height		= vpp_box->m_encode_context.video_enc_params.height;

	shm_stream_put(vpp_box->venc_shm, info, (unsigned char*)buffer->vstream_buf.vir_ptr, buffer->vstream_buf.size);
}

static int32_t alloc_graphic_buffer(hbn_vnode_image_t *img, int w, int h, int32_t format)
{
	int32_t ret = 0;

	// 一定需要是连续的内存buffer，否则解码出来的图像送进 vse 之后会有问题
	int64_t flags = HB_MEM_USAGE_MAP_INITIALIZED |
			HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD |
			HB_MEM_USAGE_CPU_READ_OFTEN |
			HB_MEM_USAGE_CPU_WRITE_OFTEN |
			HB_MEM_USAGE_CACHED |
			HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hb_mem_alloc_graph_buf(w, h, format, flags, w, h, &img->buffer);
	if (ret < 0)
		return ret;

	img->info.frame_id   = 0;
	img->info.timestamps = 0;
	img->info.frame_done  = 0;
	img->info.bufferindex = 0;

	return 0;
}


// 从解码器获取输出图像，然后送进vse模块
// 从 vse 的第一个通道里面获取编码图像，然后送进编码器
// 从 vse 的第二个通道里面获取编码图像，然后送进BPU模块
static void *get_decode_output_thread(void *ptr) {
	int32_t ret = 0;
	tsThread *privThread = (tsThread*)ptr;

	ImageFrame decode_frame = {0};
	ImageFrame vse_frame = {0};
	ImageFrame encode_frame = {0};
	ImageFrame encode_stream = {0};

	hbn_vnode_image_t src_img = {0};

	media_codec_buffer_t *decode_frame_buffer = NULL;
	bpu_buffer_info_t bpu_input_buffer = {0};

	vpp_box_t *vpp_box = (vpp_box_t *)privThread->pvThreadData;

	if (vp_allocate_image_frame(&decode_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for decode_frame failed, so exit program.");
		exit(-1);
	}

	if (vp_allocate_image_frame(&vse_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for vse_frame failed, so exit program.");
		exit(-1);
	}
	if (vp_allocate_image_frame(&encode_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for encode_frame failed, so exit program.");
		exit(-1);
	}
	if (vp_allocate_image_frame(&encode_stream) == NULL) {
		SC_LOGE("vp_allocate_image_frame for encode_stream failed, so exit program.");
		exit(-1);
	}

	mThreadSetName(privThread, __func__);

	ret = alloc_graphic_buffer(&src_img,
							vpp_box->vp_vflow_contex.vse_config.vse_ichn_attr.width,
							vpp_box->vp_vflow_contex.vse_config.vse_ichn_attr.height,
							MEM_PIX_FMT_NV12);
	if (ret < 0) {
		SC_LOGE("alloc_graphic_buffer failed");
		exit(-1);
	}

	char nv12_file_name[128];

	while (privThread->eState == E_THREAD_RUNNING) {
		ret = vp_codec_get_output(&vpp_box->m_decode_context, &decode_frame, VP_GET_FRAME_TIMEOUT);
		if (ret != 0) {
			usleep(30 * 1000);
			continue;
		}

		// 把解码后的数据送进vse模块，出两路图像
		decode_frame_buffer = (media_codec_buffer_t *)decode_frame.frame_buffer;
		vpp_video_frame_buffer_info_to_vnode_image(&decode_frame_buffer->vframe_buf,
			vse_frame.hbn_vnode_image);

		// SC_LOGW("+++++++++++++++++++ DECODE +++++++++++++++++++++++");
		// vp_codec_print_media_codec_output_buffer_info(&decode_frame);

		memcpy((char *)(src_img.buffer.virt_addr[0]),
			decode_frame_buffer->vframe_buf.vir_ptr[0],
			decode_frame_buffer->vframe_buf.width * decode_frame_buffer->vframe_buf.height);
		memcpy((char *)(src_img.buffer.virt_addr[1]),
			decode_frame_buffer->vframe_buf.vir_ptr[1],
			decode_frame_buffer->vframe_buf.width * decode_frame_buffer->vframe_buf.height / 2);
		// 使用解码出来的yuv的时间戳
		src_img.info.tv.tv_sec = decode_frame_buffer->vframe_buf.pts / 1000000;
		src_img.info.tv.tv_usec = decode_frame_buffer->vframe_buf.pts % 1000000;
		// SC_LOGW("src_img.info.tv: %ld.%06ld\n", src_img.info.tv.tv_sec, src_img.info.tv.tv_usec);

		if (log_ctrl_level_get(NULL) == LOG_TRACE) {
			vp_vin_print_hbn_vnode_image_t(&src_img);

			sprintf(nv12_file_name, "/tmp/box_vse_input_%dx%d_nv12_%ld.%06ld.yuv",
				decode_frame_buffer->vframe_buf.width, decode_frame_buffer->vframe_buf.height,
				src_img.info.tv.tv_sec, src_img.info.tv.tv_usec);
			vp_dump_2plane_yuv_to_file(nv12_file_name,
				src_img.buffer.virt_addr[0], src_img.buffer.virt_addr[1],
				decode_frame_buffer->vframe_buf.width * decode_frame_buffer->vframe_buf.height,
				decode_frame_buffer->vframe_buf.width * decode_frame_buffer->vframe_buf.height / 2);
		}

		ret = vp_vse_send_frame(&vpp_box->vp_vflow_contex, &src_img);
		if (ret != 0) {
			SC_LOGE("vp_vse_send_frame failed(%d)", ret);
			continue;
		}

		// 编码推流的时间一般比较短，而且时间固定，但是算法的运算时间与模型的选择强相关，并且模型的运行时异步进行的，所以先处理算法
		// 从第二通道获取数据给编码模块使用
		if (strlen(vpp_box->m_bpu_handle.m_model_name) > 0) {
			ret = vp_vse_get_frame(&vpp_box->vp_vflow_contex, 1, &vse_frame);
			if (ret != 0) {
				// 当线程接收到退出信号时，getframe 接口会立即报超时退出
				// 所以只有当线程是正常运行状态下的异常才属于真异常
				if (privThread->eState == E_THREAD_RUNNING) {
					SC_LOGE("vp_vse_get_frame chn 1 failed(%d).", ret);
				}
				continue;
			}

			if (log_ctrl_level_get(NULL) == LOG_TRACE) {
				sprintf(nv12_file_name, "/tmp/box_vse_chn1_%dx%d_nv12_size_%lu.yuv",
					vse_frame.hbn_vnode_image->buffer.width, vse_frame.hbn_vnode_image->buffer.height,
					vse_frame.hbn_vnode_image->buffer.size[0] + vse_frame.hbn_vnode_image->buffer.size[1]);
				vp_dump_yuv_to_file(nv12_file_name,
					vse_frame.hbn_vnode_image->buffer.virt_addr[0],
					vse_frame.hbn_vnode_image->buffer.size[0] + vse_frame.hbn_vnode_image->buffer.size[1]);
			}
			// 把yuv数据送进bpu进行算法运算
			// SC_LOGW("+++++++++++++++++++ VSE 0-1 +++++++++++++++++++++++");
			// vp_vin_print_hbn_vnode_image_t(vse_frame.hbn_vnode_image);
			memset(&bpu_input_buffer, 0, sizeof(bpu_buffer_info_t));
			vpp_graphic_buf_to_bpu_buffer_info(vse_frame.hbn_vnode_image,
				&bpu_input_buffer);
			// 这个地方一定要设置，从vse 获取的图像的时间戳在 tv 里面，如果是sensor出来的图像，时间戳在 timestamps 里面
			bpu_input_buffer.tv = src_img.info.tv;
			// print_bpu_buffer_info(&bpu_input_buffer);

			bpu_wrap_send_frame(&vpp_box->m_bpu_handle, &bpu_input_buffer);

			vp_vse_release_frame(&vpp_box->vp_vflow_contex, 1, &vse_frame);
		}

		// 从第一通道获取数据给编码模块使用
		ret = vp_vse_get_frame(&vpp_box->vp_vflow_contex, 0, &vse_frame);
		if (ret != 0) {
			// 当线程接收到退出信号时，getframe 接口会立即报超时退出
			// 所以只有当线程是正常运行状态下的异常才属于真异常
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("vp_vse_get_frame chn 0 failed(%d).", ret);
			}
			continue;
		}

		if (log_ctrl_level_get(NULL) == LOG_TRACE) {
			vp_vin_print_hbn_vnode_image_t(vse_frame.hbn_vnode_image);

			sprintf(nv12_file_name, "/tmp/box_vse_chn0_%dx%d_nv12_size_%lu.yuv",
				vse_frame.hbn_vnode_image->buffer.width, vse_frame.hbn_vnode_image->buffer.height,
				vse_frame.hbn_vnode_image->buffer.size[0] + vse_frame.hbn_vnode_image->buffer.size[1]);
			vp_dump_yuv_to_file(nv12_file_name,
				vse_frame.hbn_vnode_image->buffer.virt_addr[0],
				vse_frame.hbn_vnode_image->buffer.size[0] + vse_frame.hbn_vnode_image->buffer.size[1]);
		}
		// SC_LOGW("+++++++++++++++++++ VSE 0-0 +++++++++++++++++++++++");
		// vp_vin_print_hbn_vnode_image_t(vse_frame.hbn_vnode_image);
		// 送进编码器
		encode_frame.data[0] = vse_frame.hbn_vnode_image->buffer.virt_addr[0];
		encode_frame.data_size[0] = vse_frame.hbn_vnode_image->buffer.size[0] + vse_frame.hbn_vnode_image->buffer.size[1];
		encode_frame.image_timestamp = vse_frame.hbn_vnode_image->info.tv.tv_sec * 1000000
			+ vse_frame.hbn_vnode_image->info.tv.tv_usec;
		ret = vp_codec_set_input(&vpp_box->m_encode_context, &encode_frame, 0);
		if (ret != 0) {
			SC_LOGE("vp_codec_set_input send encode frame failed(%d)", ret);
			vp_vse_release_frame(&vpp_box->vp_vflow_contex, 0, &vse_frame);
			continue;
		}

		// 从编码器获取码流
		ret = vp_codec_get_output(&vpp_box->m_encode_context, &encode_stream, VP_GET_FRAME_TIMEOUT);
		if (ret != 0) {
			SC_LOGE("vp_codec_get_output get encode stream failed(%d)", ret);
			vp_vse_release_frame(&vpp_box->vp_vflow_contex, 0, &vse_frame);
			continue;
		}

		// SC_LOGW("+++++++++++++++++++ encode_stream +++++++++++++++++++++++");
		// vp_codec_print_media_codec_output_buffer_info(&encode_stream);

		// 推流
		vpp_box_push_stream(vpp_box, &encode_stream, vpp_box->pipline_id);

		// 把轮转 buffer queue 进队列
		vp_codec_release_output(&vpp_box->m_encode_context, &encode_stream);
		vp_vse_release_frame(&vpp_box->vp_vflow_contex, 0, &vse_frame);
		vp_codec_release_output(&vpp_box->m_decode_context, &decode_frame);

		// usleep(10 * 1000);
	}
	vp_free_image_frame(&decode_frame);
	vp_free_image_frame(&vse_frame);
	vp_free_image_frame(&encode_frame);
	vp_free_image_frame(&encode_stream);

	hb_mem_free_buf(src_img.buffer.fd[0]);

	mThreadFinish(privThread);
	return NULL;
}

int32_t vpp_box_init_param(void)
{
	int i, ret = 0;

	vpp_box_t *vpp_box = NULL;
	solution_cfg_box_vpp_t *cfg_box_vpp = NULL;
	vse_config_t *vse_config = NULL;
	int32_t input_width = 0, input_height = 0;
	int32_t model_width = 0, model_height = 0;

	memset(&g_vpp_box, 0, sizeof(g_vpp_box));

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		g_vpp_box[i].m_encode_context.codec_id = MEDIA_CODEC_ID_NONE;
		g_vpp_box[i].m_decode_context.codec_id = MEDIA_CODEC_ID_NONE;
	}

	for (i = 0; i < g_solution_config.box_solution.pipeline_count; i++) {
		vpp_box = &g_vpp_box[i];
		cfg_box_vpp = &g_solution_config.box_solution.box_vpp[i];
		strncpy(vpp_box->m_stream_path, cfg_box_vpp->stream,
				sizeof(vpp_box->m_stream_path) - 1);

		// 配置算法模型
		if (strlen(cfg_box_vpp->model) > 1 && strcmp(cfg_box_vpp->model, "null") != 0) {
			vpp_box->m_bpu_handle.m_vpp_id = i;
			strncpy(vpp_box->m_bpu_handle.m_model_name,
				cfg_box_vpp->model,
				sizeof(vpp_box->m_bpu_handle.m_model_name) - 1);
			vpp_box->m_bpu_handle.m_model_name[sizeof(vpp_box->m_bpu_handle.m_model_name) - 1] = '\0';
		}

		// 配置编码通道
		ret = vp_encode_config_param(&vpp_box->m_encode_context,
			VP_GET_MD_CODEC_TYPE(cfg_box_vpp->encode_type),
			cfg_box_vpp->encode_width,
			cfg_box_vpp->encode_height,
			cfg_box_vpp->encode_frame_rate,
			cfg_box_vpp->encode_bitrate, false);
		if (ret != 0) {
			SC_LOGE("Encode config param error, type:%d width:%d height:%d"
				" frame_rate: %d bit_rate:%d\n",
				VP_GET_MD_CODEC_TYPE(cfg_box_vpp->encode_type),
				cfg_box_vpp->encode_width,
				cfg_box_vpp->encode_height,
				cfg_box_vpp->encode_frame_rate,
				cfg_box_vpp->encode_bitrate);
		}

		// 配置解码通道
		ret = vp_decode_config_param(&vpp_box->m_decode_context,
			VP_GET_MD_CODEC_TYPE(cfg_box_vpp->decode_type),
			cfg_box_vpp->decode_width,
			ALIGN_16(cfg_box_vpp->decode_height));
		if (ret != 0)
		{
			SC_LOGE("Decode config param error, type:%d width:%d height:%d\n",
				VP_GET_MD_CODEC_TYPE(cfg_box_vpp->decode_type),
				cfg_box_vpp->decode_width,
				ALIGN_16(cfg_box_vpp->decode_height));
		}

		// 配置 vse 模块
		vse_config = &vpp_box->vp_vflow_contex.vse_config;
		input_width = cfg_box_vpp->decode_width;
		input_height = cfg_box_vpp->decode_height;

		vse_config->vse_ichn_attr.width =input_width;
		vse_config->vse_ichn_attr.height = input_height;
		vse_config->vse_ichn_attr.fmt = FRM_FMT_NV12;
		vse_config->vse_ichn_attr.bit_width = 8;

		// 第一个通道给编码器使用
		// 设置VSE通道0输出属性，ROI为原图大小，输出编码模块需要的图像
		vse_config->vse_ochn_attr[0].chn_en = CAM_TRUE;
		vse_config->vse_ochn_attr[0].roi.x = 0;
		vse_config->vse_ochn_attr[0].roi.y = 0;
		vse_config->vse_ochn_attr[0].roi.w = input_width;
		vse_config->vse_ochn_attr[0].roi.h = input_height;
		vse_config->vse_ochn_attr[0].target_w = cfg_box_vpp->encode_width;
		vse_config->vse_ochn_attr[0].target_h = cfg_box_vpp->encode_height;
		vse_config->vse_ochn_attr[0].fmt = FRM_FMT_NV12;
		vse_config->vse_ochn_attr[0].bit_width = 8;

		// 第二个通道的数据给BPU使用
		if (strlen(vpp_box->m_bpu_handle.m_model_name) > 1 && strcmp(vpp_box->m_bpu_handle.m_model_name, "null") != 0) {
			ret = bpu_wrap_get_model_hw(vpp_box->m_bpu_handle.m_model_name, &model_width, &model_height);
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
	}

	return ret;
}

int32_t vpp_box_init(void)
{
	int32_t i = 0, ret = 0;

	hb_mem_module_open();

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		g_vpp_box[i].venc_shm = NULL;
		if (strlen(g_vpp_box[i].m_stream_path) == 0)
			continue;

		// 初始化 VSE 来完成图像的缩放处理
		ret = vp_vse_init(&g_vpp_box[i].vp_vflow_contex);
		if (ret != 0) {
			SC_LOGE("vp_vse_init failed");
			return -1;
		}
		ret = vp_vflow_init(&g_vpp_box[i].vp_vflow_contex);
		if (ret != 0) {
			SC_LOGE("vp_vflow_init failed");
			return -1;
		}

		// 初始化编码器
		if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_init(&g_vpp_box[i].m_encode_context);
			if (ret != 0)
			{
				SC_LOGE("Encode vp_codec_init error(%d)", i);
				return -1;
			}
			SC_LOGI("Init video encode instance %d successful", g_vpp_box[i].m_encode_context.instance_index);
		}

		// 初始化解码器
		if (g_vpp_box[i].m_decode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_init(&g_vpp_box[i].m_decode_context);
			if (ret != 0)
			{
				SC_LOGE("Decode vp_codec_init error(%d)", i);
				return -1;
			}
			SC_LOGI("Init video decode instance %d successful", g_vpp_box[i].m_decode_context.instance_index);
		}

		// 初始化算法模块，初始化bpu
		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_model_init(&g_vpp_box[i].m_bpu_handle, g_vpp_box[i].m_bpu_handle.m_model_name);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_model_init failed");
			return -1;
		}
		// 注册算法结果回调函数
		bpu_wrap_callback_register(&g_vpp_box[i].m_bpu_handle,
			bpu_wrap_general_result_handle, &g_vpp_box[i].m_bpu_handle.m_vpp_id);
	}

	return 0;
}

int32_t vpp_box_uninit(void)
{
	int32_t i = 0, ret = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		if (strlen(g_vpp_box[i].m_stream_path) == 0)
			continue;

		vp_vflow_contex = &g_vpp_box[i].vp_vflow_contex;
		ret = vp_vflow_deinit(vp_vflow_contex);
		ret |= vp_vse_deinit(vp_vflow_contex);

		SC_ERR_CON_EQ(ret, 0, "vp_vse_deinit or vp_vflow_deinit failed");

		if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_deinit(&g_vpp_box[i].m_encode_context);
			if (ret != 0)
			{
				SC_LOGE("Encode vp_codec_deinit error(%d)", i);
				return -1;
			}
			SC_LOGI("Deinit video encode instance %d successful", g_vpp_box[i].m_encode_context.instance_index);
		}

		if (g_vpp_box[i].m_decode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_deinit(&g_vpp_box[i].m_decode_context);
			if (ret != 0)
			{
				SC_LOGE("Decode vp_codec_deinit error(%d)", i);
				return -1;
			}
			SC_LOGI("Deinit video decode instance %d successful", g_vpp_box[i].m_decode_context.instance_index);
		}

		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_deinit(&g_vpp_box[i].m_bpu_handle);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_model_init failed");
			return -1;
		}
	}

	hb_mem_module_close();

	vp_print_debug_infos();
	return 0;
}

int32_t vpp_box_start(void)
{
	int32_t i = 0, ret = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		if (strlen(g_vpp_box[i].m_stream_path) == 0)
			continue;
		g_vpp_box[i].pipline_id = i;
		vp_vflow_contex = &g_vpp_box[i].vp_vflow_contex;
		ret = vp_vse_start(vp_vflow_contex);
		ret |= vp_vflow_start(vp_vflow_contex);
		SC_ERR_CON_EQ(ret, 0, "vp_vse_start or vp_vflow_start failed");

		if(g_vpp_box[i].venc_shm == NULL) {
			T_SDK_VENC_INFO venc_chn_info;

			media_codec_context_t *codec_context = &g_vpp_box[i].m_encode_context;
			media_codec_id_t codec_type = codec_context->codec_id;

			venc_chn_info.channel = i;
			ret = SDK_Cmd_Impl(SDK_CMD_VPP_VENC_CHN_PARAM_GET, (void*)&venc_chn_info);

			char shm_id[32] = {0}, shm_name[32] = {0};
			sprintf(shm_id, "cam_id_%s_chn%d", codec_type == MEDIA_CODEC_ID_H264 ? "h264" :
					(codec_type == MEDIA_CODEC_ID_H265 ? "h265" :
					(codec_type == MEDIA_CODEC_ID_JPEG) ? "jpeg" : "other"), i);
			sprintf(shm_name, "name_%s_chn%d", codec_type == MEDIA_CODEC_ID_H264 ? "h264" :
					(codec_type == MEDIA_CODEC_ID_H265 ? "h265" :
					(codec_type == MEDIA_CODEC_ID_JPEG) ? "jpeg" : "other"), i);
			g_vpp_box[i].venc_shm = shm_stream_create(shm_id, shm_name,
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


		if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_start(&g_vpp_box[i].m_encode_context);
			if (ret != 0)
			{
				SC_LOGE("Encode vp_codec_start error(%d)", i);
				return -1;
			}
			SC_LOGI("Start video encode instance %d successful", g_vpp_box[i].m_encode_context.instance_index);

			g_vpp_box[i].m_venc_thread.pvThreadData = (void *)&g_vpp_box[i];
			mThreadStart(get_decode_output_thread, &g_vpp_box[i].m_venc_thread, E_THREAD_JOINABLE);
		}

		// 启动解码线程
		if (g_vpp_box[i].m_decode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_start(&g_vpp_box[i].m_decode_context);
			if (ret != 0)
			{
				SC_LOGE("Decode vp_codec_start error(%d)", i);
					return -1;
			}
			SC_LOGI("Start video decode instance %d successful", g_vpp_box[i].m_decode_context.instance_index);
			g_vpp_box[i].m_decode_param.context = &g_vpp_box[i].m_decode_context;
			strcpy(g_vpp_box[i].m_decode_param.stream_path, g_vpp_box[i].m_stream_path);
			g_vpp_box[i].m_vdec_thread.pvThreadData = (void*)&g_vpp_box[i].m_decode_param;
			mThreadStart(vp_decode_work_func, &g_vpp_box[i].m_vdec_thread, E_THREAD_JOINABLE);
		}

		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_start(&g_vpp_box[i].m_bpu_handle);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_start failed");
			return -1;
		}
	}

	vp_print_debug_infos();
	return 0;
}

static int32_t get_pipeline_id_by_video_id(int32_t video_id)
{
	int32_t i = 0;
	int32_t enable_pipeline_count = 0;
	// 遍历所有 pipeline
	// 用 enable_pipeline_count 记录使能的pipeline的编号，这个编号理论上与 web 上的video编号相等
	// 当 enable_pipeline_count == video_id时就说明找到了对应的pipeline
	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			enable_pipeline_count++;
			if (enable_pipeline_count == video_id) {
				return i;
			}
		}
	}
	return 0;
}

int32_t vpp_box_stop(void)
{
	int32_t i = 0, ret = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;

	// 先把所有线程停掉
	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		if (strlen(g_vpp_box[i].m_stream_path) == 0)
			continue;
		if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			// 结束编码线程
			mThreadStop(&g_vpp_box[i].m_venc_thread);
		}
		if (g_vpp_box[i].m_decode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			mThreadStop(&g_vpp_box[i].m_vdec_thread);
		}

		if(g_vpp_box[i].venc_shm != NULL){
			shm_stream_destory(g_vpp_box[i].venc_shm);
			g_vpp_box[i].venc_shm = NULL;
		}
	}

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		if (strlen(g_vpp_box[i].m_stream_path) == 0)
			continue;

		if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_stop(&g_vpp_box[i].m_encode_context);
			if (ret != 0)
			{
				SC_LOGE("Encode vp_codec_stop error(%d)", i);
				return -1;
			}
			SC_LOGI("Stop video encode instance %d successful", g_vpp_box[i].m_encode_context.instance_index);
		}

		if (g_vpp_box[i].m_decode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_stop(&g_vpp_box[i].m_decode_context);
			if (ret != 0)
			{
				SC_LOGE("Decode vp_codec_stop error(%d)", i);
				return -1;
			}
			SC_LOGI("Stop video decode instance %d successful", g_vpp_box[i].m_decode_context.instance_index);
		}

		vp_vflow_contex = &g_vpp_box[i].vp_vflow_contex;
		ret = vp_vflow_stop(vp_vflow_contex);
		ret |= vp_vse_stop(vp_vflow_contex);
		SC_ERR_CON_EQ(ret, 0, "vp_vflow_stop or vp_vse_stop failed");

		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) == 0)
			continue;
		// mThreadStop(&g_vpp_box[i].m_bpu_thread);
		ret = bpu_wrap_stop(&g_vpp_box[i].m_bpu_handle);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_start failed");
			return -1;
		}
	}

	return 0;
}

int32_t vpp_box_param_set(SOLUTION_PARAM_E type, char* val, uint32_t length)
{
	switch(type)
	{
	case SOLUTION_VENC_BITRATE_SET:
		{
			break;
		}
	default:
		break;
	}
	return 0;
}

int32_t vpp_box_param_get(SOLUTION_PARAM_E type, char* val, uint32_t* length)
{
	int32_t i= 0, ret = 0;
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
			if(i >= VPP_BOX_MAX_CHANNELS){
				SC_LOGE("box solutions max channel is %d, but get channel index is %d .", VPP_BOX_MAX_CHANNELS, i);
				return -1;
			}
			if(g_vpp_box[param->channel].m_encode_context.codec_id == MEDIA_CODEC_ID_NONE){
				SC_LOGE("box solutions channel %d is not enable, can't get encode param .", param->channel);
				return -1;
			}

			enc_params = &g_vpp_box[param->channel].m_encode_context.video_enc_params;
			param->enable = 1;
			param->width = enc_params->width;
			param->height = enc_params->height;
			param->stream_buf_size = enc_params->bitstream_buf_size;
			if (g_vpp_box[param->channel].m_encode_context.codec_id == MEDIA_CODEC_ID_H264) {
				param->type = 96;
				param->bitrate = enc_params->rc_params.h264_cbr_params.bit_rate;
				param->framerate = enc_params->rc_params.h264_cbr_params.frame_rate;
			} else if (g_vpp_box[param->channel].m_encode_context.codec_id == MEDIA_CODEC_ID_H265) {
				param->type = 265;
				param->bitrate = enc_params->rc_params.h265_cbr_params.bit_rate;
				param->framerate = enc_params->rc_params.h265_cbr_params.frame_rate;
			} else {
				SC_LOGE("unsupport codec_id %d, so exit.", g_vpp_box[param->channel].m_encode_context.codec_id);
				exit(-1);
			}
			vp_codec_get_user_buffer_param(enc_params, &param->suggest_buffer_region_size,
					&param->suggest_buffer_item_count);
			SC_LOGI("Codec_id: %d", g_vpp_box[param->channel].m_encode_context.codec_id);
			SC_LOGI("Instance Index: %d", g_vpp_box[param->channel].m_encode_context.instance_index);
			SC_LOGI("Param Channel: %d", param->channel);
			SC_LOGI("Param Enable: %d", param->enable);
			SC_LOGI("Param Width: %d", param->width);
			SC_LOGI("Param Height: %d", param->height);
			SC_LOGI("Param Stream Buffer Size: %d", param->stream_buf_size);
			SC_LOGI("Param Type: %d", param->type);
			SC_LOGI("Param Bitrate: %d", param->bitrate);
			SC_LOGI("Param Framerate: %d", param->framerate);
			break;
		}
	case SOLUTION_GET_VENC_CHN_STATUS: // 获取哪些编码通道被使能了
		{
			// 32位的整形，每个通道的状态占其中一个bit
			// 注： 64bit的值位与会有异常，待查
			unsigned int *status = (unsigned int *)val;
			*status = 0;
			int valid_index = 0;
			for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
				if (g_vpp_box[i].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
					*status |= (1 << valid_index);
					valid_index++;
				}
			}
			SC_LOGI("Box Solution current enabled status is [0x%x]\n", *status);
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
			ret = vp_vse_get_frame(&g_vpp_box[pipeline_id].vp_vflow_contex, 0, &image_frame);
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

			vp_vse_release_frame(&g_vpp_box[pipeline_id].vp_vflow_contex, 0, &image_frame);
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
