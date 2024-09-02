/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_VSE_H_
#define VP_VSE_H_

#include "vp_common.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct vp_vse_output_info_s{
	int width;
	int height;
	int fps;
}vp_vse_output_info_t;

int32_t vp_vse_init(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_vse_start(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_vse_stop(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_vse_deinit(vp_vflow_contex_t *vp_vflow_contex);

int32_t vp_vse_send_frame(vp_vflow_contex_t *vp_vflow_contex, hbn_vnode_image_t *image_frame);
// int32_t vp_vse_send_frame(vp_vflow_contex_t *vp_vflow_contex, ImageFrame *frame);
int32_t vp_vse_get_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame);
int32_t vp_vse_release_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame);

int32_t vp_vse_get_output_info(vp_vflow_contex_t *vp_vflow_contex, int32_t ochn_id, vp_vse_output_info_t *vse_out_info);
#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_VSE_H_
