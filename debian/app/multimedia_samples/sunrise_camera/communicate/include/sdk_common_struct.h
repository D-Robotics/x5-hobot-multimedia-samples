#ifndef _SDK_COMMONSTRUCT_H
#define _SDK_COMMONSTRUCT_H

#if defined (__cplusplus)
extern "C" {
#endif
#include <time.h>

///////////////////////////////up2p///////////////////////////////////////
typedef struct
{
	int			p2p_enable;
	int			tcp_enable;
	int			vquality;
}T_SDK_UP2P_TRANS_PARAM;

typedef struct
{
	char		dispatch[128];
	char		p2p_server[128];
	char		tcp_server[128];
	int			p2p_port;
	int			tcp_port;
}T_SDK_UP2P_CONF_PARAM;

typedef struct
{
	char		url[256];
}T_SDK_UP2P_TCP_SWITCH;

typedef enum
{
	T_SDK_RTSP_AUDIO_TYPE_LPCM,
}T_SDK_RTSP_AUDIO_TYPE_E;

typedef struct
{
	int 	enable;
	T_SDK_RTSP_AUDIO_TYPE_E		type;

	int 	samplerate;
	int		channels;
	int		bitspersample;
}T_SDK_RTSP_AUDIO_INFO_T;

typedef enum
{
	T_SDK_RTSP_VIDEO_TYPE_H264,
	T_SDK_RTSP_VIDEO_TYPE_H265,
	T_SDK_RTSP_VIDEO_TYPE_MJPEG,
}T_SDK_RTSP_VIDEO_TYPE_E;

typedef struct
{
	int 	enable;
	T_SDK_RTSP_VIDEO_TYPE_E		type;

	int		framerate;
}T_SDK_RTSP_VIDEO_INFO_T;

typedef enum
{
	T_SDK_RTSP_STATE_NONE,
	T_SDK_RTSP_STATE_INIT,
	T_SDK_RTSP_STATE_UNINIT,
	T_SDK_RTSP_STATE_PREPARE,
	T_SDK_RTSP_STATE_DISPREPARE,
	T_SDK_RTSP_STATE_START,
	T_SDK_RTSP_STATE_STOP,
}T_SDK_RTSP_STATE_E;

typedef struct
{
	T_SDK_RTSP_STATE_E state;
}T_SDK_RTSP_STATE;

typedef struct
{
	char		prefix[128];
	int			port;
	T_SDK_RTSP_AUDIO_INFO_T	audio;
	T_SDK_RTSP_VIDEO_INFO_T	video;
	// 以下是共享内存区域的配置
	char shm_id[32];
	char shm_name[32];
	int stream_buf_size;
	int suggest_buffer_item_count;
	int suggest_buffer_region_size;
}T_SDK_RTSP_SRV_PARAM;


///////////////////////////////camera///////////////////////////////////////
typedef void* P_RINGBUFFER;

typedef enum
{
	E_SDK_VPP_RINGBUFFER_VIDEO_MAIN,
	E_SDK_VPP_RINGBUFFER_VIDEO_SUB,
	E_SDK_VPP_RINGBUFFER_AUDIO_MAIN,
}E_SDK_VPP_RINGBUFFER;

typedef struct
{
	char					id[64];
	E_SDK_VPP_RINGBUFFER  	name;
	P_RINGBUFFER			buffer;
}T_SDK_VPP_RINGBUFFER;

typedef struct
{
	int type;
	int key;
	int seq;
	unsigned int 		length;
	unsigned long long 	pts;
	unsigned int		t_time;
	unsigned int		framerate;	// or samplerate
	unsigned int		width;
	unsigned int		height;
	char reserved[12];
}T_SDK_VPP_RINGBUFFER_FRAME_INFO;

typedef struct
{
	T_SDK_VPP_RINGBUFFER 	buff;
	unsigned char*			data;
	unsigned int			lenght;
	T_SDK_VPP_RINGBUFFER_FRAME_INFO	info;
}T_SDK_VPP_RINGBUFFER_DATA;

typedef struct
{
	int type;
}T_SDK_VPP_MIC_SWITCH;

typedef enum
{
	E_SDK_PAYLOAD_TYPE_G711A		=	19,
	E_SDK_PAYLOAD_TYPE_AAC			=	37,
	E_SDK_PAYLOAD_TYPE_H264			= 	96,
	E_SDK_PAYLOAD_TYPE_H265			=	265,
	E_SDK_PAYLOAD_TYPE_MJPEG		=	1002,
}E_SDK_PAYLOAD_TYPE;

typedef struct{
	int			enable;		// 开关
	int			channel;	// 通道号
	E_SDK_PAYLOAD_TYPE	type;		// 编码类型
	int			mic_input;	// mic输入类型0: mic 1:line
	int			mic_mute;	// 输入静音

	int			channels;	// 声道数
	int			bitrate;	// 码率
	int			samplerate;	// 采样率
	int			volume;		// 输入音量

	int			aec;		// 回声抵消算法
	int			agc;		// 自动增益算法
	int			anr;		// 语音降噪
}T_SDK_VPP_AENC_PARAM;

typedef struct{
	int 	enable;
	int		width;					// 分辨率宽
	int 	height;					// 分辨率高
	int		sensitivity;
	int		rect_enable;			// 16个位分别对应16个小块是否使能
	int		sustain;				// 一个移动侦测持续时长
}T_SDK_VPP_MD_PARAM;

typedef struct
{
	int		ch;
	int		max_frame;
	int		max_size;
	char	name[64];
}T_SDK_VPP_YUV_BUFFER;

typedef struct
{
	int mode;
}T_SDK_VPP_MIRROR;

typedef struct
{
	unsigned int	channel;
	unsigned int	num;
}T_SDK_FORCE_I_FARME;

typedef struct
{
	int		un_count;
	char	file[64];
}T_SDK_VPP_SNAP;

typedef struct{
	int			enable;
	int			channel;	// 通道号
	int			type;		// 编码类型 H264 H265
	int			mirror;		// 旋转
	int			flip;		// 镜像

	int			framerate;	// 帧率
	int			bitrate;	// 码率或最大码率
	int			stream_buf_size; // 编码器设置的buf size
	int			profile;	// H264的base main high; H265 high
	int			gop;		// I帧间隔
	int			width;		// 分辨率宽
	int 		height;		// 分辨率高
	int			cvbr;		// 码率控制方式 定码率 变码率
	int			minqp;
	int			maxqp;
	int         suggest_buffer_region_size;
	int 		suggest_buffer_item_count;

}T_SDK_VENC_INFO;

typedef struct
{
	unsigned int hz;
}T_SDK_SENSOR_NTSC_PAL;

typedef enum
{
	E_SDK_GPIO_LED_RED,
	E_SDK_GPIO_LED_GREEN,
	E_SDK_GPIO_SPEAKER,
	E_SDK_GPIO_BTN_RESET,
	E_SDK_GPIO_LAMP,
	E_SDK_GPIO_IRCUT,
	E_SDK_GPIO_BUTT,
	E_SDK_GPIO_DOORBELL,
	E_SDK_GPIO_433_K1,
	E_SDK_GPIO_433_K2,
	E_SDK_GPIO_ACBELL,
	E_SDK_GPIO_BAT_POWER,
}E_SDK_GPIO_TYPE;

typedef struct
{
	E_SDK_GPIO_TYPE un_type;
	unsigned int    un_value;
}T_SDK_GPIO;


typedef struct
{
	unsigned int un_ch;
	unsigned int un_value;
}T_SDK_ADC;

typedef struct
{
	unsigned int un_mode;	 // 0:day 1:night
}T_SDK_ISP_MODE;

typedef struct
{
	unsigned char*	cp_data;
	unsigned int	un_data_len;
} T_SDK_DEC_DATA;

typedef struct
{
	char	file[1024];
}T_SDK_DEC_FILE;

////////////////////////////////////////////////////////////////////////////
typedef struct
{
	char		dispatch[128];
	int			dispatch_port;
	char		server[128];
	int			server_port;
	int			heartbead;
}T_SDK_ULIFE_CONF;

typedef struct
{
	int			vquality;
}T_SDK_ULIFE_TRANS;

typedef struct
{
	int			regtype;		// 配网上报注册 0.sta 1.AP或4G
}T_SDK_ULIFE_EXTERN;

typedef struct
{
	T_SDK_ULIFE_CONF	conf;
	T_SDK_ULIFE_TRANS	trans;
	T_SDK_ULIFE_EXTERN	ex;
}T_SDK_ULIFE_PARAM;

///////////////////////////////record///////////////////////////////////////

typedef enum
{
	E_SDK_REC_MEDIA_TYPE_H264G711A,
	E_SDK_REC_MEDIA_TYPE_MP4,
	E_SDK_REC_MEDIA_TYPE_AVI,
	E_SDK_REC_MEDIA_TYPE_NULL,
}E_SDK_REC_MEDIA_TYPE;

typedef enum
{
	E_SDK_REC_TYPE_NULL		= 0,
	E_SDK_REC_TYPE_ALLDAY 	= 1,
	E_SDK_REC_TYPE_ALARM 	= 2,
	E_SDK_REC_TYPE_PLAN 	= 4,
	E_SDK_REC_TYPE_MANUAL 	= 8,
}E_SDK_REC_TYPE;

typedef enum
{
	E_SDK_REC_WTYPE_NULL,
	E_SDK_REC_WTYPE_DISKMGR,
	E_SDK_REC_WTYPE_PATH,
}E_SDK_REC_WTYPE;

typedef struct
{
	E_SDK_REC_MEDIA_TYPE 	mediatype;		//录像类型 MP4 AVI
	E_SDK_REC_TYPE			type;			//录像类型 全天录像，报警录像，计划录像，手动录像
	int 					duration;		//录像时长
	E_SDK_REC_WTYPE			wtype;			//文件写方式 预分配 非预分配
	char					path[64];		//默认文件路径
}T_SDK_REC_PARAM;

typedef struct
{
	unsigned int total;			//总容量
	unsigned int used;			//已用容量
	unsigned int free;			//未用容量
	unsigned int reserve[2];	//保留
}T_SDK_REC_STORAGE_INFO;

typedef struct
{
	char			a_month[16];	//example:"201401"  (input Param. 2014 means year,01 means month)
	char*			cp_list;		//example:"201401013|201401021|201401122|" (Output Param. 2014 means year , in 201401013 01 means month ,3 meas how many files exist)
	unsigned int	un_list_len;
}T_SDK_RECORD_MONTH_LIST;

typedef struct
{
	char			a_day[16];		//example:"20140112"(input Param. 2014 means year,01 means month,12 mens the day)
	unsigned int  	file_type;		//请求文件类型 (视频: 0 , 图片: 1)
	unsigned int 	file_counts;	//文件数
									/*example:"201401121132231bc0120.mp4|201401121133231bc0120.mp4|"
									(Output Param. in 201401121132231bc0120z.ts 2014 means year,01 means month ,
									12 meas the day,11 means hour, 32 means minute, 23 means second,
									1 means the file can not remove when auto circulate record is open,
									b means record type (value frome enum RECORD_TYPE + 'a')
									c means alarm type (value frome enum E_SDK_ALARM_TYPE + 'a'))
									012 means record time
									.mp4 meas record file type*/
	char*			cp_list;
	unsigned int	un_list_len;
}T_SDK_RECORD_DAY_LIST;

typedef struct
{
 	unsigned int StartTimeStamp;	//起始时间
	unsigned int EndTimeStamp;		//结束时间
	unsigned int AlarmType;			//告警类型
}T_SDK_RECORD_LIST;

typedef struct
{
	char* lastFd;						//上一次的索引的文件描述符
	unsigned int start_time;
	unsigned int end_time;				//jie
	T_SDK_RECORD_LIST record_list;
}T_SDK_GET_RECORD_LIST;


typedef struct
{
	unsigned int reference_time;	//输入参数刷新时间
	char* lastFd;
	T_SDK_RECORD_LIST record_list;	//
}T_SDK_REFRESH_RECORD_LIST;

typedef struct
{
	unsigned int oldest_time;		//获取索引最早的时间
}T_SDK_GET_RECORD_LIST_OLDEST_TIME;

///////////////////////////////aliyun///////////////////////////////////////

typedef struct
{
	int					enable;			//是否使能
	char 				endpoint[256];
	char 				access_key_id[1024];
	char 				access_key_secret[256];
	char 				bucket[256];
	char				token[2048];
	int					token_effective_duration;
	char				callback[128];
	char				prefix[256];	//远端目录
} T_SDK_ALIYUN_CONFIG;

typedef struct
{
	char			type;
	char			file[256];
	unsigned int	pts_s;
	unsigned int	pts_e;
	int 			alarm_type;
	int 			remove; 		//是否需要删除源文件 0:不删除 1:删除
	int 			resend; 		//是否重传 0:不重传 1:重传
}T_SDK_ALIYUN_EVENT;

typedef struct
{
	int		type;		// 0:media	1:picture
	int		mode;		// 0:tfcard 1:tmp
	int		max_size;
}T_SDK_ALIYUN_BUF_MODE;

///////////////////////////////alarm///////////////////////////////////////
typedef enum
{
	E_SDK_AlARM_TYPE_ALL,
	E_SDK_AlARM_TYPE_MD,
}E_SDK_AlARM_TYPE;

typedef struct
{
	int			type;
	time_t		start;
	int			duration;
}T_SDK_ALARM_INFO;

typedef int (*F_SDK_ALARM_EVENT_CB)(E_SDK_AlARM_TYPE type, T_SDK_ALARM_INFO info);

typedef struct
{
	char					name[64];
	E_SDK_AlARM_TYPE 		type;
	F_SDK_ALARM_EVENT_CB 	cb;
}T_SDK_ALARM_CB;

//////////////////////////////////////////////////////////////////////
typedef struct
{
	int			 mode;			//0:wired 1:wifi 2:4G
	char		 wired_ip[32];	//有线网络的ip地址
	char		 wired_mac[32];
	char		 wired_mask[32];

	char		 wifi_ip[32];	//wifi网络的ip地址
	char		 wifi_mac[32];
	char		 wifi_mask[32];
}T_SDK_NET_BASEINFO;

typedef enum
{
	E_SDK_NET_STATE_UNKNOWN,

	E_SDK_NET_DISCONNECT,
	E_SDK_NET_CONNECTING,
	E_SDK_NET_CONNECTED,
}E_SDK_NET_STATE;

typedef struct
{
	E_SDK_NET_STATE state;
}T_SDK_NET_STATE;

typedef enum
{
	E_SDK_NET_MODE_UNKNOWN,

	E_SDK_NET_MODE_WIRED,			//有线网络
	E_SDK_NET_MODE_WIFI_AP,
	E_SDK_NET_MODE_WIFI_STA,
	E_SDK_NET_MODE_WIFI_WPS,
	E_SDK_NET_MODE_WIFI_SMART,
	E_SDK_NET_MODE_LTE,			//4g网络

	E_SDK_NET_MODE_NUM
}E_SDK_NET_MODE;

typedef struct
{
	E_SDK_NET_MODE mode;
}T_SDK_NET_MODE;

//smartlink 配置结果回调
typedef int (*F_SDK_SMARTLINK_RESULT_CB)(char *ssid, char *passwd ,int result);

//网络连接状态回调 mode:当前模式   state:当前状态
typedef int (*F_SDK_NETWORK_STATE_CB)(int mode ,int state);


typedef struct
{
	int  signal;
	char ssid[64];
}T_SDK_WIFI_INFO;

typedef struct
{
	int  ssid_num;
	T_SDK_WIFI_INFO wifi_list[30]; //最多显示30个
}T_SDK_WIFI_LIST;


typedef struct
{
	char ssid[64];
	char passwd[64];
}T_SDK_SSID_INFO;


typedef enum
{
	E_SDK_NET_TYPE_LAN,
	E_SDK_NET_TYPE_WLAN,
	E_SDK_NET_TYPE_BOTH,
	E_SDK_NET_TYPE_BUTT,
}E_SDK_NET_TYEP;


typedef struct
{
	E_SDK_NET_TYEP type;
}T_SDK_NET_TYPE;


typedef struct
{
	char		 wired_ip[32];	//有线网络的ip地址
	char		 wired_mac[32];
	char		 wired_mask[32];

	char		 wifi_ip[32];	//wifi网络的ip地址
	char		 wifi_mac[32];
	char		 wifi_mask[32];

	char 		 sta_ssid[32];
	char		 sta_passwd[32];

	char         ap_ssid[32];
	char 		 ap_passwd[32];

	E_SDK_NET_MODE  mode;
	E_SDK_NET_STATE state;
}T_SDK_NET_ALL_PARAM;

//////////////////////////////////////////////////////////////////////


typedef struct
{
	int		enable;
	int		refresh_time;
	char	server[64];
	short	port;
}T_SDK_SYS_TIME_NTP_PARAM;

typedef struct
{
	int							zone;
	T_SDK_SYS_TIME_NTP_PARAM	ntp;
}T_SDK_SYS_TIME_PARAM;

typedef struct
{
	int				enable;
}T_SDK_SYS_UART_PARAM;

typedef struct
{
	int		enable;
	int		level;
	int		wt;
	char	file[64];
}T_SDK_SYS_LOG_PARAM;

typedef struct
{
	char			uid[64];
	char			hardware[64];
	char			software[64];
	char			kernel[64];
	char			uboot[64];
	char			mac[64];
	char			modle_name[64];
	char			modle_type[64];
}T_SDK_SYS_DEVICE_PARAM;

typedef struct
{
	unsigned int   c_device_type;			 //设备类型   800中性版  801彩益	 100海尔	200NVR设备	180VR180度	360 360全景摄像机
	unsigned int   un_resolution_0_flag;	//主码流分辨率大小 Width:高16位 Height:低16位  Ming@2016.06.14
	unsigned int   un_resolution_1_flag;	//子码流
	unsigned int   un_resolution_2_flag;	//第3路码流
	unsigned char  c_encrypted_ic_flag; 	//是否有加密IC
	unsigned char  c_pir_flag;				//是否有PIR传感器，0:无，1:有，下同
	unsigned char  c_ptz_flag;				//是否有云台
	unsigned char  c_mic_flag;				//是否有咪头
	unsigned char  c_speaker_flag;			//是否有喇叭
	unsigned char  c_sd_flag;				//是否有SD卡
	unsigned char  c_temperature_flag;		//是否有温感探头
	unsigned char  c_timezone_flag; 		//是否支持同步时区
	unsigned char  c_night_vison_flag;		//是否支持夜视
	unsigned char  c_ethernet_flag; 		//是否带网卡0:wifi 1有线2wifi加有线
	unsigned char  c_smart_connect_flag;	/* 是否支持smart扫描	0	代表不支持，		1、代表7601smart        		2、代表8188smart	3、代表ap6212 9、不支持二维码扫描 10、只支持二维码扫描
												11代表二维码扫描+7601smart 12、代表二维码扫描+8188smart 13、代表二维码扫描+ap6212smart 14、代表AP添加 15、代表AP添加+8188smart*/
	unsigned char  c_motion_detection_flag; //是否支持移动侦测
	unsigned char  c_record_duration_flag;	// 是否有设置录像录像时长
	unsigned char  c_light_flag;			// 是否有设置照明灯开关
	unsigned char  c_audio_alarm_detection_flag; //是否支持声音侦测报警
	unsigned char  c_align1;					// 是否支持摇篮曲 0.不支持 1.5886HAB 2.GD8202KE 3.VOXX系列
	unsigned char  reserver_default_off[32];// 预留能力集默认关闭
	unsigned char  reserver_default_on[32]; // 预留能力集默认开启
}T_SDK_SYS_ABILITY_PARAM;

typedef struct
{
	int		enable;
}T_SDK_SYS_LED_PARAM;

typedef struct
{
	int		mode;		// 0:auto 1:day 2:night //IRCUT_STATE_E
	int		critical;	// day & night 临界值
}T_SDK_SYS_IRCUT_PARAM;

typedef struct
{
	T_SDK_SYS_LED_PARAM		led;
	T_SDK_SYS_DEVICE_PARAM	device;
	T_SDK_SYS_TIME_PARAM	time;
	T_SDK_SYS_UART_PARAM	uart;
	T_SDK_SYS_LOG_PARAM		log;
}T_SDK_SYS_PARAM;

typedef enum
{
	E_SDK_LED_RED_FLICKER,		// 红灯闪烁
	E_SDK_LED_RED_STATIC,			// 红灯常亮
	E_SDK_LED_GREEN_QUICK_FLICKER,// 绿灯快闪
	E_SDK_LED_GREEN_FLICKER,		// 绿灯闪烁
	E_SDK_LED_GREEN_STATIC,		// 绿灯常亮
	E_SDK_LED_RED_GREEN_TURN,		// 红绿交替
}E_SDK_LED_STATE;

typedef struct
{
	E_SDK_LED_STATE		state;
}T_SDK_LED_STATE;

typedef enum
{
	E_SDK_IRCUT_STATE_AUTO,
	E_SDK_IRCUT_STATE_DAY,
	E_SDK_IRCUT_STATE_NIGHT,
}E_SDK_IRCUT_STATE;

typedef struct
{
	E_SDK_IRCUT_STATE		state;
}T_SDK_IRCUT_STATE;

typedef enum
{
	E_SDK_ANN_TURN_ON,
	E_SDK_ANN_START_SET_WIFI,
	E_SDK_ANN_SET_WIFI_TIMEOUT,
	E_SDK_ANN_ROUTER_CONN_SUCC,
	E_SDK_ANN_ROUTER_CONN_FAIL,
	E_SDK_ANN_ROUTER_CONN_WAIT,
	E_SDK_ANN_RMOTE_VIDEO_ALIVE,
	E_SDK_ANN_RMOTE_VIDEO_ABSENT,
	E_SDK_ANN_START_AP,
	E_SDK_ANN_START_AP_FAIL,
	E_SDK_ANN_START_AP_SUCC,
	E_SDK_ANN_QRCODE_OK,
	E_SDK_ANN_QRCODE_START,
	E_SDK_ANN_CLIENT,
	E_SDK_ANN_DINGDONG,
}E_SDK_ANN_TYPE;

typedef struct
{
	E_SDK_ANN_TYPE type;
}T_SDK_ANN;

typedef struct
{
	char server_addr[256];
	int  server_port;
	char device_id[64];
	char soft_version[64];
	char hard_version[64];
	char kernel_version[64];
	char uboot_version[64];
	char squashfs_version[64];
}T_SDK_SYS_UPGRADE_CONF;

typedef struct
{
	int		persent;
}T_SDK_SYS_UPGRADE_PROSS;

typedef struct {
	char chip_type[16];
} T_SDK_CHIP_TYPE;

#if defined (__cplusplus)
}
#endif

#endif


