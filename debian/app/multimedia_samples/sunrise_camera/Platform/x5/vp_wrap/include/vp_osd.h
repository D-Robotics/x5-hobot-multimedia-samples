/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_OSD_H_
#define VP_OSD_H_

#include "vp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t vp_osd_init(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_osd_start(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_osd_stop(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_osd_deinit(vp_vflow_contex_t *vp_vflow_contex);

int32_t vp_osd_draw_world(vp_vflow_contex_t *vp_vflow_contex, hbn_rgn_handle_t handle, char *str);
#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_OSD_H_
