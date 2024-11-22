/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * camera_lib.c
 *	camera operation function list. (camera_comp_t *handle, eg. fps, brightness, contrast,
 *	saturation, sharpness and so on...)
 *
 * Copyright (camera_comp_t *handle, C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */
#include <stdlib.h>
#include <errno.h>
#include "camera_lib.h"

int camera_open(camera_comp_t **handle)
{
	int ret = 0;

	if (!handle) {
		ret = -EINVAL;
		goto err;
	}

	camera_comp_t *new_handle = calloc(1, sizeof(camera_comp_t));
	if (!new_handle) {
		ret = -ENOMEM;
		goto err;
	}

	new_handle->name = "camera front";

	*handle = new_handle;

err:
	return ret;
}

void camera_close(camera_comp_t *handle)
{
	if (!handle)
		return;

	free(handle);

	return;
}

int camera_get_fps(camera_comp_t *handle, int *out_fps)
{
	return -1;
}

int camera_set_fps(camera_comp_t *handle, int fps)
{
	return -1;
}

int camera_get_brightness(camera_comp_t *handle, int *out_bri)
{
	/* not support yet */
	return -1;
}

int camera_set_brightness(camera_comp_t *handle, int bri)
{
	return -1;
}

int camera_get_contrast(camera_comp_t *handle, int *out_con)
{
	return -1;
}

int camera_set_contrast(camera_comp_t *handle, int con)
{
	return -1;
}

int camera_get_saturation(camera_comp_t *handle, int *out_sat)
{
	return -1;
}

int camera_set_saturation(camera_comp_t *handle, int sat)
{
	return -1;
}

int camera_get_sharpness(camera_comp_t *handle, int *out_sp)
{
	return -1;
}

int camera_set_sharpness(camera_comp_t *handle, int sp)
{
	return -1;
}

int camera_get_gain(camera_comp_t *handle, int *out_iso)
{
	return -1;
}

int camera_set_gain(camera_comp_t *handle, int iso)
{
	return -1;
}

int camera_set_wb(camera_comp_t *handle, enum cam_wb_mode mode)
{
	return -1;
}

int camera_get_wb(camera_comp_t *handle, enum cam_wb_mode *out_mode)
{
	return -1;
}

int camera_get_hue(camera_comp_t *handle, int *out_hue)
{
	return -1;
}

int camera_set_hue(camera_comp_t *handle, int hue)
{
	return -1;
}

int camera_get_exposure(camera_comp_t *handle, int *out_level)
{
	return -1;
}

int camera_set_exposure(camera_comp_t *handle, int level)
{
	return -1;
}

int camera_monochrome_is_enabled(camera_comp_t *handle)
{
	return -1;
}

int camera_monochrome_enable(camera_comp_t *handle, int en)
{
	return -1;
}

int camera_ae_is_enabled(camera_comp_t *handle)
{
	return -1;
}

int camera_ae_enable(camera_comp_t *handle, int en)
{
	return -1;
}

int camera_wdr_is_enabled(camera_comp_t *handle)
{
	return -1;
}

int camera_wdr_enable(camera_comp_t *handle, int en)
{
	return -1;
}
