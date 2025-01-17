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
#ifndef VP_GDC_H_
#define VP_GDC_H_
#ifdef __cplusplus
extern "C" {
#endif
#include "vp_common.h"
typedef struct {
    char *sensor_name;
    char *gdc_file_name;
    int is_valid;
} gdc_list_info_t;

int32_t vp_gdc_init(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_gdc_start(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_gdc_stop(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_gdc_deinit(vp_vflow_contex_t *vp_vflow_contex);

int32_t vp_gdc_release_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame);
int32_t vp_gdc_get_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame);

int32_t vp_gdc_send_frame(vp_vflow_contex_t *vp_vflow_contex, hbn_vnode_image_t *image_frame);

const char *vp_gdc_get_bin_file(const char *sensor_name);

int get_gdc_config_file_size(const char *sensor_name);
#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_GDC_H_
