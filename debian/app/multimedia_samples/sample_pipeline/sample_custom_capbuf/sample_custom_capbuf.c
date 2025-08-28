/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#include "hb_mem_mgr.h"
#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"
#include "./feedback_vse.h"
#include "./feedback_gdc.h"
#include "./vflow_connection.h"
#include "./mem_ion.h"

#define VSE_MAX_CHANNELS 6
#define VSE_ALL_CHN_MASK ((1 << (VSE_MAX_CHANNELS)) -1)
#define MAX_CHANELS VSE_MAX_CHANNELS
#define BIT(i) (1 << (i))


enum cap_loop_type {
	CAP_LOOP_ONLY_FEEDBACK = 0,
	CAP_LOOP_FEEDBACK_AND_CREATE = 1,
};

static char *buf_flag[CAP_BUF_INTERNAL+1] = {
	"CAP_BUF_CUSTOM",
	"CAP_BUF_CUSTOM_EXT",
	"CAP_BUF_INTERNAL",
};

static int verbose_flag = 0;
static connection_info_t con;

typedef struct {
	int cap_buf_flag;
	int cap_loop_cnt;
	int cap_loop_flag;
	int cap_thread_num;
	int cap_dump_flag;
	pthread_t cap_thread_id[6];
	int pipe_index[6];
	pipe_contex_t pipe_contex[6];
	scaler_info_s scaler_info[6];
	gdc_info_s gdc_info[6];
} SampleInfo;

static SampleInfo flags;
scaler_info_s scaler_info;
gdc_info_s gdc_info;

static struct option const vse_long_options[] = {
	{"input_file", required_argument, NULL, 'i'},
	{"input_width", required_argument, NULL, 'w'},
	{"input_height", required_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};

static struct option const gdc_long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{"iw", required_argument, NULL, 'w'},
	{"ih", required_argument, NULL, 'h'},
	{"ow", required_argument, NULL, 'x'},
	{"oh", required_argument, NULL, 'y'},
	{NULL, 0, NULL, 0}
};

static struct option const vse_gdc_long_options[] = {
	{"input_file", required_argument, NULL, 'i'},
	{"input_width", required_argument, NULL, 'w'},
	{"input_height", required_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'V'},
	{"config", required_argument, NULL, 'c'},
	{"output", required_argument, NULL, 'o'},
	{"ow", required_argument, NULL, 'x'},
	{"oh", required_argument, NULL, 'y'},
	{NULL, 0, NULL, 0}
};

static void print_help() {
    printf("Usage: program -e <engine> [OPTIONS]\n");
    printf("Options for vse:\n");
    printf("-i, --input_file FILE\tSpecify the input file\n");
    printf("-w, --input_width WIDTH\tSpecify the input width\n");
    printf("-h, --input_height HEIGHT\tSpecify the input height\n");
    printf("-V, --verbose\t\tEnable verbose mode\n");
    printf("Options for gdc:\n");
    printf("-c, --config_file FILE\tSpecify the config file\n");
    printf("-i, --input_file FILE\tSpecify the input file\n");
    printf("-o, --output_file FILE\tSpecify the output file\n");
    printf("-w, --input_width WIDTH\tSpecify the input width\n");
    printf("-h, --input_height HEIGHT\tSpecify the input height\n");
    printf("-x, --output_width WIDTH\tSpecify the output width\n");
    printf("-y, --output_height HEIGHT\tSpecify the output height\n");
    printf("Options for gdc-0-vse:\n");
    printf("-i, --input_file FILE\tSpecify the input file\n");
    printf("-w, --input_width WIDTH\tSpecify the input width\n");
    printf("-h, --input_height HEIGHT\tSpecify the input height\n");
    printf("-V, --verbose\t\tEnable verbose mode\n");
    printf("-c, --config_file FILE\tSpecify the config file for gdc\n");
    printf("-o, --output_file FILE\tSpecify the output file for gdc\n");
    printf("-x, --output_width WIDTH\tSpecify the output width for gdc\n");
    printf("-y, --output_height HEIGHT\tSpecify the output height for gdc\n");
}

