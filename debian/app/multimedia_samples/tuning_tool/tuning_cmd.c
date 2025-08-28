/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include "tuning_cmd.h"
#include <time.h>
#include <math.h>

void tuning_dump_sif_raw(tuning_context_t *ctx)
{
	int32_t i, ret;
	uint32_t dump_cnt = 0;
	hbn_vnode_image_t raw_img = {0};
	char file_name[128] = {0};
	static int32_t raw_stream_cnt = 0;
	pipe_contex_info_t *pipe_info;

	pipe_info = &ctx->pipe_contex_info[ctx->handle_id];
	if (!pipe_info->is_offline) {
		pr_tuning("Cannot dump raw when sif otf isp\n");
		return ;
	}
	if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
		pr_tuning("Can not dump raw in feedback mode\n");
		return ;
	}

	read_p("Typing the number to dump: ", "%d", &dump_cnt);
	for (i = 0; i < dump_cnt; i++) {
		ret = hbn_vnode_getframe_cond(pipe_info->pipe_contex.vin_node_handle, 0, 1000, 0, &raw_img);
		if (ret) {
			pr_tuning("get buffer from sif fail\n");
			break;
		}

		snprintf(file_name, TUNING_PRINT_SIZE_MAX, "%s/SIF_S%d_STREAM%d.raw", DEF_DUMP_PATH, ctx->handle_id, raw_stream_cnt);
		tuning_dump_file(file_name, &raw_img);
		hbn_vnode_releaseframe(pipe_info->pipe_contex.vin_node_handle, 0, &raw_img);
	}
	raw_stream_cnt++;
}

void tuning_dump_raw_and_yuv(tuning_context_t *ctx)
{
	int32_t i, ret;
	uint32_t dump_cnt = 0;
	hbn_vnode_image_t raw_img = {0};
	hbn_vnode_image_t yuv_img = {0};
	static int32_t raw_stream_cnt = 0;
	pipe_contex_info_t *pipe_info;

	// Structure to Store we want to dump
	typedef struct {
		char raw_filename[128];
		char yuv_filename[128];
		hbn_vnode_image_t raw_image;
		hbn_vnode_image_t yuv_image;
		hbn_isp_exposure_attr_t exp_attr;
	} dump_buffer_t;

	dump_buffer_t *buffers = NULL;

	pipe_info = &ctx->pipe_contex_info[ctx->handle_id];
	if (!pipe_info->is_offline) {
		pr_tuning("Cannot dump raw when sif otf isp\n");
		return ;
	}
	if (BIT_ENABLE(ctx->work_mode, FEEDBACK_MASK)) {
		pr_tuning("Can not dump raw in feedback mode\n");
		return ;
	}

	read_p("Typing the number to dump: ", "%d", &dump_cnt);
	if (dump_cnt == 0) {
		return;
	}

	// Allocate buffers
	buffers = malloc(dump_cnt * sizeof(dump_buffer_t));
	if (!buffers) {
		pr_tuning("Failed to allocate memory for dump buffers\n");
		return;
	}

	for (i = 0; i < dump_cnt; i++) {
		ret = hbn_vnode_getframe_cond(pipe_info->pipe_contex.vin_node_handle, 0, 1000, 0, &raw_img);
		if (ret) {
			pr_tuning("get buffer from sif fail\n");
			break;
		}
		ret = hbn_vnode_getframe_cond(pipe_info->pipe_contex.isp_node_handle, 0, 1000, 0, &yuv_img);
		if (ret) {
			pr_tuning("get buffer from sif fail\n");
			hbn_vnode_releaseframe(pipe_info->pipe_contex.vin_node_handle, 0, &raw_img);
			break;
		}

		// Get exposure attributes
		TUNING_API_EQ(hbn_isp_get_exposure_attr, &buffers[i].exp_attr, return);
		snprintf(buffers[i].raw_filename, sizeof(buffers[i].raw_filename),
				"%s/SIF_S%d_frameid_%d_ts_%ld.raw", DEF_DUMP_PATH, ctx->handle_id,
				raw_img.info.frame_id, raw_img.info.timestamps);
		snprintf(buffers[i].yuv_filename, sizeof(buffers[i].yuv_filename),
				"%s/ISP_S%d_frameid_%d_ts_%ld.yuv", DEF_DUMP_PATH, ctx->handle_id,
				yuv_img.info.frame_id, yuv_img.info.timestamps);

		memcpy(&buffers[i].raw_image, &raw_img, sizeof(hbn_vnode_image_t));
		memcpy(&buffers[i].yuv_image, &yuv_img, sizeof(hbn_vnode_image_t));

		hbn_vnode_releaseframe(pipe_info->pipe_contex.isp_node_handle, 0, &yuv_img);
		hbn_vnode_releaseframe(pipe_info->pipe_contex.vin_node_handle, 0, &raw_img);
	}
	raw_stream_cnt++;
	FILE *exp_fp = fopen("/userdata/AE_INFO.txt", "w");
	if (!exp_fp) {
		pr_tuning("Failed to open AE_INFO.txt for writing\n");
		free(buffers);
		return;
	}
	// dump all buffered data
	for (int j = 0; j < i; j++) {
		fprintf(exp_fp,
			"[AE_INFO Info] Frame %d:"
			"  exp_time: %.2f"
			"  again: %.2f"
			"  dgain: %.2f"
			"  ispgain: %.2f"
			"  ae_exp: %.2f"
			"  cur_lux: %u"
			"  frame_id: %u"
			"  timestamps: %lu\n",
			j,
			buffers[j].exp_attr.manual_attr.exp_time,
			buffers[j].exp_attr.manual_attr.again,
			buffers[j].exp_attr.manual_attr.dgain,
			buffers[j].exp_attr.manual_attr.ispgain,
			buffers[j].exp_attr.manual_attr.ae_exp,
			buffers[j].exp_attr.manual_attr.cur_lux,
			buffers[j].exp_attr.manual_attr.frame_id,
			buffers[j].exp_attr.manual_attr.timestamps);
		// Dump files
		tuning_dump_file(buffers[j].raw_filename, &buffers[j].raw_image);
		tuning_dump_file(buffers[j].yuv_filename, &buffers[j].yuv_image);
	}

	free(buffers);
	fclose(exp_fp);
}

