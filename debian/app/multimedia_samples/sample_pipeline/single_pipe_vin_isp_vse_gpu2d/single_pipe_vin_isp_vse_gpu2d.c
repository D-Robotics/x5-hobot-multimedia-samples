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
#include <stddef.h>
#include <execinfo.h>
#include <ctype.h>

#include "common_utils.h"

#define MAX_SENSORS 4

typedef struct {
	int select_sensor_id;
	uint32_t sensor_mode;
	pipe_contex_t pipe_contexts;
	int active_mipi_host; // 根据实际的硬件连接情况确定使用对应的 mipi host
	int vse_bind_codec_chn;

	struct {
		uint64_t frame_count;
		uint64_t drop_count;
	} stats;

} pipeline_info_t;

typedef struct {
	pipeline_info_t *pipeline_info[MAX_SENSORS]; // 使用数组存储指针
} thread_args_t;

static uint32_t sensor_mode = 0; // 1: NORMAL_M; 2: DOL2_M; 6: SLAVE_M
static int32_t total_pipeline_num = 0;
static int32_t verbose_flag = 0;
static int32_t used_mipi_host = 0;
static uint32_t link_port[MAX_SENSORS] = {};
static int32_t running = 0;
static uint16_t date_type;


static struct option const long_options[] = {
	{"sensor", required_argument, NULL, 's'},
	{NULL, 0, NULL, 0}
};

static int create_and_run_vflow(pipe_contex_t *pipe_contex, int index);
int32_t hbn_deserial_create(deserial_config_t *des_config, deserial_handle_t *des_fd);
int32_t hbn_deserial_attach_to_vin(deserial_handle_t des_fd, camera_des_link_t link, vpf_handle_t vin_fd);
void parse_config(pipeline_info_t *pipeline_info, const char *config, int pipeline_idx);


static void show_help() {
	printf("Usage: get_vin_data [OPTIONS]\n");
	printf("Options:\n");
	printf("  -s \"sensor=index\"    Specify sensor index\n");
	printf("  -h                     Show this help message\n");
	vp_show_sensors_list(); // Assuming this function displays sensor list
}


static int is_number(const char *str) {
	while (*str) {
		if (!isdigit(*str)) return 0;
		str++;
	}
	return 1;
}

// 分割字符串并返回数组的个数
static int split_string(const char *str, const char *delim, char *out[], int max_parts) {
	int count = 0;
	char *token;
	char *str_copy = strdup(str);
	char *rest = str_copy;

	while ((token = strtok_r(rest, delim, &rest)) && count < max_parts) {
		out[count++] = strdup(token);
	}

	free(str_copy);
	return count;
}


