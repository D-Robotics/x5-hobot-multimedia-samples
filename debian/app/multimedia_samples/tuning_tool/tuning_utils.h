/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __TUNING_UTILS_H__
#define __TUNING_UTILS_H__

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "hbn_api.h"
#include "hb_tool_server.h"
#include "hb_camera_interface.h"

#define tuning_fmt(fmt) "[tuning_tool]: " fmt
#define tuning_pr_warp(p_func_, fmt, ...) do { p_func_(tuning_fmt(fmt), ##__VA_ARGS__); } while(0)
#define pr_tuning(fmt, ...) tuning_pr_warp(printf, fmt, ##__VA_ARGS__)
#define apr_tuning(fmt, ...) android_printLog(6, NULL, fmt, ##__VA_ARGS__)
#define FLOAT288INT(x) ((int)((x) * 256.0f))

#define pr_linear(param_name, level, type, val) do {\
		int32_t l; \
		printf("%s:", (param_name)); \
		for (l = 0; l < (level); l++) { \
			printf(type, val[l]); \
		} \
		printf("\n"); \
	} while(0)

#define pr_double(param_name, level, level2, type, val) do {\
		int32_t l, m; \
		printf("%s:\n", (param_name)); \
		for (l = 0; l < (level); l++) { \
			for (m = 0; m < (level2); m++) { \
				printf(type, val[l][m]); \
			} \
			printf("\n"); \
		} \
		printf("\n"); \
	} while(0)

#define pr_triple(param_name, level, level2, level3, type, val) do {\
		int32_t l, m, n; \
		printf("%s:\n", (param_name)); \
		for (l = 0; l < (level); l++) { \
			for (m = 0; m < (level2); m++) { \
				for (n = 0; n < (level3); n++) { \
					printf(type, val[l][m][n]); \
				} \
				printf("\n"); \
			} \
			printf("\n"); \
		} \
		printf("\n"); \
	} while(0)

#define BIT_ENABLE(val, shift) ((val) & (1 << (shift)))
#define bit_mask(val, shift) ((val) |= (1 << (shift)))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define TUNING_PRINT_SIZE_MAX		128
#define TUNING_FEEDBACK_FILE_MAX	1024

#define read_p(text, type, pparam) do {	\
		printf("%s", text); \
		scanf(type, (pparam)); \
	} while(0)

#define FUNC_EQ(func, val, retfunc) do { \
		if ((func) != (val)) { \
			pr_tuning("error: %s(%d)[%s ne %d]\n", __func__, __LINE__, #func, (val)); \
			retfunc; \
		} \
	} while(0)

#define FUNC_NE(func, val, retfunc) do { \
		if ((func) == (val)) { \
			pr_tuning("error: %s(%d)[%s eq %d]\n", __func__, __LINE__, #func, (val)); \
			retfunc; \
		} \
	} while(0)

#define DEF_MODULE_NAME { \
	"com", \
	"sif", \
	"isp", \
	"vse", \
	"gdc", \
	"codec", \
	"n2d", \
}

typedef enum df_nmi_e {
	COM_MNI,
	SIF_MNI,
	ISP_MNI,
	VSE_MNI,
	GDC_MNI,
	CODEC_MNI,
	N2D_MNI,
} df_nmi_t;

void tuning_get_filename(char *name, char *path, hbn_vnode_image_t *out_img, df_nmi_t mni);
int32_t tuning_send_raw_to_hbplayer(tool_event_t *event, const hbn_vnode_image_t *normal_buf,
				enum RAW_BIT format, int32_t pipe_id);
int32_t tuning_send_yuv_to_hbplayer(tool_event_t *event, const hbn_vnode_image_t *normal_buf, int32_t pipe_id);
int32_t tuning_dump_file(char *filename, hbn_vnode_image_t *out_img);
int32_t tuning_alloc_feedback_buffer(hb_mem_graphic_buf_t *buf, uint32_t width, uint32_t height, uint32_t cached);
int32_t tuning_free_feedback_buffer(hb_mem_graphic_buf_t *buf);
int32_t tuning_get_raw_list(char *path, char img_path[][128], char img_name[][128], int32_t *img_num);
void tuning_time_point();
void tuning_time_delay(const char *func_name);

#endif // __TUNING_UTILS_H__