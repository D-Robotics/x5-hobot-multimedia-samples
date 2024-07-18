/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_GDC_H_
#define VP_GDC_H_
#ifdef __cplusplus
extern "C" {
#endif
#include "vp_common.h"
typedef struct {
    char *sensor_name;
    char *gdc_file_name;
    int is_valid;
} gdc_list_info_t;

int32_t vp_gdc_init(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_gdc_start(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_gdc_stop(vp_vflow_contex_t *vp_vflow_contex);
int32_t vp_gdc_deinit(vp_vflow_contex_t *vp_vflow_contex);

int32_t vp_gdc_release_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame);
int32_t vp_gdc_get_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame);

int32_t vp_gdc_send_frame(vp_vflow_contex_t *vp_vflow_contex, hbn_vnode_image_t *image_frame);

const char *vp_gdc_get_bin_file(const char *sensor_name);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif // VP_GDC_H_
