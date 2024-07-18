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

/* Defined at vp_sensor/dummy_sensor */
extern vp_sensor_config_t dummy_sensor_config;

static struct option const long_options[] = {
	{"sensor", required_argument, NULL, 's'},
	{"settle", optional_argument, NULL, 't'},
	{"mode", optional_argument, NULL, 'm'},
	{NULL, 0, NULL, 0}
};

static void print_help() {
	printf("Usage: %s [OPTIONS]\n", get_program_name());
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

static int fixed_dummy_sensor_config(pipe_contex_t *vin_isp_contex,
	vp_sensor_config_t *dummy_sensor_config)
{
	vp_sensor_config_t *vin_sensor_config = vin_isp_contex->sensor_config;

	camera_config_t *camera_config = dummy_sensor_config->camera_config;
	vin_node_attr_t *vin_node_attr = dummy_sensor_config->vin_node_attr;
	vin_ichn_attr_t *vin_ichn_attr = dummy_sensor_config->vin_ichn_attr;
	vin_ochn_attr_t *vin_ochn_attr = dummy_sensor_config->vin_ochn_attr;
	isp_attr_t      *isp_attr = dummy_sensor_config->isp_attr;
	isp_ichn_attr_t *isp_ichn_attr = dummy_sensor_config->isp_ichn_attr;
	isp_ochn_attr_t *isp_ochn_attr = dummy_sensor_config->isp_ochn_attr;

	memcpy(camera_config, vin_sensor_config->camera_config, sizeof(camera_config_t));
	memcpy(vin_node_attr, vin_sensor_config->vin_node_attr, sizeof(vin_node_attr_t));
	memcpy(vin_ichn_attr, vin_sensor_config->vin_ichn_attr, sizeof(vin_ichn_attr_t));
	memcpy(vin_ochn_attr, vin_sensor_config->vin_ochn_attr, sizeof(vin_ochn_attr_t));
	memcpy(isp_attr, vin_sensor_config->isp_attr, sizeof(isp_attr_t));
	memcpy(isp_ichn_attr, vin_sensor_config->isp_ichn_attr, sizeof(isp_ichn_attr_t));
	memcpy(isp_ochn_attr, vin_sensor_config->isp_ochn_attr, sizeof(isp_ochn_attr_t));


	/* 修改 dummy_camera_config 的 camera_config_t
	 * 1. 设置 dummy sensor 的 name 为 dummy
	 * 2. 设置 dummy sensor 的 addr 为 0xFF
	 * 3. Disable dummy sensor 的 mipi_cfg
	 * 4. 设置 dummy sensor 的 calib_lname 为实际 sensor 的 isp tuning 文件名
	*/
	strcpy(camera_config->name, "dummy");
	camera_config->addr = 0xFF;
	camera_config->mipi_cfg->rx_enable = 0;
	camera_config->gpio_enable_bit = 0,
	camera_config->gpio_level_bit = 0,
	/* 此处可以调整使用不用的 isp tuning 文件 */
	/* 示例代码中使用实际 Camera Sensor 调校好的 tuning 文件 */
	strcpy(camera_config->calib_lname, vin_sensor_config->camera_config->name);

	/* 修改 dummy_camera_config 的 vin_node_attr_t 中 cim_attr.mipi_rx
	 * 这个参数必须设置为与实际使用的mipi host不一样
	 * 这里存在一个bug：目前只能设置为0-3，但是如果硬件上存在多路sensor的时候，这个参数必然会和实际的sensor冲突
	 * 目前设置为 1，可以支持大多数场景，但是对于4lane的sensor和4路sensor时会存在问题。
	*/
	vin_node_attr->cim_attr.mipi_rx = 1;

	/* 设置ISP工作在Offline模式 */
	isp_attr->input_mode = 2; // 0: online, 1: mcm, 类似offline, 2: Offline

	/* 设置ISP输出通道的数据写入到ddr，如此才能从isp中通过 hbn_vnode_getframe 得到frame */
	isp_ochn_attr->ddr_en = 1;

	return 0;
}

static int create_camera_node(pipe_contex_t *pipe_contex) {

	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;
	int32_t ret = 0;

	sensor_config = pipe_contex->sensor_config;
	camera_config = sensor_config->camera_config;

	if (strcmp("dummy", camera_config->name) != 0) {
		/* Debug settle */
		if (settle >= 0 && settle <= 127) {
			camera_config->mipi_cfg->rx_attr.settle = settle;
		}
		if (sensor_mode >= NORMAL_M && sensor_mode < INVALID_MOD) {
			camera_config->sensor_mode = sensor_mode;
			sensor_config->vin_node_attr->lpwm_attr.enable = 1;
		}
	}
	ret = hbn_camera_create(camera_config, &pipe_contex->cam_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vin_node(pipe_contex_t *pipe_contex)
{
	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;
	vin_node_attr_t *vin_node_attr = NULL;
	vin_ichn_attr_t *vin_ichn_attr = NULL;
	vin_ochn_attr_t *vin_ochn_attr = NULL;
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t hw_id = 0;
	int32_t ret = 0;
	uint32_t chn_id = 0;
	uint64_t vin_attr_ex_mask = 0;

	sensor_config = pipe_contex->sensor_config;
	camera_config = sensor_config->camera_config;
	vin_node_attr = sensor_config->vin_node_attr;
	vin_ichn_attr = sensor_config->vin_ichn_attr;
	vin_ochn_attr = sensor_config->vin_ochn_attr;
	hw_id = vin_node_attr->cim_attr.mipi_rx;
	vin_node_handle = &pipe_contex->vin_node_handle;

	ret = hbn_vnode_open(HB_VIN, hw_id, AUTO_ALLOC_ID, vin_node_handle);
	ERR_CON_EQ(ret, 0);
	// 设置基本属性
	ret = hbn_vnode_set_attr(*vin_node_handle, vin_node_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输入通道的属性
	ret = hbn_vnode_set_ichn_attr(*vin_node_handle, chn_id, vin_ichn_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输出通道的属性
	// 使能DDR输出
	vin_ochn_attr->ddr_en = 1;
	ret = hbn_vnode_set_ochn_attr(*vin_node_handle, chn_id, vin_ochn_attr);
	ERR_CON_EQ(ret, 0);

	// 如果初始化虚拟 Sensor， 则禁止设置 MCLK
	if (strcmp("dummy", camera_config->name) != 0) {
		if(pipe_contex->csi_config.mclk_is_not_configed){
			//设备树中没有配置mclk：使用外部晶振
			printf("csi%d ignore mclk ex attr, because not config mclk.\n",
				pipe_contex->csi_config.index);
		}else{
			vin_attr_ex.vin_attr_ex_mask = 0x80;	//bit7 for mclk
			vin_attr_ex.mclk_ex_attr.mclk_freq = 24000000; // 24MHz
		}

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
	}

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*vin_node_handle, chn_id, &alloc_attr);

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

	printf("[INFO] Create isp node handle: %d\n", (int)*isp_node_handle);

	return 0;
}

int create_and_run_vin_isp_vflow(pipe_contex_t *pipe_contex) {
	int32_t ret = 0;

	// 创建pipeline中的每个node
	ret = create_camera_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);

	// 创建HBN vflow
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
							1, // online
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

// 使用一个 vflow 来初始化一个 isp 实例
// 因为isp的tuning参数在camera_config_t中定义，所以需要初始化一个虚拟Sensor来完成整个flow的配置
int create_and_run_isp_vflow(pipe_contex_t *pipe_contex) {
	int32_t ret = 0;

	// 创建pipeline中的每个node
	ret = create_camera_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);

	// 创建HBN vflow
	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							pipe_contex->isp_node_handle);
	ERR_CON_EQ(ret, 0);
	// 不需要把 vin 和 isp bind 在一起
	// hbn_camera_attach_to_vin 是必须的, 否则 sendframe 给 isp 时会报43 和 45 号错误
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
		"isp_handle_%d_chn%d_%dx%d_stride_%d_frameid_%d_ts_%ld.yuv",
		(int)isp_node_handle, chn_id,
		out_img.buffer.width, out_img.buffer.height, out_img.buffer.stride,
		out_img.info.frame_id, out_img.info.timestamps);
	printf("isp(%d) dump yuv %dx%d(stride:%d), buffer size: %ld + %ld frame id: %d,"
			" timestamp: %ld\n", (int)isp_node_handle,
			out_img.buffer.width, out_img.buffer.height,
			out_img.buffer.stride,
			out_img.buffer.size[0], out_img.buffer.size[1],
			out_img.info.frame_id,
			out_img.info.timestamps);
	// 此处，无论是写磁盘还是写内存，都会影响性能
	dump_2plane_yuv_to_file(dst_file,
			out_img.buffer.virt_addr[0],
			out_img.buffer.virt_addr[1],
			out_img.buffer.size[0],
			out_img.buffer.size[1]);

	// 释放帧数据
	hbn_vnode_releaseframe(isp_node_handle, chn_id, &out_img);
}


void vin_dump_func(hbn_vnode_handle_t vin_node_handle,
	hbn_vnode_handle_t isp_node_handle)
{
	int ret;
	char dst_file[128];
	uint32_t chn_id = 0;
	uint32_t timeout = 2000;
	hbn_vnode_image_t out_img;

	// 调用hbn_vnode_getframe获取帧数据
	ret = hbn_vnode_getframe_cond(vin_node_handle, chn_id, timeout, 0, &out_img);
	if (ret != 0) {
		printf("hbn_vnode_getframe from vin chn:%d failed(%d)\n", chn_id, ret);
		return;
	}

	// 将帧数据写入文件
	snprintf(dst_file, sizeof(dst_file),
		"vin_chn%d_%dx%d_stride_%d_frameid_%d_ts_%ld.raw",
		chn_id,
		out_img.buffer.width, out_img.buffer.height, out_img.buffer.stride,
		out_img.info.frame_id, out_img.info.timestamps);
	printf("vin dump raw %dx%d(stride:%d), buffer size: %ld frame id: %d,"
			" timestamp: %ld\n",
			out_img.buffer.width, out_img.buffer.height,
			out_img.buffer.stride,
			out_img.buffer.size[0],
			out_img.info.frame_id,
			out_img.info.timestamps);
	// dump_image_to_file(dst_file, out_img.buffer.virt_addr[0], out_img.buffer.size[0]);

	// 此处串行处理，在性能允许的情况下，可以减少内存的拷贝
	// 如果处理性能不足，需要采用多级流水线处理，但是可能需要增加一次内存拷贝
	// 把 Raw 数据送给 isp 处理
	ret = hbn_vnode_sendframe(isp_node_handle, 0, &out_img);
	if (ret != 0) {
		printf("hbn_vnode_sendframe to isp failed(%d)\n", ret);
		return;
	}

	isp_dump_func(isp_node_handle);

	// 释放帧数据
	hbn_vnode_releaseframe(vin_node_handle, chn_id, &out_img);
}

static int handle_user_command(pipe_contex_t *vin_isp_contex,
	pipe_contex_t *isp_contex)
{
	int i = 0;
	char option = 'a';
	hbn_vnode_handle_t vin_node_handle, isp_node_handle;
	int running = -1;

	vin_node_handle = vin_isp_contex->vin_node_handle;
	isp_node_handle = isp_contex->isp_node_handle;

	command_help();
	printf("\nCommand: "); // 将打印移到循环外部

	while (running && ((option=getchar()) != EOF)) {
		switch (option) {
			case 'q':
				printf("quit\n");
				running = 0;
				return 0;
			case 'g':
				isp_dump_func(vin_isp_contex->isp_node_handle);
				vin_dump_func(vin_node_handle, isp_node_handle);
				break;
			case 'l': // 循环获取，用于计算帧率
				for (i = 0; i < 12; i++) {
					isp_dump_func(vin_isp_contex->isp_node_handle);
					vin_dump_func(vin_node_handle, isp_node_handle);
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

/* 启动两个vflow：
 * 1. 启动实际 Camera Sensor 初始化 vin，并且bind isp 的 vflow
 *    可以从vin中拿到sensor的raw图，从isp中拿到 yuv 图，并且可以调节sensor 的 AE
 * 2. 启动一个isp模块的 vflow，vin的raw图送给这个isp vflow进行处理
*/
int main(int argc, char** argv) {
	int ret = 0;
	pipe_contex_t vin_isp_contex = {0};
	pipe_contex_t isp_contex = {0};
	int opt_index = 0;
	int c = 0;
	int index = -1;

	while((c = getopt_long(argc, argv, "s:t:m:h",
							long_options, &opt_index)) != -1) {
		switch (c)
		{
		case 's':
			index = atoi(optarg);
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
	if (index < vp_get_sensors_list_number() && index >= 0) {
		vin_isp_contex.sensor_config = vp_sensor_config_list[index];
		printf("Using index:%d  sensor_name:%s  config_file:%s\n",
				index,
				vp_sensor_config_list[index]->sensor_name,
				vp_sensor_config_list[index]->config_file);
		ret = vp_sensor_fixed_mipi_host(vin_isp_contex.sensor_config,  &vin_isp_contex.csi_config);
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

	ret = create_and_run_vin_isp_vflow(&vin_isp_contex);
	ERR_CON_EQ(ret, 0);

	// 根据 Camera Sensor的配置设置虚拟 Sensor 的参数
	fixed_dummy_sensor_config(&vin_isp_contex, &dummy_sensor_config);
	isp_contex.sensor_config = &dummy_sensor_config;
	ret = create_and_run_isp_vflow(&isp_contex);
	ERR_CON_EQ(ret, 0);

	handle_user_command(&vin_isp_contex, &isp_contex);

	ret = hbn_vflow_stop(vin_isp_contex.vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_stop(isp_contex.vflow_fd);
	ERR_CON_EQ(ret, 0);

	hbn_vflow_destroy(vin_isp_contex.vflow_fd);
	hbn_vflow_destroy(isp_contex.vflow_fd);

	return 0;
}
