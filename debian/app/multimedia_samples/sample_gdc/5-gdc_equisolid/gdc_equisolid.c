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

#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"


typedef struct gdc_info
{
	hbn_vflow_handle_t vflow_fd;
	hbn_vnode_handle_t gdc_vnode_fd;
	uint32_t *cfg_buf;
	uint32_t input_width;
	uint32_t input_height;
	hb_mem_common_buf_t bin_buf;
	char* input_file;
	char* output_file;
	int gdc_vnode_mode;
} gdc_info_s;

static struct option const long_options[] = {
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{"iw", required_argument, NULL, 'w'},
	{"ih", required_argument, NULL, 'h'},
	{"feedback", no_argument, NULL, 'f'},
	{NULL, 0, NULL, 0}
};

int gdc_config_free(hb_mem_common_buf_t *bin_buf);
int read_nv12_image(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image);
int create_start_gdc_vnode(gdc_info_s *gdc_info, hb_mem_common_buf_t *bin_buf);
int stop_destroy_gdc_vnode(gdc_info_s *gdc_info);
int run_gdc(gdc_info_s *gdc_info, hbn_vnode_image_t *input_image);

static void print_help() {
	printf("Usage: %s [OPTIONS]\n", get_program_name());
	printf("Options:\n");
	printf("  i, --input <input_file>       Specify the input image file.\n");
	printf("  o, --output <output_file>     Specify the output image file.\n");
	printf("  w, --iw <input_width>         Specify the width of the input image.\n");
	printf("  h, --ih <input_height>        Specify the height of the input image.\n");
	printf("  f, --feedback                 Specify feedback mode\n");
	printf("\n");
}

