#include "communicate/sdk_communicate.h"
#include "communicate/sdk_common_struct.h"

#include "media_server.h"
#include "utils/utils_log.h"

int media_server_cmd_impl(SDK_CMD_E cmd, void* param);

static sdk_cmd_reg_t cmd_reg[] =
{
	{SDK_CMD_MEDIA_SERVER_INIT,				media_server_cmd_impl,				1},
	{SDK_CMD_MEDIA_SERVER_CREATE,			media_server_cmd_impl,				1},
	{SDK_CMD_MEDIA_SERVER_PUSH_DATA,		media_server_cmd_impl,				1},
	{SDK_CMD_MEDIA_SERVER_DESTROY,			media_server_cmd_impl,				1},
	{SDK_CMD_MEDIA_SERVER_UNINIT,			media_server_cmd_impl,				1}
};

int media_server_cmd_register()
{
	int i;
	for(i=0; i<sizeof(cmd_reg)/sizeof(cmd_reg[0]); i++)
	{
		sdk_cmd_register(cmd_reg[i].cmd, cmd_reg[i].call, cmd_reg[i].enable);
	}

	return 0;
}

int media_server_cmd_impl(SDK_CMD_E cmd, void* param)
{
	int ret = 0;
	switch(cmd)
	{
		case SDK_CMD_MEDIA_SERVER_INIT:
		{
			char *config_path = (char *)param;
			ret = media_server_init(config_path);
			break;
		}

		case SDK_CMD_MEDIA_SERVER_CREATE:
		{
			T_SDK_MEDIA_SRV_CREATE_PARAM* create_param = (T_SDK_MEDIA_SRV_CREATE_PARAM*)param;
			void* media = media_server_create_media(create_param->media_name,
				create_param->stream_name, create_param->codec_type_name);
			if(media == NULL){
				ret = -1;
			}else{
				ret = 0;
			}
			create_param->media = media;
			break;
		}
		case SDK_CMD_MEDIA_SERVER_PUSH_DATA:
		{
			T_SDK_MEDIA_SRV_PUSH_PARAM* push_param = (T_SDK_MEDIA_SRV_PUSH_PARAM *)param;
			ret = media_server_push_video(push_param->media, push_param->data,
				push_param->data_length, push_param->pts,
				push_param->dts, push_param->codec_name);
			break;
		}
		case SDK_CMD_MEDIA_SERVER_DESTROY:
		{
			void *media = (void *)param;
			media_server_destroy_media(media);
			ret = 0;
			break;
		}
		case SDK_CMD_MEDIA_SERVER_UNINIT:
		{
			media_server_uninit();
			ret = 0;
			break;
		}
		default:
			SC_LOGE("unknow cmd:%d", cmd);
			break;
	}

	return ret;
}






