#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_vpp.h"
#include "solution_handle.h"
#include "communicate/sdk_communicate.h"
#include "communicate/sdk_common_cmd.h"
#include "communicate/sdk_common_struct.h"
#include "utils/utils_log.h"

int32_t vpp_cmd_impl(SDK_CMD_E cmd, void* param);

static sdk_cmd_reg_t cmd_reg[] =
{
	{SDK_CMD_VPP_RINGBUFFER_CREATE,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_RINGBUFFER_DATA_GET,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_RINGBUFFER_DATA_SYNC,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_RINGBUFFER_DESTORY,			vpp_cmd_impl,				1},

	{SDK_CMD_VPP_INIT,							vpp_cmd_impl,				1},
	{SDK_CMD_VPP_UNINIT,						vpp_cmd_impl,				1},
	{SDK_CMD_VPP_START,							vpp_cmd_impl,				1},
	{SDK_CMD_VPP_STOP,							vpp_cmd_impl,				1},
	{SDK_CMD_VPP_PARAM_SET,						vpp_cmd_impl,				1},
	{SDK_CMD_VPP_PARAM_GET,						vpp_cmd_impl,				1},
	{SDK_CMD_VPP_MIC_SWITCH,					vpp_cmd_impl,				1},
	{SDK_CMD_VPP_AENC_PARAM_GET,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_AENC_PARAM_SET,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_MOTION_PARAM_GET,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_MOTION_PARAM_SET,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_YUV_BUFFER_START,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_YUV_BUFFER_STOP,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_MIRROR_GET,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_MIRROR_SET,				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_FORCE_IDR, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_SNAP, 					vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_ALL_PARAM_GET,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_CHN_PARAM_GET,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GET_VENC_CHN_STATUS,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_SENSOR_NTSC_PAL_GET,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GPIO_STATE_SET, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GPIO_STATE_GET, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_ADC_VALUE_GET, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_ISP_MODE_SET, 					vpp_cmd_impl,				1},
	{SDK_CMD_VPP_ADEC_DATA_PUSH, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GET_RAW_FRAME, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GET_ISP_FRAME, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GET_VSE_FRAME, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_JPEG_SNAP, 					vpp_cmd_impl,				1},
	{SDK_CMD_VPP_START_RECORD, 					vpp_cmd_impl,				1},
	{SDK_CMD_VPP_STOP_RECORD, 					vpp_cmd_impl,				1},
	{SDK_CMD_VPP_VENC_BITRATE_SET, 				vpp_cmd_impl,				1},
	{SDK_CMD_VPP_GET_SOLUTION_CONFIG,			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_SET_SOLUTION_CONFIG, 			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_SAVE_SOLUTION_CONFIG, 			vpp_cmd_impl,				1},
	{SDK_CMD_VPP_RECOVERY_SOLUTION_CONFIG, 		vpp_cmd_impl,				1},
};

int32_t vpp_cmd_register()
{
	int32_t i;
	for(i=0; i<sizeof(cmd_reg)/sizeof(cmd_reg[0]); i++)
	{
		sdk_cmd_register(cmd_reg[i].cmd, cmd_reg[i].call, cmd_reg[i].enable);
	}

	return 0;
}

