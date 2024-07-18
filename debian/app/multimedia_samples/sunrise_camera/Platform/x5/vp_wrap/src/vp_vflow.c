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
#include "vp_vflow.h"

int32_t vp_vflow_init(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	// 创建HBN flow
	ret = hbn_vflow_create(&vp_vflow_contex->vflow_fd);
	SC_ERR_CON_EQ(ret, 0, "hbn_vflow_create");

	if (vp_vflow_contex->vin_node_handle) {
		ret = hbn_vflow_add_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->vin_node_handle);
		SC_ERR_CON_EQ(ret, 0, "hbn_vflow_add_vnode");
	}
	if (vp_vflow_contex->isp_node_handle) {
		ret = hbn_vflow_add_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->isp_node_handle);
		SC_ERR_CON_EQ(ret, 0, "hbn_vflow_add_vnode");
	}
	if (vp_vflow_contex->vse_node_handle) {
		ret = hbn_vflow_add_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->vse_node_handle);
		SC_ERR_CON_EQ(ret, 0, "hbn_vflow_add_vnode");
	}
	if(vp_vflow_contex->gdc_info.gdc_fd){
		ret = hbn_vflow_add_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->gdc_info.gdc_fd);
		SC_ERR_CON_EQ(ret, 0, "hbn_vflow_add_gdc_vnode");
	}

	SC_LOGD("successful");
	return 0;
}

int32_t vp_vflow_deinit(vp_vflow_contex_t *vp_vflow_contex)
{
	hbn_vflow_destroy(vp_vflow_contex->vflow_fd);
	return 0;
}

int32_t vp_vflow_start(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;

	if (vp_vflow_contex->vin_node_handle) {
		ret = hbn_camera_attach_to_vin(vp_vflow_contex->cam_fd,
							vp_vflow_contex->vin_node_handle);
		SC_ERR_CON_EQ(ret, 0, "hbn_camera_attach_to_vin");
	}

	// 绑定 vflow 和 vnode
	if (vp_vflow_contex->vin_node_handle && vp_vflow_contex->isp_node_handle) {
		ret = hbn_vflow_bind_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->vin_node_handle,
								0,
								vp_vflow_contex->isp_node_handle,
								0);
		SC_ERR_CON_EQ(ret, 0, "hbn_vflow_bind_vnode");
	}
	if(vp_vflow_contex->gdc_info.gdc_fd){
		if (vp_vflow_contex->isp_node_handle && vp_vflow_contex->gdc_info.gdc_fd) {
			ret = hbn_vflow_bind_vnode(vp_vflow_contex->vflow_fd,
									vp_vflow_contex->isp_node_handle,
									0,
									vp_vflow_contex->gdc_info.gdc_fd,
									0);
			SC_ERR_CON_EQ(ret, 0, "hbn_vflow_bind_vnode: isp->gdc");
		}

		if (vp_vflow_contex->gdc_info.gdc_fd && vp_vflow_contex->vse_node_handle) {


			ret = hbn_vflow_bind_vnode(vp_vflow_contex->vflow_fd,
									vp_vflow_contex->gdc_info.gdc_fd,
									0,
									vp_vflow_contex->vse_node_handle,
									0);
			SC_ERR_CON_EQ(ret, 0, "hbn_vflow_bind_vnode: gdc->vse");
		}
	}else{
		if (vp_vflow_contex->isp_node_handle && vp_vflow_contex->vse_node_handle) {
			ret = hbn_vflow_bind_vnode(vp_vflow_contex->vflow_fd,
									vp_vflow_contex->isp_node_handle,
									0,
									vp_vflow_contex->vse_node_handle,
									0);
			SC_ERR_CON_EQ(ret, 0, "hbn_vflow_bind_vnode: isp->vse");
		}
	}

	ret = hbn_vflow_start(vp_vflow_contex->vflow_fd);
	SC_ERR_CON_EQ(ret, 0, "hbn_vflow_start");

	SC_LOGD("successful");
	return ret;
}

int32_t vp_vflow_stop(vp_vflow_contex_t *vp_vflow_contex)
{
	int32_t ret = 0;
	ret = hbn_vflow_stop(vp_vflow_contex->vflow_fd);
	SC_ERR_CON_EQ(ret, 0, "hbn_vflow_stop");

	if (vp_vflow_contex->vin_node_handle) {
		hbn_camera_detach_from_vin(vp_vflow_contex->cam_fd);
	}
	if (vp_vflow_contex->vin_node_handle && vp_vflow_contex->isp_node_handle) {
		hbn_vflow_unbind_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->isp_node_handle,
								0,
								vp_vflow_contex->vse_node_handle,
								0);
	}
	if (vp_vflow_contex->isp_node_handle && vp_vflow_contex->vse_node_handle) {
		hbn_vflow_unbind_vnode(vp_vflow_contex->vflow_fd,
								vp_vflow_contex->vin_node_handle,
								0,
								vp_vflow_contex->isp_node_handle,
								0);
	}

	if(vp_vflow_contex->gdc_info.gdc_fd){
		if (vp_vflow_contex->isp_node_handle && vp_vflow_contex->gdc_info.gdc_fd) {
			ret = hbn_vflow_unbind_vnode(vp_vflow_contex->vflow_fd,
									vp_vflow_contex->isp_node_handle,
									0,
									vp_vflow_contex->gdc_info.gdc_fd,
									0);
			SC_ERR_CON_EQ(ret, 0, "hbn_vflow_unbind_vnode: isp->gdc");
		}

		if (vp_vflow_contex->gdc_info.gdc_fd && vp_vflow_contex->vse_node_handle) {


			ret = hbn_vflow_unbind_vnode(vp_vflow_contex->vflow_fd,
									vp_vflow_contex->gdc_info.gdc_fd,
									0,
									vp_vflow_contex->vse_node_handle,
									0);
			SC_ERR_CON_EQ(ret, 0, "hbn_vflow_unbind_vnode: gdc->vse");
		}
	}else{
		if (vp_vflow_contex->isp_node_handle && vp_vflow_contex->vse_node_handle) {
			ret = hbn_vflow_unbind_vnode(vp_vflow_contex->vflow_fd,
									vp_vflow_contex->isp_node_handle,
									0,
									vp_vflow_contex->vse_node_handle,
									0);
			SC_ERR_CON_EQ(ret, 0, "hbn_vflow_unbind_vnode: isp->vse");
		}
	}


	SC_LOGD("successful");
	return 0;
}
