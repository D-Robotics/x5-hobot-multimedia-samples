/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * uvc_event_handler.cpp
 *	uvc camera terminal, process unit, extension unit event handler
 *
 * Copyright (C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */

#include <stdio.h>
#include "uvc_event_handler.h"
#include "uvc_gadget_api.h"
#include "camera_lib.h"
#include "utils.h"

static struct uvc_control_request_param g_uvc_param;

/*
 * Append more camera terminal control selector. (Linux 5.xx
 * version only support to UVC_CT_PRIVACY_CONTROL(0x11).)
 * But some windows PC host will submit more selector...
 * eg. UVC_CT_REGION_OF_INTEREST_CONTROL
 */
#define UVC_CT_FOCUS_SIMPLE_CONTROL			0x12
#define UVC_CT_WINDOW_CONTROL				0x13
#define UVC_CT_REGION_OF_INTEREST_CONTROL		0x14

struct uvc_cs_ops camera_terminal_cs_ops[] = {
	{UVC_CT_CONTROL_UNDEFINED, 0, 0, 0},
	{UVC_CT_SCANNING_MODE_CONTROL, sizeof(uint8_t) , 0, 0},
	{UVC_CT_AE_MODE_CONTROL, sizeof(union bAutoExposureMode) , 0, 0},

	{UVC_CT_AE_PRIORITY_CONTROL, sizeof(int8_t) , 0, 0},

	{UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL, sizeof(int32_t) , 0, 0},
	{UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL, sizeof(uint8_t) , 0, 0},

	{UVC_CT_FOCUS_ABSOLUTE_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_CT_FOCUS_RELATIVE_CONTROL, sizeof(struct FocusRelativeControl) , 0, 0},
	{UVC_CT_FOCUS_AUTO_CONTROL, sizeof(uint8_t) , 0, 0},
	{UVC_CT_IRIS_ABSOLUTE_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_CT_IRIS_RELATIVE_CONTROL, sizeof(int8_t) , 0, 0},

	{UVC_CT_ZOOM_ABSOLUTE_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_CT_ZOOM_RELATIVE_CONTROL, sizeof(struct ZoomRelativeControl) , 0, 0},

	{UVC_CT_PANTILT_ABSOLUTE_CONTROL, sizeof(struct PanTiltAbsoluteControl) , 0, 0},
	{UVC_CT_PANTILT_RELATIVE_CONTROL, sizeof(struct PanTiltRelativeControl) , 0, 0},
	{UVC_CT_ROLL_ABSOLUTE_CONTROL, sizeof(uint16_t) , 0, 0},
	{UVC_CT_ROLL_RELATIVE_CONTROL, sizeof(struct RollRelativeControl) , 0, 0},
	{UVC_CT_PRIVACY_CONTROL, sizeof(uint8_t) , 0, 0},
	{UVC_CT_FOCUS_SIMPLE_CONTROL, sizeof(uint8_t) , 0, 0},
	{UVC_CT_WINDOW_CONTROL, sizeof(struct DigitalWindowControl) , 0, 0},
	{UVC_CT_REGION_OF_INTEREST_CONTROL, sizeof(struct RegionOfInterestControl) , 0, 0},
};

struct uvc_cs_ops processing_unit_cs_ops[] = {
	{UVC_PU_CONTROL_UNDEFINED, 0, 0, 0},
	{UVC_PU_BACKLIGHT_COMPENSATION_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_BRIGHTNESS_CONTROL, sizeof(uint16_t) , 0, 0},
	{UVC_PU_CONTRAST_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_GAIN_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_POWER_LINE_FREQUENCY_CONTROL, sizeof(int8_t) , 0, 0},

	{UVC_PU_HUE_CONTROL, sizeof(uint16_t) , 0, 0},

	{UVC_PU_SATURATION_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_SHARPNESS_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_GAMMA_CONTROL, sizeof(int16_t) , 0, 0},

	{UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL, sizeof(int8_t) , 0, 0},
	{UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL, sizeof(struct WhiteBalanceComponentControl) , 0, 0},
	{UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL, sizeof(int8_t) , 0, 0},

	{UVC_PU_DIGITAL_MULTIPLIER_CONTROL, sizeof(int16_t) , 0, 0},
	{UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL, sizeof(int16_t) , 0, 0},

	{UVC_PU_HUE_AUTO_CONTROL, sizeof(int8_t) , 0, 0},
	{UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL, sizeof(int8_t) , 0, 0},
	{UVC_PU_ANALOG_LOCK_STATUS_CONTROL, sizeof(int8_t) , 0, 0},
};

#define UVC_CT_CS_OPS_NUM (sizeof(camera_terminal_cs_ops)/sizeof(camera_terminal_cs_ops[0]))
#define UVC_PU_CS_OPS_NUM (sizeof(processing_unit_cs_ops)/sizeof(processing_unit_cs_ops[0]))

static int uvc_cs_ops_register(int id, int cs,
	uvc_setup_callback setup_f, uvc_data_callback data_f, int data_len)
{
	if (!setup_f || !data_f)
		goto err;

	switch (id) {
	case UVC_CTRL_CAMERA_TERMINAL_ID:
		if (cs > UVC_CT_CS_OPS_NUM - 1) {
			goto err;
		} else {
			camera_terminal_cs_ops[cs].setup_phase_f = setup_f;
			camera_terminal_cs_ops[cs].data_phase_f = data_f;
		}
		break;
	case UVC_CTRL_PROCESSING_UNIT_ID:
		if (cs > UVC_PU_CS_OPS_NUM - 1) {
			goto err;
		} else {
			processing_unit_cs_ops[cs].setup_phase_f = setup_f;
			processing_unit_cs_ops[cs].data_phase_f = data_f;
		}
		break;
	case UVC_CTRL_EXTENSION_UNIT_ID:
		UVC_PRINTF(UVC_LOG_ERR, "extension unit not support yet\n");
		goto err;
		break;

	default:
		UVC_PRINTF(UVC_LOG_ERR, "%s failed! id(%d)\n",
				__func__, id);
		goto err;
	}

	return 0;
err:
	return -1;
}

static void* uvc_get_cs_ops(uint8_t cs, uint8_t id)
{
	void *ops = NULL;

	switch (id) {
	case UVC_CTRL_CAMERA_TERMINAL_ID:
		if (cs < UVC_CT_CS_OPS_NUM)
			ops = &camera_terminal_cs_ops[cs];
		break;
	case UVC_CTRL_PROCESSING_UNIT_ID:
		if (cs < UVC_PU_CS_OPS_NUM)
			ops = &processing_unit_cs_ops[cs];
		break;
	default:
		break;
	}

	return ops;
}

/*----------------------------------------------------------------------------*/
/* control request register */
static void uvc_ct_ae_mode_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	union uvc_camera_terminal_control_u* ctrl;

	UVC_PRINTF(UVC_LOG_TRACE, "# %s (req %02x)\n", __func__, req);

	ctrl = (union uvc_camera_terminal_control_u*)&resp->data;
	resp->length = sizeof(ctrl->AEMode.d8);

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		/* Current setting attribute */
		/* 0: auto
		 * 1: manual
		 * 2: shutter_priority
		 * 3: aperture_priority
		 * */
		ctrl->AEMode.d8 = g_uvc_param.ct_ctrl_cur.AEMode.d8;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->AEMode.d8);
		/*TODO : get current value form isp or camera*/
		break;

	case UVC_GET_MIN:
		/* Minimum setting attribute */
		ctrl->AEMode.d8 = g_uvc_param.ct_ctrl_min.AEMode.d8;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->AEMode.d8);
		break;

	case UVC_GET_MAX:
		/* Maximum setting attribute */
		ctrl->AEMode.d8 = g_uvc_param.ct_ctrl_max.AEMode.d8;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->AEMode.d8);
		break;

	case UVC_GET_DEF:
		/* Default setting attribute */
		ctrl->AEMode.d8 = g_uvc_param.ct_ctrl_def.AEMode.d8;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->AEMode.d8);
		break;

	case UVC_GET_RES:
		/* Resolution attribute */
		ctrl->AEMode.d8 = g_uvc_param.ct_ctrl_res.AEMode.d8;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->AEMode.d8);
		break;

	case UVC_GET_INFO:
		/*
		 * TODO: We support Set and Get requests, but
		 * don't support async updates on an video
		 * status (interrupt) endpoint as of
		 * now.
		 *
		 *  Bit Field |    Description                    | Bit State
		 * -------------------------------------------------------------
		 *  D0        | 1=Support GET value requests      | Capability
		 *  D1        | 1=Support SET value requests      | Capability
		 *  D2        | 1=Disable due to automatic        | State
		 *            |   mode(under device control)      |
		 *  D3        | 1 = Autoupdate Control            | Capability
		 *  D4        | 1 = Asynchronous Control          | Capability
		 *  D5        | 1 = Disable due to incompatibility| State
		 *            |  with Commit state.               |
		 *  D7..D6    | Reserved (Set to 0)               | Capability
		 */
		resp->data[0] = 0x3;
		resp->length = 1;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_ct_ae_mode_data(struct uvc_context *ctx,
			struct uvc_request_data *d, uint8_t cs,
			void *userdata)
{
	int ret;
	uint8_t ae_mode, mode;
	union uvc_camera_terminal_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_camera_terminal_control_u*)&d->data;
	if (d->length != sizeof(ctrl->AEMode.d8)) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	ae_mode = ctrl->AEMode.d8;
	UVC_PRINTF(UVC_LOG_ERR, "### %s [CT][AE-Mode]: %d\n", __func__,
		ae_mode);

	if (ae_mode == UVC_AUTO_EXPOSURE_MODE_AUTO ||
		ae_mode == UVC_AUTO_EXPOSURE_MODE_APERTURE_PRIORITY)
		mode = 1;
	else
		mode = 0;

	ret = camera_ae_enable(cam, mode);
	if (ret < 0)
		UVC_PRINTF(UVC_LOG_ERR, "#camera_ae_enable failed! ae mode: %d\n", mode);
	else {
		UVC_PRINTF(UVC_LOG_ERR, "#camera_ae_enable successful! ae mode: %d\n", mode);
		g_uvc_param.ct_ctrl_cur.AEMode.d8 = ae_mode;
	}
}

