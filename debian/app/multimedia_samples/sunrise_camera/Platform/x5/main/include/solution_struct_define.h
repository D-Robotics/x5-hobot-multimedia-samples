#ifndef SOLUTION_STRUCT_DEFINE_H
#define SOLUTION_STRUCT_DEFINE_H

#include <time.h>

typedef enum
{
	SOLUTION_PARAM_NULL,
	SOLUTION_PARAM_STATE_GET,
	SOLUTION_PARAM_ALL_SET,
	SOLUTION_PARAM_ALL_GET,
	SOLUTION_DEC_DATA_PUSH,					// 发送音频解码数据
	SOLUTION_DEC_DATA_CLEAR,					// 清空音频解码数据
	SOLUTION_DEC_FILE_PUSH,					// 发送音频解码文件
	SOLUTION_MOTION_CALLBACK_REG,			// 注册移动侦测回调
	SOLUTION_MOTION_CALLBACK_UNREG,			// 注销移动侦测回调
	SOLUTION_VENC_FRAMERATE_SET,
	SOLUTION_VENC_BITRATE_SET,
	SOLUTION_VENC_GOP_SET,
	SOLUTION_VENC_CVBR_SET,
	SOLUTION_VENC_FORCEIDR,
	SOLUTION_VENC_MIRROR_SET,
	SOLUTION_VENC_NTSC_PAL_SET,
	SOLUTION_VENC_NTSC_PAL_GET,
	SOLUTION_AENC_PARAM_GET,
	SOLUTION_AENC_VOLUME_SET,
	SOLUTION_AENC_MUTE_SET,
	SOLUTION_AENC_SAMPLERATE_SET,
	SOLUTION_AENC_BITRATE_SET,
	SOLUTION_AENC_channelS_SET,
	SOLUTION_AENC_AEC_SET,
	SOLUTION_AENC_AGC_SET,
	SOLUTION_AENC_ANR_SET,
	SOLUTION_ADEC_VOLUME_SET,
	SOLUTION_ADEC_SAMPLERATE_SET,
	SOLUTION_ADEC_BITRATE_SET,
	SOLUTION_ADEC_channelS_SET,
	SOLUTION_OSD_TIMEENABLE_SET,
	SOLUTION_SNAP_PATH_SET,
	SOLUTION_SNAP_ONCE,
	SOLUTION_MOTION_PARAM_GET,
	SOLUTION_MOTION_PARAM_SET,
	SOLUTION_YUV_BUFFER_START,
	SOLUTION_YUV_BUFFER_STOP,
	SOLUTION_MOTION_SENSITIVITY_SET,
	SOLUTION_MOTION_AREA_SET,
	SOLUTION_GPIO_CTRL_SET,
	SOLUTION_GPIO_CTRL_GET,
	SOLUTION_ADC_CTRL_GET,
	SOLUTION_ISP_CTRL_MODE_SET,
	SOLUTION_RINGBUFFER_CREATE,
	SOLUTION_RINGBUFFER_DATA_GET,
	SOLUTION_RINGBUFFER_DATA_SYNC,
	SOLUTION_RINGBUFFER_DESTORY,
	SOLUTION_VENC_ALL_PARAM_GET, // 获取编码所有通道的参数
	SOLUTION_VENC_CHN_PARAM_GET, // 获取单个编码通道的参数
	SOLUTION_GET_VENC_CHN_STATUS,
	SOLUTION_GET_RAW_FRAME,
	SOLUTION_GET_ISP_FRAME,
	SOLUTION_GET_VSE_FRAME,
	SOLUTION_JPEG_SNAP,
	SOLUTION_START_RECORDER,
	SOLUTION_STOP_RECORDER,
}SOLUTION_PARAM_E;


#define VENC_MAX_channelS	(2)
typedef int (*solution_motion_effect_call)(int type, time_t time, int duration);
typedef enum
{
	SOLUTION_AENC_TYPE_G711A		=	19,
	SOLUTION_AENC_TYPE_AAC		=	37,
	SOLUTION_VENC_TYPE_H264		= 	96,
	SOLUTION_VENC_TYPE_H265		=	265,
	SOLUTION_VENC_TYPE_MJPEG		=	1002,
}SOLUTION_PAYLOAD_TYPE_E;

typedef enum
{
	SOLUTION_VENC_RC_CBR			=	0,
	SOLUTION_VENC_RC_VBR			=	1,
	SOLUTION_VENC_RC_BUTT,
}SOLUTION_RC_TYPE_E;

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
}venc_info_t;

typedef struct{
	int			channels;
	int			framerate;	// sensor vi 帧率
	int			ntsc_pal;	// 0:NTSC 1:PAL
	venc_info_t	infos[VENC_MAX_channelS];
}venc_infos_t;

