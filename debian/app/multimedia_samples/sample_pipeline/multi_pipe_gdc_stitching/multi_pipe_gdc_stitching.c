/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#include "hbn_api.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"
#include "hb_media_codec.h"
#include "hb_media_error.h"

#define PIPE_NUM 2

typedef struct {
	int select_sensor_id;
	uint32_t sensor_mode;
	pipe_contex_t pipe_contexts;
	int active_mipi_host; // 根据实际的硬件连接情况确定使用对应的mipi host
	uint32_t ispchn_id;
} pipeline_info_t;

typedef struct {
	pipeline_info_t pipeinfo[PIPE_NUM];
	hb_mem_common_buf_t bin_buf_top;
	hb_mem_common_buf_t bin_buf_bottom;
	media_codec_context_t media_context;
	hbn_vnode_handle_t gdc_node_handle;
	char encode_type[32];
	char output_file[256];
	pthread_t read_codec_thread;
} media_info_t;


static int32_t total_pipeline_num = 0;
static int32_t verbose_flag = 0;
static int32_t used_mipi_host = 0;

static int32_t running = 0;

void *read_vse_data(void *contex);

static struct option const long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"verbose", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void print_help(void) {
	printf("Usage: %s [Options]\n", get_program_name());
	printf("Options:\n");
	printf("-c, --config=\"sensor=id\"\n");
	printf("\t\tConfigure parameters for each video pipeline, can be repeated up to %d times.\n", PIPE_NUM);
	printf("\t\tsensor   --  Sensor index,can have multiple parameters, reference sensor list.\n");
	printf("-v, --verbose\tEnable verbose mode\n");
	printf("-h, --help\tShow help message\n");
	printf("Support sensor list:\n");
	vp_show_sensors_list();
}

uint64_t get_timestamp_ms()
{
	uint64_t timestamp;
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_usec / 1000;
	return timestamp;
}

void signal_handle(int signo) {
	running = 0;
}

