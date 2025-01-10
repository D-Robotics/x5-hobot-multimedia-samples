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

#ifdef __cplusplus
extern "C" {
#endif
#include "common_utils.h"
#include "hb_media_codec.h"
#include "hb_media_error.h"


int vp_codec_encoder_create_and_start(media_codec_context_t *media_context, camera_config_info_t *camera_config_info);
int vp_codec_encoder_destroy_and_stop(media_codec_context_t *media_context);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_CODEC_H_
