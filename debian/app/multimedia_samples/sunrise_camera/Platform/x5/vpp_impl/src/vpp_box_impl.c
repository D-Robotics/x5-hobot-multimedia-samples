// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

#define VPP_STEAM_COUNT 2
#define VPP_BOX_MAX_CHANNELS 8
typedef struct
{
	//for media server
	char stream_name[128];
	const char *media_type; 				//主码流和子码流 共同使用
	void *media_handler;			 		//handler for MediaServer

	//for codec
	media_codec_context_t m_encode_context;
	media_codec_user_config_t m_encode_user_config;

	//for multi thread
	tsThread 		m_venc_thread;			//从编码器获取图像, 发送给MediaServer

	int vflow_chn;
	void*   p_vpp_box;

	uint64_t first_frame_timestamp;
}vpp_codec_ctx_t;

typedef struct
{
	int pipline_id;

	//for decoder
	tsThread 		m_vdec_thread;
	char			m_stream_path[128];
	vp_decode_param_t m_decode_param;
	media_codec_context_t m_decode_context;
	
	//for vflow
	tsThread m_vflow_thread;
	vp_vflow_contex_t vp_vflow_contex;

	//for bpu
	bpu_handle_t	m_bpu_handle;
	int m_vse_for_bpu_channel;
	
	//for main and sub stream
	vpp_codec_ctx_t vpp_codec_ctxs[VPP_STEAM_COUNT];
} vpp_box_t;

static vpp_box_t g_vpp_box[VPP_BOX_MAX_CHANNELS];

static int32_t send_video_frame_info(int pipeline_id, int frame_id, int64_t timestamp)
{
	int32_t ret = 0;
	char *ws_msg = NULL;

	ws_msg = malloc(200);
	if (NULL == ws_msg) {
		SC_LOGE("Failed to allocate memory for ws_msg");
		return -1;
	}
	sprintf(ws_msg, "{\"kind\":11, \"pipeline\":%d, \"frame_id\":%d, \"timestamp\":%ld}", pipeline_id + 1, frame_id, timestamp);
	ret = SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_SEND_MSG, (void*)ws_msg);
	free(ws_msg);
	return ret;
}

static void vpp_box_push_stream(int pipline_id, vpp_codec_ctx_t *vpp_codec_ctx, ImageFrame *stream)
{
	if(stream == NULL) {
		SC_LOGE("Param is NULL");
		return;
	}
	media_codec_buffer_t *buffer = (media_codec_buffer_t *)(stream->frame_buffer);

	if(strcmp(vpp_codec_ctx->stream_name, "main") == 0){
		send_video_frame_info(pipline_id, buffer->vstream_buf.src_idx, buffer->vstream_buf.pts);
	}

	T_SDK_MEDIA_SRV_PUSH_PARAM push_param = {
		.media = vpp_codec_ctx->media_handler,
		.data = (const char*)buffer->vstream_buf.vir_ptr,
		.data_length = buffer->vstream_buf.size,
		.pts = buffer->vstream_buf.pts,
		.dts = buffer->vstream_buf.pts,
		.codec_name = vpp_codec_ctx->media_type
	};

	SDK_Cmd_Impl(SDK_CMD_MEDIA_SERVER_PUSH_DATA, &push_param);
}

