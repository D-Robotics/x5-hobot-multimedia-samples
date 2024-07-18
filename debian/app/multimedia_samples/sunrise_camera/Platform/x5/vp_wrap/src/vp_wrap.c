// #define J5
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "utils/cJSON.h"
#include "utils/utils_log.h"
#include "utils/common_utils.h"

#include "vp_vin.h"
#include "vp_wrap.h"

#include "vp_sensors.h"

void *vp_allocate_image_frame(ImageFrame *image_frame)
{
	memset(image_frame, 0, sizeof(ImageFrame));
	image_frame->frame_buffer = (media_codec_buffer_t *)malloc(sizeof(media_codec_buffer_t));
	if (image_frame->frame_buffer == NULL) {
		SC_LOGE("malloc media_codec_buffer_t failed.\n");
		return NULL;
	}

	image_frame->buffer_info = (media_codec_output_buffer_info_t *)malloc(sizeof(media_codec_output_buffer_info_t));
	if (image_frame->buffer_info == NULL) {
		SC_LOGE("malloc media_codec_output_buffer_info_t failed.\n");
		return NULL;
	}

	image_frame->hbn_vnode_image = (hbn_vnode_image_t *)malloc(sizeof(hbn_vnode_image_t));
	if (image_frame->hbn_vnode_image == NULL) {
		SC_LOGE("malloc hbn_vnode_image_t failed.\n");
		return NULL;
	}

	return image_frame;
}

void vp_free_image_frame(ImageFrame *image_frame)
{
	if (image_frame == NULL) {
		return;
	}

	if (image_frame->frame_buffer != NULL) {
		free(image_frame->frame_buffer);
		image_frame->frame_buffer = NULL;
	}

	if (image_frame->buffer_info != NULL) {
		free(image_frame->buffer_info);
		image_frame->buffer_info = NULL;
	}

	if (image_frame->hbn_vnode_image != NULL) {
		free(image_frame->hbn_vnode_image);
		image_frame->hbn_vnode_image = NULL;
	}
}


// 获取主芯片类型
static int32_t vp_get_chip_type(char *chip_type)
{
	int ret = 0;
	FILE *stream;
	char chip_id[16] = {0};

	stream = fopen("/sys/class/socinfo/chip_id", "r");
	if (!stream) {
		SC_LOGE("open fail: %s", strerror(errno));
		return -1;
	}
	ret = fread(chip_id, sizeof(char), 9, stream);
	if (ret != 9) {
		SC_LOGE("read fail: %s", strerror(errno));
		fclose(stream);
		return -1;
	}
	fclose(stream);

	// TODO: 先写死，后面有判断方法后再补充正确的逻辑
	strcpy(chip_type, "X5");

	return ret;
}

int32_t vp_get_hard_capability(solution_cfg_t *solution_config)
{
	solution_hard_capability_t *capability = &solution_config->hardware_capability;

	vp_get_chip_type(capability->chip_type);
	SC_LOGI("chip_type: %s", capability->chip_type);
	vp_sensor_detect_structed(&capability->csi_list_info);
	return 0;
}

void vp_print_debug_infos(void)
{
	printf("================= VP Modules Status ====================\n");
	printf("======================== VFLOW =========================\n");
	print_file("/sys/devices/virtual/vps/flow/path_stat");
	printf("========================= SIF ==========================\n");
	print_file("/sys/devices/platform/soc/soc:cam/3d050000.sif/cim_stat");
	print_file("/sys/devices/platform/soc/soc:cam/3d020000.sif/cim_stat");
	print_file("/sys/devices/platform/soc/soc:cam/3d040000.sif/cim_stat");
	print_file("/sys/devices/platform/soc/soc:cam/3d030000.sif/cim_stat");
	printf("========================= ISP ==========================\n");
	print_file("/sys/devices/platform/soc/soc:cam/3d000000.isp/stat");
	printf("========================= VSE ==========================\n");
	print_file("/sys/devices/platform/soc/soc:cam/3d010000.vse/stat");
	printf("========================= VENC =========================\n");
	print_file("/sys/kernel/debug/vpu/venc");
	printf("========================= VDEC =========================\n");
	print_file("/sys/kernel/debug/vpu/vdec");
	printf("========================= JENC =========================\n");
	print_file("/sys/kernel/debug/jpu/jenc");

	printf("======================= Buffer =========================\n");
	print_file("/sys/devices/virtual/vps/flow/fmgr_stats");

	if (log_ctrl_level_get(NULL) == LOG_DEBUG) {
		printf("========================= ION ==========================\n");
		print_file("/sys/kernel/debug/ion/heaps/carveout");
		// print_file("/sys/kernel/debug/ion/heaps/ion_cma");
		print_file("/sys/kernel/debug/ion/ion_buf");
	}
	printf("========================= END ===========================\n");
}