static int parse_vse_and_gdc(int argc, char **argv,
			scaler_info_s *scaler_info, gdc_info_s *gdc_info)
{
    int opt_index = 0;
    int c = 0;

    while ((c = getopt_long(argc, argv, "i:w:h:V:c:o:x:y:", vse_gdc_long_options, &opt_index)) != -1) {
        switch (c) {
            case 'i':
                scaler_info->yuv_file = optarg;
                break;
            case 'w':
                scaler_info->input_width = atoi(optarg);
                break;
            case 'h':
                scaler_info->input_height = atoi(optarg);
                break;
            case 'V':
                verbose_flag = 1;
                break;
            case 'c':
                gdc_info->gdc_bin_file = optarg;
                break;
            case 'o':
                gdc_info->output_file = optarg;
                break;
            case 'x':
                gdc_info->output_width = atoi(optarg);
                break;
            case 'y':
                gdc_info->output_height = atoi(optarg);
                break;
            default:
                print_help();
                return -1;
        }
    }

	printf("Using input file: %s, input: %dx%d\n",
		scaler_info->yuv_file,
		scaler_info->input_width, scaler_info->input_height);

	if (gdc_info->gdc_bin_file == NULL) {
		print_help();
		return -1;
	}

	if (gdc_info->output_width == 0)
		gdc_info->output_width = gdc_info->input_width;
	if (gdc_info->output_height == 0)
		gdc_info->output_height = gdc_info->input_height;

	printf("config file: %s\n", gdc_info->gdc_bin_file);
	return 0;
}

static int parse_vse(int argc, char **argv, scaler_info_s *scaler_info) {
	int opt_index = 0;
	int c = 0;

	while ((c = getopt_long(argc, argv, "i:w:h:V:", vse_long_options, &opt_index)) != -1) {
	switch (c) {
		case 'i':
			scaler_info->yuv_file = optarg;
			break;
		case 'w':
			scaler_info->input_width = atoi(optarg);
			break;
		case 'h':
			scaler_info->input_height = atoi(optarg);
			break;
		case 'V':
			verbose_flag = 1;
			break;
		default:
			print_help();
			return -1;
	}
	}

	printf("Using input file: %s, input: %dx%d\n",
		scaler_info->yuv_file,
		scaler_info->input_width, scaler_info->input_height);

	return 0;
}

static int parse_gdc(int argc, char **argv, gdc_info_s *gdc_info) {
    int opt_index = 0;
    int c = 0;

    while ((c = getopt_long(argc, argv, "c:i:o:w:h:x:y:", gdc_long_options, &opt_index)) != -1) {
        switch (c) {
            case 'c':
                gdc_info->gdc_bin_file = optarg;
                break;
            case 'i':
                gdc_info->input_file = optarg;
                break;
            case 'o':
                gdc_info->output_file = optarg;
                break;
            case 'w':
                gdc_info->input_width = atoi(optarg);
                break;
            case 'h':
                gdc_info->input_height = atoi(optarg);
                break;
            case 'x':
                gdc_info->output_width = atoi(optarg);
                break;
            case 'y':
                gdc_info->output_height = atoi(optarg);
                break;
            default:
                print_help();
                return -1;
        }
    }

    if (gdc_info->gdc_bin_file == NULL) {
        print_help();
        return -1;
    }

    if (gdc_info->output_width == 0)
        gdc_info->output_width = gdc_info->input_width;
    if (gdc_info->output_height == 0)
        gdc_info->output_height = gdc_info->input_height;

    printf("config file: %s\ninput image: %s\ninput: %dx%d\noutput: %dx%d\n",
           gdc_info->gdc_bin_file, gdc_info->input_file,
           gdc_info->input_width, gdc_info->input_height,
           gdc_info->output_width, gdc_info->output_height);

    return 0;
}

static int vse_dump_images(char *path, int32_t loop, uint32_t output_chn_mask, hbn_vnode_image_t *imagev)
{
	int i, ret = 0;
	char file_name[100];

	if (access(path, F_OK) != 0) {
		// 如果路径不存在，递归创建
		if (mkdir(path, 0755) != 0) {
			printf("Failed to create directory");
			return -1;
		}
	}

	for (i = 0; i < MAX_CHANELS; i++) {
		if (!(output_chn_mask & BIT(i))) {
			continue;
		}
		snprintf(file_name, sizeof(file_name),
			"%s/loop%d_nv12_chn%d_%dx%d_stride_%d.yuv", path, loop,
			i, imagev[i].buffer.width, imagev[i].buffer.height,
			imagev[i].buffer.stride);

		// printf("=== dump_en: %s v0 %p v1 %p, size0 %ld, size1 %ld\n",
		// 	file_name,
		// 	imagev[i].buffer.virt_addr[0],
		// 	imagev[i].buffer.virt_addr[1],
		// 	imagev[i].buffer.size[0],
		// 	imagev[i].buffer.size[1]);
		// 保存输出图像到文件
		dump_2plane_yuv_to_file(file_name,
					imagev[i].buffer.virt_addr[0],
					imagev[i].buffer.virt_addr[1],
					imagev[i].buffer.size[0],
					imagev[i].buffer.size[1]);
	}

	printf("%s path %s done\n", __func__, path);
	return ret;
}

