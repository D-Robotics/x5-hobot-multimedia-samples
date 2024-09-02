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
	int enable_gdc;
	char *sensor_name;
    camera_config_info_t camera_config_info;
}vp_pipeline_info_t;

int vp_create_and_start_pipeline(pipe_contex_t *pipe_contex, vp_pipeline_info_t* vp_pipeline_info);
int vp_destroy_and_stop_pipeline(pipe_contex_t *pipe_contex);
int vp_get_vse_channel(int input_width, int input_height, int output_width, int output_height);

const char *vp_gdc_get_bin_file(const char *sensor_name);

typedef struct vp_vse_feedback_pipeline_info_s {
	int input_width;
	int input_height;
	int output_width;
	int output_height;
	int vse_channel;
}vp_vse_feedback_pipeline_info_t;
int vp_create_stop_vse_feedback_pieline(pipe_contex_t *pipe_contex);
int vp_create_start_vse_feedback_pieline(pipe_contex_t *pipe_contex, vp_vse_feedback_pipeline_info_t *pipeline_info);
int vp_send_to_vse_feedback(pipe_contex_t *pipe_contex, int vse_channel, hbn_vnode_image_t* src);
int vp_get_from_vse_feedback(pipe_contex_t *pipe_contex, int vse_channel, hbn_vnode_image_t* src);
int vp_release_vse_feedback(pipe_contex_t *pipe_contex, int vse_channel, hbn_vnode_image_t* src);

#endif //