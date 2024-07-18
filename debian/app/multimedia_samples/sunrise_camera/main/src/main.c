/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2024 D-Robotics, Inc.
* All rights reserved.
***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "communicate/sdk_communicate.h"

#include "utils/exception_handling.h"
#include "utils/utils_log.h"

static int32_t _do_add_sms(int32_t channel)
{
	int32_t ret = 0;
	T_SDK_VENC_INFO venc_chn_info;
	// 从camera模块获取chn0的码流配置
	venc_chn_info.channel = channel;
	ret = SDK_Cmd_Impl(SDK_CMD_VPP_VENC_CHN_PARAM_GET, (void*)&venc_chn_info);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_VENC_CHN_PARAM_GET Error, ERRCODE: %d", ret);
		return -1;
	}

	SC_LOGI("venc chn %d id %s, type: %d, frameRate: %d\n", venc_chn_info.channel,
		venc_chn_info.enable == 1 ? "enable" : "disable", venc_chn_info.type,
		venc_chn_info.framerate);

	T_SDK_RTSP_SRV_PARAM sms_param = { 0 };
	int32_t type = venc_chn_info.type;
	char *codec_type_string = "h264";


	sms_param.audio.enable = 0;

	sms_param.video.enable = 1;
	if (type == 96){
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_H264;
		codec_type_string = "h264";
	}else if(type == 265){
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_H265;
		codec_type_string = "h265";
	}else if(type == 26){
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_MJPEG;
		codec_type_string = "jpeg";
	}else{
		SC_LOGE("not support codec type [%d],so use h264.", type);
		codec_type_string = "h264";
		sms_param.video.type = T_SDK_RTSP_VIDEO_TYPE_H264;
	}

	sprintf(sms_param.prefix, "stream_chn%d.%s", venc_chn_info.channel, codec_type_string);
	sprintf(sms_param.shm_id, "rtsp_id_%s_chn%d", type == 96 ? "h264" :
								(type == 265 ? "h265" :
								(type == 26) ? "jpeg" : "other"), venc_chn_info.channel);
	sprintf(sms_param.shm_name, "name_%s_chn%d", type == 96 ? "h264" :
								(type == 265 ? "h265" :
								(type == 26) ? "jpeg" : "other"), venc_chn_info.channel);

	sms_param.stream_buf_size = venc_chn_info.stream_buf_size;
	sms_param.video.framerate = venc_chn_info.framerate;

	sms_param.suggest_buffer_item_count = venc_chn_info.suggest_buffer_item_count;
	sms_param.suggest_buffer_region_size = venc_chn_info.suggest_buffer_region_size;

	SC_LOGI("prefix: %s, port: %d, video_framerate: %d, shm_id: %s, shm_name: %s, stream_buf_size: %d, region size %d, item count %d.",
		sms_param.prefix, sms_param.port,
		sms_param.video.framerate,
		sms_param.shm_id, sms_param.shm_name, sms_param.stream_buf_size,
		sms_param.suggest_buffer_region_size, sms_param.suggest_buffer_item_count);

	ret = SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_ADD_SMS, (void*)&sms_param);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RTSP_SERVER_START Error, ERRCODE: %d", ret);
		return -1;
	}
	return ret;
}