void tuning_handle_set_expsoure(tuning_context_t *ctx)
{
	hbn_isp_exposure_attr_t exp_attr = {0};
	uint32_t exp_mode;

	read_p("typing the expsoure mode, manual(0)/auto(1): ", "%d", &exp_mode);

	if (exp_mode == 0) {
		exp_attr.mode = HBN_ISP_MODE_MANUAL;
		read_p("expsoure time(units: s): ", "%f", &exp_attr.manual_attr.exp_time);
		read_p("again: ", "%f", &exp_attr.manual_attr.again);
		read_p("dgain: ", "%f", &exp_attr.manual_attr.dgain);
		read_p("ispgain: ", "%f", &exp_attr.manual_attr.ispgain);
	} else if (exp_mode == 1) {
		TUNING_API_EQ(hbn_isp_get_exposure_attr, &exp_attr, return);
		exp_attr.mode = HBN_ISP_MODE_AUTO;
		read_p("speed_over: ", "%f", &exp_attr.auto_attr.speed_over);
		read_p("speed_under: ", "%f", &exp_attr.auto_attr.speed_under);
		read_p("dampover_gain: ", "%f", &exp_attr.auto_attr.dampover_gain);
		read_p("dampover_ratio: ", "%f", &exp_attr.auto_attr.dampover_ratio);
		read_p("dampunder_gain: ", "%f", &exp_attr.auto_attr.dampunder_gain);
		read_p("dampunder_ratio: ", "%f", &exp_attr.auto_attr.dampunder_ratio);
		read_p("tolerance: ", "%f", &exp_attr.auto_attr.tolerance);
		read_p("target: ", "%f", &exp_attr.auto_attr.target);
		read_p("flicker_freq: ", "%f", &exp_attr.auto_attr.flicker_freq);
		read_p("anti_flicker_status: ", "%d", &exp_attr.auto_attr.anti_flicker_status);
	} else {
		printf("Unknown mode: %d\n", exp_mode);
		return;
	}

	TUNING_API_EQ(hbn_isp_set_exposure_attr, &exp_attr, return);
}

void tuning_handle_get_expsoure(tuning_context_t *ctx)
{
	uint32_t lines_per_second;
	hbn_isp_exposure_attr_t exp_attr = {0};

	TUNING_API_EQ(hbn_isp_get_exposure_attr, &exp_attr, return);
	TUNING_API_EQ(hbn_isp_get_lines_persecond, &lines_per_second, return);

	printf("Currently AE is in %s mode", (exp_attr.mode == HBN_ISP_MODE_MANUAL)?"manual":"auto");
	if (exp_attr.mode == HBN_ISP_MODE_AUTO) {
		if (exp_attr.lock_state) {
			printf(", lock state\n");
		} else {
			printf(", running state\n");
		}
	} else {
		printf("\n");
	}

	printf("exp_time: %f\n", exp_attr.manual_attr.exp_time);
	printf("lines_per_second: %d\n", lines_per_second);
	printf("again: %f\n", exp_attr.manual_attr.again);
	printf("dgain: %f\n", exp_attr.manual_attr.dgain);
	printf("ispgain: %f\n", exp_attr.manual_attr.ispgain);
	printf("ae_exp: %f\n",exp_attr.manual_attr.ae_exp);
	printf("mode: %d\n", exp_attr.auto_attr.mode);

	printf("exp range %f~%f\n", exp_attr.auto_attr.exp_time_range.min, exp_attr.auto_attr.exp_time_range.max);
	printf("again range %f~%f\n", exp_attr.auto_attr.again_range.min, exp_attr.auto_attr.again_range.max);
	printf("dgain range %f~%f\n", exp_attr.auto_attr.dgain_range.min, exp_attr.auto_attr.dgain_range.max);
	printf("isp dgain range %f~%f\n", exp_attr.auto_attr.isp_dgain_range.min, exp_attr.auto_attr.isp_dgain_range.max);

	printf("speed_over %f\n", exp_attr.auto_attr.speed_over);
	printf("speed_under %f\n", exp_attr.auto_attr.speed_under);
	printf("dampover_gain %f\n", exp_attr.auto_attr.dampover_gain);
	printf("dampover_ratio %f\n", exp_attr.auto_attr.dampover_ratio);
	printf("dampunder_gain %f\n", exp_attr.auto_attr.dampunder_gain);
	printf("dampunder_ratio %f\n", exp_attr.auto_attr.dampunder_ratio);

	printf("tolerance %f\n", exp_attr.auto_attr.tolerance);
	printf("target %f\n", exp_attr.auto_attr.target);
	printf("flicker_freq %f\n", exp_attr.auto_attr.flicker_freq);
	printf("anti_flicker_status %d\n", exp_attr.auto_attr.anti_flicker_status);
}

void tuning_handle_set_white_balance(tuning_context_t *ctx)
{
	hbn_isp_awb_attr_t awb_attr = {0};
	uint32_t awb_mode;

	read_p("typing the awb mode, manual(0)/auto(1): ", "%d", &awb_mode);

	if (awb_mode == 0) {
		awb_attr.mode = HBN_ISP_MODE_MANUAL;

		read_p("rgain: ", "%f", &awb_attr.manual_attr.gain.rgain);
		read_p("grgain: ", "%f", &awb_attr.manual_attr.gain.grgain);
		read_p("gbgain: ", "%f", &awb_attr.manual_attr.gain.gbgain);
		read_p("bgain: ", "%f", &awb_attr.manual_attr.gain.bgain);
	} else if (awb_mode == 1) {
		TUNING_API_EQ(hbn_isp_get_awb_attr, &awb_attr, return);

		read_p("use_damping: ", "%d", &awb_attr.auto_attr.use_damping);
		read_p("use_manual_damp_coff: ", "%d", &awb_attr.auto_attr.use_manual_damp_coff);
		read_p("manual_damp_coff: ", "%f", &awb_attr.auto_attr.manual_damp_coff);
		read_p("lock_tolerance: ", "%f", &awb_attr.auto_attr.lock_tolerance);
		read_p("unlock_tolerance: ", "%f", &awb_attr.auto_attr.unlock_tolerance);
		awb_attr.mode = HBN_ISP_MODE_AUTO;
	} else {
		printf("Unknown mode: %d\n", awb_mode);
		return;
	}

	TUNING_API_EQ(hbn_isp_set_awb_attr, &awb_attr, return);
}

void tuning_handle_get_white_balance(tuning_context_t *ctx)
{
	hbn_isp_awb_attr_t awb_attr = {0};

	TUNING_API_EQ(hbn_isp_get_awb_attr, &awb_attr, return);

	printf("Currently AWB is in %s mode", (awb_attr.mode == HBN_ISP_MODE_MANUAL)?"manual":"auto");
	if (awb_attr.mode == HBN_ISP_MODE_AUTO) {
		if (awb_attr.lock_state) {
			printf(", lock state\n");
		} else {
			printf(", running state\n");
		}
	} else {
		printf("\n");
	}

	printf("use_damping: %d\n", awb_attr.auto_attr.use_damping);
	printf("use_manual_damp_coff:  %d\n", awb_attr.auto_attr.use_manual_damp_coff);
	printf("manual_damp_coff:  %f\n", awb_attr.auto_attr.manual_damp_coff);
	printf("lock_tolerance:  %f\n", awb_attr.auto_attr.lock_tolerance);
	printf("unlock_tolerance:  %f\n", awb_attr.auto_attr.unlock_tolerance);

	printf("temper %d\n", awb_attr.auto_attr.temper);
	printf("rgain %f\n", awb_attr.auto_attr.gain.rgain);
	printf("grgain %f\n", awb_attr.auto_attr.gain.grgain);
	printf("gbgain %f\n", awb_attr.auto_attr.gain.gbgain);
	printf("bgain %f\n", awb_attr.auto_attr.gain.bgain);
}

