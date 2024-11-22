/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * uvc_event_handler.h
 *	uvc camera terminal, process unit, extension unit event handler
 *
 * Copyright (C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */

#ifndef __UVC_EVENT_HANDLER_H__
#define __UVC_EVENT_HANDLER_H__

#include "uvc_gadget_api.h"

/*----------------------------------------------------------------------------*/
/* uvc standard control request define */

#define UVC_AUTO_EXPOSURE_MODE_MANUAL (1)		//- manual exposure time, manual iris
#define UVC_AUTO_EXPOSURE_MODE_AUTO (2) 		//- auto exposure time, auto iris
#define UVC_AUTO_EXPOSURE_MODE_SHUTTER_PRIORITY (4)	//- manual exposure time, auto iris
#define UVC_AUTO_EXPOSURE_MODE_APERTURE_PRIORITY (8)	//- auto exposure time, manual iris
union bAutoExposureMode {
	uint8_t d8;
	struct {
	uint8_t manual:1;
	uint8_t auto_exposure:1;
	uint8_t shutter_priority:1;
	uint8_t aperture_priority:1;
	uint8_t reserved:4;
	}b;
};

struct FocusRelativeControl {
	uint8_t bFocusRelative;
	int8_t bSpeed;
};

struct ZoomRelativeControl {
	uint8_t bZoom;
	uint8_t bDigitalZoom;
	int8_t bSpeed;
};

struct PanTiltAbsoluteControl {
	uint32_t dwPanAbsolute;
	uint32_t dwTiltAbsolute;
};

struct PanTiltRelativeControl {
	int8_t bPanRelative;
	int8_t bPanSpeed;
	int8_t bTiltRelative;
	int8_t bTiltSpeed;
};

struct RollRelativeControl {
	uint8_t bRollRelative;
	int8_t bSpeed;
};

struct DigitalWindowControl {
	uint16_t wWindowTop;
	uint16_t wWindowLeft;
	uint16_t wWindowBottom;
	uint16_t wWindowRight;
	uint16_t wNumSteps;
	uint16_t bmNumStepsUnits;
};

struct RegionOfInterestControl {
	uint16_t wRoiTop;
	uint16_t wRoiLeft;
	uint16_t wRoiBottom;
	uint16_t wRoiRight;
	uint16_t bmAutoControls;
};

struct WhiteBalanceComponentControl {
	int16_t wWhiteBalanceBlue;
	int16_t wWhiteBalanceRed;
};

union bDevicePowerMode {
	uint8_t d8;
	struct {
		uint8_t PowerModesetting:4;
		uint8_t DependentPower:1;
		uint8_t ubs:1;
		uint8_t battery:1;
		uint8_t ac:1;
	} b;
};

union uvc_video_control_interface_u {
	union bDevicePowerMode DevicePowerMode; 	// UVC_VC_VIDEO_POWER_MODE_CONTROL
	uint8_t bRequestErrorCode; 			// UVC_VC_REQUEST_ERROR_CODE_CONTROL
};

union uvc_camera_terminal_control_u {
	uint8_t bScanningMode; 				// UVC_CT_SCANNING_MODE_CONTROL

	union bAutoExposureMode AEMode; 		// UVC_CT_AE_MODE_CONTROL
	int8_t bAutoExposurePriority; 			// UVC_CT_AE_PRIORITY_CONTROL

	int32_t dwExposureTimeAbsolute;			// UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL
	uint8_t bExposureTimeRelative;			// UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL

	int16_t wFocusAbsolute;				// UVC_CT_FOCUS_ABSOLUTE_CONTROL
	struct FocusRelativeControl FocusRelative;	// UVC_CT_FOCUS_RELATIVE_CONTROL
	uint8_t bFocusAuto;				// UVC_CT_FOCUS_AUTO_C ONTROL

	int16_t wIrisAbsolute;				// UVC_CT_IRIS_ABSOLUTE_CONTROL
	int8_t bIrisRelative;				// UVC_CT_IRIS_RELATIVE_CONTROL

	int16_t wObjectiveFocalLength;			// UVC_CT_ZOOM_ABSOLUTE_CONTROL
	struct ZoomRelativeControl ZoomRelative;	// UVC_CT_ZOOM_RELATIVE_CONTROL

	struct PanTiltAbsoluteControl PanTiltAbsolute;	// UVC_CT_PANTILT_ABSOLUTE_CONTROL
	struct PanTiltRelativeControl PanTiltRelative;	// UVC_CT_PANTILT_RE LATIVE _CO N TRO L

	uint16_t wRollAbsolute;				// UVC_CT_ROLL_ABSOLUTE_CONTROL
	struct RollRelativeControl RollRelative;	// UVC_CT_ROLL_RELATIVE_CONTROL
	uint8_t bPrivacy;				// UVC_CT_PRIVACY_CONTROL

	uint8_t buf[8];
};

union uvc_processing_unit_control_u {
	int16_t wBacklightCompensation;		// UVC_PU_BACKLIGHT_COMPENSATION_CONTROL
	uint16_t wBrightness;			// UVC_PU_BRIGHTNESS_CONTROL

	int16_t wContrast;			// UVC_PU_CONTRAST_CONTROL
	int16_t wGain;				// UVC_PU_GAIN_CONTROL
	int8_t bPowerLineFrequency;		// UVC_PU_POWER_LINE_FR EQUENCY_CONTROL

	uint16_t wHue;				// UVC_PU_HUE_CONTROL
	int8_t bHueAuto;			// UVC_PU_HUE_AUTO_CONTROL

	int16_t wSaturation;			// UVC_PU_SATURATION_CONTROL
	int16_t wSharpness;			// UVC_PU_SHARPNESS_CONTROL
	int16_t wGamma;				// UVC_PU_GAMMA_CONTROL

	int16_t wWhiteBalanceTemperature; 	// UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL
	int8_t bWhiteBalanceTemperatureAuto;	// UVC_PU_WHITE_BALANCE_TEMPERATURE_AUT O _ CONTROL
	struct WhiteBalanceComponentControl WhiteBalanceComponent; // UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL
	int8_t bWhiteBalanceComponentAuto;	// UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL

	int16_t wMultiplierStep;		// UVC_PU_DIGITAL_MULTIPLIER_CONTROL
	int16_t wMultiplierLimit;		// UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL

	int8_t bVideoStandard; 			// UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL
	int8_t bStatus;				// UVC_PU_ANALOG_LOCK_STATUS_CONTROL

	uint8_t buf[8];
};

struct uvc_video_control_interface_obj {
	union bDevicePowerMode DevicePowerMode; 	// UVC_VC_VIDEO_POWER_MODE_CONTROL
	uint8_t bRequestErrorCode; 			// UVC_VC_REQUEST_ERROR_CODE_CONTROL
};

struct uvc_camera_terminal_control_obj {
	uint8_t bScanningMode; 				// UVC_CT_SCANNING_MODE_CONTROL

	union bAutoExposureMode AEMode; 		// UVC_CT_AE_MODE_CONTROL
	int8_t bAutoExposurePriority; 			// UVC_CT_AE_PRIORITY_CONTROL

	int32_t dwExposureTimeAbsolute;			// UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL
	uint8_t bExposureTimeRelative;			// UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL

	int16_t wFocusAbsolute;				// UVC_CT_FOCUS_ABSOLUTE_CONTROL
	struct FocusRelativeControl FocusRelative;	// UVC_CT_FOCUS_RELATIVE_CONTROL
	uint8_t bFocusAuto;				// UVC_CT_FOCUS_AUTO_C ONTROL

	int16_t wIrisAbsolute;				// UVC_CT_IRIS_ABSOLUTE_CONTROL
	int8_t bIrisRelative;				// UVC_CT_IRIS_RELATIVE_CONTROL

	int16_t wObjectiveFocalLength;			// UVC_CT_ZOOM_ABSOLUTE_CONTROL
	struct ZoomRelativeControl ZoomRelative;	// UVC_CT_ZOOM_RELATIVE_CONTROL

	struct PanTiltAbsoluteControl PanTiltAbsolute;	// UVC_CT_PANTILT_ABSOLUTE_CONTROL
	struct PanTiltRelativeControl PanTiltRelative;	// UVC_CT_PANTILT_RE LATIVE _CO N TRO L

	uint16_t wRollAbsolute;				// UVC_CT_ROLL_ABSOLUTE_CONTROL
	struct RollRelativeControl RollRelative;	// UVC_CT_ROLL_RELATIVE_CONTROL
	uint8_t bPrivacy;				// UVC_CT_PRIVACY_CONTROL
};

struct uvc_processing_unit_control_obj {
	int16_t wBacklightCompensation;		// UVC_PU_BACKLIGHT_COMPENSATION_CONTROL
	int16_t wBrightness;			// UVC_PU_BRIGHTNESS_CONTROL

	int16_t wContrast;			// UVC_PU_CONTRAST_CONTROL
	int16_t wGain;				// UVC_PU_GAIN_CONTROL
	int8_t bPowerLineFrequency;		// UVC_PU_POWER_LINE_FR EQUENCY_CONTROL

	uint16_t wHue;				// UVC_PU_HUE_CONTROL
	int8_t bHueAuto;			// UVC_PU_HUE_AUTO_CONTROL

	int16_t wSaturation;			// UVC_PU_SATURATION_CONTROL
	int16_t wSharpness;			// UVC_PU_SHARPNESS_CONTROL
	int16_t wGamma;				// UVC_PU_GAMMA_CONTROL

	int16_t wWhiteBalanceTemperature; 	// UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL
	int8_t bWhiteBalanceTemperatureAuto;	// UVC_PU_WHITE_BALANCE_TEMPERATURE_AUT O _ CONTROL
	struct WhiteBalanceComponentControl WhiteBalanceComponent; // UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL
	int8_t bWhiteBalanceComponentAuto;	// UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL

	int16_t wMultiplierStep;		// UVC_PU_DIGITAL_MULTIPLIER_CONTROL
	int16_t wMultiplierLimit;		// UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL

	int8_t bVideoStandard; 			// UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL
	int8_t bStatus;				// UVC_PU_ANALOG_LOCK_STATUS_CONTROL
};

struct uvc_control_request_param {
	struct uvc_camera_terminal_control_obj ct_ctrl_def;
	struct uvc_camera_terminal_control_obj ct_ctrl_max;
	struct uvc_camera_terminal_control_obj ct_ctrl_min;
	struct uvc_camera_terminal_control_obj ct_ctrl_cur;
	struct uvc_camera_terminal_control_obj ct_ctrl_res;

	struct uvc_processing_unit_control_obj pu_ctrl_def;
	struct uvc_processing_unit_control_obj pu_ctrl_max;
	struct uvc_processing_unit_control_obj pu_ctrl_min;
	struct uvc_processing_unit_control_obj pu_ctrl_cur;
	struct uvc_processing_unit_control_obj pu_ctrl_res;
};

/*----------------------------------------------------------------------------*/
/* callback function typedef */

typedef void (*uvc_setup_callback)(struct uvc_context *ctx,
		struct uvc_request_data *resp,
		uint8_t req, uint8_t cs,
		void *userdata);

typedef void (*uvc_data_callback)(struct uvc_context *ctx,
		struct uvc_request_data *date,
		uint8_t cs, void * userdata);

struct uvc_cs_ops {
	uint8_t cs;
	uint8_t len;

	/*setup phase*/
	uvc_setup_callback setup_phase_f;	/* data: device to host */

	/*data phase*/
	uvc_data_callback data_phase_f;		/* data: host ot device */
};

/* init uvc control request callback list */
void uvc_control_request_callback_init();

/* init uvc control request param list */
void uvc_control_request_param_init();

/* CT, PU & EU setup event handler */
int uvc_camera_terminal_setup_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *resp,
				void *userdata);
int uvc_process_unit_setup_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *resp,
				void *userdata);
int uvc_extension_unit_setup_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *resp,
				void *userdata);

/* CT, PU & EU data event handler */
int uvc_camera_terminal_data_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *data,
				void *userdata);
int uvc_process_unit_data_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *data,
				void *userdata);
int uvc_extension_unit_data_event(struct uvc_context *ctx,
				uint8_t req, uint8_t cs,
				struct uvc_request_data *data,
				void *userdata);

#endif	/* __UVC_EVENT_HANDLER_H__ */
