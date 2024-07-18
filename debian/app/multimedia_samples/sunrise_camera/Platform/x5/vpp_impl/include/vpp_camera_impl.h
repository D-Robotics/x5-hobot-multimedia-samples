#ifndef VPP_CAMERA_IMPL_H_
#define VPP_CAMERA_IMPL_H_

#include "solution_struct_define.h"

int32_t vpp_camera_init_param(void);

int32_t vpp_camera_init(void);
int32_t vpp_camera_uninit(void);
int32_t vpp_camera_start(void);
int32_t vpp_camera_stop(void);

int32_t vpp_camera_param_set(SOLUTION_PARAM_E type, char* val, uint32_t length);
int32_t vpp_camera_param_get(SOLUTION_PARAM_E type, char* val, uint32_t* length);

#endif // VPP_CAMERA_IMPL_H_