void parse_config(pipeline_info_t *pipeline_info, const char *config, int pipeline_idx) {
	int ret = 0;
	char *parts[4];
	int sensor_idx = -1;
	int count = split_string(config, " ", parts, 4);

	// 默认值
	pipeline_info->vse_bind_codec_chn = 0;

	for (int i = 0; i < count; i++) {
		char *key_value[2];
		int kv_count = split_string(parts[i], "=", key_value, 2);
		if (kv_count != 2) {
			fprintf(stderr, "Invalid format in config: %s\n", parts[i]);
			continue;
		}

		if (strcmp(key_value[0], "sensor") == 0) {
			if (!is_number(key_value[1])) {
				fprintf(stderr, "Invalid sensor ID: %s\n", key_value[1]);
				continue;
			}
			sensor_idx = atoi(key_value[1]);

			if (sensor_idx < vp_get_sensors_list_number() && sensor_idx >= 0) {
				pipeline_info->pipe_contexts.sensor_config = vp_sensor_config_list[sensor_idx];
				printf("Using index:%d  sensor_name:%s  config_file:%s\n",
						sensor_idx,
						vp_sensor_config_list[sensor_idx]->sensor_name,
						vp_sensor_config_list[sensor_idx]->config_file);
				date_type = pipeline_info->pipe_contexts.sensor_config->camera_config->format;

			} else {
				printf("Unsupport sensor index:%d\n", sensor_idx);
				show_help();
				exit(0);
			}
			//gmsl 模组需要初始化后才能检测到 addr
			if(date_type == SENSOR_TYPE_NORMAL) {
				ret = vp_sensor_multi_fixed_mipi_host(pipeline_info->pipe_contexts.sensor_config, used_mipi_host,
													&pipeline_info->pipe_contexts.csi_config);
				if (ret < 0) {
					printf("vp sensor fixed mipi host fail, sensor id %d."
						"Maybe No Camera Sensor found. Please check if the specified "
						"sensor is connected to the Camera interface.\n\n", sensor_idx);
					exit(0);
				}
				pipeline_info->select_sensor_id = sensor_idx;
				pipeline_info->active_mipi_host = pipeline_info->pipe_contexts.sensor_config->vin_node_attr->cim_attr.mipi_rx;
				used_mipi_host |= (1 << pipeline_info->pipe_contexts.sensor_config->vin_node_attr->cim_attr.mipi_rx);
			}
		} else {
			fprintf(stderr, "Unknown key: %s\n", key_value[0]);
		}

		for (int j = 0; j < kv_count; j++) {
			free(key_value[j]);
		}
	}

	for (int i = 0; i < count; i++) {
		free(parts[i]);
	}
}

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
	printf("camera_config :%02x,%02x,%s, mode = %d,format:%02x, cam_handle:%ld, chn_num:%d\n\r",
		camera_config->serial_addr, camera_config->addr,
		camera_config->name,camera_config->sensor_mode,camera_config->format,
		pipe_contex->cam_fd, camera_config->mipi_cfg->rx_attr.channel_num);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vin_node(pipe_contex_t *pipe_contex, int index) {
	vp_sensor_config_t *sensor_config = NULL;
	vin_node_attr_t *vin_node_attr = NULL;
	vin_ichn_attr_t *vin_ichn_attr = NULL;
	vin_ochn_attr_t *vin_ochn_attr = NULL;
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	hbn_buf_alloc_attr_t alloc_attr = {0};
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

	link_port[index] = vin_node_attr->cim_attr.vc_index;
	printf("link_port_index:%d:%d \n" ,index, link_port[index]);

	if(pipe_contex->csi_config.mclk_is_not_configed){
		// 设备树中没有配置 mclk：使用外部晶振
		printf("csi%d ignore mclk ex attr, because not config mclk.\n",
			pipe_contex->csi_config.index);
	}else{
		vin_attr_ex.vin_attr_ex_mask = sensor_config->vin_attr_ex->vin_attr_ex_mask;
		vin_attr_ex.mclk_ex_attr.mclk_freq = sensor_config->vin_attr_ex->mclk_ex_attr.mclk_freq;
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
	// 使能 DDR 输出
	vin_ochn_attr->ddr_en = 1;
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
	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*vin_node_handle, ochn_id, &alloc_attr);

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

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
						| HB_MEM_USAGE_CPU_WRITE_OFTEN
						| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, ochn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vse_node(pipe_contex_t *pipe_contex, int vse_bind_index) {
	int ret = 0;
	hbn_vnode_handle_t *vse_node_handle = &pipe_contex->vse_node_handle;
	isp_ichn_attr_t isp_ichn_attr = {0};
	vse_attr_t vse_attr = {0};
	vse_ichn_attr_t vse_ichn_attr = {0};
	vse_ochn_attr_t vse_ochn_attr[VSE_MAX_CHANNELS] = {0};
	uint32_t ichn_id = 0;
	uint32_t hw_id = 0;
	uint32_t input_width = 0, input_height = 0;
	uint32_t output_width = 0, output_height = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	ret = hbn_vnode_get_ichn_attr(pipe_contex->isp_node_handle, ichn_id, &isp_ichn_attr);
	ERR_CON_EQ(ret, 0);
	input_width = isp_ichn_attr.width;
	input_height = isp_ichn_attr.height;

	vse_ichn_attr.width = input_width;
	vse_ichn_attr.height = input_height;
	vse_ichn_attr.fmt = FRM_FMT_NV12;
	vse_ichn_attr.bit_width = 8;

	vse_ochn_attr[vse_bind_index].chn_en = CAM_TRUE;
	vse_ochn_attr[vse_bind_index].roi.x = 0;
	vse_ochn_attr[vse_bind_index].roi.y = 0;
	vse_ochn_attr[vse_bind_index].roi.w = input_width;
	vse_ochn_attr[vse_bind_index].roi.h = input_height;
	vse_ochn_attr[vse_bind_index].fmt = FRM_FMT_NV12;
	vse_ochn_attr[vse_bind_index].bit_width = 8;

	configure_vse_max_resolution(vse_bind_index,
		input_width, input_height,
		&output_width, &output_height);

	// 输出原分辨率
	vse_ochn_attr[vse_bind_index].target_w = output_width;
	vse_ochn_attr[vse_bind_index].target_h = output_height;

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, vse_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(*vse_node_handle, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(*vse_node_handle, ichn_id, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;

	printf("hbn_vnode_set_ochn_attr: %dx%d\n", vse_ochn_attr[vse_bind_index].target_w,
		vse_ochn_attr[vse_bind_index].target_h);
	ret = hbn_vnode_set_ochn_attr(*vse_node_handle, vse_bind_index, &vse_ochn_attr[vse_bind_index]);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_buf_attr(*vse_node_handle, vse_bind_index, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static hbn_vnode_handle_t create_gpu2d_crop_node(pipe_contex_t *pipe_contex) {
	int ret = 0;
	hbn_vnode_handle_t gpu2d_crop_node_handle;
	n2d_config_t *n2d_setting = pipe_contex->sensor_config->gpu2d_crop_attr;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t ichn_id = 0;
	uint32_t hw_id = 0;

	printf("GPU2D CROP input: %ux%u (aligned), stride: %u\n",
		n2d_setting->input_width[0],
		n2d_setting->input_height[0],
		n2d_setting->input_stride[0]);
	printf("GPU2D CROP output: %ux%u (aligned), stride: %u\n",
		n2d_setting->output_width,
		n2d_setting->output_height,
		n2d_setting->output_stride);
	printf("GPU2D CROP command: %u\n", n2d_setting->command);

	ret = hbn_vnode_open(HB_N2D, hw_id, AUTO_ALLOC_ID, &gpu2d_crop_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_attr(gpu2d_crop_node_handle, n2d_setting);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ichn_attr(gpu2d_crop_node_handle, ichn_id, n2d_setting);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_attr(gpu2d_crop_node_handle, 0, n2d_setting);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
	                   HB_MEM_USAGE_CPU_WRITE_OFTEN |
	                   HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(gpu2d_crop_node_handle, 0, &alloc_attr);
	if (ret < 0) {
		printf("hbn_vnode_set_ochn_buf_attr failed, ret = %d\n", ret);
		return -1;
	}

	return gpu2d_crop_node_handle;
}

static hbn_vnode_handle_t create_gpu2d_scale_node(pipe_contex_t *pipe_contex) {
	int ret = 0;
	hbn_vnode_handle_t gpu2d_scale_node_handle;
	n2d_config_t *n2d_setting = pipe_contex->sensor_config->gpu2d_scale_attr;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t ichn_id = 0;
	uint32_t hw_id = 0;

	printf("GPU2D SCALE input: %ux%u (aligned), stride: %u\n",
		n2d_setting->input_width[0],
		n2d_setting->input_height[0],
		n2d_setting->input_stride[0]);
	printf("GPU2D SCALE output: %ux%u (aligned), stride: %u\n",
		n2d_setting->output_width,
		n2d_setting->output_height,
		n2d_setting->output_stride);
	printf("GPU2D SCALE command: %u\n", n2d_setting->command);

	ret = hbn_vnode_open(HB_N2D, hw_id, AUTO_ALLOC_ID, &gpu2d_scale_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_attr(gpu2d_scale_node_handle, n2d_setting);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ichn_attr(gpu2d_scale_node_handle, ichn_id, n2d_setting);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_attr(gpu2d_scale_node_handle, 0, n2d_setting);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
	                   HB_MEM_USAGE_CPU_WRITE_OFTEN |
	                   HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(gpu2d_scale_node_handle, 0, &alloc_attr);
	if (ret < 0) {
		printf("hbn_vnode_set_ochn_buf_attr failed, ret = %d\n", ret);
		return -1;
	}

	return gpu2d_scale_node_handle;
}


static int create_and_run_vflow(pipe_contex_t *pipe_contex, int index)
{
	int32_t ret = 0;
	hbn_vnode_handle_t gpu2d_crop_node_handle;
	hbn_vnode_handle_t gpu2d_scale_node_handle;
	// 创建 pipeline 中的每个 node
	ret = create_camera_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex, index);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vse_node(pipe_contex, 0);
	ERR_CON_EQ(ret, 0);

	// 创建 GPU2D crop 节点
	gpu2d_crop_node_handle = create_gpu2d_crop_node(pipe_contex);
	if (gpu2d_crop_node_handle == 0) {
		printf("Failed to create GPU2D crop node\n");
		return -1;
	}
	// 创建 GPU2D scale 节点
	gpu2d_scale_node_handle = create_gpu2d_scale_node(pipe_contex);
	if (gpu2d_scale_node_handle == 0) {
		printf("Failed to create GPU2D scale node\n");
		return -1;
	}
	// 最终使用 scale 节点 handle 存入 pipe_contex
	pipe_contex->gpu2d_node_handle = gpu2d_scale_node_handle;
	// 创建 HBN flow
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
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							gpu2d_crop_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
							gpu2d_scale_node_handle);
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
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							pipe_contex->vse_node_handle,
							0,
							gpu2d_crop_node_handle,
							0);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
							gpu2d_crop_node_handle,
							0,
							gpu2d_scale_node_handle,
							0);
	ERR_CON_EQ(ret, 0);

	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}


void *encode_vse_chn_data(void *context)
{
	thread_args_t *args = (thread_args_t *)context;
	uint32_t count = 0;
	char dst_file[128];

	while (running) {
		// 对每个 sensor 分别处理
		for (int i = 0; i < total_pipeline_num; i++) {
			pipeline_info_t *p = args->pipeline_info[i];
			hbn_vnode_image_t out_img = {0};

			  // 获取VIN帧
			if (hbn_vnode_getframe(p->pipe_contexts.gpu2d_node_handle, p->vse_bind_codec_chn, 2000, &out_img) != 0) {
				p->stats.drop_count++;
				printf("hbn_vnode_getframe n2d channel %d failed\n", 0);
				continue;
			}

			// 将帧数据写入文件
			if(count % 10 == 0) {
				snprintf(dst_file, sizeof(dst_file),
				"gdc_handle_%d_chn%d_%dx%d_stride_%d_frameid_%d_ts_%ld.yuv",
				(int)p->pipe_contexts.gpu2d_node_handle, p->vse_bind_codec_chn,
				out_img.buffer.width, out_img.buffer.height, out_img.buffer.stride,
				out_img.info.frame_id, out_img.info.timestamps);
				printf("gdc(%d) dump yuv %dx%d(stride:%d), buffer size: %ld + %ld frame id: %d,"
						" timestamp: %ld\n", (int)p->pipe_contexts.gpu2d_node_handle,
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
			}

			printf("Pipeline %d: Received frame %d from n2d channel %d, frame_id: %d\n",
				i, count, p->vse_bind_codec_chn, out_img.info.frame_id);
			hbn_vnode_releaseframe(p->pipe_contexts.gpu2d_node_handle, p->vse_bind_codec_chn, &out_img);
		}
		count++;
	}
	return NULL;
}

int main(int argc, char** argv) {
	int ret = 0;
	int c = 0;
	int index = -1;

	thread_args_t *args = malloc(sizeof(thread_args_t));
	pipeline_info_t pipeline_info[MAX_SENSORS] = {0};
	pthread_t  capture_thread;

	if (argc <= 1) {
		show_help();
		return 0;
	}

	while ((c = getopt_long(argc, argv, "s:vh:d", long_options, NULL)) != -1) {
		switch (c) {
		case 's':
			if (total_pipeline_num >= MAX_SENSORS) {
				fprintf(stderr, "Too many configurations. Maximum allowed is %d.\n", MAX_SENSORS);
				return 1;
			}
			parse_config(&pipeline_info[total_pipeline_num], optarg, total_pipeline_num);
			total_pipeline_num++;
			break;
		case 'v':
			verbose_flag = 1;
		break;
		case 'h':
		default:
			show_help();
			return 0;
		}
	}

	// 处理后的参数在这里可以使用
	for (int i = 0; i < total_pipeline_num; i++) {
		args->pipeline_info[i] = &pipeline_info[i];
		printf("Pipeline index %d:\n", i);
		printf("\tSensor index: %d\n", pipeline_info[i].select_sensor_id);
		printf("\tSensor name: %s\n", pipeline_info[i].pipe_contexts.sensor_config->sensor_name);
		printf("\tActive mipi host: %d\n", pipeline_info[i].active_mipi_host);
		printf("\tVse Channel: %d\n", pipeline_info[i].vse_bind_codec_chn);
	}

	printf("MIPI host: 0x%x\n", used_mipi_host);
	for (int i = 0; i < MAX_SENSORS; i++) {
		if (used_mipi_host & (1 << i)) {
			printf("  Host %d: Used\n", i);
		}
	}
	printf("Verbose: %d\n", verbose_flag);
	hb_mem_module_open();

	for (index = 0; index < total_pipeline_num; index++) {
		ret = create_and_run_vflow(&args->pipeline_info[index]->pipe_contexts, index);
		if (ret != 0) {
			for (int j = 0; j < index; j++) {
				hbn_vflow_stop(args->pipeline_info[j]->pipe_contexts.vflow_fd);
				hbn_vflow_destroy(args->pipeline_info[j]->pipe_contexts.vflow_fd);
			}
			return 0;
		}
	}

	for (int i = 0; i < total_pipeline_num; i++) {
		printf("Before create_serdes_fd_and_attach: Pipeline %d, cam_fd = %ld\n",
		i, args->pipeline_info[i]->pipe_contexts.cam_fd);
	}

	printf("pthread_create\n");

	running = 1;
	if (pthread_create(&capture_thread, NULL, encode_vse_chn_data, (void *)args) != 0) {
		fprintf(stderr, "[FATAL] Encoder thread create failed\n");
		goto cleanup_main;
	}

	// 主线程等待退出信号
	printf("Running... Press Ctrl+C to exit\n");
	while (running) {
		sleep(1); // 保持主线程存活
	}

cleanup_main:
	running = 0;
	// 等待线程退出
	if (capture_thread) pthread_join(capture_thread, NULL);
	printf("pthread_join success\n");
	for (int i = 0; i < total_pipeline_num; i++) {
		ret = hbn_vflow_stop(pipeline_info[i].pipe_contexts.vflow_fd);
		if (ret != 0) {
			printf("hbn_vflow_stop failed for sensor %d. ret = %d\n", i, ret);
		}
		hbn_vflow_destroy(pipeline_info[i].pipe_contexts.vflow_fd);
	}
	free(args);
	hb_mem_module_close();
	return 0;
}