void tuning_hanle_set_ae_table(tuning_context_t *ctx)
{
	int32_t i;
	hbn_isp_exposure_table_t ae_table_attr = {0};

	for (i = 0; i < HBN_ISP_EXP_TABLE_NUM; i++) {
		printf("input table %d, typing [0] to finish:\n", i);
		read_p("exposure_time: ", "%f", &ae_table_attr.exp_table[i].exposure_time);
		if (ae_table_attr.exp_table[i].exposure_time == 0)
			break;

		read_p("again: ", "%f", &ae_table_attr.exp_table[i].again);
		read_p("dgain: ", "%f", &ae_table_attr.exp_table[i].dgain);
		read_p("isp_gain: ", "%f", &ae_table_attr.exp_table[i].isp_gain);
	}
	ae_table_attr.valid_num = i == HBN_ISP_EXP_TABLE_NUM - 1 ? HBN_ISP_EXP_TABLE_NUM : i;

	TUNING_API_EQ(hbn_isp_set_exposure_table, &ae_table_attr, return);
}

void tuning_hanle_get_ae_table(tuning_context_t *ctx)
{
	int32_t i;
	hbn_isp_exposure_table_t ae_table_attr = {0};

	TUNING_API_EQ(hbn_isp_get_exposure_table, &ae_table_attr, return);

	for (i = 0; i < ae_table_attr.valid_num; i++) {
		printf("---------table %d---------\n", i);
		printf("exposure_time: %f\n", ae_table_attr.exp_table[i].exposure_time);
		printf("again: %f\n", ae_table_attr.exp_table[i].again);
		printf("dgain: %f\n", ae_table_attr.exp_table[i].dgain);
		printf("isp_gain: %f\n", ae_table_attr.exp_table[i].isp_gain);
	}
}

void tuning_hanle_set_exp_roi(tuning_context_t *ctx)
{
	int32_t i;
	hbn_isp_exposure_roi_t exp_roi = {0};

	read_p("roi_num: ", "%d", &exp_roi.roi_num);
	read_p("roi_weight: ", "%f", &exp_roi.roi_weight);

	for (i = 0; i < HBN_ISP_ROI_WINDOWS_MAX; i++) {
		printf("input table %d, typing [0] to finish:\n", i);
		read_p("weight: ", "%f", &exp_roi.roi_window[i].weight);
		if (exp_roi.roi_window[i].weight == 0)
			break;

		read_p("h_offset: ", "%d", &exp_roi.roi_window[i].window.h_offset);
		read_p("v_offset: ", "%d", &exp_roi.roi_window[i].window.v_offset);
		read_p("width: ", "%d", &exp_roi.roi_window[i].window.width);
		read_p("height: ", "%d", &exp_roi.roi_window[i].window.height);
	}
	exp_roi.roi_num = i == HBN_ISP_ROI_WINDOWS_MAX - 1 ? HBN_ISP_ROI_WINDOWS_MAX : i;

	TUNING_API_EQ(hbn_isp_set_exposure_roi, &exp_roi, return);
}

void tuning_hanle_get_exp_roi(tuning_context_t *ctx)
{
	int32_t i;
	hbn_isp_exposure_roi_t exp_roi = {0};

	TUNING_API_EQ(hbn_isp_get_exposure_roi, &exp_roi, return);

	printf("roi_weight: %f\n", exp_roi.roi_weight);
	for (i = 0; i < exp_roi.roi_num; i++) {
		printf("---------table %d---------\n", i);
		printf("weight: %f\n", exp_roi.roi_window[i].weight);
		printf("h_offset: %d\n", exp_roi.roi_window[i].window.h_offset);
		printf("v_offset: %d\n", exp_roi.roi_window[i].window.v_offset);
		printf("width: %d\n", exp_roi.roi_window[i].window.width);
		printf("height: %d\n", exp_roi.roi_window[i].window.height);
	}
}

void tuning_hanle_set_ae_zone_weight(tuning_context_t *ctx)
{
	int32_t i, j;
	hbn_isp_ae_zone_weight_attr_t weight_attr = {0};

	for (i = 0; i < HBN_ISP_AE_ZONE_GRID_NUM; i++) {
		for (j = 0; j < HBN_ISP_AE_ZONE_GRID_NUM; j++) {
			if (i > 10 && i < 20 && j > 10 && j < 20) {
				weight_attr.weight[i * HBN_ISP_AE_ZONE_GRID_NUM + j].weight = 2.0;
			} else {
				weight_attr.weight[i * HBN_ISP_AE_ZONE_GRID_NUM + j].weight = 1.0;
			}
		}
	}

	TUNING_API_EQ(hbn_isp_set_ae_zone_weight_attr, &weight_attr, return);
}

void tuning_hanle_get_ae_zone_weight(tuning_context_t *ctx)
{
	int32_t i, j;
	hbn_isp_ae_zone_weight_attr_t weight_attr = {0};

	TUNING_API_EQ(hbn_isp_get_ae_zone_weight_attr, &weight_attr, return);

	for (i = 0; i < HBN_ISP_AE_ZONE_GRID_NUM; i++) {
		printf("%d:", i);
		for (j = 0; j < HBN_ISP_AE_ZONE_GRID_NUM; j++) {
			printf(" %.1f", weight_attr.weight[i * HBN_ISP_AE_ZONE_GRID_NUM + j].weight);
		}
		printf("\n");
	}
}

void tuning_dump_yuv(tuning_context_t *ctx)
{
	pipe_contex_info_t *pipe_info;

	pipe_info = &ctx->pipe_contex_info[ctx->handle_id];
	read_p("typing the number to dump: ", "%d", &pipe_info->yuv_dump_cnt);
}

void tuning_get_ae_statistics(tuning_context_t *ctx)
{
	int32_t col, row;
	uint32_t *luma;
	hbn_isp_ae_statistics_t ae_statistics = {0};

	TUNING_API_EQ(hbn_isp_get_ae_statistics, &ae_statistics, return);

	printf("Ae statistics current frameid: %d, timestamps: %ld\n", ae_statistics.frame_id, ae_statistics.timestamps);
	printf("Datatype: %d\n", ae_statistics.datatype);

	for (col = 0; col < HBN_ISP_AE_ZONE_GRID_NUM * HBN_ISP_PIXEL_CHANNEL; col += HBN_ISP_PIXEL_CHANNEL) {
		for (row = 0; row < HBN_ISP_AE_ZONE_GRID_NUM; row++) {
			luma = &ae_statistics.expStat[HBN_ISP_AE_ZONE_GRID_NUM * HBN_ISP_PIXEL_CHANNEL * row + col];
			printf("(%d, %d, %d, %d) ", *luma, *(luma+1), *(luma+2), *(luma+3));
		}
		printf("\n");
	}
	printf("done\n");
}

