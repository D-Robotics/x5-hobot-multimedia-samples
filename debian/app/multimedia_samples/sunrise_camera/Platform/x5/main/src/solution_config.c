#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#include "utils/common_utils.h"
#include "utils/cJSON.h"
#include "utils/cJSON_Direct.h"
#include "utils/utils_log.h"

#include "solution_config.h"
#include "vp_wrap.h"
#include "bpu_wrap.h"
#include "vp_gdc.h"

#define SOLUTION_CONFIG_PATH "../test_data/"
#define SOLUTION_CONFIG_FILE SOLUTION_CONFIG_PATH "solution_config.json"

int32_t g_solution_cfg_is_load = 0;
solution_cfg_t g_solution_config;

static key_info_t csi_info_key[] = {
	MAKE_KEY_INFO(csi_info_t, KEY_TYPE_S32, index, NULL),
	MAKE_KEY_INFO(csi_info_t, KEY_TYPE_S32, is_valid, NULL),
	MAKE_KEY_INFO(csi_info_t, KEY_TYPE_S32, mclk_is_not_configed, NULL),
	MAKE_KEY_INFO(csi_info_t, KEY_TYPE_STRING, sensor_config_list, NULL),
	MAKE_END_INFO()
};

static key_info_t csi_list_info_key[] = {
	MAKE_KEY_INFO(csi_list_info_t, KEY_TYPE_S32, valid_count, NULL),
	MAKE_KEY_INFO(csi_list_info_t, KEY_TYPE_S32, max_count, NULL),
	MAKE_ARRAY_INFO(csi_list_info_t, KEY_TYPE_ARRAY, csi_info, csi_info_key, VP_MAX_VCON_NUM, KEY_TYPE_OBJECT),
	MAKE_END_INFO()
};
key_info_t hard_capability_key[] = {
	MAKE_KEY_INFO(solution_hard_capability_t, KEY_TYPE_STRING, chip_type, NULL),
	MAKE_KEY_INFO(solution_hard_capability_t, KEY_TYPE_OBJECT, csi_list_info, csi_list_info_key),
	MAKE_KEY_INFO(solution_hard_capability_t, KEY_TYPE_STRING, model_list, NULL),
	MAKE_KEY_INFO(solution_hard_capability_t, KEY_TYPE_STRING, gdc_status_list, NULL),
	MAKE_KEY_INFO(solution_hard_capability_t, KEY_TYPE_STRING, codec_type_list, NULL),
	MAKE_ARRAY_INFO(solution_hard_capability_t, KEY_TYPE_ARRAY, encode_bit_rate_list, NULL, 16, KEY_TYPE_S32),
	MAKE_KEY_INFO(solution_hard_capability_t, KEY_TYPE_STRING, display_dev_list, NULL),
	MAKE_END_INFO()};

static key_info_t cfg_cam_vpp_key[] = {
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, is_valid, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, is_enable, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, csi_index, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, mclk_is_not_configed, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_STRING, sensor, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, encode_type, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, encode_bitrate, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_STRING, model, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_vpp_t, KEY_TYPE_S32, gdc_status, NULL),
	MAKE_END_INFO()};

static key_info_t cfg_cam_key[] = {
	MAKE_KEY_INFO(solution_cfg_cam_t, KEY_TYPE_S32, pipeline_count, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_t, KEY_TYPE_S32, max_pipeline_count, NULL),
	MAKE_ARRAY_INFO(solution_cfg_cam_t, KEY_TYPE_ARRAY, cam_vpp, cfg_cam_vpp_key, STL_MAX_VPP_CAM_NUM, KEY_TYPE_OBJECT),
	MAKE_END_INFO()};

static key_info_t cfg_box_vpp_key[] = {
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_STRING, stream, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, decode_type, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, decode_width, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, decode_height, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, decode_frame_rate, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, encode_type, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, encode_width, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, encode_height, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, encode_frame_rate, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_S32, encode_bitrate, NULL),
	MAKE_KEY_INFO(solution_cfg_box_vpp_t, KEY_TYPE_STRING, model, NULL),
	MAKE_END_INFO()};

