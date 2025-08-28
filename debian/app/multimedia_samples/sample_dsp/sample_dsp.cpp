// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "hb_dsp_mgr.h"
#include "hb_ipcf_hal.h"

#define TIMEOUT             (1000)
#define HORIZON_ALGO_SAMPLE (0)
#define HIFI_FFT_CPLX_32x32 (1)

#define SMP_RUN_TIME (10) /* sample run time, unit: s */
#define IPC_CHAN_NUM (2)
#define PTHREAD_TIMEOUT (100000000) /* cond wait time, unit: ns */
#define USEC (1000000000)

pthread_mutex_t ipcf_mutex;
pthread_cond_t ipcf_cond;
pthread_t thread_id[IPC_CHAN_NUM];

struct dsp_call_params_s {
	char cmd[40];
	uint32_t type;
	uint32_t buf_width;
	uint32_t buf_height;
	uint64_t dsp_buf0;
	uint64_t dsp_buf1;
};

struct thread_dsp_params {
	bool is_enable;
	ipcfhal_chan_t chan;
	struct dsp_call_params_s dsp_call_params;
};

static const char *dsp_path = "/usr/hobot/lib/firmware/";
static int32_t algo_type = 1;
static const char *dsp_name = "adsp";

volatile sig_atomic_t g_running = true;

static const char short_options[] = "p:t:n:h";
static const struct option long_options[] = {
	{ "dsp_path", 1, 0, 'p' },
	{ "algo_type", 1, 0, 't' },
	{ "dsp_name", 1, 0, 'n' },
	{ "help", 0, 0, 'h' },
	{},
};

static void usage(const char *prog)
{
	printf("Usage: %s \n", prog);
	printf(
		"\t-p --dsp_path         Specifying firmware paths\n"
		"\t-t --algo_type        Specifying algo type\n"
		"\t-n --dsp_name         Specifying dsp name\n"
		"\t-h --help             print usage\n"
		"Example: %s -p /usr/hobot/lib/firmware\n", prog
	);
	exit(1);
}

void signal_handler(int sig) {
	if (sig == SIGINT)
		g_running = false;
}

static void dump_dsp_call_params_s(struct dsp_call_params_s *params)
{
	pr_info("dsp_call_params_s:\n");
	pr_info("\tcmd:%s\n", params->cmd);
	pr_info("\ttype:%d\n", params->type);
	pr_info("\tbuf_width:%d\n", params->buf_width);
	pr_info("\tbuf_height:%d\n", params->buf_height);
	pr_info("\tdsp_buf0:0x%lx\n", params->dsp_buf0);
	pr_info("\tdsp_buf1:0x%lx\n", params->dsp_buf1);
}

