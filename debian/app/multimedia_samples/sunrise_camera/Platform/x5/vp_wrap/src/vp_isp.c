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
#include "vp_isp.h"

int32_t vp_isp_init(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;
	vp_sensor_config_t *sensor_config = NULL;
	isp_attr_t      *isp_attr = NULL;
	isp_ichn_attr_t *isp_ichn_attr = NULL;
	isp_ochn_attr_t *isp_ochn_attr = NULL;
	hbn_vnode_handle_t *isp_node_handle = NULL;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t chn_id = 0;

	sensor_config = vp_vflow_contex->sensor_config;
	isp_attr = sensor_config->isp_attr;
	isp_ichn_attr = sensor_config->isp_ichn_attr;
	isp_ochn_attr = sensor_config->isp_ochn_attr;
	isp_node_handle = &vp_vflow_contex->isp_node_handle;

	isp_attr->input_mode = 2; // offline
	ret = hbn_vnode_open(HB_ISP, 0, AUTO_ALLOC_ID, isp_node_handle);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_open");
	ret = hbn_vnode_set_attr(*isp_node_handle, isp_attr);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_attr");
	ret = hbn_vnode_set_ochn_attr(*isp_node_handle, chn_id, isp_ochn_attr);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_ochn_attr");
	ret = hbn_vnode_set_ichn_attr(*isp_node_handle, chn_id, isp_ichn_attr);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_ichn_attr");

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED
						| HB_MEM_USAGE_HW_ISP
						| HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, chn_id, &alloc_attr);
	SC_ERR_CON_EQ(ret, 0, "hbn_vnode_set_ochn_buf_attr");

	SC_LOGD("successful");
	return 0;
}

int32_t vp_isp_deinit(vp_vflow_contex_t *vp_vflow_contex)
{
	hbn_vnode_close(vp_vflow_contex->isp_node_handle);
	SC_LOGD("successful");
	return 0;
}

int32_t vp_isp_start(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	SC_LOGD("successful");
	return ret;
}

int32_t vp_isp_stop(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	SC_LOGD("successful");
	return ret;
}

int32_t vp_isp_get_frame(vp_vflow_contex_t *vp_vflow_contex, ImageFrame *frame)
{
	int32_t ret = 0;
	hbn_vnode_handle_t isp_node_handle = vp_vflow_contex->isp_node_handle;
	uint32_t chn_id = 0;

	ret = hbn_vnode_getframe(isp_node_handle, chn_id, 1000, frame->hbn_vnode_image);
	if (ret != 0) {
		SC_LOGE("hbn_vnode_getframe %d ISP failed\n", chn_id);
	}

	return ret;
}

int32_t vp_isp_release_frame(vp_vflow_contex_t *vp_vflow_contex, ImageFrame *frame)
{
	int32_t ret = 0;
	uint32_t chn_id = 0;
	hbn_vnode_handle_t isp_node_handle = vp_vflow_contex->isp_node_handle;

	ret = hbn_vnode_releaseframe(isp_node_handle, chn_id, frame->hbn_vnode_image);

	return ret;
}
