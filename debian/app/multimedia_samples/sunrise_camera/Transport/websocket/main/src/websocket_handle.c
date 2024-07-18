#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <utils/stream_manager.h>
#include "websocket_handle.h"
#include "utils/utils_log.h"

#include "WebsocketWrap.h"

static websocket_handle_t s_websocket_handle;

int websocket_init()
{
	websocket_handle_t *handle = &s_websocket_handle;
	memset(handle, 0, sizeof(websocket_handle_t));
	handle->instance  = ws_wrap_instace();
	SC_LOGI("handle->instance:%p", handle->instance);

	handle->state = WEBSOCKET_STATE_INIT;
	return 0;
}

int websocket_uninit()
{
	ws_wrap_destory(s_websocket_handle.instance);
	s_websocket_handle.state = WEBSOCKET_STATE_UNINIT;
	return 0;
}

int websocket_start()
{
	ws_wrap_start(4567); // 默认就用 4567 端口
	s_websocket_handle.state = WEBSOCKET_STATE_START;
	return 0;
}

int websocket_stop()
{
	s_websocket_handle.state = WEBSOCKET_STATE_STOP;
	/*ws_wrap_stop();*/
	return 0; /* 暂时什么都不做 */
}

int websocket_upload_file(char *file_name)
{
	char upload_file_message[128] = {0};
	if (file_name == NULL || strlen(file_name) == 0)
		return -1;

	sprintf(upload_file_message, "{\"kind\":2,\"Filename\":\"%s\"}", file_name);
	ws_send_message(upload_file_message, strlen(upload_file_message));
	return 0;
}

int websocket_send_message(char *msg)
{
	if (msg == NULL || strlen(msg) == 0)
		return -1;
	ws_send_message(msg, strlen(msg));
	return 0;
}

int websocket_register_callback()
{
	return 0;
}

int websocket_unregister_callback()
{
	return 0;
}