void uvc_ct_exposure_abs_setup(struct uvc_context *ctx,
			struct uvc_request_data *resp,
			uint8_t req, uint8_t cs,
			void *userdata)
{
	int exposure = 0;
	union uvc_camera_terminal_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	UVC_PRINTF(UVC_LOG_TRACE, "# %s (req %02x)\n", __func__, req);

	ctrl = (union uvc_camera_terminal_control_u*)&resp->data;
	resp->length = sizeof(ctrl->dwExposureTimeAbsolute);

	memset(ctrl, 0, sizeof(*ctrl));

	switch (req) {
	case UVC_GET_CUR:
		/*TODO : get current value form isp or camera*/
		if(camera_get_exposure(cam, &exposure)) {
			UVC_PRINTF(UVC_LOG_TRACE, "camera_get_exposure failed\n");
				break;
		} else {
			g_uvc_param.ct_ctrl_cur.dwExposureTimeAbsolute = exposure;
			ctrl->dwExposureTimeAbsolute =
				g_uvc_param.ct_ctrl_cur.dwExposureTimeAbsolute;
		}
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->dwExposureTimeAbsolute);
		break;

	case UVC_GET_MIN:
		ctrl->dwExposureTimeAbsolute =
			g_uvc_param.ct_ctrl_min.dwExposureTimeAbsolute;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->dwExposureTimeAbsolute);
		break;

	case UVC_GET_MAX:
		ctrl->dwExposureTimeAbsolute =
			g_uvc_param.ct_ctrl_max.dwExposureTimeAbsolute;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->dwExposureTimeAbsolute);
		break;

	case UVC_GET_DEF:
		ctrl->dwExposureTimeAbsolute =
			g_uvc_param.ct_ctrl_def.dwExposureTimeAbsolute;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->dwExposureTimeAbsolute);
		break;

	case UVC_GET_RES:
		ctrl->dwExposureTimeAbsolute =
			g_uvc_param.ct_ctrl_res.dwExposureTimeAbsolute;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->dwExposureTimeAbsolute);
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0x3;
		resp->length = 1;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_ct_exposure_abs_data(struct uvc_context *ctx,
			struct uvc_request_data *d, uint8_t cs,
			void *userdata)
{
	int exposure = 0;
	union uvc_camera_terminal_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_camera_terminal_control_u*)&d->data;
	if (d->length != sizeof(ctrl->dwExposureTimeAbsolute)) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	exposure = ctrl->dwExposureTimeAbsolute - 5;
	if(exposure >= -4 && exposure <= 4) {
		int ret = camera_set_exposure(cam, exposure);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_exposure failed! exposure mode: %d\n", exposure);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_exposure successful! exposure mode: %d\n", exposure);
			g_uvc_param.ct_ctrl_cur.dwExposureTimeAbsolute = ctrl->dwExposureTimeAbsolute;
		}
	} else {
		UVC_PRINTF(UVC_LOG_TRACE, "exposure is out of range: %d\n", exposure);
	}
}