void tuning_set_module_control(tuning_context_t *ctx)
{
	hbn_isp_module_ctrl_t module_ctrl = {0};
	uint32_t key;

	printf("Typing [1] to enable, [0] to disable\n");
	read_p("COMPAND: ", "%d", &key); module_ctrl.module.u32Key |= key << 0;
	read_p("LSC: ", "%d", &key); module_ctrl.module.u32Key |= key << 1;
	read_p("DG: ", "%d", &key); module_ctrl.module.u32Key |= key << 2;
	read_p("WDR: ", "%d", &key); module_ctrl.module.u32Key |= key << 3;
	read_p("GE: ", "%d", &key); module_ctrl.module.u32Key |= key << 4;
	read_p("DPCC: ", "%d", &key); module_ctrl.module.u32Key |= key << 5;
	read_p("2DNR: ", "%d", &key); module_ctrl.module.u32Key |= key << 6;
	read_p("3DNR: ", "%d", &key); module_ctrl.module.u32Key |= key << 7;
	read_p("Demosaic: ", "%d", &key); module_ctrl.module.u32Key |= key << 8;
	read_p("CCM: ", "%d", &key); module_ctrl.module.u32Key |= key << 9;
	read_p("Gamma: ", "%d", &key); module_ctrl.module.u32Key |= key << 10;
	read_p("EE: ", "%d", &key); module_ctrl.module.u32Key |= key << 11;
	read_p("CPROC: ", "%d", &key); module_ctrl.module.u32Key |= key << 12;
	read_p("CNR: ", "%d", &key); module_ctrl.module.u32Key |= key << 13;

	TUNING_API_EQ(hbn_isp_set_module_control, &module_ctrl, return);
}

void tuning_get_module_control(tuning_context_t *ctx)
{
	hbn_isp_module_ctrl_t module_ctrl = {0};
	TUNING_API_EQ(hbn_isp_get_module_control, &module_ctrl, return);

	printf("module_ctrl.module.u32Key %d\n", module_ctrl.module.u32Key);

	printf("COMPAND: %s\n", (module_ctrl.module.u32Key & 1 << 0)?"Enable":"Disable");
	printf("LSC: %s\n", (module_ctrl.module.u32Key & 1 << 1)?"Enable":"Disable");
	printf("DG: %s\n", (module_ctrl.module.u32Key & 1 << 2)?"Enable":"Disable");
	printf("WDR: %s\n", (module_ctrl.module.u32Key & 1 << 3)?"Enable":"Disable");
	printf("GE: %s\n", (module_ctrl.module.u32Key & 1 << 4)?"Enable":"Disable");
	printf("DPCC: %s\n", (module_ctrl.module.u32Key & 1 << 5)?"Enable":"Disable");
	printf("2DNR: %s\n", (module_ctrl.module.u32Key & 1 << 6)?"Enable":"Disable");
	printf("3DNR: %s\n", (module_ctrl.module.u32Key & 1 << 7)?"Enable":"Disable");
	printf("Demosaic: %s\n", (module_ctrl.module.u32Key & 1 << 8)?"Enable":"Disable");
	printf("CCM: %s\n", (module_ctrl.module.u32Key & 1 << 9)?"Enable":"Disable");
	printf("Gamma: %s\n", (module_ctrl.module.u32Key & 1 << 10)?"Enable":"Disable");
	printf("EE: %s\n", (module_ctrl.module.u32Key & 1 << 11)?"Enable":"Disable");
	printf("CPROC: %s\n", (module_ctrl.module.u32Key & 1 << 12)?"Enable":"Disable");
	printf("CNR: %s\n", (module_ctrl.module.u32Key & 1 << 13)?"Enable":"Disable");
}

void tuning_get_af_statistics(tuning_context_t *ctx)
{
	int32_t i, j, pos;
	hbn_isp_af_statistics_t af_statistics = {0};

	TUNING_API_EQ(hbn_isp_get_af_statistics, &af_statistics, return);

	printf("frame_id: %d\n", af_statistics.frame_id);
	printf("sharpnessLowPass:\n");
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			pos = i * 15 + j;
			printf(" %d", af_statistics.sharpnessLowPass[pos]);
		}
		printf("\n");
	}
	printf("\n");

	printf("sharpnessHighPass:\n");
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			pos = i * 15 + j;
			printf(" %d", af_statistics.sharpnessHighPass[pos]);
		}
		printf("\n");
	}
	printf("\n");

	printf("histLowData:\n");
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			pos = i * 15 + j;
			printf(" %d", af_statistics.histLowData[pos]);
		}
		printf("\n");
	}
	printf("\n");

	printf("histHighData:\n");
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			pos = i * 15 + j;
			printf(" %d", af_statistics.histHighData[pos]);
		}
		printf("\n");
	}
}

void tuning_hanle_set_2dnr_attr(tuning_context_t *ctx)
{
	uint32_t mode;
	hbn_isp_2dnr_attr_t dnr2_attr = {0};

	read_p("typing the 2dnr mode, manual(0)/auto(1): ", "%d", &mode);
	TUNING_API_EQ(hbn_isp_get_2dnr_attr, &dnr2_attr, return);

	if (mode == 0) {
		dnr2_attr.mode = HBN_ISP_MODE_MANUAL;
	} else if (mode == 1) {
		dnr2_attr.mode = HBN_ISP_MODE_AUTO;
	} else {
		printf("Unknown mode: %d\n", mode);
		return;
	}

	TUNING_API_EQ(hbn_isp_set_2dnr_attr, &dnr2_attr, return);
}