static void *get_decode_and_vse_process_thread_func(void *ptr) {
	int32_t ret = 0;
	tsThread *privThread = (tsThread*)ptr;
	vpp_box_t *vpp_box = (vpp_box_t *)privThread->pvThreadData;

	ImageFrame decode_frame = {0};
	if (vp_allocate_image_frame(&decode_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for decode_frame failed, so exit program.");
		exit(-1);
	}

	ImageFrame vse_frame = {0};
	if (vp_allocate_image_frame(&vse_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for vse_frame failed, so exit program.");
		exit(-1);
	}
	mThreadSetName(privThread, __func__);
	
	hbn_vnode_image_t src_img = {0};
	ret = alloc_graphic_buffer(&src_img,
							vpp_box->vp_vflow_contex.vse_config.vse_ichn_attr.width,
							vpp_box->vp_vflow_contex.vse_config.vse_ichn_attr.height,
							MEM_PIX_FMT_NV12);
	if (ret < 0) {
		SC_LOGE("alloc_graphic_buffer failed");
		exit(-1);
	}


	int32_t vse_channel = vpp_box->m_vse_for_bpu_channel;
	while (privThread->eState == E_THREAD_RUNNING) {
		ret = vp_codec_get_output(&vpp_box->m_decode_context, &decode_frame, VP_DECODER_GET_FRAME_TIMEOUT);
		if (ret != 0) {
			usleep(30 * 1000);
			continue;
		}

		//构造 hbn_vnode_image_t， 解码器输出的是 common_buffer_t, 所以这里使用内存拷贝的方式
		media_codec_buffer_t*decode_frame_buffer = (media_codec_buffer_t *)decode_frame.frame_buffer;
		memcpy((char *)(src_img.buffer.virt_addr[0]),
			decode_frame_buffer->vframe_buf.vir_ptr[0],
			decode_frame_buffer->vframe_buf.width * decode_frame_buffer->vframe_buf.height);
		memcpy((char *)(src_img.buffer.virt_addr[1]),
			decode_frame_buffer->vframe_buf.vir_ptr[1],
			decode_frame_buffer->vframe_buf.width * decode_frame_buffer->vframe_buf.height / 2);
		src_img.info.tv.tv_sec = decode_frame_buffer->vframe_buf.pts / 1000000;
		src_img.info.tv.tv_usec = decode_frame_buffer->vframe_buf.pts % 1000000;
		src_img.info.timestamps = decode_frame_buffer->vframe_buf.pts;

		ret = vp_vse_send_frame(&vpp_box->vp_vflow_contex, &src_img);
		if (ret != 0) {
			SC_LOGE("pipeline %d vp_vse_send_frame failed(%d)", vpp_box->pipline_id, ret);
			break;
		}

		/**
		 * 获取 BPU 通道的视频帧率, 两个目的：
		 * 	1. 保证当前帧被VSE处理完成了，可以送入下一帧了
		 *  2. BPU 使能时，作为BPU的前处理 
		 */
		ret = vp_vse_get_frame(&vpp_box->vp_vflow_contex, vse_channel, &vse_frame);
		if (ret != 0) {
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("pipeline %d vp_vse_get_frame chn %d failed(%d).", 
						vpp_box->pipline_id, vse_channel, ret);
			}
			break;
		}
		if (strlen(vpp_box->m_bpu_handle.m_model_name) > 0) {
			bpu_buffer_info_t bpu_input_buffer = {0};
			memset(&bpu_input_buffer, 0, sizeof(bpu_buffer_info_t));
			vpp_graphic_buf_to_bpu_buffer_info(vse_frame.hbn_vnode_image, &bpu_input_buffer);
			bpu_input_buffer.tv = src_img.info.tv;
			bpu_wrap_send_frame(&vpp_box->m_bpu_handle, &bpu_input_buffer);
		}
		ret = vp_vse_release_frame(&vpp_box->vp_vflow_contex, vse_channel, &vse_frame);
		if (ret != 0) {
			SC_LOGE("pipeline %d vp_vse_release_frame failed", vpp_box->pipline_id);
			break;
		}

		ret = vp_codec_release_output(&vpp_box->m_decode_context, &decode_frame);
		if (ret != 0) {
			SC_LOGE("vp_codec_release_output failed");
			break;
		}
	}
	vp_free_image_frame(&decode_frame);
	vp_free_image_frame(&vse_frame);

	hb_mem_free_buf(src_img.buffer.fd[0]);
	mThreadFinish(privThread);
	return NULL;
}

static void* get_vse_and_codec_process_thread_func(void *ptr)
{
	int32_t ret = 0;

	//handler
	tsThread *privThread = (tsThread*)ptr;
	vpp_codec_ctx_t *p_vpp_codec_ctx = (vpp_codec_ctx_t *)privThread->pvThreadData;
	vpp_box_t *p_vpp_box = (vpp_box_t *)p_vpp_codec_ctx->p_vpp_box;

	mThreadSetNameWidthIndex(privThread, __func__, p_vpp_box->pipline_id);

	//for vflow
	ImageFrame vse_frame = {0};
	int vflow_chn = p_vpp_codec_ctx->vflow_chn;

	ImageFrame encode_stream = {0};
	//for frames
	if (vp_allocate_image_frame(&vse_frame) == NULL) {
		SC_LOGE("vp_allocate_image_frame for vse_frame failed, so exit program.");
		exit(-1);
	}
	if (vp_allocate_image_frame(&encode_stream) == NULL) {
		SC_LOGE("vp_allocate_image_frame for encode_stream failed, so exit program.");
		exit(-1);
	}

	//for debug
	int vflow_wait_count = 0;
	uint8_t is_geted_codec_stream = 0;
	while (privThread->eState == E_THREAD_RUNNING){

		//get frame from vflow
		ret = vp_vse_get_frame(&p_vpp_box->vp_vflow_contex, vflow_chn, &vse_frame);
		if (ret != 0) {
			vflow_wait_count++;
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("[%d] [%s] vp_vse_get_frame chn %d failed(%d), and wait %d.",
						p_vpp_box->pipline_id, p_vpp_codec_ctx->stream_name, vflow_chn, ret, vflow_wait_count);
				continue;
			}else{
				break;
			}
		}else{
			vflow_wait_count = 0;
		}

		ret = vp_codec_encoder_set_input(&p_vpp_codec_ctx->m_encode_context, &vse_frame);
		if(ret != 0){
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("pipline %d stream %s vp_codec_encoder_set_input failed !!!", p_vpp_box->pipline_id, p_vpp_codec_ctx->stream_name);
			}
			exit(-1); //无法处理
		}

		is_geted_codec_stream = 0;
		while(privThread->eState == E_THREAD_RUNNING){
			// 从编码器获取码流
			ret = vp_codec_get_output(&p_vpp_codec_ctx->m_encode_context, &encode_stream, 2000);
			if(ret != 0){
				if (privThread->eState == E_THREAD_RUNNING) {
					SC_LOGE("channel %d stream %s vp_codec_get_output failed %d.",
						p_vpp_box->pipline_id, p_vpp_codec_ctx->stream_name, ret);
				}
				if(ret == -2){
					continue;
				}else{
					exit(-1);
				}
			}else{
				is_geted_codec_stream = 1;
				break;
			}
		}

		if(is_geted_codec_stream){
			vpp_box_push_stream(p_vpp_box->pipline_id, p_vpp_codec_ctx, &encode_stream);
		}

		ret = vp_vse_release_frame(&p_vpp_box->vp_vflow_contex, vflow_chn, &vse_frame);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed");
			exit(-1);
		}
		ret = vp_codec_release_output(&p_vpp_codec_ctx->m_encode_context, &encode_stream);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed");
			exit(-1);
		}
	}

	vp_free_image_frame(&encode_stream);
	vp_free_image_frame(&vse_frame);
	mThreadFinish(privThread);

	SC_LOGI("channel %d stream %s codec thread exit.\n", p_vpp_box->pipline_id, p_vpp_codec_ctx->stream_name);
	return NULL;
}