static int get_rc_params(media_codec_context_t *context, mc_rate_control_params_t *rc_params)
{
	int ret = 0;
	ret = hb_mm_mc_get_rate_control_config(context, rc_params);
	if (ret != 0) {
		printf("hb_mm_mc_get_rate_control_config Failed to get rc params ret=0x%x\n", ret);
		return -1;
	}
	switch (rc_params->mode) {
		case MC_AV_RC_MODE_H264CBR:
			rc_params->h264_cbr_params.intra_period = 30;
			rc_params->h264_cbr_params.intra_qp = 30;
			rc_params->h264_cbr_params.bit_rate = 5000;
			rc_params->h264_cbr_params.frame_rate = 30;
			rc_params->h264_cbr_params.initial_rc_qp = 20;
			rc_params->h264_cbr_params.vbv_buffer_size = 20;
			rc_params->h264_cbr_params.mb_level_rc_enalbe = 1;
			rc_params->h264_cbr_params.min_qp_I = 8;
			rc_params->h264_cbr_params.max_qp_I = 50;
			rc_params->h264_cbr_params.min_qp_P = 8;
			rc_params->h264_cbr_params.max_qp_P = 50;
			rc_params->h264_cbr_params.min_qp_B = 8;
			rc_params->h264_cbr_params.max_qp_B = 50;
			rc_params->h264_cbr_params.hvs_qp_enable = 1;
			rc_params->h264_cbr_params.hvs_qp_scale = 2;
			rc_params->h264_cbr_params.max_delta_qp = 10;
			rc_params->h264_cbr_params.qp_map_enable = 0;
			break;
		case MC_AV_RC_MODE_H264VBR:
			rc_params->h264_vbr_params.intra_qp = 20;
			rc_params->h264_vbr_params.intra_period = 30;
			rc_params->h264_vbr_params.intra_qp = 35;
			break;
		case MC_AV_RC_MODE_H264AVBR:
			rc_params->h264_avbr_params.intra_period = 15;
			rc_params->h264_avbr_params.intra_qp = 25;
			rc_params->h264_avbr_params.bit_rate = 2000;
			rc_params->h264_avbr_params.vbv_buffer_size = 3000;
			rc_params->h264_avbr_params.min_qp_I = 15;
			rc_params->h264_avbr_params.max_qp_I = 50;
			rc_params->h264_avbr_params.min_qp_P = 15;
			rc_params->h264_avbr_params.max_qp_P = 45;
			rc_params->h264_avbr_params.min_qp_B = 15;
			rc_params->h264_avbr_params.max_qp_B = 48;
			rc_params->h264_avbr_params.hvs_qp_enable = 0;
			rc_params->h264_avbr_params.hvs_qp_scale = 2;
			rc_params->h264_avbr_params.max_delta_qp = 5;
			rc_params->h264_avbr_params.qp_map_enable = 0;
			break;
		case MC_AV_RC_MODE_H264FIXQP:
			rc_params->h264_fixqp_params.force_qp_I = 23;
			rc_params->h264_fixqp_params.force_qp_P = 23;
			rc_params->h264_fixqp_params.force_qp_B = 23;
			rc_params->h264_fixqp_params.intra_period = 23;
			break;
		case MC_AV_RC_MODE_H264QPMAP:
			break;
		case MC_AV_RC_MODE_H265CBR:
			rc_params->h265_cbr_params.intra_period = 20;
			rc_params->h265_cbr_params.intra_qp = 30;
			rc_params->h265_cbr_params.bit_rate = 5000;
			rc_params->h265_cbr_params.frame_rate = 30;
			if (context->video_enc_params.width >= 480 ||
					context->video_enc_params.height >= 480) {
				rc_params->h265_cbr_params.initial_rc_qp = 30;
				rc_params->h265_cbr_params.vbv_buffer_size = 3000;
				rc_params->h265_cbr_params.ctu_level_rc_enalbe = 1;
			} else {
				rc_params->h265_cbr_params.initial_rc_qp = 20;
				rc_params->h265_cbr_params.vbv_buffer_size = 20;
				rc_params->h265_cbr_params.ctu_level_rc_enalbe = 1;
			}
			rc_params->h265_cbr_params.min_qp_I = 8;
			rc_params->h265_cbr_params.max_qp_I = 50;
			rc_params->h265_cbr_params.min_qp_P = 8;
			rc_params->h265_cbr_params.max_qp_P = 50;
			rc_params->h265_cbr_params.min_qp_B = 8;
			rc_params->h265_cbr_params.max_qp_B = 50;
			rc_params->h265_cbr_params.hvs_qp_enable = 1;
			rc_params->h265_cbr_params.hvs_qp_scale = 2;
			rc_params->h265_cbr_params.max_delta_qp = 10;
			rc_params->h265_cbr_params.qp_map_enable = 0;
			break;
		case MC_AV_RC_MODE_H265VBR:
			rc_params->h265_vbr_params.intra_qp = 20;
			rc_params->h265_vbr_params.intra_period = 30;
			rc_params->h265_vbr_params.intra_qp = 35;
			break;
		case MC_AV_RC_MODE_H265AVBR:
			rc_params->h265_avbr_params.intra_period = 15;
			rc_params->h265_avbr_params.intra_qp = 25;
			rc_params->h265_avbr_params.bit_rate = 2000;
			rc_params->h265_avbr_params.vbv_buffer_size = 3000;
			rc_params->h265_avbr_params.min_qp_I = 15;
			rc_params->h265_avbr_params.max_qp_I = 50;
			rc_params->h265_avbr_params.min_qp_P = 15;
			rc_params->h265_avbr_params.max_qp_P = 45;
			rc_params->h265_avbr_params.min_qp_B = 15;
			rc_params->h265_avbr_params.max_qp_B = 48;
			rc_params->h265_avbr_params.hvs_qp_enable = 0;
			rc_params->h265_avbr_params.hvs_qp_scale = 2;
			rc_params->h265_avbr_params.max_delta_qp = 5;
			rc_params->h265_avbr_params.qp_map_enable = 0;
			break;
		case MC_AV_RC_MODE_H265FIXQP:
			rc_params->h265_fixqp_params.force_qp_I = 23;
			rc_params->h265_fixqp_params.force_qp_P = 23;
			rc_params->h265_fixqp_params.force_qp_B = 23;
			rc_params->h265_fixqp_params.intra_period = 23;
			break;
		case MC_AV_RC_MODE_H265QPMAP:
			break;
		default:
			ret = HB_MEDIA_ERR_INVALID_PARAMS;
			break;
	}
	return ret;
}

static media_codec_id_t get_codec_media_id(const char* str) {
	if (strcasecmp(str, "h264") == 0) {
		return MEDIA_CODEC_ID_H264;
	} else if (strcasecmp(str, "h265") == 0) {
		return MEDIA_CODEC_ID_H265;
	} else
		return MEDIA_CODEC_ID_H264;
}

