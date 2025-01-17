#ifndef ___VP_PIPELINE__
#define ___VP_PIPELINE__
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "common_utils.h"
typedef struct vp_pipeline_info_s {
    int active_mipi_host;
    int vse_bind_index;
    int sensor_mode;
    camera_config_info_t camera_config_info;
}vp_pipeline_info_t;

int vp_create_and_start_pipeline(pipe_contex_t *pipe_contex, vp_pipeline_info_t* vp_pipeline_info);
int vp_destroy_and_stop_pipeline(pipe_contex_t *pipe_contex);
int vp_get_vse_channel(int input_width, int input_height, int output_width, int output_height);
#endif //