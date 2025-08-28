/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "common_utils.h"
#include <time.h>
#include <sys/time.h>

const char *g_module_name[] = {
	"sif",
	"isp",
	"vse",
	"gdc",
	"codec",
	"n2d",
};

const char *get_module_name(int32_t mni)
{
	if (mni >= SIF_MNI && mni <= N2D_MNI)
		return g_module_name[mni];
	else
		return NULL;
}

int32_t runtime_end(uvc_camera_context *context)
{
	time_t now = time(NULL);
	time_t start = context->work_info.start;
	time_t run = (time_t)context->run_time;

	if (run > 0 && difftime(now, start + run) > 0) {
		printf("run time use up\n");
		return 1;
	}

	if (context->loop_cnt > 0 && (context->work_info.remaining_loop-- <= 0)) {
		printf("loop cnt use up\n");
		return 1;
	}

	return 0;
}

void runtime_start(uvc_camera_context *context)
{
	context->work_info.remaining_loop = context->loop_cnt;
	printf("loop_cnt: %d\n",context->work_info.remaining_loop);
	context->work_info.start = time(NULL);
}

int dumpToFile(char *filename, char *srcBuf, unsigned int size)
{
	FILE *yuvFd = NULL;
	char *buffer = NULL;

	yuvFd = fopen(filename, "w+");

	if (yuvFd == NULL) {
		printf("ERRopen(%s) fail", filename);
		return -1;
	}

	buffer = (char *)malloc(size);

	if (buffer == NULL) {
		printf(":malloc file");
		fclose(yuvFd);
		return -1;
	}

	memcpy(buffer, srcBuf, size);

	fflush(stdout);

	fwrite(buffer, 1, size, yuvFd);

	fflush(yuvFd);

	if (yuvFd)
		fclose(yuvFd);
	if (buffer)
		free(buffer);

	printf("filedump(%s, size(%d) is successed\n", filename, size);

	return 0;
}

int dumpToFile2plane(char *filename, char *srcBuf, char *srcBuf1, unsigned int size, unsigned int size1)
{
	FILE *yuvFd = NULL;
	char *buffer = NULL;

	yuvFd = fopen(filename, "w+");

	if (yuvFd == NULL) {
		printf("open(%s) fail", filename);
		return -1;
	}

	buffer = (char *)malloc(size + size1);

	if (buffer == NULL) {
		printf("ERR:malloc file");
		fclose(yuvFd);
		return -1;
	}

	memcpy(buffer, srcBuf, size);
	memcpy(buffer + size, srcBuf1, size1);

	fflush(stdout);

	fwrite(buffer, 1, size + size1, yuvFd);

	fflush(yuvFd);

	if (yuvFd)
		fclose(yuvFd);
	if (buffer)
		free(buffer);

	printf("filedump(%s, size(%d-%d) is successed\n", filename, size, size1);

	return 0;
}

// hbmem
static pthread_mutex_t ion_lock = PTHREAD_MUTEX_INITIALIZER; /*PRQA S 3004*/
static uint32_t g_ion_opened = 0;
static int32_t m_ionClient = -1;

int32_t vpm_hb_mem_init(void)
{
	uint32_t v_major, V_minor, v_patch;

	pthread_mutex_lock(&ion_lock);
	if (g_ion_opened == 0u) {
		m_ionClient = hb_mem_module_open();
		if (m_ionClient < 0) {
			printf("hb_mem_module_open failed ret %d\n", m_ionClient);
			m_ionClient = -1;
			g_ion_opened = 0;
			pthread_mutex_unlock(&ion_lock);
			return -1;
		}
		hb_mem_get_version(&v_major, &V_minor, &v_patch);
		printf("hb_mem_module_open success version: %u.%u.%u\n", v_major, V_minor, v_patch);
		g_ion_opened = 1;
	} else {
		g_ion_opened++;
		printf("skip hb mem open count %d.\n", g_ion_opened);
	}
	pthread_mutex_unlock(&ion_lock);
	return 0;
}

void vpm_hb_mem_deinit(void)
{
	int32_t ret;

	pthread_mutex_lock(&ion_lock);
	if (g_ion_opened > 0u) {
		g_ion_opened--;
		printf("Try release hb mem  open count %d.\n", g_ion_opened);
	}

	if ((g_ion_opened == 0u) && (m_ionClient == 0)) {
		ret = hb_mem_module_close();
		if (ret < 0) {
			printf("hb_mem_module_close failed ret %d\n", ret);
		} else {
			printf("vpm release hb mem done.\n");
		}
		m_ionClient = -1;
		g_ion_opened = 0;
	}
	pthread_mutex_unlock(&ion_lock);
	return;
}