static key_info_t cfg_box_key[] = {
	MAKE_KEY_INFO(solution_cfg_box_t, KEY_TYPE_S32, pipeline_count, NULL),
	MAKE_KEY_INFO(solution_cfg_cam_t, KEY_TYPE_S32, max_pipeline_count, NULL),
	MAKE_ARRAY_INFO(solution_cfg_box_t, KEY_TYPE_ARRAY, box_vpp, cfg_box_vpp_key, STL_MAX_VPP_BOX_NUM, KEY_TYPE_OBJECT),
	MAKE_END_INFO()};

static key_info_t solution_cfg_key[] = {
	MAKE_KEY_INFO(solution_cfg_t, KEY_TYPE_STRING, version, NULL),
	MAKE_KEY_INFO(solution_cfg_t, KEY_TYPE_OBJECT, hardware_capability, hard_capability_key),
	MAKE_KEY_INFO(solution_cfg_t, KEY_TYPE_STRING, solution_name, NULL),
	MAKE_KEY_INFO(solution_cfg_t, KEY_TYPE_OBJECT, cam_solution, cfg_cam_key),
	MAKE_KEY_INFO(solution_cfg_t, KEY_TYPE_OBJECT, box_solution, cfg_box_key),
	MAKE_KEY_INFO(solution_cfg_t, KEY_TYPE_STRING, display_dev, NULL),
	MAKE_END_INFO()};

// 打印 solution_cfg_t 结构体的所有值
void print_solution_cfg(const solution_cfg_t *config)
{
	printf("Version: %s\n", config->version);
	printf("Hardware Capability:\n");
	printf("  Chip Type: %s\n", config->hardware_capability.chip_type);

	printf("  Sensor List: \n");
	const csi_list_info_t *csi_list_info = &config->hardware_capability.csi_list_info;
	for(int i = 0; i< csi_list_info->max_count; i++){
		printf("  	CSI_%d: [%s]", csi_list_info->csi_info[i].index,
			csi_list_info->csi_info[i].sensor_config_list);
	}
	printf("\n");

	printf("  MclkIsNotConfiged Status: \n");
	for(int i = 0; i< csi_list_info->max_count; i++){
		printf("  	CSI_%d: [%d]", csi_list_info->csi_info[i].index,
			csi_list_info->csi_info[i].mclk_is_not_configed);
	}
	printf("\n");

	printf("  Model List: %s\n", config->hardware_capability.model_list);
	printf("  Codec Type List: %s\n", config->hardware_capability.codec_type_list);
	printf("  Encode Bit Rate List: ");
	for (int i = 0; i < 16; i++)
	{
		printf("%d ", config->hardware_capability.encode_bit_rate_list[i]);
	}
	printf("\n");
	printf("  Display Device List: %s\n", config->hardware_capability.display_dev_list);
	printf("Solution Name: %s\n", config->solution_name);
	printf("Camera Solution:\n");
	printf("  Pipeline Count: %d\n", config->cam_solution.pipeline_count);
	printf("  Max Pipeline Count: %d\n", config->cam_solution.max_pipeline_count);
	for (int i = 0; i < config->cam_solution.max_pipeline_count; i++)
	{
		printf("  Camera VPP %d:\n", i + 1);
		printf("    Valid: %d\n", config->cam_solution.cam_vpp[i].is_valid);
		printf("    Enable: %d\n", config->cam_solution.cam_vpp[i].is_enable);
		printf("    CSI: %d\n", config->cam_solution.cam_vpp[i].csi_index);
		printf("    Sensor: %s\n", config->cam_solution.cam_vpp[i].sensor);
		printf("    Encode Type: %d\n", config->cam_solution.cam_vpp[i].encode_type);
		printf("    Encode Bitrate: %d\n", config->cam_solution.cam_vpp[i].encode_bitrate);
		printf("    Model: %s\n", config->cam_solution.cam_vpp[i].model);
		printf("    Gdb Status: %d\n", config->cam_solution.cam_vpp[i].gdc_status);
		printf("    MclkIsNotConfiged Status: %d\n", config->cam_solution.cam_vpp[i].mclk_is_not_configed);
	}
	printf("Box Solution:\n");
	printf("  Pipeline Count: %d\n", config->box_solution.pipeline_count);
	printf("  Max Pipeline Count: %d\n", config->box_solution.max_pipeline_count);
	for (int i = 0; i < config->box_solution.pipeline_count; i++)
	{
		printf("  Box VPP %d:\n", i + 1);
		printf("    Stream: %s\n", config->box_solution.box_vpp[i].stream);
		printf("    Decode Type: %d\n", config->box_solution.box_vpp[i].decode_type);
		printf("    Decode Width: %d\n", config->box_solution.box_vpp[i].decode_width);
		printf("    Decode Height: %d\n", config->box_solution.box_vpp[i].decode_height);
		printf("    Decode Frame Rate: %d\n", config->box_solution.box_vpp[i].decode_frame_rate);
		printf("    Encode Type: %d\n", config->box_solution.box_vpp[i].encode_type);
		printf("    Encode Width: %d\n", config->box_solution.box_vpp[i].encode_width);
		printf("    Encode Height: %d\n", config->box_solution.box_vpp[i].encode_height);
		printf("    Encode Frame Rate: %d\n", config->box_solution.box_vpp[i].encode_frame_rate);
		printf("    Encode Bitrate: %d\n", config->box_solution.box_vpp[i].encode_bitrate);
		printf("    Model: %s\n", config->box_solution.box_vpp[i].model);
	}
	printf("Display Device: %s\n", config->display_dev);
}

