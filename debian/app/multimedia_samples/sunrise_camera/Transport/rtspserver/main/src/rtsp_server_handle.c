#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C"{
#endif
#include "RtspSvrWrap.hh"
#ifdef __cplusplus
}
#endif

#include <utils/stream_manager.h>
#include "rtsp_server_handle.h"
#include "utils/utils_log.h"
#include "rtsp_server_default_param.h"

static rtsp_server_t s_rtsp_server_handle;

int rtsp_server_init()
{
	rtsp_server_t *handle = &s_rtsp_server_handle;
	memset(handle, 0, sizeof(rtsp_server_t));
	handle->instance  = rtspsvr_wrap_instance();
	SC_LOGI("handle->instance:%p", handle->instance);
	/*rtspsvr_wrap_prepare(handle->instance, 554);*/

	handle->state = RTSP_SRV_STATE_PREPARE;
	return 0;
}

int rtsp_server_prepare()
{
	rtsp_server_t *handle = &s_rtsp_server_handle;
	rtspsvr_wrap_prepare(handle->instance, 554);

	handle->state = RTSP_SRV_STATE_PREPARE;
	return 0;
}

int rtsp_server_uninit()
{
	rtsp_server_t *handle = &s_rtsp_server_handle;
	rtspsvr_wrap_reclaim(handle->instance);
	handle->state = RTSP_SRV_STATE_UNINIT;
	return 0;
}

int rtsp_server_start()
{
	rtsp_server_t *handle = &s_rtsp_server_handle;
	if(handle->state == RTSP_SRV_STATE_START)
	{
		SC_LOGW("rtsp server alread start!");
		return 0;
	}

	rtspsvr_wrap_start(handle->instance);
	handle->state = RTSP_SRV_STATE_START;
	return 0;
}

int rtsp_server_stop()
{
	rtsp_server_t *handle = &s_rtsp_server_handle;
	if(handle->state == RTSP_SRV_STATE_STOP)
	{
		SC_LOGW("rtsp server alread stop!");
		return 0;
	}

	rtspsvr_wrap_stop(handle->instance);
	handle->state = RTSP_SRV_STATE_STOP;
	SC_LOGI("rtsp_server_stop");

	return 0;
}

int rtsp_server_param_set(void* param, unsigned int length)
{
	/*rtsp_server_t *handle = &s_rtsp_server_handle;*/
	/*memcpy(&handle->param, param, sizeof(rtspserver_info_t));*/
	/*rtspserver_param_save(&handle->param);*/
	return 0;
}

int rtsp_server_param_get(void* param, unsigned int length)
{
	/*rtsp_server_t *handle = &s_rtsp_server_handle;*/
	/*memcpy(param, &handle->param, sizeof(rtspserver_info_t));*/
	return 0;
}

int rtsp_server_param_state_get()
{
	return s_rtsp_server_handle.state;
}

int rtsp_server_add_sms(void* param, unsigned int length)
{
	int i = 0;
	rtsp_server_t *handle = &s_rtsp_server_handle;
	if (handle->state == RTSP_SRV_STATE_PREPARE)
		rtsp_server_start();

	rtspserver_info_t* sms_param = (rtspserver_info_t*)param;

	int ret = rtspsvr_wrap_add_sms(handle->instance, sms_param->prefix,
		sms_param->audio.enable, sms_param->audio.type, sms_param->audio.samplerate,
		sms_param->audio.bitspersample, sms_param->audio.channels,
		sms_param->video.enable, sms_param->video.type, sms_param->video.framerate,
		sms_param->shm_id, sms_param->shm_name, sms_param->stream_buf_size,
		sms_param->video.framerate, sms_param->suggest_buffer_region_size,
		sms_param->suggest_buffer_item_count);
	if(ret != 0){
		SC_LOGE("rtspsvr_wrap_add_sms failed stream_buf_size: %d, framerate:%d\n",
		sms_param->stream_buf_size, sms_param->video.framerate);
	}
	SC_LOGI("rtsp_server_add_sms stream_buf_size: %d, framerate:%d [%d:%d]\n",
		sms_param->stream_buf_size, sms_param->video.framerate,
		sms_param->suggest_buffer_region_size, sms_param->suggest_buffer_item_count);

	// 从参数数组中找个空位置把配置保存下来，删除sms的时候可以直接用
	for (i = 0; i < 8; i++) {
		if (strlen(handle->params[i].prefix) != 0) continue;
		memcpy(&handle->params[i], param, sizeof(rtspserver_info_t));
		break;
	}
	return 0;
}

int rtsp_server_del_all_sms(void)
{
	int i = 0;
	rtsp_server_t *handle = &s_rtsp_server_handle;

	for (i = 0; i < 8; i++) {
		if (strlen(handle->params[i].prefix) == 0) continue;
		rtspsvr_wrap_del_sms(handle->instance, handle->params[i].prefix);
		printf("%d: delete StreamName:%s\n", i, handle->params[i].prefix);
		memset(&handle->params[i], 0, sizeof(rtspserver_info_t));
	}
	return 0;
}


unsigned long long get_system_time_ms(void) {
	unsigned long long sys_us;
	struct timeval currtime;

	gettimeofday(&currtime, NULL);
	sys_us = currtime.tv_sec * 1000000 + currtime.tv_usec;

	return sys_us / 1000;
}

static int gettickcount(struct timeval* tp, int* tz) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	tp->tv_sec =  (long)(now.tv_sec);
	tp->tv_usec = (long)(now.tv_nsec / 1000);
	return 0;
}

unsigned long long  get_system_time_us(void) {
	unsigned long long sys_us;
	struct timeval currtime;

	//gettimeofday(&currtime, NULL);
	gettickcount(&currtime, NULL);
	sys_us = currtime.tv_sec * 1000000 + currtime.tv_usec;

	return sys_us;
}

unsigned long long video_ts_convert()
{
	struct timeval ts;
	gettickcount(&ts, NULL);
	//gettimeofday(&ts, NULL);
	unsigned long long pts;
	pts = ts.tv_sec * 90000 + ts.tv_usec * 9 / 100;
	return pts;
}

#if 0
unsigned long long audio_ts_convert(rtsp_server_t *handle)
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	unsigned long long pts;
	pts = ts.tv_sec * handle->param.audio.samplerate + ts.tv_usec * 8/1000;
	return pts;
}
#endif

