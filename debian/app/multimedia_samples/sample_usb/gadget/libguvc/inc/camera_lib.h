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

/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * camera_lib.h
 *	camera operation function list. (eg. fps, brightness, contrast,
 *	saturation, sharpness and so on...)
 *
 * Copyright (C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */

#ifndef __CAMERA_LIB_H__
#define __CAMERA_LIB_H__

enum cam_wb_mode {
    CAM_WB_AUTO,
    CAM_WB_2500K,
    CAM_WB_4000K,
    CAM_WB_5300K,
    CAM_WB_6800K
};

/* front-end camera component (isp, ipu, venc...) */
typedef struct camera_comp_s {
	const char *name;
} camera_comp_t;

/* camera component api */
int camera_open(camera_comp_t **handle);
void camera_close(camera_comp_t *handle);

/* camera property get/set */
int camera_get_fps(camera_comp_t *handle, int *out_fps);
int camera_set_fps(camera_comp_t *handle, int fps);
int camera_get_brightness(camera_comp_t *handle, int *out_bri);
int camera_set_brightness(camera_comp_t *handle, int bri);
int camera_get_contrast(camera_comp_t *handle, int *out_con);
int camera_set_contrast(camera_comp_t *handle, int con);
int camera_get_saturation(camera_comp_t *handle, int *out_sat);
int camera_set_saturation(camera_comp_t *handle, int sat);
int camera_get_sharpness(camera_comp_t *handle, int *out_sp);
int camera_set_sharpness(camera_comp_t *handle, int sp);
int camera_get_gain(camera_comp_t *handle, int *out_iso);
int camera_set_gain(camera_comp_t *handle, int iso);
int camera_set_wb(camera_comp_t *handle, enum cam_wb_mode mode);
int camera_get_wb(camera_comp_t *handle, enum cam_wb_mode *out_mode);
int camera_get_hue(camera_comp_t *handle, int *out_hue);
int camera_set_hue(camera_comp_t *handle, int hue);
int camera_get_exposure(camera_comp_t *handle, int *out_level);
int camera_set_exposure(camera_comp_t *handle, int level);

/* features on/off */
int camera_monochrome_is_enabled(camera_comp_t *handle);
int camera_monochrome_enable(camera_comp_t *handle, int en);
int camera_ae_is_enabled(camera_comp_t *handle);
int camera_ae_enable(camera_comp_t *handle, int en);
int camera_wdr_is_enabled(camera_comp_t *handle);
int camera_wdr_enable(camera_comp_t *handle, int en);

#endif	/* __CAMERA_LIB_H__ */
