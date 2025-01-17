#ifndef __VFLOW_CONNECTION_H__
#define __VFLOW_CONNECTION_H__

#include "common_utils.h"

typedef struct {
	uint32_t node_handle;  // 节点句柄
	uint32_t out_chn;      // 输出通道编号
	uint32_t in_chn;
} node_info_t;

typedef struct {
	node_info_t src_node;  // 源节点信息
	node_info_t dst_node;  // 目标节点信息
} connection_info_t;

int vflow_create_and_run(pipe_contex_t *pipe_contex, node_info_t *nodes,
    size_t node_count, connection_info_t *connections, size_t connection_count);
int vflow_stop_and_destory(pipe_contex_t *pipe_contex);
int vflow_feedback(pipe_contex_t *pipe_contex, connection_info_t *connections,
			hbn_vnode_image_t *input_image, hbn_vnode_image_t *output_image,
			uint32_t flag);


#endif // __VFLOW_CONNECTION_H__