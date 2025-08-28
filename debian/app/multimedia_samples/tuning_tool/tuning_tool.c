/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <pthread.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "tuning_cmd.h"
#include "tuning_tool.h"
#include "common_utils.h"
#include "vp_sensors.h"
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "common_utils.h"
#include "hb_media_codec.h"
#include "hb_media_error.h"

static void print_help() {
	printf("Usage: isp_tuning [OPTIONS]\n");
	printf("Options:\n");
	printf("  -s <sensor_index>      Specify sensor index\n");
	printf("  -t <settle_value>      Specify settle time for debug\n");
	printf("  -m <sensor_mode>       Specify sensor mode of camera_config_t\n");
	printf("  -r 1                   Send raw to hbplayer\n");
	printf("  -w 2                   Dump 20 yuv from the start\n");
	printf("  -f -H -W -F            feedback raw file xx with specified height, width, and format(raw8/raw10/raw12)\n");
	printf("  -h                     Show this help message\n");
	vp_show_sensors_list(); // Assuming this function displays sensor list
}

tuning_context_t *global_ctx;
static int settle = -1;
static uint32_t sensor_mode = 0; // 1: NORMAL_M; 2: DOL2_M; 6: SLAVE_M
static uint32_t pipelinemode = 2; // 0: Online ; 1: MCM; 2: Offline
static uint32_t feedback_raw_hight;
static uint32_t feedback_raw_width;
static char feedback_raw_format[32] = {0};
static int32_t used_mipi_host = 0;

int32_t lut3d_map[LUT_SIZE][LUT_SIZE][LUT_SIZE][3];

static int is_number(const char *str) {
	while (*str) {
		if (!isdigit(*str)) return RET_SUCCESS;
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

void parse_config(pipe_contex_info_t *pipe_contex_info, const char *config, int pipeline_idx) {
	char *parts[4];
	int sensor_idx = -1;
	int count = split_string(config, " ", parts, 4);

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
				pipe_contex_info->pipe_contex.sensor_config = vp_sensor_config_list[sensor_idx];
				printf("Using index:%d  sensor_name:%s  config_file:%s\n",
						sensor_idx,
						vp_sensor_config_list[sensor_idx]->sensor_name,
						vp_sensor_config_list[sensor_idx]->config_file);
			} else {
				printf("Unsupport sensor index:%d\n", sensor_idx);
				vp_show_sensors_list();
				exit(0);
			}
			if(strcmp(pipe_contex_info->pipe_contex.sensor_config->sensor_name, "dummy") != 0){
				if(vp_sensor_multi_fixed_mipi_host(pipe_contex_info->pipe_contex.sensor_config, used_mipi_host,
					&pipe_contex_info->pipe_contex.csi_config) < 0) {
					printf("vp sensor fixed mipi host fail, sensor id %d."
						"Maybe No Camera Sensor found. Please check if the specified "
						"sensor is connected to the Camera interface.\n\n", sensor_idx);
					exit(0);
				}
			}
			pipe_contex_info->select_sensor_id = sensor_idx;
			// active_mipi_host 的配置在create_vin_node函数中需要再配置一下
			pipe_contex_info->active_mipi_host = pipe_contex_info->pipe_contex.sensor_config->vin_node_attr->cim_attr.mipi_rx;
			used_mipi_host |= (1 << pipe_contex_info->pipe_contex.sensor_config->vin_node_attr->cim_attr.mipi_rx);
		} else {
			fprintf(stderr, "Unknown key: %s\n", key_value[0]);
			exit(0);
		}

		for (int j = 0; j < kv_count; j++) {
			free(key_value[j]);
		}
	}

	for (int i = 0; i < count; i++) {
		free(parts[i]);
	}
}

