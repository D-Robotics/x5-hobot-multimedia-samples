#ifndef RTSPSERVER_DEFAULT_PARAM_H
#define RTSPSERVER_DEFAULT_PARAM_H

#define RTSPSERVER_CONF_PATH		"/mnt/config/"
#define RTSPSERVER_CONF_FILE		RTSPSERVER_CONF_PATH"rtspserver.json"
#define RTSPSERVER_CONF_DEFAULT  	RTSPSERVER_CONF_PATH"rtspserver.json.default"

typedef enum
{
	RTSPSRV_AUDIO_TYPE_LPCM,
	RTSPSRV_AUDIO_TYPE_PCMA,
}RTSPSRV_AUDIO_TYPE_E;

typedef struct
{
	int 	enable;
	int		type;

	int 	samplerate;
	int		channels;
	int		bitspersample;
}audio_info_t;

typedef enum
{
	RTSPSRV_VIDEO_TYPE_H264,
	RTSPSRV_VIDEO_TYPE_H265,
}RTSPSRV_VIDEO_TYPE_E;

typedef struct
{
	int 	enable;
	int		type;

	int		framerate;
}video_info_t;

typedef struct
{
	char		prefix[128];
	int			port;

	audio_info_t	audio;
	video_info_t	video;
	// 以下是共享内存区域的配置
	char shm_id[32];
	char shm_name[32];
	int stream_buf_size;
	int suggest_buffer_item_count;
	int suggest_buffer_region_size;
}rtspserver_info_t;

#ifdef __cplusplus
extern "C"{
#endif
int rtspserver_param_init(rtspserver_info_t* info);
int rtspserver_param_save(rtspserver_info_t* info);

#ifdef __cplusplus
}
#endif

#endif

