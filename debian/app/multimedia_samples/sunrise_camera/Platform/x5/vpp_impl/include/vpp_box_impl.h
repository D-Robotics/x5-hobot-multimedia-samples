#ifndef VPP_BOX_IMPL_H_
#define VPP_BOX_IMPL_H_

#include "solution_struct_define.h"

int32_t vpp_box_init_param(void);

int32_t vpp_box_init(void);
int32_t vpp_box_uninit(void);
int32_t vpp_box_start(void);
int32_t vpp_box_stop(void);

int32_t vpp_box_param_set(SOLUTION_PARAM_E type, char* val, uint32_t length);
int32_t vpp_box_param_get(SOLUTION_PARAM_E type, char* val, uint32_t* length);

#endif // VPP_BOX_IMPL_H_