static int gdc_dump_images(char *path, int32_t loop, hbn_vnode_image_t *image)
{
	int ret = 0;
	char file_name[100];

	if (access(path, F_OK) != 0) {
		// 如果路径不存在，递归创建
		if (mkdir(path, 0755) != 0) {
			printf("Failed to create directory");
			return -1;
		}
	}

	snprintf(file_name, sizeof(file_name),
		"%s/loop%d_nv12_chn_%dx%d_stride_%d.yuv", path, loop,
		image->buffer.width, image->buffer.height,
		image->buffer.stride);
	// printf("=== dump: %s v0 %p v1 %p, size0 %ld, size1 %ld\n",
	// 	file_name,
	// 	image->buffer.virt_addr[0],
	// 	image->buffer.virt_addr[1],
	// 	image->buffer.size[0],
	// 	image->buffer.size[1]);
	// 保存输出图像到文件
	dump_2plane_yuv_to_file(file_name,
				image->buffer.virt_addr[0],
				image->buffer.virt_addr[1],
				image->buffer.size[0],
				image->buffer.size[1]);

	printf("%s path %s done\n", __func__, path);
	return ret;
}

static int read_nv12_image(char *image_path, uint32_t iw, uint32_t ih,
				uint32_t sw, uint32_t sh,
				hbn_vnode_image_t *input_image) {
	int64_t alloc_flags = 0;
	int ret = 0;
	uint64_t offset = 0;

	memset(input_image, 0, sizeof(hbn_vnode_image_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
				HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD |
				HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN |
				HB_MEM_USAGE_CACHED |
				HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hb_mem_alloc_graph_buf(iw, ih,
				MEM_PIX_FMT_NV12,
				alloc_flags,
				sw, sh,
				&input_image->buffer);
	ERR_CON_EQ(ret, 0);
	read_yuvv_nv12_file(image_path,
			(char *)(input_image->buffer.virt_addr[0]),
			(char *)(input_image->buffer.virt_addr[1]),
			input_image->buffer.size[0]);
	if (input_image->buffer.fd[0] > 0) {
		hb_mem_flush_buf(input_image->buffer.fd[0], offset,
					input_image->buffer.size[0]);
	}
	if (input_image->buffer.fd[1] > 0) {
		hb_mem_flush_buf(input_image->buffer.fd[1], offset,
					input_image->buffer.size[1]);
	}
	// 设置一个时间戳
	gettimeofday(&input_image->info.tv, NULL);

	return ret;
}

static int free_nv12_image(hbn_vnode_image_t *input_image)
{
	int ret = 0;

	ret = hb_mem_free_buf(input_image->buffer.fd[0]);
	if (ret != 0) {
		printf("Fail to free image buf ret %d\n", ret);
	}
	return ret;
}

static int feedback_vse_create_and_run(int n)
{
	int ret = 0;

	printf(">>> %s n %d vflow_fd %ld\n", __func__, n, flags.pipe_contex[n].vflow_fd);
	ret = feedback_vse_create(&flags.pipe_contex[n], &flags.scaler_info[n],
					VSE_ALL_CHN_MASK, flags.cap_buf_flag);
	if (ret != 0) {
		printf("Fail to feedback_vse_create\n");
		return ret;
	}
	node_info_t vse_node;
	vse_node.node_handle = flags.pipe_contex[n].vse_node_handle;
	ret = vflow_create_and_run(&flags.pipe_contex[n], &vse_node, 1, NULL, 0);
	if (ret != 0) {
		printf("Fail to vflow_create_and_run\n");
		return ret;
	}

	return ret;
}

static int feedback_vse_stop_and_destory(int n)
{
	int ret;

	ret = feedback_vse_destory(&flags.pipe_contex[n], &flags.scaler_info[n],
				VSE_ALL_CHN_MASK, flags.cap_buf_flag);
	if (ret != 0) {
		printf("Fail to feedback_vse_destory\n");
	}
	ret = vflow_stop_and_destory(&flags.pipe_contex[n]);
	if (ret != 0) {
		printf("Fail to vflow_stop_and_destory\n");
	}
	printf(">>> %s n %d vflow_fd %ld\n", __func__, n, flags.pipe_contex[n].vflow_fd);
	flags.pipe_contex[n].vflow_fd = 0;

	return ret;
}

static double time_diff_ms(struct timeval start, struct timeval end) {
    double start_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_usec / 1000.0;
    double end_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_usec / 1000.0;
    return end_ms - start_ms;
}

void *feedback_thread_vse(void *args)
{
	int ret;
	int n = *(int *)args;
	int32_t loop_cnt = 0;
	char path[100];
	struct timeval start, end;

	printf("==== thread %d vflow fd %ld\n", n, flags.pipe_contex[n].vflow_fd);
	gettimeofday(&start, NULL);
loop:
	if (flags.cap_loop_flag != CAP_LOOP_ONLY_FEEDBACK) {
		ret = feedback_vse_create_and_run(n);
		if (ret != 0) {
			printf("Fail to feedback_vse_create_and_run\n");
		}
	}
	ret = feedback_vse(&flags.pipe_contex[n], &flags.scaler_info[n], VSE_ALL_CHN_MASK,
				flags.cap_buf_flag);
	if (ret != 0) {
		printf("Fail to do vse\n");
	}

	if (flags.cap_dump_flag != 0) {
		snprintf(path, sizeof(path), "/userdata/vse/%ld",
			flags.pipe_contex[n].vflow_fd);

		vse_dump_images(path, loop_cnt, VSE_ALL_CHN_MASK,
			(hbn_vnode_image_t *)flags.scaler_info[n].output_image);
	}

	if (flags.cap_loop_flag != CAP_LOOP_ONLY_FEEDBACK) {
		feedback_vse_stop_and_destory(n);
		usleep(100*1000);
	}

	usleep(1*1000);
	if ((loop_cnt++) < flags.cap_loop_cnt)
		goto loop;

	gettimeofday(&end, NULL);
	printf("==== %s buf flag %s loop %d time cost %.3f ms\n", __func__,
		buf_flag[flags.cap_buf_flag], flags.cap_loop_cnt,
		time_diff_ms(start, end));

	return NULL;
}

static int feedback_gdc_create_and_run(int n)
{
	int ret;

	ret = feedback_gdc_create(&flags.pipe_contex[n],
				&flags.gdc_info[n],
				flags.cap_buf_flag);
	if (ret != 0) {
		printf("Fail to feedback_gdc_create\n");
	}
	node_info_t gdc_node;
	gdc_node.node_handle = flags.pipe_contex[n].gdc_node_handle;
	ret = vflow_create_and_run(&flags.pipe_contex[n],
			&gdc_node, 1, NULL, 0);
	if (ret != 0) {
		printf("Fail to vflow_create_and_run\n");
	}

	return ret;
}

static int feedback_gdc_stop_and_destory(int n)
{
	int ret;

	ret = feedback_gdc_destory(&flags.pipe_contex[n],
				&flags.gdc_info[n], flags.cap_buf_flag);
	if (ret != 0) {
		printf("Fail to feedback_gdc_destory\n");
	}
	ret = vflow_stop_and_destory(&flags.pipe_contex[n]);
	if (ret != 0) {
		printf("Fail to vflow_stop_and_destory\n");
	}

	return ret;
}

void *feedback_thread_gdc(void *args)
{
	int ret;
	int n = *(int *)args;
	int32_t loop_cnt = 0;
	char path[100];
	struct timeval start, end;

	printf("==== thread %d vflow fd %ld\n", n, flags.pipe_contex[n].vflow_fd);
	gettimeofday(&start, NULL);
loop:
	if (flags.cap_loop_flag != CAP_LOOP_ONLY_FEEDBACK) {
		feedback_gdc_create_and_run(n);
	}

	ret = feedback_gdc(&flags.pipe_contex[n], &flags.gdc_info[n],
			flags.cap_buf_flag);
	if (ret != 0) {
		printf("Fail to do gdc\n");
	}

	if (flags.cap_dump_flag != 0) {
		snprintf(path, sizeof(path), "/userdata/gdc/%ld",
			flags.pipe_contex[n].vflow_fd);
		gdc_dump_images(path, loop_cnt, &flags.gdc_info[n].output_image);

	}

	if (flags.cap_loop_flag != CAP_LOOP_ONLY_FEEDBACK) {
		feedback_gdc_stop_and_destory(n);
		usleep(100*1000);
	}

	usleep(1*1000);
	if ((loop_cnt++) < flags.cap_loop_cnt)
		goto loop;

	gettimeofday(&end, NULL);
	printf("==== %s buf flag %s loop %d time cost %.3f ms\n", __func__,
		buf_flag[flags.cap_buf_flag], flags.cap_loop_cnt,
		time_diff_ms(start, end));

	return NULL;
}

static void load_env_flags(SampleInfo *flags) {
	// 获取 CAP_BUF_FLAG 环境变量
	char *cap_buf_env = getenv("CAP_BUF_FLAG");
	flags->cap_buf_flag = (cap_buf_env != NULL) ? atoi(cap_buf_env) : 0;

	// 获取 LOOP_FLAG 环境变量
	char *loop_env = getenv("CAP_LOOP_FLAG");
	flags->cap_loop_flag = (loop_env != NULL) ? atoi(loop_env) : 0;

	char *loop_cnt = getenv("CAP_LOOP_CNT");
	flags->cap_loop_cnt = (loop_cnt != NULL) ? atoi(loop_cnt) : 0;

	char *thread_num = getenv("CAP_THREAD_NUM");
	flags->cap_thread_num = (thread_num != NULL) ? atoi(thread_num) : 1;

	char *dump_flag = getenv("CAP_DUMP_FLAG");
	flags->cap_dump_flag = (dump_flag != NULL) ? atoi(dump_flag) : 0;

	printf("env flag: "
		"flags->cap_buf_flag %d\n"
		"flags->cap_loop_flag %d\n"
		"flags->cap_loop_cnt %d\n"
		"flags->cap_thread_num %d\n"
		"flags->cap_dump_flag %d\n",
		flags->cap_buf_flag,
		flags->cap_loop_flag,
		flags->cap_loop_cnt,
		flags->cap_thread_num,
		flags->cap_dump_flag
		);
}

int main(int argc, char** argv)
{
	int ret = 0;
	char *engine_type = NULL;
	int channel = -1;
	char *token;

	load_env_flags(&flags);
	hb_mem_module_open();
	(void)mem_ion_open();

	if (argc < 3) {
		print_help();
		return 1;
	}

	if (strcmp(argv[1], "-e") != 0) {
		print_help();
		return 1;
	}

	engine_type = argv[2];
	if (strncmp(engine_type, "vse-", 4) == 0) {
		scaler_info_s scaler_info;
		gdc_info_s gdc_info;
		pipe_contex_t pipe_contex = {0};

		token = strtok(engine_type, "-");
		token = strtok(NULL, "-");
		if (token != NULL) {
			channel = atoi(token);
			printf("Using VSE on channel: %d\n", channel);
		}
		token = strtok(NULL, "-");
		if (token == NULL || strcmp(token, "gdc") != 0) {
			print_help();
			return -1;
		}
		if (channel != 0) {
			print_help();
			return -1;
		}

		printf("Using VSE on channel: %d bind GDC\n", channel);
		memset(&scaler_info, 0, sizeof(scaler_info));
		memset(&gdc_info, 0, sizeof(gdc_info));
		ret = parse_vse_and_gdc(argc - 2, &argv[2], &scaler_info, &gdc_info);
		ERR_CON_EQ(ret, 0);

		ret = feedback_vse_create(&pipe_contex, &scaler_info, BIT(channel), CAP_BUF_INTERNAL);
		ERR_CON_EQ(ret, 0);

		vse_ochn_attr_t vse_ochn_attr = {0};
		memset(&vse_ochn_attr, 0, sizeof(vse_ochn_attr_t));
		ret = hbn_vnode_get_ochn_attr(pipe_contex.vse_node_handle, channel, &vse_ochn_attr);
		ERR_CON_EQ(ret, 0);
		gdc_info.input_width = vse_ochn_attr.target_w;
		gdc_info.input_height = vse_ochn_attr.target_h;
		gdc_info.output_width = vse_ochn_attr.target_w;
		gdc_info.output_height = vse_ochn_attr.target_h;
		ret = feedback_gdc_create(&pipe_contex, &gdc_info, flags.cap_buf_flag);
		ERR_CON_EQ(ret, 0);

		con.src_node.node_handle = pipe_contex.vse_node_handle;
		con.src_node.out_chn = channel;
		con.src_node.in_chn = 0;
		con.dst_node.node_handle = pipe_contex.gdc_node_handle;
		con.dst_node.in_chn = 0;
		con.dst_node.out_chn = 0;
		ret = vflow_create_and_run(&pipe_contex, (node_info_t *)&con,
					2, &con, 1);
		ERR_CON_EQ(ret, 0);

		hbn_vnode_image_t input;
		read_nv12_image(scaler_info.yuv_file,
				scaler_info.input_width, scaler_info.input_height,
				scaler_info.input_width, scaler_info.input_height,
				&input);
		ret = vflow_feedback(&pipe_contex, &con, &input,
					&gdc_info.output_image, flags.cap_buf_flag);
		ERR_CON_EQ(ret, 0);
		gdc_dump_images("/userdata/vse_gdc", 0, &gdc_info.output_image);

		// 模拟业务处理耗时
		sleep(10);

		ret = feedback_vse_destory(&pipe_contex, &scaler_info,
					BIT(channel), CAP_BUF_INTERNAL);
		ERR_CON_EQ(ret, 0);
		ret = feedback_gdc_destory(&pipe_contex, &gdc_info, flags.cap_buf_flag);
		ERR_CON_EQ(ret, 0);
		ret = vflow_stop_and_destory(&pipe_contex);
		ERR_CON_EQ(ret, 0);
	} else if (strncmp(engine_type, "gdc-", 4) == 0) {
		scaler_info_s scaler_info;
		gdc_info_s gdc_info;
		pipe_contex_t pipe_contex = {0};

		token = strtok(engine_type, "-");
		token = strtok(NULL, "-");
		if (token != NULL) {
			channel = atoi(token);
			printf("Using GDC on channel: %d\n", channel);
		}
		token = strtok(NULL, "-");
		if (token == NULL || strcmp(token, "vse") != 0) {
			print_help();
			return -1;
		}
		printf("Using GDC on channel: %d bind VSE\n", channel);
		if (channel != 0) {
			print_help();
			return -1;
		}
		memset(&scaler_info, 0, sizeof(scaler_info));
		memset(&gdc_info, 0, sizeof(gdc_info));
		ret = parse_vse_and_gdc(argc - 2, &argv[2], &scaler_info, &gdc_info);
		ERR_CON_EQ(ret, 0);

		gdc_info.input_width = scaler_info.input_width;
		gdc_info.input_height = scaler_info.input_height;
		gdc_info.output_width = scaler_info.input_width;
		gdc_info.output_height = scaler_info.input_height;
		ret = feedback_gdc_create(&pipe_contex, &gdc_info, CAP_BUF_INTERNAL);
		ERR_CON_EQ(ret, 0);
		ret = feedback_vse_create(&pipe_contex, &scaler_info,
					VSE_ALL_CHN_MASK, flags.cap_buf_flag);
		ERR_CON_EQ(ret, 0);

		// 模拟业务处理耗时
		sleep(10);

		con.src_node.node_handle = pipe_contex.gdc_node_handle;
		con.src_node.out_chn = channel;
		con.src_node.in_chn = 0;
		con.dst_node.node_handle = pipe_contex.vse_node_handle;
		con.dst_node.in_chn = 0;
		con.dst_node.out_chn = 1;
		ret = vflow_create_and_run(&pipe_contex, (node_info_t *)&con,
					2, &con, 1);
		ERR_CON_EQ(ret, 0);

		hbn_vnode_image_t input;
		read_nv12_image(scaler_info.yuv_file,
				scaler_info.input_width, scaler_info.input_height,
				scaler_info.input_width, scaler_info.input_height,
				&input);
		ret = vflow_feedback(&pipe_contex, &con, &input,
					&scaler_info.output_image[1], flags.cap_buf_flag);
		ERR_CON_EQ(ret, 0);
		gdc_dump_images("/userdata/gdc_vse", 0, &scaler_info.output_image[1]);

		ret = feedback_vse_destory(&pipe_contex, &scaler_info,
					VSE_ALL_CHN_MASK, flags.cap_buf_flag);
		ERR_CON_EQ(ret, 0);
		ret = feedback_gdc_destory(&pipe_contex, &gdc_info, CAP_BUF_INTERNAL);
		ERR_CON_EQ(ret, 0);
		ret = vflow_stop_and_destory(&pipe_contex);
		ERR_CON_EQ(ret, 0);
	} else if (strcmp(engine_type, "vse") == 0) {
		scaler_info_s scaler_info;
		memset(&scaler_info, 0, sizeof(scaler_info));
		ret = parse_vse(argc - 2, &argv[2], &scaler_info); // 传递 VSE 参数
		ERR_CON_EQ(ret, 0);

		printf("Using VSE engine\n");
		for (int n = 0; n < flags.cap_thread_num; n++) {
			flags.pipe_index[n] = n;
			memcpy(&flags.scaler_info[n], &scaler_info, sizeof(scaler_info));
			read_nv12_image(flags.scaler_info[n].yuv_file,
					flags.scaler_info[n].input_width, flags.scaler_info[n].input_height,
					flags.scaler_info[n].input_width, flags.scaler_info[n].input_height,
					&flags.scaler_info[n].input_image);
			if (flags.cap_loop_flag == CAP_LOOP_ONLY_FEEDBACK) {
				feedback_vse_create_and_run(n);
			}
		}

		for (int n = 0; n < flags.cap_thread_num; n++) {
			ret = pthread_create(&flags.cap_thread_id[n], NULL,
					feedback_thread_vse, (void *)&flags.pipe_index[n]);
			if (ret != 0) {
				printf("Fail to create pthread\n");
				break;
			}
		}

		for (int n = 0; n < flags.cap_thread_num; n++) {
			if (flags.cap_thread_id[n] == 0) {
				continue;
			}
			ret = pthread_join(flags.cap_thread_id[n], NULL);
			if (ret != 0) {
				printf("Fail to join pthread\n");
			}
		}

		for (int n = 0; n < flags.cap_thread_num; n++) {

			if (flags.cap_thread_id[n] == 0) {
				continue;
			}
			if (flags.cap_loop_flag == CAP_LOOP_ONLY_FEEDBACK) {
				feedback_vse_stop_and_destory(n);
			}
			ERR_CON_EQ(ret, 0);
			free_nv12_image(&flags.scaler_info[n].input_image);
		}
	} else if (strcmp(engine_type, "gdc") == 0) {
		printf("Using GDC engine\n");
		gdc_info_s gdc_info;
		memset(&gdc_info, 0, sizeof(gdc_info_s));
		ret = parse_gdc(argc - 2, &argv[2], &gdc_info); // 传递 GDC 参数
		ERR_CON_EQ(ret, 0);

		for (int n = 0; n < flags.cap_thread_num; n++) {
			flags.pipe_index[n] = n;
			memcpy(&flags.gdc_info[n], &gdc_info, sizeof(gdc_info));
			read_nv12_image(flags.gdc_info[n].input_file,
					flags.gdc_info[n].input_width, flags.gdc_info[n].input_height,
					flags.gdc_info[n].input_width, flags.gdc_info[n].input_height,
					&flags.gdc_info[n].input_image);
			ERR_CON_EQ(ret, 0);
			if (flags.cap_loop_flag == CAP_LOOP_ONLY_FEEDBACK) {
				feedback_gdc_create_and_run(n);
			}
		}

		for (int n = 0; n < flags.cap_thread_num; n++) {
			ret = pthread_create(&flags.cap_thread_id[n], NULL,
					feedback_thread_gdc, (void *)&flags.pipe_index[n]);
			if (ret != 0) {
				printf("Fail to create pthread\n");
				break;
			}
		}
		for (int n = 0; n < flags.cap_thread_num; n++) {
			if (flags.cap_thread_id[n] == 0) {
				continue;
			}
			ret = pthread_join(flags.cap_thread_id[n], NULL);
			if (ret != 0) {
				printf("Fail to join pthread\n");
			}
		}

		for (int n = 0; n < flags.cap_thread_num; n++) {
			if (flags.cap_thread_id[n] == 0) {
				continue;
			}
			if (flags.cap_loop_flag == CAP_LOOP_ONLY_FEEDBACK) {
				feedback_gdc_stop_and_destory(n);
			}
			free_nv12_image(&flags.gdc_info[n].input_image);
		}
	} else {
		printf("Unkonwn engine %s\n", engine_type);
	}

	hb_mem_module_close();
	mem_ion_close();
	return ret;
}
