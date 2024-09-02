/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-02-23 13:47:14
 * @LastEditTime: 2023-03-05 15:55:29
 ***************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "vp_codec.h"
#include "common_utils.h"
static int verbose = 0;

static int32_t get_rc_params(media_codec_context_t *context,
			mc_rate_control_params_t *rc_params) {
	int32_t ret = 0;
	ret = hb_mm_mc_get_rate_control_config(context, rc_params);
	if (ret) {
		printf("Failed to get rc params ret=0x%x\n", ret);
		return ret;
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

int32_t vp_encode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
	int32_t width, int32_t height, int32_t frame_rate, uint32_t bit_rate)
{
	mc_video_codec_enc_params_t *params;

	memset(context, 0x00, sizeof(media_codec_context_t));
	context->encoder = true;
	params = &context->video_enc_params;
	params->width = width;
	params->height = height;
	params->pix_fmt = MC_PIXEL_FORMAT_NV12;
	params->bitstream_buf_size = (width * height * 3 / 2  + 0x3ff) & ~0x3ff;
	params->frame_buf_count = 3;
	params->external_frame_buf = false;
	params->bitstream_buf_count = 3;
	/* Hardware limitations of x5 wave521cl:
	 * - B-frame encoding is not supported.
	 * - Multi-frame reference is not supported.
	 * Therefore, GOP presets are restricted to 1 and 9.
	 */
	params->gop_params.gop_preset_idx = 1;
	params->rot_degree = MC_CCW_0;
	params->mir_direction = MC_DIRECTION_NONE;
	params->frame_cropping_flag = false;
	params->enable_user_pts = 1;
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
		params->bitstream_buf_size = (width * height * 3 / 2  + 0xfff) & ~0xfff;
		break;
	case MEDIA_CODEC_ID_JPEG:
		context->codec_id = MEDIA_CODEC_ID_JPEG;
		params->jpeg_enc_config.quality_factor = 50;
		params->mjpeg_enc_config.restart_interval = width / 16;
		params->bitstream_buf_size = (width * height * 3 / 2  + 0xfff) & ~0xfff;
		break;
	default:
		printf("Not Support encoding type: %d!\n", codec_type);
		return -1;
	}

	return 0;
}