void vp_normal_buf_info_print(ImageFrame *frame)
{
}

int32_t vp_dump_1plane_image_to_file(char *filename, uint8_t *src_buffer, uint32_t size)
{
	int image_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (image_fd == -1) {
		SC_LOGE("Error opening file(%s)", filename);
		return -1;
	}

	ssize_t bytes_written = write(image_fd, src_buffer, size);
	close(image_fd);

	if (bytes_written != size) {
		SC_LOGE("Error writing to file");
		return -1;
	}
	return 0;
}

int32_t vp_dump_yuv_to_file(char *filename, uint8_t *src_buffer, uint32_t size)
{
	int yuv_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (yuv_fd == -1) {
		SC_LOGE("Error opening file(%s)", filename);
		return -1;
	}

	ssize_t bytes_written = write(yuv_fd, src_buffer, size);
	close(yuv_fd);

	if (bytes_written != size) {
		SC_LOGE("Error writing to file");
		return -1;
	}

	// SC_LOGI("Dump yuv to file(%s), size(%d) succeeded\n", filename, size);
	return 0;
}

int32_t vp_dump_nv12_to_file(char *filename, uint8_t *data_y, uint8_t *data_uv,
		int width, int height)
{
	int yuv_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (yuv_fd == -1) {
		SC_LOGE("Error opening file(%s)", filename);
		return -1;
	}

	ssize_t bytes_written = write(yuv_fd, data_y, width * height);
	if (bytes_written != width * height) {
		SC_LOGE("Error writing to file");
		close(yuv_fd);
		return -1;
	}

	bytes_written = write(yuv_fd, data_uv,  width * height / 2);
	if (bytes_written != width * height /2) {
		SC_LOGE("Error writing to file");
		close(yuv_fd);
		return -1;
	}

	close(yuv_fd);

	// SC_LOGI("Dump yuv to file(%s), size(%d) + size1(%d) succeeded\n", filename, size, size1);
	return 0;
}

int32_t vp_dump_2plane_yuv_to_file(char *filename, uint8_t *src_buffer, uint8_t *src_buffer1,
		uint32_t size, uint32_t size1)
{
	int yuv_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (yuv_fd == -1) {
		SC_LOGE("Error opening file(%s)", filename);
		return -1;
	}

	ssize_t bytes_written = write(yuv_fd, src_buffer, size);
	if (bytes_written != size) {
		SC_LOGE("Error writing to file");
		close(yuv_fd);
		return -1;
	}

	bytes_written = write(yuv_fd, src_buffer1, size1);
	if (bytes_written != size1) {
		SC_LOGE("Error writing to file");
		close(yuv_fd);
		return -1;
	}

	close(yuv_fd);

	// SC_LOGI("Dump yuv to file(%s), size(%d) + size1(%d) succeeded\n", filename, size, size1);
	return 0;
}

// 函数声明
void vp_vin_print_hbn_frame_info_t(const hbn_frame_info_t *frame_info);
void vp_vin_print_hb_mem_graphic_buf_t(const hb_mem_graphic_buf_t *graphic_buf);

// 打印 hbn_vnode_image_t 结构体的所有字段内容
void vp_vin_print_hbn_vnode_image_t(const hbn_vnode_image_t *frame)
{
	printf("=== hbn_vnode_image ===\n");
	printf("=== Frame Info ===\n");
	vp_vin_print_hbn_frame_info_t(&(frame->info));
	printf("\n=== Graphic Buffer ===\n");
	vp_vin_print_hb_mem_graphic_buf_t(&(frame->buffer));
	printf("\n=== Metadata ===\n");
	printf("Metadata: %p\n", frame->metadata);
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
