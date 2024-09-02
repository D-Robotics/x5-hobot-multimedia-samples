#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include "common_utils.h"
#include "hbn_api.h"
#include "hb_rgn.h"
#include <stdarg.h>

#define vio_test_pr_fmt(fmt, ...) vio_test_pr(__func__, __LINE__, fmt, ##__VA_ARGS__)

typedef struct scaler_info
{
	uint32_t input_width;
	uint32_t input_height;
	char* yuv_file;
} scaler_info_s;

enum work_mode_type {
	cover_test = 1,
	draw_word_test,
	draw_line_test,
} work_mode_type_e;


void vp_vin_print_hbn_frame_info_t(const hbn_frame_info_t *frame_info);
void vp_vin_print_hb_mem_graphic_buf_t(const hb_mem_graphic_buf_t *graphic_buf);
int read_nv12_image(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image);

void vio_test_pr(const char *func, int line, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("[VIO TEST OSD]:[%s][%d]", func, line);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
}

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

static int32_t rgn_draw_word_init(int32_t vse_vnode_fd)
{
	int32_t ret = 0;
	time_t tt = 0;
	int32_t i;
	hbn_rgn_handle_t rgn_handle;
	hbn_rgn_attr_t region;
	hbn_rgn_chn_attr_t chn_attr = {0};
	hbn_rgn_bitmap_t rgn_bitmap;
	hbn_rgn_draw_word_t draw_word = {0};
	char str[32] = {0};

	region.type = OVERLAY_RGN;
	region.color = FONT_COLOR_ORANGE;
	region.alpha = 0;
	region.overlay_attr.size.width = 320;
	region.overlay_attr.size.height = 160;
	region.overlay_attr.pixel_fmt = PIXEL_FORMAT_VGA_8;

	chn_attr.show = true;
	chn_attr.invert_en = 0;
	chn_attr.display_level = 0;
	chn_attr.point.x = 100;
	chn_attr.point.y = 250;

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		hbn_rgn_create(rgn_handle, &region); // 用于创建一块region区域。
	}

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		// 用于将创建好的region区域叠加到VSE的某个通道上。
		// 同一个VSE的通道支持4个硬件OSD区域，多于4个区域时使用软件OSD
		ret = hbn_rgn_attach_to_chn(rgn_handle, vse_vnode_fd, i, &chn_attr);
		ERR_CON_EQ(ret, 0);
	}

	memset(&rgn_bitmap, 0, sizeof(hbn_rgn_bitmap_t));
	rgn_bitmap.pixel_fmt = PIXEL_FORMAT_VGA_8;
	rgn_bitmap.size.width = region.overlay_attr.size.width;
	rgn_bitmap.size.height = region.overlay_attr.size.height;
	rgn_bitmap.paddr = malloc(rgn_bitmap.size.width * rgn_bitmap.size.height);
	memset(rgn_bitmap.paddr, 0x0F, rgn_bitmap.size.width * rgn_bitmap.size.height);

	tt = time(0);
	strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", localtime(&tt));

	draw_word.font_size = FONT_SIZE_MEDIUM;
	draw_word.font_color = FONT_COLOR_WHITE;
	draw_word.bg_color = FONT_COLOR_DARKGRAY;
	draw_word.font_alpha = 15; // VSE 的通道 4 不支持透明度，不等于15时会显示不出来
	draw_word.bg_alpha = 2;
	draw_word.point.x = 0;
	draw_word.point.y = 0;
	draw_word.flush_en = false;
	draw_word.draw_string = (uint8_t*)str;
	draw_word.paddr = rgn_bitmap.paddr;
	draw_word.size = rgn_bitmap.size;

	ret = hbn_rgn_draw_word(&draw_word);
	ERR_CON_EQ(ret, 0);

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		ret = hbn_rgn_setbitmap(rgn_handle, &rgn_bitmap);
		ERR_CON_EQ(ret, 0);
	}

	if (rgn_bitmap.paddr) {
		free(rgn_bitmap.paddr);
	}

	return 0;
}

static int32_t rgn_draw_word_deinit(int32_t vse_vnode_fd)
{
	int32_t i;
	hbn_rgn_handle_t rgn_handle;

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		hbn_rgn_detach_from_chn(rgn_handle, vse_vnode_fd, i);
		hbn_rgn_destroy(rgn_handle);
	}

	return 0;
}

