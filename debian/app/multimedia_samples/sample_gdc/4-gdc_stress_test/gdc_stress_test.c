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

/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"


typedef struct gdc_info
{
	hbn_vflow_handle_t vflow_fd;
	hbn_vnode_handle_t gdc_vnode_fd;
	uint32_t input_width;
	uint32_t input_height;
	uint32_t output_width;
	uint32_t output_height;
	uint32_t count;
	char* gdc_bin_file;
	char* input_file;
	char* output_file;
	char* pid;
	int gdc_vnode_mode;
} gdc_info_s;


static struct option const long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{"iw", required_argument, NULL, 'w'},
	{"ih", required_argument, NULL, 'h'},
	{"ow", required_argument, NULL, 'x'},
	{"oh", required_argument, NULL, 'y'},
	{"count", required_argument, NULL, 'C'},
	{"pid", required_argument, NULL, 'p'},
	{"feedback", no_argument, NULL, 'f'},
	{NULL, 0, NULL, 0}
};

int read_gdc_config(gdc_info_s * gdc_info, hb_mem_common_buf_t *bin_buf);
int gdc_config_free(hb_mem_common_buf_t *bin_buf);
int read_nv12_image(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image);
int create_start_gdc_vnode(gdc_info_s *gdc_info, hb_mem_common_buf_t *bin_buf);
int stop_destroy_gdc_vnode(gdc_info_s *gdc_info);
int run_gdc(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image);
int run_gdc_stress_test(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image);

static void print_help() {
	printf("Usage: %s [OPTIONS]\n", get_program_name());
	printf("Options:\n");
	printf("  c, --config <gdc_bin_file>    Specify the gdc configuration bin file.\n");
	printf("  i, --input <input_file>       Specify the input image file.\n");
	printf("  o, --output <output_file>     Specify the output image file.\n");
	printf("  w, --iw <input_width>         Specify the width of the input image.\n");
	printf("  h, --ih <input_height>        Specify the height of the input image.\n");
	printf("  x, --ow [output_width]        Specify the width of the output image (optional).\n");
	printf("  y, --oh [output_height]       Specify the height of the output image (optional).\n");
	printf("  C, --Count [run count]        Specify gdc sendframe and getframe count.\n");
	printf("  p, --p [process id]           Specify id of process.\n");
	printf("  f, --feedback                 Specify feedback mode\n");
	printf("\n");
	printf("If --ow and --oh are not specified, they will default to the input width and height, respectively.\n");
}

