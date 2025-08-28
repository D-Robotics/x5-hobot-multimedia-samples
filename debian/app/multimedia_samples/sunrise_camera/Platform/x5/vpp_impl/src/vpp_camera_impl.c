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

#include "utils/nalu_utils.h"
#include "utils/utils_log.h"
#include "utils/cqueue.h"
#include "utils/common_utils.h"
#include "utils/stream_define.h"
#include "utils/mthread.h"
#include "utils/mqueue.h"
#include "utils/time_utils.h"

#include "model_info.h"

#include "bpu_wrap.h"
#include "vp_wrap.h"
#include "vp_codec.h"
#include "vp_sensors.h"
#include "vp_display.h"

#include "vp_gdc.h"

#include "vpp_preparam.h"
#include "vpp_camera_impl.h"

#define VPP_STEAM_COUNT 2
#define VPP_CAM_MAX_CHANNELS 32
#define VPP_VSE_OUTBUFFER_COUNT 3
#define VPP_VSE_OUTBUFFER_RELEASE_COUNT 2


typedef enum {
	WaitFrameFromFlowQueue = 0,
	SendToCodec,
	WaitDataFromCodec,
	GiveBackFrameToFlow,
	GiveBackFrameToFlowQueue,
	SendVideoFrameInfo,
	PushStreamToMediaServer,
	GiveBackDataToCodec,
	EncodeThreadStepSentry,
}encode_thread_step_t;

typedef enum {
	BPUWaitFrameFromFlowQueue = 0,
	SendFrameToBPUQueue,
	BPUGiveBackFrameToFlow,
	BPUThreadStepSentry
}bpu_thread_step_t;
const char* encode_thread_step_name(encode_thread_step_t step) {
	static const char* step_names[] = {
		"WaitFrameFromFlowQueue",
		"SendToCodec",
		"WaitDataFromCodec",
		"GiveBackFrameToFlow",
		"GiveBackFrameToFlowQueue",
		"SendVideoFrameInfo",
		"PushStreamToMediaServer",
		"GiveBackDataToCodec",
		"EncodeThreadStepSentry"
	};
	if (step < EncodeThreadStepSentry) {
		return step_names[step];
	}
	return "UnknownEncodeStep";
}
const char* bpu_thread_step_name(bpu_thread_step_t step) {
	static const char* step_names[] = {
		"BPUWaitFrameFromFlowQueue",
		"SendFrameToBPUQueue",
		"BPUGiveBackFrameToFlow",
		"BPUThreadStepSentry"
	};
	if (step < BPUThreadStepSentry) {
		return step_names[step];
	}
	return "UnknownBpuStep";
}

typedef struct
{
	//for media server
	char stream_name[128];
	const char *media_type; //主码流和子码流 共同使用
	void *media_handler; // media handler for MediaServer

	//for display
	int stream_index;
	//for codec
	media_codec_context_t m_encode_context;
	media_codec_user_config_t m_encode_user_config;

	//for multi thread
	tsThread 		m_vflow_thread;		/* 从vse获取图像，送入编码 */
	tsThread 		m_venc_thread;		/*从编码器获取图像，送入共享内存 */
	tsQueue			m_vflow_to_enc_queue;
	tsQueue			m_enc_to_vflow_queue;
	void*   p_vpp_camera;
	int vflow_chn;
	int8_t osd_chn;
	//for debug
	int vflow_thread_run_counter;
	int codec_thread_run_counter;
	int vflow_frame_counter;
	int vflow_buffer_used_count;

	encode_thread_step_t encode_thread_step;
	uint64_t first_frame_timestamp;
}vpp_codec_ctx_t;

typedef struct
{
	//for app info
	int pipline_id;
	int vpp_impl_index;

	//for drm
	int drm_init_succesed;
	vp_drm_context_t *drm_context;

	//for vflow
	vp_vflow_contex_t vp_vflow_contex;

	//for vflow and codec
	vpp_codec_ctx_t vpp_codec_ctxs[VPP_STEAM_COUNT];

	//for bpu
	tsThread		m_bpu_thread;
	bpu_handle_t	m_bpu_handle;
	int m_vse_for_bpu_channel;
	bpu_model_user_info_t bpu_model_user_info;

	//for debug: run times
	int bpu_thread_run_counter;

	//for debug: check is blocked
	bpu_thread_step_t bpu_thread_step;
} vpp_camera_t;

static vp_drm_context_t g_drm_context;
static vpp_camera_t g_vpp_camera[VPP_CAM_MAX_CHANNELS];


static void vp_print_debug_infos_for_multithread(vpp_codec_ctx_t *p_vpp_codec_ctx){

	vpp_camera_t *p_vpp_camera = (vpp_camera_t *)p_vpp_codec_ctx->p_vpp_camera;
	SC_LOGI("[%d][%s] thread counter: [flow thread: %d][encode thread: %d] [bpu thread: %d]",
		p_vpp_camera->pipline_id,
		p_vpp_codec_ctx->stream_name,
		p_vpp_codec_ctx->vflow_thread_run_counter,
		p_vpp_codec_ctx->codec_thread_run_counter,
		p_vpp_camera->bpu_thread_run_counter);
	SC_LOGI("[%d][%s] thread status: [encode thread: %s] [bpu thread: %s]\n",
		p_vpp_camera->pipline_id,
		p_vpp_codec_ctx->stream_name,
		encode_thread_step_name(p_vpp_codec_ctx->encode_thread_step),
		bpu_thread_step_name(p_vpp_camera->bpu_thread_step));

	SC_LOGI("[%d][%s] queue status:[vse_to_enc_queue: %d] [enc_to_vse_queue:%d]\n",
		p_vpp_camera->pipline_id,
		p_vpp_codec_ctx->stream_name,
		mQueueGetCount(&p_vpp_codec_ctx->m_vflow_to_enc_queue),
		mQueueGetCount(&p_vpp_codec_ctx->m_enc_to_vflow_queue));
	int get_frame_form_flow = __sync_fetch_and_add(&p_vpp_codec_ctx->vflow_frame_counter, 0);
	SC_LOGI("[%d][%s] get frame from flow: %d\n", p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, get_frame_form_flow);
}
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