static int32_t rgn_cover_test_init(int32_t vse_vnode_fd)
{
	int32_t i, ret;
	hbn_rgn_handle_t rgn_handle;
	hbn_rgn_attr_t region_polygon;
	hbn_rgn_attr_t region_rect;
	hbn_rgn_attr_t region_get;
	hbn_rgn_chn_attr_t rgn_chn = {0};
	hbn_rgn_chn_attr_t rgn_chn_get = {0};

	memset(&region_polygon, 0, sizeof(hbn_rgn_attr_t));
	memset(&region_rect, 0, sizeof(hbn_rgn_attr_t));

	region_polygon.type = COVER_RGN;
	region_polygon.color = FONT_COLOR_PINK;
	region_polygon.cover_attr.cover_type = COVER_POLYGON;
	region_polygon.cover_attr.polygon.side_num = 3;
	region_polygon.cover_attr.polygon.vertex[0].x = 100;
	region_polygon.cover_attr.polygon.vertex[0].y = 100;
	region_polygon.cover_attr.polygon.vertex[1].x = 300;
	region_polygon.cover_attr.polygon.vertex[1].y = 400;
	region_polygon.cover_attr.polygon.vertex[2].x = 500;
	region_polygon.cover_attr.polygon.vertex[2].y = 200;

	region_rect.type = COVER_RGN;
	region_rect.color = FONT_COLOR_BROWN;
	region_rect.cover_attr.cover_type = COVER_RECT;
	region_rect.cover_attr.size.width = 160;
	region_rect.cover_attr.size.height = 200;

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		if (i < 3) {
			ret = hbn_rgn_create(rgn_handle, &region_polygon);
			ERR_CON_EQ(ret, 0);
		} else {
			ret = hbn_rgn_create(rgn_handle, &region_rect);
			ERR_CON_EQ(ret, 0);
		}
	}

	ret = hbn_rgn_getattr(0, &region_get);
	ERR_CON_EQ(ret, 0);
	region_get.color = FONT_COLOR_DARKBLUE;
	region_get.cover_attr.polygon.side_num = 4;
	region_get.cover_attr.polygon.vertex[0].x = 100;
	region_get.cover_attr.polygon.vertex[0].y = 100;
	region_get.cover_attr.polygon.vertex[1].x = 300;
	region_get.cover_attr.polygon.vertex[1].y = 400;
	region_get.cover_attr.polygon.vertex[2].x = 500;
	region_get.cover_attr.polygon.vertex[2].y = 200;
	region_get.cover_attr.polygon.vertex[3].x = 250;
	region_get.cover_attr.polygon.vertex[3].y = 50;
	ret = hbn_rgn_setattr(0, &region_get);
	ERR_CON_EQ(ret, 0);

	ret = hbn_rgn_getattr(4, &region_get);
	ERR_CON_EQ(ret, 0);
	region_get.color = FONT_COLOR_ORANGE;
	region_get.cover_attr.size.width = 320;
	region_get.cover_attr.size.height = 100;
	ret = hbn_rgn_setattr(4, &region_get);
	ERR_CON_EQ(ret, 0);

	rgn_chn.show = true; // 是否显示
	rgn_chn.invert_en = 0; // 是否反色，暂时不支持
	rgn_chn.display_level = 1; // 显示level，硬件osd时需要为0，软件为1-3， 目前需要设置以区分软硬件，暂时不支持显示level
	rgn_chn.point.x = 100; // 坐标信息
	rgn_chn.point.y = 20;

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		// 用于将创建好的region区域叠加到VSE的某个通道上。
		// 同一个VSE的通道支持4个硬件OSD区域，多于4个区域时使用软件OSD
		ret = hbn_rgn_attach_to_chn(rgn_handle, vse_vnode_fd, i, &rgn_chn);
		ERR_CON_EQ(ret, 0);
	}

	ret = hbn_rgn_get_displayattr(3, vse_vnode_fd, 3, &rgn_chn_get);
	ERR_CON_EQ(ret, 0);
	rgn_chn_get.point.x = 20;
	rgn_chn_get.point.y = 100;
	ret = hbn_rgn_set_displayattr(3, vse_vnode_fd, 3, &rgn_chn_get);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static void rgn_cover_test_deinit(int32_t vse_vnode_fd)
{
	int32_t i;
	hbn_rgn_handle_t rgn_handle;

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		hbn_rgn_detach_from_chn(rgn_handle, vse_vnode_fd, i);
		hbn_rgn_destroy(rgn_handle);
	}
}