int32_t vpp_box_init_param_full(solution_cfg_t *solution_config)
{
	int i, ret = 0, vse_chn = 0;

	vpp_box_t *vpp_box = NULL;
	solution_cfg_box_vpp_t *cfg_box_vpp = NULL;
	vse_config_t *vse_config = NULL;

	memset(&g_vpp_box, 0, sizeof(g_vpp_box));

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		g_vpp_box[i].m_decode_context.codec_id = MEDIA_CODEC_ID_NONE;
		for (int j = 0; j < VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			p_vpp_codec_ctx->m_encode_context.codec_id = MEDIA_CODEC_ID_NONE;
		}
	}

	for (i = 0; i < solution_config->box_solution.pipeline_count; i++) {
		vpp_box = &g_vpp_box[i];
		cfg_box_vpp = &solution_config->box_solution.box_vpp[i];
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
		int input_width = cfg_box_vpp->decode_width;
		int input_height = cfg_box_vpp->decode_height;
		
		// 配置编码通道
		for (int j = 0; j < VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &vpp_box->vpp_codec_ctxs[j];
			p_vpp_codec_ctx->p_vpp_box = vpp_box;
			int target_w = cfg_box_vpp->encode_width;
			int target_h = cfg_box_vpp->encode_height;
			if(j != 0){
				vpp_get_sub_stream_resolution(input_width, input_height, &target_w, &target_h);
			}
			media_codec_user_config_t *codec_user_config = &p_vpp_codec_ctx->m_encode_user_config;
			codec_user_config->bit_rate = cfg_box_vpp->encode_bitrate;
			codec_user_config->codec_type = VP_GET_MD_CODEC_TYPE(cfg_box_vpp->encode_type);
			codec_user_config->frame_rate = cfg_box_vpp->encode_frame_rate;
			codec_user_config->width = target_w;
			codec_user_config->height = target_h;

			codec_user_config->input_buffer_is_extrenal = true;
			codec_user_config->input_buffer_count = 0;
			codec_user_config->output_buffer_count = 5;
			ret = vp_encode_config_param(&p_vpp_codec_ctx->m_encode_context, codec_user_config);
			if (ret != 0) {
				SC_LOGE("Encode config param error, type:%d width:%d height:%d"
					" frame_rate: %d bit_rate:%d\n",
					VP_GET_MD_CODEC_TYPE(cfg_box_vpp->encode_type),
					cfg_box_vpp->encode_width,
					cfg_box_vpp->encode_height,
					cfg_box_vpp->encode_frame_rate,
					cfg_box_vpp->encode_bitrate);
			}
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
		vse_config->vse_ichn_attr.width =input_width;
		vse_config->vse_ichn_attr.height = input_height;
		vse_config->vse_ichn_attr.fmt = FRM_FMT_NV12;
		vse_config->vse_ichn_attr.bit_width = 8;
		SC_LOGD("pipeline %d: input_width: %d input_height: %d", i, input_width, input_height);
		for (int j = 0; j < VPP_STEAM_COUNT; j++){
			int target_w = input_width;
			int target_h = input_height;
			if(j != 0){
				vpp_get_sub_stream_resolution(input_width, input_height, &target_w, &target_h);
			}
			vpp_codec_ctx_t *p_vpp_codec_ctx = &vpp_box->vpp_codec_ctxs[j];
			p_vpp_codec_ctx->vflow_chn = j;
			vse_config->vse_ochn_attr[j].chn_en = CAM_TRUE; //缩小通道: 4K
			vse_config->vse_ochn_attr[j].roi.x = 0;
			vse_config->vse_ochn_attr[j].roi.y = 0;
			vse_config->vse_ochn_attr[j].roi.w = input_width;
			vse_config->vse_ochn_attr[j].roi.h = input_height;
			vse_config->vse_ochn_attr[j].target_w = target_w;
			vse_config->vse_ochn_attr[j].target_h = target_h;
			vse_config->vse_ochn_attr[j].fmt = FRM_FMT_NV12;
			vse_config->vse_ochn_attr[j].bit_width = 8;
			if(VPP_STEAM_COUNT > 3 /*VSE输出可以大于1080P的通道：0 1 2*/){
				SC_LOGE("vpp stream count max is 3, but %d", VPP_STEAM_COUNT);
				exit(-1);
			}
			SC_LOGI("[%d] VSE channel %d: out_width: %d out_height: %d ",
				i, j, target_w, target_h);

		}

		// BPU 前处理
		int32_t model_width = 512, model_height = 512;
		if (strlen(vpp_box->m_bpu_handle.m_model_name) > 1 && strcmp(vpp_box->m_bpu_handle.m_model_name, "null") != 0) {
			ret = bpu_wrap_get_model_hw(vpp_box->m_bpu_handle.m_model_name, &model_width, &model_height);
			if(ret != 0){
				return -1;
			}
		}
		// 无论BPU是否是能都要 使能对应的VSE通道: vse_send 的线程知道何时释放 vse 输入帧
		if (model_width > input_width || model_height > input_height){
			vse_chn = 5;
		}else{
			vse_chn = 4;
		}				
		vse_config->vse_ochn_attr[vse_chn].chn_en = CAM_TRUE;
		vse_config->vse_ochn_attr[vse_chn].roi.x = 0;
		vse_config->vse_ochn_attr[vse_chn].roi.y = 0;
		vse_config->vse_ochn_attr[vse_chn].roi.w = input_width;
		vse_config->vse_ochn_attr[vse_chn].roi.h = input_height;
		vse_config->vse_ochn_attr[vse_chn].target_w = model_width;
		vse_config->vse_ochn_attr[vse_chn].target_h = model_height;
		vse_config->vse_ochn_attr[vse_chn].fmt = FRM_FMT_NV12;
		vse_config->vse_ochn_attr[vse_chn].bit_width = 8;
		vpp_box->m_vse_for_bpu_channel = vse_chn;	
	}

	return ret;
}
int32_t vpp_box_init_param(void)
{
	return vpp_box_init_param_full(&g_solution_config);
}

