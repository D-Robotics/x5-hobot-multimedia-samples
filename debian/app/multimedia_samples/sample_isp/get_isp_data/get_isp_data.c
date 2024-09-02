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

static struct option const long_options[] = {
	{"sensor", required_argument, NULL, 's'},
	{"settle", optional_argument, NULL, 't'},
	{"mode", optional_argument, NULL, 'm'},
	{NULL, 0, NULL, 0}
};

static int create_and_run_vflow(pipe_contex_t *pipe_contex);
static void handle_user_command(pipe_contex_t *pipe_contex, int sensor_count);

static void print_help() {
	printf("Usage: get_isp_data [OPTIONS]\n");
	printf("Options:\n");
	printf("  -s <sensor_index>      Specify sensor index\n");
	printf("  -t <settle_value>      Specify settle time for debug\n");
	printf("  -m <sensor_mode>       Specify sensor mode of camera_config_t\n");
	printf("  -h                     Show this help message\n");
	vp_show_sensors_list(); // Assuming this function displays sensor list
}

static void command_help() {
	printf("\n");
	printf("***************  Command Lists  ***************\n");
	printf(" g	-- get single frame \n");
	printf(" l	-- get a set frames \n");
	printf(" q	-- quit  \n");
	printf(" h	-- print help message\n");
}

static int settle = -1;
static uint32_t sensor_mode = 0; // 1: NORMAL_M; 2: DOL2_M; 6: SLAVE_M

int main(int argc, char** argv) {
	int ret = 0;
	pipe_contex_t pipe_contex[MAX_SENSORS] = {0};
	int opt_index = 0;
	int c = 0;
	int index = -1;
	int sensor_indexes[MAX_SENSORS] = {-1};
	int sensor_count = 0;

	while((c = getopt_long(argc, argv, "s:t:m:h",
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
		case 't':
			settle = atoi(optarg);
			break;
		case 'm':
			sensor_mode = atoi(optarg);
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
			printf("Unsupported sensor index:%d\n", index);
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
			printf("hbn_vflow_stop failed for sensor %d. ret = %d\n", sensor_indexes[i], ret);
		}
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
	/* Debug settle */
	if (settle >= 0 && settle <= 127) {
		camera_config->mipi_cfg->rx_attr.settle = settle;
	}
	if (sensor_mode >= NORMAL_M && sensor_mode < INVALID_MOD) {
		camera_config->sensor_mode = sensor_mode;
		sensor_config->vin_node_attr->lpwm_attr.enable = 1;
	}
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
							pipe_contex->vin_node_handle,
							1,
							pipe_contex->isp_node_handle,
							0);
	ERR_CON_EQ(ret, 0);
	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}

void isp_dump_func(hbn_vnode_handle_t isp_node_handle) {
	int ret;
	char dst_file[128];
	uint32_t chn_id = 0;
	uint32_t timeout = 10000;
	hbn_vnode_image_t out_img;

	// 调用hbn_vnode_getframe获取帧数据
	ret = hbn_vnode_getframe(isp_node_handle, chn_id, timeout, &out_img);
	if (ret != 0) {
		printf("hbn_vnode_getframe from isp chn:%d failed(%d)\n", chn_id, ret);
		return;
	}

	// 将帧数据写入文件
	snprintf(dst_file, sizeof(dst_file),
		"handle_%d_isp_chn%d_%dx%d_stride_%d_frameid_%d_ts_%ld.yuv",
		(int)isp_node_handle, chn_id,
		out_img.buffer.width, out_img.buffer.height, out_img.buffer.stride,
		out_img.info.frame_id, out_img.info.timestamps);
	printf("handle %d isp dump yuv %dx%d(stride:%d), buffer size: %ld + %ld frame id: %d,"
			" timestamp: %ld\n",
			(int)isp_node_handle,
			out_img.buffer.width, out_img.buffer.height,
			out_img.buffer.stride,
			out_img.buffer.size[0], out_img.buffer.size[1],
			out_img.info.frame_id,
			out_img.info.timestamps);
	dump_2plane_yuv_to_file(dst_file,
			out_img.buffer.virt_addr[0],
			out_img.buffer.virt_addr[1],
			out_img.buffer.size[0],
			out_img.buffer.size[1]);

	// 释放帧数据
	hbn_vnode_releaseframe(isp_node_handle, chn_id, &out_img);
}

static void handle_user_command(pipe_contex_t *pipe_contex, int sensor_count)
{
	int i = 0, j = 0;
	char option = 'a';
	hbn_vnode_handle_t isp_node_handle;
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
					isp_node_handle = pipe_contex[i].isp_node_handle;
					isp_dump_func(isp_node_handle);
				}
				break;
			case 'l':// get multiple frames for all sensors
				for (j = 0; j < 12; j++) {
					for (i = 0; i < sensor_count; i++) {
						isp_node_handle = pipe_contex[i].isp_node_handle;
						isp_dump_func(isp_node_handle);
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
