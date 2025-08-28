// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
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
#include <ctype.h>

#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"
#include "hb_media_codec.h"
#include "hb_media_error.h"

#include "GC820/nano2D.h"
#include "GC820/nano2D_util.h"

#include "performance_test_util.h"
#include "create_n2d_buffer_wraper.h"

#define MAX_PIPE_NUM 4

#define ALIGN_UP(a, size) (((a) + (size)-1u) & (~((size)-1u)))

typedef struct {
	int select_sensor_id;
	uint32_t sensor_mode;
	pipe_contex_t pipe_contexts;
	int active_mipi_host; // 根据实际的硬件连接情况确定使用对应的 mipi host
	int gpu2d_channel;
	char encode_type[32];
	hbn_vnode_image_t vse_chn_frame;
	media_codec_context_t media_context;
	char output_file[256];
	pthread_t read_codec_thread;
	hbn_vnode_image_t out_img;
} pipeline_info_t;

typedef struct {
	pipeline_info_t *pipeline_info[MAX_PIPE_NUM]; // 使用数组存储指针
} thread_args_t;

static   n2d_error_t error = N2D_SUCCESS;
static   n2d_buffer_t tmpbuffer = {0};
static   n2d_buffer_t dst_rotation = {0};
static	 n2d_buffer_t dst_nv12_buffer = {0};
static   n2d_buffer_t src = {0};

static int32_t total_pipeline_num = 0;
static int32_t verbose_flag = 0;
static int32_t used_mipi_host = 0;

static int32_t running = 0;
static uint32_t n2d_input_width = 0;
static uint32_t n2d_input_height = 0;
static uint32_t yuv_debug_enabled = 0;
static uint32_t sensor_type = 0;
static uint32_t link_port[MAX_PIPE_NUM] = {};

void *read_vse_data(void *contex);
int32_t hbn_deserial_create(deserial_config_t *des_config, deserial_handle_t *des_fd);
int32_t hbn_deserial_attach_to_vin(deserial_handle_t des_fd, camera_des_link_t link, vpf_handle_t vin_fd);


static struct option const long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"verbose", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void print_helpcat(void) {
	printf("Usage: %s [Options]\n", get_program_name());
	printf("Options:\n");
	printf("-c, --config=\"sensor=id output=FILE\"\n");
	printf("\t\tConfigure parameters for each video pipeline, can be repeated up to %d times.\n", MAX_PIPE_NUM);
	printf("\t\tsensor   --  Sensor index,can have multiple parameters, reference sensor list.\n");
	printf("\t\toutput   --  Save bmp data to file.\n");
	printf("-v, --verbose\tEnable verbose mode\n");
	printf("-y, --enable yuv-debug\n");
	printf("-h, --help\tShow help message\n");
	printf("Support sensor list:\n");
	vp_show_sensors_list();
}