int32_t vpp_box_decode_param_get(solution_cfg_t* solution_cfg, solution_decode_param_info_t *solution_param_info){
	solution_cfg_box_vpp_t *cfg_box_vpp = NULL;
	solution_param_info->valid_count = 0;
	for (int i = 0; i < solution_cfg->box_solution.pipeline_count; i++) {
		cfg_box_vpp = &solution_cfg->box_solution.box_vpp[i];
		solution_decode_param_single_t *param_single = &solution_param_info->params[solution_param_info->valid_count];
		param_single->input_file = cfg_box_vpp->stream;
		if(cfg_box_vpp->decode_type == MEDIA_CODEC_ID_H264){
			param_single->codec_type = "h264";
		}else if(cfg_box_vpp->decode_type == MEDIA_CODEC_ID_H265){
			param_single->codec_type = "h265";
		}else if(cfg_box_vpp->decode_type == MEDIA_CODEC_ID_JPEG){
			param_single->codec_type = "jpeg";
		}else{
			SC_LOGI("%d recv unsupport codec type %d, so exit.", cfg_box_vpp->decode_type);
			exit(-1);
		}

		solution_param_info->valid_count++;

		SC_LOGI("vpp_box_decode_param_get [%d] input file %s, codec type is %s",
			solution_param_info->valid_count, param_single->input_file, param_single->codec_type);
	}
	return 0;
}


