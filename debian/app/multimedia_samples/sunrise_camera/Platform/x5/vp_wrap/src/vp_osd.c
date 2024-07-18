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

static int region_init(vp_vflow_contex_t *vp_vflow_contex, int width, int height){

    hbn_rgn_attr_t region;
    region.type = OVERLAY_RGN;
	region.color = FONT_COLOR_ORANGE;
	region.alpha = 0;
	region.overlay_attr.size.width = width;
	region.overlay_attr.size.height = height;
	region.overlay_attr.pixel_fmt = PIXEL_FORMAT_VGA_8;

    for (int i = 0; i < 6; i++) {
		hbn_rgn_handle_t rgn_handle = i;
		int ret = hbn_rgn_create(rgn_handle, &region);
        if(ret != 0){
            SC_LOGE("osd init region for channel %d failed %d.", i, ret);
            return -1;
        }
	}
    for (int i = 0; i < 6; i++) {
        hbn_rgn_bitmap_t *bitmap_p = &(vp_vflow_contex->osd_info.bitmap[i]);

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

static int channel_attr_init(vp_vflow_contex_t *vp_vflow_contex, int x, int y){
    int vse_vnode_fd = vp_vflow_contex->vse_node_handle;

    hbn_rgn_chn_attr_t chn_attr = {0};
    memset(&chn_attr, 0, sizeof(hbn_rgn_chn_attr_t));
    chn_attr.show = true;
	chn_attr.invert_en = 0;
	chn_attr.display_level = 0;
	chn_attr.point.x = x;
	chn_attr.point.y = y;

	for (int i = 0; i < 6; i++) {
		hbn_rgn_handle_t rgn_handle = i;
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
    ret = region_init(vp_vflow_contex, 320, 200);
    if(ret != 0){
        return -1;
    }
    ret = channel_attr_init(vp_vflow_contex, 50, 50);
      if(ret != 0){
        return -1;
    }

	SC_LOGD("successful");
	return ret;
}

int32_t vp_osd_deinit(vp_vflow_contex_t *vp_vflow_contex)
{
    int vse_vnode_fd = vp_vflow_contex->vse_node_handle;
    for (int i = 0; i < 6; i++) {
		hbn_rgn_handle_t rgn_handle = i;
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
int32_t vp_osd_draw_world(vp_vflow_contex_t *vp_vflow_contex, hbn_rgn_handle_t handle, char *str){

    int osd_index = handle;
    if((osd_index < 0) || (osd_index > sizeof(vp_vflow_contex->osd_info.bitmap) / sizeof(hbn_rgn_bitmap_t))){
        SC_LOGE("osd draw world failed, handle is invalid %d.", handle);
        return -1;
    }

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

    int ret = hbn_rgn_draw_word(&draw_word);
    if(ret != 0){
        SC_LOGE("osd draw world for channel %d failed.", osd_index);
        return -1;
    }
    ret = hbn_rgn_setbitmap(handle, bitmap_p);
    if(ret != 0){
        SC_LOGE("osd set bitmap for channel %d failed.", osd_index);
        return -1;
    }
    return 0;
}
