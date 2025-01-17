/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#include "hb_mem_mgr.h"
#include "common_utils.h"
#include "./vflow_connection.h"
#include "./mem_ion.h"

hbn_vnode_image_t gdc_output_image = {0};

int vflow_feedback(pipe_contex_t *pipe_contex, connection_info_t *connections,
			hbn_vnode_image_t *input_image, hbn_vnode_image_t *output_image,
			uint32_t flag)
{
	int ret = 0;

	printf(">>>> %s %d flag %d \n", __func__, __LINE__, flag);
	if ((flag == CAP_BUF_CUSTOM) || (flag == CAP_BUF_CUSTOM_EXT)) {

		ret = hbn_vnode_set_output_frame(connections->dst_node.node_handle,
						connections->dst_node.out_chn,
						output_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_sendframe(connections->src_node.node_handle,
					connections->src_node.in_chn, input_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_get_output_frame(connections->dst_node.node_handle,
						connections->dst_node.out_chn,
						output_image);
		ERR_CON_EQ(ret, 0);
	} else if (flag == CAP_BUF_INTERNAL) {
		ret = hbn_vnode_sendframe(connections->src_node.node_handle,
					connections->src_node.in_chn, input_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_getframe(connections->dst_node.node_handle,
					connections->dst_node.out_chn, 1000,
					output_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_releaseframe(connections->dst_node.node_handle,
					connections->dst_node.out_chn,
					output_image);
		ERR_CON_EQ(ret, 0);
	}

	return ret;
}

int vflow_create_and_run(pipe_contex_t *pipe_contex, node_info_t *nodes,
		size_t node_count, connection_info_t *connections, size_t connection_count)
{
	int ret = 0;

	// 创建vflow
	pipe_contex->vflow_fd = 0;
	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	printf(">>>> %s %d node_count %ld vflow_fd %ld\n", __func__, __LINE__, node_count, pipe_contex->vflow_fd);

	// 添加所有节点
	for (size_t i = 0; i < node_count; ++i) {
		printf("*** %s %d node i %ld handle %d\n", __func__, __LINE__,
				i, nodes[i].node_handle);
		ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd, nodes[i].node_handle);
		ERR_CON_EQ(ret, 0);
	}

	// 处理节点之间的连接
	for (size_t i = 0; i < connection_count; ++i) {
		ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
					connections->src_node.node_handle,
					connections->src_node.out_chn,
					connections->dst_node.node_handle,
					connections->dst_node.in_chn);
		ERR_CON_EQ(ret, 0);
	}

	// 启动vflow
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return ret;
}

int vflow_stop_and_destory(pipe_contex_t *pipe_contex)
{
	int ret = 0;
	printf(">>>> %s %d vflowid %ld \n", __func__, __LINE__, pipe_contex->vflow_fd);

	ret = hbn_vflow_stop(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(pipe_contex->vflow_fd);

	return ret;
}