void uvc_pu_brightness_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int brightness = 0;
	union uvc_processing_unit_control_u *ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	UVC_PRINTF(UVC_LOG_TRACE, "# %s (req %02x)\n", __func__, req);

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof(ctrl->wBrightness);

	memset(ctrl, 0, sizeof(*ctrl));;

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_brightness(cam, &brightness);
		if (ret < 0)
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_brightness failed \n",
				__func__);

		if (!ret && brightness >=  0 &&  brightness < 255) {
			g_uvc_param.pu_ctrl_cur.wBrightness = brightness;
		}
		ctrl->wBrightness =
			g_uvc_param.pu_ctrl_cur.wBrightness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wBrightness);
		break;

	case UVC_GET_MIN:
		ctrl->wBrightness =
			g_uvc_param.pu_ctrl_min.wBrightness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wBrightness);
		break;

	case UVC_GET_MAX:
		ctrl->wBrightness =
			g_uvc_param.pu_ctrl_max.wBrightness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wBrightness);
		break;

	case UVC_GET_DEF:
		ctrl->wBrightness =
			g_uvc_param.pu_ctrl_def.wBrightness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wBrightness);
		break;

	case UVC_GET_RES:
		ctrl->wBrightness =
			g_uvc_param.pu_ctrl_res.wBrightness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wBrightness);
		break;

	case UVC_GET_INFO:
		/*
		 * TODO: We support Set and Get requests, but
		 * don't support async updates on an video
		 * status (interrupt) endpoint as of
		 * now.
		 *
		 *  Bit Field |    Description                    | Bit State
		 * -------------------------------------------------------------
		 *  D0        | 1=Support GET value requests      | Capability
		 *  D1        | 1=Support SET value requests      | Capability
		 *  D2        | 1=Disable due to automatic        | State
		 *            |   mode(under device control)      |
		 *  D3        | 1 = Autoupdate Control            | Capability
		 *  D4        | 1 = Asynchronous Control          | Capability
		 *  D5        | 1 = Disable due to incompatibility| State
		 *            |  with Commit state.               |
		 *  D7..D6    | Reserved (Set to 0)               | Capability
		 */
		resp->data[0] = 0x3;
		resp->length = 1;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_brightness_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int brightness;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof(ctrl->wBrightness))  {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	brightness = ctrl->wBrightness;
	UVC_PRINTF(UVC_LOG_INFO, "### %s [PU][wBrightness]: %d\n", __func__,
		brightness);

	if (brightness >= 0 && brightness < 256) {
		ret = camera_set_brightness(cam, brightness);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_brightness failed! "
				"brightness: %d\n", brightness);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_brightness successful! "
					"brightness: %d\n", brightness);
			g_uvc_param.pu_ctrl_cur.wBrightness = brightness;
		}
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "### %s [PU][wBrightness]: %d\n", __func__,
			brightness);
	}
}

void uvc_pu_contrast_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int contrast = 0;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->wContrast;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_contrast(cam, &contrast);
		if (ret < 0)
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_contrast failed\n",
				__func__);

		if (contrast >= 0 && contrast < 255) {
			g_uvc_param.pu_ctrl_cur.wContrast = contrast;
		}
		ctrl->wContrast =
			g_uvc_param.pu_ctrl_cur.wContrast;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wContrast);
		break;

	case UVC_GET_MIN:
		ctrl->wContrast =
			g_uvc_param.pu_ctrl_min.wContrast;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wContrast);
		break;

	case UVC_GET_MAX:
		ctrl->wContrast =
			g_uvc_param.pu_ctrl_max.wContrast;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wContrast);
		break;

	case UVC_GET_DEF:
		ctrl->wContrast =
			g_uvc_param.pu_ctrl_def.wContrast;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wContrast);
		break;

	case UVC_GET_RES:
		ctrl->wContrast =
			g_uvc_param.pu_ctrl_res.wContrast;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wContrast);
		break;

	case UVC_GET_INFO:
		resp->data[0] = cs;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_contrast_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int contrast;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->wContrast) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	contrast = ctrl->wContrast;
	UVC_PRINTF(UVC_LOG_INFO, "### %s [PU][wContrast]: %d\n", __func__,
		contrast);

	if (contrast >= 0 && contrast < 256) {
		ret = camera_set_contrast(cam, contrast);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_contrast failed! "
				"contrast: %d\n", contrast);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_contrast successful! "
				"contrast: %d\n", contrast);
			g_uvc_param.pu_ctrl_cur.wContrast = contrast;
		}
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "### %s [PU][wContrast]: %d\n", __func__,
			contrast);
	}
}

