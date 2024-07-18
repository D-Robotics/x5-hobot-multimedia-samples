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

#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"

typedef struct gdc_info
{
	char* gdc_bin_file;
	hb_mem_common_buf_t bin_buf;
} gdc_info_s;

static struct option const long_options[] = {
	{"sensor", required_argument, NULL, 's'},
	{"mode", optional_argument, NULL, 'm'},
	{"gdc_bin_file", required_argument, NULL, 'f'},
	{NULL, 0, NULL, 0}
};

static void print_help() {
	printf("Usage: %s [OPTIONS]\n", get_program_name());
	printf("Options:\n");
	printf("  -s <sensor_index>      Specify sensor index\n");
	printf("  -m <sensor_mode>       Specify sensor mode of camera_config_t\n");
	printf("  -f <gdc_bin_file>      Specify sensor gdc_bin_file path\n");
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

static uint32_t sensor_mode = 0; // 1: NORMAL_M; 2: DOL2_M; 6: SLAVE_M

static int create_camera_node(pipe_contex_t *pipe_contex) {

	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;
	int32_t ret = 0;

	sensor_config = pipe_contex->sensor_config;
	camera_config = sensor_config->camera_config;
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

static int32_t read_gdc_config(char *gdc_bin_file, hb_mem_common_buf_t *bin_buf)
{
	int64_t alloc_flags = 0;
	int ret = 0;
	int offset = 0;
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
		return -1;
	}

	memcpy(bin_buf->virt_addr, cfg_buf, file_size);
	ret = hb_mem_flush_buf(bin_buf->fd, offset, file_size);
	ERR_CON_EQ(ret, 0);

	return ret;
}

static int32_t create_gdc_node(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info)
{
	int ret = 0;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;
	gdc_attr_t gdc_attr = {0};
	gdc_ochn_attr_t gdc_ochn_attr = {0};
	uint32_t input_width = 0;
	uint32_t input_height = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	ret = read_gdc_config(gdc_info->gdc_bin_file, &gdc_info->bin_buf);
	if(ret != 0){
		printf("gdc bin file [%s] is not valid\n", gdc_info->gdc_bin_file);
		return -1;
	}

	input_width = pipe_contex->sensor_config->isp_ichn_attr->width;
	input_height = pipe_contex->sensor_config->isp_ichn_attr->height;

	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &(pipe_contex->gdc_node_handle));
	if(ret != 0){
		printf("gdc bin file [%s] is valid, but open failed %d\n",
			gdc_info->gdc_bin_file, ret);
		return -1;
	}

	gdc_attr.config_addr = gdc_info->bin_buf.phys_addr;
	gdc_attr.config_size = gdc_info->bin_buf.size;
	gdc_attr.binary_ion_id = gdc_info->bin_buf.share_id;
	gdc_attr.binary_offset = gdc_info->bin_buf.offset;
	gdc_attr.total_planes = 2;
	gdc_attr.div_width = 0;
	gdc_attr.div_height = 0;
	ret = hbn_vnode_set_attr(pipe_contex->gdc_node_handle, &gdc_attr);
	if(ret != 0){
		printf("gdc bin file [%s] is valid, but set attr failed %d\n",
			gdc_info->gdc_bin_file, ret);
		return -1;
	}

	gdc_ichn_attr_t gdc_ichn_attr = {0};
	gdc_ichn_attr.input_width = input_width;
	gdc_ichn_attr.input_height = input_height;
	gdc_ichn_attr.input_stride = input_width;
	ret = hbn_vnode_set_ichn_attr(pipe_contex->gdc_node_handle, chn_id, &gdc_ichn_attr);
	if(ret != 0){
		printf("gdc bin file [%s] is valid, but set ichn failed %d\n",
			gdc_info->gdc_bin_file, ret);
		return -1;
	}

	gdc_ochn_attr.output_width = input_width;
	gdc_ochn_attr.output_height = input_height;
	gdc_ochn_attr.output_stride = input_width;
	ret = hbn_vnode_set_ochn_attr(pipe_contex->gdc_node_handle, chn_id, &gdc_ochn_attr);
	if(ret != 0){
		printf("gdc bin file [%s] is valid, but set ochn failed %d\n",
			gdc_info->gdc_bin_file, ret);
		return -1;
	}

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
					HB_MEM_USAGE_CPU_WRITE_OFTEN |
					HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(pipe_contex->gdc_node_handle, chn_id, &alloc_attr);
	if(ret != 0){
		printf("gdc bin file [%s] is valid, but set ochn buffer failed %d\n",
			gdc_info->gdc_bin_file, ret);
		return -1;
	}

	return 0;
}

int create_and_run_vin_isp_gdc_vflow(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info) {
	int32_t ret = 0;

	// 创建pipeline中的每个node
	ret = create_camera_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_gdc_node(pipe_contex, gdc_info);
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
							pipe_contex->gdc_node_handle);
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
							pipe_contex->gdc_node_handle,
							0);
	ERR_CON_EQ(ret, 0);
	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}


int create_and_run_gdc_vflow(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info) {
	int ret = 0;

	ret = create_gdc_node(pipe_contex, gdc_info);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
		pipe_contex->gdc_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return ret;
}

