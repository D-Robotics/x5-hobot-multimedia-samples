#ifndef _SDK_COMMON_CMD_H
#define _SDK_COMMON_CMD_H

typedef enum
{
	SDK_CMD_VPP_RINGBUFFER_CREATE,		// T_SDK_VPP_RINGBUFFER	RINGBUFFER_T
	SDK_CMD_VPP_RINGBUFFER_DATA_GET,		// T_SDK_VPP_RINGBUFFER_DATA
	SDK_CMD_VPP_RINGBUFFER_DATA_SYNC,	// T_SDK_VPP_RINGBUFFER
	SDK_CMD_VPP_RINGBUFFER_DESTORY, 		// T_SDK_VPP_RINGBUFFER

	SDK_CMD_VPP_INIT,
	SDK_CMD_VPP_UNINIT,
	SDK_CMD_VPP_START,
	SDK_CMD_VPP_STOP,
	SDK_CMD_VPP_PARAM_SET,
	SDK_CMD_VPP_PARAM_GET,
	SDK_CMD_VPP_MIC_SWITCH,			// T_SDK_VPP_MIC_SWITCH
	SDK_CMD_VPP_AENC_PARAM_SET,		// T_SDK_VPP_AENC_PARAM
	SDK_CMD_VPP_AENC_PARAM_GET,
	SDK_CMD_VPP_MOTION_PARAM_GET,	// T_SDK_VPP_MD_PARAM
	SDK_CMD_VPP_MOTION_PARAM_SET,
	SDK_CMD_VPP_YUV_BUFFER_START,	// T_SDK_VPP_YUV_BUFFER
	SDK_CMD_VPP_YUV_BUFFER_STOP,
	SDK_CMD_VPP_VENC_MIRROR_GET,		// T_SDK_VPP_MIRROR
	SDK_CMD_VPP_VENC_MIRROR_SET,
	SDK_CMD_VPP_VENC_FORCE_IDR,		// T_SDK_FORCE_I_FARME
	SDK_CMD_VPP_VENC_SNAP,			// T_SDK_VPP_SNAP
	SDK_CMD_VPP_VENC_ALL_PARAM_GET,
	SDK_CMD_VPP_VENC_CHN_PARAM_GET,
	SDK_CMD_VPP_GET_VENC_CHN_STATUS,
	SDK_CMD_VPP_SENSOR_NTSC_PAL_GET, // T_SDK_SENSOR_NTSC_PAL
	SDK_CMD_VPP_GPIO_STATE_SET,		// T_SDK_GPIO
	SDK_CMD_VPP_GPIO_STATE_GET,
	SDK_CMD_VPP_ADC_VALUE_GET,		// T_SDK_ADC
	SDK_CMD_VPP_ISP_MODE_SET,		// T_SDK_ISP_MODE
	SDK_CMD_VPP_ADEC_DATA_PUSH,		// T_SDK_DEC_DATA
	SDK_CMD_VPP_ADEC_FILE_PUSH,		// T_SDK_DEC_FILE
	SDK_CMD_VPP_GET_RAW_FRAME,
	SDK_CMD_VPP_GET_ISP_FRAME,
	SDK_CMD_VPP_GET_VSE_FRAME,
	SDK_CMD_VPP_JPEG_SNAP,
	SDK_CMD_VPP_START_RECORD,
	SDK_CMD_VPP_STOP_RECORD,
	SDK_CMD_VPP_VENC_BITRATE_SET,	// 设置比特率
	SDK_CMD_VPP_GET_SOLUTION_CONFIG,
	SDK_CMD_VPP_SET_SOLUTION_CONFIG,
	SDK_CMD_VPP_SAVE_SOLUTION_CONFIG,
	SDK_CMD_VPP_RECOVERY_SOLUTION_CONFIG,

	SDK_CMD_FACTURE_INIT,
	SDK_CMD_FACTURE_UNINIT,
	SDK_CMD_FACTURE_START,
	SDK_CMD_FACTURE_STOP,
	SDK_CMD_FACTURE_PARAM_SET,
	SDK_CMD_FACTURE_PARAM_GET,

	SDK_CMD_UP2P_INIT,
	SDK_CMD_UP2P_UNINIT,
	SDK_CMD_UP2P_START,
	SDK_CMD_UP2P_STOP,
	SDK_CMD_UP2P_PARAM_SET,
	SDK_CMD_UP2P_PARAM_GET,
	SDK_CMD_UP2P_TRANS_PARAM_SET,	// T_SDK_UP2P_TRANS_PARAM
	SDK_CMD_UP2P_TRANS_PARAM_GET,
	SDK_CMD_UP2P_CONF_PARAM_SET,	// T_SDK_UP2P_CONF_PARAM
	SDK_CMD_UP2P_CONF_PARAM_GET,
	SDK_CMD_UP2P_TCP_START_SET,		// T_SDK_UP2P_TCP_SWITCH
	SDK_CMD_UP2P_TCP_STOP_SET,

	SDK_CMD_TUTK_INIT,
	SDK_CMD_TUTK_UNINIT,
	SDK_CMD_TUTK_START,
	SDK_CMD_TUTK_STOP,
	SDK_CMD_TUTK_TRANS_PARAM_SET,	// T_SDK_TUTK_TRANS_PARAM
	SDK_CMD_TUTK_TRANS_PARAM_GET,
	SDK_CMD_TUTK_CONF_PARAM_SET,	// T_SDK_TUTK_TRANS_PARAM
	SDK_CMD_TUTK_CONF_PARAM_GET,

	SDK_CMD_RTSP_SERVER_INIT,
	SDK_CMD_RTSP_SERVER_UNINIT,
	SDK_CMD_RTSP_SERVER_START,
	SDK_CMD_RTSP_SERVER_STOP,
	SDK_CMD_RTSP_SERVER_PREPARE,
	SDK_CMD_RTSP_SERVER_SATE_GET,		// T_SDK_RTSP_STATE
	SDK_CMD_RTSP_SERVER_PARAM_GET,		// T_SDK_RTSP_SRV_PARAM
	SDK_CMD_RTSP_SERVER_PARAM_SET,
	SDK_CMD_RTSP_SERVER_ADD_SMS,
	SDK_CMD_RTSP_SERVER_DEL_SMS,

	SDK_CMD_RTMP_INIT,
	SDK_CMD_RTMP_UNINIT,
	SDK_CMD_RTMP_START,
	SDK_CMD_RTMP_STOP,

	SDK_CMD_ULIFE_INIT,
	SDK_CMD_ULIFE_UNINIT,
	SDK_CMD_ULIFE_START,
	SDK_CMD_ULIFE_STOP,
	SDK_CMD_ULIFE_UP2P_TCP_RESP,
	SDK_CMD_ULIFE_PARAM_SET,			// T_SDK_ULIFE_PARAM
	SDK_CMD_ULIFE_PARAM_GET,

	SDK_CMD_ALARM_INIT,
	SDK_CMD_ALARM_UNINIT,
	SDK_CMD_ALARM_START,
	SDK_CMD_ALARM_STOP,
	SDK_CMD_ALARM_CB_REGISTER,			// T_SDK_ALARM_CB
	SDK_CMD_ALARM_CB_UNREGISTER,
	SDK_CMD_ALARM_EVENT_PUSH,			// T_SDK_ALARM_INFO

	SDK_CMD_RECORD_INIT,
	SDK_CMD_RECORD_UNINIT,
	SDK_CMD_RECORD_START,
	SDK_CMD_RECORD_STOP,
	SDK_CMD_RECORD_PARAM_SET,			// T_SDK_REC_PARAM
	SDK_CMD_RECORD_PARAM_GET,
	SDK_CMD_RECORD_STORAGE_INFO_GET,	// T_SDK_REC_STORAGE_INFO
	SDK_CMD_RECORD_STORAGE_FORMAT,
	SDK_CMD_RECORD_MONTH_LIST_GET,		// T_SDK_RECORD_MONTH_LIST
	SDK_CMD_RECORD_DAY_LIST_GET,		// T_SDK_RECORD_DAY_LIST
	SDK_CMD_RECORD_ALL_FILETIME_GET,		// T_SDK_GET_RECORD_LIST
	SDK_CMD_RECORD_ALL_FILETIME_ALARM_GET,	// T_SDK_GET_RECORD_LIST
	SDK_CMD_RECORD_REFRESH_LIST_GET,		// T_SDK_REFRESH_RECORD_LIST
	SDK_CMD_RECORD_REFRESH_LIST_ALARM_GET,	// T_SDK_REFRESH_RECORD_LIST
	SDK_CMD_RECORD_OLDEST_TIME_LIST_GET,	// T_SDK_GET_RECORD_LIST_OLDEST_TIME

	SDK_CMD_ALIYUN_INIT,
	SDK_CMD_ALIYUN_UNINIT,
	SDK_CMD_ALIYUN_START,
	SDK_CMD_ALIYUN_STOP,
	SDK_CMD_ALIYUN_MEDIA_CONFIG_SET,	//	T_SDK_ALIYUN_CONFIG
	SDK_CMD_ALIYUN_MEDIA_CONFIG_GET,
	SDK_CMD_ALIYUN_PIC_CONFIG_SET,
	SDK_CMD_ALIYUN_PIC_CONFIG_GET,
	SDK_CMD_ALIYUN_CLEARCACHE,
	SDK_CMD_ALIYUN_BUFFERING_MODE_GET,	// T_SDK_ALIYUN_BUF_MODE
	SDK_CMD_ALIYUN_BUFFERING_MODE_SET,
	SDK_CMD_ALIYUN_PUSH,				//	T_SDK_ALIYUN_EVENT

	SDK_CMD_NETWORK_INIT,
	SDK_CMD_NETWORK_UNINIT,
	SDK_CMD_NETWORK_START,
	SDK_CMD_NETWORK_STOP,
	SDK_CMD_NETWORK_PARAM_SET,
	SDK_CMD_NETWORK_PARAM_GET,
	SDK_CMD_NETWORK_BASE_INFO_GET,	// T_SDK_NET_BASEINFO
	SDK_CMD_NETWORK_TYPE_SET,		// T_SDK_NET_TYPE
	SDK_CMD_NETWORK_TYPE_GET,
	SDK_CMD_NETWORK_STATE_GET,		// T_SDK_NET_STATE
	SDK_CMD_NETWORK_MODE_SET,		// T_SDK_NET_MODE
	SDK_CMD_NETWORK_MODE_GET,
	SDK_CMD_NETWORK_REGISTER_SMART_CALLBACK,	// F_SDK_SMARTLINK_RESULT_CB
	SDK_CMD_NETWORK_REGISTER_STATE_CALLBACK,	// F_SDK_NETWORK_STATE_CB
	SDK_CMD_NETWORK_WIFI_LIST_GET,		// T_SDK_WIFI_LIST
	SDK_CMD_NETWORK_STA_WIFIINFO_SET,	// T_SDK_SSID_INFO
	SDK_CMD_NETWORK_STA_WIFIINFO_GET,
	SDK_CMD_NETWORK_AP_WIFIINFO_SET,	// T_SDK_SSID_INFO
	SDK_CMD_NETWORK_AP_WIFIINFO_GET,

	SDK_CMD_SYSTEM_INIT,
	SDK_CMD_SYSTEM_UNINIT,
	SDK_CMD_SYSTEM_START,
	SDK_CMD_SYSTEM_STOP,
	SDK_CMD_SYSTEM_PARAM_SET,
	SDK_CMD_SYSTEM_PARAM_GET,			// T_SDK_SYS_PARAM
	SDK_CMD_SYSTEM_DEVICE_PARAM_SET,	// T_SDK_SYS_DEVICE_PARAM
	SDK_CMD_SYSTEM_DEVICE_PARAM_GET,
	SDK_CMD_SYSTEM_ABILITY_PARAM_GET,	// T_SDK_SYS_ABILITY_PARAM
	SDK_CMD_SYSTEM_LED_PARAM_SET,		// T_SDK_SYS_LED_PARAM
	SDK_CMD_SYSTEM_LED_PARAM_GET,
	SDK_CMD_SYSTEM_IRCUT_PARAM_GET,		// T_SDK_SYS_IRCUT_PARAM
	SDK_CMD_SYSTEM_LED_STATE_SET,		// T_SDK_LED_STATE
	SDK_CMD_SYSTEM_IRCUT_STATE_SET,		// T_SDK_IRCUT_STATE
	SDK_CMD_SYSTEM_ANN_SET,				// T_SDK_ANN
	SDK_CMD_SYSTEM_UPGRADE_CONF_SET,	// T_SDK_SYS_UPGRADE_CONF
	SDK_CMD_SYSTEM_UPGRADE_START,
	SDK_CMD_SYSTEM_UPGRADE_STOP,
	SDK_CMD_SYSTEM_UPGRADE_PROSS_GET,	// T_SDK_SYS_UPGRADE_PROSS
	SDK_CMD_SYSTEM_NTP_CONF_GET,		// T_SDK_SYS_TIME_PARAM
	SDK_CMD_SYSTEM_NTP_CONF_SET,
	SDK_CMD_SYSTEM_NTP_SYNC_READY,

	SDK_CMD_WEBSOCKET_INIT,
	SDK_CMD_WEBSOCKET_UNINIT,
	SDK_CMD_WEBSOCKET_START,
	SDK_CMD_WEBSOCKET_STOP,
	SDK_CMD_WEBSOCKET_PARAM_GET,		// T_SDK_RTSP_SRV_PARAM
	SDK_CMD_WEBSOCKET_PARAM_SET,
	SDK_CMD_WEBSOCKET_UPLOAD_FILE,
	SDK_CMD_WEBSOCKET_SEND_MSG,

	SDK_CMD_NULL,
}SDK_CMD_E;

#endif
