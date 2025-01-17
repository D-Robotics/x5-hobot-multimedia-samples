// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

typedef struct{
	int32_t width;
	int32_t height;

	int32_t frame_rate;
	uint32_t bit_rate;
	media_codec_id_t codec_type;

	//ion buffer
	bool input_buffer_is_extrenal;
	int32_t input_buffer_count;
	int32_t output_buffer_count;
}media_codec_user_config_t;

int32_t vp_encode_config_param(media_codec_context_t *context, media_codec_user_config_t *user_config);
// int32_t vp_encode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
// 	int32_t width, int32_t height, int32_t frame_rate, uint32_t bit_rate, bool external_frame_buf);
int32_t vp_decode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
	int32_t width, int32_t height);

int32_t vp_codec_init(media_codec_context_t *context);
int32_t vp_codec_deinit(media_codec_context_t *context);
int32_t vp_codec_start(media_codec_context_t *context);
int32_t vp_codec_stop(media_codec_context_t *context);
int32_t vp_codec_restart(media_codec_context_t *context);

int32_t vp_codec_encoder_set_input(media_codec_context_t *context, ImageFrame *vse_frame);
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