#include "param_parser.h"
static int is_number(const char *str) {
	while (*str) {
		if (!isdigit(*str)) return 0;
		str++;
	}
	return 1;
}

int check_camera_config(param_config_t *param_config){

	//VSE放大： 最大分辨率是4K，放大倍数最大是4倍
	int quarter_of_vse_max_resolution = 3840 *2160 / 4;
	for(int i = 0; i < param_config->sensor_config_count; i++){
		vp_sensor_config_t* sensor_config = param_config->sensor_param_config[i].sensor_config;
		int width_tmp = sensor_config->camera_config->width;
		int height_tmp = sensor_config->camera_config->height;
		char *camera_name_tmp = sensor_config->camera_config->name;

		if(width_tmp * height_tmp < quarter_of_vse_max_resolution){
			printf("camera %s width %d height %d is too small, after zooming in 4 times, the resolution cannot reach 4K.\n",
					camera_name_tmp, width_tmp, height_tmp);
			return -1;
		}
	}
	return 0;
}

static void print_help(void) {
	printf("Usage: %s [Options]\n", get_program_name());
	printf("Options:\n");
	printf("-c, --config=\"sensor=id \"\n");
	printf("\t\tConfigure parameters for each video pipeline, can be repeated up to %d times.\n", MAX_PIPE_NUM);
	printf("\t\tsensor   --  Sensor index,can have multiple parameters, reference sensor list.\n");
	printf("-o, --output=\"file or hdmi, default is file\n");
	printf("-r, --ratio=\"camera image width ratio, used to blend, default is 0.0\n");
	printf("-g, --gdc_enable\tEnable gdc, default is disable\n");
	printf("-v, --verbose\tEnable verbose mode\n");
	printf("-h, --help\tShow help message\n");

	printf("\n\n");

	printf("Support sensor list:\n");
	vp_show_sensors_list();

#if 1
	printf("\n\nExample:(only support 2 cameras .)\n");
	printf("Save File: ./multi_pipe_crop_and_stitch -c \"sensor=3\" -c \"sensor=3\"\n");
	printf("HDMI Display: ./multi_pipe_crop_and_stitch -c \"sensor=3\" -c \"sensor=3\" -o hdmi\n");
	printf("HDMI Display, Enable GDC: ./multi_pipe_crop_and_stitch -c \"sensor=3\" -c \"sensor=3\" -o hdmi -g\n");
	printf("HDMI Display, Enable GDC, Enable Blend: ./multi_pipe_crop_and_stitch -c \"sensor=3\" -c \"sensor=3\" -o hdmi -g -r 0.02\n");
#else
	printf("\n\nExample:(only support 2 cameras and 4 cameras)\n");
	printf("2 cameras:  ./multi_pipe_crop_and_stitch -c \"sensor=7\" -c \"sensor=3\"\n");
	printf("4 cameras:  ./multi_pipe_crop_and_stitch -c \"sensor=7\" -c \"sensor=3\" -c \"sensor=3\" -c \"sensor=3\"\n");
#endif
	printf("\n\n");

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

void parse_config(sensor_param_config_t *sensor_param_config, const char *config) {
	int ret = 0;
	char *parts[4];
	int sensor_idx = -1;
	int count = split_string(config, " ", parts, 4);

	static int32_t used_mipi_host = 0; //must is static
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
				sensor_param_config->sensor_config = vp_sensor_config_list[sensor_idx];
				printf("\n\nUsing index:%d  sensor_name:%s  config_file:%s\n",
						sensor_idx,
						vp_sensor_config_list[sensor_idx]->sensor_name,
						vp_sensor_config_list[sensor_idx]->config_file);
			} else {
				printf("Unsupport sensor index:%d\n", sensor_idx);
				print_help();
				exit(0);
			}
			ret = vp_sensor_multi_fixed_mipi_host(sensor_param_config->sensor_config, used_mipi_host,
				&sensor_param_config->csi_config);
			if (ret < 0) {
				printf("vp sensor fixed mipi host fail, sensor id %d."
					"Maybe No Camera Sensor found. Please check if the specified "
					"sensor is connected to the Camera interface.\n\n", sensor_idx);
				exit(0);
			}
			sensor_param_config->select_sensor_id = sensor_idx;
			// active_mipi_host 的配置在create_vin_node函数中需要再配置一下
			sensor_param_config->active_mipi_host = sensor_param_config->sensor_config->vin_node_attr->cim_attr.mipi_rx;
			used_mipi_host |= (1 << sensor_param_config->sensor_config->vin_node_attr->cim_attr.mipi_rx);

			printf("mipi host %d\n", sensor_param_config->active_mipi_host);

		} else if (strcmp(key_value[0], "channel") == 0) {
			if (!is_number(key_value[1])) {
				fprintf(stderr, "Invalid channel ID: %s\n", key_value[1]);
				continue;
			}
			sensor_param_config->vse_bind_n2d_chn = atoi(key_value[1]);
		} else if (strcmp(key_value[0], "mode") == 0) {
			if (!is_number(key_value[1])) {
				fprintf(stderr, "Invalid sensor mode number: %s\n", key_value[1]);
				continue;
			}
			sensor_param_config->sensor_mode = atoi(key_value[1]);
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

	printf("MIPI host: 0x%x\n", used_mipi_host);
	for (int i = 0; i < MAX_PIPE_NUM; i++) {
		if (used_mipi_host & (1 << i)) {
			printf("  Host %d: Used\n", i);
		}
	}
}

