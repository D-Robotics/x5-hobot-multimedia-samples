#ifndef __MEDIA_SERVER_HH__
#define __MEDIA_SERVER_HH
#include <stdint.h>

int media_server_init(char *config_path);
void* media_server_create_media(const char* meida_name, const char* stream_name, const char*codec_type_name);
int media_server_push_video(void *media, const char* data, int length, uint64_t pts, uint64_t dts, const char *codec_name);
void media_server_destroy_media(void *media);
void media_server_uninit();

#endif // !__MEDIA_SERVER_HH