void uvc_pu_gain_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int gain = 0;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->wGain;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_gain(cam, &gain);
		if (ret < 0)
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_gain failed \n",
				__func__);

		if (gain >= 0 && gain < 255) {
			g_uvc_param.pu_ctrl_cur.wGain = gain;
		}
		ctrl->wGain =
			g_uvc_param.pu_ctrl_cur.wGain;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wGain);
		break;

	case UVC_GET_MIN:
		ctrl->wGain =
			g_uvc_param.pu_ctrl_min.wGain;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wGain);
		break;

	case UVC_GET_MAX:
		ctrl->wGain =
			g_uvc_param.pu_ctrl_max.wGain;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wGain);
		break;

	case UVC_GET_DEF:
		ctrl->wGain =
			g_uvc_param.pu_ctrl_def.wGain;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wGain);
		break;

	case UVC_GET_RES:
		ctrl->wGain =
			g_uvc_param.pu_ctrl_res.wGain;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wGain);
		break;

	case UVC_GET_INFO:
		resp->data[0] = cs;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_gain_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int gain;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->wGain) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	gain = ctrl->wGain;
	UVC_PRINTF(UVC_LOG_INFO, "### %s [PU][wGain]: %d\n", __func__,
		gain);

	if (gain >= 0 && gain < 256) {
		ret = camera_set_gain(cam, gain);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_ISOLimit failed! "
				"gain: %d\n", gain);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_ISOLimit successful! "
				"gain: %d\n", gain);
			g_uvc_param.pu_ctrl_cur.wGain = gain;
		}
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "### %s [PU][wGain]: %d\n", __func__,
			gain);
	}
}

void uvc_pu_wb_temperature_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	enum cam_wb_mode wb = CAM_WB_AUTO;

	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->wWhiteBalanceTemperature;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_wb(cam, &wb);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_wb failed \n",
				__func__);
		} else {
			if (wb > 0 && wb < 5) {
				g_uvc_param.pu_ctrl_cur.wWhiteBalanceTemperature = wb;
			}
		}

		ctrl->wWhiteBalanceTemperature  =
			g_uvc_param.pu_ctrl_cur.wWhiteBalanceTemperature;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wWhiteBalanceTemperature);
		break;

	case UVC_GET_MIN:
		ctrl->wWhiteBalanceTemperature  =
			g_uvc_param.pu_ctrl_min.wWhiteBalanceTemperature;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wWhiteBalanceTemperature);
		break;

	case UVC_GET_MAX:
		ctrl->wWhiteBalanceTemperature  =
			g_uvc_param.pu_ctrl_max.wWhiteBalanceTemperature;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wWhiteBalanceTemperature);
		break;

	case UVC_GET_DEF:
		ctrl->wWhiteBalanceTemperature  =
			g_uvc_param.pu_ctrl_def.wWhiteBalanceTemperature;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wWhiteBalanceTemperature);
		break;

	case UVC_GET_RES:
		ctrl->wWhiteBalanceTemperature  =
			g_uvc_param.pu_ctrl_res.wWhiteBalanceTemperature;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wWhiteBalanceTemperature);
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0xff;
		resp->length = 1;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_wb_temperature_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int wb;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->wWhiteBalanceTemperature) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	wb = ctrl->wWhiteBalanceTemperature;
	UVC_PRINTF(UVC_LOG_TRACE, "wb: %d\n", wb);

	ret = camera_set_wb(cam, wb);
	if (ret < 0) {
		UVC_PRINTF(UVC_LOG_ERR, "#camera_set_wb failed! "
			"wb: %d\n", wb);
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "#camera_set_wb successful! "
			"wb: %d\n", wb);
		g_uvc_param.pu_ctrl_cur.wWhiteBalanceTemperature = wb;
	}
}

void uvc_pu_wb_temperature_auto_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	enum cam_wb_mode wb = CAM_WB_AUTO;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->bWhiteBalanceTemperatureAuto;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_wb(cam, &wb);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_wb failed\n",
				__func__);
		} else {
			if (wb == 0)
				g_uvc_param.pu_ctrl_cur.bWhiteBalanceTemperatureAuto = 1;
			else
				g_uvc_param.pu_ctrl_cur.bWhiteBalanceTemperatureAuto = 0;
		}

		ctrl->bWhiteBalanceTemperatureAuto  =
			g_uvc_param.pu_ctrl_cur.bWhiteBalanceTemperatureAuto;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->bWhiteBalanceTemperatureAuto);
		break;

	case UVC_GET_MIN:
		ctrl->bWhiteBalanceTemperatureAuto  =
			g_uvc_param.pu_ctrl_min.bWhiteBalanceTemperatureAuto;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->bWhiteBalanceTemperatureAuto);
		break;

	case UVC_GET_MAX:
		ctrl->bWhiteBalanceTemperatureAuto  =
			g_uvc_param.pu_ctrl_max.bWhiteBalanceTemperatureAuto;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->bWhiteBalanceTemperatureAuto);
		break;

	case UVC_GET_DEF:
		ctrl->bWhiteBalanceTemperatureAuto  =
			g_uvc_param.pu_ctrl_def.bWhiteBalanceTemperatureAuto;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->bWhiteBalanceTemperatureAuto);
		break;

	case UVC_GET_RES:
		ctrl->bWhiteBalanceTemperatureAuto  =
			g_uvc_param.pu_ctrl_res.bWhiteBalanceTemperatureAuto;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->bWhiteBalanceTemperatureAuto);
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0xff;
		resp->length = 1;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_wb_temperature_auto_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int wb;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->bWhiteBalanceTemperatureAuto) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}
	/* 1: auto , 0: manual */
	wb = ctrl->bWhiteBalanceTemperatureAuto;
	UVC_PRINTF(UVC_LOG_TRACE, "wb auto: %d\n", wb);
	if(wb) {
		/* set 1 means auto */
		ret = camera_set_wb(cam, 1);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_wb failed! "
					"wb: %d\n", wb);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_wb successful! "
					"wb: %d\n", wb);
			g_uvc_param.pu_ctrl_cur.bWhiteBalanceTemperatureAuto = wb;
		}
	} else {
		g_uvc_param.pu_ctrl_cur.bWhiteBalanceTemperatureAuto = wb;
	}
}

