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
#include "vp_vse.h"

int32_t vp_vse_init(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0, i = 0;
	uint32_t hw_id = 0;
	uint32_t ichn_id = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	hbn_vnode_handle_t *vse_node_handle = NULL;
	vse_config_t *vse_config = NULL;

	vse_node_handle = &vp_vflow_contex->vse_node_handle;
	vse_config = &vp_vflow_contex->vse_config;

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, vse_node_handle);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_open");

	ret = hbn_vnode_set_attr(*vse_node_handle, &vse_config->vse_attr);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_attr");

	ret = hbn_vnode_set_ichn_attr(*vse_node_handle, ichn_id, &vse_config->vse_ichn_attr);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_ichn_attr");

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED
						| HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;

	for (i = 0; i < 6; i++) {
		if (vse_config->vse_ochn_attr[i].chn_en) {
			ret = hbn_vnode_set_ochn_attr(*vse_node_handle, i, &vse_config->vse_ochn_attr[i]);
			SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_ochn_attr");
			ret = hbn_vnode_set_ochn_buf_attr(*vse_node_handle, i, &alloc_attr);
			SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_ochn_buf_attr");
		}
	}

	SC_LOGD("successful");
	return 0;
}

int32_t vp_vse_deinit(vp_vflow_contex_t *vp_vflow_contex)
{
	hbn_vnode_close(vp_vflow_contex->vse_node_handle);
	SC_LOGD("successful");
	return 0;
}

int32_t vp_vse_start(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	SC_LOGD("successful");
	return ret;
}

int32_t vp_vse_stop(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	SC_LOGD("successful");
	return ret;
}

int32_t vp_vse_send_frame(vp_vflow_contex_t *vp_vflow_contex, hbn_vnode_image_t *image_frame)
{
	int32_t ret = 0;
	ret = hbn_vnode_sendframe(vp_vflow_contex->vse_node_handle, 0, image_frame);
	if (ret != 0) {
		SC_LOGE("hbn_vnode_sendframe failed(%d)", ret);
		return -1;
	}
	return ret;
}

int32_t vp_vse_get_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame)
{
	int32_t ret = 0;
	hbn_vnode_handle_t vse_node_handle = vp_vflow_contex->vse_node_handle;

	ret = hbn_vnode_getframe(vse_node_handle, ochn_id, VP_GET_FRAME_TIMEOUT,
		frame->hbn_vnode_image);
	return ret;
}

int32_t vp_vse_release_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame)
{
	int32_t ret = 0;
	hbn_vnode_handle_t vse_node_handle = vp_vflow_contex->vse_node_handle;

	ret = hbn_vnode_releaseframe(vse_node_handle, ochn_id, frame->hbn_vnode_image);
	return ret;
}