int32_t module_init()
{
	int32_t ret;
#ifdef MODULE_ALARM
	ret = SDK_Cmd_Impl(SDK_CMD_ALARM_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_ALARM_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_SYSTEM
	ret = SDK_Cmd_Impl(SDK_CMD_SYSTEM_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_SYSTEM_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_NETWORK
	ret = SDK_Cmd_Impl(SDK_CMD_NETWORK_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_NETWORK_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_RTSP
	ret = SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RTSP_SERVER_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
	ret = SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_PREPARE, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RTMP_START Error, ERRCODE: %d", ret);
		return -1;
	}
#endif


#ifdef MODULE_VPP
	// 因为sdk核心和子模块原则上是分离的，sdk核心不引用子模块的头文件
	// 一般核心和子模块需要各自定义结构体来完成交互
	// 所以建议是通信用到的参数尽量用基础类型，如果用到结构体也需要尽量考虑通用和简洁
	ret = SDK_Cmd_Impl(SDK_CMD_VPP_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_RECORD
	ret = SDK_Cmd_Impl(SDK_CMD_RECORD_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RECORD_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

	return ret;

}

int32_t module_start()
{
	int32_t i, ret;
#ifdef MODULE_ALARM
	ret = SDK_Cmd_Impl(SDK_CMD_ALARM_START, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_ALARM_START Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_SYSTEM
	ret = SDK_Cmd_Impl(SDK_CMD_SYSTEM_START, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_SYSTEM_START Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_NETWORK
	ret = SDK_Cmd_Impl(SDK_CMD_NETWORK_START, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_NETWORK_START Error, ERRCODE: %d", ret);
		return -1;
	}
#endif
	usleep(500*1000);

#ifdef MODULE_VPP
	ret = SDK_Cmd_Impl(SDK_CMD_VPP_START, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_START Error, ERRCODE: %d", ret);
		return -1;
	}
	usleep(500*1000);
#endif


#ifdef MODULE_SYSTEM
	while(1)
	{
		ret = SDK_Cmd_Impl(SDK_CMD_SYSTEM_NTP_SYNC_READY, NULL);
		if(ret != 0)
		{
			usleep(100*1000);
//			continue;
		}
		else
		{
			SC_LOGI("ntp sync done");
			break;
		}
	}

#endif

#ifdef MODULE_RECORD
		ret = SDK_Cmd_Impl(SDK_CMD_RECORD_START, NULL);
		if(ret < 0)
		{
			SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RECORD_START Error, ERRCODE: %d", ret);
			return -1;
		}
#endif
#ifdef MODULE_RTSP
		ret = SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_START, NULL);
		if(ret < 0)
		{
			SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RTSP_SERVER_START Error, ERRCODE: %d", ret);
			return -1;
		}
		// 此处的配置首先要有一个主码流的默认配置，需要从camara模块获取
		// 其他的码流配置情况可能会根据web的配置来修改

		// 稍微等待rtsp server 启动完成
		usleep(500);
		// 根据编码通道的配置添加推流
		uint32_t venc_chns_status = 0;
		SDK_Cmd_Impl(SDK_CMD_VPP_GET_VENC_CHN_STATUS, (void*)&venc_chns_status);
		SC_LOGI("venc_chns_status: %u", venc_chns_status);
		for (i = 0; i < 32; i++) {
			if (venc_chns_status & (1 << i))
				_do_add_sms(i); // 给对应的编码数据建立rtsp推流sms
		}
#endif

	return 0;
}


int32_t module_uninit()
{
	int32_t ret;

#ifdef MODULE_RTSP
	ret = SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_UNINIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_RTSP_SERVER_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_VPP
	// 因为sdk核心和子模块原则上是分离的，sdk核心不引用子模块的头文件
	// 一般核心和子模块需要各自定义结构体来完成交互
	// 所以建议是通信用到的参数尽量用基础类型，如果用到结构体也需要尽量考虑通用和简洁
	ret = SDK_Cmd_Impl(SDK_CMD_VPP_UNINIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_WEBSOCKET
		ret = SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_UNINIT, NULL);
		if(ret < 0)
		{
			SC_LOGE("SDK_Cmd_Impl: SDK_CMD_WEBSOCKET_INIT Error, ERRCODE: %d", ret);
			return -1;
		}
#endif

	return ret;

}

int32_t module_stop()
{
	int32_t ret;
#ifdef MODULE_RTSP
	SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_DEL_SMS, NULL);
	SDK_Cmd_Impl(SDK_CMD_RTSP_SERVER_STOP, NULL);
#endif

#ifdef MODULE_VPP
	ret = SDK_Cmd_Impl(SDK_CMD_VPP_STOP, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_VPP_START Error, ERRCODE: %d", ret);
		return -1;
	}
	usleep(500*1000);
#endif

#ifdef MODULE_WEBSOCKET
	ret = SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_STOP, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_WEBSOCKET_START Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

	return 0;
}

static void signal_handler(int32_t signal_number)
{
	SC_LOGI("stop and uninit modules");
	module_stop();
	module_uninit();
	// 退出程序
	exit(0);
}

int32_t main(int32_t argc, char *argv[]) {
	int32_t ret = 0;

	// 接收程序退出信号，完成模块 stop 和 uninit
	signal(SIGINT, signal_handler);

	// 注册程序异常奔溃时的处理
	register_exception_signals();

	// 主程序完成子模块注册，启动websocket模块，由web通过websocket下发命令启动相应的功能模块
	sdk_globle_prerare();

#if 0
	// 从配置文件中获取默认的启动app id，目的是在不同的应用场景下，
	// 关注的应用不一样，这里可以在调试程序时改成每次启动默认应用
	app_id = get_default_app_id();
	if (app_id < 1) app_id = APP_BOX_1TO1;
#endif

	// 启动websocket，保证即使sensor没有接好的情况下，不至于和web通信不上
#ifdef MODULE_WEBSOCKET
	ret = SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_INIT, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_WEBSOCKET_INIT Error, ERRCODE: %d", ret);
		return -1;
	}
#endif

#ifdef MODULE_WEBSOCKET
	ret = SDK_Cmd_Impl(SDK_CMD_WEBSOCKET_START, NULL);
	if(ret < 0)
	{
		SC_LOGE("SDK_Cmd_Impl: SDK_CMD_WEBSOCKET_START Error, ERRCODE: %d", ret);
		return -1;
	}
#endif
#if 1
	ret = module_init();
	if (ret) {
		SC_LOGE("module_init failed, ret: %d\n", ret);
		goto loop;
	}

	ret = module_start();
	if (ret) {
		SC_LOGE("module_init failed, ret: %d\n", ret);
	}
	usleep(5*1000*1000);
loop:
#endif


	while(1)
	{
		usleep(5*1000*1000);
	}

	return 0;
}