void tuning_hanle_get_2dnr_attr(tuning_context_t *ctx)
{
	hbn_isp_2dnr_attr_t dnr2_attr = {0};

	TUNING_API_EQ(hbn_isp_get_2dnr_attr, &dnr2_attr, return);

	printf("2dnr is in %s mode\n", (dnr2_attr.mode == HBN_ISP_MODE_MANUAL)?"manual":"auto");
	printf("2dnr current value:\n");
	printf("blend_static: %f\n", dnr2_attr.manual_attr.blend_static);
	printf("blend_motion: %f\n", dnr2_attr.manual_attr.blend_motion);
	printf("blend_slope: %f\n", dnr2_attr.manual_attr.blend_slope);
	printf("vst_factor: %f\n", dnr2_attr.manual_attr.vst_factor);

	pr_linear("sigma_scale", HBN_ISP_2DNR_SIGMA_NUM, " %f", dnr2_attr.manual_attr.sigma_scale);
	pr_linear("sigma_factor_mul", HBN_ISP_2DNR_SIGMA_NUM, " %f", dnr2_attr.manual_attr.sigma_factor_mul);

	printf("sigma_factor_motion_max: %d\n", dnr2_attr.manual_attr.sigma_factor_motion_max);
	printf("sigma_factor_motion_min: %d\n", dnr2_attr.manual_attr.sigma_factor_motion_min);
	printf("sigma_offset: %d\n", dnr2_attr.manual_attr.sigma_offset);

	pr_double("static_detail_thresh", HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM,
		" %d", dnr2_attr.manual_attr.static_detail_thresh);
	pr_double("static_detail_boost_thresh", HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM,
		" %d", dnr2_attr.manual_attr.static_detail_boost_thresh);
	pr_double("static_detail_boost", HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM,
		" %f", dnr2_attr.manual_attr.static_detail_boost);
	pr_double("static_detail_clip_thresh", HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM,
		" %d", dnr2_attr.manual_attr.static_detail_clip_thresh);

	pr_double("moving_detail_thresh", HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM,
		" %d", dnr2_attr.manual_attr.moving_detail_thresh);
	pr_double("moving_detail_boost_thresh", HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM,
		" %d", dnr2_attr.manual_attr.moving_detail_boost_thresh);
	pr_double("moving_detail_boost", HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM,
		" %f", dnr2_attr.manual_attr.moving_detail_boost);
	pr_double("moving_detail_clip_thresh", HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM,
		" %d", dnr2_attr.manual_attr.moving_detail_clip_thresh);
	pr_linear("static_factor", HBN_ISP_2DNR_SIGMA_NUM, " %f", dnr2_attr.manual_attr.static_factor);

	printf("luma_curve_cfg:\n");
	pr_linear("    ary_x", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.luma_curve_cfg.ary_x);
	pr_linear("    ary_y", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.luma_curve_cfg.ary_y);
	pr_linear("    ary_px", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.luma_curve_cfg.ary_px);
	printf("    interp_mode: %d\n", dnr2_attr.manual_attr.luma_curve_cfg.interp_mode);

	printf("lsc_comp_curve_cfg:\n");
	pr_linear("    ary_x", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.lsc_comp_curve_cfg.ary_x);
	pr_linear("    ary_y", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.lsc_comp_curve_cfg.ary_y);
	pr_linear("    ary_px", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.lsc_comp_curve_cfg.ary_px);
	printf("    interp_mode: %d\n", dnr2_attr.manual_attr.lsc_comp_curve_cfg.interp_mode);

	printf("motion_cfg:\n");
	pr_linear("    motion_anchor_x", HBN_ISP_2DNR_MOTION_SIZE, " %d", dnr2_attr.manual_attr.motion_cfg.motion_anchor_x);
	pr_linear("    ary_x", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.motion_cfg.curve_cfg.ary_x);
	pr_linear("    ary_y", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.motion_cfg.curve_cfg.ary_y);
	pr_linear("    ary_px", HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.manual_attr.motion_cfg.curve_cfg.ary_px);
	printf("    interp_mode: %d\n", dnr2_attr.manual_attr.motion_cfg.curve_cfg.interp_mode);


	// printf("2dnr auto config value:\n");
	printf("auto_level: %d\n", dnr2_attr.auto_attr.auto_level);
	// pr_linear("gain", dnr2_attr.auto_attr.auto_level, " %f", dnr2_attr.auto_attr.gain);
	// pr_linear("vst_factor", dnr2_attr.auto_attr.auto_level, " %f", dnr2_attr.auto_attr.vst_factor);
	// pr_linear("blend_static", dnr2_attr.auto_attr.auto_level, " %f", dnr2_attr.auto_attr.blend_static);
	// pr_linear("blend_motion", dnr2_attr.auto_attr.auto_level, " %f", dnr2_attr.auto_attr.blend_motion);
	// pr_linear("blend_slope", dnr2_attr.auto_attr.auto_level, " %f", dnr2_attr.auto_attr.blend_slope);
	// pr_linear("sigma_offset", dnr2_attr.auto_attr.auto_level, " %d", dnr2_attr.auto_attr.sigma_offset);
	// pr_double("luma_curve_y", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.auto_attr.luma_curve_y);
	// pr_double("lsc_comp_curve_y", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.auto_attr.lsc_comp_curve_y);
	// pr_double("motion_fac_curve_y", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_CURVE_SIZE, " %d", dnr2_attr.auto_attr.motion_fac_curve_y);
	// pr_double("motion_anchor_x", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_MOTION_SIZE, " %d", dnr2_attr.auto_attr.motion_anchor_x);

	// pr_double("sigma_scale", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_SIGMA_NUM, " %f", dnr2_attr.auto_attr.sigma_scale);
	// pr_double("static_factor", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_SIGMA_NUM, " %f", dnr2_attr.auto_attr.static_factor);
	// pr_double("sigma_factor_mul", dnr2_attr.auto_attr.auto_level, HBN_ISP_2DNR_SIGMA_NUM, " %f", dnr2_attr.auto_attr.sigma_factor_mul);
	// pr_linear("sigma_factor_motion_max", dnr2_attr.auto_attr.auto_level, " %d", dnr2_attr.auto_attr.sigma_factor_motion_max);

	// pr_triple("static_detail_thresh", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM, " %d", dnr2_attr.auto_attr.static_detail_thresh);
	// pr_triple("static_detail_boost_thresh", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM, " %d", dnr2_attr.auto_attr.static_detail_boost_thresh);
	// pr_triple("static_detail_boost", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM, " %f", dnr2_attr.auto_attr.static_detail_boost);
	// pr_triple("static_detail_clip_thresh", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_STATIC_X_NUM, HBN_ISP_2DNR_STATIC_Y_NUM, " %d", dnr2_attr.auto_attr.static_detail_clip_thresh);
	
	// pr_triple("moving_detail_thresh", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM, " %d", dnr2_attr.auto_attr.moving_detail_thresh);
	// pr_triple("moving_detail_boost_thresh", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM, " %d", dnr2_attr.auto_attr.moving_detail_boost_thresh);
	// pr_triple("moving_detail_boost", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM, " %f", dnr2_attr.auto_attr.moving_detail_boost);
	// pr_triple("moving_detail_clip_thresh", dnr2_attr.auto_attr.auto_level,
	// 	HBN_ISP_2DNR_MOVING_X_NUM, HBN_ISP_2DNR_MOVING_Y_NUM, " %d", dnr2_attr.auto_attr.moving_detail_clip_thresh);
	
}

void tuning_hanle_set_3dnr_attr(tuning_context_t *ctx)
{
	uint32_t mode;
	hbn_isp_3dnr_attr_t dnr3_attr = {0};

	read_p("typing the 3dnr mode, manual(0)/auto(1): ", "%d", &mode);
	TUNING_API_EQ(hbn_isp_get_3dnr_attr, &dnr3_attr, return);

	if (mode == 0) {
		dnr3_attr.mode = HBN_ISP_MODE_MANUAL;
	} else if (mode == 1) {
		dnr3_attr.mode = HBN_ISP_MODE_AUTO;
	} else {
		printf("Unknown mode: %d\n", mode);
		return;
	}

	TUNING_API_EQ(hbn_isp_set_3dnr_attr, &dnr3_attr, return);
}