void uvc_pu_hue_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int hue;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->wHue;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_hue(cam, &hue);
		if (ret < 0)
			UVC_PRINTF(UVC_LOG_INFO, "# camera_get_hue failed \n");
		else
			g_uvc_param.pu_ctrl_cur.wHue = hue;

		ctrl->wHue =
			g_uvc_param.pu_ctrl_cur.wHue;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wHue);
		break;

	case UVC_GET_MIN:
		ctrl->wHue =
			g_uvc_param.pu_ctrl_min.wHue;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wHue);
		break;

	case UVC_GET_MAX:
		ctrl->wHue =
			g_uvc_param.pu_ctrl_max.wHue;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wHue);
		break;

	case UVC_GET_DEF:
		ctrl->wHue =
			g_uvc_param.pu_ctrl_def.wHue;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wHue);
		break;

	case UVC_GET_RES:
		ctrl->wHue =
			g_uvc_param.pu_ctrl_res.wHue;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wHue);
		break;

	case UVC_GET_INFO:
		resp->data[0] = cs;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_hue_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int hue;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->wHue) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	hue = ctrl->wHue;
	UVC_PRINTF(UVC_LOG_INFO, "### %s [PU][wHue]: %d\n", __func__,
		hue);

	if (hue >= 0 && hue < 256) {
		ret = camera_set_hue(cam, hue);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_hue failed! hue: %d\n", hue);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_hue successful! hue: %d\n", hue);
			g_uvc_param.pu_ctrl_cur.wHue = hue;
		}
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "### %s [PU][wHue]: %d\n", __func__,
			hue);
	}
}

void uvc_pu_saturation_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int saturation = 0;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->wSaturation;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_saturation(cam, &saturation);
		if (ret < 0)
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_saturation failed \n",
				__func__);

		if (saturation >= 0 && saturation < 255) {
			g_uvc_param.pu_ctrl_cur.wSaturation = saturation;
		}
		ctrl->wSaturation =
			g_uvc_param.pu_ctrl_cur.wSaturation;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wSaturation);
		break;

	case UVC_GET_MIN:
		ctrl->wSaturation =
			g_uvc_param.pu_ctrl_min.wSaturation;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wSaturation);
		break;

	case UVC_GET_MAX:
		ctrl->wSaturation =
			g_uvc_param.pu_ctrl_max.wSaturation;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wSaturation);
		break;

	case UVC_GET_DEF:
		ctrl->wSaturation =
			g_uvc_param.pu_ctrl_def.wSaturation;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wSaturation);
		break;

	case UVC_GET_RES:
		ctrl->wSaturation =
			g_uvc_param.pu_ctrl_res.wSaturation;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wSaturation);
		break;

	case UVC_GET_INFO:
		resp->data[0] = cs;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_saturation_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int saturation;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->wSaturation) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	saturation = ctrl->wSaturation;
	UVC_PRINTF(UVC_LOG_INFO, "### %s [PU][wSaturation]: %d\n", __func__,
		saturation);

	if (saturation > 0 && saturation < 256) {
		ret = camera_set_saturation(cam, saturation);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_saturation failed!"
				"saturation: %d\n", saturation);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_saturation successful!"
				"saturation: %d\n", saturation);
			g_uvc_param.pu_ctrl_cur.wSaturation = saturation;
		}
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "### %s [PU][wSaturation]: %d\n", __func__,
			saturation);
	}
}

void uvc_pu_sharpness_setup(struct uvc_context *ctx,
			   struct uvc_request_data *resp,
			   uint8_t req, uint8_t cs,
			   void *userdata)
{
	int sharpness = 0;
	int ret = 0;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&resp->data;
	resp->length = sizeof ctrl->wSharpness;

	memset(ctrl, 0, sizeof *ctrl);

	switch (req) {
	case UVC_GET_CUR:
		ret = camera_get_sharpness(cam, &sharpness);
		if (ret < 0)
			UVC_PRINTF(UVC_LOG_ERR, "# %s camera_get_sharpness failed\n",
			__func__);

		if (sharpness  >= 0 && sharpness < 255) {
			g_uvc_param.pu_ctrl_cur.wSharpness = sharpness;
		}
		ctrl->wSharpness =
					g_uvc_param.pu_ctrl_cur.wSharpness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_CUR val = %d\n",
			__func__, ctrl->wSharpness);
		break;

	case UVC_GET_MIN:
		ctrl->wSharpness =
			g_uvc_param.pu_ctrl_min.wSharpness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MIN val = %d\n",
			__func__, ctrl->wSharpness);
		break;

	case UVC_GET_MAX:
		ctrl->wSharpness =
			g_uvc_param.pu_ctrl_max.wSharpness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_MAX val = %d\n",
			__func__, ctrl->wSharpness);
		break;

	case UVC_GET_DEF:
		ctrl->wSharpness =
			g_uvc_param.pu_ctrl_def.wSharpness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_DEF val = %d\n",
			__func__, ctrl->wSharpness);
		break;

	case UVC_GET_RES:
		ctrl->wSharpness =
			g_uvc_param.pu_ctrl_res.wSharpness;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_RES val = %d\n",
			__func__, ctrl->wSharpness);
		break;

	case UVC_GET_INFO:
		resp->data[0] = cs;
		UVC_PRINTF(UVC_LOG_INFO, "# %s UVC_GET_INFO val = %d\n",
			__func__, cs);
		break;
	}
}

void uvc_pu_sharpness_data(struct uvc_context *ctx,
			   struct uvc_request_data *d, uint8_t cs,
			   void *userdata)
{
	int ret = 0;
	int sharpness;
	union uvc_processing_unit_control_u* ctrl;
	camera_comp_t *cam = (camera_comp_t *)userdata;

	ctrl = (union uvc_processing_unit_control_u*)&d->data;
	if (d->length != sizeof ctrl->wSharpness) {
		UVC_PRINTF(UVC_LOG_ERR, "!!! %s return a no vaild data, req len = %d\n",
			__func__, d->length);
		return;
	}

	sharpness = ctrl->wSharpness;
	UVC_PRINTF(UVC_LOG_INFO, "### %s [PU][wSharpness]: %d\n", __func__,
		sharpness);

	if (sharpness >= 0 && sharpness < 256) {
		ret = camera_set_sharpness(cam, sharpness);
		if (ret < 0) {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_sharpness failed! "
				"sharpness: %d\n", sharpness);
		} else {
			UVC_PRINTF(UVC_LOG_ERR, "#camera_set_sharpness successful! "
				"sharpness: %d\n", sharpness);
			g_uvc_param.pu_ctrl_cur.wSharpness = sharpness;
		}
	} else {
		UVC_PRINTF(UVC_LOG_ERR, "### %s [PU][wSharpness] is novalid: %d\n", __func__,
			sharpness);
	}
}

