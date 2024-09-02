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

#ifdef __cplusplus
extern "C" {
#endif
#include "common_utils.h"
#include "hb_media_codec.h"
#include "hb_media_error.h"


int vp_codec_encoder_create_and_start(media_codec_context_t *media_context, camera_config_info_t *camera_config_info);
int vp_codec_encoder_destroy_and_stop(media_codec_context_t *media_context);

int32_t vp_codec_release_output(media_codec_context_t *context, media_codec_buffer_t *frame_buffer);
int32_t vp_codec_get_output(media_codec_context_t *context, media_codec_buffer_t *frame_buffer, media_codec_output_buffer_info_t *buffer_info, int32_t timeout);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_CODEC_H_
