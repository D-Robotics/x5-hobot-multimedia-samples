#ifndef WEBSOCKET_HANDLE_H
#define WEBSOCKET_HANDLE_H

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	WEBSOCKET_STATE_NONE,
	WEBSOCKET_STATE_INIT,
	WEBSOCKET_STATE_UNINIT,
	WEBSOCKET_STATE_START,
	WEBSOCKET_STATE_STOP,
}WEBSOCKET_STATE_E;

typedef struct
{
	int					state;
	void* 				instance; // websocke server 实例
} websocket_handle_t;

int websocket_init();
int websocket_uninit();
int websocket_start();
int websocket_stop();
int websocket_upload_file(char *file_name);
int websocket_send_message(char *msg);
int websocket_register_callback();
int websocket_unregister_callback();

/*int websocket_param_get(void* param, unsigned int length);*/
/*int websocket_param_set(void* param, unsigned int length);*/

#ifdef __cplusplus
}
#endif

#endif // WEBSOCKET_HANDLE_H