void tuning_hanle_get_3dnr_attr(tuning_context_t *ctx)
{
	hbn_isp_3dnr_attr_t dnr3_attr = {0};

	TUNING_API_EQ(hbn_isp_get_3dnr_attr, &dnr3_attr, return);

	printf("3dnr is in %s mode\n", (dnr3_attr.mode == HBN_ISP_MODE_MANUAL)?"manual":"auto");
	printf("3dnr current value:\n");
	printf("vst_factor: %f\n", dnr3_attr.manual_attr.vst_factor);
	printf("tnr_strength: %d\n", dnr3_attr.manual_attr.tnr_strength);
	printf("tnr_strength2: %d\n", dnr3_attr.manual_attr.tnr_strength2);
	printf("filter_len: %d\n", dnr3_attr.manual_attr.filter_len);
	printf("filter_len2: %d\n", dnr3_attr.manual_attr.filter_len2);
	printf("motion_smooth_factor: %f\n", dnr3_attr.manual_attr.motion_smooth_factor);
	printf("range_h: %d\n", dnr3_attr.manual_attr.range_h);
	printf("sad_weight: %d\n", dnr3_attr.manual_attr.sad_weight);
	printf("diff_type: %d\n", dnr3_attr.manual_attr.diff_type);
	printf("sqr_diff_factor: %d\n", dnr3_attr.manual_attr.sqr_diff_factor);
	printf("motion_smooth_lvl: %d\n", dnr3_attr.manual_attr.motion_smooth_lvl);
	printf("dilate_h: %d\n", dnr3_attr.manual_attr.dilate_h);
	printf("noise_level: %d\n", dnr3_attr.manual_attr.noise_level);
	printf("thr_motion_slope: %d\n", dnr3_attr.manual_attr.thr_motion_slope);
	pr_linear("tnr_luma_curve_x", HBN_ISP_3DNR_THR_LUMA_CURVE_NUM, " %d", dnr3_attr.manual_attr.tnr_luma_curve_x);
	pr_linear("tnr_luma_curve_y", HBN_ISP_3DNR_THR_LUMA_CURVE_NUM, " %d", dnr3_attr.manual_attr.tnr_luma_curve_y);
	pr_linear("tnr_motion_slop_y", HBN_ISP_3DNR_THR_LUMA_CURVE_NUM, " %d", dnr3_attr.manual_attr.tnr_motion_slop_y);
	printf("noise_cfg:\n");
	printf("    input_bits: %d\n", dnr3_attr.manual_attr.noise_cfg.input_bits);
	printf("    fix_curve_start: %d\n", dnr3_attr.manual_attr.noise_cfg.fix_curve_start);
	printf("    noisemodel_a: %f\n", dnr3_attr.manual_attr.noise_cfg.noisemodel_a);
	printf("    noisemodel_b: %f\n", dnr3_attr.manual_attr.noise_cfg.noisemodel_b);
	pr_linear("    bls_exp", HBN_ISP_3DNR_BLS_EXP_NUM, " %d", dnr3_attr.manual_attr.noise_cfg.bls_exp);


	// printf("3dnr auto config value:\n");
	// printf("auto_level: %d\n", dnr3_attr.auto_attr.auto_level);
	// printf("nm_k: %f\n", dnr3_attr.auto_attr.nm_k);
	// printf("nm_p: %f\n", dnr3_attr.auto_attr.nm_p);
	// pr_linear("gains", dnr3_attr.auto_attr.auto_level, " %f", dnr3_attr.auto_attr.gains);
	// pr_linear("fix_curve_start", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.fix_curve_start);
	// pr_linear("noisemodel_a", dnr3_attr.auto_attr.auto_level, " %f", dnr3_attr.auto_attr.noisemodel_a);
	// pr_linear("noisemodel_b", dnr3_attr.auto_attr.auto_level, " %f", dnr3_attr.auto_attr.noisemodel_b);
	// pr_linear("tnr_strength", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.tnr_strength);
	// pr_linear("tnr_strength2", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.tnr_strength2);
	// pr_linear("filter_len", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.filter_len);
	// pr_linear("filter_len2", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.filter_len2);
	// pr_linear("motion_smooth_factor", dnr3_attr.auto_attr.auto_level, " %f", dnr3_attr.auto_attr.motion_smooth_factor);
	// pr_linear("range_h", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.range_h);
	// pr_linear("sad_weight", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.sad_weight);
	// pr_linear("sqr_diff_factor", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.sqr_diff_factor);
	// pr_linear("motion_smooth_lvl", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.motion_smooth_lvl);
	// pr_linear("motion_dilate_en", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.motion_dilate_en);
	// pr_linear("dilate_h", dnr3_attr.auto_attr.auto_level, " %d", dnr3_attr.auto_attr.dilate_h);
	// pr_double("bls_exp", dnr3_attr.auto_attr.auto_level, HBN_ISP_3DNR_BLS_EXP_NUM, " %d", dnr3_attr.auto_attr.bls_exp);
	// pr_double("tnr_luma_curve_y", dnr3_attr.auto_attr.auto_level, HBN_ISP_3DNR_THR_LUMA_CURVE_NUM, " %d", dnr3_attr.auto_attr.tnr_luma_curve_y);
	// pr_double("tnr_motion_slop_y", dnr3_attr.auto_attr.auto_level, HBN_ISP_3DNR_THR_LUMA_CURVE_NUM, " %d", dnr3_attr.auto_attr.tnr_motion_slop_y);
}

void tuning_hanle_set_awb_preference_attr(tuning_context_t *ctx)
{
	int32_t illum, level;
	hbn_isp_awb_preference_attr_t awb_pre_attr = {0};

	for (illum = 0; illum < HBN_ISP_ILLUPROFILE_NUM; illum++) {
		awb_pre_attr.gray_preference[illum].enable = 1;
		for (level = 0; level < HBN_ISP_AWB_LIGHT_LEVEL; level++) {
			awb_pre_attr.gray_preference[illum].brightness_level[level] = 1.0f + level;
			awb_pre_attr.gray_preference[illum].gray_rgain[level] = 256 + level;
			awb_pre_attr.gray_preference[illum].gray_bgain[level] = 256 - level;
		}
	}

	TUNING_API_EQ(hbn_isp_set_awb_preference_attr, &awb_pre_attr, return);
}

void tuning_hanle_get_awb_preference_attr(tuning_context_t *ctx)
{
	int32_t illum, level;
	hbn_isp_awb_preference_attr_t awb_pre_attr = {0};

	TUNING_API_EQ(hbn_isp_get_awb_preference_attr, &awb_pre_attr, return);

	for (illum = 0; illum < HBN_ISP_ILLUPROFILE_NUM; illum++) {
		printf("illum: %d, %s\n", illum, (awb_pre_attr.gray_preference[illum].enable ? "enable":"disable"));
		printf("brightness_level:");
		for (level = 0; level < HBN_ISP_AWB_LIGHT_LEVEL; level++) {
			printf(" %.1f", awb_pre_attr.gray_preference[illum].brightness_level[level]);
		}
		printf("\n");
		printf("gray_rgain:");
		for (level = 0; level < HBN_ISP_AWB_LIGHT_LEVEL; level++) {
			printf(" %d", awb_pre_attr.gray_preference[illum].gray_rgain[level]);
		}
		printf("\n");
		printf("gray_bgain:");
		for (level = 0; level < HBN_ISP_AWB_LIGHT_LEVEL; level++) {
			printf(" %d", awb_pre_attr.gray_preference[illum].gray_bgain[level]);
		}
		printf("\n");
	}
}