int param_process(int argc, char** argv, param_config_t* param_config){

	static struct option const long_options[] = {
		{"config", required_argument, NULL, 'c'},
		{"ratio", no_argument, NULL, 'r'},
		{"output", no_argument, NULL, 'o'},
		{"gdc_enable", no_argument, NULL, 'g'},
		{"verbose", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	//output default is file.
	strcpy(param_config->output, "file");
	param_config->output_file_name = "output.h265";
	param_config->blend_ratio = 0.0;
	param_config->gdc_enable = 0;

	int c = 0;
	int32_t total_pipeline_num = 0;
	while ((c = getopt_long(argc, argv, "c:r:o:gvh", long_options, NULL)) != -1) {
		switch (c) {
		case 'c':
			if (total_pipeline_num >= MAX_PIPE_NUM) {
				fprintf(stderr, "Too many configurations. Maximum allowed is %d.\n", MAX_PIPE_NUM);
				return -1;
			}

			parse_config(&param_config->sensor_param_config[total_pipeline_num], optarg);
			total_pipeline_num++;
			break;
		case 'r':
			float blend_ratio = atof(optarg);
			if((blend_ratio >= 0.0) && (blend_ratio <= 1.0)){
				param_config->blend_ratio = blend_ratio;
			}else{
				printf("input blend ratio is invalid [%s] => %f, so use 0.0\n", optarg, blend_ratio);
			}
			break;
		case 'o':
			if(strcmp("file", optarg) == 0){
				strcpy(param_config->output, "file");
			}else if(strcmp("hdmi", optarg) == 0){
				strcpy(param_config->output, "hdmi");
			}else{
				printf("output form only support [%s]: file and hdmi\n", param_config->output);
				return -1;
			}
		case 'v':
			param_config->verbose_flag = 1;
			break;
		case 'g':
			param_config->gdc_enable = 1;
			break;
		case 'h':
		default:
			print_help();
			return -1;
		}
	}
#if 1
	if(total_pipeline_num != 2){
	printf("\n[%s] only support 2 cameras as input, current input %d cameras.\n\n",
		argv[0], total_pipeline_num);
#else
	if((total_pipeline_num != 2) && (total_pipeline_num != 4)){
		printf("\n[%s] only support 2 or 4 cameras as input, current input %d cameras.\n\n",
		argv[0], total_pipeline_num);
#endif
		print_help();
		return -1;
	}

	param_config->sensor_config_count = total_pipeline_num;
	// 处理后的参数在这里可以使用
	printf("\n\n Show sensor info:\n");
	for (int i = 0; i < param_config->sensor_config_count; i++) {
		param_config->sensor_param_config[i].vse_bind_n2d_chn = 5; 					//vse resize to 4K

		printf("  Pipeline index %d:\n", i);
		printf("\tSensor index: %d\n", param_config->sensor_param_config[i].select_sensor_id);
		printf("\tSensor name: %s\n", param_config->sensor_param_config[i].sensor_config->sensor_name);
		printf("\tActive mipi host: %d\n", param_config->sensor_param_config[i].active_mipi_host);
		printf("\tVse Channel: %d\n", param_config->sensor_param_config[i].vse_bind_n2d_chn);
		printf("\tGDC Enable: %d\n", param_config->gdc_enable);
	}

	printf("\n\n Show output info:\n");
	printf("\t Output Form: %s\n", param_config->output);
	if(strcmp(param_config->output, "file") == 0){
		printf("\t Output filename:%s\n", param_config->output_file_name);
	}
	printf("\n\n");

	return 0;
}