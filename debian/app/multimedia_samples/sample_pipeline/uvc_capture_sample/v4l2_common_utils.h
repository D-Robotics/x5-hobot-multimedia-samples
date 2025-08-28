/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __V4L2_COMMMON_UTILS__
#define __V4L2_COMMMON_UTILS__

#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "log.h"
#include "logging.h"
#include "hb_mem_mgr.h"
#include "hbn_api.h"
#include "vin_cfg.h"
#include "isp_cfg.h"
#include "vse_cfg.h"
#include "n2d_cfg.h"
#include "gdc_cfg.h"
#include "common_utils.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#define V4L2_MAX_PIPE_NUM 24
#define TIMEOUT_S (3) /* 3s */
#define TIMEOUT_US (10000) /* 10 ms */

#define SHARE_MASK ((1 << 3) - 1)
extern int g_pipe_num;
extern int t_pipe_num;
extern uvc_camera_context TestContext[V4L2_MAX_PIPE_NUM];

void print_control_range(int fd, unsigned int id, const char* name);
int v4l2_dr_isp_expsoure_test(uvc_camera_context * const ptc);
int v4l2_dr_isp_awb_test(uvc_camera_context * const ptc);
int v4l2_isp_uvc_camera_device_dumpframe(uvc_camera_context *ptc);
int v4l2_uvc_camera_device_open(uvc_camera_context *ptc);
int v4l2_uvc_camera_device_init(uvc_camera_context *ptc);
int v4l2_uvc_camera_device_reqbufs(uvc_camera_context *ptc, unsigned int num);
int v4l2_uvc_camera_device_start(uvc_camera_context *ptc);
int v4l2_uvc_camera_device_stop(uvc_camera_context *ptc);
int v4l2_uvc_camera_device_close(uvc_camera_context *ptc);
void v4l2_filename_get(char *name, const char *path, struct v4l2_buffer *pbuffer, uvc_camera_context *ptc, int32_t raw, df_module_name_index_e mni);
uint32_t uvc_camera_get_pixelformat(uint32_t pixelformat, uint32_t format);
cam_format_e str_to_format(const char *str);
int get_format_stride(uvc_camera_context *ptc);

#ifdef __cplusplus
	}
#endif	/* __cplusplus */

#endif

