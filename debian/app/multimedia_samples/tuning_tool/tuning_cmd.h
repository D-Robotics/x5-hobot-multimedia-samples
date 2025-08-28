/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __TUNING_CMD_H__
#define __TUNING_CMD_H__

#include "tuning_tool.h"

extern tuning_context_t *global_ctx;
extern int32_t lut3d_map[LUT_SIZE][LUT_SIZE][LUT_SIZE][3];

typedef struct tuning_cmd_func {
	char cmd;
	void (*api_func)(tuning_context_t *ctx);
} tuning_cmd_func_t;

/* app cmd */
#define PARSE_SHORT_OPTS "s:t:m:w:f:l:d:r:H:W:F:h:a:"
#define PARSE_LONG_OPTS {\
		{"sensor_index", required_argument, 0, 's'},\
		{"settle_value", optional_argument, 0, 't'},\
		{"sensor_mode", optional_argument, 0, 'm'},\
		{"send_raw", required_argument, 0, 'r'},\
		{"dump_stream", required_argument, 0, 'l'},\
		{"online", no_argument, 0, 0},\
		{"offline", no_argument, 0, 0},\
		{"mcm", no_argument, 0, 0},\
		{"work_mode", required_argument, 0, 'w'},\
		{"feedback_times", required_argument, 0, 'f'},\
		{"hight", required_argument, 0, 'H'},\
		{"width", required_argument, 0, 'W'},\
		{"format", required_argument, 0, 'F'},\
		{"lut3d", required_argument, 0, 'a'},\
		{"help", no_argument, 0, 'h'},\
		{ NULL, 0, 0, 0 },\
	}


#define PARSE_SHOW_OPTS "-s        Specify sensor index\n"\
			"-t        Specify settle time for debug\n"\
			"-m        Specify sensor mode of camera_config_t\n"\
			"-r        send raw to hbplayer\n"\
			"-l        dump stream flag\n"\
			"-w        work mode mask\n"\
			"-a        run with opencl for 3dlut func\n"\
			"-f -H -W -F       feedback raw file xx with specified height, width, and format(raw8/raw10/raw12)\n"\
			"-h        usage help\n"

#define parse_opts_print(prog) do {\
		pr_tuning("Usage: %s\n", prog);\
		printf(PARSE_SHOW_OPTS);\
	} while(0)

/* api cmd */
#define VALID_CMD_USAGE "s -> dump frame sif raw\n"\
			"y -> dump yuv\n"\
			"a -> dump raw and yuv\n"\
			"e -> set ae attr\n"\
			"E -> get ae attr\n"\
			"r -> set ae roi\n"\
			"R -> get ae roi\n"\
			"b -> get ae statistics\n"\
			"f -> get af statistics\n"\
			"w -> set awb attr\n"\
			"W -> get awb attr\n"\
			"t -> set exp table\n"\
			"T -> get exp table\n"\
			"m -> set module control\n"\
			"M -> get module control\n"\
			"d -> set 2dnr attr\n"\
			"D -> get 2dnr attr\n"\
			"n -> set 3dnr attr\n"\
			"N -> get 3dnr attr\n"\
			"z -> set ae zone weight\n"\
			"Z -> get ae zone weight\n"\
			"p -> set awb preference attr\n"\
			"P -> get awb preference attr\n"\
			"c -> set dpcc attr\n"\
			"C -> get dpcc attr\n"\
			"g -> set bayer pattern attr\n"\
			"i -> handle with 3dlut\n"\
			"q -> quit\n"\
			"h -> help\n"

#define valid_cmd_print() do {\
		pr_tuning("Support list:\n");\
		printf(VALID_CMD_USAGE);\
	} while(0)