typedef enum
{
	SOLUTION_AUDIO_SAMPLE_RATE_8000   = 8000,    /* 8K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_12000  = 12000,   /* 12K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_11025  = 11025,   /* 11.025K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_16000  = 16000,   /* 16K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_22050  = 22050,   /* 22.050K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_24000  = 24000,   /* 24K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_32000  = 32000,   /* 32K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_44100  = 44100,   /* 44.1K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_48000  = 48000,   /* 48K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_64000  = 64000,   /* 64K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_96000  = 96000,   /* 96K samplerate*/
	SOLUTION_AUDIO_SAMPLE_RATE_BUTT,
} SOLUTION_AUDIO_SAMPLE_RATE_E;

typedef enum
{
	SOLUTION_AUDIO_SOUND_MODE_MONO   =0,/*mono*/
	SOLUTION_AUDIO_SOUND_MODE_STEREO =1,/*stereo*/
	SOLUTION_AUDIO_SOUND_MODE_BUTT
} SOLUTION_AUDIO_SOUND_MODE_E;

typedef struct{
	int			enable;		// 开关
	int			channel;	// 通道号
	int			type;		// 编码类型
	int			mic_input;	// mic输入类型0: mic 1:line
	int			mic_mute;	// 输入静音

	int			channels;	// 声道数
	int			bitrate;	// 码率
	int			samplerate;	// 采样率
	int			volume;		// 输入音量

	int			aec;		// 回声抵消算法
	int			agc;		// 自动增益算法
	int			anr;		// 语音降噪
}aenc_info_t;

typedef struct{
	int			enable;
	int			channel;
	int			type;		// 编码类型 同aenc

	int			channels;	// 声道数
	int			bitrate;	// 码率
	int			samplerate;	// 采样率
	int			volume;		// 输出音量
}adec_info_t;

typedef struct
{
	int				enable;
	int				layer;			/* OVERLAY region layer range:[0,7]*/

	int				width;			//区域宽
	int				height;			//区域高
	int				point_x;		//point坐标 /* X:[0,4096],align:4,Y:[0,4096],align:4 */
	int				point_y;
	int				alpha_fg;		//前景透明度	range:[0,128]
	int				alpha_bg;		//背景透明度	range:[0,128]

	unsigned int	color_bg;		//背景颜色		PIXEL_FORMAT_RGB_1555
	unsigned short	color_fg;		//前景颜色		PIXEL_FORMAT_RGB_1555
	unsigned short	color_bd;		//边界颜色
	unsigned short	color_dg;		//阴影颜色
	int				font;			//字体大小		//支持 32 24 8
	int				space;			//空格占长		建议14
	//反色处理???
}osd_region_t;

typedef struct{
	int				enable;
	int				vchn;

	osd_region_t	time[VENC_MAX_channelS];
//  notic
//  logo
}osd_info_t;

typedef struct{					//约定 venc的第一路视频参数做抓拍 绑定到venc chn0的vpss chn
	int		enable;
	int		channel;			// 通道号		// venc channel
	int		width;				// 分辨率宽
	int 	height;				// 分辨率高
	char	path[64];			// 存储位置
}snap_info_t;

#define MAX_MD_RECT		16		// 将整体区域分为16个均等的小块，每个小块可以设置是否使能
typedef struct{
	int		x;
	int		y;
	int		w;
	int		h;
}md_rect;

typedef struct{
	int 	enable;
	int 	channel;				// 通道号
	int 	vbblkcnt;				// 海思VB BLOCK COUNT
	int		width;					// 分辨率宽
	int 	height;					// 分辨率高
	float	framerate;
	int		sensitivity;
	int		rect_enable;			// 16个位分别对应16个小块是否使能
	int		sustain;				// 一个移动侦测持续时长
	md_rect	rect[MAX_MD_RECT];
	solution_motion_effect_call	func;	// 发送移动时状态回调
}motion_info_t;

typedef struct
{
	int 		enable;
	int			frequency;				// 频率，默认0
	int			mode;					// 0:普通 1:自动
}ae_antiflicker_t;

typedef struct
{
	ae_antiflicker_t antiflicker;		// 抗闪烁
}ae_info_t;

typedef struct
{
	int		enable;
	int		op_type;					// 0:自动曝光 1:手动曝光
	int		interval;					// AE 算法运行的间隔，取值范围为[1,255], 建议1

	ae_info_t		st_auto;
//	me_info_t		st_manual;
}isp_exposure_info_t;

typedef struct
{
	int		enable;
	char	ini[64];

	isp_exposure_info_t exposure;
}isp_info_t;

typedef struct
{
	isp_info_t		isp_info;
	venc_infos_t	venc_info;
	aenc_info_t		aenc_info;
	adec_info_t		adec_info;
	osd_info_t		osd_info;
	snap_info_t		snap_info;
	motion_info_t	motion_info;
}camera_info_t;

////////////////////////////////////////////////////////////////////
typedef struct
{
	int 	channel;
	int		count;
}solution_forceidr_t;

typedef struct
{
	int		ch;
	int		max_frame;
	int		max_size;
	char	name[64];
}solution_yuv_buffer_t;

typedef struct
{
	int			 	type;  //GPIO_CTRL_NAME
	int				val;
}solution_gpoi_ctrl_t;

typedef struct
{
	int			 	ch;
	int				val;
}solution_adc_ctrl_t;

////////////////////////////////////////////////////////////////////

#endif