int32_t vp_codec_init(media_codec_context_t *context)
{
	int32_t ret = 0;

	ret = hb_mm_mc_initialize(context);
	if (0 != ret)
	{
		printf("hb_mm_mc_initialize failed.\n");
		return -1;
	}

	ret = hb_mm_mc_configure(context);
	if (0 != ret)
	{
		printf("hb_mm_mc_configure failed.\n");
		hb_mm_mc_release(context);
		return -1;
	}

	printf("%s idx: %d, init successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	return 0;
}

int32_t vp_codec_deinit(media_codec_context_t *context)
{
	int32_t ret = 0;

	ret = hb_mm_mc_release(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_release ret = %d \n", ret);
		return -1;
	}

	printf("%s idx: %d, deinit successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	return 0;
}

int32_t vp_codec_start(media_codec_context_t *context)
{
	int32_t ret = 0;
	mc_av_codec_startup_params_t startup_params = {0};

	ret = hb_mm_mc_start(context, &startup_params);
	if (ret != 0)
	{
		printf("%s:%d hb_mm_mc_start failed.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	printf("%s idx: %d, start successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}

int32_t vp_codec_stop(media_codec_context_t *context)
{
	int32_t ret = 0;
	ret = hb_mm_mc_pause(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_pause ret = %d \n", ret);
		return -1;
	}

	printf("%s idx: %d, stop successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}
int32_t vp_codec_restart(media_codec_context_t *context)
{
	int32_t ret = 0;

	ret = hb_mm_mc_stop(context);
	if (ret != 0)
	{
		printf("%s:%d Failed to hb_mm_mc_stop ret = %d \n",
				__FUNCTION__, __LINE__, ret);
		return -1;
	}
	printf("%s idx: %d, restart successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	return 0;
}

int32_t vp_codec_set_input(media_codec_context_t *context, media_codec_buffer_t *frame_buffer, uint8_t *data, uint32_t data_size, int32_t eos)
{
	int32_t ret = 0;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame_buffer == NULL) || (data == NULL))
	{
		printf("codec param is NULL!\n");
		return -1;
	}

	buffer = frame_buffer;

	buffer->type = (context->encoder) ? MC_VIDEO_FRAME_BUFFER : MC_VIDEO_STREAM_BUFFER;
	ret = hb_mm_mc_dequeue_input_buffer(context, buffer, 2000);
	if (ret != 0)
	{
		printf("hb_mm_mc_dequeue_input_buffer failed ret = %d\n", ret);
		return -1;
	}

	if (context->encoder == false)
	{
		if (buffer->vstream_buf.size < data_size)
		{
			printf("The input stream/frame data is larger than the stream buffer size\n");
			hb_mm_mc_queue_input_buffer(context, buffer, 3000);
			return -1;
		}

		buffer->type = MC_VIDEO_STREAM_BUFFER;
		if (eos == 0)
		{
			buffer->vstream_buf.size = data_size;
			buffer->vstream_buf.stream_end = 0;
		}
		else
		{
			buffer->vstream_buf.size = 0;
			buffer->vstream_buf.stream_end = 1;
		}
		if (verbose) {
			printf("buffer->vstream_buf.size: %d\n", buffer->vstream_buf.size);
			printf("buffer->vstream_buf.vir_ptr: %p\n", buffer->vstream_buf.vir_ptr);
		}

		memcpy(buffer->vstream_buf.vir_ptr, data, data_size);
	}

	ret = hb_mm_mc_queue_input_buffer(context, buffer, 2000);
	if (ret != 0)
	{
		printf("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
		return -1;
	}

	if (verbose)
		printf("%s idx: %d, set input successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}

int32_t vp_codec_get_output(media_codec_context_t *context, media_codec_buffer_t *frame_buffer, media_codec_output_buffer_info_t *buffer_info, int32_t timeout)
{
	int32_t ret = 0;
	media_codec_output_buffer_info_t *info = NULL;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame_buffer == NULL) || (buffer_info == NULL))
	{
		printf("codec param is NULL\n");
		return -1;
	}
	buffer = frame_buffer;
	info = buffer_info;

	ret = hb_mm_mc_dequeue_output_buffer(context, buffer, info, timeout);
	if (ret != 0 && ret != -268435443)  // Check for timeout error
	{
		printf("%s idx: %d, %s ret = %d\n",
			context->encoder ? "Encode" : "Decode", context->instance_index,
			ret == -1 ? "hb_mm_mc_dequeue_output_buffer failed" : "hb_mm_mc_dequeue_output_buffer encountered an error",
			ret);
		return -1;
	}
	else if (ret == -268435443)
	{
		printf("%s idx: %d, %s\n",
			context->encoder ? "Encode" : "Decode", context->instance_index,
			"hb_mm_mc_dequeue_output_buffer timed out (possibly normal exit due to lack of data)");
	}
	if ((!context->encoder) && (buffer->type != MC_VIDEO_FRAME_BUFFER))
	{
		if (buffer != NULL)
		{
			ret = hb_mm_mc_queue_output_buffer(context, buffer, 0);
			if (ret != 0)
			{
				printf("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
				return -1;
			}
		}
		return -1;
	}

	if (context->codec_id >= MEDIA_CODEC_ID_H264 ||
		context->codec_id <= MEDIA_CODEC_ID_JPEG)
	{
		if (context->encoder == 0) // decoder
		{
			if (info->video_frame_info.decode_result == 0 || buffer->vframe_buf.size == 0)
			{
				if (buffer != NULL)
				{
					ret = hb_mm_mc_queue_output_buffer(context, buffer, 0);
					if (ret != 0)
					{
						printf("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d\n", context->instance_index, ret);
						return -1;
					}
				}
				return -1;
			}

			if (verbose) {
				printf("Decodec idx: %d type:%d get frame size:%d\n",
					context->instance_index, context->codec_id, buffer->vframe_buf.size);
			}
		}
	}

	return ret;
}

int32_t vp_codec_release_output(media_codec_context_t *context, media_codec_buffer_t *frame_buffer)
{
	int32_t ret = 0;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame_buffer == NULL))
	{
		printf("codec param is NULL!\n");
		return -1;
	}
	buffer = frame_buffer;

	if (verbose) {
		printf("%s idx: %d type:%d, buffer:%p\n",
			context->encoder ? "Encode" : "Decode", context->instance_index, context->codec_id, buffer);
	}

	if (buffer != NULL)
	{
		ret = hb_mm_mc_queue_output_buffer(context, buffer, 0);
		if (ret != 0)
		{
			printf("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
			return -1;
		}
	}

	return ret;
}
int vp_codec_encoder_create_and_start(media_codec_context_t *media_context, camera_config_info_t *camera_config_info)
{
	int ret = 0;
	int encode_width = camera_config_info->width;
	int encode_height = camera_config_info->height;
	int encode_fps = camera_config_info->fps;
	mc_av_codec_startup_params_t startup_params = {0};

	ret = vp_encode_config_param(media_context, camera_config_info->encode_type,
								encode_width, encode_height,
								encode_fps, 8192);
	ERR_CON_EQ(ret, 0);

	ret = hb_mm_mc_initialize(media_context);
	ERR_CON_EQ(ret, 0);

	ret = hb_mm_mc_configure(media_context);
	ERR_CON_EQ(ret, 0);

	ret = hb_mm_mc_start(media_context, &startup_params);
	ERR_CON_EQ(ret, 0);

	printf("Create %s idx: %d, init successful\n",
			media_context->encoder ? "Encode" : "Decode",
			media_context->instance_index);
	return 0;
}

int vp_codec_encoder_destroy_and_stop(media_codec_context_t *media_context)
{
	int ret = 0;
	ret = hb_mm_mc_stop(media_context);
	ERR_CON_EQ(ret, 0);
	ret = hb_mm_mc_release(media_context);
	ERR_CON_EQ(ret, 0);

	return ret;
}