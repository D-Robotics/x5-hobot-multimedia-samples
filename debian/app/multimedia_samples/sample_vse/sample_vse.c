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

#include "common_utils.h"

#define VSE_MAX_CHANNELS 6

typedef struct scaler_info
{
	uint32_t input_width;
	uint32_t input_height;
	char* yuv_file;
} scaler_info_s;

static int verbose_flag = 0;

static struct option const long_options[] = {
	{"input_file", required_argument, NULL, 'i'},
	{"input_width", required_argument, NULL, 'w'},
	{"input_height", required_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};

int read_nv12_image(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image);
int create_and_run_vflow(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image);

static void print_help() {
	printf("Usage: sample_vse [OPTIONS]\n");
	printf("Options:\n");
	printf("-i, --input_file FILE\tSpecify the input file\n");
	printf("-w, --input_width WIDTH\tSpecify the input width\n");
	printf("-h, --input_height HEIGHT\tSpecify the input height\n");
	printf("-V, --verbose\t\tEnable verbose mode\n");
}

int main(int argc, char** argv) {
	hbn_vnode_image_t input_image = {0};
	int ret = 0;
	scaler_info_s scaler_info = {0};
	int opt_index = 0;
	int c = 0;

	// 检查标准输入是否来自终端
	if ((!isatty(fileno(stdin))) || (argc == 1)) {
		print_help();
		return 0;
	}

	while((c = getopt_long(argc, argv, "i:w:h:V",
							long_options, &opt_index)) != -1) {
		switch (c)
		{
			case 'i':
				scaler_info.yuv_file = optarg;
				break;
			case 'w':
				scaler_info.input_width = atoi(optarg);
				break;
			case 'h':
				scaler_info.input_height = atoi(optarg);
				break;
			case 'V':
				verbose_flag = 1;
				break;

			default:
				print_help();
				return 0;
		}
	}
	printf("Using input file:%s, input:%dx%d\n",
			scaler_info.yuv_file,
			scaler_info.input_width, scaler_info.input_height);

	ret = hb_mem_module_open();
	ERR_CON_EQ(ret, 0);
	ret = read_nv12_image(&scaler_info, &input_image);
	ERR_CON_EQ(ret, 0);
	ret = create_and_run_vflow(&scaler_info, &input_image);
	ERR_CON_EQ(ret, 0);
	ret = hb_mem_free_buf(input_image.buffer.fd[0]);
	hb_mem_module_close();

	return 0;
}

int read_nv12_image(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image) {
	int64_t alloc_flags = 0;
	int ret = 0;
	// uint64_t offset = 0;
	char *input_image_path = scaler_info->yuv_file;
	memset(input_image, 0, sizeof(hbn_vnode_image_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
				HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD |
				HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN |
				HB_MEM_USAGE_CACHED |
				HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hb_mem_alloc_graph_buf(scaler_info->input_width,
								scaler_info->input_height,
								MEM_PIX_FMT_NV12,
								alloc_flags,
								scaler_info->input_width,
								scaler_info->input_height,
								&input_image->buffer);
	ERR_CON_EQ(ret, 0);
	read_yuvv_nv12_file(input_image_path,
					(char *)(input_image->buffer.virt_addr[0]),
					(char *)(input_image->buffer.virt_addr[1]),
					input_image->buffer.size[0]);

	// 设置一个时间戳
	gettimeofday(&input_image->info.tv, NULL);

	return ret;
}

void vp_vin_print_hbn_frame_info_t(const hbn_frame_info_t *frame_info);
void vp_vin_print_hb_mem_graphic_buf_t(const hb_mem_graphic_buf_t *graphic_buf);

// 打印 hbn_vnode_image_t 结构体的所有字段内容
void vp_vin_print_hbn_vnode_image_t(const hbn_vnode_image_t *frame)
{
	printf("=== Frame Info ===\n");
	vp_vin_print_hbn_frame_info_t(&(frame->info));
	printf("\n=== Graphic Buffer ===\n");
	vp_vin_print_hb_mem_graphic_buf_t(&(frame->buffer));
}

// 打印 hbn_frame_info_t 结构体的所有字段内容
void vp_vin_print_hbn_frame_info_t(const hbn_frame_info_t *frame_info) {
	printf("Frame ID: %u\n", frame_info->frame_id);
	printf("Timestamps: %lu\n", frame_info->timestamps);
	printf("tv: %ld.%06ld\n", frame_info->tv.tv_sec, frame_info->tv.tv_usec);
	printf("trig_tv: %ld.%06ld\n", frame_info->trig_tv.tv_sec, frame_info->trig_tv.tv_usec);
	printf("Frame Done: %u\n", frame_info->frame_done);
	printf("Buffer Index: %d\n", frame_info->bufferindex);
}

// 打印 hb_mem_graphic_buf_t 结构体的所有字段内容
void vp_vin_print_hb_mem_graphic_buf_t(const hb_mem_graphic_buf_t *graphic_buf) {
	printf("File Descriptors: ");
	for (int i = 0; i < MAX_GRAPHIC_BUF_COMP; i++) {
		printf("%d ", graphic_buf->fd[i]);
	}
	printf("\n");

	printf("Plane Count: %d\n", graphic_buf->plane_cnt);
	printf("Format: %d\n", graphic_buf->format);
	printf("Width: %d\n", graphic_buf->width);
	printf("Height: %d\n", graphic_buf->height);
	printf("Stride: %d\n", graphic_buf->stride);
	printf("Vertical Stride: %d\n", graphic_buf->vstride);
	printf("Is Contiguous: %d\n", graphic_buf->is_contig);

	printf("Share IDs: ");
	for (int i = 0; i < MAX_GRAPHIC_BUF_COMP; i++) {
		printf("%d ", graphic_buf->share_id[i]);
	}
	printf("\n");

	printf("Flags: %ld\n", graphic_buf->flags);

	printf("Sizes: ");
	for (int i = 0; i < MAX_GRAPHIC_BUF_COMP; i++) {
		printf("%lu ", graphic_buf->size[i]);
	}
	printf("\n");

	printf("Virtual Addresses: ");
	for (int i = 0; i < MAX_GRAPHIC_BUF_COMP; i++) {
		printf("%p ", graphic_buf->virt_addr[i]);
	}
	printf("\n");

	printf("Physical Addresses: ");
	for (int i = 0; i < MAX_GRAPHIC_BUF_COMP; i++) {
		printf("%lu ", graphic_buf->phys_addr[i]);
	}
	printf("\n");

	printf("Offsets: ");
	for (int i = 0; i < MAX_GRAPHIC_BUF_COMP; i++) {
		printf("%lu ", graphic_buf->offset[i]);
	}
	printf("\n");
}

int create_and_run_vflow(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image) {
	int ret = 0;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;
	hbn_vflow_handle_t vflow_fd;
	hbn_vnode_handle_t vnode_fd;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	hbn_vnode_image_t output_img = {0};
	char output_image_path[128];

	vse_attr_t vse_attr = {0};
	vse_ichn_attr_t vse_ichn_attr = {0};
	vse_ochn_attr_t vse_ochn_attr[VSE_MAX_CHANNELS] = {0};
	uint32_t input_width = 0;
	uint32_t input_height = 0;
	int timeout = 1000;

	input_width = scaler_info->input_width;
	input_height = scaler_info->input_height;

	printf("ichn input width = %d\n", input_width);
	printf("ichn input input_height = %d\n", input_height);

	vse_ichn_attr.width = input_width;
	vse_ichn_attr.height = input_height;
	vse_ichn_attr.fmt = FRM_FMT_NV12;
	vse_ichn_attr.bit_width = 8;

	for (int i = 0; i < VSE_MAX_CHANNELS; ++i) {
		vse_ochn_attr[i].chn_en = CAM_TRUE;
		vse_ochn_attr[i].roi.x = 0;
		vse_ochn_attr[i].roi.y = 0;
		vse_ochn_attr[i].roi.w = input_width;
		vse_ochn_attr[i].roi.h = input_height;
		vse_ochn_attr[i].fmt = FRM_FMT_NV12;
		vse_ochn_attr[i].bit_width = 8;
	}

	// 输出原分辨率
	vse_ochn_attr[0].target_w = input_width;
	vse_ochn_attr[0].target_h = input_height;

	// 输出 16 像素对齐的常用算法图像使用的分辨率
	vse_ochn_attr[1].target_w = 512;
	vse_ochn_attr[1].target_h = 512;

	// 输出常用算法图像使用的分辨率
	vse_ochn_attr[2].target_w = 224;
	vse_ochn_attr[2].target_h = 224;

	// 设置VSE通道3输出属性，ROI为原图中心点不变，宽、高各裁剪一半，输出图像宽、高等于ROI区域宽高
	vse_ochn_attr[3].roi.x = input_width / 2 - input_width / 4;
	vse_ochn_attr[3].roi.y = input_height / 2 - input_height / 4;
	vse_ochn_attr[3].roi.w = input_width / 2;
	vse_ochn_attr[3].roi.h = input_height / 2;

	// 缩小到支持的最小分辨率
	vse_ochn_attr[3].target_w = 64;
	vse_ochn_attr[3].target_h = 64;

	vse_ochn_attr[4].target_w = 672;
	vse_ochn_attr[4].target_h = 672;

	// 放大到支持的最大分辨率
	vse_ochn_attr[5].target_w = (input_width * 4) > 4096 ? 4096 : (input_width * 4);
	vse_ochn_attr[5].target_h = (input_height * 4) > 3076 ? 3076 : (input_height * 4);

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, &vnode_fd);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(vnode_fd, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(vnode_fd, 0, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;

	for (int i = 0; i < VSE_MAX_CHANNELS; ++i) {
		printf("hbn_vnode_set_ochn_attr: %d, %dx%d\n", i, vse_ochn_attr[i].target_w, vse_ochn_attr[i].target_h);
		ret = hbn_vnode_set_ochn_attr(vnode_fd, i, &vse_ochn_attr[i]);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_set_ochn_buf_attr(vnode_fd, i, &alloc_attr);
		ERR_CON_EQ(ret, 0);
	}

	ret = hbn_vflow_create(&vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(vflow_fd, vnode_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(vflow_fd);
	ERR_CON_EQ(ret, 0);

	if (verbose_flag) {
		vp_vin_print_hbn_vnode_image_t(input_image);
	}

	// 发送帧并获取输出图像
	ret = hbn_vnode_sendframe(vnode_fd, 0, input_image);
	ERR_CON_EQ(ret, 0);

	// 循环处理每个输出通道
	for (chn_id = 0; chn_id < VSE_MAX_CHANNELS; chn_id++) {
		ret = hbn_vnode_getframe(vnode_fd, chn_id, timeout, &output_img);
		ERR_CON_EQ(ret, 0);

		if (verbose_flag) {
			vp_vin_print_hbn_vnode_image_t(&output_img);
		}

		// 根据当前输出通道属性设置输出文件路径
		snprintf(output_image_path, sizeof(output_image_path),
			"./vse_output_nv12_chn%d_%dx%d_stride_%d.yuv",
			chn_id, output_img.buffer.width, output_img.buffer.height,
			output_img.buffer.stride);
		// 保存输出图像到文件
		dump_2plane_yuv_to_file(output_image_path,
						output_img.buffer.virt_addr[0],
						output_img.buffer.virt_addr[1],
						output_img.buffer.size[0],
						output_img.buffer.size[1]);

		// 释放输出图像内存
		ret = hbn_vnode_releaseframe(vnode_fd, chn_id, &output_img);
		ERR_CON_EQ(ret, 0);

	}

	ret = hbn_vflow_stop(vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(vflow_fd);

	return ret;
}