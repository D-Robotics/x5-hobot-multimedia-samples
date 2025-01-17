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
#include "synchronous_queue.h"

//#define PTS_ENABLE
#define GPU_ENABLE

#define SENSOR_OUTFILE_NAME_LEN  64
typedef struct {
	char type[16];
	int enable;
	FILE *fp;
	int chn;
	int queue_user_flag;
	int enable_save_file;
	char filename[SENSOR_OUTFILE_NAME_LEN];
	media_codec_context_t encode_context;
	pthread_t codec_thread;
	int is_running;
	sync_queue_t *data_queue;
#ifdef PTS_ENABLE
	uint64_t cur_timestamps;
	uint64_t last_timestamps;
#endif
}sensor_outfile_config_t;

#define MAX_PIPE_NUM 4
typedef struct {
	int select_sensor_id;
	int sensor_mode;
	int active_mipi_host;
	int vse_bind_n2d_chn;
	int gdc_enable;
	sensor_outfile_config_t h264_outfile;
	sensor_outfile_config_t mjpeg_outfile;
	vp_csi_config_t csi_config;
	vp_sensor_config_t* sensor_config;
}sensor_param_config_t;

typedef struct {
	char mode_name[256];
	char output[256];
	char *output_file_name;

	float blend_ratio;

	int bpu_fps;
	int bpu_enable;
	int bpu_postporcess_enable;

	int gpu_enable;	   // For stitch image, If there is no HDMI or file output, the option disable by default

//	int gdc_enable;
	int verbose_flag;
	int sensor_config_count;
	sensor_param_config_t sensor_param_config[MAX_PIPE_NUM];
}param_config_t;

int param_process(int argc, char** argv, param_config_t* param_config);
int check_camera_config(param_config_t *param_config,
	int *pipe_contex_need_vse, int* enable_isp_online);
#endif