static cJSON *open_json_file(char *filename)
{
	FILE *f;
	long len;
	char *data;
	cJSON *json;

	f = fopen(filename, "rb");
	if (NULL == f)
		return NULL;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = (char *)malloc(len + 1);
	fread(data, 1, len, f);
	fclose(f);

	data[len] = '\0';
	json = cJSON_Parse(data);
	if (!json)
	{
		printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		free(data);
		return NULL;
	}

	free(data);
	return json;
}

static int32_t write_json_file(char *filename, char *out)
{
	FILE *fp = NULL;

	fp = fopen(filename, "a+");
	if (fp == NULL)
	{
		fprintf(stderr, "open file failed\n");
		return -1;
	}
	fprintf(fp, "%s", out);

	if (fp != NULL)
		fclose(fp);

	return 0;
}
//不包含 "\0"
static int get_first_camera_name_from_camera_list(const char *sensor_list, size_t size){
	int ret = -1;

	for(int i = 0; i< size; i++){
		if(sensor_list[i] == '/'){
			if(i > 0){
				ret = i;
			}
			break;
		}else if(sensor_list[i] == '\0'){
			if(i > 0){
				ret = i;
			}
			break;
		}
	}
	return ret;
}
int32_t solution_cfg_update_camera_config(){
	const csi_list_info_t *csi_list_info = &g_solution_config.hardware_capability.csi_list_info;

	g_solution_config.cam_solution.pipeline_count = csi_list_info->valid_count;
	g_solution_config.cam_solution.max_pipeline_count = STL_MAX_VPP_CAM_NUM;

	for(int i = 0; i < csi_list_info->max_count; i++){
		solution_cfg_cam_vpp_t* cam_vpp = &g_solution_config.cam_solution.cam_vpp[i];
		const char *sensor_list_str = csi_list_info->csi_info[i].sensor_config_list;
		//mclk 是否配置与系统有关，每次启动 不管摄像头是否接入，都必须更新
		cam_vpp->mclk_is_not_configed = csi_list_info->csi_info[i].mclk_is_not_configed;

		//本次启动: CSI_${i} 没有接摄像头
		if(!csi_list_info->csi_info[i].is_valid){
			if(cam_vpp->is_valid == 1){
				SC_LOGI("[%d] camera vpp config is [%s], but no camera connect, so remove it.",
					i, cam_vpp->sensor);
				cam_vpp->is_valid = 0;
				cam_vpp->is_enable = 0;
				cam_vpp->csi_index = i;
				cam_vpp->encode_type = 0;
				cam_vpp->encode_bitrate = 0;
				cam_vpp->gdc_status = GDC_STATUS_INVALID;
				memset(cam_vpp->sensor, 0, sizeof(cam_vpp->sensor));
				memset(cam_vpp->model, 0, sizeof(cam_vpp->model));
			}
			continue;
		}

		//本次启动: CSI_${i} 接了摄像头
		bool cam_vpp_config_is_valid = false;
		if(cam_vpp->is_valid == 1){ //配置文件中有摄像头
			char* result = strstr(sensor_list_str, cam_vpp->sensor);
			if(result == NULL){
				SC_LOGI("[%d] camera vpp config is [%s], but new camera [%s] so update it.",
					i, cam_vpp->sensor, sensor_list_str);
				cam_vpp_config_is_valid = false;
			}else{
				SC_LOGI("[%d] camera vpp config is [%s], and camera [%s] is not change, so use file config.",
					i, cam_vpp->sensor, sensor_list_str);
				cam_vpp_config_is_valid = true;
			}
		}else{ //配置文件中没有摄像头
			cam_vpp_config_is_valid = false;
			SC_LOGI("[%d] camera vpp config is null, but new connect camera [%s], so add it.", i, sensor_list_str);
		}

		//本次启动接入的摄像头 == 配置文件中对应的摄像头一致
		if(cam_vpp_config_is_valid){
			const char *gdc_bin_file = vp_gdc_get_bin_file(cam_vpp->sensor);
			if((gdc_bin_file != NULL) && (cam_vpp->gdc_status == GDC_STATUS_INVALID)){
				SC_LOGI("[%d] camera vpp config update gdc status, because new gdc file is added {%s}", i, gdc_bin_file);
				cam_vpp->gdc_status = GDC_STATUS_CLOSE;
			}else if((gdc_bin_file == NULL) && (cam_vpp->gdc_status != GDC_STATUS_INVALID)){
				SC_LOGW("[%d] camera vpp config update gdc status, because new gdc file is removed {%s}", i, gdc_bin_file);
				cam_vpp->gdc_status = GDC_STATUS_INVALID;
			}else{
				//保持不变
			}
			continue;
		}

		size_t sensor_config_list_size = sizeof(csi_list_info->csi_info[i].sensor_config_list);
		int first_camera_end_index = get_first_camera_name_from_camera_list(sensor_list_str, sensor_config_list_size);
		if(first_camera_end_index == -1){
			SC_LOGW("parse sensor list failed :%s, so exit.", sensor_list_str);
			exit(-1);
		}

		strncpy(cam_vpp->sensor, sensor_list_str, first_camera_end_index);
		cam_vpp->sensor[first_camera_end_index] = '\0';

		const char *gdc_bin_file = vp_gdc_get_bin_file(cam_vpp->sensor);
		if(gdc_bin_file == NULL){
			cam_vpp->gdc_status = GDC_STATUS_INVALID;
		}else {
			cam_vpp->gdc_status = GDC_STATUS_CLOSE;
		}

		cam_vpp->is_valid = 1;
		cam_vpp->is_enable = 0; //新增相机默认关闭
		cam_vpp->csi_index = csi_list_info->csi_info[i].index;
		cam_vpp->encode_type = 0;
		cam_vpp->encode_bitrate = 8192;
		strcpy(cam_vpp->model, "null");
	}

	return 0;
}
int32_t solution_cfg_load_default_config()
{
	//只清除静态的配置(运行时获取的参数比如能力列表 不清除)
	memset(&g_solution_config.solution_name, 0, sizeof(g_solution_config.solution_name));
	memset(&g_solution_config.cam_solution, 0, sizeof(g_solution_config.cam_solution));
	memset(&g_solution_config.box_solution, 0, sizeof(g_solution_config.box_solution));
	memset(&g_solution_config.display_dev, 0, sizeof(g_solution_config.display_dev));
	memset(&g_solution_config.hardware_capability.model_list, 0, sizeof(g_solution_config.hardware_capability.model_list));

	strcpy(g_solution_config.hardware_capability.codec_type_list, "H264/H265");
	strcpy(g_solution_config.hardware_capability.gdc_status_list, "close/open");
	// strcpy(g_solution_config.hardware_capability.codec_type_list, "H264/H265/Mjpeg");

	// 初始化编码码率列表
	// 标清视频（480p） 256, 512, 768, 1024, 1536, 2048,
	// 高清视频（720p） 512, 1024, 2048, 3072, 4096, 6144,
	// 全高清视频（1080p） 1024, 2048, 4096, 6144, 8192, 12288,
	// 2K视频 2048, 4096, 8192, 12288, 16384, 24576,
	// 4K视频  4096, 8192, 16384, 24576, 32768, 49152
	int32_t encode_bitrate_options[] = {
		256, 512, 768, 1024, 1536, 2048,
		3072, 4096, 6144, 8192, 12288,
		16384, 24576, 32768, 49152
	};
	int num_options = sizeof(encode_bitrate_options) / sizeof(encode_bitrate_options[0]);
	for (int i = 0; i < num_options && i < 16; i++)
	{
		g_solution_config.hardware_capability.encode_bit_rate_list[i] = encode_bitrate_options[i];
	}
	strcpy(g_solution_config.hardware_capability.display_dev_list, "hdmi");

	// 默认应用方案
	strcpy(g_solution_config.solution_name, "box_solution");

	// camera solution
	//没接摄像头的情况下，for 循环内部不会执行，pipeline_count = 0， 可以保证 web 页面时空的。
	memset(&g_solution_config.cam_solution, 0, sizeof(g_solution_config.cam_solution));
	const csi_list_info_t *csi_list_info = &g_solution_config.hardware_capability.csi_list_info;
	g_solution_config.cam_solution.pipeline_count = csi_list_info->valid_count;
	g_solution_config.cam_solution.max_pipeline_count = STL_MAX_VPP_CAM_NUM;
	for(int i = 0; i < csi_list_info->max_count; i++){
		solution_cfg_cam_vpp_t* cam_vpp = &g_solution_config.cam_solution.cam_vpp[i];
		cam_vpp->mclk_is_not_configed = csi_list_info->csi_info[i].mclk_is_not_configed;

		if(csi_list_info->csi_info[i].is_valid){ //接入了摄像头
			size_t sensor_config_list_size = sizeof(csi_list_info->csi_info[i].sensor_config_list);
			const char *sensor_list_str = csi_list_info->csi_info[i].sensor_config_list;
			int first_camera_end_index = get_first_camera_name_from_camera_list(sensor_list_str, sensor_config_list_size);
			if(first_camera_end_index == -1){
				SC_LOGW("parse sensor list failed :%s, so exit.", sensor_list_str);
				exit(-1);
			}

			strncpy(cam_vpp->sensor, sensor_list_str, first_camera_end_index);
			cam_vpp->sensor[first_camera_end_index] = '\0';
			const char *gdc_bin_file = vp_gdc_get_bin_file(cam_vpp->sensor);
			if(gdc_bin_file == NULL){
				cam_vpp->gdc_status = GDC_STATUS_INVALID;
			}else{
				cam_vpp->gdc_status = GDC_STATUS_CLOSE;
			}

			cam_vpp->is_valid = 1;
			//开机只打开一路相机
			if(i == 0){
				cam_vpp->is_enable = 1;
				strcpy(cam_vpp->model, "yolov5s");
			}else{
				cam_vpp->is_enable = 0;
				strcpy(cam_vpp->model, "null");
			}
			cam_vpp->csi_index = csi_list_info->csi_info[i].index;

			cam_vpp->encode_type = 0;
			cam_vpp->encode_bitrate = 8192;
		}else{ //没有接摄像头
			cam_vpp->is_valid = 0;
			cam_vpp->is_enable = 0;
			cam_vpp->csi_index = i;
			cam_vpp->gdc_status = GDC_STATUS_INVALID;
			cam_vpp->encode_type = 0;
			cam_vpp->encode_bitrate = 0;
			memset(cam_vpp->sensor, 0, sizeof(cam_vpp->sensor));
			memset(cam_vpp->model, 0, sizeof(cam_vpp->model));
		}
	}

	// video box
	g_solution_config.box_solution.pipeline_count = 1;
	g_solution_config.box_solution.max_pipeline_count = STL_MAX_VPP_BOX_NUM;
	strcpy(g_solution_config.box_solution.box_vpp[0].stream, "../test_data/1080P_test.h264");
	g_solution_config.box_solution.box_vpp[0].decode_type = 0;
	g_solution_config.box_solution.box_vpp[0].decode_width = 1920;
	g_solution_config.box_solution.box_vpp[0].decode_height = 1080;
	g_solution_config.box_solution.box_vpp[0].decode_frame_rate = 30;
	g_solution_config.box_solution.box_vpp[0].encode_type = 0;
	g_solution_config.box_solution.box_vpp[0].encode_width = 1920;
	g_solution_config.box_solution.box_vpp[0].encode_height = 1080;
	g_solution_config.box_solution.box_vpp[0].encode_frame_rate = 30;
	g_solution_config.box_solution.box_vpp[0].encode_bitrate = 8192;
	strcpy(g_solution_config.box_solution.box_vpp[0].model, "yolov5s");

	strcpy(g_solution_config.display_dev, "hdmi");

	bpu_wrap_get_model_list(g_solution_config.hardware_capability.model_list);

	// 保存到配置文件中
	return solution_cfg_save();
}