static int parse_opts(int argc, char *argv[], tuning_context_t *ctx)
{
	int32_t cmd_ret;
	int32_t raw_type;
	int32_t bit_width;
	const char short_options[] = PARSE_SHORT_OPTS;
	const struct option long_options[] = PARSE_LONG_OPTS;
	int option_index = 0;

	if (argc == 1) {
		vp_show_sensors_list();
		return RET_SUCCESS;
	}

	while (1) {
		cmd_ret = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (cmd_ret == -1){
			break;
		}

		switch (cmd_ret) {
		case 's':
			if (ctx->sensor_count >= MAX_SENSORS) {
				fprintf(stderr, "Too many configurations. Maximum allowed is %d.\n", MAX_SENSORS);
				return 1;
			}
			parse_config(&ctx->pipe_contex_info[ctx->sensor_count], optarg, ctx->sensor_count);
			ctx->sensor_count++;
			break;
		case 't':
			settle = atoi(optarg);
			break;
		case 'm':
			sensor_mode = atoi(optarg);
			break;
		case 'r':
			ctx->send_raw = atoi(optarg);
			break;
		case 'l':
			ctx->dump_stream_flag = atoi(optarg);
			break;
		case 'w':
			ctx->work_mode = atoi(optarg);
			break;
		case 0:
			if (strcmp(long_options[option_index].name, "online") == 0) {
				printf("Online mode enabled!!!\n");
				pipelinemode = 0;
			}
			if (strcmp(long_options[option_index].name, "offline") == 0) {
				printf("Offline mode enabled!!!\n");
				pipelinemode = 2;
			}
			if (strcmp(long_options[option_index].name, "mcm") == 0) {
				printf("MCM mode enabled!!!\n");
				pipelinemode = 1;
			}
			break;
		case 'f':
			ctx->feedback_times = atoi(optarg);
			break;
		case 'H':
			feedback_raw_hight = atoi(optarg);
			break;
		case 'W':
			feedback_raw_width = atoi(optarg);
			break;
		case 'F':
			strncpy(feedback_raw_format, optarg, sizeof(feedback_raw_format) - 1);
			feedback_raw_format[sizeof(feedback_raw_format) - 1] = '\0';
			break;
		case 'a':
			bit_mask(ctx->work_mode, LUT3D_MASK);
			break;
		case 'h':
			print_help();
			return RET_SUCCESS;
		default:
			print_help();
			return RET_SUCCESS;
		}
	}

	// 处理后的参数在这里可以使用
	for (int i = 0; i < ctx->sensor_count ; i++) {
		printf("Pipeline index %d:\n", i);
		printf("\tSensor index: %d\n", ctx->pipe_contex_info[i].select_sensor_id);
		printf("\tSensor name: %s\n", ctx->pipe_contex_info[i].pipe_contex.sensor_config->sensor_name);
		printf("\tUse mipi host: %d\n", ctx->pipe_contex_info[i].active_mipi_host);
		raw_type = (!strcmp(feedback_raw_format, "raw8")) ? 0x2A : 
			(!strcmp(feedback_raw_format, "raw10")) ? 0x2B :
			(!strcmp(feedback_raw_format, "raw12")) ? 0x2C : 0x2B;
		bit_width = (!strcmp(feedback_raw_format, "raw8")) ? 8 :
			(!strcmp(feedback_raw_format, "raw10")) ? 10 :
			(!strcmp(feedback_raw_format, "raw12")) ? 12 : 10;
		if(strcmp(ctx->pipe_contex_info[i].pipe_contex.sensor_config->sensor_name, "dummy") == 0){
			printf("feedback_raw_width: %d feedback_raw_hight:  %d raw_type: %#X\n",feedback_raw_width, feedback_raw_hight,raw_type);
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->camera_config->format = raw_type;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->camera_config->height = feedback_raw_hight;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->camera_config->width = feedback_raw_width;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->vin_ichn_attr->format = raw_type;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->vin_ichn_attr->height = feedback_raw_hight;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->vin_ichn_attr->width = feedback_raw_width;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->vin_ochn_attr->vin_basic_attr.format = raw_type;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->vin_ochn_attr->vin_basic_attr.wstride = feedback_raw_width * 2;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->vin_ochn_attr->vin_basic_attr.vstride = feedback_raw_hight;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->isp_attr->input_mode = 2;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->isp_attr->crop.w = feedback_raw_width;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->isp_attr->crop.h = feedback_raw_hight;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->isp_ichn_attr->height = feedback_raw_hight;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->isp_ichn_attr->width = feedback_raw_width;
			ctx->pipe_contex_info[i].pipe_contex.sensor_config->isp_ichn_attr->bit_width = bit_width;

		}
	}

	return 1;
}

static int32_t tuning_feeback_prepare_next(tuning_context_t *ctx)
{
	struct stat statbuf;
	FILE *file = NULL;

	stat(ctx->img_path[ctx->cur_img], &statbuf);
	if (!statbuf.st_size) {
		pr_tuning("No such file: %s\n", ctx->img_path[ctx->cur_img]);
		return RET_FAILURE;
	}
#ifdef TUNING_DEBUG
	pr_tuning("feedback file: %s, index %d, size %ld\n",
		ctx->img_path[ctx->cur_img], ctx->cur_img, statbuf.st_size);
#endif

	file = fopen(ctx->img_path[ctx->cur_img], "r");
	if (!file) {
		pr_tuning("open %s fail\n", ctx->img_path[ctx->cur_img]);
		return RET_FAILURE;
	}

	if ((uint32_t)statbuf.st_size > ctx->src_img.buffer.size[0]) {
		pr_tuning("alloc buffer donot match src file size!\n");
		return RET_FAILURE;
	}

	fread(ctx->src_img.buffer.virt_addr[0], 1, statbuf.st_size, file);
	fclose(file);

	if ((ctx->feedback_times <= 1) &&
		((ctx->cur_img + 1) >= ctx->img_num)) {
		pr_tuning("feedback raw list done!\n");
		return RET_FAILURE;
	}

	if ((ctx->cur_img + 1) >= ctx->img_num) {
		ctx->cur_img = 0;
		ctx->feedback_times -= 1;
	} else {
		ctx->cur_img += 1;
	}

	return RET_SUCCESS;
}

