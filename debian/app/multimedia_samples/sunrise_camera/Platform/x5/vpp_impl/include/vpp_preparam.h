#ifndef VPP_PREPARAM_H
#define VPP_PREPARAM_H

#include "vp_wrap.h"
#include "bpu_wrap.h"

void* vpp_osd_set_timestamp_thread(void *ptr);

void vpp_graphic_buf_to_bpu_buffer_info(const hbn_vnode_image_t *src, bpu_buffer_info_t *dst);
void vpp_video_frame_buffer_info_to_bpu_buffer_info(const mc_video_frame_buffer_info_t *src,
	bpu_buffer_info_t *dst);
void vpp_video_frame_buffer_info_to_vnode_image(const mc_video_frame_buffer_info_t *src,
	hbn_vnode_image_t *dst);

#endif // VPP_PREPARAM_H