static int32_t rgn_draw_line_init(int32_t vse_vnode_fd,hbn_vnode_image_t *input_image)
{
	int32_t i, ret;
	hbn_rgn_handle_t rgn_handle;
	hbn_rgn_attr_t region;
	hbn_rgn_chn_attr_t chn_attr = {0};
	hbn_rgn_bitmap_t rgn_bitmap;
	hbn_rgn_draw_line_t draw_line = {0};

	region.type = OVERLAY_RGN; // 将某一个图像覆盖到通道上
	region.color = FONT_COLOR_YELLOW; // 黄色
	region.alpha = 0; // alpha混合系数， 暂时不支持
	region.overlay_attr.size.width = 320; // 叠加属性
	region.overlay_attr.size.height = 160;

	region.overlay_attr.pixel_fmt = PIXEL_FORMAT_VGA_8; // 8bit 颜色

	chn_attr.show = true; // 是否显示
	chn_attr.invert_en = 0; // 是否反色，暂时不支持
	chn_attr.display_level = 0; // 显示level，硬件osd时需要为0，软件为1-3， 目前需要设置以区分软硬件，暂时不支持显示level
	chn_attr.point.x = 100; // 坐标信息
	chn_attr.point.y = 250; // 坐标信息

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		ret = hbn_rgn_create(rgn_handle, &region);
		ERR_CON_EQ(ret, 0);
	}

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		ret = hbn_rgn_attach_to_chn(rgn_handle, vse_vnode_fd, i, &chn_attr);
		ERR_CON_EQ(ret, 0);
	}

	memset(&rgn_bitmap, 0, sizeof(hbn_rgn_bitmap_t));
	rgn_bitmap.pixel_fmt = PIXEL_FORMAT_VGA_8; // 数据格式类型
	rgn_bitmap.size = region.overlay_attr.size; // 尺寸信息
	rgn_bitmap.paddr = malloc(region.overlay_attr.size.width * region.overlay_attr.size.height); // 要输入的buffer地址
	memset(rgn_bitmap.paddr, 0, region.overlay_attr.size.width * region.overlay_attr.size.height);

	draw_line.paddr = rgn_bitmap.paddr; // 要画线的buffer地址
	draw_line.size = rgn_bitmap.size; // 画布的大小

	draw_line.thick = 4; // 线宽
	draw_line.flush_en = false; // 是否刷新画布
	draw_line.color = FONT_COLOR_RED; // 画线颜色
	draw_line.alpha = 15; // 背景透明度，线的透明度默认为15，硬件支持 背景半透明，软件目前只支持背景完全透明

	for (i = 0; i < 4; i++) {
		if (i == 0) {
			draw_line.start_point.x = 20; // 画线的起始坐标
			draw_line.start_point.y = 20;
			draw_line.end_point.x = 200; // 画线的终止坐标
			draw_line.end_point.y = 20;
		} else if (i == 1) {
			draw_line.start_point.x = 20;
			draw_line.start_point.y = 20;
			draw_line.end_point.x = 20;
			draw_line.end_point.y = 100;
		} else if (i == 2) {
			draw_line.start_point.x = 20;
			draw_line.start_point.y = 100;
			draw_line.end_point.x = 150;
			draw_line.end_point.y = 150;
		} else {
			draw_line.start_point.x = 150;
			draw_line.start_point.y = 150;
			draw_line.end_point.x = 200;
			draw_line.end_point.y = 20;
		}
		ret = hbn_rgn_draw_line(&draw_line);
		ERR_CON_EQ(ret, 0);
	}
	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		ret = hbn_rgn_setbitmap(rgn_handle, &rgn_bitmap);
		ERR_CON_EQ(ret, 0);
	}
	if (rgn_bitmap.paddr) {
		free(rgn_bitmap.paddr);
	}

	return 0;
}

static int32_t rgn_draw_line_deinit(int32_t vse_vnode_fd)
{
	int32_t i;
	hbn_rgn_handle_t rgn_handle;

	for (i = 0; i < 6; i++) {
		rgn_handle = i;
		hbn_rgn_detach_from_chn(rgn_handle, vse_vnode_fd, i);
		hbn_rgn_destroy(rgn_handle);
	}

	return 0;
}