int32_t solution_cfg_load()
{
	if (g_solution_cfg_is_load == 1)
	{
		return 0;
	}
	sprintf(g_solution_config.version,
		"sunrise camera version: v3.1.0, build time:%s %s", __DATE__, __TIME__);
	// 获取芯片型号、接入的sensor型号、支持的算法模型清单
	vp_get_hard_capability(&g_solution_config);

	if (is_file_exist(SOLUTION_CONFIG_FILE) != 0)
	{
		printf("config file %s not exist\n", SOLUTION_CONFIG_FILE);
		// 使用默认配置
		solution_cfg_load_default_config();
		print_solution_cfg(&g_solution_config);
	}
	else
	{
		FILE *fd = fopen(SOLUTION_CONFIG_FILE, "r");
		if (fd != NULL)
		{
			fseek(fd, 0, SEEK_END);
			long file_size = ftell(fd);
			fseek(fd, 0, SEEK_SET);

			char *str_json = (char *)malloc(file_size + 1); // 动态分配内存
			if (str_json != NULL)
			{
				fread(str_json, 1, file_size, fd); // 读取文件内容到内存
				str_json[file_size] = '\0'; // 添加字符串结束符
				fclose(fd);

				SC_LOGI("read config from config file: [%s]\n", str_json);
				csi_list_info_t csi_info_tmp = g_solution_config.hardware_capability.csi_list_info;
				if (cjson_string2object(solution_cfg_key, str_json, &g_solution_config) == NULL)
				{
					g_solution_config.hardware_capability.csi_list_info = csi_info_tmp;
					SC_LOGW("config file parser failed, so use default config .");
					solution_cfg_load_default_config();
				}else{
					/**
					 * 上次启动保存了一些配置到，但是有些信息，再一次启动时会变化, 如果不 重新更新可能会导致程序执行错误
					 * 1. 新插入了摄像头
					 * 2. gdc 配置文件 进行了更新
					*/

					SC_LOGI("update camera info.");
					g_solution_config.hardware_capability.csi_list_info = csi_info_tmp;
					solution_cfg_update_camera_config();
				}
				print_solution_cfg(&g_solution_config);

				free(str_json); // 释放内存
			}
			else
			{
				solution_cfg_load_default_config();
			}
		}
		else
		{
			// 文件打开失败，使用默认配置
			solution_cfg_load_default_config();
		}
	}

	g_solution_cfg_is_load = 1;

	return 0;
}

int32_t solution_cfg_save()
{
	if (is_dir_exist(SOLUTION_CONFIG_PATH) != 0)
	{
		if (mkdir(SOLUTION_CONFIG_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
		{
			printf("mkdir %s error", SOLUTION_CONFIG_PATH);
			return -1;
		}
	}

	char *out = cjson_object2string(solution_cfg_key, (void *)&g_solution_config);
	FILE *fd = fopen(SOLUTION_CONFIG_FILE, "w");
	if (fd != NULL)
	{
		fwrite(out, 1, strlen(out), fd);
		fclose(fd);
	}
	free(out);

	return 0;
}

char *solution_cfg_obj2string()
{
	return cjson_object2string(solution_cfg_key, (void *)&g_solution_config);
}

void solution_cfg_string2obj(char *in)
{
	cjson_string2object(solution_cfg_key, in, &g_solution_config);
}