void tuning_handle_set_dpcc_attr(tuning_context_t *ctx)
{
	uint32_t mode;
	int32_t tmp_num;
	hbn_isp_dpcc_attr_t dpcc_attr = {0};

	read_p("typing the dpcc mode, manual(0)/auto(1): ", "%d", &mode);
	TUNING_API_EQ(hbn_isp_get_dpcc_attr, &dpcc_attr, return);

	if (mode == 0) {
		dpcc_attr.mode = HBN_ISP_MODE_MANUAL;
		read_p("bpt_enable: ", "%d", &tmp_num);
		dpcc_attr.manual_attr.bpt_enable = tmp_num;
		read_p("bpt_num: ", "%d", &tmp_num);
		dpcc_attr.manual_attr.bpt_num = tmp_num;
		read_p("bpt_out_mode: ", "%d", &tmp_num);
		dpcc_attr.manual_attr.bpt_out_mode = tmp_num;
	} else if (mode == 1) {
		dpcc_attr.mode = HBN_ISP_MODE_AUTO;
	} else {
		printf("Unknown mode: %d\n", mode);
		return;
	}

	TUNING_API_EQ(hbn_isp_set_dpcc_attr, &dpcc_attr, return);
}

void tuning_handle_get_dpcc_attr(tuning_context_t *ctx)
{
	hbn_isp_dpcc_attr_t dpcc_attr = {0};

	TUNING_API_EQ(hbn_isp_get_dpcc_attr, &dpcc_attr, return);

	printf("dpcc is in %s mode\n", (dpcc_attr.mode == HBN_ISP_MODE_MANUAL)?"manual":(dpcc_attr.mode == HBN_ISP_MODE_AUTO)?"auto":"disable");
	printf("bpt_enable: %d\n", dpcc_attr.manual_attr.bpt_enable);
	printf("bpt_num: %d\n", dpcc_attr.manual_attr.bpt_num);
	printf("bpt_out_mode: %d\n", dpcc_attr.manual_attr.bpt_out_mode);
	printf("out_mode: %d\n", dpcc_attr.manual_attr.out_mode);
	printf("set_use: %d\n", dpcc_attr.manual_attr.set_use);
	pr_double("line_mad_fac", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.line_mad_fac);
	pr_double("line_thresh", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.line_thresh);
	pr_linear("methods_set", HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.manual_attr.methods_set);
	pr_double("pg_fac", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.pg_fac);
	pr_double("rg_fac", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.rg_fac);
	pr_double("rnd_offs", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.rnd_offs);
	pr_double("rnd_thresh", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.rnd_thresh);
	pr_double("ro_limits", HBN_ISP_DPCC_CHANNEL_NUM, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.manual_attr.ro_limits);

	printf("auto config:\n");
	printf("auto_level: %d\n", dpcc_attr.auto_attr.auto_level);
	pr_linear("gains", HBN_ISP_AUTO_LEVEL_MAX, " %f", dpcc_attr.auto_attr.gains);
	pr_triple("line_mad_fac", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.line_mad_fac);
	pr_triple("line_thresh", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.line_thresh);
	pr_triple("pg_fac", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.pg_fac);
	pr_triple("rg_fac", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.rg_fac);
	pr_triple("rnd_offs", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.rnd_offs);
	pr_triple("rnd_thresh", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.rnd_thresh);
	pr_triple("ro_limits", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_CHANNEL_NUM,
		HBN_ISP_DPCC_MP_TYPE_NUM, " %d", dpcc_attr.auto_attr.ro_limits);
	pr_double("methods_set", dpcc_attr.auto_attr.auto_level, HBN_ISP_DPCC_MP_TYPE_NUM,
		" %d", dpcc_attr.auto_attr.methods_set);
	pr_linear("out_mode", dpcc_attr.auto_attr.auto_level, " %d", dpcc_attr.auto_attr.out_mode);
	pr_linear("set_use", dpcc_attr.auto_attr.auto_level, " %d", dpcc_attr.auto_attr.set_use);
}

void tuning_handle_set_pattern_attr(tuning_context_t *ctx)
{
	int32_t tmp_num;
	hbn_isp_pattern_t pattern;

	read_p("bayer pattern(0-RGGB 1-GRBG 2-GBRG 3-BGGR): ", "%d", &tmp_num);
	pattern = tmp_num;

	TUNING_API_EQ(hbn_isp_set_pattern_attr, &pattern, return);
}

#ifdef CPU_3DLUT
static void tuning_cpu_3dlut(tuning_context_t *ctx, unsigned char *buf_src, unsigned char *buf_cpu)
{
	int32_t height, width;
	int32_t off;
	int32_t img_height, img_width;

	img_width = ctx->pipe_contex_info[ctx->handle_id].img_width;
	img_height = ctx->pipe_contex_info[ctx->handle_id].img_height;
	off = img_width * img_height;

	for (height = 0; height < img_height; height++) {
	for (width = 0; width < img_width; width++) {
		int y_pos = height * img_width + width;
		int u_pos = off + (height / 2) * img_width + (width & ~1u);
		int v_pos = off + (height / 2) * img_width + (width | 1u);

		unsigned char Y = buf_src[y_pos];
		unsigned char U = buf_src[u_pos];
		unsigned char V = buf_src[v_pos];

		int R = Y + (int)(1.403 * (V - 128));
		int G = Y - (int)(0.344136 * (U - 128) + 0.714136 * (V - 128));
		int B = Y + (int)(1.772 * (U - 128));

		R = R < 0 ? 0 : (R > 255 ? 255 : R);
		G = G < 0 ? 0 : (G > 255 ? 255 : G);
		B = B < 0 ? 0 : (B > 255 ? 255 : B);

		int x = FLOAT288INT(R / 255.0f * (LUT_SIZE - 1));
		int y = FLOAT288INT(G / 255.0f * (LUT_SIZE - 1));
		int z = FLOAT288INT(B / 255.0f * (LUT_SIZE - 1));

		int x0 = x >> 8;
		int y0 = y >> 8;
		int z0 = z >> 8;

		int x1 = (x0 + 1) < LUT_SIZE ? x0 + 1 : x0;
		int y1 = (y0 + 1) < LUT_SIZE ? y0 + 1 : y0;
		int z1 = (z0 + 1) < LUT_SIZE ? z0 + 1 : z0;

		unsigned long long dx = x & 0xFF;
		unsigned long long dy = y & 0xFF;
		unsigned long long dz = z & 0xFF;

		unsigned long long tmp[3] = {0};
		for (int i = 0; i < 3; i++) {
			tmp[i] = (256 - dx) * (256 - dy) * (256 - dz) * lut3d_map[x0][y0][z0][i] +
				dx * (256 - dy) * (256 - dz) * lut3d_map[x1][y0][z0][i] +
				(256 - dx) * dy * (256 - dz) * lut3d_map[x0][y1][z0][i] +
				(256 - dx) * (256 - dy) * dz * lut3d_map[x0][y0][z1][i] +
				dx * (256 - dy) * dz * lut3d_map[x1][y0][z1][i] +
				(256 - dx) * dy * dz * lut3d_map[x0][y1][z1][i] +
				dx * dy * (256 - dz) * lut3d_map[x1][y1][z0][i] +
				dx * dy * dz * lut3d_map[x1][y1][z1][i];
			tmp[i] = tmp[i] >> 32;
		}

		unsigned char R_R = (unsigned char)(tmp[0] > 255 ? 255 : tmp[0]);
		unsigned char G_R = (unsigned char)(tmp[1] > 255 ? 255 : tmp[1]);
		unsigned char B_R = (unsigned char)(tmp[2] > 255 ? 255 : tmp[2]);

		int Y_R = (int)(0.299 * R_R + 0.587 * G_R + 0.114 * B_R);
		int V_R = (int)(0.500 * R_R - 0.419 * G_R - 0.081 * B_R + 128);
		int U_R = (int)(-0.169 * R_R - 0.331 * G_R + 0.500 * B_R + 128);

		buf_cpu[y_pos] = Y_R < 0 ? 0 : (Y_R > 255 ? 255 : Y_R);
		buf_cpu[u_pos] = U_R < 0 ? 0 : (U_R > 255 ? 255 : U_R);
		buf_cpu[v_pos] = V_R < 0 ? 0 : (V_R > 255 ? 255 : V_R);
	}
	}
}
#endif

