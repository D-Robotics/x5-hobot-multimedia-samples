#include <stdio.h>
#include <stdlib.h>

#include "sdk_communicate.h"

static sdk_cmd_reg_t cmd_reg[SDK_CMD_NULL];

#ifdef MODULE_SYSTEM
extern int system_cmd_register();
#endif
#ifdef MODULE_VPP
extern int vpp_cmd_register();
#endif
#ifdef MODULE_NETWORK
extern int network_cmd_register();
#endif
#ifdef MODULE_ALARM
extern int alarm_cmd_register();
#endif
#ifdef MODULE_RECORD
extern int record_cmd_register();
#endif
#ifdef MODULE_RTSP
extern int rtspserver_cmd_register();
#endif

#ifdef MODULE_WEBSOCKET
extern int websocket_cmd_register();
#endif


int sdk_globle_prerare()
{
#ifdef MODULE_SYSTEM
	system_cmd_register();
#endif
#ifdef MODULE_VPP
	vpp_cmd_register();
#endif
#ifdef MODULE_NETWORK
	network_cmd_register();
#endif
#ifdef MODULE_ALARM
	alarm_cmd_register();
#endif
#ifdef MODULE_RECORD
	record_cmd_register();
#endif
#ifdef MODULE_RTSP
	rtspserver_cmd_register();
#endif
#ifdef MODULE_WEBSOCKET
	websocket_cmd_register();
#endif

	return 0;
}

int sdk_cmd_register(SDK_CMD_E cmd, SDK_CMD_IMPL_CALL call, int enable)
{
	cmd_reg[cmd].cmd = cmd;
	cmd_reg[cmd].call = call;
	cmd_reg[cmd].enable = enable;
	return 0;
}

int sdk_cmd_unregister(SDK_CMD_E cmd)
{
	cmd_reg[cmd].call = NULL;
	cmd_reg[cmd].enable = 0;
	return 0;
}

int sdk_cmd_impl(SDK_CMD_E cmd, void* param)
{
	int ret = 0;

	if(cmd >= SDK_CMD_NULL)
	{
		printf("sdk_cmd_impl unknow cmd:%d\n", cmd);
		return -1;	// unknow cmd
	}

	if(cmd_reg[cmd].enable == 1 && cmd_reg[cmd].call != NULL)
	{
		ret = cmd_reg[cmd].call(cmd, param);
	}

	return ret;
}