static void *tuning_main_worker_thread(void *arg)
{
	int i = 0;
	int ret;
	hbn_vnode_image_t raw_img = {0};
	hbn_vnode_image_t yuv_img = {0};
	static int32_t yuv_stream_cnt = 0;
	hbn_vnode_handle_t isp_node_handle;
	hbn_vnode_handle_t vin_node_handle;
	enum RAW_BIT raw_type;
	char file_name[32] = {0};
	tuning_context_t *ctx = (tuning_context_t *)arg;;

#ifdef TUNING_DEBUG
	pr_tuning("%s run sensor count: %d\n", __func__, ctx->sensor_count);
#endif

	if (ctx->send_raw) {
		if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
			pr_tuning("Cannot send raw in feedback mode!\n");
			goto out;
		}
		for (i = 0; i < ctx->sensor_count; i++) {
			if (!ctx->pipe_contex_info[i].is_offline) {
				pr_tuning("Sensor-%d ddr disable, cannot send raw!\n", i);
				goto out;
			}
		}
	}
	printf("Input cmd:");

	while (1) {
	for (i = 0; i < ctx->sensor_count; i++) {
		vin_node_handle = ctx->pipe_contex_info[i].pipe_contex.vin_node_handle;
		isp_node_handle = ctx->pipe_contex_info[i].pipe_contex.isp_node_handle;

		if (ctx->send_raw) {
			ret = hbn_vnode_getframe(vin_node_handle, 0, 1000, &raw_img);
			if (ret) {
				pr_tuning("Sensor-%d get buffer from sif fail\n", i);
				goto out;
			}

			if (HBPLAYER_EN) {
				raw_type = (ctx->pipe_contex_info[i].vin_format == 0x2A) ? RAW_8 :
					(ctx->pipe_contex_info[i].vin_format == 0x2B) ? RAW_10 :
					(ctx->pipe_contex_info[i].vin_format == 0x2C) ? RAW_12 : RAW_10;
				ret = tuning_send_raw_to_hbplayer(ctx->hbplayer_event, &raw_img, raw_type, i);
				if (ret)
					pr_tuning("send to hbplayer failed for sensor %d, skip it\n", i);
			}
			hbn_vnode_releaseframe(vin_node_handle, 0, &raw_img);
		}

		if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
			ret = tuning_feeback_prepare_next(ctx);
			if (ret) goto out;
			ret = hbn_vnode_sendframe(isp_node_handle, 0, &ctx->src_img);
			if (ret) {
				pr_tuning("isp hbn_vnode_sendframe failed!\n");
				goto out;
			}
		}

		ret = hbn_vnode_getframe(isp_node_handle, 0, 1000, &yuv_img);
		if (ret) {
			pr_tuning("Sensor-%d get buffer from isp fail\n", i);
			goto out;
		}

		if (ctx->pipe_contex_info[i].yuv_dump_cnt) {
			snprintf(file_name, TUNING_PRINT_SIZE_MAX, "%s/ISP_S%d_STREAM%d.yuv", DEF_DUMP_PATH, i, yuv_stream_cnt);
			tuning_dump_file(file_name, &yuv_img);
			ctx->pipe_contex_info[i].yuv_dump_cnt--;
			if (!ctx->pipe_contex_info[i].yuv_dump_cnt) {
				yuv_stream_cnt++;
				pr_tuning("Input cmd:");
			}
		}

		if (HBPLAYER_EN) {
			ret = tuning_send_yuv_to_hbplayer(ctx->hbplayer_event, &yuv_img, i);
			if (ret)
				pr_tuning("send to hbplayer failed, skip it\n");
		}
#ifdef TUNING_DEBUG
		// pr_tuning("get buffer size %ld-%ld\n", yuv_img.buffer.size[0], yuv_img.buffer.size[1]);
#endif
		hbn_vnode_releaseframe(isp_node_handle, 0, &yuv_img);
		if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
			usleep(20*1000);
		}
	}
	}
out:
	pr_tuning("typing [q] to quit\n");
	sleep(100000);

	return NULL;
}