static void vpp_camera_push_stream(int pipeline_id, int video_id, vpp_codec_ctx_t *vpp_codec_ctx, ImageFrame *stream)
{
	if(stream == NULL) {
		SC_LOGE("Param is NULL");
		return;
	}
	__sync_lock_test_and_set(&vpp_codec_ctx->encode_thread_step, SendVideoFrameInfo);
	media_codec_buffer_t *buffer = (media_codec_buffer_t *)(stream->frame_buffer);
	if(vpp_codec_ctx->first_frame_timestamp == 0){
		vpp_codec_ctx->first_frame_timestamp = buffer->vstream_buf.pts / 1000;
		SC_LOGI("channel %d stream %s recved first frame, and pts is %lld.", pipeline_id, vpp_codec_ctx->stream_name, buffer->vstream_buf.pts / 1000);
	}
	if(strcmp(vpp_codec_ctx->stream_name, "main") == 0){
		send_video_frame_info(video_id, buffer->vstream_buf.src_idx, buffer->vstream_buf.pts);
	}


	T_SDK_MEDIA_SRV_PUSH_PARAM push_param = {
		.media = vpp_codec_ctx->media_handler,
		.data = (const char*)buffer->vstream_buf.vir_ptr,
		.data_length = buffer->vstream_buf.size,
		.pts = buffer->vstream_buf.pts /1000,
		.dts = buffer->vstream_buf.pts /1000,
		.codec_name = vpp_codec_ctx->media_type
	};

	__sync_lock_test_and_set(&vpp_codec_ctx->encode_thread_step, PushStreamToMediaServer);
	SDK_Cmd_Impl(SDK_CMD_MEDIA_SERVER_PUSH_DATA, &push_param);
}
static void update_osd_info(vp_vflow_contex_t* vp_vflow_contex, uint64_t *next_update_time_ms, int osd_chn){
	uint64_t current_time_ms = get_timestamp_ms();
	if(osd_chn < 0){
		return;
	}
	if(current_time_ms > *next_update_time_ms){
		char world_time_string[100];
		get_world_time_string(world_time_string, sizeof(world_time_string));
		vp_osd_draw_world(vp_vflow_contex, osd_chn, world_time_string);
		*next_update_time_ms = (current_time_ms / 1000) * 1000 + 1000;
	}
}
static void* venc_get_stream_proc(void *ptr)
{
	int32_t ret = 0;
	tsThread *privThread = (tsThread*)ptr;
	vpp_codec_ctx_t *p_vpp_codec_ctx = (vpp_codec_ctx_t *)privThread->pvThreadData;
	vpp_camera_t *p_vpp_camera = (vpp_camera_t *)p_vpp_codec_ctx->p_vpp_camera;

	ImageFrame encode_stream = {0};
	if (vp_allocate_image_frame(&encode_stream) == NULL) {
		SC_LOGE("vp_allocate_image_frame for encode_stream failed, so exit program.");
		exit(-1);
	}
	teQueueStatus status = E_QUEUE_OK;
	ImageFrame vse_frame = {0};
	hbn_vnode_image_t *hbn_vnode_image = NULL;

	uint8_t is_geted_codec_stream = 0;

	int dequeue_enc_count = 0;
	int enqueue_vse_count = 0;
	int vflow_chn = p_vpp_codec_ctx->vflow_chn;
	//线程退出时，保证hbn_vnode_image 已经处理完, 处理的方式是放到 m_enc_to_vflow_queue
	while (privThread->eState == E_THREAD_RUNNING){
		__sync_lock_test_and_set(&p_vpp_codec_ctx->encode_thread_step, WaitFrameFromFlowQueue);

		status = mQueueDequeueTimed(&p_vpp_codec_ctx->m_vflow_to_enc_queue, 2000, (void **)&hbn_vnode_image);
		if(status != E_QUEUE_OK){
			SC_LOGI("channel %d stream %s dequeue from vse_to_enc_queue failed:%d\n", p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, status);
			continue;
		}
		dequeue_enc_count++;

		// 送进编码器
		__sync_lock_test_and_set(&p_vpp_codec_ctx->encode_thread_step, SendToCodec);
		vse_frame.hbn_vnode_image = hbn_vnode_image;
		ret = vp_codec_encoder_set_input(&p_vpp_codec_ctx->m_encode_context, &vse_frame);
		if(ret != 0){
			if (privThread->eState == E_THREAD_RUNNING) {
				SC_LOGE("pipline %d stream %s vp_codec_set_input failed.", p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name);
			}
			break;
		}
		__sync_lock_test_and_set(&p_vpp_codec_ctx->encode_thread_step, WaitDataFromCodec);
		is_geted_codec_stream = 0;
		while(privThread->eState == E_THREAD_RUNNING){
			// 从编码器获取码流
			ret = vp_codec_get_output(&p_vpp_codec_ctx->m_encode_context, &encode_stream, 2000);
			if(ret != 0){
				if (privThread->eState == E_THREAD_RUNNING) {
					SC_LOGE("channel %d stream %s vp_codec_get_output failed %d.",
						p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, ret);
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

		// 编码器用完VSE的数据 就释放
		__sync_lock_test_and_set(&p_vpp_codec_ctx->encode_thread_step, GiveBackFrameToFlow);
		ret = vp_vse_release_frame(&p_vpp_camera->vp_vflow_contex, vflow_chn, &vse_frame);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed.");
			break;
		}else{
			int used_count_tmp = __sync_sub_and_fetch(&p_vpp_codec_ctx->vflow_buffer_used_count, 1);
			if(used_count_tmp < 0 ){
				SC_LOGE("pipline %d stream %s vse used count %d < 0, should not run here.",
					p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, p_vpp_codec_ctx->vflow_buffer_used_count);
			}
		}
		// 编码器用完VnodeBuffer,就归还给 VSE
		__sync_lock_test_and_set(&p_vpp_codec_ctx->encode_thread_step, GiveBackFrameToFlowQueue);
		while(privThread->eState == E_THREAD_RUNNING){
			status = mQueueEnqueueEx(&p_vpp_codec_ctx->m_enc_to_vflow_queue, hbn_vnode_image);
			if (status != E_QUEUE_OK){
				SC_LOGE("channel %d stream %s enqueue from enc_to_vse_queue failed:%d\n",
					p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, status);
				sleep(1);
				continue;
			}
			hbn_vnode_image = NULL;
			enqueue_vse_count++;
			break;
		}
		if(is_geted_codec_stream){
			vpp_camera_push_stream(p_vpp_camera->pipline_id, p_vpp_camera->vpp_impl_index, p_vpp_codec_ctx, &encode_stream);
		}

		__sync_lock_test_and_set(&p_vpp_codec_ctx->encode_thread_step, GiveBackDataToCodec);
		if(is_geted_codec_stream){
			ret = vp_codec_release_output(&p_vpp_codec_ctx->m_encode_context, &encode_stream);
			if (ret != 0) {
				SC_LOGE("vp_codec_release_output failed.");
				break;
			}
		}
		__sync_fetch_and_add(&p_vpp_codec_ctx->codec_thread_run_counter, 1);
		__sync_sub_and_fetch(&p_vpp_codec_ctx->vflow_frame_counter, 1);
	}

	int free_dissociate_count = 0;
	if(hbn_vnode_image != NULL){
		free(hbn_vnode_image);
		free_dissociate_count = 1;
	}
	SC_LOGI("channel %d stream %s dequeue enc %d = enqueue vse %d + free_dissociate_count %d.\n",
		p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, dequeue_enc_count, enqueue_vse_count, free_dissociate_count);

	vp_free_image_frame(&encode_stream);
	mThreadFinish(privThread);
	return NULL;
}

/******************************************************************************
 * funciton : get stream from each channels
 ******************************************************************************/
static void* vlfow_get_stream_proc(void *ptr)
{
	int32_t ret = 0;

	tsThread *privThread = (tsThread*)ptr;
	vpp_codec_ctx_t *p_vpp_codec_ctx = (vpp_codec_ctx_t *)privThread->pvThreadData;
	vpp_camera_t *p_vpp_camera = (vpp_camera_t *)p_vpp_codec_ctx->p_vpp_camera;

	mThreadSetNameWidthIndex(privThread, __func__, p_vpp_camera->pipline_id);

	teQueueStatus status = E_QUEUE_OK;
	ImageFrame vse_frame = {0};
	hbn_vnode_image_t *hbn_vnode_image = NULL;

	uint64_t next_update_time_ms = ((get_timestamp_ms() + 999) / 1000) * 1000;
	struct TimeStatistics time_statistics;

	int dequeue_vse_count = 0;
	int enqueue_enc_count = 0;
	uint8_t is_got_vlflow_frame = 0;
	uint8_t is_saved_vlflow_frame = 0;
	int vflow_chn = p_vpp_codec_ctx->vflow_chn;
	while (privThread->eState == E_THREAD_RUNNING){
		time_statistics_at_beginning_of_loop(&time_statistics);
		status = mQueueDequeueTimed(&p_vpp_codec_ctx->m_enc_to_vflow_queue, 2000, (void **)&hbn_vnode_image);
		if (status != E_QUEUE_OK){
			SC_LOGI("channel %d stream %s dequeue from enc_to_vse_queue failed:%d\n",
				p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, status);
			continue;
		}
		dequeue_vse_count++;


		int wait_count = 0;
		vse_frame.hbn_vnode_image = hbn_vnode_image;
		is_got_vlflow_frame = 0;
		while(privThread->eState == E_THREAD_RUNNING){
			ret = vp_vse_get_frame(&p_vpp_camera->vp_vflow_contex, vflow_chn, &vse_frame);
			if (ret != 0) {
				wait_count++;
				// 当线程接收到退出信号时，getframe 接口会立即报超时退出
				// 所以只有当线程是正常运行状态下的异常才属于真异常
				if (privThread->eState == E_THREAD_RUNNING) {
					int used_count_tmp = __sync_fetch_and_add(&p_vpp_codec_ctx->vflow_buffer_used_count, 0);
					if(used_count_tmp > VPP_VSE_OUTBUFFER_COUNT - VPP_VSE_OUTBUFFER_RELEASE_COUNT){
						SC_LOGI("[%d] [%s] vp_vse_get_frame chn not geted data(%d), because buffer is not enough, vse used count %d, vse all count %d, wait %d",
							p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, ret, used_count_tmp, VPP_VSE_OUTBUFFER_COUNT, wait_count);
					}else{
						SC_LOGE("[%d] [%s] vp_vse_get_frame chn %d failed(%d), vse used count %d, vse all count %d.",
							p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, vflow_chn, ret, used_count_tmp, VPP_VSE_OUTBUFFER_COUNT);
					}
					vp_print_debug_infos_for_multithread(p_vpp_codec_ctx);
					vp_print_debug_infos_when_error();
					continue;
				}
			}else{
				__sync_fetch_and_add(&p_vpp_codec_ctx->vflow_frame_counter, 1);
				int used_count_tmp = __sync_fetch_and_add(&p_vpp_codec_ctx->vflow_buffer_used_count, 1);
				if(used_count_tmp > VPP_VSE_OUTBUFFER_COUNT){
					SC_LOGE("pipline %d stream %s vse used count %d > %d, should not run here.", p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name,
						p_vpp_codec_ctx->vflow_buffer_used_count, VPP_VSE_OUTBUFFER_COUNT);
				}
				is_got_vlflow_frame = 1;
				break;
			}
		}

		is_saved_vlflow_frame = 0;
		while(privThread->eState == E_THREAD_RUNNING){
			status = mQueueEnqueueEx(&p_vpp_codec_ctx->m_vflow_to_enc_queue, hbn_vnode_image);
			if (status != E_QUEUE_OK){
				printf("channel %d stream %s enqueue from enc_to_vse_queue failed:%d\n", p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, status);
				sleep(1);
				continue;
			}
			/**
			 * 1. 入队成功：编码线程释放VSE
			 * 2. 入队失败：当前线程释放VSE
			 */
			is_saved_vlflow_frame = 1;
			/**
			 * 1. 入队成功：编码线程释放 hbn_vnode_image
			 * 2. 入队失败：当前线程释放 hbn_vnode_image 变量（单纯结构体，不好含视频帧）
			 */
			hbn_vnode_image = NULL;
			enqueue_enc_count++;
			break;
		}

		update_osd_info(&p_vpp_camera->vp_vflow_contex, &next_update_time_ms, p_vpp_codec_ctx->osd_chn);
		if((p_vpp_camera->drm_context != NULL) && (p_vpp_camera->drm_init_succesed != 0)
			&& (is_got_vlflow_frame != 0) && (p_vpp_codec_ctx->stream_index == 0)){
			ret = vp_display_set_frame(p_vpp_camera->drm_context, vse_frame.hbn_vnode_image);
			if(ret != 0){
				SC_LOGW("vp_display_set_frame chn failed(%d).", ret);
			}
		}
		__sync_fetch_and_add(&p_vpp_codec_ctx->vflow_thread_run_counter, 1);
		time_statistics_at_ending_of_loop(&time_statistics);
		time_statistics_info_show(&time_statistics, "read_camera", false);
	}

	//当前线程获取到VSE，但是入队失败
	if(is_got_vlflow_frame && (is_saved_vlflow_frame == 0)){
		ret = vp_vse_release_frame(&p_vpp_camera->vp_vflow_contex, vflow_chn, &vse_frame);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed");
		}
	}

	int free_dissociate_count = 0;
	if(hbn_vnode_image != NULL){
		free(hbn_vnode_image);
		free_dissociate_count = 1;
	}

	SC_LOGI("channel %d stream %s dequeue vse %d = enqueue enc %d + free_dissociate_count %d.\n",
		p_vpp_camera->pipline_id, p_vpp_codec_ctx->stream_name, dequeue_vse_count, enqueue_enc_count, free_dissociate_count);
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

	uint8_t is_got_frame_from_vflow = 0;
	int32_t vse_channel = vpp_camera->m_vse_for_bpu_channel;
	while(privThread->eState == E_THREAD_RUNNING) {

		__sync_lock_test_and_set(&vpp_camera->bpu_thread_step, BPUWaitFrameFromFlowQueue);
		is_got_frame_from_vflow = 0;
		while(privThread->eState == E_THREAD_RUNNING) {
			ret = vp_vse_get_frame(&vpp_camera->vp_vflow_contex, vse_channel, &vse_frame);
			if (ret != 0) {
				// 当线程接收到退出信号时，getframe 接口会立即报超时退出
				// 所以只有当线程是正常运行状态下的异常才属于真异常
				if (privThread->eState == E_THREAD_RUNNING) {
					SC_LOGE("channel %d vp_vse_get_frame chn %d failed(%d).", vpp_camera->pipline_id, vse_channel, ret);
					vp_print_debug_infos_when_error();
				}
			}else{
				is_got_frame_from_vflow = 1;
				break;
			}
		}
		if((privThread->eState != E_THREAD_RUNNING) && (is_got_frame_from_vflow == 0)){
			break;
		}
		hbn_vnode_image = (hbn_vnode_image_t *)vse_frame.hbn_vnode_image;
		// vp_vin_print_hbn_vnode_image_t(hbn_vnode_image);

		__sync_lock_test_and_set(&vpp_camera->bpu_thread_step, SendFrameToBPUQueue);
		// 把yuv数据送进bpu进行算法运算
		memset(&bpu_input_buffer, 0, sizeof(bpu_buffer_info_t));
		vpp_graphic_buf_to_bpu_buffer_info(hbn_vnode_image, &bpu_input_buffer);
		// print_bpu_buffer_info(&bpu_input_buffer);

		bpu_wrap_send_frame(&vpp_camera->m_bpu_handle, &bpu_input_buffer);
		__sync_lock_test_and_set(&vpp_camera->bpu_thread_step, BPUGiveBackFrameToFlow);
		ret = vp_vse_release_frame(&vpp_camera->vp_vflow_contex, vse_channel, &vse_frame);
		if (ret != 0) {
			SC_LOGE("vp_vse_release_frame failed");
			break;
		}
		__sync_fetch_and_add(&vpp_camera->bpu_thread_run_counter, 1);
	}

	vp_free_image_frame(&vse_frame);

	mThreadFinish(privThread);
	return NULL;
}

int32_t vpp_camera_init_param_full(solution_cfg_t* solution_cfg){
	int32_t i = 0;
	int32_t ret = 0;

	isp_ichn_attr_t *isp_ichn_attr = NULL;
	vse_config_t *vse_config = NULL;
	int32_t input_width = 0, input_height = 0;

	memset(&g_vpp_camera, 0, sizeof(g_vpp_camera));

	int vpp_camera_index = 0;
	int hdmi_display_channel = -1;
	int pipeline_count =solution_cfg->cam_solution.pipeline_count;
	int hdmi_is_connected = vp_display_check_hdmi_is_connected();
	if(hdmi_is_connected){
		SC_LOGI("hdmi is connected");
	}else{
		SC_LOGI("hdmi is not connected");
	}

	// 根据camera solution的配置设置vin、vse、venc、bpu模块的使能和参数
	for (i = 0; i <solution_cfg->cam_solution.max_pipeline_count; i++) {
		vpp_camera_t *p_vpp_camera = &g_vpp_camera[i];
		p_vpp_camera->pipline_id = -1;
		p_vpp_camera->vpp_impl_index = -1;

		// 1. 配置 vin
		if(solution_cfg->cam_solution.cam_vpp[i].is_valid == 0){
			continue;
		}
		char* sensor_name =solution_cfg->cam_solution.cam_vpp[i].sensor;
		if(solution_cfg->cam_solution.cam_vpp[i].is_enable == 0){
			SC_LOGI("Ignore camera sensor [%s] [%d/%d].", sensor_name, i, pipeline_count);
			continue;
		}

		p_vpp_camera->pipline_id = i;
		p_vpp_camera->vpp_impl_index = vpp_camera_index;

		//for vflow
		p_vpp_camera->vp_vflow_contex.mipi_csi_rx_index =solution_cfg->cam_solution.cam_vpp[i].csi_index;
		p_vpp_camera->vp_vflow_contex.sensor_config = vp_get_sensor_config_by_name(sensor_name);
		p_vpp_camera->vp_vflow_contex.mclk_is_not_configed =solution_cfg->cam_solution.cam_vpp[i].mclk_is_not_configed;

		SC_LOGI("Enable camera sensor [%s] [%d/%d] mclk_is_not_configed:[%d]", sensor_name, i, pipeline_count,
			p_vpp_camera->vp_vflow_contex.mclk_is_not_configed);
		if (p_vpp_camera->vp_vflow_contex.sensor_config == NULL) {
			SC_LOGE("sensor name not found(%s)", sensor_name);
			return -1;
		}
		p_vpp_camera->vp_vflow_contex.vin_info.ochn_buffer_count = 3;
		p_vpp_camera->vp_vflow_contex.isp_info.ochn_buffer_count = 3;
		p_vpp_camera->vp_vflow_contex.gdc_info.output_buffer_count = 3;

		//isp
		p_vpp_camera->vp_vflow_contex.sensor_config->isp_attr->input_mode = 2; // offline

		// 2. 配置算法模型
		if (strlen(solution_cfg->cam_solution.cam_vpp[i].model) > 1
			&& strcmp(solution_cfg->cam_solution.cam_vpp[i].model, "null") != 0) {
			//g_vpp_camera 与插入的摄像头的顺序一一对应
			//m_vpp_id 与使能的摄像头一一对应
			//比如插入了两个摄像头,只使能第二个: g_vpp_camera[0] 是空 g_vpp_camera[1]是有效的
			// 					  			  m_vpp_id 为0 (决定了算法上报结果的通道号)
			p_vpp_camera->m_bpu_handle.m_vpp_id = vpp_camera_index;
			strncpy(p_vpp_camera->m_bpu_handle.m_model_name,
				solution_cfg->cam_solution.cam_vpp[i].model,
				sizeof(p_vpp_camera->m_bpu_handle.m_model_name) - 1);
			p_vpp_camera->m_bpu_handle.m_model_name[sizeof(p_vpp_camera->m_bpu_handle.m_model_name) - 1] = '\0';

			//for debug: bpu
			p_vpp_camera->bpu_thread_step = BPUThreadStepSentry;
			p_vpp_camera->bpu_thread_run_counter = 0;
		}

		// 3. 配置 vse
		vse_config = &p_vpp_camera->vp_vflow_contex.vse_config;
		isp_ichn_attr = p_vpp_camera->vp_vflow_contex.sensor_config->isp_ichn_attr;

		input_width = isp_ichn_attr->width;
		input_height = isp_ichn_attr->height;
		SC_LOGD("pipeline %d: input_width: %d input_height: %d", i, input_width, input_height);

		vse_config->vse_ichn_attr.width = input_width;
		vse_config->vse_ichn_attr.height = input_height;
		vse_config->vse_ichn_attr.fmt = FRM_FMT_NV12;
		vse_config->vse_ichn_attr.bit_width = 8;

		vse_config->vse_ochn_buffer_count = VPP_VSE_OUTBUFFER_COUNT;

		for (int j = 0; j < VPP_STEAM_COUNT; j++){
			int target_w = input_width;
			int target_h = input_height;
			if(j != 0){
				vpp_get_sub_stream_resolution(input_width, input_height, &target_w, &target_h);
			}
			vpp_codec_ctx_t *p_vpp_codec_ctx = &p_vpp_camera->vpp_codec_ctxs[j];
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

		// 第二个通道的数据给BPU使用
		bpu_model_user_info_t *bpu_model_info = &p_vpp_camera->bpu_model_user_info;
		bpu_model_info->is_enable = 0;
		int32_t vse_chn_tmp = 0;
		if (strlen(p_vpp_camera->m_bpu_handle.m_model_name) > 1
			&& strcmp(p_vpp_camera->m_bpu_handle.m_model_name, "null") != 0) {
			bpu_model_info->is_enable = 1;
			ret = bpu_wrap_get_model_user_info(p_vpp_camera->m_bpu_handle.m_model_name, bpu_model_info);
			if (bpu_model_info->input_width > input_width || bpu_model_info->input_height > input_height)
				vse_chn_tmp = 5;// 放大通道：4K
			else
				vse_chn_tmp = 4; //缩小通道: 720P

			SC_LOGI("[%d] VSE channel %d: input_width: %d input_height: %d model_width: %d model_height: %d ", i,
				vse_chn_tmp, input_width, input_height, bpu_model_info->input_width, bpu_model_info->input_height);
			// ret = bpu_wrap_get_model_hw(p_vpp_camera->m_bpu_handle.m_model_name, &model_width, &model_height);
			vse_config->vse_ochn_attr[vse_chn_tmp].chn_en = CAM_TRUE;
			vse_config->vse_ochn_attr[vse_chn_tmp].roi.x = 0;
			vse_config->vse_ochn_attr[vse_chn_tmp].roi.y = 0;
			vse_config->vse_ochn_attr[vse_chn_tmp].roi.w = input_width;
			vse_config->vse_ochn_attr[vse_chn_tmp].roi.h = input_height;
			vse_config->vse_ochn_attr[vse_chn_tmp].target_w = bpu_model_info->input_width;
			vse_config->vse_ochn_attr[vse_chn_tmp].target_h = bpu_model_info->input_height;
			vse_config->vse_ochn_attr[vse_chn_tmp].fmt = FRM_FMT_NV12;
			vse_config->vse_ochn_attr[vse_chn_tmp].bit_width = 8;

			p_vpp_camera->m_vse_for_bpu_channel = vse_chn_tmp;
		}

		//配置OSD
		osd_user_info_t *osd_info = &p_vpp_camera->vp_vflow_contex.osd_info;
		int valid_osd_region_count = 0;
		for (int j = 0; j < VPP_STEAM_COUNT; j++){
			int target_w = input_width;
			int target_h = input_height;
			if(j != 0){
				vpp_get_sub_stream_resolution(input_width, input_height, &target_w, &target_h);
			}
			vpp_codec_ctx_t *p_vpp_codec_ctx = &p_vpp_camera->vpp_codec_ctxs[j];
			int osd_boundary_w = 320 + 50;
			int osd_boundary_h = 200 + 50;
			p_vpp_codec_ctx->osd_chn = -1;
			if(( osd_boundary_w >= target_w) || (osd_boundary_h >= target_h)){
				SC_LOGW("pipeline %d stream_index %d resolution is too small ( %d =< %d or %d =< %d), so disable osd.",
					p_vpp_camera->pipline_id, j, target_w, osd_boundary_w, target_h, osd_boundary_h);
				continue;
			}
			osd_info->handle[valid_osd_region_count] = i * VP_MAX_OSD_REGION + j;
			osd_info->vse_chn[valid_osd_region_count] = p_vpp_codec_ctx->vflow_chn;
			osd_info->position[valid_osd_region_count].x = 50;
			osd_info->position[valid_osd_region_count].y = 50;
			osd_info->position[valid_osd_region_count].width = 320;
			osd_info->position[valid_osd_region_count].height = 200;
			
			p_vpp_codec_ctx->osd_chn = valid_osd_region_count;
			valid_osd_region_count++;
		}
		osd_info->valid_osd_region_count = valid_osd_region_count;
		
		//gdc
		p_vpp_camera->vp_vflow_contex.gdc_info.input_width = input_width;
		p_vpp_camera->vp_vflow_contex.gdc_info.input_height = input_height;
		strcpy(p_vpp_camera->vp_vflow_contex.gdc_info.sensor_name, sensor_name);
		p_vpp_camera->vp_vflow_contex.gdc_info.status = solution_cfg->cam_solution.cam_vpp[i].gdc_status;

		if((hdmi_is_connected) && (hdmi_display_channel == -1)){

			p_vpp_camera->drm_context = &g_drm_context;
			hdmi_display_channel = p_vpp_camera->pipline_id;
			SC_LOGI("channel %d enable hdmi display", hdmi_display_channel);
		}

		//for codec
		for (int j = 0; j < VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &p_vpp_camera->vpp_codec_ctxs[j];
			camera_config_t *camera_config = p_vpp_camera->vp_vflow_contex.sensor_config->camera_config;
			int target_w = input_width;
			int target_h = input_height;
			if(j != 0){
				vpp_get_sub_stream_resolution(input_width, input_height, &target_w, &target_h);
			}
			p_vpp_codec_ctx->stream_index = j;

			media_codec_user_config_t *codec_user_config = &p_vpp_codec_ctx->m_encode_user_config;
			codec_user_config->bit_rate = solution_cfg->cam_solution.cam_vpp[i].encode_bitrate;
			codec_user_config->codec_type = VP_GET_MD_CODEC_TYPE(solution_cfg->cam_solution.cam_vpp[i].encode_type);
			codec_user_config->frame_rate = camera_config->fps;
			codec_user_config->width = target_w;
			codec_user_config->height = target_h;

			codec_user_config->input_buffer_is_extrenal = true;
			codec_user_config->input_buffer_count = 0;
			codec_user_config->output_buffer_count = 5;

			ret = vp_encode_config_param(&p_vpp_codec_ctx->m_encode_context, codec_user_config);
			if (ret != 0){
				SC_LOGE("Encode config param error");
				exit(-1);
			}
			//for debug: codec
			p_vpp_codec_ctx->vflow_thread_run_counter = 0;
			p_vpp_codec_ctx->codec_thread_run_counter = 0;
			p_vpp_codec_ctx->vflow_frame_counter = 0;
			p_vpp_codec_ctx->vflow_buffer_used_count = 0;

			p_vpp_codec_ctx->encode_thread_step = EncodeThreadStepSentry;
			p_vpp_codec_ctx->first_frame_timestamp = 0;

			//for thread param
			p_vpp_codec_ctx->p_vpp_camera = p_vpp_camera;
			
		}
		vpp_camera_index++;
	}

	return ret;
}
int32_t vpp_camera_init_param(void){
	return vpp_camera_init_param_full(&g_solution_config);
}

int32_t vpp_init_ion_pipeline_param_from_vflow_contex(vp_vflow_contex_t *vp_vflow_contex,
	media_codec_user_config_t *codec_user_config, bpu_model_user_info_t *bpu_config,
	vp_ion_pipeline_param_t *ion_param){

	memset(ion_param, 0, sizeof(vp_ion_pipeline_param_t));
	//vin
	vp_ion_buffer_param_t *vin = &ion_param->vin;
	vp_sensor_config_t *vp_sensor_config = vp_vflow_contex->sensor_config;
	if(vp_sensor_config == NULL){
		SC_LOGE("vp_sensor_config is null.");
		return -1;
	}
	vin->format = ION_BUFFER_RAW10;

	vin->width = vp_sensor_config->vin_ichn_attr->width;
	vin->height = vp_sensor_config->vin_ichn_attr->height;

	vin->count = vp_vflow_contex->vin_info.ochn_buffer_count;

	//isp
	vp_ion_buffer_param_t *isp = &ion_param->isp;
	isp->width = vp_sensor_config->isp_ichn_attr->width;
	isp->height = vp_sensor_config->isp_ichn_attr->height;
	isp->format = ION_BUFFER_NV12; //vp_sensor_config->isp_ochn_attr.fmt
	isp->count = vp_vflow_contex->isp_info.ochn_buffer_count;

	//vse
	ion_param->vse_valid_count = 0;
	vse_config_t *vse_config = &vp_vflow_contex->vse_config;
	for(int i = 0; i< VSE_MAX_CHANNLE; i++){
		if(vse_config->vse_ochn_attr[i].chn_en == CAM_TRUE){
			vp_ion_buffer_param_t *vse = &ion_param->vse[ion_param->vse_valid_count];
			vse->width = vse_config->vse_ochn_attr[i].target_h;
			vse->height = vse_config->vse_ochn_attr[i].target_w;
			vse->format = ION_BUFFER_NV12;
			vse->count = vse_config->vse_ochn_buffer_count;
			ion_param->vse_valid_count++;
		}
	}

	//gdc
	gdc_user_info_t *gdc_info = &vp_vflow_contex->gdc_info;
	if(gdc_info->status == GDC_STATUS_OPEN){
		vp_ion_buffer_param_t *gdc = &ion_param->gdc;
		gdc->width = gdc_info->input_width;
		gdc->height = gdc_info->input_height;
		gdc->format = ION_BUFFER_NV12;
		gdc->count = gdc_info->output_buffer_count;

		int gdc_file_size = get_gdc_config_file_size(vp_vflow_contex->gdc_info.sensor_name);
		if(gdc_file_size != -1){
			ion_param->gdc_bin_file_size = gdc_file_size;
		}
		ion_param->is_enable_gdc = 1;
	}else{
		ion_param->is_enable_gdc = 0;
	}

	//osd
	osd_user_info_t *osd_info = &vp_vflow_contex->osd_info;
	ion_param->osd_valid_count = osd_info->valid_osd_region_count;
	for(int i = 0; i< ion_param->osd_valid_count; i++){
		vp_ion_buffer_param_t *osd = &ion_param->osd[i];
		osd->width = osd_info->position[i].width;
		osd->height =osd_info->position[i].height;
		osd->format = ION_OSD_BUFFER_VGA8;
		osd->count = 1;
	}

	//vpu
	vp_ion_vpu_param_t *vpu = &ion_param->vpu;
	vpu->width = codec_user_config->width;
	vpu->height = codec_user_config->height;
	vpu->output_buffer_count = codec_user_config->output_buffer_count;
	if(codec_user_config->input_buffer_is_extrenal){
		vpu->input_buffer_count = 0;
	}else{
		vpu->input_buffer_count = codec_user_config->input_buffer_count;
	}

	if(codec_user_config->codec_type == MEDIA_CODEC_ID_H264){
		vpu->type = ION_H264_ENCODEC;
	}else if(codec_user_config->codec_type == MEDIA_CODEC_ID_H265){
		vpu->type = ION_H265_ENCODEC;
	}else{
		SC_LOGE("not support codec type :%d", codec_user_config->codec_type);
	}

	//bpu
	if(bpu_config->is_enable){
		vp_ion_bpu_param_t *bpu = &ion_param->bpu;
		bpu->input_height = bpu_config->input_height;
		bpu->input_width = bpu_config->input_width;
		bpu->input_queue_count = BPU_INPUT_BUFFER_NUM;

		if(strcmp(bpu_config->model_name, "mobilenetv2") == 0){
			bpu->output_queue_count = 1;
		}else{
			bpu->output_queue_count = BPU_OUTPUT_BUFFER_NUM;
		}
		bpu->output_dimensions = bpu_config->output_dimension;
		for(int i = 0; i< bpu->output_dimensions; i++){
			bpu->output_size[i] = bpu_config->output_size[i];
		}

		const bpu_model_info_t* bpu_model_info = bpu_wrap_model_info(bpu_config->model_name);
		if(bpu_model_info == NULL){
			SC_LOGE("model %s get info failed", bpu_config->model_name);
			// return -1;
		}
		if(bpu_model_info->ouput_calculator_size_is_fixed){
			bpu->ouput_calculator_size_dynamic = 0;
		}else{
			bpu->ouput_calculator_size_dynamic = bpu_model_info->ouput_calculator_size;
		}
	}

	//for camera service
	vp_ion_camera_service_param_t *camera_service = &ion_param->camera_service;
	camera_service->width = vp_sensor_config->isp_ichn_attr->width;
	camera_service->height = vp_sensor_config->isp_ichn_attr->height;

	camera_service->format = ION_BUFFER_NV12;

	if(vp_sensor_config->isp_attr->input_mode == 1){
		camera_service->is_mcm_mode = 1; //vin->isp
	}else{
		camera_service->is_mcm_mode = 0; //vin->isp
	}
	camera_service->is_enable_3dnr = 1;
	camera_service->is_enable_isp = 1;
	camera_service->is_enable_vse = 1;
	return 0;
}

int32_t vpp_init_ion_pipeline_fixed_param_from_vflow_contex(
		vpp_camera_t *vpp, solution_cfg_cam_t* cam_cfg, vp_ion_pipeline_fixed_param_t *fixed_param){

	memset(fixed_param, 0, sizeof(vp_ion_pipeline_fixed_param_t));
	//for bpu
	vp_ion_bpu_extern_param_t *bpu = &fixed_param->bpu;
	bpu->is_used_bpu = false;

	for(int i = 0; i< cam_cfg->max_pipeline_count; i++){
		if(cam_cfg->cam_vpp[i].is_valid == 0){
			continue;
		}
		if(cam_cfg->cam_vpp[i].is_enable == 0){
			continue;
		}
		bpu_model_user_info_t *bpu_model_user_info = &vpp[i].bpu_model_user_info;
		if(!bpu_model_user_info->is_enable){
			continue;
		}
		if(!bpu->is_used_bpu){
			bpu->is_used_bpu = true;
		}
		int is_already_calculated = 0;
		//查看当前通道的模型，是否已经计算
		for(int j = 0; j < bpu->item_count; j++){
			int cmp_ret = strcmp(bpu->item_param[j].model_name, bpu_model_user_info->model_name);
			if(cmp_ret == 0){
				SC_LOGI("pipeline channel[%d] calculate bpu fixed size, found is alread calculated, so ignore it.\n");
				is_already_calculated = 1;
				break;
			}
		}
		if(is_already_calculated){
			continue;
		}

		const bpu_model_info_t *model_info = bpu_wrap_model_info(vpp[i].bpu_model_user_info.model_name);
		if(model_info == NULL){
			SC_LOGE("not found model info for: %s", vpp[i].bpu_model_user_info.model_name);
			continue;
		}
		vp_ion_bpu_extern_single_param_t *item_param = &bpu->item_param[bpu->item_count];
		strcpy(item_param->model_name, bpu_model_user_info->model_name);

		item_param->heap_region_size = model_info->heap_region_size;
		item_param->model_file_sizes = model_info->model_file_size;
		if(model_info->ouput_calculator_size_is_fixed){
			item_param->ouput_calculator_size_static = model_info->ouput_calculator_size;
		}else{
			item_param->ouput_calculator_size_static = 0;
		}
		// SC_LOGI("[%s] heap_region_size:%d model_file_sizes:%d ouput_calculator_size_static:%d", item_param->model_name,
		// 	item_param->heap_region_size, item_param->model_file_sizes, item_param->ouput_calculator_size_static);
		bpu->item_count++;
	}

	//for camera service
	vp_ion_camera_service_extern_param_t *camera_service = &fixed_param->camera_service;
	for(int i = 0; i< cam_cfg->max_pipeline_count; i++){
		if(cam_cfg->cam_vpp[i].is_valid == 0){
			continue;
		}
		if(cam_cfg->cam_vpp[i].is_enable == 0){
			continue;
		}

		camera_service->is_used_isp = 1;
		camera_service->is_used_vse = 1;
	}
	return 0;
}

int32_t vpp_camera_ion_param_get(solution_cfg_t* solution_cfg, solution_ion_param_info_t *solution_param_info){
	int ret = 0;

	//1. 预初始化，根据web参数，得到运行参数
	vpp_camera_init_param_full(solution_cfg);

	//2. 程序运行参数 ==> vp_ion 动态参数
	solution_cfg_cam_t* cam_cfg = &solution_cfg->cam_solution;
	vp_ion_pipeline_param_t *vp_ion_param = solution_param_info->pipeline_params;
	solution_param_info->pipeline_param_vaild_count = 0;
	for(int i = 0; i< cam_cfg->max_pipeline_count; i++){
		if(cam_cfg->cam_vpp[i].is_valid == 0){
			continue;
		}
		if(cam_cfg->cam_vpp[i].is_enable == 0){
			continue;
		}
		ret = vpp_init_ion_pipeline_param_from_vflow_contex(
				&g_vpp_camera[i].vp_vflow_contex,
				&g_vpp_camera[i].vpp_codec_ctxs[0].m_encode_user_config,
				&g_vpp_camera[i].bpu_model_user_info,
				&vp_ion_param[solution_param_info->pipeline_param_vaild_count]);
		if(ret != 0){
			SC_LOGE("vpp_init_ion_pipeline_param_from_vflow_contex failed for channel %d.", i);
			continue;
		}
		solution_param_info->pipeline_param_vaild_count++;
	}

	//3. 程序运行参数 ==> vp_ion 静态参数
	vpp_init_ion_pipeline_fixed_param_from_vflow_contex(g_vpp_camera, cam_cfg, &solution_param_info->extern_param);
	return 0;
}

int32_t vpp_camera_init(void)
{
	int32_t ret = 0;
	int32_t i = 0;
	vp_vflow_contex_t *vp_vflow_contex = NULL;


	hb_mem_module_open();

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
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

		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[j];
			ret = vp_codec_init(&p_vpp_codec_ctx->m_encode_context);
			if (ret != 0){
				SC_LOGE("Encode vp_codec_init error for channel %d stream %d", i, j);
				continue;
			}
			SC_LOGI("Init video encode instance %d successful",p_vpp_codec_ctx->m_encode_context.instance_index);
		}

		// 初始化算法模块， 从vse的chn1通道get yuv数据
		// 初始化bpu
		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		ret = bpu_wrap_model_init(&g_vpp_camera[i].m_bpu_handle, g_vpp_camera[i].m_bpu_handle.m_model_name);
		if (ret != 0) {
			SC_LOGE("bpu_wrap_model_init failed");
			continue;
		}

		/*
			1. 数值下标的含义：
				主码流：下标是0
				子码流：下标是1、2、3 ..., 由于目前只支持1个子码流，所以这里固定用1
			2. 编码类型区分的原因：JS实现的播放器中根据编码器类型做了不同逻辑
				H264：固定使用主码流
				H265：固定使用子码流

		*/
		int image_width = g_vpp_camera[i].vpp_codec_ctxs[0].m_encode_context.video_enc_params.width;
		int image_height = g_vpp_camera[i].vpp_codec_ctxs[0].m_encode_context.video_enc_params.height;

		if((VPP_STEAM_COUNT >= 2) &&
			(g_vpp_camera[i].vpp_codec_ctxs[0].m_encode_user_config.codec_type == MEDIA_CODEC_ID_H265)){
			image_width = g_vpp_camera[i].vpp_codec_ctxs[1].m_encode_context.video_enc_params.width;
			image_height = g_vpp_camera[i].vpp_codec_ctxs[1].m_encode_context.video_enc_params.height;
		}
		// 设置bpu后处理的原始图像大小为推流图像大小
		bpu_wrap_set_ori_hw(&g_vpp_camera[i].m_bpu_handle, image_width, image_height);
		// 注册算法结果回调函数
		bpu_wrap_callback_register(&g_vpp_camera[i].m_bpu_handle,
			bpu_wrap_general_result_handle, &g_vpp_camera[i].m_bpu_handle.m_vpp_id);
	}

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

		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[j];
			ret = vp_codec_deinit(&p_vpp_codec_ctx->m_encode_context);
		}
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

		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;

		char meida_name[64];
		sprintf(meida_name, "ch%d", g_vpp_camera[i].vpp_impl_index);
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[j];
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

			//队列的个数根据 vse 输出buffer的个数设置
			teQueueStatus status = mQueueCreate(&p_vpp_codec_ctx->m_vflow_to_enc_queue, VPP_VSE_OUTBUFFER_COUNT + 1); //必须是加1：mqueue 为了判断空和满的区别，保留了一个item
			if(status != E_QUEUE_OK){
				SC_LOGE("mqueue create failed %d, for channle:%d stream:%d.", status, i, j);
				continue;
			}

			status = mQueueCreate(&p_vpp_codec_ctx->m_enc_to_vflow_queue, VPP_VSE_OUTBUFFER_COUNT + 1); //必须是加1：mqueue 为了判断空和满的区别，保留了一个item
			if(status != E_QUEUE_OK){
				SC_LOGE("mqueue create failed %d, for channle:%d stream:%d.", status, i, j);
				continue;
			}
			for (size_t k = 0; k < VPP_VSE_OUTBUFFER_COUNT; k++){
				hbn_vnode_image_t *hbn_vnode_image = (hbn_vnode_image_t *)malloc(sizeof(hbn_vnode_image_t));
				if (hbn_vnode_image == NULL){
					SC_LOGE("malloc failed\n");
					exit(-1);
				}
				memset(hbn_vnode_image, 0, sizeof(hbn_vnode_image_t));

				teQueueStatus status = mQueueEnqueue(&p_vpp_codec_ctx->m_enc_to_vflow_queue, (void *)hbn_vnode_image);
				if (status != E_QUEUE_OK){
					printf("mqueue enqueue failed:%d\n", status);
					return -1;
				}
			}
			ret = vp_codec_start(&p_vpp_codec_ctx->m_encode_context);
			if (ret != 0){
				SC_LOGE("Encode vp_codec_start error");
				return -1;
			}
			SC_LOGI("Start video encode instance %d successful", p_vpp_codec_ctx->m_encode_context.instance_index);
		}
		ret = vp_vin_start(vp_vflow_contex);
		ret |= vp_isp_start(vp_vflow_contex);
		ret |= vp_vse_start(vp_vflow_contex);
		ret |= vp_osd_start(vp_vflow_contex);
		ret |= vp_gdc_start(vp_vflow_contex);
		ret |= vp_vflow_start(vp_vflow_contex);
		SC_ERR_CON_EQ(ret, 0, "vpp_camera_start");

		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[j];
			p_vpp_codec_ctx->m_vflow_thread.pvThreadData = (void*)p_vpp_codec_ctx;
			mThreadStart(vlfow_get_stream_proc, &p_vpp_codec_ctx->m_vflow_thread, E_THREAD_JOINABLE);

			p_vpp_codec_ctx->m_venc_thread.pvThreadData = (void*)p_vpp_codec_ctx;
			mThreadStart(venc_get_stream_proc, &p_vpp_codec_ctx->m_venc_thread, E_THREAD_JOINABLE);
		}

		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;

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
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[j];
			mThreadStop(&p_vpp_codec_ctx->m_vflow_thread); //必须在m_venc_thread的前面：
			mThreadStop(&p_vpp_codec_ctx->m_venc_thread);
			int enc_remain_count = 0;
			int vse_remain_count = 0;
			teQueueStatus status = E_QUEUE_OK;
			while(!mQueueIsEmpty(&p_vpp_codec_ctx->m_vflow_to_enc_queue)){
				hbn_vnode_image_t *hbn_vnode_image = NULL;
				status = mQueueDequeueTimed(&p_vpp_codec_ctx->m_vflow_to_enc_queue, 0, (void **)&hbn_vnode_image);
				if(status != E_QUEUE_OK){
					SC_LOGE("mqueue clear failed %d, for channle:%d stream %d.", status, i, j);
					break;
				}
				free(hbn_vnode_image);
				enc_remain_count++;
			}
			status = mQueueDestroy(&p_vpp_codec_ctx->m_vflow_to_enc_queue);
			if(status != E_QUEUE_OK){
				SC_LOGE("mqueue destroy failed %d, for channle:%d stream %d.", status, i, j);
			}

			while(!mQueueIsEmpty(&p_vpp_codec_ctx->m_enc_to_vflow_queue)){
				hbn_vnode_image_t *hbn_vnode_image = NULL;
				status = mQueueDequeueTimed(&p_vpp_codec_ctx->m_enc_to_vflow_queue, 0, (void **)&hbn_vnode_image);
				if(status != E_QUEUE_OK){
					SC_LOGE("mqueue clear failed %d, for channle:%d stream %d.", status, i, j);
					break;
				}
				free(hbn_vnode_image);
				vse_remain_count++;
			}
			SDK_Cmd_Impl(SDK_CMD_MEDIA_SERVER_DESTROY, p_vpp_codec_ctx->media_handler);
			// media_server_destroy_media(p_vpp_codec_ctx->media);
			p_vpp_codec_ctx->media_handler = NULL;
			p_vpp_codec_ctx->media_type = NULL;
			SC_LOGI("channel %d stream %d enc queue remain %d, vse queue remain %d .\n",
					g_vpp_camera[i].pipline_id, j, enc_remain_count, vse_remain_count);

		}

		if (strlen(g_vpp_camera[i].m_bpu_handle.m_model_name) == 0)
			continue;
		mThreadStop(&g_vpp_camera[i].m_bpu_thread);
	}

	for (i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].vp_vflow_contex.sensor_config == NULL)
			continue;

		vp_vflow_contex = &g_vpp_camera[i].vp_vflow_contex;
		for(int j = 0; j< VPP_STEAM_COUNT; j++){
			vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[j];
			ret = vp_codec_stop(&p_vpp_codec_ctx->m_encode_context);
		}

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
	for (int i = 0; i < VPP_CAM_MAX_CHANNELS; i++) {
		if (g_vpp_camera[i].vpp_impl_index != -1) {
			// video_id 是web端传下来的，从 1开始
			if ((g_vpp_camera[i].vpp_impl_index + 1) == video_id) {
				return g_vpp_camera[i].pipline_id;
			}
		}
	}
	SC_LOGE("get_pipeline_id_by_video_id %d failed.", video_id);
	exit(-1);
	return -1;
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
				vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[0];
				if (p_vpp_codec_ctx->m_encode_context.codec_id == MEDIA_CODEC_ID_NONE) {
					continue;
				}
				if (p_vpp_codec_ctx->m_encode_context.instance_index == param->channel) {
					// 填充对外的信息
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
					}else{
						SC_LOGE("unsupport codec id %d.", p_vpp_codec_ctx->m_encode_context.codec_id);
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
				vpp_codec_ctx_t *p_vpp_codec_ctx = &g_vpp_camera[i].vpp_codec_ctxs[0];
				if (p_vpp_codec_ctx->m_encode_context.codec_id != MEDIA_CODEC_ID_NONE) {
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