int32_t vpp_box_ion_param_get(solution_cfg_t* solution_cfg, solution_ion_param_info_t *solution_param_info){
	return 0;
}
int32_t vpp_box_vpu_param_get(solution_cfg_t* solution_cfg, solution_vpu_param_info_t *solution_param_info){
	solution_cfg_box_vpp_t *cfg_box_vpp = NULL;
	solution_param_info->valid_count = 0;
	for (int i = 0; i < solution_cfg->box_solution.pipeline_count; i++) {
		cfg_box_vpp = &solution_cfg->box_solution.box_vpp[i];
		vp_codec_usr_param_single_t *param_single = &solution_param_info->params[solution_param_info->valid_count];
		param_single->encode.width = cfg_box_vpp->encode_width;
		param_single->encode.height = cfg_box_vpp->encode_height;
		param_single->encode.fps = cfg_box_vpp->encode_frame_rate;

		param_single->decode.width = cfg_box_vpp->decode_width;
		param_single->decode.height = cfg_box_vpp->decode_height;
		param_single->decode.fps = cfg_box_vpp->decode_frame_rate;
		solution_param_info->valid_count++;

		SC_LOGI("vpp_box_vpu_param_get [%d] [encode:%d %d %d] [decode:%d %d %d]",
			solution_param_info->valid_count,
			param_single->encode.width, param_single->encode.height, param_single->encode.fps,
			param_single->decode.width, param_single->decode.height, param_single->decode.fps);
	}
	return 0;
}