int main(int argc, char** argv) {
	hbn_vnode_image_t input_image = {0};
	int ret = 0;
	gdc_info_s gdc_info = {0};
	char output_filename[128]={0};
	int opt_index = 0;
	int c = 0;
	memset(&gdc_info.bin_buf, 0, sizeof(hb_mem_common_buf_t));

	while((c = getopt_long(argc, argv, "i:o:w:h:x:y:",
					long_options, &opt_index)) != -1) {
		switch (c)
		{
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
			case 'f':
				gdc_info.gdc_vnode_mode = VNODE_WORK_MODE_FEEDBACK;
				break;
			default:
				print_help();
				return 0;
		}
	}

	if (gdc_info.input_file == NULL) {
		printf("please Specify the input image file.\n");
		printf("you can run such as ./gdc_equisolid -i test_image_1920x1080.yuv --iw 1920 --ih 1080\n");
		return 0;
	}

	if (gdc_info.input_width == 0 || gdc_info.input_height == 0) {
		printf("please Specify the input image height and width.\n");
		printf("you can run such as ./gdc_equisolid -i test_image_1920x1080.yuv --iw 1920 --ih 1080\n");
		return 0;
	}

	if(gdc_info.output_file == NULL) {
		snprintf(output_filename, sizeof(output_filename), "gdc_output_%dx%d.yuv", gdc_info.input_height, gdc_info.input_width);
		gdc_info.output_file = output_filename;
	}

	printf("GDC vnode work mode: %s\n", (gdc_info.gdc_vnode_mode == VNODE_WORK_MODE_VFLOW)?"vflow":"feedback");
	printf("input file: %s\noutput file: %s\ninput:%dx%d\n",
			gdc_info.input_file,
			gdc_info.output_file,
			gdc_info.input_width, gdc_info.input_height);

	ret = hb_mem_module_open();
	ERR_CON_EQ(ret, 0);
	ret = read_nv12_image(&gdc_info, &input_image);
	ERR_CON_EQ(ret, 0);

	ret = create_start_gdc_vnode(&gdc_info, &gdc_info.bin_buf);
	ERR_CON_EQ(ret, 0);
	ret = run_gdc(&gdc_info, &input_image);
	ERR_CON_EQ(ret, 0);
	ret = stop_destroy_gdc_vnode(&gdc_info);
	ERR_CON_EQ(ret, 0);

	ret = gdc_config_free(&gdc_info.bin_buf);
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

int gdc_config_free(hb_mem_common_buf_t *bin_buf) {
	return hb_mem_free_buf(bin_buf->fd);
}

int init_windows(window_t *windows,uint32_t width,uint32_t height) {
	windows->strength = 1.0;       // Dimensionless non-negative parameter defining the strength of transformation along X axis
	windows->strengthY = 1.0;      // Dimensionless non-negative parameter defining the strength of transformation along X axis
	windows->angle = 0;            // Angle of main projection axis rotation around itself in degrees
	windows->elevation = 0;        // Angle in degrees which specify the main projection axis
	windows->azimuth = 0;          // Angle in degrees which specify the main projection axis, counted clockwise from North direction (positive to East)
	windows->keep_ratio = 1;       // Keep the same stretching strength in both horizontal and vertical directions
	windows->FOV_h = 90;           // Size of output field of view in vertical dimension in degrees
	windows->FOV_w = 90;           // Size of output field of view in horizontal dimension in degrees
	windows->cylindricity_y = 0;   // Level of cylindricity for target projection shape in vertical direction
	windows->cylindricity_x = 0;   // Level of cylindricity for target projection shape in horizontal direction
	windows->trapezoid_left_angle = 90;  //  Left Acute angle in degrees between trapezoid base and leg
	windows->trapezoid_right_angle = 90; //  Right Acute angle in degrees between trapezoid base and leg
	windows->pan = 1;             // Target shift in horizontal direction from centre of the output image in pixels
	windows->tilt = 1;            // Target shift in vertical direction from centre of the output image in pixels
	windows->zoom = 1;            // Target zoom dimensionless coefficient (must not be bigger than zero)

	// Output window position and size
	windows->out_r.x = 0;
	windows->out_r.y = 0;
	windows->out_r.w = width;
	windows->out_r.h = height;

	// input roi
	windows->input_roi_r.x = 0;
	windows->input_roi_r.y = 0;
	windows->input_roi_r.w = width;
	windows->input_roi_r.h = height;
	// The type of transformation applied to the image
	windows->transform = PANORAMIC;
	return 1;
}

int create_start_gdc_vnode(gdc_info_s *gdc_info, hb_mem_common_buf_t *bin_buf) {
	int ret = 0;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;

	hbn_buf_alloc_attr_t alloc_attr = {0};
	int64_t alloc_flags = 0;
	gdc_attr_t gdc_attr = {0};

	param_t gdc_param ={0};
	window_t windows ={0};
	uint32_t wnd_num = 1;
	uint64_t config_size = 0;
	int offset = 0;
	uint32_t getwidth=gdc_info->input_width;
	uint32_t getheight=gdc_info->input_height;
	uint32_t *cfg_buf = NULL;

	init_windows(&windows,getwidth,getheight);
	memset(&gdc_param, 0, sizeof(gdc_param));

	gdc_param.in.w=getwidth; 			//input frame resolution
	gdc_param.in.h=getheight;
	gdc_param.out.w=getwidth;			//output frame resolution
	gdc_param.out.h=getheight;
	gdc_param.fov=180;					//定义输入图像的视场角
	gdc_param.diameter=1080;			//输入图像的直径，可控制变换网格的整体大小
	gdc_param.x_offset=0;				//center offset for input x coordinate
	gdc_param.y_offset=0;				//center offset for input y coordinate
	gdc_param.format=FMT_SEMIPLANAR_420;//FMT_SEMIPLANAR_420 frame format
	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &gdc_info->gdc_vnode_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_gen_gdc_bin(&gdc_param, &windows, wnd_num, &gdc_info->cfg_buf, &config_size);
	if (ret != 0 || cfg_buf == NULL) {
		printf("hbn_gen_gdc_bin failed \n");
		return -1;
	}
	memset(&gdc_info->bin_buf, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(config_size, alloc_flags, &gdc_info->bin_buf);
	if (ret != 0 || gdc_info->bin_buf.virt_addr == NULL) {
		printf("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		return -1;
	}
	memcpy(gdc_info->bin_buf.virt_addr, cfg_buf, config_size);
	ret = hb_mem_flush_buf(gdc_info->bin_buf.fd, offset, config_size);
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
	gdc_ochn_attr.output_width = gdc_info->input_width;
	gdc_ochn_attr.output_height = gdc_info->input_height;
	gdc_ochn_attr.output_stride = gdc_info->input_width;
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
	hbn_free_gdc_bin(gdc_info->cfg_buf);
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
