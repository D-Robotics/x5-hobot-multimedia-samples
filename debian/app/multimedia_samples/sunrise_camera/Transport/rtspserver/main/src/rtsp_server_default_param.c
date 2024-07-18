#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>

#include "utils/utils_log.h"
#include "utils/common_utils.h"

#include "rtsp_server_default_param.h"

int rtspserver_param_all_default(rtspserver_info_t* info)
{
	strcpy(info->prefix, "venc0_stream.264");
	info->port = 554;

	info->audio.enable = 0;
	info->audio.type = RTSPSRV_AUDIO_TYPE_PCMA;
	info->audio.bitspersample = 16;
	info->audio.samplerate = 8000;
	info->audio.channels = 1;

	info->video.enable = 1;
	info->video.type = RTSPSRV_VIDEO_TYPE_H264;
	info->video.framerate = 30;

	rtspserver_param_save(info);

	return 0;
}

int rtspserver_param_init(rtspserver_info_t* info)
{
	rtspserver_param_all_default(info);
	return 0;
}

int rtspserver_param_save(rtspserver_info_t* info)
{
	return 0;
}