int main(int argc, char** argv) {
	hbn_vnode_image_t input_image = {0};
	int ret = 0;
	gdc_info_s gdc_info = {0};
	int opt_index = 0;
	int c = 0;
	hb_mem_common_buf_t bin_buf = {0};
	gdc_info.pid = "default";

	while((c = getopt_long(argc, argv, "c:i:o:w:h:x:y:C:p:f",
							long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 'c':
			gdc_info.gdc_bin_file = optarg;
			break;
		case 'i':
			gdc_info.input_file = optarg;
			break;
		case 'o':
			gdc_info.output_file = optarg;
			break;
		case 'w':
			gdc_info.input_width = atoi(optarg);
			break;
		case 'h':
			gdc_info.input_height = atoi(optarg);
			break;
		case 'x':
			gdc_info.output_width = atoi(optarg);
			break;
		case 'y':
			gdc_info.output_height = atoi(optarg);
			break;
		case 'C':
			gdc_info.count = atoi(optarg);
			break;
		case 'p':
			gdc_info.pid = optarg;
			break;
		case 'f':
			gdc_info.gdc_vnode_mode = VNODE_WORK_MODE_FEEDBACK;
			break;
		default:
			print_help();
			return 0;
		}
	}
	if (gdc_info.gdc_bin_file == NULL) {
		print_help();
		return 0;
	}

	if (gdc_info.output_width == 0)
		gdc_info.output_width = gdc_info.input_width;
	if (gdc_info.output_height == 0)
		gdc_info.output_height = gdc_info.input_height;

	printf("GDC vnode work mode: %s\n", (gdc_info.gdc_vnode_mode == VNODE_WORK_MODE_VFLOW)?"vflow":"feedback");
	printf("config file: %s\ninput image: %s\noutput image: %s\ninput:%dx%d\noutput:%dx%d\n",
			gdc_info.gdc_bin_file, gdc_info.input_file,
			gdc_info.output_file,
			gdc_info.input_width, gdc_info.input_height,
			gdc_info.output_width, gdc_info.output_height);

	ret = hb_mem_module_open();
	ERR_CON_EQ(ret, 0);
	ret = read_gdc_config(&gdc_info, &bin_buf);
	ERR_CON_EQ(ret, 0);
	ret = read_nv12_image(&gdc_info, &input_image);
	ERR_CON_EQ(ret, 0);

	ret = create_start_gdc_vnode(&gdc_info, &bin_buf);
	ERR_CON_EQ(ret, 0);
	ret = run_gdc_stress_test(&gdc_info, &input_image);
	ERR_CON_EQ(ret, 0);
	ret = stop_destroy_gdc_vnode(&gdc_info);
	ERR_CON_EQ(ret, 0);

	ret = gdc_config_free(&bin_buf);
	ret = hb_mem_free_buf(input_image.buffer.fd[0]);
	hb_mem_module_close();

	return 0;
}

int read_gdc_config(gdc_info_s *gdc_info, hb_mem_common_buf_t *bin_buf) {
	int64_t alloc_flags = 0;
	int ret = 0;
	int offset = 0;
	char *gdc_bin_file = gdc_info->gdc_bin_file;
	char *cfg_buf = NULL;

	FILE *fp = fopen(gdc_bin_file, "r");
	if (fp == NULL) {
		printf("File %s open failed\n", gdc_bin_file);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	cfg_buf = malloc(file_size);
	int n = fread(cfg_buf, 1, file_size, fp);
	if (n != file_size) {
		printf("Read file size failed\n");
	}
	fclose(fp);

	memset(bin_buf, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(file_size, alloc_flags, bin_buf);
	if (ret != 0 || bin_buf->virt_addr == NULL) {
		printf("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		free(cfg_buf);
		return -1;
	}

	memcpy(bin_buf->virt_addr, cfg_buf, file_size);
	ret = hb_mem_flush_buf(bin_buf->fd, offset, file_size);
	ERR_CON_EQ(ret, 0);

	free(cfg_buf);
	return ret;
}

int read_nv12_image(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image) {
	int64_t alloc_flags = 0;
	int ret = 0;
	uint64_t offset = 0;
	char *input_image_path = gdc_info->input_file;
	memset(input_image, 0, sizeof(hbn_vnode_image_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
				HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD |
				HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN |
				HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_graph_buf(gdc_info->input_width,
								gdc_info->input_height,
								MEM_PIX_FMT_NV12,
								alloc_flags,
								gdc_info->input_width,
								gdc_info->input_height,
								&input_image->buffer);
	ERR_CON_EQ(ret, 0);
	read_yuvv_nv12_file(input_image_path,
					(char *)(input_image->buffer.virt_addr[0]),
					(char *)(input_image->buffer.virt_addr[1]),
					input_image->buffer.size[0]);
	hb_mem_flush_buf(input_image->buffer.fd[0], offset,
					input_image->buffer.size[0]);
	hb_mem_flush_buf(input_image->buffer.fd[1], offset,
					input_image->buffer.size[1]);

	return ret;
}

int gdc_config_free(hb_mem_common_buf_t *bin_buf) {
	return hb_mem_free_buf(bin_buf->fd);
}

int create_start_gdc_vnode(gdc_info_s *gdc_info, hb_mem_common_buf_t *bin_buf) {
	int ret = 0;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;

	hbn_buf_alloc_attr_t alloc_attr = {0};
	gdc_attr_t gdc_attr = {0};

	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &gdc_info->gdc_vnode_fd);
	ERR_CON_EQ(ret, 0);
	gdc_attr.config_addr = bin_buf->phys_addr;
	gdc_attr.config_size = bin_buf->size;
	gdc_attr.binary_ion_id = bin_buf->share_id;
	gdc_attr.binary_offset = bin_buf->offset;
	gdc_attr.total_planes = 2;
	gdc_attr.div_width = 0;
	gdc_attr.div_height = 0;
	ret = hbn_vnode_set_attr(gdc_info->gdc_vnode_fd, &gdc_attr);
	ERR_CON_EQ(ret, 0);

	gdc_ichn_attr_t gdc_ichn_attr = {0};
	gdc_ichn_attr.input_width = gdc_info->input_width;
	gdc_ichn_attr.input_height = gdc_info->input_height;
	gdc_ichn_attr.input_stride = gdc_info->input_width;
	ret = hbn_vnode_set_ichn_attr(gdc_info->gdc_vnode_fd, chn_id, &gdc_ichn_attr);
	ERR_CON_EQ(ret, 0);

	gdc_ochn_attr_t gdc_ochn_attr = {0};
	gdc_ochn_attr.output_width = gdc_info->output_width;
	gdc_ochn_attr.output_height = gdc_info->output_height;
	gdc_ochn_attr.output_stride = gdc_info->output_width;
	ret = hbn_vnode_set_ochn_attr(gdc_info->gdc_vnode_fd, chn_id, &gdc_ochn_attr);
	ERR_CON_EQ(ret, 0);
	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
					HB_MEM_USAGE_CPU_WRITE_OFTEN |
					HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(gdc_info->gdc_vnode_fd, chn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	switch (gdc_info->gdc_vnode_mode) {
		case VNODE_WORK_MODE_VFLOW:
			ret = hbn_vflow_create(&gdc_info->vflow_fd);
			ERR_CON_EQ(ret, 0);
			ret = hbn_vflow_add_vnode(gdc_info->vflow_fd, gdc_info->gdc_vnode_fd);
			ERR_CON_EQ(ret, 0);
			ret = hbn_vflow_start(gdc_info->vflow_fd);
			ERR_CON_EQ(ret, 0);
			break;
		case VNODE_WORK_MODE_FEEDBACK:
			ret = hbn_vnode_start(gdc_info->gdc_vnode_fd);
			ERR_CON_EQ(ret, 0);
			break;
		default:
			printf("Unknow GDC vnode work mode[%d]\n", gdc_info->gdc_vnode_mode);
			break;
	}

	return ret;
}

int stop_destroy_gdc_vnode(gdc_info_s *gdc_info) {
	int ret;
	switch (gdc_info->gdc_vnode_mode) {
		case VNODE_WORK_MODE_VFLOW:
			ret = hbn_vflow_stop(gdc_info->vflow_fd);
			ERR_CON_EQ(ret, 0);
			hbn_vnode_close(gdc_info->gdc_vnode_fd);
			hbn_vflow_destroy(gdc_info->vflow_fd);
			break;
		case VNODE_WORK_MODE_FEEDBACK:
			ret = hbn_vnode_stop(gdc_info->gdc_vnode_fd);
			ERR_CON_EQ(ret, 0);
			hbn_vnode_close(gdc_info->gdc_vnode_fd);
			break;
		default:
			printf("Unknow GDC vnode work mode[%d]\n", gdc_info->gdc_vnode_mode);
			break;
	}
	return 0;
}

int run_gdc(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image) {
	int ret;
	uint32_t chn_id = 0;
	hbn_vnode_image_t output_img = {0};
	int timeout = 1000;

	ret = hbn_vnode_sendframe(gdc_info->gdc_vnode_fd, chn_id, input_image);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_getframe(gdc_info->gdc_vnode_fd, chn_id, timeout, &output_img);
	ERR_CON_EQ(ret, 0);
	dump_2plane_yuv_to_file(gdc_info->output_file,
					output_img.buffer.virt_addr[0],
					output_img.buffer.virt_addr[1],
					output_img.buffer.size[0],
					output_img.buffer.size[1]);
	ret = hbn_vnode_releaseframe(gdc_info->gdc_vnode_fd, chn_id, &output_img);
	ERR_CON_EQ(ret, 0);

	return 0;
}

int run_gdc_stress_test(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image)
{
	int ret;
	int i;
	uint32_t time_count = 50;
	struct timeval tv_begin, tv_end;

	gettimeofday(&tv_begin, NULL);
	for(i = 0; i <= gdc_info->count; i++){
		ret = run_gdc(gdc_info, input_image);
		ERR_CON_EQ(ret, 0);
		if(i > time_count){
			gettimeofday(&tv_end, NULL);
			printf("gdc temp fps [%s] = %ld\n", gdc_info->pid, time_count / (tv_end.tv_sec - tv_begin.tv_sec));
			time_count += 100;
		}
	}
	gettimeofday(&tv_end, NULL);
	printf("Gdc time consuming [%s]: %ld \n", gdc_info->pid, tv_end.tv_sec - tv_begin.tv_sec);
	printf("fps average gdc [%s] = %ld \n", gdc_info->pid, gdc_info->count / (tv_end.tv_sec - tv_begin.tv_sec));

	return 0;
}