static int32_t rgn_test_init(int32_t vse_vnode_fd, int32_t mode,hbn_vnode_image_t *input_image)
{
	int32_t ret = 0;

	switch (mode) {
	case cover_test:
		ret = rgn_cover_test_init(vse_vnode_fd);
		break;
	case draw_word_test:
		ret = rgn_draw_word_init(vse_vnode_fd);
		break;
	case draw_line_test:
		ret = rgn_draw_line_init(vse_vnode_fd,input_image);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static void rgn_test_deinit(int32_t vse_vnode_fd, int32_t mode)
{
	switch (mode) {
	case cover_test:
		rgn_cover_test_deinit(vse_vnode_fd);
		break;
	case draw_word_test:
		rgn_draw_word_deinit(vse_vnode_fd);
		break;
	case draw_line_test:
		rgn_draw_line_deinit(vse_vnode_fd);
		break;
	default:
		break;
	}
}

int read_nv12_image(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image) {
	int64_t alloc_flags = 0;
	int ret = 0;
	//uint64_t offset = 0;
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
	ret = read_yuvv_nv12_file(input_image_path,
					(char *)(input_image->buffer.virt_addr[0]),
					(char *)(input_image->buffer.virt_addr[1]),
					input_image->buffer.size[0]);
	ERR_CON_EQ(ret, 0);

	// 设置一个时间戳
	gettimeofday(&input_image->info.tv, NULL);

	return ret;
}


int create_and_run_vflow(scaler_info_s *scaler_info, hbn_vnode_image_t *input_image,int32_t work_mode) {
	int32_t ret;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;
	hbn_vflow_handle_t vflow_fd = 0;
	hbn_vnode_handle_t vse_vnode_fd;
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
		configure_vse_max_resolution(i,
			input_width, input_height,
			&vse_ochn_attr[i].target_w, &vse_ochn_attr[i].target_h);
	}

	// 设置为一个常用的算法模型使用的分辨率
	vse_ochn_attr[4].target_w = 672;
	vse_ochn_attr[4].target_h = 672;

	// 放大到支持的最大分辨率
	vse_ochn_attr[5].target_w = (input_width * 2) > 4096 ? 4096 : (input_width * 2);
	vse_ochn_attr[5].target_h = (input_height * 2) > 3076 ? 3076 : (input_height * 2);

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, &vse_vnode_fd);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(vse_vnode_fd, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(vse_vnode_fd, 0, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;

	for (int i = 0; i < VSE_MAX_CHANNELS; ++i) {
		printf("hbn_vnode_set_ochn_attr: %d, %dx%d\n",
			i, vse_ochn_attr[i].target_w, vse_ochn_attr[i].target_h);
		ret = hbn_vnode_set_ochn_attr(vse_vnode_fd, i, &vse_ochn_attr[i]);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_set_ochn_buf_attr(vse_vnode_fd, i, &alloc_attr);
		ERR_CON_EQ(ret, 0);
	}

	ret = hbn_vflow_create(&vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(vflow_fd, vse_vnode_fd);
	ERR_CON_EQ(ret, 0);

	ret = rgn_test_init(vse_vnode_fd, work_mode,input_image);
	if (ret < 0)
		return -1;

	hbn_vflow_start(vflow_fd);

	vp_vin_print_hbn_vnode_image_t(input_image);

	// 发送帧并获取输出图像
	ret = hbn_vnode_sendframe(vse_vnode_fd, 0, input_image);
	ERR_CON_EQ(ret, 0);

	// 循环处理每个输出通道
	for (chn_id = 0; chn_id < VSE_MAX_CHANNELS; chn_id++) {
		ret = hbn_vnode_getframe(vse_vnode_fd, chn_id, timeout, &output_img);
		ERR_CON_EQ(ret, 0);

		//vp_vin_print_hbn_vnode_image_t(&output_img);

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
		printf("Dump image to file(%s), size(%ld) + size1(%ld) succeeded\n", output_image_path, output_img.buffer.size[0],output_img.buffer.size[1]);
		// 释放输出图像内存
		ret = hbn_vnode_releaseframe(vse_vnode_fd, chn_id, &output_img);
		ERR_CON_EQ(ret, 0);

	}

	hbn_vflow_stop(vflow_fd);
	rgn_test_deinit(vse_vnode_fd, work_mode);
	hbn_vflow_destroy(vflow_fd);

	return ret;
}

static struct option const long_options[] = {
	{"input_file", required_argument, NULL, 'i'},
	{"input_width", required_argument, NULL, 'w'},
	{"input_height", required_argument, NULL, 'h'},
	{"work_mode", required_argument, NULL, 'm'},
	{NULL, 0, NULL, 0}
};

static void print_help() {
	printf("Usage: sample_osd [OPTIONS]\n");
	printf("Options:\n");
	printf("-i, --input_file FILE\t\tSpecify the input file\n");
	printf("-w, --input_width WIDTH\t\tSpecify the input width\n");
	printf("-h, --input_height HEIGHT\tSpecify the input height\n");
	printf("-m, --work_mode\t\t\t1.cover_test 2.draw_word_test 3.draw_line_test\n");
}

int main(int argc, char** argv){
	hbn_vnode_image_t input_image = {0};
	int ret = 0;
	scaler_info_s scaler_info = {0};
	int32_t work_mode;
	int opt_index = 0;
	int c = 0;

	// 检查标准输入是否来自终端
	if ((!isatty(fileno(stdin))) || (argc == 1)) {
		print_help();
		return 0;
	}

	while((c = getopt_long(argc, argv, "i:w:h:m:",
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
			case 'm':
				work_mode = atoi(optarg);
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
	ret = create_and_run_vflow(&scaler_info, &input_image,work_mode);
	ERR_CON_EQ(ret, 0);
	ret = hb_mem_free_buf(input_image.buffer.fd[0]);
	hb_mem_module_close();

	return 0;
}
