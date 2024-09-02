/***************************************************************************
 * COPYRIGHT NOTICE
 * Copyright 2019 Horizon Robotics, Inc.
 * All rights reserved.
 ***************************************************************************/
#ifndef __SAMPLE_COMMON_H__
#define __SAMPLE_COMMON_H__

#include <stdint.h>

#include <hb_mem_mgr.h>

#define gettid() syscall(SYS_gettid)

#define MAX_HEAP_NUM 3
#define MAX_HW_NUM 12

#define PID_NAME "sample_hbmem"

uint64_t get_time_us(void);
char * get_heap_name(int64_t heap_mask);
int32_t do_sys_command(int64_t heap_mask);
int32_t get_ycbcr_info(int32_t w, int32_t h, int32_t format,
			int32_t stride, int32_t vstride,
			int32_t *planes, size_t *luma_size, size_t *chroma_size,
			size_t * total_size);

int sample_alloc_com_buf();
int sample_alloc_com_buf_with_cache();
int sample_alloc_com_buf_with_heapmask();
int sample_alloc_graph_buf();
int sample_alloc_graph_buf_with_heapmask();
int sample_alloc_graph_buf_group();
int sample_alloc_graph_buf_group_heapmask();

int sample_share_com_buffer_fork_process_scenario();
int sample_share_com_buffer();
int sample_share_com_buffer_use_consume_cnt();

int sample_share_graph_buffer_fork_process_scenario();
int sample_share_graph_buffer();
int sample_share_graph_buffer_use_consume_cnt();

int sample_share_graph_buffer_group_fork_process_scenario();
int sample_share_graph_buffer_group();
int sample_share_graph_buffer_group_use_consume_cnt();

int sample_share_pool_fork_process_scenario();
int sample_share_pool();

int sample_queue_producer_consumer();

int sample_pool();

int sample_change_com_graph_buf();

int sample_com_buf_user_consume_cnt();
int sample_graph_buf_user_consume_cnt();
#endif