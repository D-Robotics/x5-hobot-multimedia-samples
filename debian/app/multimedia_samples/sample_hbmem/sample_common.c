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
#include <hb_mem_mgr.h>
#include <hb_mem_err.h>
#include "sample_common.h"

uint64_t get_time_us(void)
{
	struct timespec tp;

	clock_gettime(CLOCK_MONOTONIC, &tp);

	return ((uint64_t)tp.tv_sec*1000000 + tp.tv_nsec/1000);
}

char * get_heap_name(int64_t heap_mask)
{
	if (heap_mask == HB_MEM_USAGE_PRIV_HEAP_DMA) {
		return "ion_cma";
	} else if (heap_mask == HB_MEM_USAGE_PRIV_HEAP_RESERVED) {
		return "carveout";
	} else if (heap_mask == HB_MEM_USAGE_PRIV_HEAP_2_RESERVED) {
		return "cma_reserved";
	} else {
		return "*";
	}
}

int32_t get_ycbcr_info(int32_t w, int32_t h, int32_t format,
			int32_t stride, int32_t vstride,
			int32_t *planes, size_t *luma_size, size_t *chroma_size,
			size_t * total_size)
{
	int32_t ret = 0;
	int32_t width, height;
	width = stride > w ? stride : w;
	height = vstride > h ? vstride : h;

	switch (format) {
		case MEM_PIX_FMT_YUV420P:
			*luma_size = width * height;
			*chroma_size = *luma_size / 4;
			*planes = 3;
			break;
		case MEM_PIX_FMT_NV12:
		case MEM_PIX_FMT_NV21:
			*luma_size = width * height;
			*chroma_size = *luma_size / 2;
			*planes = 2;
			break;
		case MEM_PIX_FMT_YUV422P:
			*luma_size = width * height;
			*chroma_size = *luma_size / 2;
			*planes = 3;
			break;
		case MEM_PIX_FMT_NV16:
		case MEM_PIX_FMT_NV61:
			*luma_size = width * height;
			*chroma_size = *luma_size;
			*planes = 2;
			break;
		case MEM_PIX_FMT_YUYV422:
		case MEM_PIX_FMT_YVYU422:
		case MEM_PIX_FMT_UYVY422:
		case MEM_PIX_FMT_VYUY422:
			*luma_size = width * height * 2;
			*planes = 1;
			break;
		case MEM_PIX_FMT_YUV444:
			*luma_size = width * height * 3;
			*planes = 1;
			break;
		case MEM_PIX_FMT_YUV444P:
			*luma_size = width * height;
			*chroma_size = *luma_size;
			*planes = 3;
			break;
		case MEM_PIX_FMT_NV24:
		case MEM_PIX_FMT_NV42:
			*luma_size = width * height;
			*chroma_size = *luma_size * 2;
			*planes = 2;
			break;
		case MEM_PIX_FMT_YUV440P:
			*luma_size = width * height;
			*chroma_size = *luma_size / 2;
			*planes = 3;
			break;
		case MEM_PIX_FMT_YUV400:
			*luma_size = width * height;
			*planes = 1;
			break;
		default:
			*luma_size = 0;
			*planes = 0;
			break;
	}

	if (*planes != 0) {
		*total_size = *luma_size + (*planes -1) * (*chroma_size);
	} else {
		*total_size = 0;
	}

	return ret;
}

int32_t do_sys_command(int64_t heap_mask)
{
	int32_t ret;
	char sys_cmd[256];

	snprintf(sys_cmd, sizeof(sys_cmd),
		"cat /sys/kernel/debug/ion/clients/%d*", getpid());
	printf("[%d:%ld] Do system command %s.\n", getpid(), gettid(), sys_cmd);
	ret = system(sys_cmd);
	snprintf(sys_cmd, sizeof(sys_cmd),
		"cat /sys/kernel/debug/ion/heaps/%s | grep -w %d | grep -w %s",
		get_heap_name(heap_mask), getpid(), PID_NAME);
	printf("[%d:%ld] Do system command %s.\n", getpid(), gettid(), sys_cmd);
	ret = system(sys_cmd);
	printf("[%d:%ld] Result %d.\n", getpid(), gettid(), ret);

	return ret;
}

