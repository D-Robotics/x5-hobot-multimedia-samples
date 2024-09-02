#ifndef __PARAM_PARSER_HH__
#define __PARAM_PARSER_HH__
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>

#include "vp_sensors.h"
#include "common_utils.h"

#define MAX_PIPE_NUM 4
typedef struct {
	int select_sensor_id;
	int sensor_mode;
	int active_mipi_host;
	int vse_bind_n2d_chn;

	vp_csi_config_t csi_config;
	vp_sensor_config_t* sensor_config;
}sensor_param_config_t;

typedef struct {
	char output[256];
	char *output_file_name;

	float blend_ratio;

	int gdc_enable;
	int verbose_flag;
	int sensor_config_count;
	sensor_param_config_t sensor_param_config[MAX_PIPE_NUM];
}param_config_t;

int param_process(int argc, char** argv, param_config_t* param_config);
int check_camera_config(param_config_t *param_config);
#endif
