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
#include <pthread.h>

#include "common_utils.h"

#define VSE_MAX_CHANNELS 6

static struct option const long_options[] = {
	{"sensor", required_argument, NULL, 's'},
	{NULL, 0, NULL, 0}
};


int32_t running = 0;

int create_and_run_vflow(pipe_contex_t *pipe_contex);
void *read_vse_data(void *contex);

static void print_help() {
	printf("single_pipe_vin_isp_vse -s/--sensor sensor_index\n");
	vp_show_sensors_list();
}

void signal_handle(int signo) {
	running = 0;
}


int main(int argc, char** argv) {
	int ret = 0;
	pipe_contex_t pipe_contex = {0};
	pthread_t read_thread;
	int opt_index = 0;
	int c = 0;
	int index = -1;

	while((c = getopt_long(argc, argv, "s:h",
							long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			index = atoi(optarg);
			break;
		case 'h':
		default:
			print_help();
			return 0;
		}
	}
	if (index < vp_get_sensors_list_number() && index >= 0) {
		pipe_contex.sensor_config = vp_sensor_config_list[index];
		printf("Using index:%d  sensor_name:%s  config_file:%s\n",
				index,
				vp_sensor_config_list[index]->sensor_name,
				vp_sensor_config_list[index]->config_file);
		ret = vp_sensor_fixed_mipi_host(pipe_contex.sensor_config, &pipe_contex.csi_config);
		if (ret != 0) {
			printf("No Camera Sensor found. Please check if the specified "
				"sensor is connected to the Camera interface.\n");
			return ret;
		}
	} else {
		printf("Unsupport sensor index:%d\n", index);
		print_help();
		return 0;
	}


	hb_mem_module_open();
	ret = create_and_run_vflow(&pipe_contex);
	ERR_CON_EQ(ret, 0);
	running = 1;
	ret = pthread_create(&read_thread, NULL, (void *)read_vse_data,
						(void *)&pipe_contex);
	pthread_join(read_thread, NULL);
	ret = hbn_vflow_stop(pipe_contex.vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(pipe_contex.vflow_fd);
	hb_mem_module_close();

	return 0;
}

static int create_camera_node(pipe_contex_t *pipe_contex) {

	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;
	int32_t ret = 0;

	sensor_config = pipe_contex->sensor_config;
	camera_config = sensor_config->camera_config;
	ret = hbn_camera_create(camera_config, &pipe_contex->cam_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vin_node(pipe_contex_t *pipe_contex) {
	vp_sensor_config_t *sensor_config = NULL;
	vin_node_attr_t *vin_node_attr = NULL;
	vin_ichn_attr_t *vin_ichn_attr = NULL;
	vin_ochn_attr_t *vin_ochn_attr = NULL;
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	uint32_t hw_id = 0;
	int32_t ret = 0;
	uint32_t chn_id = 0;
	uint64_t vin_attr_ex_mask = 0;

	sensor_config = pipe_contex->sensor_config;
	vin_node_attr = sensor_config->vin_node_attr;
	vin_ichn_attr = sensor_config->vin_ichn_attr;
	vin_ochn_attr = sensor_config->vin_ochn_attr;
	hw_id = vin_node_attr->cim_attr.mipi_rx;
	vin_node_handle = &pipe_contex->vin_node_handle;

	if(pipe_contex->csi_config.mclk_is_not_configed){
		//设备树中没有配置mclk：使用外部晶振
		printf("csi%d ignore mclk ex attr, because not config mclk.\n",
			pipe_contex->csi_config.index);
	}else{
		vin_attr_ex.vin_attr_ex_mask = 0x80;	//bit7 for mclk
		vin_attr_ex.mclk_ex_attr.mclk_freq = 24000000; // 24MHz
	}

	ret = hbn_vnode_open(HB_VIN, hw_id, AUTO_ALLOC_ID, vin_node_handle);
	ERR_CON_EQ(ret, 0);
	// 设置基本属性
	ret = hbn_vnode_set_attr(*vin_node_handle, vin_node_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输入通道的属性
	ret = hbn_vnode_set_ichn_attr(*vin_node_handle, chn_id, vin_ichn_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输出通道的属性
	ret = hbn_vnode_set_ochn_attr(*vin_node_handle, chn_id, vin_ochn_attr);
	ERR_CON_EQ(ret, 0);
	vin_attr_ex_mask = vin_attr_ex.vin_attr_ex_mask;
	if (vin_attr_ex_mask) {
		for (uint8_t i = 0; i < VIN_ATTR_EX_INVALID; i ++) {
			if ((vin_attr_ex_mask & (1 << i)) == 0)
				continue;

			vin_attr_ex.ex_attr_type = i;
			/*we need to set hbn_vnode_set_attr_ex in a loop*/
			ret = hbn_vnode_set_attr_ex(*vin_node_handle, &vin_attr_ex);
			ERR_CON_EQ(ret, 0);
		}
	}

	return 0;
}

static int create_isp_node(pipe_contex_t *pipe_contex) {
	vp_sensor_config_t *sensor_config = NULL;
	isp_attr_t      *isp_attr = NULL;
	isp_ichn_attr_t *isp_ichn_attr = NULL;
	isp_ochn_attr_t *isp_ochn_attr = NULL;
	hbn_vnode_handle_t *isp_node_handle = NULL;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t chn_id = 0;
	int ret = 0;

	sensor_config = pipe_contex->sensor_config;
	isp_attr = sensor_config->isp_attr;
	isp_ichn_attr = sensor_config->isp_ichn_attr;
	isp_ochn_attr = sensor_config->isp_ochn_attr;
	isp_node_handle = &pipe_contex->isp_node_handle;

	ret = hbn_vnode_open(HB_ISP, 0, AUTO_ALLOC_ID, isp_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_attr(*isp_node_handle, isp_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_attr(*isp_node_handle, chn_id, isp_ochn_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ichn_attr(*isp_node_handle, chn_id, isp_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, chn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vse_node(pipe_contex_t *pipe_contex) {
	int ret = 0;
	hbn_vnode_handle_t *vse_node_handle = &pipe_contex->vse_node_handle;
	isp_ichn_attr_t isp_ichn_attr = {0};
	vse_attr_t vse_attr = {0};
	vse_ichn_attr_t vse_ichn_attr = {0};
	vse_ochn_attr_t vse_ochn_attr[VSE_MAX_CHANNELS] = {0};
	uint32_t chn_id = 0;
	uint32_t hw_id = 0;
	uint32_t input_width = 0;
	uint32_t input_height = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	ret = hbn_vnode_get_ichn_attr(pipe_contex->isp_node_handle, chn_id, &isp_ichn_attr);
	ERR_CON_EQ(ret, 0);
	input_width = isp_ichn_attr.width;
	input_height = isp_ichn_attr.height;

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

	// 输出非 16 像素对齐的常用算法图像使用的分辨率
	vse_ochn_attr[2].target_w = 224;
	vse_ochn_attr[2].target_h = 224;

	// 设置VSE通道2输出属性，ROI为原图中心点不变，宽、高各裁剪一半，输出图像宽、高等于ROI区域宽高
	vse_ochn_attr[3].roi.x = input_width / 2 - input_width / 4;
	vse_ochn_attr[3].roi.y = input_height / 2 - input_height / 4;
	vse_ochn_attr[3].roi.w = input_width / 2;
	vse_ochn_attr[3].roi.h = input_height / 2;

	// 缩小到支持的最小分辨率
	vse_ochn_attr[3].target_w = 64;
	vse_ochn_attr[3].target_h = 64;

	// 放大到支持的最大分辨率
	vse_ochn_attr[4].target_w = 672;
	vse_ochn_attr[4].target_h = 672;
	vse_ochn_attr[5].target_w = (input_width * 4) > 4096 ? 4096 : (input_width * 4);
	vse_ochn_attr[5].target_h = (input_height * 4) > 3076 ? 3076 : (input_height * 4);

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, vse_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(*vse_node_handle, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(*vse_node_handle, chn_id, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;

	for (int i = 0; i < VSE_MAX_CHANNELS; ++i) {
		printf("hbn_vnode_set_ochn_attr: %d, %dx%d\n", i, vse_ochn_attr[i].target_w, vse_ochn_attr[i].target_h);
		ret = hbn_vnode_set_ochn_attr(*vse_node_handle, i, &vse_ochn_attr[i]);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_set_ochn_buf_attr(*vse_node_handle, i, &alloc_attr);
		ERR_CON_EQ(ret, 0);
	}

	return 0;
}


int create_and_run_vflow(pipe_contex_t *pipe_contex) {
	int32_t ret = 0;

	// 创建pipeline中的每个node
	ret = create_camera_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vse_node(pipe_contex);
	ERR_CON_EQ(ret, 0);

	// 创建HBN flow
	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->isp_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vse_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle,
							1,
							pipe_contex->isp_node_handle,
							0);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->isp_node_handle,
							0,
							pipe_contex->vse_node_handle,
							0);
	ERR_CON_EQ(ret, 0);
	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
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

void *read_vse_data(void *context) {
	pipe_contex_t *pipe_context = (pipe_contex_t *)context;
	hbn_vnode_handle_t vse_node_handle = pipe_context->vse_node_handle;
	hbn_vnode_image_t out_img[VSE_MAX_CHANNELS] = {0};
	char dst_file[128] = {0};
	uint32_t count = 0;
	int ret = 0;

	while (running) {
		for (uint32_t i = 0; i < VSE_MAX_CHANNELS; ++i) {
			ret = hbn_vnode_getframe(vse_node_handle, i, 1000, &out_img[i]);
			if (ret != 0) {
				printf("hbn_vnode_getframe VSE channel %d failed\n", i);
				continue;
			}
		}

		if (count % 60 == 0) {
			for (uint32_t i = 0; i < VSE_MAX_CHANNELS; ++i) {
				for (int j = 0; j < 2; ++j) {
					hb_mem_invalidate_buf_with_vaddr((uint64_t)out_img[i].buffer.virt_addr[j], out_img[i].buffer.size[j]);
				}
				snprintf(dst_file, sizeof(dst_file), "vse_ch%d_%d.yuv", i, count);
				dump_2plane_yuv_to_file(dst_file,
					out_img[i].buffer.virt_addr[0],
					out_img[i].buffer.virt_addr[1],
					out_img[i].buffer.size[0],
					out_img[i].buffer.size[1]);
				printf("####################### vse chn %d #######################\n", i);
				vp_vin_print_hbn_vnode_image_t(&out_img[i]);
			}
		}

		for (uint32_t i = 0; i < VSE_MAX_CHANNELS; ++i) {
			hbn_vnode_releaseframe(vse_node_handle, i, &out_img[i]);
		}

		count++;
		usleep(1000 * 20);
	}

	return NULL;
}
