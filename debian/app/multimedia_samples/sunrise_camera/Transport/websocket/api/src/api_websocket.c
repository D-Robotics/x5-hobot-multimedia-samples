#include "api_websocket.h"
#include "communicate/sdk_communicate.h"
#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "utils/utils_log.h"

#include "websocket_handle.h"

int websocket_cmd_impl(SDK_CMD_E cmd, void* param);

static sdk_cmd_reg_t cmd_reg[] =
{
	{SDK_CMD_WEBSOCKET_INIT,			websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_UNINIT,			websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_START,			websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_STOP,			websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_PARAM_GET,		websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_PARAM_SET,		websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_UPLOAD_FILE,		websocket_cmd_impl,				1},
	{SDK_CMD_WEBSOCKET_SEND_MSG,		websocket_cmd_impl,				1},
};

int websocket_cmd_register()
{
	int i;
	for(i=0; i<sizeof(cmd_reg)/sizeof(cmd_reg[0]); i++)
	{
		sdk_cmd_register(cmd_reg[i].cmd, cmd_reg[i].call, cmd_reg[i].enable);
	}

	return 0;
}

int websocket_cmd_impl(SDK_CMD_E cmd, void* param)
{
	int ret = 0;
	switch(cmd)
	{
		case SDK_CMD_WEBSOCKET_INIT:
		{
			ret = websocket_init();
			break;
		}
		case SDK_CMD_WEBSOCKET_UNINIT:
		{
			ret = websocket_uninit();
			break;
		}
		case SDK_CMD_WEBSOCKET_START:
		{
			ret = websocket_start();
			break;
		}
		case SDK_CMD_WEBSOCKET_STOP:
		{
			ret = websocket_stop();
			break;
		}
		case SDK_CMD_WEBSOCKET_UPLOAD_FILE:
		{
			ret = websocket_upload_file((char*)param);
			break;
		}
		case SDK_CMD_WEBSOCKET_SEND_MSG:
		{
			ret = websocket_send_message((char*)param);
			break;
		}
#if 0
		case SDK_CMD_WEBSOCKET_PARAM_GET:
		{
			ret = websocket_param_get(param, sizeof(T_SDK_RTSP_SRV_PARAM));
			break;
		}
		case SDK_CMD_WEBSOCKET_PARAM_SET:
		{
			ret = websocket_param_set(param, sizeof(T_SDK_RTSP_SRV_PARAM));
			break;
		}
#endif
		default:
			SC_LOGE("unknow cmd:%d", cmd);
			break;
	}

	return ret;
}







