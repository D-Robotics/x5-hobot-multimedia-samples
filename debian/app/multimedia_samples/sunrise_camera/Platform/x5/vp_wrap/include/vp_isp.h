/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_ISP_H_
#define VP_ISP_H_

#include "vp_common.h"


#ifdef __cplusplus
extern "C" {
#endif

int32_t vp_isp_init(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_isp_start(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_isp_stop(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_isp_deinit(vp_vflow_contex_t *vp_vflow_contex);

int32_t vp_isp_get_frame(vp_vflow_contex_t *vp_vflow_contex, ImageFrame *frame);
int32_t vp_isp_release_frame(vp_vflow_contex_t *vp_vflow_contex, ImageFrame *frame);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_ISP_H_
