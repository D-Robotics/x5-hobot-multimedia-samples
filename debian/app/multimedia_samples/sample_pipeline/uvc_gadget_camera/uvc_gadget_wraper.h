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

#ifndef ___UVC_GADGET_WRAPER_HH__
#define ___UVC_GADGET_WRAPER_HH__

#include "uvc_gadget_api.h"


typedef struct uvc_gadget_camera_info_s
{
    int frame_width;
    int frame_height;

    struct uvc_prepare_callback prepare_frame_cb;
    struct uvc_release_callback release_frame_cb;
    struct uvc_streamon_callback stream_on_or_off_cb;

}uvc_gadget_camera_info_t;

struct uvc_context *uvc_gadget_create_and_start(uvc_gadget_camera_info_t *uvc_gadget_camera_info);
void uvc_gadget_destroy_and_stop(struct uvc_context *ctx);

#endif //