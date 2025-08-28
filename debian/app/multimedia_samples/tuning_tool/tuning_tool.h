/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __TUNING_TOOL_H__
#define __TUNING_TOOL_H__

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#include "hbn_isp_api.h"
#include "isp_cfg.h"
#include "tuning_utils.h"
#include "common_utils.h"


#define RET_SUCCESS	0
#define RET_FAILURE	-1

#define LUT_SIZE 10
#define LUT_KNEE 150

#define DEF_DUMP_PATH	"/userdata"
#define MAX_SENSORS	4
#define HBPLAYER_EN	1
#define FEEDBACK_MASK	0
#define START_DUMP_MASK	1
#define LUT3D_MASK	2

typedef enum pipeline_mode_e {
	Online = 0,
	MCM = 1,
	Offline = 2,
} pipeline_mode_t;

typedef struct tuning_opencl_ctx_s {
	cl_platform_id platform;
	cl_device_id device;
	cl_context context;
	cl_command_queue queue;
	cl_program program;
	cl_kernel kernel;
	cl_mem input_image;
	cl_mem output_image;
	cl_mem lut_buffer;
} tuning_opencl_ctx_t;

typedef struct pipe_contex_info {
	pipe_contex_t pipe_contex;
	tuning_opencl_ctx_t opencl_ctx;
	uint32_t active_mipi_host;
	uint32_t select_sensor_id;
	uint32_t img_width;
	uint32_t img_height;
	uint32_t is_offline;
	uint32_t vin_format;
	uint32_t yuv_dump_cnt;
} pipe_contex_info_t;

typedef struct tuning_context {
	pipe_contex_info_t pipe_contex_info[MAX_SENSORS];
	uint32_t sensor_count;
	uint32_t dump_mask;
	uint32_t send_raw;
	uint32_t dump_stream_flag;
	uint32_t work_mode;
	uint32_t feedback_times;

	uint32_t handle_id;	
	pthread_t main_thid;
	pthread_t api_thid;
	tool_event_t *hbplayer_event;

	int32_t img_num;
	int32_t cur_img;
	char img_path[TUNING_FEEDBACK_FILE_MAX][128];
	char img_name[TUNING_FEEDBACK_FILE_MAX][128];
	hbn_vnode_image_t src_img;
} tuning_context_t;

#define select_id(ctx, pparam) do { \
	if (ctx->sensor_count > 1) { \
		printf("select the sensor id(0~%d) to dump:", ctx->sensor_count - 1); \
		scanf("%d", (pparam)); \
	} else { \
		*(pparam) = 0; \
	} \
} while(0)

#define main_while_func_run(func, ctx) {	\
	unsigned int cmd;			\
	while ((cmd = getchar()) != EOF)	\
	{					\
		if (cmd == '\n') continue;	\
		if (cmd == 'q') break;		\
		(func(cmd, (ctx)));		\
		for(;;) {			\
			if (getchar() == '\n')	\
				break;		\
		}				\
	}					\
	}

#endif // __TUNING_TOOL_H__