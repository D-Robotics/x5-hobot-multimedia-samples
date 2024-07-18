/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_VFLOW_H_
#define VP_VFLOW_H_

#include "vp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t vp_vflow_init(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_vflow_start(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_vflow_stop(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_vflow_deinit(vp_vflow_contex_t *vp_vflow_contex);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_VFLOW_H_