static void gdc_dump_func(hbn_vnode_handle_t gdc_node_handle) {
	int ret;
	char dst_file[128];
	uint32_t chn_id = 0;
	uint32_t timeout = 2000;
	hbn_vnode_image_t out_img;

	// 调用hbn_vnode_getframe获取帧数据
	ret = hbn_vnode_getframe(gdc_node_handle, chn_id, timeout, &out_img);
	if (ret != 0) {
		printf("hbn_vnode_getframe from isp chn:%d failed(%d)\n", chn_id, ret);
		return;
	}

	// 将帧数据写入文件
	snprintf(dst_file, sizeof(dst_file),
		"gdc_handle_%d_chn%d_%dx%d_stride_%d_frameid_%d_ts_%ld.yuv",
		(int)gdc_node_handle, chn_id,
		out_img.buffer.width, out_img.buffer.height, out_img.buffer.stride,
		out_img.info.frame_id, out_img.info.timestamps);
	printf("gdc(%d) dump yuv %dx%d(stride:%d), buffer size: %ld + %ld frame id: %d,"
			" timestamp: %ld\n", (int)gdc_node_handle,
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
	hbn_vnode_releaseframe(gdc_node_handle, chn_id, &out_img);
}

static void isp_dump_to_gdc_func(hbn_vnode_handle_t isp_node_handle,
	hbn_vnode_handle_t gdc_node_handle)
{
	int ret;
	uint32_t chn_id = 0;
	uint32_t timeout = 10000;
	hbn_vnode_image_t out_img;

	// 调用hbn_vnode_getframe获取帧数据
	ret = hbn_vnode_getframe(isp_node_handle, chn_id, timeout, &out_img);
	if (ret != 0) {
		printf("hbn_vnode_getframe from isp chn:%d failed(%d)\n", chn_id, ret);
		return;
	}

	printf("isp dump yuv %dx%d(stride:%d), buffer size: %ld + %ld frame id: %d,"
			" timestamp: %ld\n",
			out_img.buffer.width, out_img.buffer.height,
			out_img.buffer.stride,
			out_img.buffer.size[0], out_img.buffer.size[1],
			out_img.info.frame_id,
			out_img.info.timestamps);

	// 此处串行处理，在性能允许的情况下，可以减少内存的拷贝
	// 如果处理性能不足，需要采用多级流水线处理，但是可能需要增加一次内存拷贝
	// 把 Raw 数据送给 isp 处理
	ret = hbn_vnode_sendframe(gdc_node_handle, 0, &out_img);
	if (ret != 0) {
		printf("hbn_vnode_sendframe to gdc failed(%d)\n", ret);
		return;
	}

	gdc_dump_func(gdc_node_handle);

	// 释放帧数据
	hbn_vnode_releaseframe(isp_node_handle, chn_id, &out_img);
}

static int handle_user_command(pipe_contex_t *pipe_contex,
	pipe_contex_t *gdc_pipe_contex)
{
	int i = 0;
	char option = 'a';
	int running = -1;

	command_help();
	printf("\nCommand: "); // 将打印移到循环外部

	while (running && ((option=getchar()) != EOF)) {
		switch (option) {
			case 'q':
				printf("quit\n");
				running = 0;
				return 0;
			case 'g':
				gdc_dump_func(pipe_contex->gdc_node_handle);
				isp_dump_to_gdc_func(pipe_contex->isp_node_handle,
					gdc_pipe_contex->gdc_node_handle);
				break;
			case 'l': // 循环获取，用于计算帧率
				for (i = 0; i < 12; i++) {
					gdc_dump_func(pipe_contex->gdc_node_handle);
					isp_dump_to_gdc_func(pipe_contex->isp_node_handle,
						gdc_pipe_contex->gdc_node_handle);
				}
				break;
			case 'h':
				command_help();
				break; // 添加 break 以退出 switch 语句
			case '\n':
			case '\r':
				continue;
			default:
				printf("Command does not supported!\n");
				command_help();
				break;
		}
		printf("\nCommand: "); // 统一在循环末尾打印
	}

	return 0;
}

int main(int argc, char** argv)
{
	int ret = 0;
	pipe_contex_t pipe_contex = {0};
	pipe_contex_t gdc_pipe_contex = {0};
	gdc_info_s gdc_info = {0};
	int opt_index = 0;
	int c = 0;
	int index = -1;

	while((c = getopt_long(argc, argv, "s:m:x:y:f:h",
							long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			index = atoi(optarg);
			break;
		case 'm':
			sensor_mode = atoi(optarg);
			break;
		case 'f':
			gdc_info.gdc_bin_file = optarg;
			break;
		case 'h':
		default:
			print_help();
			return 0;
		}
	}

	if (gdc_info.gdc_bin_file == NULL) {
		fprintf(stderr, "Error: Missing argument: -f\n");
		print_help();
		exit(EXIT_FAILURE);
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
	ret = create_and_run_vin_isp_gdc_vflow(&pipe_contex, &gdc_info);
	ERR_CON_EQ(ret, 0);

	// 再初始化另一个单独的 gdc node，用户可以在这里更换自定义的gdb bin
	// 此处使用相同的gdc bin是为了简化示例逻辑
	// 然后从主通道的isp模块getframe后send给本独立的gdb node
	gdc_pipe_contex.sensor_config = vp_sensor_config_list[index];
	ret = create_and_run_gdc_vflow(&gdc_pipe_contex, &gdc_info);
	ERR_CON_EQ(ret, 0);

	handle_user_command(&pipe_contex, &gdc_pipe_contex);

	ret = hbn_vflow_stop(pipe_contex.vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(pipe_contex.vflow_fd);

	ret = hbn_vflow_stop(gdc_pipe_contex.vflow_fd);
	ERR_CON_EQ(ret, 0);
	hbn_vflow_destroy(gdc_pipe_contex.vflow_fd);
	hb_mem_module_close();

	return 0;
}
