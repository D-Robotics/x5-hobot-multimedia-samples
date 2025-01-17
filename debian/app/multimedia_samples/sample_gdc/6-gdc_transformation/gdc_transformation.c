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
#include <dirent.h>
#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"



typedef struct gdc_info
{
	uint32_t input_width;
	uint32_t input_height;
	hb_mem_common_buf_t bin_buf;
	char* input_file;
} gdc_info_s;

static struct option const long_options[] = {
	{"input", required_argument, NULL, 'i'},
	{"ix", required_argument, NULL, 'x'},
	{"iy", required_argument, NULL, 'y'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

int read_nv12_image(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image);
int create_and_run_vflow(gdc_info_s *gdc_info,
		hbn_vnode_image_t *input_image,
		hb_mem_common_buf_t *bin_buf);

static void print_help() {
	printf("Usage: %s [OPTIONS]\n", get_program_name());
	printf("Options:\n");
	printf("  i, --input <input_file>       Specify the input image file.\n");
	printf("  x, --ix <input_width>         Specify the width of the input image.\n");
	printf("  y, --iy <input_height>        Specify the height of the input image.\n");
	printf("\n");
}

int main(int argc, char** argv) {
	hbn_vnode_image_t input_image = {0};
	int ret = 0;
	gdc_info_s gdc_info = {0};
	int opt_index = 0;
	int c = 0;
	memset(&gdc_info.bin_buf, 0, sizeof(hb_mem_common_buf_t));

	while((c = getopt_long(argc, argv, "i:x:y:h",
					long_options, &opt_index)) != -1) {
		switch (c)
		{
			case 'i':
				gdc_info.input_file = optarg;
				break;
			case 'x':
				gdc_info.input_width = atoi(optarg);
				break;
			case 'y':
				gdc_info.input_height = atoi(optarg);
				break;
			case 'h':
			default:
				print_help();
				return 0;
		}
	}

	// 检查标准输入是否来自终端
	if ((!isatty(fileno(stdin))) || (argc == 1)) {
		print_help();
		return 0;
	}

	if (gdc_info.input_file == NULL) {
		printf("please Specify the input image file.\n");
		printf("you can run such as ./gdc_transformation -i gdc_res/test_building_1920x1080.yuv --ix 1920 --iy 1080\n");
		return 0;
	}

	if (gdc_info.input_width == 0 || gdc_info.input_height == 0) {
		printf("please Specify the input image height and width.\n");
		printf("you can run such as ./gdc_transformation -i gdc_res/test_building_1920x1080.yuv --ix 1920 --iy 1080\n");
		return 0;
	}

	printf("input file: %s\ninput:%dx%d\n",
			gdc_info.input_file,
			gdc_info.input_width, gdc_info.input_height);
	ret = hb_mem_module_open();
	ERR_CON_EQ(ret, 0);
	ret = read_nv12_image(&gdc_info, &input_image);
	ERR_CON_EQ(ret, 0);
	ret = create_and_run_vflow(&gdc_info, &input_image,
			&gdc_info.bin_buf);
	ERR_CON_EQ(ret, 0);
	ret = hb_mem_free_buf(input_image.buffer.fd[0]);
	hb_mem_module_close();

	return 0;
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

int create_and_run_vflow(gdc_info_s *gdc_info,
						 hbn_vnode_image_t *input_image,
						 hb_mem_common_buf_t *bin_buf) {
	int ret = 0;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;
	hbn_vflow_handle_t vflow_fd;
	hbn_vnode_handle_t gdc_vnode_fd;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	int64_t alloc_flags = 0;
	hbn_vnode_image_t output_img = {0};
	gdc_attr_t gdc_attr = {0};
	int timeout = 1000;
	DIR *dir;
	struct dirent *entry;

	dir = opendir("./gdc_res");
	if (dir == NULL) {
		printf("Failed to open gdc_res directory.\n");
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_REG) {
			continue; // Skip non-regular files
		}

		char *json_filename = entry->d_name;
		if (strstr(json_filename, ".json") == NULL) {
			continue; // Skip non-JSON files
		}

		char json_path[256];
		snprintf(json_path, sizeof(json_path), "./gdc_res/%s", json_filename);

		char output_filename[256];
		snprintf(output_filename, sizeof(output_filename), "%s.yuv", strtok(json_filename, "."));

		uint64_t config_size = 0;
		uint32_t *cfg_buf = NULL;

		ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &gdc_vnode_fd);
		ERR_CON_EQ(ret, 0);

		ret = hbn_gen_gdc_bin_json(json_path, NULL, &cfg_buf, &config_size);
		if (ret != 0) {
			printf("hbn_gen_gdc_bin_json failed for %s, ret = %d\n", json_path, ret);
			continue;
		}

		memset(&gdc_info->bin_buf, 0, sizeof(hb_mem_common_buf_t));
		alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD |
					  HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
		ret = hb_mem_alloc_com_buf(config_size, alloc_flags, &gdc_info->bin_buf);
		if (ret != 0 || gdc_info->bin_buf.virt_addr == NULL) {
			printf("hb_mem_alloc_com_buf failed for %s, ret = %d\n", json_path, ret);
			hbn_free_gdc_bin(cfg_buf);
			continue;
		}

		memcpy(gdc_info->bin_buf.virt_addr, cfg_buf, config_size);
		hb_mem_flush_buf(gdc_info->bin_buf.fd, 0, config_size);
		gdc_attr.config_addr = bin_buf->phys_addr;
		gdc_attr.config_size = bin_buf->size;
		gdc_attr.binary_ion_id = bin_buf->share_id;
		gdc_attr.binary_offset = bin_buf->offset;
		gdc_attr.total_planes = 2;

		ret = hbn_vnode_set_attr(gdc_vnode_fd, &gdc_attr);
		ERR_CON_EQ(ret, 0);

		gdc_ichn_attr_t gdc_ichn_attr = {0};
		gdc_ichn_attr.input_width = gdc_info->input_width;
		gdc_ichn_attr.input_height = gdc_info->input_height;
		gdc_ichn_attr.input_stride = gdc_info->input_width;

		ret = hbn_vnode_set_ichn_attr(gdc_vnode_fd, chn_id, &gdc_ichn_attr);
		ERR_CON_EQ(ret, 0);

		gdc_ochn_attr_t gdc_ochn_attr = {0};
		gdc_ochn_attr.output_width = gdc_info->input_width;
		gdc_ochn_attr.output_height = gdc_info->input_height;
		gdc_ochn_attr.output_stride = gdc_info->input_width;

		ret = hbn_vnode_set_ochn_attr(gdc_vnode_fd, chn_id, &gdc_ochn_attr);
		ERR_CON_EQ(ret, 0);

		alloc_attr.buffers_num = 3;
		alloc_attr.is_contig = 1;
		alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;

		ret = hbn_vnode_set_ochn_buf_attr(gdc_vnode_fd, chn_id, &alloc_attr);
		ERR_CON_EQ(ret, 0);

		ret = hbn_vflow_create(&vflow_fd);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vflow_add_vnode(vflow_fd, gdc_vnode_fd);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vflow_start(vflow_fd);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_sendframe(gdc_vnode_fd, chn_id, input_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_getframe(gdc_vnode_fd, chn_id, timeout, &output_img);
		ERR_CON_EQ(ret, 0);

		dump_2plane_yuv_to_file(output_filename,
								output_img.buffer.virt_addr[0],
								output_img.buffer.virt_addr[1],
								output_img.buffer.size[0],
								output_img.buffer.size[1]);
		printf("Dump image to file(%s), size(%ld) + size1(%ld) succeeded\n", output_filename, output_img.buffer.size[0],output_img.buffer.size[1]);

		ret = hbn_vnode_releaseframe(gdc_vnode_fd, chn_id, &output_img);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vflow_stop(vflow_fd);
		ERR_CON_EQ(ret, 0);
		hb_mem_free_buf(gdc_info->bin_buf.fd);
		hbn_vflow_destroy(vflow_fd);
		hbn_free_gdc_bin(cfg_buf);
		hbn_vnode_close(gdc_vnode_fd);
	}

	closedir(dir);
	return ret;
}
