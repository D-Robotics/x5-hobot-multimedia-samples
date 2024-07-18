/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_CODEC_H_
#define VP_CODEC_H_

#include <semaphore.h>

#include "vp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"

#include "hb_media_codec.h"
#include "hb_media_error.h"

#ifdef __cplusplus
}
#endif /* extern "C" */

typedef struct {
	media_codec_context_t *context;
	char stream_path[256];
	int32_t frame_count;
} vp_decode_param_t;

#ifdef __cplusplus
extern "C" {
#endif


int32_t vp_encode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
	int32_t width, int32_t height, int32_t frame_rate, uint32_t bit_rate);
int32_t vp_decode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
	int32_t width, int32_t height);

int32_t vp_codec_init(media_codec_context_t *context);
int32_t vp_codec_deinit(media_codec_context_t *context);
int32_t vp_codec_start(media_codec_context_t *context);
int32_t vp_codec_stop(media_codec_context_t *context);
int32_t vp_codec_restart(media_codec_context_t *context);

int32_t vp_codec_set_input(media_codec_context_t *context, ImageFrame *frame, int32_t eos);
int32_t vp_codec_get_output(media_codec_context_t *context, ImageFrame *frame, int32_t timeout);
int32_t vp_codec_release_output(media_codec_context_t *context, ImageFrame *frame);
void vp_codec_get_user_buffer_param(mc_video_codec_enc_params_t *enc_param, int *buffer_region_size, int *buffer_item_count);
void *vp_decode_work_func(void *param);

void vp_codec_print_media_codec_output_buffer_info(ImageFrame *frame);
#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_CODEC_H_