void uvc_control_request_callback_init()
{
	uvc_cs_ops_register(UVC_CTRL_CAMERA_TERMINAL_ID,
		UVC_CT_AE_MODE_CONTROL,
		uvc_ct_ae_mode_setup,
		uvc_ct_ae_mode_data, 0);

	uvc_cs_ops_register(UVC_CTRL_CAMERA_TERMINAL_ID,
		UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
		uvc_ct_exposure_abs_setup,
		uvc_ct_exposure_abs_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_BRIGHTNESS_CONTROL,
		uvc_pu_brightness_setup,
		uvc_pu_brightness_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_CONTRAST_CONTROL,
		uvc_pu_contrast_setup,
		uvc_pu_contrast_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_GAIN_CONTROL,
		uvc_pu_gain_setup,
		uvc_pu_gain_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
		uvc_pu_wb_temperature_setup,
		uvc_pu_wb_temperature_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
		uvc_pu_wb_temperature_auto_setup,
		uvc_pu_wb_temperature_auto_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_HUE_CONTROL,
		uvc_pu_hue_setup,
		uvc_pu_hue_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_SATURATION_CONTROL,
		uvc_pu_saturation_setup,
		uvc_pu_saturation_data, 0);

	uvc_cs_ops_register(UVC_CTRL_PROCESSING_UNIT_ID,
		UVC_PU_SHARPNESS_CONTROL,
		uvc_pu_sharpness_setup,
		uvc_pu_sharpness_data, 0);
}

