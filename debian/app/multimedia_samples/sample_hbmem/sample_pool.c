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

// TEST_F(MemPoolTest, test_hb_mem_pool_alloc_buf) {
int sample_pool()
{
	uint64_t i, num = 10, size = 1024 * 1024 * 10, alloc_size; // 4M
	int32_t ret = 0;
	int64_t flags = HB_MEM_USAGE_CPU_READ_NEVER;
	hb_mem_common_buf_t com_buf = {0, };
	hb_mem_common_buf_t out_buf[10] = {0, };
	hb_mem_pool_t pool = {0, };

	printf("=================================================\n");
	printf("Ready to sample_pool\n");

	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}
	flags = HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN ;
	ret = hb_mem_pool_create(size, flags, &pool);
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	ret = hb_mem_get_com_buf(pool.fd, &com_buf);
	if (ret != 0) {
		printf("hb_mem_get_com_buf failed\n");
		(void)hb_mem_module_close();
		return ret;
	}
	do_sys_command(flags & HB_MEM_USAGE_PRIV_MASK);

	alloc_size = pool.page_size/num;
	for (i = 0; i < num; i++) {
		ret = hb_mem_pool_alloc_buf(pool.fd, alloc_size, &out_buf[i]);
		if (ret != 0) {
			printf("hb_mem_pool_alloc_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
	}

	ret = hb_mem_pool_destroy(pool.fd);
	if (ret == HB_MEM_ERR_POOL_BUSY) {
		printf("hb_mem_pool_destroy HB_MEM_ERR_POOL_BUSY, need free first.\n");
		for (i = 0; i < num; i++) {
			ret = hb_mem_pool_free_buf((uint64_t)out_buf[i].virt_addr);
			if (ret != 0) {
				printf("hb_mem_pool_free_buf failed\n");
				(void)hb_mem_module_close();
				return ret;
			}
		}

		ret = hb_mem_pool_destroy(pool.fd);
		if (ret != 0) {
			printf("hb_mem_pool_destroy failed %d\n", ret);
			(void)hb_mem_module_close();
			return ret;
		}
	}

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return ret;
	}

	printf("sample_pool done\n");
	printf("=================================================\n");

	return 0;
}


