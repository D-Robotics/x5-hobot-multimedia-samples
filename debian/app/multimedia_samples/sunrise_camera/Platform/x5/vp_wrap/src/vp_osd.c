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

/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-02-23 14:01:59
 * @LastEditTime: 2023-03-05 15:57:48
 ***************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils/utils_log.h"

#include "vp_wrap.h"
#include "vp_osd.h"

static int region_init(vp_vflow_contex_t *vp_vflow_contex){

	int osd_valid_region_count = vp_vflow_contex->osd_info.valid_osd_region_count;

    for (int i = 0; i < osd_valid_region_count; i++) {
		hbn_rgn_handle_t rgn_handle = vp_vflow_contex->osd_info.handle[i];
		hbn_rgn_attr_t region;

		int width = vp_vflow_contex->osd_info.position[i].width;
		int height = vp_vflow_contex->osd_info.position[i].height;
		region.type = OVERLAY_RGN;
		region.color = FONT_COLOR_ORANGE;
		region.alpha = 0;
		region.overlay_attr.size.width = width;
		region.overlay_attr.size.height = height;
		region.overlay_attr.pixel_fmt = PIXEL_FORMAT_VGA_8;

		SC_LOGI("osd region init %d :%d*%d.", width, height);
		//VSE硬件上最多支持4块OSD，其他多余的OSD通过软件操作图像数据完成。
		int ret = hbn_rgn_create(rgn_handle, &region);
        if(ret != 0){
            SC_LOGE("osd init region for channel %d failed %d.", i, ret);
            return -1;
        }
	}
    for (int i = 0; i < osd_valid_region_count; i++) {
        hbn_rgn_bitmap_t *bitmap_p = &(vp_vflow_contex->osd_info.bitmap[i]);
		int width = vp_vflow_contex->osd_info.position[i].width;
		int height = vp_vflow_contex->osd_info.position[i].height;

	    int32_t size = width * height;
        memset(bitmap_p, 0, sizeof(hbn_rgn_bitmap_t));
        bitmap_p->pixel_fmt = PIXEL_FORMAT_VGA_8;
        bitmap_p->size.width = width;
        bitmap_p->size.height = height;
        bitmap_p->paddr = malloc(size);
        if(bitmap_p->paddr == NULL){
            SC_LOGE("regino init failed.");
            exit(-1);
        }
        memset(bitmap_p->paddr, 0x0F, size);
    }
    return 0;
}

static int channel_attr_init(vp_vflow_contex_t *vp_vflow_contex){
    int vse_vnode_fd = vp_vflow_contex->vse_node_handle;
	int osd_valid_region_count = vp_vflow_contex->osd_info.valid_osd_region_count;


	for (int i = 0; i < osd_valid_region_count; i++) {
		hbn_rgn_handle_t rgn_handle = vp_vflow_contex->osd_info.handle[i];

		hbn_rgn_chn_attr_t chn_attr = {0};
		memset(&chn_attr, 0, sizeof(hbn_rgn_chn_attr_t));
		chn_attr.show = true;
		chn_attr.invert_en = 0;
		chn_attr.display_level = 0;
		chn_attr.point.x = vp_vflow_contex->osd_info.position[i].x;
		chn_attr.point.y = vp_vflow_contex->osd_info.position[i].y;

		/*
			1. region 和 VSE 绑定
			2. rgn_handle: 函数region_init中初始化中 rgn_handle从0开始
		*/
		int ret = hbn_rgn_attach_to_chn(rgn_handle, vse_vnode_fd, i, &chn_attr);
        if(ret != 0){
            SC_LOGE("osd init attr for channel %d vse %d failed, ret: %d:%s", i, vse_vnode_fd, ret, hbn_err_info(ret));
            return -1;
        }
	}

    return 0;
}

int32_t vp_osd_init(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;
    ret = region_init(vp_vflow_contex); //320, 200
    if(ret != 0){
        return -1;
    }
	//(50, 50): 起始点坐标
    ret = channel_attr_init(vp_vflow_contex);
      if(ret != 0){
        return -1;
    }

	SC_LOGD("successful");
	return ret;
}

int32_t vp_osd_deinit(vp_vflow_contex_t *vp_vflow_contex)
{
	int osd_valid_region_count = vp_vflow_contex->osd_info.valid_osd_region_count;

    int vse_vnode_fd = vp_vflow_contex->vse_node_handle;
    for (int i = 0; i < osd_valid_region_count; i++) {
		hbn_rgn_handle_t rgn_handle = vp_vflow_contex->osd_info.handle[i];
		hbn_rgn_detach_from_chn(rgn_handle, vse_vnode_fd, i);
		hbn_rgn_destroy(rgn_handle);

        hbn_rgn_bitmap_t *bitmap_p = &(vp_vflow_contex->osd_info.bitmap[i]);
        if(bitmap_p->paddr != NULL){
            free(bitmap_p->paddr);
            bitmap_p->paddr = NULL;
        }
	}
	SC_LOGD("successful");
	return 0;
}

int32_t vp_osd_start(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	SC_LOGD("successful");
	return ret;
}

int32_t vp_osd_stop(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	SC_LOGD("successful");
	return ret;
}
int32_t vp_osd_draw_world(vp_vflow_contex_t *vp_vflow_contex, int osd_index, char *str){

	int osd_valid_region_count = vp_vflow_contex->osd_info.valid_osd_region_count;

    if((osd_index < 0) || (osd_index >= osd_valid_region_count)){
        SC_LOGE("osd draw world failed, handle is invalid %d, osd region count is %d.",
			osd_index, osd_valid_region_count);
        return -1;
    }

	hbn_rgn_handle_t handle = vp_vflow_contex->osd_info.handle[osd_index];
    hbn_rgn_bitmap_t *bitmap_p = &(vp_vflow_contex->osd_info.bitmap[osd_index]);
    hbn_rgn_draw_word_t draw_word = {0};
	draw_word.font_size = FONT_SIZE_MEDIUM;
	draw_word.font_color = FONT_COLOR_WHITE;
	draw_word.bg_color = FONT_COLOR_DARKGRAY;
    draw_word.font_alpha = 10;
	draw_word.bg_alpha = 5;
	draw_word.point.x = 0;
	draw_word.point.y = 0;
	draw_word.flush_en = false;
	draw_word.draw_string = (uint8_t*)str;
	draw_word.paddr = bitmap_p->paddr;
	draw_word.size = bitmap_p->size;

	//用户申请好的buffer(malloc)上画字, 纯软件的操作
    int ret = hbn_rgn_draw_word(&draw_word);
    if(ret != 0){
        SC_LOGE("osd draw world for channel %d failed.", handle);
        return -1;
    }

	//将bitmap 中的数据，拷贝到物理内存中
    ret = hbn_rgn_setbitmap(handle, bitmap_p);
    if(ret != 0){
        SC_LOGE("osd set bitmap for channel %d failed.", handle);
        return -1;
    }
    return 0;
}