static int32_t vp_encode_config_param(media_codec_context_t *context,
		media_codec_id_t codec_type,
		int32_t width, int32_t height,
		int32_t frame_rate, uint32_t bit_rate)
{
	mc_video_codec_enc_params_t *params;

	memset(context, 0x00, sizeof(media_codec_context_t));
	context->encoder = 1;
	params = &context->video_enc_params;
	params->width = width;
	params->height = height;
	params->pix_fmt = MC_PIXEL_FORMAT_NV12;
	params->bitstream_buf_size = (width * height * 3 / 2  + 0x3ff) & ~0x3ff;
	params->frame_buf_count = 5;
	params->external_frame_buf = 0;
	params->bitstream_buf_count = 8;
	/* Hardware limitations of x5 wave521cl:
	 * - B-frame encoding is not supported.
	 * - Multi-frame reference is not supported.
	 * Therefore, GOP presets are restricted to 1 and 9.
	 */
	params->gop_params.gop_preset_idx = 1;
	params->rot_degree = MC_CCW_0;
	params->mir_direction = MC_DIRECTION_NONE;
	params->frame_cropping_flag = 0;
	params->enable_user_pts = 1;
	params->gop_params.decoding_refresh_type = 2;
	switch (codec_type)
	{
		case MEDIA_CODEC_ID_H264:
			context->codec_id = MEDIA_CODEC_ID_H264;
			params->rc_params.mode = MC_AV_RC_MODE_H264CBR;
			get_rc_params(context, &params->rc_params);
			params->rc_params.h264_cbr_params.frame_rate = frame_rate;
			params->rc_params.h264_cbr_params.bit_rate = bit_rate;
			break;
		case MEDIA_CODEC_ID_H265:
			context->codec_id = MEDIA_CODEC_ID_H265;
			params->rc_params.mode = MC_AV_RC_MODE_H265CBR;
			get_rc_params(context, &params->rc_params);
			params->rc_params.h265_cbr_params.frame_rate = frame_rate;
			params->rc_params.h265_cbr_params.bit_rate = bit_rate;
			break;
		case MEDIA_CODEC_ID_MJPEG:
			context->codec_id = MEDIA_CODEC_ID_MJPEG;
			params->rc_params.mode = MC_AV_RC_MODE_MJPEGFIXQP;
			get_rc_params(context, &params->rc_params);
			params->mjpeg_enc_config.restart_interval = width / 16;
			break;
		case MEDIA_CODEC_ID_JPEG:
			context->codec_id = MEDIA_CODEC_ID_JPEG;
			params->jpeg_enc_config.quality_factor = 50;
			params->mjpeg_enc_config.restart_interval = width / 16;
			break;
		default:
			printf("Not Support encoding type: %d!\n", codec_type);
			return -1;
	}

	return 0;
}

static int create_encodec(media_info_t * media_info)
{
	int ret = 0;
	pipe_contex_t *pipe_context = NULL;
	pipeline_info_t * pipe_info = &media_info->pipeinfo[0];
	pipe_context = &media_info->pipeinfo[0].pipe_contexts;
	media_codec_context_t * media_context = &media_info->media_context;

	int encode_width = 0;
	int encode_height = 0;
	int encode_fps = 30;
	media_codec_id_t encode_type = MEDIA_CODEC_ID_H264;
	isp_ichn_attr_t isp_ichn_attr = {0};

	mc_av_codec_startup_params_t startup_params = {0};

	ret = hbn_vnode_get_ichn_attr(pipe_context->isp_node_handle, pipe_info->ispchn_id,
			&isp_ichn_attr);
	ERR_CON_EQ(ret, 0);
	encode_type = get_codec_media_id(media_info->encode_type);
	encode_fps = pipe_context->sensor_config->camera_config->fps;
	encode_width = isp_ichn_attr.width;
	//因为要拼接,这里设置isp_ichn_attr.height *2
	encode_height = isp_ichn_attr.height *2;

	ret = vp_encode_config_param(media_context, encode_type,
			encode_width, encode_height,
			encode_fps, 8192);
	ERR_CON_EQ(ret, 0);
	ret = hb_mm_mc_initialize(media_context);
	ERR_CON_EQ(ret, 0);
	ret = hb_mm_mc_configure(media_context);
	ERR_CON_EQ(ret, 0);
	ret = hb_mm_mc_start(media_context, &startup_params);
	printf("Create %s idx: %d, init successful\n",
			media_context->encoder ? "Encode" : "Decode",
			media_context->instance_index);
	return 0;
}