static void tuning_opencl_3dlut(tuning_context_t *ctx, unsigned char *buf_src, unsigned char *buf_opencl)
{
	cl_int ret;
	tuning_opencl_ctx_t *opencl_ctx;
	int32_t img_size, img_width, img_height;

	opencl_ctx = &ctx->pipe_contex_info[ctx->handle_id].opencl_ctx;
	img_width = ctx->pipe_contex_info[ctx->handle_id].img_width;
	img_height = ctx->pipe_contex_info[ctx->handle_id].img_height;
	img_size = img_width * img_height * 1.5;

	ret = clEnqueueWriteBuffer(opencl_ctx->queue, opencl_ctx->input_image, CL_TRUE, 0, img_size, buf_src, 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clEnqueueWriteBuffer fail, ret: %d\n", ret);
		return;
	}

	ret = clEnqueueWriteBuffer(opencl_ctx->queue, opencl_ctx->lut_buffer, CL_TRUE, 0, LUT_SIZE * LUT_SIZE * LUT_SIZE * 3 * sizeof(int), lut3d_map, 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clEnqueueWriteBuffer fail, ret: %d\n", ret);
		return;
	}

	int lut_size = LUT_SIZE;
	ret = clSetKernelArg(opencl_ctx->kernel, 0, sizeof(cl_mem), &opencl_ctx->input_image);
	ret |= clSetKernelArg(opencl_ctx->kernel, 1, sizeof(cl_mem), &opencl_ctx->output_image);
	ret |= clSetKernelArg(opencl_ctx->kernel, 2, sizeof(cl_mem), &opencl_ctx->lut_buffer);
	ret |= clSetKernelArg(opencl_ctx->kernel, 3, sizeof(int), &lut_size);
	ret |= clSetKernelArg(opencl_ctx->kernel, 4, sizeof(int), &img_height);
	ret |= clSetKernelArg(opencl_ctx->kernel, 5, sizeof(int), &img_width);
	if (ret != CL_SUCCESS) {
		pr_tuning("clSetKernelArg fail, ret: %d\n", ret);
		return;
	}

	size_t global_work_size[2] = {img_height, img_width};
	ret = clEnqueueNDRangeKernel(opencl_ctx->queue, opencl_ctx->kernel, 2, NULL, global_work_size, NULL, 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clEnqueueNDRangeKernel fail, ret: %d\n", ret);
		return;
	}

	ret = clEnqueueReadBuffer(opencl_ctx->queue, opencl_ctx->output_image, CL_TRUE, 0, img_size, buf_opencl, 0, NULL, NULL);
	if (ret != CL_SUCCESS) {
		pr_tuning("clEnqueueReadBuffer fail, ret: %d\n", ret);
		return;
	}
}

void tuning_handle_3dlut(tuning_context_t *ctx)
{
	int32_t ret;
	unsigned char *buf_src = NULL, *buf_opencl = NULL;
#ifdef CPU_3DLUT
	unsigned char *buf_cpu = NULL;
#endif
	hbn_vnode_image_t yuv_img = {0};
	hbn_vnode_handle_t isp_node_handle;
	char file_name[128] = {0};
	int32_t size;
	FILE *Fr;

	if (!BIT_ENABLE(ctx->work_mode, LUT3D_MASK)) {
		pr_tuning("Tuning tool not in 3dlut mode, try run with -a 1\n");
		return ;
	}

	isp_node_handle = ctx->pipe_contex_info[ctx->handle_id].pipe_contex.isp_node_handle;

	ret = hbn_vnode_getframe(isp_node_handle, 0, 1000, &yuv_img);
	if (ret) {
		pr_tuning("get buffer from isp fail\n");
		return;
	}
	size = yuv_img.buffer.size[0] + yuv_img.buffer.size[1];

	buf_src = (unsigned char *)malloc(size);
#ifdef CPU_3DLUT
	buf_cpu = (unsigned char *)malloc(size);
#endif
	buf_opencl = (unsigned char *)malloc(size);
	if (!buf_src
#ifdef CPU_3DLUT
		|| !buf_cpu
#endif
		|| !buf_opencl) {
		pr_tuning("malloc fail\n");
		return;
	}

	memcpy(buf_src, (unsigned char *)yuv_img.buffer.virt_addr[0], yuv_img.buffer.size[0]);
	memcpy(buf_src + yuv_img.buffer.size[0], (unsigned char *)yuv_img.buffer.virt_addr[1], yuv_img.buffer.size[1]);

	snprintf(file_name, TUNING_PRINT_SIZE_MAX, "%s/lut_src.yuv", DEF_DUMP_PATH);
	tuning_dump_file(file_name, &yuv_img);

	hbn_vnode_releaseframe(isp_node_handle, 0, &yuv_img);

#ifdef CPU_3DLUT
	clock_t start_cpu = clock();
	tuning_cpu_3dlut(ctx, buf_src, buf_cpu);
	clock_t end_cpu = clock();
	double time_cpu = ((double)(end_cpu - start_cpu)) / CLOCKS_PER_SEC;
	printf("CPU execution time: %f seconds\n", time_cpu);
#endif

	clock_t start_opencl = clock();
	tuning_opencl_3dlut(ctx, buf_src, buf_opencl);
	clock_t end_opencl = clock();
	double time_opencl = ((double)(end_opencl - start_opencl)) / CLOCKS_PER_SEC;
	printf("OpenCL execution time: %f seconds\n", time_opencl);

#ifdef CPU_3DLUT
	snprintf(file_name, TUNING_PRINT_SIZE_MAX, "%s/lut_res_cpu.yuv", DEF_DUMP_PATH);
	Fr = fopen(file_name, "w");
	if (Fr == NULL) {
		printf("open %s fail", file_name);
		return;
	}

	fflush(stdout);
	fwrite(buf_cpu, 1, size, Fr);
	fflush(Fr);
#endif

	memset(file_name, 0, sizeof(file_name));
	snprintf(file_name, TUNING_PRINT_SIZE_MAX, "%s/lut_res_opencl.yuv", DEF_DUMP_PATH);
	Fr = fopen(file_name, "w");
	if (Fr == NULL) {
		printf("open %s fail", file_name);
		return;
	}

	fflush(stdout);
	fwrite(buf_opencl, 1, size, Fr);
	fflush(Fr);


	if (buf_src)
		free(buf_src);
#ifdef CPU_3DLUT
	if (buf_cpu)
		free(buf_cpu);
#endif
	if (buf_opencl)
		free(buf_opencl);
}
