/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <getopt.h>
#include <string.h>
#include "uvc_capture_sample.h"

uint32_t dump_file;
int32_t run_time;
int32_t loop_cnt;

void print_usage(const char *prog)
{
	printf("Usage: %s \n", prog);
	puts(
		 "	-i --video_id <id>  		Video device ID\n"
		 "	-d --dump_file      		Save image file flag\n"
		 "	-F --format <fmt>    		picture format(e.g., YUYV, NV12)\n"
		 "	-l --loop_cnt <num>    		Number of capture loops\n"
		 "	-H --height <px>    		Image height\n"
		 "	-W --width <px>     		Image width\n"
		 "	-E --isp_info            	Show Isp Info flag\n"
		 "	-h --help           		Show this message\n"
	);
	exit(1);
}

int v4l2_parse_opts(int argc, char **argv, uvc_camera_context *ptc, int max_num)
{
	int cmd_ret, num = 0;
	int index = -1;
	int opt_index = 0;
	static const char short_options[] = "i:F:l:H:W:dEh";
	static const struct option long_options[] = {
		{"video_id", required_argument, 0, 'i'},
		{"dump_file", no_argument, 0, 'd'},
		{"picture_format", required_argument, 0, 'F'},
		{"loop_cnt", required_argument, 0, 'l'},
		{"picture_height", required_argument, 0, 'H'},
		{"picture_width", required_argument, 0, 'W'},
		{"isp_info", no_argument, 0, 'E'},
		{"help", no_argument, 0, 'h'},
		{ NULL, 0, 0, 0 }
	};

	if (argc == 1) {
		print_usage(argv[0]);
		return -1;
	}

	while((cmd_ret = getopt_long(argc, argv, short_options,
								long_options, &opt_index)) != -1) {

		if (cmd_ret == 'h')
		{
			print_usage(argv[0]);
			return -1;
		}

		if ((cmd_ret == 'i' || cmd_ret == 'F' || cmd_ret == 'l' || cmd_ret == 'H' || cmd_ret == 'W') && optarg == NULL)
		{
			printf("Error: Missing argument for option -%c\n", cmd_ret);
			return -1;
		}
		switch (cmd_ret) {
			case 'i':
				index++;
				if (num >= max_num)
				{
					printf("Error: Exceeding max pipeline count (%d)\n", max_num);
					return -1;
				}
				ptc[index].video_id = atoi(optarg);
				printf("ptc[%d].video_id = %d\n", index, ptc[index].video_id);
				break;
			case 'd':
				if (num < 0)
				{
					printf("Error: video_id must be set before other parameters\n");
					return -1;
				}
				ptc[index].dump_mask = 1;
				printf("ptc[%d].dump_mask = %d\n", index, ptc[index].dump_mask);
				break;
			case 'F':
				if (num < 0)
				{
					printf("Error: video_id must be set before other parameters\n");
					return -1;
				}
				ptc[index].pic_format = str_to_format(optarg);
				printf("ptc[%d].pic_format = %d\n", index, ptc[index].pic_format);
				break;
			case 'l':
				if (num < 0)
				{
					printf("Error: video_id must be set before other parameters\n");
					return -1;
				}
				ptc[index].loop_cnt = atoi(optarg);
				printf("ptc[%d].loop_cnt = %d\n", index, ptc[index].loop_cnt);
				break;
			case 'H':
				if (num < 0)
				{
					printf("Error: video_id must be set before other parameters\n");
					return -1;
				}
				ptc[index].pic_height = atoi(optarg);
				printf("ptc[%d].pic_height = %d\n", index, ptc[index].pic_height);
				break;
			case 'W':
				if (num < 0)
				{
					printf("Error: video_id must be set before other parameters\n");
					return -1;
				}
				ptc[index].pic_width = atoi(optarg);
				printf("ptc[%d].pic_width = %d\n", index, ptc[index].pic_width);
				break;
			case 'E':
				if (num < 0)
				{
					printf("Error: video_id must be set before other parameters\n");
					return -1;
				}
				ptc[index].isp_info_mask = 1;
				printf("ptc[%d].isp_info_mask = %d\n", index, ptc[index].isp_info_mask);
				break;

			case 'h':
				print_usage(argv[0]);
				return -1;
		}
	}

	printf("DEBUG: index = %d, max_num = %d\n", index, max_num);
	return (index >= 0) ? index : -1;
}


static void *v4l2_thread_func(void *arg)
{
	uvc_camera_context *ptc = (uvc_camera_context *)arg;

	int rc;

	runtime_start(ptc);

	rc = v4l2_uvc_camera_device_open(ptc);
	if (rc < 0)
		return NULL;

	rc = v4l2_uvc_camera_device_init(ptc);
	if (rc < 0)
		goto err0;

	rc = v4l2_uvc_camera_device_reqbufs(ptc, MAX_HOR_BUF_NUM);
	if (rc < 0)
		goto err0;

	rc = v4l2_uvc_camera_device_start(ptc);
	if (rc < 0)
		goto err1;

	// dqbuf after start stream for 100ms
	sleep(0.1);

	rc = v4l2_isp_uvc_camera_device_dumpframe(ptc);
	if (rc < 0)
		printf("dump frame failed\n");

	v4l2_uvc_camera_device_stop(ptc);
err1:
	v4l2_uvc_camera_device_reqbufs(ptc, 0);
err0:
	v4l2_uvc_camera_device_close(ptc);
	return NULL;
}

int main(int argc, char** argv) {

	int rc, i, pipe_num;

	memset(TestContext, 0x0, sizeof(uvc_camera_context) * V4L2_MAX_PIPE_NUM);

	pipe_num = v4l2_parse_opts(argc, argv, TestContext, V4L2_MAX_PIPE_NUM);

	if (pipe_num == -1) {
		printf("v4l2 parse opts, test error!!!\n");
		exit(-1) ;
	}

	printf("pipe_num:%d\n", pipe_num);
	for (i = 0; i <= pipe_num; i++) {
		printf("TestContext[%d].pic_format:%d\n", i, TestContext[i].pic_format);
		printf("TestContext[%d].pic_width:%d\n", i, TestContext[i].pic_width);
		printf("TestContext[%d].pic_height:%d\n", i, TestContext[i].pic_height);
		printf("TestContext[%d].loop_cnt:%d\n", i, TestContext[i].loop_cnt);
	}

	g_pipe_num = pipe_num;

	for (i = 0; i <= pipe_num; i++) {
		rc = pthread_create(&TestContext[i].work_info.thid, NULL, v4l2_thread_func, (void *)(&TestContext[i]));
		printf("TestContext[%d] create pthread %s\n", i, !rc ? "success" : "failed");
	}

	for (i = 0; i <= pipe_num; i++) {
		pthread_join(TestContext[i].work_info.thid, NULL);
		printf("pipe(%d)Test thread %lu---join done.\n", i, TestContext[i].work_info.thid);
	}

	printf("------ Test case uvc_capture_sample done  ------\n");
	return 0;
}