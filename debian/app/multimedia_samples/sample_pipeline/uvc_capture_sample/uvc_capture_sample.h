/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __VPM_TEST_H__
#define __VPM_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

// #include "common_utils.h"
#include "v4l2_common_utils.h"

extern uint32_t dump_mask;
extern int32_t run_time;
extern int32_t feedback_mode;
extern int32_t loop_cnt;
extern int32_t isp_info_mask;
int v4l2_parse_opts(int argc, char **argv, uvc_camera_context *ptc, int max_num);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif
