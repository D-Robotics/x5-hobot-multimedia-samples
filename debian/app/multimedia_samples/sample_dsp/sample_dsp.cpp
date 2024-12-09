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

struct dsp_call_params_s {
	char cmd[40];
	uint32_t type;
	uint32_t buf_width;
	uint32_t buf_height;
	uint64_t dsp_buf0;
	uint64_t dsp_buf1;
};

static const char *dsp_path = "/usr/hobot/lib/firmware";
static int32_t algo_type = 1;
static const char *dsp_name = "adsp";

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

void dsp_bufs_setup(char *buf_src, char *buf_dst, uint32_t buf_width, uint32_t buf_height, ssize_t size) {
	memset(buf_src, 0x80, size);
	memset(buf_dst, 0x80, size);

	for (size_t h_idx = 0; h_idx < buf_height; h_idx++) {
		for(size_t w_idx = 0; w_idx < buf_width; w_idx++) {
			buf_src[buf_width * h_idx + w_idx] = w_idx;
		}
	}
}

int32_t dsp_call(uint8_t *data, ssize_t data_size, ipcfhal_chan_t *chan) {
	int32_t ret = 0;
	uint8_t recv_data[data_size] = {0};

	ret = hb_ipcfhal_send(data, data_size, chan);
	if (ret < 0) {
		pr_err("ipcfhal send error\n");
		return ret;
	}

	ret = hb_ipcfhal_recv(recv_data, data_size, TIMEOUT, chan);
	if (ret < 0) {
		pr_err("ipcfhal recv error %d\n", ret);
		return ret;
	}

	return 0;
}

int32_t dsp_sample() {
	int32_t ret = 0;
	char pathname[256] = "/usr/hobot/lib/firmware/adsp";
	int32_t status = 0;
	int32_t dsp_id = 0;
	uint32_t width = 640;
	uint32_t height = 480;
	int32_t timeout = 5;
	int64_t hbmem_flag = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_PRIV_HEAP_2_RESERVED;
	uint64_t buf_size = width * height;
	uint64_t input_va, output_va;
	uint64_t input_iova, output_iova;

	ipcfhal_chan_t chan;
	const char *json_path = "/app/platform_samples/sample_dsp/sample_confg.json";
	const char *chan_name = "cpu2dsp_ins2ch0";

	ret = hb_dsp_init(dsp_id);
	if (ret < 0) {
		pr_err("hb_dsp_init failed, return ret %d\n", ret);
		return ret;
	}

	ret = hb_dsp_mem_alloc(dsp_id, buf_size, hbmem_flag, &input_va, &input_iova);
	if (ret) {
		pr_err("hbmem input failed, return ret 0x%x\n", ret);
		return ret;
	}

	ret = hb_dsp_mem_alloc(dsp_id, buf_size, hbmem_flag, &output_va, &output_iova);
	if (ret) {
		pr_err("hbmem output failed, return ret 0x%x\n", ret);
		return ret;
	}

	pr_debug("input va:0x%lx iova:0x%lx, output va:0x%lx iova:0x%lx\n",
			input_va, input_iova, output_va, output_iova);

	dsp_bufs_setup((char *)input_va, (char *)output_va, width, height, buf_size);

	ret = hb_ipcfhal_getchan_byjson(chan_name, &chan, json_path);
	if (ret < 0) {
		pr_err("parse json failed %d\n", ret);
		return ret;
	}

	ret = hb_ipcfhal_init(&chan);
	if (ret < 0) {
		pr_err("ipcfhal init error\n");
		return ret;
	}

	strcpy(pathname, dsp_path);
	strcat(pathname, dsp_name);
	pr_info("pathname %s\n", pathname);
	ret = hb_dsp_start(dsp_id, timeout, pathname);
	if (ret < 0) {
		pr_err("hb_dsp_start failed return ret %d\n", ret);
		return ret;
	}

	struct dsp_call_params_s dsp_call_params = {
		.cmd = "horizon-algo-sample",
		.type = HORIZON_ALGO_SAMPLE,
		.buf_width = width,
		.buf_height = height,
		.dsp_buf0 = input_iova,
		.dsp_buf1 = output_iova,
	};
	dsp_call_params.type = algo_type;
	dump_dsp_call_params_s(&dsp_call_params);

	ret = hb_dsp_get_status(dsp_id, &status);

	ret = dsp_call((uint8_t *)&dsp_call_params, sizeof(dsp_call_params), &chan);
	if (ret) {
		pr_err("dsp call fail:%d\n", ret);
		return ret;
	}

	ret = hb_ipcfhal_deinit(&chan);
	if (ret < 0) {
		pr_err("ipcfhal deinit error\n");
		return ret;
	}

	ret = hb_dsp_stop(dsp_id);
	if (ret) {
		pr_err("dsp stop fail:%d\n", ret);
		return ret;
	}

	ret = hb_dsp_mem_free(dsp_id, input_va);
	if (ret) {
		pr_err("hbmem free input failed, return ret 0x%x\n", ret);
		return ret;
	}

	ret = hb_dsp_mem_free(dsp_id, output_va);
	if (ret) {
		pr_err("hbmem free output failed, return ret 0x%x\n", ret);
		return ret;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	char exec[256];

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	parse_opts(argc, argv);
	snprintf(exec, 256, "echo -n \"%s\" > /sys/module/firmware_class/parameters/path", dsp_path);
	system(exec);

	dsp_sample();

	return 0;
}