static int is_number(const char *str) {
	while (*str) {
		if (!isdigit(*str)) return 0;
		str++;
	}
	return 1;
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

void parse_config(pipeline_info_t *pipeline_info, const char *config, int pipeline_idx,media_info_t *mediainfo) {
	int ret = 0;
	char *parts[4];
	int sensor_idx = -1;
	int count = split_string(config, " ", parts, 4);
	strncpy(mediainfo->encode_type, "h264", sizeof(mediainfo->encode_type));

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
				pipeline_info->pipe_contexts.sensor_config = vp_sensor_config_list[sensor_idx];
				printf("Using index:%d  sensor_name:%s  config_file:%s\n",
						sensor_idx,
						vp_sensor_config_list[sensor_idx]->sensor_name,
						vp_sensor_config_list[sensor_idx]->config_file);
			} else {
				printf("Unsupport sensor index:%d\n", sensor_idx);
				print_help();
				exit(0);
			}
			ret = vp_sensor_multi_fixed_mipi_host(pipeline_info->pipe_contexts.sensor_config, used_mipi_host,
					&pipeline_info->pipe_contexts.csi_config);
			if (ret < 0) {
				printf("vp sensor fixed mipi host fail, sensor id %d."
						"Maybe No Camera Sensor found. Please check if the specified "
						"sensor is connected to the Camera interface.\n\n", sensor_idx);
				exit(0);
			}
			pipeline_info->select_sensor_id = sensor_idx;
			// active_mipi_host 的配置在create_vin_node函数中需要再配置一下
			pipeline_info->active_mipi_host = pipeline_info->pipe_contexts.sensor_config->vin_node_attr->cim_attr.mipi_rx;
			used_mipi_host |= (1 << pipeline_info->pipe_contexts.sensor_config->vin_node_attr->cim_attr.mipi_rx);
		}
		for (int j = 0; j < kv_count; j++) {
			free(key_value[j]);
		}
	}

	// set default output file name
	sprintf(mediainfo->output_file, "pipeline%d_%dx%d_%dfps.%s",
			pipeline_idx, pipeline_info->pipe_contexts.sensor_config->camera_config->width,
			pipeline_info->pipe_contexts.sensor_config->camera_config->height,
			pipeline_info->pipe_contexts.sensor_config->camera_config->fps,
			mediainfo->encode_type);

	for (int i = 0; i < count; i++) {
		free(parts[i]);
	}
}