int32_t vpp_box_init(void)
{
	int32_t i = 0, ret = 0;

	hb_mem_module_open();

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
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
		
		//初始化编码器
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			ret = vp_codec_init(&p_vpp_codec_ctx->m_encode_context);
			if (ret != 0){
				SC_LOGE("Encode vp_codec_init error(%d)", i);
				return -1;
			}
		}

		// 初始化解码器
		if (g_vpp_box[i].m_decode_context.codec_id != MEDIA_CODEC_ID_NONE) {
			ret = vp_codec_init(&g_vpp_box[i].m_decode_context);
			if (ret != 0){
				SC_LOGE("Decode vp_codec_init error(%d)", i);
				return -1;
			}
			SC_LOGI("Init video decode instance %d successful", g_vpp_box[i].m_decode_context.instance_index);
		}

		// 初始化算法模块，初始化bpu
		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) != 0){
			ret = bpu_wrap_model_init(&g_vpp_box[i].m_bpu_handle, g_vpp_box[i].m_bpu_handle.m_model_name);
			if (ret != 0) {
				SC_LOGE("bpu_wrap_model_init failed");
				return -1;
			}
			// 注册算法结果回调函数
			bpu_wrap_callback_register(&g_vpp_box[i].m_bpu_handle,
				bpu_wrap_general_result_handle, &g_vpp_box[i].m_bpu_handle.m_vpp_id);
		}
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
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			ret |= vp_codec_deinit(&p_vpp_codec_ctx->m_encode_context);
		}
		ret |= vp_codec_deinit(&g_vpp_box[i].m_decode_context);
		SC_ERR_CON_EQ(ret, 0, "vp_vse_deinit or vp_vflow_deinit failed");

		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) != 0){
			ret = bpu_wrap_deinit(&g_vpp_box[i].m_bpu_handle);
			if (ret != 0) {
				SC_LOGE("bpu_wrap_model_init failed");
				return -1;
			}
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
		///////////////////////////////////////////////
		ret = vp_vse_start(vp_vflow_contex);
		ret |= vp_vflow_start(vp_vflow_contex);
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			ret |= vp_codec_start(&p_vpp_codec_ctx->m_encode_context);
		}
		ret |= vp_codec_start(&g_vpp_box[i].m_decode_context);
		SC_ERR_CON_EQ(ret, 0, "vp_vse_start or vp_vflow_start failed");
		
		//流媒体
		char meida_name[64];
		sprintf(meida_name, "ch%d", i);
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			p_vpp_codec_ctx->media_type = vp_codec_get_codec_type_string(p_vpp_codec_ctx->m_encode_context.codec_id);
			if(j == 0){
				sprintf(p_vpp_codec_ctx->stream_name, "main");
			}else{
				sprintf(p_vpp_codec_ctx->stream_name, "sub%d", j);
			}

			T_SDK_MEDIA_SRV_CREATE_PARAM create_param = {
				.media_name = meida_name,
				.stream_name = p_vpp_codec_ctx->stream_name,
				.codec_type_name = p_vpp_codec_ctx->media_type,
				.media = NULL,
			};

			SDK_Cmd_Impl(SDK_CMD_MEDIA_SERVER_CREATE, &create_param);
		 		p_vpp_codec_ctx->media_handler = create_param.media;
		}
		
		///////////////////////////////////////////////
		// 线程: 文件读取并送入解码器
		SC_LOGI("Start video decode instance %d successful", g_vpp_box[i].m_decode_context.instance_index);
		g_vpp_box[i].m_decode_param.context = &g_vpp_box[i].m_decode_context;
		strcpy(g_vpp_box[i].m_decode_param.stream_path, g_vpp_box[i].m_stream_path);
		g_vpp_box[i].m_vdec_thread.pvThreadData = (void*)&g_vpp_box[i].m_decode_param;
		mThreadStart(vp_decode_work_func, &g_vpp_box[i].m_vdec_thread, E_THREAD_JOINABLE);

		// 线程：读取解码器 送入VSE
		g_vpp_box[i].m_vflow_thread.pvThreadData = (void*)&g_vpp_box[i];
		mThreadStart(get_decode_and_vse_process_thread_func, &g_vpp_box[i].m_vflow_thread, E_THREAD_JOINABLE);

		// 线程: 读取VSE 送入编码器
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			p_vpp_codec_ctx->m_venc_thread.pvThreadData = (void*)p_vpp_codec_ctx;
			mThreadStart(get_vse_and_codec_process_thread_func, &p_vpp_codec_ctx->m_venc_thread, E_THREAD_JOINABLE);
		}

		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) != 0){
			ret = bpu_wrap_start(&g_vpp_box[i].m_bpu_handle);
			if (ret != 0) {
				SC_LOGE("bpu_wrap_start failed");
				return -1;
			}
			SC_LOGI("Start BPU %d process successful, %s", i, g_vpp_box[i].m_bpu_handle.m_model_name);	
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
		if (g_vpp_box[i].vpp_codec_ctxs[0].m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
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

		// 线程: 读取VSE 送入编码器
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			p_vpp_codec_ctx->m_venc_thread.pvThreadData = (void*)p_vpp_codec_ctx;
			mThreadStop(&p_vpp_codec_ctx->m_venc_thread);
		}
		mThreadStop(&g_vpp_box[i].m_vflow_thread);
		mThreadStop(&g_vpp_box[i].m_vdec_thread);
	}

	for (i = 0; i < VPP_BOX_MAX_CHANNELS; i++) {
		if (strlen(g_vpp_box[i].m_stream_path) == 0)
			continue;
		
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			ret = vp_codec_stop(&p_vpp_codec_ctx->m_encode_context);
			if (ret != 0){
				SC_LOGE("Encode vp_codec_stop error(%d)", i);
				return -1;
			}
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

		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[j];
			SDK_Cmd_Impl(SDK_CMD_MEDIA_SERVER_DESTROY, p_vpp_codec_ctx->media_handler);
			p_vpp_codec_ctx->media_handler = NULL;
			p_vpp_codec_ctx->media_type = NULL;
		}

		if (strlen(g_vpp_box[i].m_bpu_handle.m_model_name) == 0)
			continue;

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
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[param->channel].vpp_codec_ctxs[0];
			if(p_vpp_codec_ctx->m_encode_context.codec_id == MEDIA_CODEC_ID_NONE){
				SC_LOGE("box solutions channel %d is not enable, can't get encode param .", param->channel);
				return -1;
			}
			
			enc_params = &p_vpp_codec_ctx->m_encode_context.video_enc_params;
			param->enable = 1;
			param->width = enc_params->width;
			param->height = enc_params->height;
			param->stream_buf_size = enc_params->bitstream_buf_size;
			if (p_vpp_codec_ctx->m_encode_context.codec_id == MEDIA_CODEC_ID_H264) {
				param->type = 96;
				param->bitrate = enc_params->rc_params.h264_cbr_params.bit_rate;
				param->framerate = enc_params->rc_params.h264_cbr_params.frame_rate;
			} else if (p_vpp_codec_ctx->m_encode_context.codec_id == MEDIA_CODEC_ID_H265) {
				param->type = 265;
				param->bitrate = enc_params->rc_params.h265_cbr_params.bit_rate;
				param->framerate = enc_params->rc_params.h265_cbr_params.frame_rate;
			} else {
				SC_LOGE("unsupport codec_id %d, so exit.", p_vpp_codec_ctx->m_encode_context.codec_id);
				exit(-1);
			}
			vp_codec_get_user_buffer_param(enc_params, &param->suggest_buffer_region_size,
					&param->suggest_buffer_item_count);
			SC_LOGI("Codec_id: %d", p_vpp_codec_ctx->m_encode_context.codec_id);
			SC_LOGI("Instance Index: %d", p_vpp_codec_ctx->m_encode_context.instance_index);
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
				vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_box[i].vpp_codec_ctxs[0];
				if (p_vpp_codec_ctx->m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
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
