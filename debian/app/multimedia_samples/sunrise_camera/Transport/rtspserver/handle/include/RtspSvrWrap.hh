#ifndef RTSP_SVR_WRAP_HH
#define RTSP_SVR_WRAP_HH

#include <utils/stream_manager.h>

void* rtspsvr_wrap_instance();
int rtspsvr_wrap_prepare(void* instance, unsigned short port);
int rtspsvr_wrap_reclaim(void* instance);
int rtspsvr_wrap_start(void* instance);
int rtspsvr_wrap_stop(void* instance);
int rtspsvr_wrap_restart(void* instance);
int rtspsvr_wrap_add_sms(void* instance, const char* streamName,
	int audioEnable, int audioType, int audioSampleRate, int audioBitPerSample,
	int audioChannels, int videoEnable, int videoType, int videoFrameRate,
	char *shmId, char *shmName, int streamBufSize, int frameRate,
	int suggest_buffer_region_size, int suggest_buffer_item_count);
int rtspsvr_wrap_del_sms(void* instance, const char* streamName);
#if 0
int rtspsvr_wrap_h264_data_put(void* instance, frame_info info, unsigned char* data, unsigned int length);
int rtspsvr_wrap_pcma_data_put(void* instance, frame_info info, unsigned char* data, unsigned int length);
#endif

#endif