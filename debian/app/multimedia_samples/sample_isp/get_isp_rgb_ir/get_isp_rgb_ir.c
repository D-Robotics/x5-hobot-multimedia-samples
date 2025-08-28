
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

#define MAX_SENSORS 4
#define MAX_FILE_NAME_LENGTH 256
#define RGB_CHANNEL 0
#define IR_CHANNEL 2

static struct option const long_options[] = {
	{"sensor", required_argument, NULL, 's'},
	{"verbose", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

int32_t verbose_flag = 0;

int create_and_run_vflow(pipe_contex_t *pipe_contex);
static void handle_user_command(pipe_contex_t *pipe_contex, int sensor_count);
void dump_data(void *contex);

static void print_help() {
	printf("get_isp_rgb_ir -s/--sensor sensor_index\n");
	printf("Options:\n");
	printf("  -s, --sensor sensor_index   Specify the sensor index\n");
	printf("  -v, --verbose               Enable detailed log information\n");
	printf("Note: Default Offline Mode\n");
	vp_show_sensors_list();
}

int main(int argc, char** argv) {
	int ret = 0;
	pipe_contex_t pipe_contex[MAX_SENSORS] = {0};
	int opt_index = 0;
	int c = 0;
	int index = -1;
	int sensor_indexes[MAX_SENSORS] = {-1};
	int sensor_count = 0;

	while((c = getopt_long(argc, argv, "s:vh",
							long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			if (sensor_count < MAX_SENSORS) {
				sensor_indexes[sensor_count++] = atoi(optarg);
			} else {
				printf("Maximum number of sensors exceeded\n");
				return 0;
			}
			break;
		case 'v':
			verbose_flag = 1;
			break;
		case 'h':
		default:
			print_help();
			return 0;
		}
	}

	if (sensor_count == 0) {
		printf("No sensors specified.\n");
		print_help();
		return 0;
	}

	for (int i = 0; i < sensor_count; ++i) {
		index = sensor_indexes[i];
		if (index < vp_get_sensors_list_number() && index >= 0) {
			pipe_contex[i].sensor_config = vp_sensor_config_list[index];
			printf("Using index:%d  sensor_name:%s  config_file:%s\n",
					index,
					vp_sensor_config_list[index]->sensor_name,
					vp_sensor_config_list[index]->config_file);
			ret = vp_sensor_fixed_mipi_host(pipe_contex[i].sensor_config, &pipe_contex[i].csi_config);
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
	}

	hb_mem_module_open();

	for (int i = 0; i < sensor_count; ++i) {
		ret = create_and_run_vflow(&pipe_contex[i]);
		if (ret != 0) {
			printf("create_and_run_vflow failed for sensor %d. ret = %d\n", sensor_indexes[i], ret);
			return ret;
		}
	}

	handle_user_command(pipe_contex, sensor_count);

	for (int i = 0; i < sensor_count; ++i) {
		ret = hbn_vflow_stop(pipe_contex[i].vflow_fd);
		if (ret != 0) {
			printf("hbn_vflow_stop RGB failed for sensor %d. ret = %d\n", sensor_indexes[i], ret);
		}

		hbn_vnode_close(pipe_contex[i].vin_node_handle);
		hbn_vnode_close(pipe_contex[i].isp_node_handle);
		hbn_camera_destroy(pipe_contex[i].cam_fd);
		hbn_vflow_destroy(pipe_contex[i].vflow_fd);
	}

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
	hbn_buf_alloc_attr_t alloc_attr = {0};
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	uint32_t hw_id = 0;
	int32_t ret = 0;
	uint32_t ichn_id = 0;
	uint32_t ochn_id = 0;
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
		vin_attr_ex.vin_attr_ex_mask = sensor_config->vin_attr_ex->vin_attr_ex_mask;
		vin_attr_ex.mclk_ex_attr.mclk_freq = sensor_config->vin_attr_ex->mclk_ex_attr.mclk_freq;
		vin_attr_ex_mask = vin_attr_ex.vin_attr_ex_mask;
	}

	ret = hbn_vnode_open(HB_VIN, hw_id, AUTO_ALLOC_ID, vin_node_handle);
	ERR_CON_EQ(ret, 0);
	// 设置基本属性
	ret = hbn_vnode_set_attr(*vin_node_handle, vin_node_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输入通道的属性
	ret = hbn_vnode_set_ichn_attr(*vin_node_handle, ichn_id, vin_ichn_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输出通道的属性
	ret = hbn_vnode_set_ochn_attr(*vin_node_handle, ochn_id, vin_ochn_attr);
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

	if(vin_ochn_attr->ddr_en)
	{
		memset(&alloc_attr, 0, sizeof(hbn_buf_alloc_attr_t));
		alloc_attr.buffers_num = 6;
		alloc_attr.is_contig = 1;
		alloc_attr.flags =
			HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
		ret = hbn_vnode_set_ochn_buf_attr(*vin_node_handle, ochn_id, &alloc_attr);
		if (ret < 0) {
			printf("hbn_vnode_set_ochn_buf_attr fail ret %d\n", ret);
			return ret;
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
	uint32_t ichn_id = 0;
	uint32_t ochn_id = 0;
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
	ret = hbn_vnode_set_ochn_attr(*isp_node_handle, ochn_id, isp_ochn_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ichn_attr(*isp_node_handle, ichn_id, isp_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 6;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, ochn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	isp_ochn_attr_t isp_raw_ochn_attr;
	uint32_t ir_ochn_id = ISP_MAIN_RAW_FRAME;
	/* isp ir raw attr */
	isp_raw_ochn_attr.ddr_en = 1;
	isp_raw_ochn_attr.fmt = FRM_FMT_RAW;
	isp_raw_ochn_attr.bit_width = 8;
	ret = hbn_vnode_set_ochn_attr(*isp_node_handle, ir_ochn_id, &isp_raw_ochn_attr);
	ERR_CON_EQ(ret, 0);

	memset(&alloc_attr, 0 , sizeof(alloc_attr));
	alloc_attr.buffers_num = 6;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, ir_ochn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

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


	// 创建HBN flow
	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->isp_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle,  // offline mode
							0,
							pipe_contex->isp_node_handle,  // offline mode
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
	printf("Systimestamps: %lu\n", frame_info->sys_timestamps);
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

void dump_data(void *context) {
	pipe_contex_t *pipe_context = (pipe_contex_t *)context;
	hbn_vnode_handle_t isp_node_handle = pipe_context->isp_node_handle;
	hbn_vnode_image_t isp_out_img;
	char dst_file[MAX_FILE_NAME_LENGTH] = {0};
	uint32_t timeout = 10000;
	int ret = 0;
	int isp_schn = 0;  // 0: RGB, 2: IR

	// channel 0 for RGB; channel 2 for IR
	for (int k = 0; k < 3; k+=2) {
		isp_schn = k;
		ret = hbn_vnode_getframe(isp_node_handle, isp_schn, timeout, &isp_out_img);
		if (ret != 0) {
			printf("hbn_vnode_getframe ISP channel %d failed, error code %d\n.", isp_schn, ret);
			continue;
		}

		for (int j = 0; j < 2; ++j) {
			if (isp_schn == 2 && j == 1) {
				continue; // 跳过第二个平面
			}
			hb_mem_invalidate_buf_with_vaddr((uint64_t)isp_out_img.buffer.virt_addr[j],
												isp_out_img.buffer.size[j]);
		}
		/* 计算文件名并 dump */
		const char* ch_name = (isp_schn == RGB_CHANNEL) ? "rgb" :
							(isp_schn == IR_CHANNEL)  ? "ir"  : NULL;
		if (!ch_name) {
			printf("Invalid channel number: %d\n", isp_schn);
			hbn_vnode_releaseframe(isp_node_handle, isp_schn, &isp_out_img);
			continue;
		}

		snprintf(dst_file, sizeof(dst_file), "handle_%d_%s_%s_frameid_%d_ts_%ld.yuv",
				(int)isp_node_handle,
				pipe_context->sensor_config->sensor_name,
				ch_name,
				isp_out_img.info.frame_id,
				isp_out_img.info.timestamps);
		printf("Dumping %s frame to file: %s\n", ch_name, dst_file);
		dump_2plane_yuv_to_file(dst_file,
								isp_out_img.buffer.virt_addr[0],
								isp_out_img.buffer.virt_addr[1],
								isp_out_img.buffer.size[0],
								isp_out_img.buffer.size[1]);
		if (verbose_flag > 0) {
			vp_vin_print_hbn_vnode_image_t(&isp_out_img);  //  打印详细信息，调试使用
		}

		hbn_vnode_releaseframe(isp_node_handle, isp_schn, &isp_out_img);
	}

}

static void command_help() {
	printf("\n");
	printf("***************  Command Lists  ***************\n");
	printf(" g	-- get single frame \n");
	printf(" l	-- get a set frames \n");
	printf(" q	-- quit  \n");
	printf(" h	-- print help message\n");
}

static void handle_user_command(pipe_contex_t *pipe_contex, int sensor_count)
{
	int i = 0, j = 0;
	char option = 'a';
	// hbn_vnode_handle_t isp_node_handle;
	int running = -1;

	command_help();
	printf("\nCommand: ");

	while (running && ((option=getchar()) != EOF)) {
		switch (option) {
			case 'q':
				printf("quit\n");
				running = 0;
				return;
			case 'g':  // get a isp yuv file for all sensors
				for (i = 0; i < sensor_count; i++) {

					dump_data((void *)&pipe_contex[i]);
				}
				break;
			case 'l':// get multiple frames for all sensors
				for (j = 0; j < 12; j++) {
					for (i = 0; i < sensor_count; i++) {
						dump_data((void *)&pipe_contex[i]);
					}
				}
				break;
			case 'h':
				command_help();
				break;
			case '\n':
			case '\r':
				continue;
			default:
				printf("Command does not supported!\n");
				command_help();
				break;
		}
		printf("\nCommand: ");
	}

	return;
}