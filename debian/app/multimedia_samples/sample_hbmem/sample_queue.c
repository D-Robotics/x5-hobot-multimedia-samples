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
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include <hb_mem_mgr.h>
#include <hb_mem_err.h>
#include "sample_common.h"

#define QUEUE_ITEM_CNT 16

typedef struct MemQueueTestContext {
	int32_t quit;
	hb_mem_buf_queue_t share_queue;
} MemQueueTestContext;

static void consumer_thread(void *arg)
{
	MemQueueTestContext *ctx = (MemQueueTestContext *)arg;
	int32_t slot, ret;
	int64_t timeout = 6000; // 6s
	hb_mem_common_buf_t out_buf = {0, };
	hb_mem_common_buf_t out_info = {0, };
	uint64_t startTime, endTime, test_time = 1000000;// 6s

	startTime = get_time_us();
	printf("[%d:%ld] Start test %lu.\n", getpid(), gettid(), startTime);
	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return;
	}

	while (ctx->quit == 0) {
		ret = hb_mem_request_buf(&ctx->share_queue, &slot, &out_buf, timeout);
		// do test to the buffer
		ret = hb_mem_get_com_buf(out_buf.fd, &out_info);
		printf("[%d:%ld] out buffer share id %d.\n", getpid(), gettid(), out_buf.share_id);
		ret = hb_mem_release_buf(&ctx->share_queue, slot);
		usleep(10000); // 100ms

		endTime = get_time_us();
		if (endTime - startTime > test_time) {
			ctx->quit = 1;
			printf("[%d:%ld] End test %lu, interval %lu.\n", getpid(), gettid(), endTime,
				endTime - startTime);
		}
	}

	ret = hb_mem_module_close();
	if (ret != 0) {
		printf("hb_mem_module_close failed\n");
		return;
	}

	printf("[%d:%ld] thread quits.\n", getpid(), gettid());
}

int sample_queue_producer_consumer()
{
	uint64_t size = 1024 * 256; // 4k
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	hb_mem_common_buf_t com_buf[QUEUE_ITEM_CNT] = {0, };
	hb_mem_common_buf_t out_info = {0, };
	pthread_t thread_id;
	MemQueueTestContext *ctx;
	void* retVal;
	int32_t i, slot, ret;
	int64_t timeout = 6000; // 6s

	printf("=================================================\n");
	printf("Ready to sample_queue_producer_consumer\n");

	ctx = (MemQueueTestContext *)malloc(sizeof(*ctx));
	memset(ctx, 0x00, sizeof(*ctx));

	// test open module again
	printf("[%d:%ld] Queue test case.\n", getpid(), gettid());
	ret = hb_mem_module_open();
	if (ret != 0) {
		printf("hb_mem_module_open failed\n");
		return ret;
	}

	for (i = 0; i < QUEUE_ITEM_CNT; i++) {
		ret = hb_mem_alloc_com_buf(size, flags, &com_buf[i]);
		if (ret != 0) {
			printf("hb_mem_alloc_com_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
	}
	ctx->share_queue.count = QUEUE_ITEM_CNT;
	ctx->share_queue.item_size = sizeof(com_buf[0]);
	ret = hb_mem_create_buf_queue(&ctx->share_queue);
	if (ret != 0) {
		printf("hb_mem_create_buf_queue failed\n");
		(void)hb_mem_module_close();
		return ret;
	}

	pthread_create(&thread_id, NULL, (void* (*)(void*))consumer_thread, ctx);

	while (ctx->quit == 0) {
		ret = hb_mem_dequeue_buf(&ctx->share_queue, &slot, &out_info, timeout);
		if (ret != 0) {
			printf("hb_mem_dequeue_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		ret = hb_mem_queue_buf(&ctx->share_queue, slot, &com_buf[slot]);
		if (ret != 0) {
			printf("hb_mem_queue_buf failed\n");
			(void)hb_mem_module_close();
			return ret;
		}
		usleep(10000); // 100ms
	}
	pthread_join(thread_id, &retVal);
	printf("[%d:%ld] thread return = %ld\n", getpid(), gettid(), (intptr_t)retVal);

	for (i = 0; i < QUEUE_ITEM_CNT; i++) {
		ret = hb_mem_free_buf(com_buf[i].fd);
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

	free(ctx);
	printf("sample_queue_producer_consumer done\n");
	printf("=================================================\n");
	return 0;
}
