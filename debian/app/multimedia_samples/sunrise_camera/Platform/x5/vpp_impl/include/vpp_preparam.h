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

#ifndef VPP_PREPARAM_H
#define VPP_PREPARAM_H

#include "vp_wrap.h"
#include "bpu_wrap.h"

void* vpp_osd_set_timestamp_thread(void *ptr);
void vpp_get_sub_stream_resolution(const int width, const int height, int *sub_width, int *sub_height);

int32_t alloc_graphic_buffer(hbn_vnode_image_t *img, int w, int h, int32_t format);
void vpp_graphic_buf_to_bpu_buffer_info(const hbn_vnode_image_t *src, bpu_buffer_info_t *dst);
void vpp_video_frame_buffer_info_to_bpu_buffer_info(const mc_video_frame_buffer_info_t *src,
	bpu_buffer_info_t *dst);
void vpp_video_frame_buffer_info_to_vnode_image(const mc_video_frame_buffer_info_t *src,
	hbn_vnode_image_t *dst);

#endif // VPP_PREPARAM_H