static int create_camera_node(pipe_contex_t *pipe_contex, uint32_t sensor_mode)
{
	camera_config_t *camera_config = NULL;
	vp_sensor_config_t *sensor_config = NULL;
	int32_t ret = 0;

	sensor_config = pipe_contex->sensor_config;
	camera_config = sensor_config->camera_config;
	if (sensor_mode >= NORMAL_M && sensor_mode < INVALID_MOD) {
		camera_config->sensor_mode = sensor_mode;
		sensor_config->vin_node_attr->lpwm_attr.enable = 1;
	}
	ret = hbn_camera_create(camera_config, &pipe_contex->cam_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int create_vin_node(pipe_contex_t *pipe_contex, int active_mipi_host) {
	vp_sensor_config_t *sensor_config = NULL;
	vin_node_attr_t *vin_node_attr = NULL;
	vin_ichn_attr_t *vin_ichn_attr = NULL;
	vin_ochn_attr_t *vin_ochn_attr = NULL;
	hbn_vnode_handle_t *vin_node_handle = NULL;
	vin_attr_ex_t vin_attr_ex;
	uint32_t hw_id = 0;
	int32_t ret = 0;
	uint32_t chn_id = 0;
	uint64_t vin_attr_ex_mask = 0;

	sensor_config = pipe_contex->sensor_config;
	vin_node_attr = sensor_config->vin_node_attr;
	vin_ichn_attr = sensor_config->vin_ichn_attr;
	vin_ochn_attr = sensor_config->vin_ochn_attr;
	// 调整 mipi_rx 的 index
	vin_node_attr->cim_attr.mipi_rx = active_mipi_host;
	hw_id = vin_node_attr->cim_attr.mipi_rx;
	vin_node_handle = &pipe_contex->vin_node_handle;

	if(pipe_contex->csi_config.mclk_is_not_configed){
		//设备树中没有配置mclk：使用外部晶振
		printf("csi%d ignore mclk ex attr, because not config mclk.\n",
				pipe_contex->csi_config.index);
	}else{
		vin_attr_ex.vin_attr_ex_mask = 0x80;	//bit7 for mclk
		vin_attr_ex.mclk_ex_attr.mclk_freq = 24000000; // 24MHz
	}

	ret = hbn_vnode_open(HB_VIN, hw_id, AUTO_ALLOC_ID, vin_node_handle);
	ERR_CON_EQ(ret, 0);
	// 设置基本属性
	ret = hbn_vnode_set_attr(*vin_node_handle, vin_node_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输入通道的属性
	ret = hbn_vnode_set_ichn_attr(*vin_node_handle, chn_id, vin_ichn_attr);
	ERR_CON_EQ(ret, 0);
	// 设置输出通道的属性
	ret = hbn_vnode_set_ochn_attr(*vin_node_handle, chn_id, vin_ochn_attr);
	ERR_CON_EQ(ret, 0);
	vin_attr_ex_mask = vin_attr_ex.vin_attr_ex_mask;
	if (vin_attr_ex_mask) {
		for (uint8_t i = 0; i < VIN_ATTR_EX_INVALID; i ++) {
			if ((vin_attr_ex_mask & (1 << i)) == 0)
				continue;
			vin_attr_ex.ex_attr_type = i;
			/*we need to set hbn_vnode_set_attr_ex in a loop*/
			ret = hbn_vnode_set_attr_ex(*vin_node_handle, &vin_attr_ex);
			ERR_CON_EQ(ret, 0);
		}
	}
	return 0;
}

static int create_isp_node(pipe_contex_t *pipe_contex) {
	vp_sensor_config_t *sensor_config = NULL;
	isp_attr_t      *isp_attr = NULL;
	isp_ichn_attr_t *isp_ichn_attr = NULL;
	isp_ochn_attr_t *isp_ochn_attr = NULL;
	hbn_vnode_handle_t *isp_node_handle = NULL;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	uint32_t chn_id = 0;
	int ret = 0;

	sensor_config = pipe_contex->sensor_config;
	isp_attr = sensor_config->isp_attr;
	isp_ichn_attr = sensor_config->isp_ichn_attr;
	isp_ochn_attr = sensor_config->isp_ochn_attr;
	isp_node_handle = &pipe_contex->isp_node_handle;

	ret = hbn_vnode_open(HB_ISP, 0, AUTO_ALLOC_ID, isp_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(*isp_node_handle, isp_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ochn_attr(*isp_node_handle, chn_id, isp_ochn_attr);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vnode_set_ichn_attr(*isp_node_handle, chn_id, isp_ichn_attr);
	ERR_CON_EQ(ret, 0);

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN
		| HB_MEM_USAGE_CPU_WRITE_OFTEN
		| HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(*isp_node_handle, chn_id, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

int init_windows(window_t *windows,uint32_t width,uint32_t height) {
	windows->strength = 1.0;       // Dimensionless non-negative parameter defining the strength of transformation along X axis
	windows->strengthY = 1.0;      // Dimensionless non-negative parameter defining the strength of transformation along X axis
	windows->angle = 0;            // Angle of main projection axis rotation around itself in degrees
	windows->elevation = 0;        // Angle in degrees which specify the main projection axis
	windows->azimuth = 0;          // Angle in degrees which specify the main projection axis, counted clockwise from North direction (positive to East)
	windows->keep_ratio = 1;       // Keep the same stretching strength in both horizontal and vertical directions
	windows->FOV_h = 90;           // Size of output field of view in vertical dimension in degrees
	windows->FOV_w = 90;           // Size of output field of view in horizontal dimension in degrees
	windows->cylindricity_y = 0;   // Level of cylindricity for target projection shape in vertical direction
	windows->cylindricity_x = 0;   // Level of cylindricity for target projection shape in horizontal direction
	windows->trapezoid_left_angle = 90;  //  Left Acute angle in degrees between trapezoid base and leg
	windows->trapezoid_right_angle = 90; //  Right Acute angle in degrees between trapezoid base and leg
	windows->pan = 0;             // Target shift in horizontal direction from centre of the output image in pixels
	windows->tilt = 0;            // Target shift in vertical direction from centre of the output image in pixels
	windows->zoom = 1.03;         // Target zoom dimensionless coefficient (must not be bigger than zero)

	// Output window position and size
	windows->out_r.x = 0;
	windows->out_r.y = 0;
	windows->out_r.w = width;
	windows->out_r.h = height;

	// input roi
	windows->input_roi_r.x = 0;
	windows->input_roi_r.y = 0;
	windows->input_roi_r.w = width;
	windows->input_roi_r.h = height;
	// The type of transformation applied to the image
	windows->transform = AFFINE;
	return 1;
}

static int32_t create_gdc_node(pipe_contex_t *pipe_contex,media_info_t *mediainfo)
{
	int ret = 0;
	uint32_t hw_id = 0;
	uint32_t chn_id = 0;
	gdc_attr_t gdc_attr = {0};
	gdc_ochn_attr_t gdc_ochn_attr = {0};
	uint32_t input_width = 0;
	uint32_t input_height = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};
	int64_t alloc_flags = 0;
	param_t gdc_param ={0};
	window_t windows ={0};
	uint32_t wnd_num = 1;
	uint64_t config_size = 0;
	uint32_t *cfg_buf_top = NULL;
	uint32_t *cfg_buf_bottom = NULL;
	int offset = 0;
	uint32_t getwidth=pipe_contex->sensor_config->isp_ichn_attr->width;
	uint32_t getheight=pipe_contex->sensor_config->isp_ichn_attr->height;

	init_windows(&windows,getwidth,getheight);
	memset(&gdc_param, 0, sizeof(gdc_param));

	gdc_param.in.w=getwidth;//input frame resolution
	gdc_param.in.h=getheight;
	gdc_param.out.w=getwidth;//output frame resolution
	gdc_param.out.h=getheight;
	gdc_param.fov=180;//定义摄像头或镜头的视场宽度
	gdc_param.diameter=2160;// 180 度视场的直径
	gdc_param.x_offset=0;//center offset for input x coordinate
	gdc_param.y_offset=0;//center offset for input y coordinate
	gdc_param.format=4;//FMT_SEMIPLANAR_420 frame format
	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &(pipe_contex->gdc_node_handle));
	if(ret != 0){
		printf("gdc  gdc_node_handle is valid, but open failed \n");
		return -1;
	}
	ret = hbn_gen_gdc_bin(&gdc_param, &windows, wnd_num, &cfg_buf_top, &config_size);
	if (ret != 0 || cfg_buf_top == NULL) {
		printf("hbn_gen_gdc_bin failed \n");
		return -1;
	}
	memset(&mediainfo->bin_buf_top, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(config_size, alloc_flags, &mediainfo->bin_buf_top);
	if (ret != 0 || mediainfo->bin_buf_top.virt_addr == NULL) {
		printf("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		return -1;
	}
	memcpy(mediainfo->bin_buf_top.virt_addr, cfg_buf_top, config_size);
	ret = hb_mem_flush_buf(mediainfo->bin_buf_top.fd, offset, config_size);
	ERR_CON_EQ(ret, 0);
	//Output window position adjustment
	windows.out_r.x=0;
	windows.out_r.y=getheight;
	ret = hbn_gen_gdc_bin(&gdc_param, &windows,wnd_num, &cfg_buf_bottom, &config_size);
	if (ret != 0 || cfg_buf_bottom == NULL) {
		printf("hbn_gen_gdc_bin failed \n");
		return -1;
	}
	memset(&mediainfo->bin_buf_bottom, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(config_size, alloc_flags, &mediainfo->bin_buf_bottom);
	if (ret != 0 || mediainfo->bin_buf_bottom.virt_addr == NULL) {
		printf("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		return -1;
	}
	memcpy(mediainfo->bin_buf_bottom.virt_addr, cfg_buf_bottom, config_size);
	ret = hb_mem_flush_buf(mediainfo->bin_buf_bottom.fd, offset, config_size);
	ERR_CON_EQ(ret, 0);

	gdc_attr.total_planes = 2;
	gdc_attr.div_width = 0;
	gdc_attr.div_height = 0;
	input_width = getwidth;
	input_height = getheight;

	ret = hbn_vnode_set_attr(pipe_contex->gdc_node_handle, &gdc_attr);
	if(ret != 0){
		printf("hbn_vnode_set_attrfailed \n");
		return -1;
	}

	gdc_ichn_attr_t gdc_ichn_attr = {0};
	gdc_ichn_attr.input_width = input_width;
	gdc_ichn_attr.input_height = input_height;
	gdc_ichn_attr.input_stride = input_width;
	gdc_ichn_attr.n_in_one = 2;
	ret = hbn_vnode_set_ichn_attr(pipe_contex->gdc_node_handle, chn_id, &gdc_ichn_attr);
	if(ret != 0){
		printf("hbn_vnode_set_ichn_attr failed \n");
		return -1;
	}

	gdc_ochn_attr.output_width = input_width;
	gdc_ochn_attr.output_height = input_height *2;
	gdc_ochn_attr.output_stride = input_width;
	ret = hbn_vnode_set_ochn_attr(pipe_contex->gdc_node_handle, chn_id, &gdc_ochn_attr);
	if(ret != 0){
		printf("hbn_vnode_set_ochn_attr\n");
		return -1;
	}
	alloc_attr.buffers_num = 4;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN |
		HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(pipe_contex->gdc_node_handle, chn_id, &alloc_attr);
	if(ret != 0){
		printf("hbn_vnode_set_ochn_buf_attr failed \n");
		return -1;
	}
	hbn_free_gdc_bin(cfg_buf_top);
	hbn_free_gdc_bin(cfg_buf_bottom);
	return 0;
}

int create_and_run_gdc_vflow(pipe_contex_t *pipe_contex, media_info_t *mediainfo) {
	int ret = 0;

	ret=create_gdc_node(pipe_contex,mediainfo);
	ERR_CON_EQ(ret, 0);
	mediainfo->gdc_node_handle = pipe_contex->gdc_node_handle;

	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
			pipe_contex->gdc_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return ret;
}

static int create_and_run_vflow(pipe_contex_t *pipe_contex,
		int active_mipi_host, uint32_t sensor_mode,media_info_t *mediainfo)
{
	int32_t ret = 0;

	// 创建pipeline中的每个node
	ret = create_camera_node(pipe_contex, sensor_mode);
	ERR_CON_EQ(ret, 0);
	ret = create_vin_node(pipe_contex, active_mipi_host);
	ERR_CON_EQ(ret, 0);
	ret = create_isp_node(pipe_contex);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vflow_create(&pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
			pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_add_vnode(pipe_contex->vflow_fd,
			pipe_contex->isp_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vflow_bind_vnode(pipe_contex->vflow_fd,
			pipe_contex->vin_node_handle,
			1,
			pipe_contex->isp_node_handle,
			0);
	ERR_CON_EQ(ret, 0);

	ret = hbn_camera_attach_to_vin(pipe_contex->cam_fd,
			pipe_contex->vin_node_handle);
	ERR_CON_EQ(ret, 0);
	ret = hbn_vflow_start(pipe_contex->vflow_fd);
	ERR_CON_EQ(ret, 0);

	return 0;
}


void *encode_isp_chn_data(void *media)
{
	media_info_t *media_info  = (media_info_t *)media;
	pipeline_info_t *pipeinfo0 = &media_info->pipeinfo[0];
	pipeline_info_t *pipeinfo1 = &media_info->pipeinfo[1];
	hb_mem_common_buf_t* gdc_bin_buf_top = &media_info->bin_buf_top;
	hb_mem_common_buf_t* gdc_bin_buf_bottom = &media_info->bin_buf_bottom;
	hbn_vnode_handle_t isp_node_handle0 = media_info->pipeinfo[0].pipe_contexts.isp_node_handle;
	hbn_vnode_handle_t isp_node_handle1 = media_info->pipeinfo[1].pipe_contexts.isp_node_handle;
	hbn_vnode_handle_t gdc_node_handle = media_info->gdc_node_handle;
	hbn_vnode_image_t isp_out_img_0 = {0}, isp_out_img_1 = {0}, out_img = {0};
	uint32_t count = 0;
	int ret = 0;
	media_codec_buffer_t input_buffer = {0};
	media_codec_buffer_t ouput_buffer = {0};
	media_codec_output_buffer_info_t info;
	media_codec_context_t *media_context = &media_info->media_context;
	gdc_attr_t gdc_attr = {0};
	uint8_t uuid[] = "dc45e9bd-e6d948b7-962cd820-d923eeef+SEI_D-Robotics";

	uint32_t length = sizeof(uuid)/sizeof(uuid[0]);
	ret = hb_mm_mc_insert_user_data(media_context, uuid, length);
	if (ret != 0) {
		printf("#### insert user data failed. ret(%d) ####\n", ret);
		return NULL;
	}

	FILE *fp_output = fopen(media_info->output_file, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file\n");
		return NULL;
	}
	ret = hbn_vnode_get_attr_ex(gdc_node_handle, &gdc_attr);
	if(ret != 0){
		printf("hbn_vnode_get_attr_ex gdc_node_handle error \n");
	}
	while (running) {

		ret = hbn_vnode_getframe(isp_node_handle0, 0, 8000, &isp_out_img_0);
		if (ret != 0) {
			printf("hbn_vnode_getframe isp failed, error code %d\n", ret);
			continue;
		}
		gdc_attr.config_addr = gdc_bin_buf_top->phys_addr;
		gdc_attr.config_size = gdc_bin_buf_top->size;
		gdc_attr.binary_ion_id = gdc_bin_buf_top->share_id;
		gdc_attr.binary_offset = gdc_bin_buf_top->offset;
		hbn_vnode_set_attr_ex(gdc_node_handle, &gdc_attr);
		ret = hbn_vnode_sendframe(gdc_node_handle, 0, &isp_out_img_0);  // send
		if (ret != 0) {
			printf("hbn_vnode_sendframe gdc_node_handle failed\n");
		}

		hbn_vnode_getframe(isp_node_handle1, 0, 8000, &isp_out_img_1);
		if(ret != HBN_STATUS_SUCESS){
			printf("hbn_vnode_getframe isp_node_handle1 error\n");
			continue;
		}
		gdc_attr.config_addr = gdc_bin_buf_bottom->phys_addr;
		gdc_attr.config_size = gdc_bin_buf_bottom->size;
		gdc_attr.binary_ion_id = gdc_bin_buf_bottom->share_id;
		gdc_attr.binary_offset = gdc_bin_buf_bottom->offset;
		hbn_vnode_set_attr_ex(gdc_node_handle, &gdc_attr);
		hbn_vnode_sendframe(gdc_node_handle, 0, &isp_out_img_1);  // send
		ret = hbn_vnode_getframe(gdc_node_handle, 0, 8000, &out_img);
		if(ret != HBN_STATUS_SUCESS){
			printf("hbn_vnode_getframe gdc_node failed\n");
			continue;
		}
		memset(&input_buffer, 0x00, sizeof(media_codec_buffer_t));
		ret = hb_mm_mc_dequeue_input_buffer(media_context, &input_buffer,
				2000);
		if (ret != 0) {
			printf("hb_mm_mc_dequeue_input_buffer failed\n");
			break;
		}
		int frame_width = out_img.buffer.width;
		int frame_height = out_img.buffer.height;
		memcpy(input_buffer.vframe_buf.vir_ptr[0], out_img.buffer.virt_addr[0],
				frame_width * frame_height * 3 / 2);
		ret = hb_mm_mc_queue_input_buffer(media_context, &input_buffer, 2000);
		if (ret != 0) {
			printf("hb_mm_mc_queue_input_buffer failed\n");
			break;
		}
		memset(&ouput_buffer, 0x0, sizeof(media_codec_buffer_t));
		memset(&info, 0x0, sizeof(media_codec_output_buffer_info_t));
		ret = hb_mm_mc_dequeue_output_buffer(media_context, &ouput_buffer,
				&info, 2000);
		if (ret != 0) {
			printf("hb_mm_mc_dequeue_output_buffer failed\n");
			break;
		}
		fwrite(ouput_buffer.vstream_buf.vir_ptr,
				ouput_buffer.vstream_buf.size, 1, fp_output);
		if (verbose_flag) {
			printf("Encodec idx: %d type:%d get stream size:%d\n",
					media_context->instance_index, media_context->codec_id, ouput_buffer.vstream_buf.size);
		}
		ret = hb_mm_mc_queue_output_buffer(media_context,
				&ouput_buffer, 2000);
		if (ret != 0) {
			printf("hb_mm_mc_queue_output_buffer failed\n");
			break;
		}
		hbn_vnode_releaseframe(isp_node_handle0, pipeinfo0->ispchn_id, &isp_out_img_0);
		hbn_vnode_releaseframe(isp_node_handle1, pipeinfo1->ispchn_id, &isp_out_img_1);
		hbn_vnode_releaseframe(media_info->gdc_node_handle, 0, &out_img);
		count++;
	}

	return NULL;
}

int main(int argc, char** argv) {
	int ret = 0;
	int c = 0;
	int index = -1;
	media_info_t media_info = {0};
	memset(&media_info,0,sizeof(media_info_t));

	if (argc <= 1) {
		print_help();
		return 0;
	}

	while ((c = getopt_long(argc, argv, "c:vh", long_options, NULL)) != -1) {
		switch (c) {
			case 'c':
				if (total_pipeline_num >= PIPE_NUM) {
					fprintf(stderr, "Too many configurations. Maximum allowed is %d.\n", PIPE_NUM);
					return 1;
				}
				parse_config(&media_info.pipeinfo[total_pipeline_num], optarg, total_pipeline_num,&media_info);
				total_pipeline_num++;
				break;
			case 'v':
				verbose_flag = 1;
				break;
			case 'h':
			default:
				print_help();
				return 0;
		}
	}
	printf("total_pipeline_num:%d\n",total_pipeline_num);
	// 处理后的参数在这里可以使用
	for (int i = 0; i < total_pipeline_num; i++) {
		printf("Pipeline index %d:\n", i);
		printf("\tSensor index: %d\n", media_info.pipeinfo[i].select_sensor_id);
		printf("\tSensor name: %s\n", media_info.pipeinfo[i].pipe_contexts.sensor_config->sensor_name);
		printf("\tActive mipi host: %d\n", media_info.pipeinfo[i].active_mipi_host);
		if(i == 1){
			printf("Encode type: %s\n", media_info.encode_type);
			printf("Output file: %s\n", media_info.output_file);
		}

	}

	printf("MIPI host: 0x%x\n", used_mipi_host);
	for (int i = 0; i < PIPE_NUM; i++) {
		if (used_mipi_host & (1 << i)) {
			printf("  Host %d: Used\n", i);
		}
	}
	printf("Verbose: %d\n", verbose_flag);

	hb_mem_module_open();
	ERR_CON_EQ(ret, 0);
	for (index = 0; index < total_pipeline_num; index++) {
		ret = create_and_run_vflow(&media_info.pipeinfo[index].pipe_contexts,
				media_info.pipeinfo[index].active_mipi_host,
				media_info.pipeinfo[index].sensor_mode,
				&media_info);
		if (ret != 0) {
			for (int j = 0; j < index; j++) {
				hbn_vflow_stop(media_info.pipeinfo[j].pipe_contexts.vflow_fd);
				hbn_vflow_destroy(media_info.pipeinfo[j].pipe_contexts.vflow_fd);
			}
			return 0;
		}
	}
	if(total_pipeline_num != PIPE_NUM){
		printf("Only Support %d line stitching,please try ./%s -c \"sensor=id\" -c \"sensor=id\"\n",PIPE_NUM,get_program_name());
		exit(-1);
	}
	ret=create_and_run_gdc_vflow(&media_info.pipeinfo[0].pipe_contexts,&media_info);
	ERR_CON_EQ(ret, 0);
	create_encodec(&media_info);
	running = 1;
	ret = pthread_create(&media_info.read_codec_thread, NULL, (void *)encode_isp_chn_data,
			(void *)&media_info);
	pthread_join(media_info.read_codec_thread, NULL);

	for (index = 0; index < total_pipeline_num; index++) {

		ret = hbn_vflow_stop(media_info.pipeinfo[index].pipe_contexts.vflow_fd);
		ERR_CON_EQ(ret, 0);
		hbn_vflow_destroy(media_info.pipeinfo[index].pipe_contexts.vflow_fd);
	}

	hb_mem_module_close();

	return 0;
}