tuning_cmd_func_t cmd_funcs[] = TUNING_CMD_FUNC_LIST;
static void tuning_api_func(int32_t cmd, tuning_context_t *ctx)
{
	int32_t i;

	select_id(ctx, &ctx->handle_id);
	for (i = 0; i < ARRAY_SIZE(cmd_funcs); i++) {
		if (cmd_funcs[i].cmd == cmd) {
			cmd_funcs[i].api_func(ctx);
			printf("Input cmd:");
			return;
		}
	}

	if (cmd != 'h')
		printf("Unknown cmd: %c!\n", (char)cmd);
	valid_cmd_print();
	printf("Input cmd:");
}

static void *tuning_api_worker_thread(void *arg)
{
	tuning_context_t *ctx = (tuning_context_t *)arg;

	main_while_func_run(tuning_api_func, ctx)

	return NULL;
}

static int32_t tuning_feeback_init(tuning_context_t *ctx)
{
	isp_ichn_attr_t ichn_attr = {0};
	char file_path[255] = {0};
	hbn_vnode_handle_t isp_node_handle;

	if (getcwd(file_path, sizeof(file_path)) != NULL) {
#ifdef TUNING_DEBUG
		pr_tuning("Current working directory: %s\n", file_path);
#endif
	} else {
		pr_tuning("Getcwd fail\n");
		return RET_FAILURE;
	}

	FUNC_EQ(tuning_get_raw_list(file_path, ctx->img_path, ctx->img_name, &ctx->img_num), 0, return RET_FAILURE);
	if (ctx->img_num < 1) {
		pr_tuning("No raw file found in working directory\n");
		return RET_FAILURE;
	}

	isp_node_handle = ctx->pipe_contex_info[0].pipe_contex.isp_node_handle;
	FUNC_EQ(hbn_vnode_get_ichn_attr(isp_node_handle, 0, &ichn_attr), 0, return RET_FAILURE);
	FUNC_EQ(tuning_alloc_feedback_buffer(&ctx->src_img.buffer, ichn_attr.width, ichn_attr.height, 0), 0, return RET_FAILURE);

	return RET_SUCCESS;
}

const char *kernelSource =
"__kernel void apply_3dlut(__global const unsigned char* buf_src, __global unsigned char* buf_opencl, __global int* lut3d_map, int lut_size, int img_height, int img_width) {\n"
"	int height = get_global_id(0);\n"
"	int width = get_global_id(1);\n"
"	int off = img_width * img_height;\n"

"	int y_pos = height * img_width + width;\n"
"	int u_pos = off + (height / 2) * img_width + (width & ~1u);\n"
"	int v_pos = off + (height / 2) * img_width + (width | 1u);\n"

"	unsigned char Y = buf_src[y_pos];\n"
"	unsigned char U = buf_src[u_pos];\n"
"	unsigned char V = buf_src[v_pos];\n"

"	int R = Y + (int)(1.403 * (V - 128));\n"
"	int G = Y - (int)(0.344136 * (U - 128) + 0.714136 * (V - 128));\n"
"	int B = Y + (int)(1.772 * (U - 128));\n"

"	R = clamp(R, 0, 255);\n"
"	G = clamp(G, 0, 255);\n"
"	B = clamp(B, 0, 255);\n"

"	float x = R / 255.0f * (lut_size - 1);\n"
"	float y = G / 255.0f * (lut_size - 1);\n"
"	float z = B / 255.0f * (lut_size - 1);\n"

"	int x0 = (int)x, y0 = (int)y, z0 = (int)z;\n"
"	int x1 = min(x0 + 1, lut_size - 1);\n"
"	int y1 = min(y0 + 1, lut_size - 1);\n"
"	int z1 = min(z0 + 1, lut_size - 1);\n"

"	float dx = x - x0;\n"
"	float dy = y - y0;\n"
"	float dz = z - z0;\n"

"	float tmp[3] = {0};\n"
"	for (int i = 0; i < 3; i++) {\n"
"		tmp[i] = (1 - dx) * (1 - dy) * (1 - dz) * lut3d_map[((x0 * lut_size + y0) * lut_size + z0) * 3 + i] +\n"
"			dx * (1 - dy) * (1 - dz) * lut3d_map[((x1 * lut_size + y0) * lut_size + z0) * 3 + i] +\n"
"			(1 - dx) * dy * (1 - dz) * lut3d_map[((x0 * lut_size + y1) * lut_size + z0) * 3 + i] +\n"
"			(1 - dx) * (1 - dy) * dz * lut3d_map[((x0 * lut_size + y0) * lut_size + z1) * 3 + i] +\n"
"			dx * (1 - dy) * dz * lut3d_map[((x1 * lut_size + y0) * lut_size + z1) * 3 + i] +\n"
"			(1 - dx) * dy * dz * lut3d_map[((x0 * lut_size + y1) * lut_size + z1) * 3 + i] +\n"
"			dx * dy * (1 - dz) * lut3d_map[((x1 * lut_size + y1) * lut_size + z0) * 3 + i] +\n"
"			dx * dy * dz * lut3d_map[((x1 * lut_size + y1) * lut_size + z1) * 3 + i];\n"
"		tmp[i] = min(tmp[i], 65280.0f);\n"
"	}\n"

