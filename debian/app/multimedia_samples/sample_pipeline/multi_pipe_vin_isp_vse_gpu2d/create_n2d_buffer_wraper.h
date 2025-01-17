// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __CREATE_N2D_BUFFER_WRAPER__
#define __CREATE_N2D_BUFFER_WRAPER__

#include "hb_mem_mgr.h"
#include "GC820/nano2D.h"
#include "GC820/nano2D_util.h"


n2d_error_t create_n2d_buffer_from_hbm_graphic(n2d_buffer_t *n2d_buffer, hb_mem_graphic_buf_t *hbm_buffer);
n2d_error_t create_n2d_buffer_from_phyaddr_continuous_memory(n2d_buffer_t *n2d_buffer,
n2d_buffer_format_t format, n2d_uintptr_t phys_addr, int width, int height);

n2d_error_t create_n2d_buffer_from_hbm_common(n2d_buffer_t *n2d_buffer,
hb_mem_common_buf_t *hbm_buffer, int width, int height);

int create_hbm_graphic_buffer_from_normal_memory(hb_mem_graphic_buf_t *hbn_mem_src,
int width, int height, mem_pixel_format_t format , void *virt_addr);
#endif