void signal_handle(int signo) {
	running = 0;
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
	pipeline_info->gpu2d_channel = 0;
	strncpy(pipeline_info->encode_type, "bmp", sizeof(pipeline_info->encode_type));

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
				sensor_type = pipeline_info->pipe_contexts.sensor_config->sensor_type;
			} else {
				printf("Unsupport sensor index:%d\n", sensor_idx);
				print_helpcat();
				exit(0);
			}
			if(sensor_type == SENSOR_TYPE_NORMAL) {
				ret = vp_sensor_multi_fixed_mipi_host(pipeline_info->pipe_contexts.sensor_config, used_mipi_host,
					&pipeline_info->pipe_contexts.csi_config);
				if (ret < 0) {
					printf("vp sensor fixed mipi host fail, sensor id %d."
						"Maybe No Camera Sensor found. Please check if the specified "
						"sensor is connected to the Camera interface.\n\n", sensor_idx);
					exit(0);
				}
				pipeline_info->select_sensor_id = sensor_idx;
				// active_mipi_host 的配置在 create_vin_node 函数中需要再配置一下
				pipeline_info->active_mipi_host = pipeline_info->pipe_contexts.sensor_config->vin_node_attr->cim_attr.mipi_rx;
				used_mipi_host |= (1 << pipeline_info->pipe_contexts.sensor_config->vin_node_attr->cim_attr.mipi_rx);
			}
		}
		else if (strcmp(key_value[0], "output") == 0) {
			strncpy(pipeline_info->output_file, key_value[1], sizeof(pipeline_info->output_file) - 1);
			pipeline_info->output_file[sizeof(pipeline_info->output_file) - 1] = '\0';
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


static int create_deserial_node(pipe_contex_t *pipe_contex) {

	vp_sensor_config_t *sensor_config = NULL;
	deserial_config_t *deserial_config = NULL;
	deserial_handle_t *des_handle = NULL;

	int32_t ret = 0;
	des_handle = &pipe_contex->des_fd;

	sensor_config = pipe_contex->sensor_config;
	deserial_config = sensor_config->deserial_node_attr;

	ret = hbn_deserial_create(deserial_config, des_handle);
	if(ret != 0){
		printf("hbn_deserial_create failed ret = %d\n", ret);
		return ret;
	}
	if (verbose_flag) {
		printf("deserial_config:%02x_%s, des_handle:%ld \n\r" ,deserial_config->addr,
		deserial_config->name, *des_handle);
	}
	return 0;
}


int create_serdes_fd_and_attach(pipeline_info_t **pipeline_info, int sensor_count) {
	int32_t ret = 0;
	deserial_handle_t tdes[MAX_PIPE_NUM] = { 0 };

	// 打印每个传入管道的信息
	for (int i = 0; i < sensor_count; i++) {
		printf("port_link[%d]: %d, cam_fd[%d]: %ld\n"
		,i ,link_port[i], i, pipeline_info[i]->pipe_contexts.cam_fd);
	}

	ret = create_deserial_node(&pipeline_info[0]->pipe_contexts); // 只传递第一个管道的 pipe_context 创建 des_fd
	tdes[0] = pipeline_info[0]->pipe_contexts.des_fd;
	ERR_CON_EQ(ret, 0);

	// 通过 camera 和 deserial 的 handle，选择对应的 kink_port 将两者绑定，
	for (int i = 0; i < sensor_count; i++) {
		ret = hbn_camera_attach_to_deserial(pipeline_info[i]->pipe_contexts.cam_fd, tdes[0], link_port[i]);
		ERR_CON_EQ(ret, 0);
	}

	// 硬件上带有解串器，将 deserial 与 vin node 绑定，并初始化 gmsl 模组
	for (int i = 0; i < sensor_count; i++) {
		ret = hbn_deserial_attach_to_vin(tdes[0], link_port[i], pipeline_info[i]->pipe_contexts.vin_node_handle);
		ERR_CON_EQ(ret, 0);
	}
	return 0;
}

int32_t vflow_fd_start(pipe_contex_t *pipe_contex)
{
	int32_t ret = 0;

	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	printf("hbn_vflow_start\n");
	return 0;

}

static int create_camera_node(pipe_contex_t *pipe_contex, uint32_t sensor_mode)
{
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

static int create_vin_node(pipe_contex_t *pipe_contex, int active_mipi_host, int index) {
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
	// 调整 mipi_rx 的 index
	vin_node_attr->cim_attr.mipi_rx = active_mipi_host;
	hw_id = vin_node_attr->cim_attr.mipi_rx;
	vin_node_handle = &pipe_contex->vin_node_handle;

	link_port[index] = vin_node_attr->cim_attr.vc_index;
	printf("mipi rx = %d,active_mipi_host = %d \n\r" , hw_id,active_mipi_host);
	if(pipe_contex->csi_config.mclk_is_not_configed){
		// 设备树中没有配置 mclk：使用外部晶振
		printf("csi%d ignore mclk ex attr, because not config mclk.\n",
				pipe_contex->csi_config.index);
	}
	else{
		vin_attr_ex.vin_attr_ex_mask = 0x80;	//bit7 for mclk
		vin_attr_ex.mclk_ex_attr.mclk_freq = 24000000; // 24MHz
		vin_attr_ex_mask = vin_attr_ex.vin_attr_ex_mask;
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

	isp_ochn_attr->fmt= 2;
	isp_attr->input_mode = 1;  // 1: online,  2: offline
	sensor_config->vin_node_attr->cim_attr.cim_isp_flyby = 1;

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
	printf("isp_width:%d , isp_height:%d",input_width , input_height );
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

	n2d_input_width = vse_ochn_attr[vse_bind_index].target_w;
	n2d_input_height = vse_ochn_attr[vse_bind_index].target_h;

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

n2d_error_t rotation_sample(n2d_buffer_t *src, n2d_buffer_t *dst, n2d_orientation_t orientation)
{
	n2d_error_t error = N2D_SUCCESS;
	src->orientation = N2D_0;
	dst->orientation = orientation;

	error = n2d_blit(dst, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE);
	if (N2D_IS_ERROR(error))
	{
		printf("blit error, error=%d.\n", error);
		goto on_error;
	}

	error = n2d_commit();
	if (N2D_IS_ERROR(error))
	{
		printf("blit error, error=%d.\n", error);
		goto on_error;
	}

	return N2D_SUCCESS;
on_error:
	if(N2D_INVALID_HANDLE != src->handle)
	{
		n2d_free(src);
	}

	return error;
}

int create_and_run_vflow(pipe_contex_t *pipe_contex,
	int active_mipi_host, int vse_bind_index, uint32_t sensor_mode, int index) {
	int32_t ret = 0;

	// 创建 pipeline 中的每个 node
	ret = create_camera_node(pipe_contex, sensor_mode);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex, active_mipi_host, index);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);
	ret = create_vse_node(pipe_contex, vse_bind_index);
	ERR_CON_EQ(ret, 0);

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

	if(sensor_type != SENSOR_TYPE_NORMAL)
	return 0;

	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
							pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}

void *encode_vse_chn_data(void *context)
{
	int ret = 0;
	uint32_t count = 0;
	// 根据输入传参决定使用几路 pipeline
	thread_args_t *args = (thread_args_t *)context;
	pipeline_info_t *current_pipeline[total_pipeline_num];
	for (int i = 0; i < total_pipeline_num; i++) {
		current_pipeline[i] = args->pipeline_info[i];
	}

	hbn_vnode_image_t vse_chn_frame = {0};
	error = n2d_open();
	if (N2D_IS_ERROR(error)) {
		printf("open context failed! error=%d.\n", error);
		return NULL;
	}

	N2D_ON_ERROR(n2d_switch_device(N2D_DEVICE_0));
	N2D_ON_ERROR(n2d_switch_core(N2D_CORE_0));
	N2D_ON_ERROR(n2d_util_allocate_buffer(n2d_input_width, n2d_input_height, N2D_ABGR8888, N2D_0, N2D_LINEAR, N2D_TSC_DISABLE, &tmpbuffer));
	N2D_ON_ERROR(n2d_util_allocate_buffer(n2d_input_width, n2d_input_height, N2D_BGRA8888, N2D_0, N2D_LINEAR, N2D_TSC_DISABLE, &dst_rotation));
	N2D_ON_ERROR(n2d_util_allocate_buffer(n2d_input_width, n2d_input_height, N2D_NV12, N2D_0, N2D_LINEAR, N2D_TSC_DISABLE, &dst_nv12_buffer));

	printf("************ n2d read start*********\n\r");

	while (running) {
		for (int index = 0; index < total_pipeline_num; index++) {
			ret = hbn_vnode_getframe(current_pipeline[index]->pipe_contexts.isp_node_handle, args->pipeline_info[index]->gpu2d_channel, 2000, &vse_chn_frame);//current_pipeline[index]->pipe_contexts.gdc_node_handle
			if (ret != 0) {
				printf("sensor_%s_hbn_vnode_getframe GDC channel %d failed, error code %d\n",current_pipeline[index]->output_file, 0, ret);
				continue;
			}

			if(yuv_debug_enabled) {

				char dst_file[128];
				int len = snprintf(dst_file, sizeof(dst_file), "./%s_width:%d_height:%d_stride%d_frameid%d.yuv", \
					current_pipeline[index]->output_file, vse_chn_frame.buffer.width, \
					vse_chn_frame.buffer.height,vse_chn_frame.buffer.stride, \
					vse_chn_frame.info.frame_id);

				if (len < 0 || len >= sizeof(dst_file)) {
					fprintf(stderr, "Warning: Output truncated for file name: %s\n", dst_file);
				}
				dump_2plane_yuv_to_file(dst_file,
					vse_chn_frame.buffer.virt_addr[0],
					vse_chn_frame.buffer.virt_addr[1],
					vse_chn_frame.buffer.size[0],
					vse_chn_frame.buffer.size[1]);
				printf("dump %s ok\n" , current_pipeline[index]->output_file);
			}
			if (count % 3 == 0) {
				// Create buffer from HB memory
				error = create_n2d_buffer_from_hbm_graphic(&src, &vse_chn_frame.buffer);
				if (N2D_IS_ERROR(error)) {
					printf("Error loading buffer from hb_mem, error=%d.\n", error);
					hbn_vnode_releaseframe(current_pipeline[index]->pipe_contexts.vse_node_handle, current_pipeline[index]->gpu2d_channel, &vse_chn_frame);
					continue;
				}

				// Perform Blit operation
				error = n2d_blit(&tmpbuffer, N2D_NULL, &src, N2D_NULL, N2D_BLEND_NONE);
				if (N2D_IS_ERROR(error)) {
					printf("Blit error, error=%d.\n", error);
					goto on_error;
				}
				N2D_ON_ERROR(n2d_commit());

				// Perform rotation
				error = rotation_sample(&tmpbuffer, &dst_rotation, N2D_90);
				if (N2D_IS_ERROR(error)) {
					printf("Rotation failed! error=%d.\n", error);
					goto on_free_src;
				}

				char dst_file[128];
				int len = snprintf(dst_file, sizeof(dst_file), "./%s_width:%d_height:%d_stride%d_frameid%d.bmp", current_pipeline[index]->output_file, vse_chn_frame.buffer.width,
				vse_chn_frame.buffer.height,vse_chn_frame.buffer.stride,
				vse_chn_frame.info.frame_id);
				if (len < 0 || len >= sizeof(dst_file)) {
					fprintf(stderr, "Warning: Output truncated for file name: %s\n", dst_file);
				}
				error = n2d_util_save_buffer_to_file(&dst_rotation, dst_file);
				if (N2D_IS_ERROR(error)) {
					printf("Save to file failed! error=%d.\n", error);
				} else {
					printf("Saved file to [%s].\n", dst_file);
				}
			}

			// 释放帧
			hbn_vnode_releaseframe(current_pipeline[index]->pipe_contexts.isp_node_handle, args->pipeline_info[index]->gpu2d_channel, &vse_chn_frame);
			N2D_ON_ERROR(n2d_free(&src));
		}
		count++;
	}

on_error:
on_free_src:
	N2D_ON_ERROR(n2d_free(&tmpbuffer));
	N2D_ON_ERROR(n2d_free(&dst_rotation));
	N2D_ON_ERROR(n2d_free(&dst_nv12_buffer));
	error = n2d_close();
	if (N2D_IS_ERROR(error)) {
		printf("Close context failed! error=%d.\n", error);
	}
	return NULL;
}


int main(int argc, char** argv) {
	int ret = 0;
	int c = 0;
	int index = -1;

	thread_args_t *args = malloc(sizeof(thread_args_t));
	pipeline_info_t pipeline_info[MAX_PIPE_NUM] = {0}; // 第一个结构体

	if (argc <= 1) {
		print_helpcat();
		return 0;
	}

	while ((c = getopt_long(argc, argv, "c:vh:f:y", long_options, NULL)) != -1) {
		switch (c) {
		case 'c':
			if (total_pipeline_num >= MAX_PIPE_NUM) {
				fprintf(stderr, "Too many configurations. Maximum allowed is %d.\n", MAX_PIPE_NUM);
				return 1;
			}
			parse_config(&pipeline_info[total_pipeline_num], optarg, total_pipeline_num);
			total_pipeline_num++;
			break;
		case 'v':
			verbose_flag = 1;
			break;
		case 'y':  // 处理新的 YUV 调试选项
			yuv_debug_enabled = 1;
		break;
		case 'h':
		default:
			print_helpcat();
			return 0;
		}
	}
	printf("total_pipeline_num:%d\n",total_pipeline_num);

	// 处理后的参数在这里可以使用
	for (int i = 0; i < total_pipeline_num; i++) {
		args->pipeline_info[i] = &pipeline_info[i];
		printf("Pipeline index %d:\n", i);
		printf("\tSensor index: %d\n", pipeline_info[i].select_sensor_id);
		printf("\tSensor name: %s\n", pipeline_info[i].pipe_contexts.sensor_config->sensor_name);
		printf("\tActive mipi host: %d\n", pipeline_info[i].active_mipi_host);
		printf("\tVse Channel: %d\n", pipeline_info[i].gpu2d_channel);
		printf("\tEncode type: %s\n", pipeline_info[i].encode_type);
		printf("\tOutput file: %s\n", pipeline_info[i].output_file);
	}

	printf("MIPI host: 0x%x\n", used_mipi_host);
	for (int i = 0; i < MAX_PIPE_NUM; i++) {
		if (used_mipi_host & (1 << i)) {
			printf("  Host %d: Used\n", i);
		}
	}
	printf("Verbose: %d\n", verbose_flag);

	hb_mem_module_open();
	ERR_CON_EQ(ret, 0);
	for (index = 0; index < total_pipeline_num; index++) {
		ret = create_and_run_vflow(&args->pipeline_info[index]->pipe_contexts,args->pipeline_info[index]->active_mipi_host,
				args->pipeline_info[index]->gpu2d_channel,args->pipeline_info[index]->sensor_mode, index);
		if (ret != 0) {
			for (int j = 0; j < index; j++) {
				hbn_vflow_stop(args->pipeline_info[j]->pipe_contexts.vflow_fd);
				hbn_vflow_destroy(args->pipeline_info[j]->pipe_contexts.vflow_fd);
			}
			return 0;
		}
	}

	if(sensor_type != SENSOR_TYPE_NORMAL) {
		ret = create_serdes_fd_and_attach(args->pipeline_info, total_pipeline_num);
		if (ret != 0) {
			printf("camera_config_init_seq fail for sensor ret = %d\n", ret);
			return ret;
		}

		for (int i = 0; i < total_pipeline_num; i++) {
			ret = vflow_fd_start(&args->pipeline_info[i]->pipe_contexts);
			if (ret != 0) {
				printf("vflow_fd_start fail for sensor ret = %d\n",  ret);
				return ret;
			}
		}
	}

	running = 1;
	pthread_t encode_thread_id;
	ret = pthread_create(&encode_thread_id, NULL, encode_vse_chn_data, (void *)args);
	if (ret != 0) {
		printf("Failed to create encode thread\n");
		return 1;
	}
	/* 线程执行 5s 自动退出 */
	sleep(5);
	running = 0;
	printf(" encode thread end \n");

	pthread_join(encode_thread_id, NULL);

	for (int index = 0; index < total_pipeline_num; index++) {
		ret = hbn_vflow_stop(args->pipeline_info[index]->pipe_contexts.vflow_fd);
		ERR_CON_EQ(ret, 0);
		hbn_vflow_destroy(args->pipeline_info[index]->pipe_contexts.vflow_fd);
	}

	free(args);
	args = NULL;
	hb_mem_module_close();

	return 0;
}
