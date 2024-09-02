/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2019 Horizon Robotics, Inc.
* All rights reserved.
***************************************************************************/
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include <hb_mem_mgr.h>
#include <hb_mem_err.h>
#include "sample_common.h"

static int64_t avail_heap_flag[MAX_HEAP_NUM] = {HB_MEM_USAGE_PRIV_HEAP_DMA,
						HB_MEM_USAGE_PRIV_HEAP_RESERVED,
						HB_MEM_USAGE_PRIV_HEAP_2_RESERVED,
					};


int sample_alloc_com_buf()
{
	int32_t ret = 0;
	uint64_t size = 4*1024*1024;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	hb_mem_common_buf_t com_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_alloc_com_buf\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_com_buf(size, flags, &com_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("alloc com buf, share_id: %d\n", com_buf.share_id);
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_free_buf(com_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free com buf\n");
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	printf("sample_alloc_com_buf done\n");
	printf("=================================================\n");

	return 0;
}

int sample_alloc_com_buf_with_cache()
{
	int32_t ret = 0, i = 0;
	uint64_t size = 4*1024*1024;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	int64_t flags_cached = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	hb_mem_common_buf_t com_buf = {0, };
	hb_mem_common_buf_t com_buf_cached = {0, };
	int64_t ts0, ts1;
	hb_mem_common_buf_t info = {0, };

	printf("=================================================\n");
	printf("Ready to sample_alloc_com_buf_with_cache\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_com_buf(size, flags, &com_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_alloc_com_buf(size, flags_cached, &com_buf_cached);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	// do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_get_com_buf(com_buf_cached.fd, &info);
	if (ret != 0) {
		printf("hb_mem_get_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_flush_buf((uint64_t)com_buf_cached.fd, 0, com_buf_cached.size);
	if (ret != 0) {
		printf("hb_mem_flush_buf_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_flush_buf_with_vaddr((uint64_t)com_buf_cached.virt_addr, com_buf_cached.size);
	if (ret != 0) {
		printf("hb_mem_flush_buf_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_invalidate_buf(com_buf_cached.fd, 0, com_buf_cached.size);
	if (ret != 0) {
		printf("hb_mem_invalidate_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_invalidate_buf_with_vaddr((uint64_t)com_buf_cached.virt_addr, com_buf_cached.size);
	if (ret != 0) {
		printf("hb_mem_invalidate_buf_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ts0 = get_time_us();
	for (i = 0; i < com_buf.size; i++)
		com_buf.virt_addr[i] = random();
	ts0 = get_time_us() - ts0;
	ts1 = get_time_us();
	for (i = 0; i < com_buf_cached.size; i++)
		com_buf_cached.virt_addr[i] = random();
	ts1 = get_time_us() - ts1;
	printf("memset uncached buf(size:%ld) use time: %ld\n", com_buf.size, ts0);
	printf("memset   cached buf(size:%ld) use time: %ld\n", com_buf_cached.size, ts1);

	ret = hb_mem_free_buf(com_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_free_buf(com_buf_cached.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	hb_mem_module_close();
	printf("sample_alloc_com_buf_with_cache done\n");
	printf("=================================================\n");

	return 0;
}

int sample_alloc_com_buf_with_heapmask()
{
	int32_t ret;
	uint64_t size = 1024*1024;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	int32_t i = 0;
	int64_t tmp_flags = 0;
	hb_mem_common_buf_t com_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_alloc_com_buf_with_heapmask\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	for (i = 0; i < MAX_HEAP_NUM; i++) {
		tmp_flags = flags | avail_heap_flag[i];
		printf("alloc com buf form %s, size: %lu\n", get_heap_name(avail_heap_flag[i]), size);
		ret = hb_mem_alloc_com_buf(size, tmp_flags, &com_buf);
		if (ret != 0) {
			printf("hb_mem_alloc_com_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		do_sys_command(tmp_flags & HB_MEM_USAGE_PRIV_MASK);
		printf("free com buf\n\n");
		ret = hb_mem_free_buf(com_buf.fd);
		if (ret != 0) {
			printf("hb_mem_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		// do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	}

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_alloc_com_buf_with_heapmask done\n");
	printf("=================================================\n");

	return 0;
}

int sample_alloc_graph_buf()
{
	int32_t ret = 0;
	int32_t w = 1280, h = 720, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_graphic_buf_t graph_buf = {0, };
	hb_mem_graphic_buf_t info = {0, };

	printf("=================================================\n");
	printf("Ready to sample_alloc_graph_buf\n");

	// open module
	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	// test output informations
	ret = hb_mem_alloc_graph_buf(w, h, format, flags, stride, vstride, &graph_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("graph_buf.plane_cnt: %d, graph_buf.format: %d, graph_buf.width: %d, graph_buf.height: %d, "
		   "graph_buf.stride: %d, graph_buf.vstride: %d, graph_buf.stride: %d, graph_buf.flags: %ld\n",
		   graph_buf.plane_cnt, graph_buf.format, graph_buf.width, graph_buf.height,
		   graph_buf.stride, graph_buf.vstride, graph_buf.stride, graph_buf.flags);
	for (int j = 0; j < graph_buf.plane_cnt; j++) {
		hb_mem_get_graph_buf(graph_buf.fd[j], &info);
	}
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_free_buf(graph_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	// do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_alloc_graph_buf done\n");
	printf("=================================================\n");

	return 0;
}

int sample_alloc_graph_buf_with_heapmask()
{
	int32_t ret = 0, i = 0;
	int64_t tmp_flags = 0;
	int32_t w = 1280, h = 720, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_graphic_buf_t graph_buf = {0, };
	hb_mem_graphic_buf_t info = {0, };

	printf("=================================================\n");
	printf("Ready to sample_alloc_graph_buf_with_heapmask\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	for (i = 0; i < MAX_HEAP_NUM; i++) {
		tmp_flags = flags | avail_heap_flag[i];
		printf("alloc graph buf form %s\n", get_heap_name(avail_heap_flag[i]));
		ret = hb_mem_alloc_graph_buf(w, h, format, tmp_flags, stride, vstride, &graph_buf);
		if (ret != 0) {
			printf("hb_mem_alloc_graph_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		printf("graph_buf.plane_cnt: %d, graph_buf.format: %d, graph_buf.width: %d, graph_buf.height: %d, "
			"graph_buf.stride: %d, graph_buf.vstride: %d, graph_buf.stride: %d, graph_buf.flags: %ld\n",
			graph_buf.plane_cnt, graph_buf.format, graph_buf.width, graph_buf.height,
			graph_buf.stride, graph_buf.vstride, graph_buf.stride, graph_buf.flags);
		for (int j = 0; j < graph_buf.plane_cnt; j++) {
			hb_mem_get_graph_buf(graph_buf.fd[j], &info);
		}
		do_sys_command(tmp_flags & HB_MEM_USAGE_PRIV_MASK);
		ret = hb_mem_free_buf(graph_buf.fd[0]);
		if (ret != 0) {
			printf("hb_mem_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
	}

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}

	printf("sample_alloc_graph_buf_with_heapmask done\n");
	printf("=================================================\n");

	return ret;
}

int sample_alloc_graph_buf_group()
{
	hb_mem_graphic_buf_group_t graph_buf = {0, };
	hb_mem_graphic_buf_group_t info = {0, };
	uint32_t bitmap = 0xFF;
	int32_t w[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t h[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t format[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t stride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t vstride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int64_t flags[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t ret = 0, i;

	printf("=================================================\n");
	printf("Ready to sample_alloc_graph_buf_with_heapmask\n");

	// open module
	hb_mem_module_open();

	// test DMA heap type
	for (i = 0; i < HB_MEM_MAXIMUM_GRAPH_BUF; i++) {
		if (bitmap & (1u << i)) {
			w[i] = 1280;
			h[i] = 720;
			format[i] = MEM_PIX_FMT_NV12;
			flags[i] = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;;
			stride[i] = 0;
			vstride[i] = 0;
		}
	}

	ret = hb_mem_alloc_graph_buf_group(w, h, format, flags, stride, vstride, &graph_buf, bitmap);
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf_group failed\n");
		return ret;
	}
	hb_mem_get_graph_buf_group(graph_buf.graph_group[0].fd[0], &info);
	do_sys_command(flags[0] & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_free_buf(graph_buf.graph_group[0].fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	hb_mem_module_close();
	printf("sample_alloc_graph_buf_group done\n");
	printf("=================================================\n");

	return ret;
}

int sample_alloc_graph_buf_group_heapmask()
{
	hb_mem_graphic_buf_group_t graph_buf = {0, };
	hb_mem_graphic_buf_group_t info = {0, };
	uint32_t bitmap = 0xFF;
	int32_t w[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t h[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t format[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t stride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t vstride[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int64_t flags[HB_MEM_MAXIMUM_GRAPH_BUF] = {0, };
	int32_t ret = 0, i, j;

	printf("=================================================\n");
	printf("Ready to sample_alloc_graph_buf_group_heapmask\n");

	// open module
	hb_mem_module_open();

	// test DMA heap type
	for (i = 0; i < HB_MEM_MAXIMUM_GRAPH_BUF; i++) {
		if (bitmap & (1u << i)) {
			w[i] = 1280;
			h[i] = 720;
			format[i] = MEM_PIX_FMT_NV12;
			flags[i] = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;;
			stride[i] = 0;
			vstride[i] = 0;
		}
	}
	for (i = 0; i < MAX_HEAP_NUM; i++) {
		for (j = 0; j < HB_MEM_MAXIMUM_GRAPH_BUF; j++) {
			if (bitmap & (1u << j)) {
				flags[j] = flags[j] | avail_heap_flag[i];
			}
		}
		printf("alloc graph buf group form %s\n", get_heap_name(avail_heap_flag[i]));
		ret = hb_mem_alloc_graph_buf_group(w, h, format, flags, stride, vstride, &graph_buf, bitmap);
		if (ret != 0) {
			printf("hb_mem_alloc_graph_buf_group failed\n");
			return ret;
		}
		hb_mem_get_graph_buf_group(graph_buf.graph_group[0].fd[0], &info);
		do_sys_command(flags[0] & HB_MEM_USAGE_PRIV_MASK);
		ret = hb_mem_free_buf(graph_buf.graph_group[0].fd[0]);
		if (ret != 0) {
			printf("hb_mem_free_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
	}
	printf("sample_alloc_graph_buf_group_heapmask done\n");
	printf("=================================================\n");

	hb_mem_module_close();
	return ret;
}

int sample_change_com_graph_buf()
{
	int32_t w = 1920, h = 1080, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	uint64_t size = 1024 * 64; // 4k
	hb_mem_buffer_type_t type;
	hb_mem_common_buf_t com_buf = {0, };
	hb_mem_graphic_buf_t graph_buf = {0, };
	hb_mem_common_buf_t out_com_buf = {0, };
	hb_mem_graphic_buf_t out_graph_buf = {0, };
	int32_t ret = 0;

	printf("=================================================\n");
	printf("Ready to sample_alloc_graph_buf_group_heapmask\n");

	// open module
	hb_mem_module_open();

	flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hb_mem_alloc_graph_buf(w, h, format, flags, stride, vstride, &graph_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	printf("----alloc graphic buffer done!----\n");
	printf("the data in graphic buffer:\n");
	printf("size: 0x%lx\n", graph_buf.size[0]);
	printf("flags: 0x%lx\n", graph_buf.flags);
	printf("fd: %d\n", graph_buf.fd[0]);
	printf("share id: %d\n", graph_buf.share_id[0]);
	printf("virt_addr: 0x%p\n", graph_buf.virt_addr[0]);
	printf("paddr: 0x%lx\n", graph_buf.phys_addr[0]);

	//change graph buf to com buf, only continue graphic buffer support
	ret = hb_mem_get_buf_type_and_buf_with_vaddr((uint64_t)graph_buf.virt_addr[0], &type, &out_com_buf, NULL);
	if (ret != 0) {
		printf("hb_mem_get_buf_type_and_buf_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	printf("----change graphic buffer to common buffer done!----\n");
	printf("the data in new common buffer:\n");
	printf("size: 0x%lx\n", out_com_buf.size);
	printf("flags: 0x%lx\n", out_com_buf.flags);
	printf("fd: %d\n", out_com_buf.fd);
	printf("share id: %d\n", out_com_buf.share_id);
	printf("virt_addr: 0x%p\n", out_com_buf.virt_addr);
	printf("paddr: 0x%lx\n", out_com_buf.phys_addr);

	ret = hb_mem_alloc_com_buf(size, flags, &com_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	printf("----alloc common buffer done!----\n");
	printf("the data in common buffer:\n");
	printf("size: 0x%lx\n", com_buf.size);
	printf("flags: 0x%lx\n", com_buf.flags);
	printf("fd: %d\n", com_buf.fd);
	printf("share id: %d\n", com_buf.share_id);
	printf("virt_addr: 0x%p\n", com_buf.virt_addr);
	printf("paddr: 0x%lx\n", com_buf.phys_addr);

	//change com buf to graph buf
	ret = hb_mem_get_buf_type_and_buf_with_vaddr((uint64_t)com_buf.virt_addr, &type, NULL, &out_graph_buf);
	if (ret != 0) {
		printf("hb_mem_get_buf_type_and_buf_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	printf("----change common buffer to graphic buffer done!----\n");
	printf("the data in new graphic buffer:\n");
	printf("size: 0x%lx\n", out_graph_buf.size[0]);
	printf("flags: 0x%lx\n", out_graph_buf.flags);
	printf("fd: %d\n", out_graph_buf.fd[0]);
	printf("share id: %d\n", out_graph_buf.share_id[0]);
	printf("virt_addr: 0x%p\n", out_graph_buf.virt_addr[0]);
	printf("paddr: 0x%lx\n", out_graph_buf.phys_addr[0]);

	ret = hb_mem_free_buf(graph_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_free_buf(com_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	hb_mem_module_close();

	printf("sample_alloc_graph_buf_group_heapmask done\n");
	printf("=================================================\n");

	return ret;
}

int sample_com_buf_user_consume_cnt()
{
	int32_t ret = 0;
	uint64_t size = 4 * 1024 * 1024;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	hb_mem_common_buf_t com_buf = {0, };

	printf("=================================================\n");
	printf("Ready to sample_com_buf_user_consume_cnt\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	ret = hb_mem_alloc_com_buf(size, flags, &com_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("alloc com buf, share_id: %d\n", com_buf.share_id);
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	//inc/dec user consume with fd
	ret = hb_mem_dec_consume_cnt(com_buf.fd);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_dec_consume_cnt(com_buf.fd);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	//inc/dec user consume with vaddr
	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)com_buf.virt_addr);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)com_buf.virt_addr);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_free_buf(com_buf.fd);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("free com buf\n");
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);
	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}

	printf("sample_com_buf_user_consume_cnt done\n");
	printf("=================================================\n");

	return 0;
}

int sample_graph_buf_user_consume_cnt()
{
	int32_t ret = 0;
	int32_t w = 1280, h = 720, format = MEM_PIX_FMT_NV12, stride = 0, vstride = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_graphic_buf_t graph_buf = {0, };
	hb_mem_graphic_buf_t info = {0, };

	printf("=================================================\n");
	printf("Ready to sample_graph_buf_user_consume_cnt\n");

	// open module
	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	// test output informations
	ret = hb_mem_alloc_graph_buf(w, h, format, flags, stride, vstride, &graph_buf);
	if (ret != 0) {
		printf("hb_mem_alloc_graph_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	printf("graph_buf.plane_cnt: %d, graph_buf.format: %d, graph_buf.width: %d, graph_buf.height: %d, "
		   "graph_buf.stride: %d, graph_buf.vstride: %d, graph_buf.stride: %d, graph_buf.flags: %ld\n",
		   graph_buf.plane_cnt, graph_buf.format, graph_buf.width, graph_buf.height,
		   graph_buf.stride, graph_buf.vstride, graph_buf.stride, graph_buf.flags);
	for (int j = 0; j < graph_buf.plane_cnt; j++) {
		hb_mem_get_graph_buf(graph_buf.fd[j], &info);
	}
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	//inc/dec user consume with fd
	ret = hb_mem_dec_consume_cnt(graph_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_dec_consume_cnt(graph_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	//inc/dec user consume with vaddr
	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)graph_buf.virt_addr[0]);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_dec_consume_cnt_with_vaddr((uint64_t)graph_buf.virt_addr[0]);
	if (ret != 0) {
		printf("hb_mem_dec_consume_cnt_with_vaddr failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	ret = hb_mem_free_buf(graph_buf.fd[0]);
	if (ret != 0) {
		printf("hb_mem_free_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	// do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}
	printf("sample_graph_buf_user_consume_cnt done\n");
	printf("=================================================\n");

	return 0;
}
