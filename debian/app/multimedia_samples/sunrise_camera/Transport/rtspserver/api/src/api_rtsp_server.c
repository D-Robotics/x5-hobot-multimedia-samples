#include "api_rtsp_server.h"
#include "communicate/sdk_communicate.h"
#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "utils/utils_log.h"

#include "rtsp_server_handle.h"

int rtspserver_cmd_impl(SDK_CMD_E cmd, void* param);

static sdk_cmd_reg_t cmd_reg[] =
{
	{SDK_CMD_RTSP_SERVER_INIT,				rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_UNINIT,			rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_START,				rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_STOP,				rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_PREPARE,			rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_SATE_GET,			rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_PARAM_GET,			rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_PARAM_SET,			rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_ADD_SMS,			rtspserver_cmd_impl,				1},
	{SDK_CMD_RTSP_SERVER_DEL_SMS,			rtspserver_cmd_impl,				1},
};

int rtspserver_cmd_register()
{
	int i;
	for(i=0; i<sizeof(cmd_reg)/sizeof(cmd_reg[0]); i++)
	{
		sdk_cmd_register(cmd_reg[i].cmd, cmd_reg[i].call, cmd_reg[i].enable);
	}

	return 0;
}

int rtspserver_cmd_impl(SDK_CMD_E cmd, void* param)
{
	int ret = 0;
	switch(cmd)
	{
		case SDK_CMD_RTSP_SERVER_INIT:
		{
			ret = rtsp_server_init();
			break;
		}
		case SDK_CMD_RTSP_SERVER_UNINIT:
		{
			ret = rtsp_server_uninit();
			break;
		}
		case SDK_CMD_RTSP_SERVER_START:
		{
			ret = rtsp_server_start();
			break;
		}
		case SDK_CMD_RTSP_SERVER_STOP:
		{
			ret = rtsp_server_stop();
			break;
		}
		case SDK_CMD_RTSP_SERVER_PREPARE:
		{
			ret = rtsp_server_prepare();
			break;
		}
		case SDK_CMD_RTSP_SERVER_SATE_GET:
		{
			T_SDK_RTSP_STATE* p = (T_SDK_RTSP_STATE*)param;
			p->state = rtsp_server_param_state_get();
			break;
		}
		case SDK_CMD_RTSP_SERVER_PARAM_GET:
		{
			ret = rtsp_server_param_get(param, sizeof(T_SDK_RTSP_SRV_PARAM));
			break;
		}
		case SDK_CMD_RTSP_SERVER_PARAM_SET:
		{
			ret = rtsp_server_param_set(param, sizeof(T_SDK_RTSP_SRV_PARAM));
			break;
		}
		case SDK_CMD_RTSP_SERVER_ADD_SMS:
		{
			// 此处共用T_SDK_RTSP_SRV_PARAM参数，port字段不使用
			ret = rtsp_server_add_sms(param, sizeof(T_SDK_RTSP_SRV_PARAM));
			break;
		}
		case SDK_CMD_RTSP_SERVER_DEL_SMS:
		{
			rtsp_server_del_all_sms();
			break;
		}
		default:
			SC_LOGE("unknow cmd:%d", cmd);
			break;
	}

	return ret;
}