static void parse_opts(int argc, char **argv) {
	while (1) {
		int ret;
		ret = getopt_long(argc, argv, short_options, long_options, NULL);
		if (ret == -1)
			break;

		switch (ret) {
		case 'p':
			dsp_path = optarg;
			break;
		case 't':
			algo_type = atoi(optarg);
			break;
		case 'n':
			dsp_name = optarg;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	pr_info("dsp params:\n");
	pr_info("\tdsp_path:%s\n", dsp_path);
	pr_info("\talgo_type:%d\n", algo_type);
	pr_info("\tdsp_name:%s\n", dsp_name);
}

void init_pthread() {
	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	pthread_mutex_init(&ipcf_mutex, NULL);
	pthread_cond_init(&ipcf_cond, &attr);
	pthread_condattr_destroy(&attr);
}

void dsp_bufs_setup(char *buf_src, char *buf_dst, uint32_t buf_width, uint32_t buf_height, ssize_t size) {
	memset(buf_src, 0x80, size);
	memset(buf_dst, 0x80, size);

	for (size_t h_idx = 0; h_idx < buf_height; h_idx++) {
		for(size_t w_idx = 0; w_idx < buf_width; w_idx++) {
			buf_src[buf_width * h_idx + w_idx] = w_idx;
		}
	}
}

static void *recv_dsp_data(void *args) {
	struct thread_dsp_params *dsp_args = (struct thread_dsp_params*)args;
	ipcfhal_chan_t *chan = &dsp_args->chan;
	uint8_t recv_data[128] = {0};
	int32_t ret = 0;

	while (dsp_args->is_enable) {
		ret = hb_ipcfhal_recv(recv_data, 128, TIMEOUT, chan);
		if (ret < 0) {
			pr_err("ipcfhal recv error %d\n", ret);
			return NULL;
		}

		pthread_mutex_lock(&ipcf_mutex);
		pthread_cond_signal(&ipcf_cond);
		pthread_mutex_unlock(&ipcf_mutex);
	}

	return NULL;
}

static void *send_dsp_data(void *args) {
	struct thread_dsp_params *dsp_args = (struct thread_dsp_params*)args;
	int32_t data_size = sizeof(dsp_args->dsp_call_params);
	ipcfhal_chan_t *chan = &dsp_args->chan;
	uint8_t *data = (uint8_t *)&dsp_args->dsp_call_params;
	int32_t ret = 0;
	struct timespec ts;

	while (dsp_args->is_enable) {
		ret = hb_ipcfhal_send(data, data_size, chan);
		if (ret < 0) {
			pr_err("ipcfhal send error\n");
			return NULL;
		}

		clock_gettime(CLOCK_MONOTONIC, &ts);
		ts.tv_nsec += PTHREAD_TIMEOUT;
		if (ts.tv_nsec >= USEC) {
			ts.tv_sec += 1;
			ts.tv_nsec -= USEC;
		}

		pthread_mutex_lock(&ipcf_mutex);
		pthread_cond_timedwait(&ipcf_cond, &ipcf_mutex, &ts);
		pthread_mutex_unlock(&ipcf_mutex);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	char exec[256];
	char pathname[256];
	const char *json_path = "/app/platform_samples/sample_dsp/sample_confg.json";
	const char *chan_name[IPC_CHAN_NUM] = {
		"cpu2dsp_ins2ch0",
		"cpu2dsp_ins2ch1",
	};
	int32_t ret = 0;
	void *status;

	struct timespec start_time;
	struct timespec cur_time;

	int32_t dsp_id = 0;
	int32_t timeout = 5;
	uint64_t input_va, output_va;
	uint64_t input_iova, output_iova;
	int64_t hbmem_flag = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_PRIV_HEAP_2_RESERVED;
	uint32_t width = 640;
	uint32_t height = 480;
	uint64_t buf_size = width *height;
	struct thread_dsp_params dsp_args[IPC_CHAN_NUM];
	struct dsp_call_params_s dsp_call_params;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	parse_opts(argc, argv);
	snprintf(exec, 256, "echo -n \"%s\" > /sys/module/firmware_class/parameters/path", dsp_path);
	system(exec);

	dsp_call_params.buf_width = width;
	dsp_call_params.buf_height = height;
	dsp_call_params.type = algo_type;
	if (algo_type == HORIZON_ALGO_SAMPLE)
		snprintf(dsp_call_params.cmd, sizeof(dsp_call_params.cmd), "horizon-algo-sample");
	else if (algo_type == HIFI_FFT_CPLX_32x32) {
		snprintf(dsp_call_params.cmd, sizeof(dsp_call_params.cmd), "hifi-fft-cplx");
	} else {
		snprintf(dsp_call_params.cmd, sizeof(dsp_call_params.cmd), "undefine-cmd");
	}

	init_pthread();

	// install signal handler
	signal(SIGINT, signal_handler);

	// dsp init
	ret = hb_dsp_init(dsp_id);
	if (ret < 0) {
		return ret;
	}

	// hbmem alloc
	ret = hb_dsp_mem_alloc(dsp_id, buf_size, hbmem_flag, &input_va, &input_iova);
	if (ret < 0)
		goto __failed_to_end1;
	ret = hb_dsp_mem_alloc(dsp_id, buf_size, hbmem_flag, &output_va, &output_iova);
	if (ret < 0)
		goto __failed_to_end2;
	dsp_bufs_setup((char *)input_va, (char *)output_va, width, height, buf_size);
	dsp_call_params.dsp_buf0 = input_iova;
	dsp_call_params.dsp_buf1 = output_iova;
	dump_dsp_call_params_s(&dsp_call_params);

	// ipc init
	for (int32_t i = 0; i < IPC_CHAN_NUM; i++) {
		ret = hb_ipcfhal_getchan_byjson(chan_name[i], &dsp_args[i].chan, json_path);
		if (ret < 0) {
			pr_err("%s not found, ret %d\n", chan_name[i], ret);
			goto __failed_to_end3;
		}

		ret = hb_ipcfhal_init(&dsp_args[i].chan);
		if (ret < 0) {
			pr_err("%s init failed, ret %d\n", chan_name[i], ret);
			goto __failed_to_end3;
		}

		dsp_args[i].dsp_call_params = dsp_call_params;
		dsp_args[i].is_enable = true;
	}

	// dsp start
	snprintf(pathname, sizeof(pathname), "%s%s", dsp_path, dsp_name);
	ret = hb_dsp_start(dsp_id, timeout, pathname);
	if (ret) {
		pr_err("hb_dsp_start failed, ret %d\n", ret);
		goto __failed_to_end4;
	}

	pthread_create(&thread_id[0], NULL, send_dsp_data, &dsp_args[0]);
	pthread_create(&thread_id[1], NULL, recv_dsp_data, &dsp_args[1]);

	clock_gettime(CLOCK_MONOTONIC, &start_time);
	while (g_running) {
		clock_gettime(CLOCK_MONOTONIC, &cur_time);
		if (cur_time.tv_sec - start_time.tv_sec > SMP_RUN_TIME) {
			break;
		}
		sleep(1);
	}

__failed_to_end4:
	for (int32_t i = 0; i < IPC_CHAN_NUM; i++) {
		if (thread_id[i]) {
			dsp_args[i].is_enable = false;
			pthread_join(thread_id[i], &status);
		}
		ret = hb_ipcfhal_deinit(&dsp_args[i].chan);
	}
	ret = hb_dsp_stop(dsp_id);
	pthread_cond_destroy(&ipcf_cond);
	pthread_mutex_destroy(&ipcf_mutex);
__failed_to_end3:
	ret = hb_dsp_mem_free(dsp_id, output_va);
__failed_to_end2:
	ret = hb_dsp_mem_free(dsp_id, input_va);
__failed_to_end1:
	ret = hb_dsp_deinit(dsp_id);

	return ret;
}