"	unsigned char R_R = ((unsigned char)tmp[0]) >> 8;\n"
"	unsigned char G_R = ((unsigned char)tmp[1]) >> 8;\n"
"	unsigned char B_R = ((unsigned char)tmp[2]) >> 8;\n"

"	int Y_R = (int)(0.299 * R_R + 0.587 * G_R + 0.114 * B_R);\n"
"	int U_R = (int)(-0.169 * R_R - 0.331 * G_R + 0.500 * B_R + 128);\n"
"	int V_R = (int)(0.500 * R_R - 0.419 * G_R - 0.081 * B_R + 128);\n"

"	buf_opencl[y_pos] = (unsigned char)clamp(Y_R, 0, 255);\n"
"	buf_opencl[u_pos] = (unsigned char)clamp(U_R, 0, 255);\n"
"	buf_opencl[v_pos] = (unsigned char)clamp(V_R, 0, 255);\n"
"} \n";

static int32_t tuning_opencl_3dlut_init(pipe_contex_info_t *pipe_info)
{
	cl_int ret;
	size_t log_size;
	char *log;
	size_t img_size;
	tuning_opencl_ctx_t *opencl_ctx = &pipe_info->opencl_ctx;

	ret = clGetPlatformIDs(1, &opencl_ctx->platform, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clGetPlatformIDs fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	ret = clGetDeviceIDs(opencl_ctx->platform, CL_DEVICE_TYPE_GPU, 1, &opencl_ctx->device, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clGetDeviceIDs fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	opencl_ctx->context = clCreateContext(NULL, 1, &opencl_ctx->device, NULL, NULL, &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateContext fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	opencl_ctx->queue = clCreateCommandQueue(opencl_ctx->context, opencl_ctx->device, 0, &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateCommandQueue fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	opencl_ctx->program = clCreateProgramWithSource(opencl_ctx->context, 1, &kernelSource, NULL, &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateCommandQueue fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	ret = clBuildProgram(opencl_ctx->program, 1, &opencl_ctx->device, NULL, NULL, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clBuildProgram fail, ret: %d\n", ret);
		clGetProgramBuildInfo(opencl_ctx->program, opencl_ctx->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

		log = (char*)malloc(log_size + 1);
		if (log) {
			clGetProgramBuildInfo(opencl_ctx->program, opencl_ctx->device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
			log[log_size] = '\0';
			pr_tuning("Build Log: \n%s\n", log);
			free(log);
		}
		return RET_FAILURE;
	}

	opencl_ctx->kernel = clCreateKernel(opencl_ctx->program, "apply_3dlut", &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateKernel fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	img_size = pipe_info->img_width * pipe_info->img_height * 1.5;
	opencl_ctx->input_image = clCreateBuffer(opencl_ctx->context, CL_MEM_READ_ONLY, img_size, NULL, &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateBuffer fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	opencl_ctx->output_image = clCreateBuffer(opencl_ctx->context, CL_MEM_WRITE_ONLY, img_size, NULL, &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateBuffer fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	opencl_ctx->lut_buffer = clCreateBuffer(opencl_ctx->context, CL_MEM_READ_ONLY,
						LUT_SIZE * LUT_SIZE * LUT_SIZE * 3 * sizeof(int), NULL, &ret);
	if (ret != CL_SUCCESS) {
		pr_tuning("clCreateBuffer fail, ret: %d\n", ret);
		return RET_FAILURE;
	}

	return RET_SUCCESS;
}

static void tuning_opencl_3dlut_deinit(tuning_opencl_ctx_t *opencl_ctx)
{
	clReleaseMemObject(opencl_ctx->input_image);
	clReleaseMemObject(opencl_ctx->output_image);
	clReleaseMemObject(opencl_ctx->lut_buffer);
	clReleaseKernel(opencl_ctx->kernel);
	clReleaseProgram(opencl_ctx->program);
	clReleaseCommandQueue(opencl_ctx->queue);
	clReleaseContext(opencl_ctx->context);
}

void lut3d_map_init()
{
	int r, g, b;

	for (r = 0; r < LUT_SIZE; r++) {
	for (g = 0; g < LUT_SIZE; g++) {
	for (b = 0; b < LUT_SIZE; b++) {
	if (r < LUT_SIZE / 2 && g < LUT_SIZE / 2 && b < LUT_SIZE / 2) {
		lut3d_map[r][g][b][0] = FLOAT288INT((LUT_KNEE * r) / ((LUT_SIZE - 1) / 2.0));
		lut3d_map[r][g][b][1] = FLOAT288INT((LUT_KNEE * g) / ((LUT_SIZE - 1) / 2.0));
		lut3d_map[r][g][b][2] = FLOAT288INT((LUT_KNEE * b) / ((LUT_SIZE - 1) / 2.0));
	} else {
		lut3d_map[r][g][b][0] = FLOAT288INT(((255.0 - LUT_KNEE) * (r - LUT_SIZE / 2.0)) / ((LUT_SIZE - 1) / 2.0) + LUT_KNEE);
		lut3d_map[r][g][b][1] = FLOAT288INT(((255.0 - LUT_KNEE) * (g - LUT_SIZE / 2.0)) / ((LUT_SIZE - 1) / 2.0) + LUT_KNEE);
		lut3d_map[r][g][b][2] = FLOAT288INT(((255.0 - LUT_KNEE) * (b - LUT_SIZE / 2.0)) / ((LUT_SIZE - 1) / 2.0) + LUT_KNEE);
	}
	// lut3d_map[r][g][b][0] = (255.0 * r) / ((LUT_SIZE - 1));
	// lut3d_map[r][g][b][1] = (255.0 * g) / ((LUT_SIZE - 1));
	// lut3d_map[r][g][b][2] = (255.0 * b) / ((LUT_SIZE - 1));
	}
	}
	}
}

static int32_t create_camera_node(pipe_contex_t *pipe_contex)
{
	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;

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

	FUNC_EQ(hbn_camera_create(camera_config, &pipe_contex->cam_fd), 0, return RET_FAILURE);

	return RET_SUCCESS;
}

static int32_t create_vin_node(pipe_contex_t *pipe_contex, uint32_t pipelinemode)
{
	vp_sensor_config_t *sensor_config = NULL;
	vin_node_attr_t *vin_node_attr = NULL;
	vin_ichn_attr_t *vin_ichn_attr = NULL;
	vin_ochn_attr_t *vin_ochn_attr = NULL;
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	uint32_t hw_id = 0;
	uint32_t ichn_id = 0;
	uint32_t ochn_id = 0;
	uint64_t vin_attr_ex_mask = 0;

	sensor_config = pipe_contex->sensor_config;
	vin_node_attr = sensor_config->vin_node_attr;
	vin_ichn_attr = sensor_config->vin_ichn_attr;
	vin_ochn_attr = sensor_config->vin_ochn_attr;
	hw_id = vin_node_attr->cim_attr.mipi_rx;
	vin_node_handle = &pipe_contex->vin_node_handle;

	switch (pipelinemode) {
	case Online:
		vin_node_attr->cim_attr.cim_isp_flyby = 1;
		vin_ochn_attr->ddr_en = 0;
		break;
	case MCM:
		vin_node_attr->cim_attr.cim_isp_flyby = 1;
		vin_ochn_attr->ddr_en = 1;
		break;
	case Offline:
		vin_node_attr->cim_attr.cim_isp_flyby = 0;
		vin_ochn_attr->ddr_en = 1;
		break;
	default:
		break;
	}

	if(pipe_contex->csi_config.mclk_is_not_configed){
		//设备树中没有配置mclk：使用外部晶振
		pr_tuning("csi%d ignore mclk ex attr, because not config mclk.\n",
			pipe_contex->csi_config.index);
	}else{
		vin_attr_ex.vin_attr_ex_mask = sensor_config->vin_attr_ex->vin_attr_ex_mask;
		vin_attr_ex.mclk_ex_attr.mclk_freq = sensor_config->vin_attr_ex->mclk_ex_attr.mclk_freq;
		vin_attr_ex_mask = vin_attr_ex.vin_attr_ex_mask;
	}

	FUNC_EQ(hbn_vnode_open(HB_VIN, hw_id, AUTO_ALLOC_ID, vin_node_handle), 0, return RET_FAILURE);
	FUNC_EQ(hbn_vnode_set_attr(*vin_node_handle, vin_node_attr), 0, return RET_FAILURE);
	FUNC_EQ(hbn_vnode_set_ichn_attr(*vin_node_handle, ichn_id, vin_ichn_attr), 0, return RET_FAILURE);
	FUNC_EQ(hbn_vnode_set_ochn_attr(*vin_node_handle, ochn_id, vin_ochn_attr), 0, return RET_FAILURE);

	if (vin_attr_ex_mask) {
		for (uint8_t i = 0; i < VIN_ATTR_EX_INVALID; i ++) {
			if ((vin_attr_ex_mask & (1 << i)) == 0)
				continue;

			vin_attr_ex.ex_attr_type = i;
			/*we need to set hbn_vnode_set_attr_ex in a loop*/
			FUNC_EQ(hbn_vnode_set_attr_ex(*vin_node_handle, &vin_attr_ex), 0, return RET_FAILURE);
		}
	}

	if(pipelinemode == Offline || pipelinemode == MCM){
		hbn_buf_alloc_attr_t alloc_attr = {0};
		alloc_attr.buffers_num = 3;
		alloc_attr.is_contig = 1;
		alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
				| HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED
				| HB_MEM_USAGE_HW_CIM
				| HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
		FUNC_EQ(hbn_vnode_set_ochn_buf_attr(*vin_node_handle, ochn_id, &alloc_attr), 0, return RET_FAILURE);
	}
	return RET_SUCCESS;
}

static int32_t create_isp_node(pipe_contex_t *pipe_contex, uint32_t pipelinemode) {
	vp_sensor_config_t *sensor_config = NULL;
	isp_attr_t      *isp_attr = NULL;
	isp_ichn_attr_t *isp_ichn_attr = NULL;
	isp_ochn_attr_t *isp_ochn_attr = NULL;
	hbn_vnode_handle_t *isp_node_handle = NULL;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t ichn_id = 0;
	uint32_t ochn_id = 0;

	sensor_config = pipe_contex->sensor_config;
	isp_attr = sensor_config->isp_attr;
	isp_ichn_attr = sensor_config->isp_ichn_attr;
	isp_ochn_attr = sensor_config->isp_ochn_attr;
	isp_node_handle = &pipe_contex->isp_node_handle;

	switch (pipelinemode) {
	case Online:
		isp_attr->input_mode = 0;
		break;
	case MCM:
		isp_attr->input_mode = 1;
		break;
	case Offline:
		isp_attr->input_mode = 2;
		break;
	default:
		break;
	}

	FUNC_EQ(hbn_vnode_open(HB_ISP, 0, AUTO_ALLOC_ID, isp_node_handle), 0, return RET_FAILURE);
	FUNC_EQ(hbn_vnode_set_attr(*isp_node_handle, isp_attr), 0, return RET_FAILURE);
	FUNC_EQ(hbn_vnode_set_ochn_attr(*isp_node_handle, ochn_id, isp_ochn_attr), 0, return RET_FAILURE);
	FUNC_EQ(hbn_vnode_set_ichn_attr(*isp_node_handle, ichn_id, isp_ichn_attr), 0, return RET_FAILURE);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
			| HB_MEM_USAGE_CPU_WRITE_OFTEN
			| HB_MEM_USAGE_CACHED;
	FUNC_EQ(hbn_vnode_set_ochn_buf_attr(*isp_node_handle, ochn_id, &alloc_attr), 0, return RET_FAILURE);

	return RET_SUCCESS;
}

static int32_t multi_pipe_create(tuning_context_t *ctx, uint32_t pipelinemode)
{
	int32_t i, ret = 0;
	pipe_contex_t *pipe_contex;

	for (i = 0; i < ctx->sensor_count; i++) {
		pipe_contex = &ctx->pipe_contex_info[i].pipe_contex;

		FUNC_EQ(create_camera_node(pipe_contex), 0, return RET_FAILURE);
		FUNC_EQ(create_vin_node(pipe_contex, pipelinemode), 0, return RET_FAILURE);
		FUNC_EQ(create_isp_node(pipe_contex, pipelinemode), 0, return RET_FAILURE);
		FUNC_EQ(hbn_vflow_create(&pipe_contex->vflow_fd), 0, return RET_FAILURE);
		FUNC_EQ(hbn_vflow_add_vnode(pipe_contex->vflow_fd, pipe_contex->vin_node_handle), 0, return RET_FAILURE);
		FUNC_EQ(hbn_vflow_add_vnode(pipe_contex->vflow_fd, pipe_contex->isp_node_handle), 0, return RET_FAILURE);

		if (!BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
			switch (pipelinemode) {
			case Online:
			case MCM:
				ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
					pipe_contex->vin_node_handle, 1,
					pipe_contex->isp_node_handle, 0);
				break;
			case Offline:
				ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
					pipe_contex->vin_node_handle, 0,
					pipe_contex->isp_node_handle, 0);
				break;
			default:
				break;
			}
			if (ret < 0) {
				printf("sensor-%d hbn_vflow_bind_vnode fail\n", i);
				return ret;
			}
		}

		FUNC_EQ(hbn_camera_attach_to_vin(pipe_contex->cam_fd, pipe_contex->vin_node_handle), 0, return RET_FAILURE);
	}

	return ret;
}

static int32_t tuning_case_run(tuning_context_t *ctx)
{
	int32_t i, ret = 0;
	isp_ichn_attr_t ichn_attr = {0};
	vin_ochn_attr_t ochn_attr = {0};
	pipe_contex_t *pipe_contex;

	for (i = 0; i < ctx->sensor_count; i++) {
		pipe_contex = &ctx->pipe_contex_info[i].pipe_contex;
		FUNC_EQ(hbn_vnode_get_ichn_attr(pipe_contex->isp_node_handle, 0, &ichn_attr), 0, return RET_FAILURE);
		FUNC_EQ(hbn_vnode_get_ochn_attr(pipe_contex->vin_node_handle, 0, &ochn_attr), 0, return RET_FAILURE);

		ctx->pipe_contex_info[i].vin_format = ochn_attr.vin_basic_attr.format;
		ctx->pipe_contex_info[i].is_offline = ochn_attr.ddr_en ? 1 : 0;
		ctx->pipe_contex_info[i].img_width = ichn_attr.width;
		ctx->pipe_contex_info[i].img_height = ichn_attr.height;

		if (BIT_ENABLE(ctx->work_mode, START_DUMP_MASK))
			ctx->pipe_contex_info[i].yuv_dump_cnt = 20;
	}

	if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
		pr_tuning("tuning_tool run with feedback\n");
		ctx->feedback_times = ctx->feedback_times == 0 ? 0xFFFF : ctx->feedback_times;
		FUNC_EQ(tuning_feeback_init(ctx), 0, goto destroy_cam);
	}

	if (HBPLAYER_EN) {
		ctx->hbplayer_event = hb_tool_start_transfer(0);
		hb_tool_event_setcb(ctx->hbplayer_event, NULL, NULL, NULL, NULL, NULL);
	}

	if (BIT_ENABLE(ctx->work_mode, LUT3D_MASK)) {
		pr_tuning("tuning_tool can support 3dlut now\n");
		lut3d_map_init();
		for (i = 0; i < ctx->sensor_count; i++) {
			tuning_opencl_3dlut_init(&ctx->pipe_contex_info[i]);
		}
	}

	for (i = 0; i < ctx->sensor_count; i++) {
		pipe_contex = &ctx->pipe_contex_info[i].pipe_contex;
		hbn_vflow_start(pipe_contex->vflow_fd);
	}

	// todo: create multi pthread
	FUNC_EQ(pthread_create(&ctx->main_thid, NULL, tuning_main_worker_thread, (void *)(ctx)), 0, goto destroy);
	FUNC_EQ(pthread_create(&ctx->api_thid, NULL, tuning_api_worker_thread, (void *)(ctx)), 0, goto destroy);

	pthread_join(ctx->api_thid, NULL);
#ifdef TUNING_DEBUG
	pr_tuning("api thread join done\n");
#endif
	pthread_cancel(ctx->main_thid);
#ifdef TUNING_DEBUG
	pr_tuning("main thread cancel done\n");
#endif

destroy:
	if (BIT_ENABLE(ctx->work_mode, LUT3D_MASK)) {
		for (i = 0; i < ctx->sensor_count; i++) {
			tuning_opencl_3dlut_deinit(&ctx->pipe_contex_info[i].opencl_ctx);
		}
	}

	if (HBPLAYER_EN)
		hb_tool_stop_transfer(ctx->hbplayer_event);
#ifdef TUNING_DEBUG
	pr_tuning("stop transfer done\n");
#endif

	if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK))
		tuning_free_feedback_buffer(&ctx->src_img.buffer);

destroy_cam:
	for (i = 0; i < ctx->sensor_count; i++) {
		pipe_contex = &ctx->pipe_contex_info[i].pipe_contex;
		hbn_vflow_stop(pipe_contex->vflow_fd);
		hbn_camera_destroy(pipe_contex->cam_fd);
		hbn_vflow_destroy(pipe_contex->vflow_fd);
	}

	pr_tuning("camsys destory done\n");
	return ret;
}

int32_t main(int argc, char *argv[])
{
	int32_t ret = 0;
	tuning_context_t ctx = {0};
	global_ctx = &ctx;

	ret = parse_opts(argc, argv, &ctx);
	if (ret != 1) {
		return ret;
	}

	if (access(DEF_DUMP_PATH, 0)) {
		ret = mkdir(DEF_DUMP_PATH, 0777);
		if (ret < 0) {
			pr_tuning("mkdir %s for dump failed !\n", DEF_DUMP_PATH);
			return RET_FAILURE;
		}
	}

	FUNC_EQ(multi_pipe_create(&ctx, pipelinemode), 0, return RET_FAILURE);
	FUNC_EQ(tuning_case_run(&ctx), 0, return RET_FAILURE);

	return ret;
}
