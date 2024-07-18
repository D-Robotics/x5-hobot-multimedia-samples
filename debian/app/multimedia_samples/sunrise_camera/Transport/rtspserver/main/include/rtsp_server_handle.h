#ifndef RTSP_SERVER_HANDLE_H
#define RTSP_SERVER_HANDLE_H

#ifdef __cplusplus
extern "C"{
#endif

#include "rtsp_server_default_param.h"

typedef enum
{
	RTSP_SRV_STATE_NONE,
	RTSP_SRV_STATE_INIT,
	RTSP_SRV_STATE_UNINIT,
	RTSP_SRV_STATE_PREPARE,
	RTSP_SRV_STATE_DISPREPARE,
	RTSP_SRV_STATE_START,
	RTSP_SRV_STATE_STOP,
}RTSP_SRV_STATE_E;

typedef struct
{
	int					state;
	void* 				instance;
	rtspserver_info_t	params[32]; // 保存各sms配置
}rtsp_server_t;

int rtsp_server_init();
int rtsp_server_uninit();
int rtsp_server_start();
int rtsp_server_stop();
int rtsp_server_prepare();
int rtsp_server_param_state_get();
int rtsp_server_param_get(void* param, unsigned int length);
int rtsp_server_param_set(void* param, unsigned int length);
int rtsp_server_add_sms(void* param, unsigned int length);
int rtsp_server_del_all_sms(void);
/*int rtsp_server_h264_data_put(int cmd, long long index, unsigned char* data, unsigned int length);*/
/*int rtsp_server_pcma_data_put(unsigned char* data, unsigned int length);*/
#ifdef __cplusplus
}
#endif

#endif