void uvc_control_request_param_init()
{
	memset(&g_uvc_param, 0, sizeof g_uvc_param);

	/* Camera terminal  */
	/*1. ae mode*/
	/* Current setting attribute */
	/* 0: manual
	 * 1: auto
	 * 2: shutter_priority
	 * 3: aperture_priority
	 * res is the bitmask of control abitlity
	 * 1: manual
	 * 2: auto
	 * 4: shutter_priority
	 * 8: aperture_priority
	 * */
	g_uvc_param.ct_ctrl_def.AEMode.d8 = 2;
	g_uvc_param.ct_ctrl_cur.AEMode.d8 = 2;
	g_uvc_param.ct_ctrl_res.AEMode.d8 = 0x2; /* enable auto */
	g_uvc_param.ct_ctrl_min.AEMode.d8 = 0;
	g_uvc_param.ct_ctrl_max.AEMode.d8 = 3;

	/*2. Exposure, auto priority*/
	g_uvc_param.ct_ctrl_def.bAutoExposurePriority = 0;
	g_uvc_param.ct_ctrl_cur.bAutoExposurePriority = 0;
	g_uvc_param.ct_ctrl_res.bAutoExposurePriority = 1; /* enable auto */
	g_uvc_param.ct_ctrl_min.bAutoExposurePriority = 0;
	g_uvc_param.ct_ctrl_max.bAutoExposurePriority = 1;

	/*3. Exposure tims abs*/
	/* The setting for the attribute of the addressed Exposure Time
	 * (Absolute)Control
	 * 0: Reserved
	 * 1: 0.0001 sec
	 * ...
	 * 100000: 10 sec
	 *
	 * exposure time: 10ms ~ 2000ms. default value 100ms
	 * */
	g_uvc_param.ct_ctrl_def.dwExposureTimeAbsolute  = 100;
	g_uvc_param.ct_ctrl_cur.dwExposureTimeAbsolute  = 100;
	g_uvc_param.ct_ctrl_res.dwExposureTimeAbsolute  = 10;
	g_uvc_param.ct_ctrl_min.dwExposureTimeAbsolute  = 10;
	g_uvc_param.ct_ctrl_max.dwExposureTimeAbsolute  = 2000;

	/*4.Focus(absolute) control*/
	g_uvc_param.ct_ctrl_def.wFocusAbsolute = 50;
	g_uvc_param.ct_ctrl_cur.wFocusAbsolute = 50;
	g_uvc_param.ct_ctrl_res.wFocusAbsolute = 1;
	g_uvc_param.ct_ctrl_min.wFocusAbsolute = 0;
	g_uvc_param.ct_ctrl_max.wFocusAbsolute = 65535;

	/*5.Iris(absolute) control*/
	g_uvc_param.ct_ctrl_def.wIrisAbsolute = 50;
	g_uvc_param.ct_ctrl_cur.wIrisAbsolute = 50;
	g_uvc_param.ct_ctrl_res.wIrisAbsolute = 1;
	g_uvc_param.ct_ctrl_min.wIrisAbsolute = 0;
	g_uvc_param.ct_ctrl_max.wIrisAbsolute = 65535;

	/*6.Zoom(absolute) control*/
	g_uvc_param.ct_ctrl_def.wObjectiveFocalLength = 50;
	g_uvc_param.ct_ctrl_cur.wObjectiveFocalLength = 50;
	g_uvc_param.ct_ctrl_res.wObjectiveFocalLength = 1;
	g_uvc_param.ct_ctrl_min.wObjectiveFocalLength = 0;
	g_uvc_param.ct_ctrl_max.wObjectiveFocalLength = 65535;

	/*7.Pan(absolute) control*/
	g_uvc_param.ct_ctrl_def.PanTiltAbsolute.dwPanAbsolute = 50;
	g_uvc_param.ct_ctrl_cur.PanTiltAbsolute.dwPanAbsolute = 50;
	g_uvc_param.ct_ctrl_res.PanTiltAbsolute.dwPanAbsolute = 1;
	g_uvc_param.ct_ctrl_min.PanTiltAbsolute.dwPanAbsolute = 0;
	g_uvc_param.ct_ctrl_max.PanTiltAbsolute.dwPanAbsolute = 65535;

	/*8.Tilt(absolute) control*/
	g_uvc_param.ct_ctrl_def.PanTiltAbsolute.dwTiltAbsolute = 50;
	g_uvc_param.ct_ctrl_cur.PanTiltAbsolute.dwTiltAbsolute = 50;
	g_uvc_param.ct_ctrl_res.PanTiltAbsolute.dwTiltAbsolute = 1;
	g_uvc_param.ct_ctrl_min.PanTiltAbsolute.dwTiltAbsolute = 0;
	g_uvc_param.ct_ctrl_max.PanTiltAbsolute.dwTiltAbsolute = 65535;

	/*8.Pan(Speed) control*/
	g_uvc_param.ct_ctrl_def.PanTiltRelative.bPanSpeed = 50;
	g_uvc_param.ct_ctrl_cur.PanTiltRelative.bPanSpeed = 50;
	g_uvc_param.ct_ctrl_res.PanTiltRelative.bPanSpeed = 1;
	g_uvc_param.ct_ctrl_min.PanTiltRelative.bPanSpeed = 0;
	g_uvc_param.ct_ctrl_max.PanTiltRelative.bPanSpeed = 255;

	/*9.Tilt(Speed) control*/
	g_uvc_param.ct_ctrl_def.PanTiltRelative.bTiltSpeed = 50;
	g_uvc_param.ct_ctrl_cur.PanTiltRelative.bTiltSpeed = 50;
	g_uvc_param.ct_ctrl_res.PanTiltRelative.bTiltSpeed = 1;
	g_uvc_param.ct_ctrl_min.PanTiltRelative.bTiltSpeed = 0;
	g_uvc_param.ct_ctrl_max.PanTiltRelative.bTiltSpeed = 255;

	/*-----------------------------------------------------------*/
	/* processing uinit */
	g_uvc_param.pu_ctrl_def.wBacklightCompensation = 50;
	g_uvc_param.pu_ctrl_cur.wBacklightCompensation = 50;
	g_uvc_param.pu_ctrl_res.wBacklightCompensation = 1;
	g_uvc_param.pu_ctrl_min.wBacklightCompensation = 0;
	g_uvc_param.pu_ctrl_max.wBacklightCompensation = 65535;

	g_uvc_param.pu_ctrl_def.wBrightness = 50;
	g_uvc_param.pu_ctrl_cur.wBrightness = 50;
	g_uvc_param.pu_ctrl_res.wBrightness = 1;
	g_uvc_param.pu_ctrl_min.wBrightness = 0;
	g_uvc_param.pu_ctrl_max.wBrightness = 100;

	g_uvc_param.pu_ctrl_def.wContrast = 50;
	g_uvc_param.pu_ctrl_cur.wContrast = 50;
	g_uvc_param.pu_ctrl_res.wContrast = 1;
	g_uvc_param.pu_ctrl_min.wContrast = 0;
	g_uvc_param.pu_ctrl_max.wContrast = 100;

	g_uvc_param.pu_ctrl_def.wGain = 50;
	g_uvc_param.pu_ctrl_cur.wGain = 50;
	g_uvc_param.pu_ctrl_res.wGain = 1;
	g_uvc_param.pu_ctrl_min.wGain = 0;
	g_uvc_param.pu_ctrl_max.wGain = 65535;

	/* 0: Disabled
	 * 1: 50 HZ
	 * 2: 60 HZ
	 * 3: Auto */
	g_uvc_param.pu_ctrl_def.bPowerLineFrequency = 2;
	g_uvc_param.pu_ctrl_cur.bPowerLineFrequency = 2;
	g_uvc_param.pu_ctrl_res.bPowerLineFrequency = 1;
	g_uvc_param.pu_ctrl_min.bPowerLineFrequency = 0;
	g_uvc_param.pu_ctrl_max.bPowerLineFrequency = 3;

	g_uvc_param.pu_ctrl_def.wHue = 50;
	g_uvc_param.pu_ctrl_cur.wHue = 50;
	g_uvc_param.pu_ctrl_res.wHue = 1;
	g_uvc_param.pu_ctrl_min.wHue = 0;
	g_uvc_param.pu_ctrl_max.wHue = 255;

	g_uvc_param.pu_ctrl_def.bHueAuto = 1;
	g_uvc_param.pu_ctrl_cur.bHueAuto = 1;
	g_uvc_param.pu_ctrl_res.bHueAuto = 1;
	g_uvc_param.pu_ctrl_min.bHueAuto = 0;
	g_uvc_param.pu_ctrl_max.bHueAuto = 1;

	g_uvc_param.pu_ctrl_def.wSaturation = 50;
	g_uvc_param.pu_ctrl_cur.wSaturation = 50;
	g_uvc_param.pu_ctrl_res.wSaturation = 1;
	g_uvc_param.pu_ctrl_min.wSaturation = 0;
	g_uvc_param.pu_ctrl_max.wSaturation = 65535;

	g_uvc_param.pu_ctrl_def.wSharpness = 50;
	g_uvc_param.pu_ctrl_cur.wSharpness = 50;
	g_uvc_param.pu_ctrl_res.wSharpness = 1;
	g_uvc_param.pu_ctrl_min.wSharpness = 0;
	g_uvc_param.pu_ctrl_max.wSharpness = 65535;

	g_uvc_param.pu_ctrl_def.wGamma = 50;
	g_uvc_param.pu_ctrl_cur.wGamma = 50;
	g_uvc_param.pu_ctrl_res.wGamma = 1;
	g_uvc_param.pu_ctrl_min.wGamma = 0;
	g_uvc_param.pu_ctrl_max.wGamma = 500;

	g_uvc_param.pu_ctrl_def.wWhiteBalanceTemperature = 1500;
	g_uvc_param.pu_ctrl_cur.wWhiteBalanceTemperature = 1500;
	g_uvc_param.pu_ctrl_res.wWhiteBalanceTemperature = 100;
	g_uvc_param.pu_ctrl_min.wWhiteBalanceTemperature = 1500;
	g_uvc_param.pu_ctrl_max.wWhiteBalanceTemperature = 15000;

	g_uvc_param.pu_ctrl_def.bWhiteBalanceTemperatureAuto = 1;
	g_uvc_param.pu_ctrl_cur.bWhiteBalanceTemperatureAuto = 1;
	g_uvc_param.pu_ctrl_res.bWhiteBalanceTemperatureAuto = 1;
	g_uvc_param.pu_ctrl_min.bWhiteBalanceTemperatureAuto = 0;
	g_uvc_param.pu_ctrl_max.bWhiteBalanceTemperatureAuto = 1;

	g_uvc_param.pu_ctrl_def.WhiteBalanceComponent.wWhiteBalanceBlue = 50;
	g_uvc_param.pu_ctrl_cur.WhiteBalanceComponent.wWhiteBalanceBlue = 50;
	g_uvc_param.pu_ctrl_res.WhiteBalanceComponent.wWhiteBalanceBlue = 1;
	g_uvc_param.pu_ctrl_min.WhiteBalanceComponent.wWhiteBalanceBlue = 0;
	g_uvc_param.pu_ctrl_max.WhiteBalanceComponent.wWhiteBalanceBlue = 255;

	g_uvc_param.pu_ctrl_def.WhiteBalanceComponent.wWhiteBalanceRed = 50;
	g_uvc_param.pu_ctrl_cur.WhiteBalanceComponent.wWhiteBalanceRed = 50;
	g_uvc_param.pu_ctrl_res.WhiteBalanceComponent.wWhiteBalanceRed = 1;
	g_uvc_param.pu_ctrl_min.WhiteBalanceComponent.wWhiteBalanceRed = 0;
	g_uvc_param.pu_ctrl_max.WhiteBalanceComponent.wWhiteBalanceRed = 255;
}