int32_t vpp_cmd_impl(SDK_CMD_E cmd, void* param)
{
	int32_t ret = 0;
	switch(cmd)
	{
		case SDK_CMD_VPP_INIT:
		{
			ret = solution_handle_init();
			break;
		}
		case SDK_CMD_VPP_UNINIT:
		{
			ret = solution_handle_uninit();
			break;
		}
		case SDK_CMD_VPP_START:
		{
			ret = solution_handle_start();
			break;
		}
		case SDK_CMD_VPP_STOP:
		{
			ret = solution_handle_stop();
			break;
		}
		case SDK_CMD_VPP_GET_SOLUTION_CONFIG:
		{
			ret = solution_handle_get_config((char *)param);
			break;
		}
		case SDK_CMD_VPP_SET_SOLUTION_CONFIG:
		{
			ret = solution_handle_set_config((char *)param);
			break;
		}
		case SDK_CMD_VPP_SAVE_SOLUTION_CONFIG:
		{
			ret = solution_handle_save_config((char *)param);
			break;
		}
		case SDK_CMD_VPP_RECOVERY_SOLUTION_CONFIG:
		{
			ret = solution_handle_recovery_config((char *)param);
			break;
		}
		case SDK_CMD_VPP_AENC_PARAM_GET:
		{
			T_SDK_VPP_AENC_PARAM* p = (T_SDK_VPP_AENC_PARAM*)param;
			aenc_info_t	info;
			uint32_t length = sizeof(aenc_info_t);
			ret = solution_handle_param_get(SOLUTION_AENC_PARAM_GET, (char*)&info, &length);
			if(ret != -1)
			{
				memcpy(p, &info, sizeof(T_SDK_VPP_AENC_PARAM));
			}
			break;
		}
		case SDK_CMD_VPP_MOTION_PARAM_GET:
		{
			T_SDK_VPP_MD_PARAM* p = (T_SDK_VPP_MD_PARAM*)param;
			motion_info_t info;
			uint32_t length = sizeof(motion_info_t);
			ret = solution_handle_param_get(SOLUTION_MOTION_PARAM_GET, (char*)&info, &length);
			if(ret != -1)
			{
				p->enable = info.enable;
				p->sensitivity = info.sensitivity;
				p->sustain = info.sustain;
				p->rect_enable = info.rect_enable;
				p->width = info.width;
				p->height = info.height;
			}
			break;
		}
		case SDK_CMD_VPP_MOTION_PARAM_SET:
		{
			T_SDK_VPP_MD_PARAM* p = (T_SDK_VPP_MD_PARAM*)param;
			motion_info_t info;
			uint32_t length = sizeof(motion_info_t);
			solution_handle_param_get(SOLUTION_MOTION_PARAM_GET, (char*)&info, &length);
			info.enable = p->enable;
			info.sensitivity = p->sensitivity;
			info.sustain = p->sustain;
			info.rect_enable = p->rect_enable;
//			info.width = p->width;
//			info.height = p->height;
			ret = solution_handle_param_set(SOLUTION_MOTION_PARAM_SET, (char*)&info, length);
			break;
		}
		case SDK_CMD_VPP_YUV_BUFFER_START:
		{
			T_SDK_VPP_YUV_BUFFER* p = (T_SDK_VPP_YUV_BUFFER*)param;
			solution_yuv_buffer_t yuv_buffer;
			uint32_t length = sizeof(solution_yuv_buffer_t);
			memcpy(&yuv_buffer, p, sizeof(T_SDK_VPP_YUV_BUFFER));
			ret = solution_handle_param_set(SOLUTION_YUV_BUFFER_START, (char*)&yuv_buffer, length);
			break;
		}
		case SDK_CMD_VPP_YUV_BUFFER_STOP:
		{
			ret = solution_handle_param_set(SOLUTION_YUV_BUFFER_STOP, NULL, 0);
			break;
		}
		case SDK_CMD_VPP_ADEC_FILE_PUSH:
		{
			T_SDK_DEC_FILE* pData = (T_SDK_DEC_FILE*)param;
			solution_handle_param_set(SOLUTION_DEC_FILE_PUSH, pData->file, strlen(pData->file));
			break;
		}
		case SDK_CMD_VPP_VENC_FORCE_IDR:
		{
			T_SDK_FORCE_I_FARME* pData = (T_SDK_FORCE_I_FARME*)param;
			solution_forceidr_t idr;
			idr.channel = pData->channel;
			idr.count = pData->num;
			ret = solution_handle_param_set(SOLUTION_VENC_FORCEIDR, (char*)&idr, sizeof(solution_forceidr_t));
			break;
		}
		case SDK_CMD_VPP_VENC_SNAP:
		{
			T_SDK_VPP_SNAP* pData = (T_SDK_VPP_SNAP*)param;
			ret = solution_handle_param_set(SOLUTION_SNAP_ONCE, pData->file, strlen(pData->file));
			break;
		}
		case SDK_CMD_VPP_ADEC_DATA_PUSH:
		{
			T_SDK_DEC_DATA* pData = (T_SDK_DEC_DATA*)param;

			ret = solution_handle_param_set(SOLUTION_DEC_DATA_PUSH, (char*)(pData->cp_data), pData->un_data_len);
			break;
		}
		case SDK_CMD_VPP_GPIO_STATE_SET:
		{
			T_SDK_GPIO*	gpio = (T_SDK_GPIO*)param;
//			SC_LOGI("un_type:%d un_value:%d", gpio->un_type, gpio->un_value);
			solution_gpoi_ctrl_t gpoi_ctrl;
			gpoi_ctrl.type = gpio->un_type;
			gpoi_ctrl.val = gpio->un_value;
			ret = solution_handle_param_set(SOLUTION_GPIO_CTRL_SET, (char*)&gpoi_ctrl, sizeof(solution_gpoi_ctrl_t));
			break;
		}
		case SDK_CMD_VPP_GPIO_STATE_GET:
		{
			T_SDK_GPIO*	gpio = (T_SDK_GPIO*)param;
//			SC_LOGI("un_type:%d", gpio->un_type);
			solution_gpoi_ctrl_t gpoi_ctrl;
			uint32_t lenght = sizeof(solution_gpoi_ctrl_t);
			gpoi_ctrl.type = gpio->un_type;
			ret = solution_handle_param_get(SOLUTION_GPIO_CTRL_GET, (char*)&gpoi_ctrl, &lenght);
			if(ret != -1)
			{
				gpio->un_value = gpoi_ctrl.val;
			}

			break;
		}
		case SDK_CMD_VPP_ADC_VALUE_GET:
		{
			T_SDK_ADC* adc = (T_SDK_ADC*)param;
//			SC_LOGI("un_type:%d", gpio->un_type);
			solution_adc_ctrl_t adc_ctrl;
			uint32_t lenght = sizeof(solution_adc_ctrl_t);
			adc_ctrl.ch = adc->un_ch;
			ret = solution_handle_param_get(SOLUTION_ADC_CTRL_GET, (char*)&adc_ctrl, &lenght);
			if(ret != -1)
			{
				adc->un_value = adc_ctrl.val;
			}
			break;
		}
		case SDK_CMD_VPP_ISP_MODE_SET:
		{
			T_SDK_ISP_MODE* p = (T_SDK_ISP_MODE*)param;
			ret = solution_handle_param_set(SOLUTION_ISP_CTRL_MODE_SET, (char*)(&p->un_mode), sizeof(unsigned int));
			break;
		}
		case SDK_CMD_VPP_VENC_MIRROR_GET:
		{
			T_SDK_VPP_MIRROR* p = (T_SDK_VPP_MIRROR*)param;
			int32_t mode;
			uint32_t length = sizeof(int);
			ret = solution_handle_param_get(SOLUTION_VENC_MIRROR_SET, (char*)&mode, &length);
			if(ret == 0)
			{
				p->mode = mode;
			}
			break;
		}
		case SDK_CMD_VPP_VENC_MIRROR_SET:
		{
			T_SDK_VPP_MIRROR* p = (T_SDK_VPP_MIRROR*)param;
			int32_t mode = p->mode;
			ret = solution_handle_param_set(SOLUTION_VENC_MIRROR_SET, (char*)&mode, sizeof(int));
			break;
		}
		case SDK_CMD_VPP_VENC_CHN_PARAM_GET:
		{
			T_SDK_VENC_INFO* p = (T_SDK_VENC_INFO*)param;
			uint32_t length = sizeof(T_SDK_VENC_INFO);
			ret = solution_handle_param_get(SOLUTION_VENC_CHN_PARAM_GET, (char*)p, &length);
			break;
		}
		case SDK_CMD_VPP_GET_VENC_CHN_STATUS:
		{
			unsigned int* p = (unsigned int*)param;
			uint32_t length = sizeof(unsigned int);
			ret = solution_handle_param_get(SOLUTION_GET_VENC_CHN_STATUS, (char*)p, &length);
			break;
		}
		case SDK_CMD_VPP_SENSOR_NTSC_PAL_GET:
		{
			T_SDK_SENSOR_NTSC_PAL* p = (T_SDK_SENSOR_NTSC_PAL*)param;
			int32_t ntsc_pal;
			uint32_t length = sizeof(int);
			ret = solution_handle_param_get(SOLUTION_VENC_NTSC_PAL_GET, (char*)&ntsc_pal, &length);
			if(ret == 0)
			{
				p->hz = (unsigned int)ntsc_pal;
			}
			else
			{
				p->hz = 0;
			}
			break;
		}
		case SDK_CMD_VPP_MIC_SWITCH:
		{
			T_SDK_VPP_MIC_SWITCH* p = (T_SDK_VPP_MIC_SWITCH*)param;
			int32_t mute = p->type;
			solution_handle_param_set(SOLUTION_AENC_MUTE_SET, (char*)&mute, sizeof(int));
			break;
		}
		case SDK_CMD_VPP_RINGBUFFER_CREATE:
		{
			T_SDK_VPP_RINGBUFFER* p = (T_SDK_VPP_RINGBUFFER*)param;
			uint32_t length = sizeof(T_SDK_VPP_RINGBUFFER);
			ret = solution_handle_param_get(SOLUTION_RINGBUFFER_CREATE, (char*)p, &length);
			break;
		}
		case SDK_CMD_VPP_RINGBUFFER_DATA_GET:
		{
			T_SDK_VPP_RINGBUFFER_DATA* p = (T_SDK_VPP_RINGBUFFER_DATA*)param;
			uint32_t length = sizeof(T_SDK_VPP_RINGBUFFER_DATA);
			ret = solution_handle_param_get(SOLUTION_RINGBUFFER_DATA_GET, (char*)p, &length);
			break;
		}
		case SDK_CMD_VPP_RINGBUFFER_DATA_SYNC:
		{
			T_SDK_VPP_RINGBUFFER* p = (T_SDK_VPP_RINGBUFFER*)param;
			uint32_t length = sizeof(T_SDK_VPP_RINGBUFFER);
			ret = solution_handle_param_get(SOLUTION_RINGBUFFER_DATA_SYNC, (char*)p, &length);
			break;
		}
		case SDK_CMD_VPP_RINGBUFFER_DESTORY:
		{
			T_SDK_VPP_RINGBUFFER* p = (T_SDK_VPP_RINGBUFFER*)param;
			uint32_t length = sizeof(T_SDK_VPP_RINGBUFFER);
			ret = solution_handle_param_get(SOLUTION_RINGBUFFER_DESTORY, (char*)p, &length);
			break;
		}
		case SDK_CMD_VPP_GET_RAW_FRAME:
		{
			uint32_t length = sizeof(int32_t);
			ret = solution_handle_param_get(SOLUTION_GET_RAW_FRAME, (char*)param, &length);
			break;
		}
		case SDK_CMD_VPP_GET_ISP_FRAME:
		{
			uint32_t length = sizeof(int32_t);
			ret = solution_handle_param_get(SOLUTION_GET_ISP_FRAME, (char*)param, &length);
			break;
		}
		case SDK_CMD_VPP_GET_VSE_FRAME:
		{
			uint32_t length = sizeof(int32_t);
			ret = solution_handle_param_get(SOLUTION_GET_VSE_FRAME, (char*)param, &length);
			break;
		}
		case SDK_CMD_VPP_JPEG_SNAP:
		{
			uint32_t length = sizeof(int32_t);
			ret = solution_handle_param_get(SOLUTION_JPEG_SNAP, (char*)param, &length);
			break;
		}
		case SDK_CMD_VPP_START_RECORD:
			ret = solution_handle_param_set(SOLUTION_START_RECORDER, (char*)param, sizeof(int));
			break;
		case SDK_CMD_VPP_STOP_RECORD:
			ret = solution_handle_param_set(SOLUTION_STOP_RECORDER, (char*)param, sizeof(int));
			break;
		case SDK_CMD_VPP_VENC_BITRATE_SET:
			ret = solution_handle_param_set(SOLUTION_VENC_BITRATE_SET, (char*)param, sizeof(int));
			break;
		default:
			ret = -1;
			SC_LOGE("unknow cmd:%d", cmd);
			break;
	}

	return ret;
}