#define TUNING_CMD_FUNC_LIST {\
	{'s',	tuning_dump_sif_raw},\
	{'e',	tuning_handle_set_expsoure},\
	{'E',	tuning_handle_get_expsoure},\
	{'r',	tuning_hanle_set_exp_roi},\
	{'R',	tuning_hanle_get_exp_roi},\
	{'w',	tuning_handle_set_white_balance},\
	{'W',	tuning_handle_get_white_balance},\
	{'t',	tuning_hanle_set_ae_table},\
	{'T',	tuning_hanle_get_ae_table},\
	{'y',	tuning_dump_yuv},\
	{'a',	tuning_dump_raw_and_yuv},\
	{'b',	tuning_get_ae_statistics},\
	{'f',	tuning_get_af_statistics},\
	{'m',	tuning_set_module_control},\
	{'M',	tuning_get_module_control},\
	{'d',	tuning_hanle_set_2dnr_attr},\
	{'D',	tuning_hanle_get_2dnr_attr},\
	{'n',	tuning_hanle_set_3dnr_attr},\
	{'N',	tuning_hanle_get_3dnr_attr},\
	{'z',	tuning_hanle_set_ae_zone_weight},\
	{'Z',	tuning_hanle_get_ae_zone_weight},\
	{'p',	tuning_hanle_set_awb_preference_attr},\
	{'P',	tuning_hanle_get_awb_preference_attr},\
	{'c',	tuning_handle_set_dpcc_attr},\
	{'C',	tuning_handle_get_dpcc_attr},\
	{'g',	tuning_handle_set_pattern_attr},\
	{'i',	tuning_handle_3dlut},\
}

void tuning_dump_sif_raw(tuning_context_t *ctx);
void tuning_handle_set_expsoure(tuning_context_t *ctx);
void tuning_handle_get_expsoure(tuning_context_t *ctx);
void tuning_handle_set_white_balance(tuning_context_t *ctx);
void tuning_handle_get_white_balance(tuning_context_t *ctx);
void tuning_hanle_set_ae_table(tuning_context_t *ctx);
void tuning_hanle_get_ae_table(tuning_context_t *ctx);
void tuning_hanle_set_exp_roi(tuning_context_t *ctx);
void tuning_hanle_get_exp_roi(tuning_context_t *ctx);
void tuning_dump_yuv(tuning_context_t *ctx);
void tuning_dump_raw_and_yuv(tuning_context_t *ctx);
void tuning_get_ae_statistics(tuning_context_t *ctx);
void tuning_set_module_control(tuning_context_t *ctx);
void tuning_get_module_control(tuning_context_t *ctx);
void tuning_get_af_statistics(tuning_context_t *ctx);
void tuning_hanle_set_2dnr_attr(tuning_context_t *ctx);
void tuning_hanle_get_2dnr_attr(tuning_context_t *ctx);
void tuning_hanle_set_3dnr_attr(tuning_context_t *ctx);
void tuning_hanle_get_3dnr_attr(tuning_context_t *ctx);
void tuning_hanle_set_ae_zone_weight(tuning_context_t *ctx);
void tuning_hanle_get_ae_zone_weight(tuning_context_t *ctx);
void tuning_hanle_set_awb_preference_attr(tuning_context_t *ctx);
void tuning_hanle_get_awb_preference_attr(tuning_context_t *ctx);
void tuning_handle_set_dpcc_attr(tuning_context_t *ctx);
void tuning_handle_get_dpcc_attr(tuning_context_t *ctx);
void tuning_handle_set_pattern_attr(tuning_context_t *ctx);
void tuning_handle_3dlut(tuning_context_t *ctx);


void tuning_time_point();
void tuning_time_delay(const char *func_name);

#define RECORD_START() do { \
		tuning_time_point(); \
	} while(0)

#define RECORD_END(func_name) do { \
		tuning_time_delay(func_name); \
	} while(0)

#define TUNING_API_EQ(func, pattr, retfunc) do { \
		int32_t func_ret; \
		hbn_vnode_handle_t isp_node_handle; \
		isp_node_handle = global_ctx->pipe_contex_info[global_ctx->handle_id].pipe_contex.isp_node_handle; \
		RECORD_START(); \
		func_ret = func(isp_node_handle, pattr); \
		RECORD_END(#func); \
		if ((func_ret) != 0) { \
			pr_tuning("Error[%d]: %s fail!\n", __LINE__, #func); \
			retfunc; \
		} \
	} while(0)

#endif // __TUNING_CMD_H__