int uvc_camera_terminal_setup_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *resp,
				void *userdata)
{
	uint16_t len = 0;
	struct uvc_cs_ops *ops = (struct uvc_cs_ops *)uvc_get_cs_ops(cs,
			UVC_CTRL_CAMERA_TERMINAL_ID);

	if (!ops) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s get cs[%d] ops failed!\n", __func__, cs);
		return -1;
	}

	if (req == UVC_GET_LEN) {
		UVC_PRINTF(UVC_LOG_DEBUG, "# %s UVC_GET_LEN len = %d\n", __func__, ops->len);
		len = ops->len;
		memcpy(resp->data, &len, sizeof(len));
		resp->length = sizeof(len);

		goto end;
	}

	if (!ops->setup_phase_f)
		return -1;

	(*ops->setup_phase_f)(ctx, resp, req, cs, userdata);

end:
	return 0;
}

int uvc_process_unit_setup_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *resp,
				void *userdata)
{
	uint16_t len;
	struct uvc_cs_ops *ops = (struct uvc_cs_ops *)uvc_get_cs_ops(cs,
			UVC_CTRL_PROCESSING_UNIT_ID);

	if (!ops) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s get cs[%d] ops failed!\n", __func__, cs);
		return -1;
	}

	if (req == UVC_GET_LEN) {
		UVC_PRINTF(UVC_LOG_DEBUG, "# %s UVC_GET_LEN len = %d\n", __func__, ops->len);
		len = ops->len;
		memcpy(resp->data, &len, sizeof(len));
		resp->length = sizeof(len);

		goto end;
	}

	if (!ops->setup_phase_f)
		return -1;

	(*ops->setup_phase_f)(ctx, resp, req, cs, userdata);

end:
	return 0;
}

int uvc_extension_unit_setup_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *resp,
				void *userdata)
{
	uint16_t len;
	struct uvc_cs_ops *ops = (struct uvc_cs_ops *)uvc_get_cs_ops(cs,
			UVC_CTRL_EXTENSION_UNIT_ID);

	if (!ops) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s get cs[%d] ops failed!\n", __func__, cs);
		return -1;
	}

	if (req == UVC_GET_LEN) {
		UVC_PRINTF(UVC_LOG_DEBUG, "# %s UVC_GET_LEN len = %d\n", __func__, ops->len);
		len = ops->len;
		memcpy(resp->data, &len, sizeof(len));
		resp->length = sizeof(len);

		goto end;
	}

	if (!ops->setup_phase_f)
		return -1;

	(*ops->setup_phase_f)(ctx, resp, req, cs, userdata);

end:
	return 0;
}

int uvc_camera_terminal_data_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *data,
				void *userdata)
{
	struct uvc_cs_ops *ops = (struct uvc_cs_ops *)uvc_get_cs_ops(cs,
			UVC_CTRL_CAMERA_TERMINAL_ID);

	if (!ops) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s get cs[%d] ops failed!\n", __func__, cs);
		return -1;
	}

	if (!ops->data_phase_f) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s cs[%d] no ops->data_phase_f\n",
				__func__, cs);
		return -1;
	}

	(*ops->data_phase_f)(ctx, data, cs, userdata);

	return 0;
}

int uvc_process_unit_data_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *data,
				void *userdata)
{
	struct uvc_cs_ops *ops = (struct uvc_cs_ops *)uvc_get_cs_ops(cs,
			UVC_CTRL_PROCESSING_UNIT_ID);

	if (!ops) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s get cs[%d] ops failed!\n", __func__, cs);
		return -1;
	}

	if (!ops->data_phase_f) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s cs[%d] no ops->data_phase_f\n",
				__func__, cs);
		return -1;
	}

	(*ops->data_phase_f)(ctx, data, cs, userdata);

	return 0;
}

int uvc_extension_unit_data_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *data,
				void *userdata)
{
	struct uvc_cs_ops *ops = (struct uvc_cs_ops *)uvc_get_cs_ops(cs,
			UVC_CTRL_EXTENSION_UNIT_ID);

	if (!ops) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s get cs[%d] ops failed!\n", __func__, cs);
		return -1;
	}

	if (!ops->data_phase_f) {
		UVC_PRINTF(UVC_LOG_ERR, "# %s cs[%d] no ops->data_phase_f\n",
				__func__, cs);
		return -1;
	}

	(*ops->data_phase_f)(ctx, data, cs, userdata);

	return 0;
}
