/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __COMMMON_UTILS__
#define __COMMMON_UTILS__

#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "log.h"
#include "logging.h"
#include "hb_mem_mgr.h"
#include "hb_camera_interface.h"
#include "hb_tool_server.h"
#include "hbn_api.h"
#include "vin_cfg.h"
#include "isp_cfg.h"
#include "vse_cfg.h"
#include "n2d_cfg.h"
#include "gdc_cfg.h"
#include <pthread.h>


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#define MAX_VIO_FILE_NAME		128
#define TEST_SLEEP_TIME			500 //us
#define PYM_FULL_DATA_WATER_LINE	8 //Full frame done mark info from kernel
#define RAW_DUMP_PATH			"/userdata/raw_dump"
#define YUV_DUMP_PATH			"/userdata/yuv_dump"
#define RAW_DATA			1
#define YUV_DATA			0

#define MAX_VIO_TEST_PRINT_LENGTH	512
#define MAX_VIO_TEST_LARGE_PRINT_LENGTH	1024
#define CHN_MAX	24
#define MAX_HOR_BUF_NUM 4
#define VIO_ASSERT_FUNC_EQ(func, val, retfunc) do { \
		if ((func) != (val)) { \
			printf("error: %s(%d)[%s ne %d]\n", __func__, __LINE__, #func, (val)); \
			retfunc; \
		} \
	} while(0)

#define VIO_ASSERT_FUNC_NE(func, val, retfunc) do { \
		if ((func) == (val)) { \
			printf("error: %s(%d)[%s eq %d]\n", __func__, __LINE__, #func, (val)); \
			retfunc; \
		} \
	} while(0)

typedef enum df_module_name_index {
	SIF_MNI,
	ISP_MNI,
	VSE_MNI,
	GDC_MNI,
	CODEC_MNI,
	N2D_MNI,
} df_module_name_index_e;

typedef struct work_info_s {
	uint32_t pipe_id;
	int32_t priv_fd;
	pthread_t thid;
	int32_t running;
	int32_t work_count;
	time_t start;
	int32_t remaining_loop;
} work_info_t;

typedef enum cam_format {
	CAM_FMT_NULL,
	CAM_FMT_RAW8,
	CAM_FMT_RAW10,
	CAM_FMT_RAW12,
	CAM_FMT_RAW16,
	CAM_FMT_RAW24,
	CAM_FMT_YUYV,
	CAM_FMT_NV12,
	CAM_FMT_NV16,
	CAM_FMT_RGB888X,
} cam_format_e;

typedef struct mplane_buffers_s {
	void *start[2];
	size_t length[2];
	int32_t dmafd[2];
} m_buffers_t;

typedef struct uvc_camera_context_s {
	const char *src_pic_path;
	uint32_t fd;
	uint32_t dump_mask;
	int32_t video_id;
	int32_t run_time;
	int32_t loop_cnt;
	int32_t pic_height;
	int32_t pic_width;
	int32_t isp_info_mask;
	void *buffers[MAX_HOR_BUF_NUM];
	int32_t dmabuf[MAX_HOR_BUF_NUM];
	m_buffers_t mplane_buffers[MAX_HOR_BUF_NUM];
	m_buffers_t output_buffers[MAX_HOR_BUF_NUM];
	cam_format_e pic_format;
	uint32_t error[CHN_MAX];
	work_info_t work_info;
} uvc_camera_context;

int dumpToFile(char *filename, char *srcBuf, unsigned int size);
int dumpToFile2plane(char *filename, char *srcBuf, char *srcBuf1,
			unsigned int size, unsigned int size1);
extern int32_t vpm_hb_mem_init(void);
extern void vpm_hb_mem_deinit(void);
int32_t runtime_end(uvc_camera_context *context);
void runtime_start(uvc_camera_context *context);
void filename_get(char *name, const char *path, hbn_vnode_image_t *out_img,
		  int32_t pipe_id, int32_t chn, int32_t raw, df_module_name_index_e mni);
const char *get_module_name(int32_t mni);

#ifdef __cplusplus
	}
#endif	/* __cplusplus */

#endif
