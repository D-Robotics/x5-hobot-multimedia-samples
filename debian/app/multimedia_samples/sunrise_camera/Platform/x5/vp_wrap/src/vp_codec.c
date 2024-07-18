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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "utils/utils_log.h"
#include "utils/mthread.h"

#include "vp_common.h"
#include "vp_wrap.h"
#include "vp_codec.h"

static void print_avpacket_info(const AVPacket *pkt) {
	printf("AVPacket Information:\n");
	printf("  pts: %ld\n", pkt->pts);
	printf("  dts: %ld\n", pkt->dts);
	printf("  duration: %ld\n", pkt->duration);
	printf("  size: %d\n", pkt->size);
	printf("  stream_index: %d\n", pkt->stream_index);
	printf("  flags: %d\n", pkt->flags);
	printf("  pos: %ld\n", pkt->pos);
	printf("  side_data_elems: %d\n", pkt->side_data_elems);
	printf("  duration_ts: %ld\n", pkt->duration);
	printf("  buf: %p\n", pkt->buf);
	printf("  side_data: %p\n", pkt->side_data);
	printf("  data: %p\n", pkt->data);
	printf("  size: %d\n", pkt->size);
}

// 获取系统运行时间，避免系统时间变化引起的时间不同步问题
int64_t get_current_time_us() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((int64_t)ts.tv_sec) * 1000000 + (ts.tv_nsec / 1000);
}

// AV_open_stream: 打开视频流并找到最佳视频流索引
// p_param: 视频解码工作函数参数
// p_avContext: 保存 AVFormatContext 指针的指针
// p_avpacket: AVPacket 结构指针
// 返回值: 返回找到的最佳视频流索引，如果失败返回 -1
int32_t AV_open_stream(vp_decode_param_t *p_param,
					   AVFormatContext **p_avContext, AVPacket *p_avpacket)
{
	int32_t ret = 0;
	uint8_t retry = 10;
	int32_t video_idx = -1;

	if (!p_param || !p_avContext || !p_avpacket)
		return -1;

	AVDictionary *option = NULL;

	// 设置 AVOption 字典
	av_dict_set(&option, "stimeout", "3000000", 0);
	av_dict_set(&option, "bufsize", "1024000", 0);
	av_dict_set(&option, "rtsp_transport", "tcp", 0);

	// 循环尝试打开视频流，最多重试 10 次
	do
	{
		ret = avformat_open_input(p_avContext, p_param->stream_path, 0, &option);
		if (ret != 0)
		{
			SC_LOGE("avformat_open_input: %d, retry\n", ret);
		}
	} while (--retry && ret != 0);

	if (!retry)
	{
		SC_LOGE("Failed to avformat open %s\n", p_param->stream_path);
		goto exit;
	}

	// 查找视频流的信息
	ret = avformat_find_stream_info(*p_avContext, 0);
	if (ret < 0)
	{
		SC_LOGE("avformat_find_stream_info failed\n");
		return -1;
	}

	// 查找最佳视频流索引
	video_idx = av_find_best_stream(*p_avContext, AVMEDIA_TYPE_VIDEO,
									-1, -1, NULL, 0);
	if (video_idx < 0)
	{
		SC_LOGE("av_find_best_stream failed, ret: %d\n", video_idx);
		return -1;
	}

	// 获取视频流的帧数
	p_param->frame_count = (*p_avContext)->streams[video_idx]->codec_info_nb_frames;

exit:
	return video_idx;
}

#define SET_BYTE(_p, _b) \
*_p++ = (unsigned char)_b;

#define SET_BUFFER(_p, _buf, _len) \
memcpy(_p, _buf, _len);        \
(_p) += (_len);

int32_t AV_build_dec_seq_header(uint8_t *pbHeader,
							const media_codec_id_t codec_id,
							const AVStream *st, int32_t *sizelength)
{
	AVCodecParameters *avc = st->codecpar;

	uint8_t *pbMetaData = avc->extradata;
	int32_t nMetaData = avc->extradata_size;
	uint8_t *p = pbMetaData;
	uint8_t *a = p + 4 - ((long)p & 3);
	uint8_t *t = pbHeader;
	int32_t size;
	int32_t sps, pps, i, nal;

	size = 0;
	*sizelength = 4; // default size length(in bytes) = 4
	if (codec_id == MEDIA_CODEC_ID_H264)
	{
		if (nMetaData > 1 && pbMetaData && pbMetaData[0] == 0x01)
		{
			// check mov/mo4 file format stream
			p += 4;
			*sizelength = (*p++ & 0x3) + 1;
			sps = (*p & 0x1f); // Number of sps
			p++;
			for (i = 0; i < sps; i++)
			{
				nal = (*p << 8) + *(p + 1) + 2;
				SET_BYTE(t, 0x00);
				SET_BYTE(t, 0x00);
				SET_BYTE(t, 0x00);
				SET_BYTE(t, 0x01);
				SET_BUFFER(t, p + 2, nal - 2);
				p += nal;
				size += (nal - 2 + 4); // 4 => length of start code to be inserted
			}

			pps = *(p++); // number of pps
			for (i = 0; i < pps; i++)
			{
				nal = (*p << 8) + *(p + 1) + 2;
				SET_BYTE(t, 0x00);
				SET_BYTE(t, 0x00);
				SET_BYTE(t, 0x00);
				SET_BYTE(t, 0x01);
				SET_BUFFER(t, p + 2, nal - 2);
				p += nal;
				size += (nal - 2 + 4); // 4 => length of start code to be inserted
			}
		}
		else if (nMetaData > 3)
		{
			size = -1; // return to meaning of invalid stream data;
			for (; p < a; p++)
			{
				if (p[0] == 0 && p[1] == 0 && p[2] == 1)
				{
					// find startcode
					size = avc->extradata_size;
					if (pbMetaData && 0x00 == pbMetaData[size - 1])
					{
						size -= 1;
					}
					if (!pbHeader || !pbMetaData)
						return 0;
					SET_BUFFER(pbHeader, pbMetaData, size);
					break;
				}
			}
		}
	}
	else if (codec_id == MEDIA_CODEC_ID_H265)
	{
		if (nMetaData > 1 && pbMetaData && pbMetaData[0] == 0x01)
		{
			static const int8_t nalu_header[4] = {0, 0, 0, 1};
			int32_t numOfArrays = 0;
			uint16_t numNalus = 0;
			uint16_t nalUnitLength = 0;
			uint32_t offset = 0;

			p += 21;
			*sizelength = (*p++ & 0x3) + 1;
			numOfArrays = *p++;

			while (numOfArrays--)
			{
				p++; // NAL type
				numNalus = (*p << 8) + *(p + 1);
				p += 2;
				for (i = 0; i < numNalus; i++)
				{
					nalUnitLength = (*p << 8) + *(p + 1);
					p += 2;
					// if(i == 0)
					{
						memcpy(pbHeader + offset, nalu_header, 4);
						offset += 4;
						memcpy(pbHeader + offset, p, nalUnitLength);
						offset += nalUnitLength;
					}
					p += nalUnitLength;
				}
			}

			size = offset;
		}
		else if (nMetaData > 3)
		{
			size = -1; // return to meaning of invalid stream data;

			for (; p < a; p++)
			{
				if (p[0] == 0 && p[1] == 0 && p[2] == 1) // find startcode
				{
					size = avc->extradata_size;
					if (!pbHeader || !pbMetaData)
						return 0;
					SET_BUFFER(pbHeader, pbMetaData, size);
					break;
				}
			}
		}
	}
	else
	{
		SET_BUFFER(pbHeader, pbMetaData, nMetaData);
		size = nMetaData;
	}

	return size;
}

static int32_t get_rc_params(media_codec_context_t *context,
			mc_rate_control_params_t *rc_params) {
	int32_t ret = 0;
	ret = hb_mm_mc_get_rate_control_config(context, rc_params);
	if (ret) {
		SC_LOGE("Failed to get rc params ret=0x%x\n", ret);
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
	SC_LOGD("params->bitstream_buf_size: %d", params->bitstream_buf_size);
	params->frame_buf_count = 5;
	params->external_frame_buf = false;
	params->bitstream_buf_count = 5;
	/* Hardware limitations of x5 wave521cl:
	 * - B-frame encoding is not supported.
	 * - Multi-frame reference is not supported.
	 * Therefore, GOP presets are restricted to 1 and 9.
	 */

	params->gop_params.decoding_refresh_type = 2;
	params->gop_params.gop_preset_idx = 9;

	params->rot_degree = MC_CCW_0;
	params->mir_direction = MC_DIRECTION_NONE;
	params->frame_cropping_flag = false;
	params->enable_user_pts = 1;
	switch (codec_type)
	{
	case MEDIA_CODEC_ID_H264:
		SC_LOGI("codec type is h264: frame size:%d  frame rate: %d", params->bitstream_buf_size, frame_rate);
		context->codec_id = MEDIA_CODEC_ID_H264;
		params->rc_params.mode = MC_AV_RC_MODE_H264CBR;
		get_rc_params(context, &params->rc_params);
		params->rc_params.h264_cbr_params.frame_rate = frame_rate;
		params->rc_params.h264_cbr_params.bit_rate = bit_rate;
		break;
	case MEDIA_CODEC_ID_H265:
		SC_LOGI("codec type is h265: frame size:%d  frame rate: %d", params->bitstream_buf_size, frame_rate);
		context->codec_id = MEDIA_CODEC_ID_H265;
		params->rc_params.mode = MC_AV_RC_MODE_H265CBR;
		get_rc_params(context, &params->rc_params);
		params->rc_params.h265_cbr_params.frame_rate = frame_rate;
		params->rc_params.h265_cbr_params.bit_rate = bit_rate;
		break;
	case MEDIA_CODEC_ID_MJPEG:
		SC_LOGI("codec type is mjpeg: frame size:%d  frame rate: %d", params->bitstream_buf_size, frame_rate);
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
		SC_LOGE("Not Support encoding type: %d!\n", codec_type);
		return -1;
	}

	return 0;
}

int32_t vp_decode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
	int32_t width, int32_t height)
{
	mc_video_codec_dec_params_t *params;
	context->encoder = false; // decoder output
	params = &context->video_dec_params;
	params->feed_mode = MC_FEEDING_MODE_FRAME_SIZE;
	params->pix_fmt = MC_PIXEL_FORMAT_NV12;
	params->bitstream_buf_size = (width * height * 3 / 2  + 0x3ff) & ~0x3ff;
	params->bitstream_buf_count = 6;
	params->frame_buf_count = 6;

	switch (codec_type)
	{
	case MEDIA_CODEC_ID_H264:
		context->codec_id = MEDIA_CODEC_ID_H264;
		params->h264_dec_config.bandwidth_Opt = true;
		params->h264_dec_config.reorder_enable = true;
		params->h264_dec_config.skip_mode = 0;
		break;
	case MEDIA_CODEC_ID_H265:
		context->codec_id = MEDIA_CODEC_ID_H265;
		params->h265_dec_config.bandwidth_Opt = true;
		params->h265_dec_config.reorder_enable = true;
		params->h265_dec_config.skip_mode = 0;
		params->h265_dec_config.cra_as_bla = false;
		params->h265_dec_config.dec_temporal_id_mode = 0;
		params->h265_dec_config.target_dec_temporal_id_plus1 = 0;
		break;
	case MEDIA_CODEC_ID_MJPEG:
		context->codec_id = MEDIA_CODEC_ID_MJPEG;
		params->mjpeg_dec_config.rot_degree = MC_CCW_0;
		params->mjpeg_dec_config.mir_direction = MC_DIRECTION_NONE;
		params->mjpeg_dec_config.frame_crop_enable = false;
		break;
	case MEDIA_CODEC_ID_JPEG:
		context->codec_id = MEDIA_CODEC_ID_JPEG;
		params->jpeg_dec_config.frame_crop_enable = 0;
		params->jpeg_dec_config.rot_degree = MC_CCW_0;
		params->jpeg_dec_config.mir_direction = MC_DIRECTION_NONE;
		params->mjpeg_dec_config.frame_crop_enable = false;
		break;
	default:
		SC_LOGE("Not Support decoding type: %d!\n",
				codec_type);
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
		SC_LOGE("hb_mm_mc_initialize failed.\n");
		return -1;
	}

	ret = hb_mm_mc_configure(context);
	if (0 != ret)
	{
		SC_LOGE("hb_mm_mc_configure failed.\n");
		hb_mm_mc_release(context);
		return -1;
	}
#if 0
	SC_LOGI("request idr header\n");
	ret = hb_mm_mc_request_idr_header(context, 1);
	if(ret != 0){
		SC_LOGE("request idr header faield %d!\n", ret);
	}

	SC_LOGI("enable idr frame\n");
	ret = hb_mm_mc_enable_idr_frame(context, true);
	if(ret != 0){
		SC_LOGE("enable idr frame faield %d!\n", ret);
	}
#endif

	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return 0;
}

int32_t vp_codec_deinit(media_codec_context_t *context)
{
	int32_t ret = 0;

	ret = hb_mm_mc_release(context);
	if (ret != 0)
	{
		SC_LOGE("Failed to hb_mm_mc_release ret = %d \n", ret);
		return -1;
	}

	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return 0;
}

int32_t vp_codec_start(media_codec_context_t *context)
{
	int32_t ret = 0;
	mc_av_codec_startup_params_t startup_params = {0};

	ret = hb_mm_mc_start(context, &startup_params);
	if (ret != 0)
	{
		SC_LOGE("%s:%d hb_mm_mc_start failed.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}

int32_t vp_codec_stop(media_codec_context_t *context)
{
	int32_t ret = 0;
	ret = hb_mm_mc_pause(context);
	if (ret != 0)
	{
		SC_LOGE("Failed to hb_mm_mc_pause ret = %d \n", ret);
		return -1;
	}

	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}
int32_t vp_codec_restart(media_codec_context_t *context)
{
	int32_t ret = 0;

	ret = hb_mm_mc_stop(context);
	if (ret != 0)
	{
		SC_LOGE("%s:%d Failed to hb_mm_mc_stop ret = %d \n",
				__FUNCTION__, __LINE__, ret);
		return -1;
	}
	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return 0;
}
void vp_codec_get_user_buffer_param(mc_video_codec_enc_params_t *enc_param, int *buffer_region_size, int *buffer_item_count){
	int bitrate_byte = enc_param->rc_params.h264_cbr_params.bit_rate * 1024 / 8; //bit_rate单位是kbps
	int frame_rate = enc_param->rc_params.h264_cbr_params.frame_rate;
	int min_buffer_region_size = enc_param->bitstream_buf_size;
	int min_buffer_item_count = 5;

	bitrate_byte = bitrate_byte + bitrate_byte / 5; // 添加余量
	if(bitrate_byte < min_buffer_region_size){
		*buffer_region_size = min_buffer_region_size;
		SC_LOGD("bit_rate %d too simal than min buffer region size %d, so use min buffer region size.",
			bitrate_byte, min_buffer_region_size);
	}else{
		*buffer_region_size = bitrate_byte;
	}

	if(frame_rate < min_buffer_item_count){
		*buffer_item_count = min_buffer_item_count;
		SC_LOGW("frame_rate %d too simal than min rate %d, so use min value.", frame_rate, min_buffer_item_count);
	}else{
		*buffer_item_count = frame_rate;
	}
}

// for debug
int32_t vp_codec_set_input_single_file(media_codec_context_t *context, ImageFrame *frame, int32_t eos)
{
	int32_t ret = 0;
	media_codec_buffer_t buffer = {0};

	memset(&buffer, 0x00, sizeof(media_codec_buffer_t));

	if ((context == NULL) || (frame == NULL))
	{
		SC_LOGE("codec param is NULL!\n");
		return -1;
	}

	buffer.type = (context->encoder) ? MC_VIDEO_FRAME_BUFFER : MC_VIDEO_STREAM_BUFFER;
	ret = hb_mm_mc_dequeue_input_buffer(context, &buffer, 2000);
	if (ret != 0)
	{
		SC_LOGE("hb_mm_mc_dequeue_input_buffer failed ret = %d", ret);
		return -1;
	}

	if (context->encoder == true)
	{
		buffer.type = MC_VIDEO_FRAME_BUFFER;
		buffer.vframe_buf.width = context->video_enc_params.width;
		buffer.vframe_buf.height = context->video_enc_params.height;
		buffer.vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
		// buffer.vframe_buf.size = frame->data_size[0];
		SC_LOGW("Buffer Type: %d", buffer.type);
		SC_LOGW("Width: %d", buffer.vframe_buf.width);
		SC_LOGW("Height: %d", buffer.vframe_buf.height);
		SC_LOGW("Pixel Format: %d", buffer.vframe_buf.pix_fmt);
		SC_LOGW("Size: %d", buffer.vframe_buf.size);
		SC_LOGW("plane_count: %d", frame->plane_count);

#define SINGLE_IMAGE_TEST 1
#ifdef SINGLE_IMAGE_TEST
	int img_in_fd = open("/tmp/1920x1080.yuv", O_RDWR | O_CREAT, 0666);
	if (img_in_fd < 0) {
		printf("open image failed !\n");
		return -1;
	}

	ssize_t bytes_read = read(img_in_fd, buffer.vframe_buf.vir_ptr[0], buffer.vframe_buf.size);
	if (bytes_read == -1) {
		printf("read image failed !\n");
		close(img_in_fd);
		return -1;
	}
	close(img_in_fd);
#endif

		// memcpy(buffer.vframe_buf.vir_ptr[0], frame->data[0], buffer.vframe_buf.size);
	}
	else
	{
		if (buffer.vstream_buf.size < frame->data_size[0])
		{
			SC_LOGE("The input stream/frame data is larger than the stream buffer size\n");
			hb_mm_mc_queue_input_buffer(context, &buffer, 3000);
			return -1;
		}

		buffer.type = MC_VIDEO_STREAM_BUFFER;
		if (eos == 0)
		{
			buffer.vstream_buf.size = frame->data_size[0];
			buffer.vstream_buf.stream_end = 0;
		}
		else
		{
			buffer.vstream_buf.size = 0;
			buffer.vstream_buf.stream_end = 1;
		}
		SC_LOGD("buffer.vstream_buf.size: %d", buffer.vstream_buf.size);
		SC_LOGD("buffer.vstream_buf.vir_ptr: %p", buffer.vstream_buf.vir_ptr);

		memcpy(buffer.vstream_buf.vir_ptr, frame->data[0], frame->data_size[0]);
	}

	ret = hb_mm_mc_queue_input_buffer(context, &buffer, 2000);
	if (ret != 0)
	{
		SC_LOGE("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
		return -1;
	}

	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}

int32_t vp_codec_set_input(media_codec_context_t *context, ImageFrame *frame, int32_t eos)
{
	int32_t ret = 0;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame == NULL))
	{
		SC_LOGE("codec param is NULL!\n");
		return -1;
	}
	buffer = (media_codec_buffer_t *)(frame->frame_buffer);

	buffer->type = (context->encoder) ? MC_VIDEO_FRAME_BUFFER : MC_VIDEO_STREAM_BUFFER;
	ret = hb_mm_mc_dequeue_input_buffer(context, buffer, 2000);
	if (ret != 0)
	{
		SC_LOGE("hb_mm_mc_dequeue_input_buffer failed ret = %d", ret);
		return -1;
	}

	if (context->encoder == true)
	{
		buffer->type = MC_VIDEO_FRAME_BUFFER;
		buffer->vframe_buf.width = context->video_enc_params.width;
		buffer->vframe_buf.height = context->video_enc_params.height;
		buffer->vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
		buffer->vframe_buf.size = frame->data_size[0];
		buffer->vframe_buf.pts = frame->image_timestamp;
		// SC_LOGW("Buffer Type: %d", buffer->type);
		// SC_LOGW("Width: %d", buffer->vframe_buf.width);
		// SC_LOGW("Height: %d", buffer->vframe_buf.height);
		// SC_LOGW("Pixel Format: %d", buffer->vframe_buf.pix_fmt);
		// SC_LOGW("Size: %d", buffer->vframe_buf.size);
		// SC_LOGW("plane_count: %d", frame->plane_count);
		// SC_LOGW("pts: %llu", buffer->vframe_buf.pts);

		if (buffer->vframe_buf.size != 0) {
			memcpy(buffer->vframe_buf.vir_ptr[0], frame->data[0], buffer->vframe_buf.size);
		} else {
			memcpy(buffer->vframe_buf.vir_ptr[0], frame->data[0], frame->data_size[0]);
			// char file_name[128];
			// hbn_vnode_image_t *hbn_vnode_image = (hbn_vnode_image_t *)frame->hbn_vnode_image;
			// sprintf(file_name, "/tmp/codec_%ux%u_%03d.yuv",
			// 					hbn_vnode_image->buffer.width,
			// 					hbn_vnode_image->buffer.height,
			// 					hbn_vnode_image->info.frame_id);
			// vp_dump_yuv_to_file(file_name, frame->data[0], frame->data_size[0]);
		}
	}
	else
	{
		if (buffer->vstream_buf.size < frame->data_size[0])
		{
			SC_LOGE("The input stream/frame data is larger than the stream buffer size\n");
			hb_mm_mc_queue_input_buffer(context, buffer, 3000);
			return -1;
		}

		buffer->type = MC_VIDEO_STREAM_BUFFER;
		if (eos == 0)
		{
			buffer->vstream_buf.size = frame->data_size[0];
			buffer->vstream_buf.stream_end = 0;
		}
		else
		{
			buffer->vstream_buf.size = 0;
			buffer->vstream_buf.stream_end = 1;
		}
		SC_LOGD("buffer->vstream_buf.size: %d", buffer->vstream_buf.size);
		SC_LOGD("buffer->vstream_buf.vir_ptr: %p", buffer->vstream_buf.vir_ptr);

		memcpy(buffer->vstream_buf.vir_ptr, frame->data[0], frame->data_size[0]);
	}

	ret = hb_mm_mc_queue_input_buffer(context, buffer, 2000);
	if (ret != 0)
	{
		SC_LOGE("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
		return -1;
	}

	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}

int32_t vp_codec_get_output(media_codec_context_t *context, ImageFrame *frame, int32_t timeout)
{
	int32_t ret = 0;
	media_codec_output_buffer_info_t *info = NULL;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame == NULL))
	{
		SC_LOGE("codec param is NULL");
		return -1;
	}
	buffer = (media_codec_buffer_t *)(frame->frame_buffer);
	info = (media_codec_output_buffer_info_t *)(frame->buffer_info);

	ret = hb_mm_mc_dequeue_output_buffer(context, buffer, info, timeout);
	if (ret != 0)
	{
		SC_LOGE("%s idx: %d, hb_mm_mc_dequeue_output_buffer failed ret = %d",
			context->encoder ? "Encode" : "Decode", context->instance_index, ret);
		return -1;
	}
	// 如果是解码器，拿到的 buffer 类型不是视频帧，说明解码器还没有完全工作起来，则返回错误
	if ((!context->encoder) && (buffer->type != MC_VIDEO_FRAME_BUFFER))
	{
		if (buffer != NULL)
		{
			ret = hb_mm_mc_queue_output_buffer(context, buffer, 0);
			if (ret != 0)
			{
				SC_LOGE("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
				return -1;
			}
		}
		return -1;
	}

	// if (log_ctrl_level_get(NULL) == LOG_DEBUG) {
	// 	vp_codec_print_media_codec_output_buffer_info(frame);
	// }

	if (context->codec_id >= MEDIA_CODEC_ID_H264 ||
		context->codec_id <= MEDIA_CODEC_ID_JPEG)
	{
		if (context->encoder)
		{
			frame->data[0] = (uint8_t *)(buffer->vstream_buf.vir_ptr);
			frame->frame_id = info->video_stream_info.frame_index;
			frame->image_timestamp = buffer->vstream_buf.pts;
			frame->data_size[0] = buffer->vstream_buf.size;
			frame->plane_count = 1;

			SC_LOGD("Encodec idx: %d type:%d get stream frame size:%d, buffer:%p",
				context->instance_index, context->codec_id, frame->data_size[0], buffer);
		}
		else
		{
			if (info->video_frame_info.decode_result == 0 || buffer->vframe_buf.size == 0) {
				if (buffer != NULL)
				{
					ret = hb_mm_mc_queue_output_buffer(context, buffer, 0);
					if (ret != 0)
					{
						SC_LOGE("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
						return -1;
					}
				}
				return -1;
			}
			frame->data[0] = (uint8_t *)(buffer->vframe_buf.vir_ptr[0]);
			frame->data_size[0] = buffer->vframe_buf.size;
			frame->plane_count = 1;

			// TODO： 这样设置pts可能会存在问题，但是为了与算法结果进行同步，先这么处理
			if (buffer->vframe_buf.pts <= 0) {
				frame->image_timestamp = get_current_time_us();
				buffer->vframe_buf.pts = frame->image_timestamp;
			} else {
				frame->image_timestamp = buffer->vframe_buf.pts;
			}

			// char file_name[128];
			// sprintf(file_name, "/tmp/codec%d_chn%d_%ux%u_%04d.yuv",
			// 					context->codec_id, context->instance_index,
			// 					info->video_frame_info.display_width,
			// 					info->video_frame_info.display_height,
			// 					info->video_frame_info.frame_display_index);
			// vp_dump_yuv_to_file(file_name, frame->data[0], frame->data_size[0]);

			SC_LOGD("Decodec idx: %d type:%d get frame size:%d",
				context->instance_index, context->codec_id, frame->data_size[0]);
		}
	}

	return ret;
}

int32_t vp_codec_release_output(media_codec_context_t *context, ImageFrame *frame)
{
	int32_t ret = 0;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame == NULL))
	{
		SC_LOGE("codec param is NULL!\n");
		return -1;
	}
	buffer = (media_codec_buffer_t *)(frame->frame_buffer);

	SC_LOGD("%s idx: %d type:%d, buffer:%p",
		context->encoder ? "Encode" : "Decode", context->instance_index, context->codec_id, buffer);
	// vp_codec_print_media_codec_output_buffer_info(frame);

	if (buffer != NULL)
	{
		ret = hb_mm_mc_queue_output_buffer(context, buffer, 0);
		if (ret != 0)
		{
			SC_LOGE("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
			return -1;
		}
	}else{
		SC_LOGW("idx: %d, hb_mm_mc_queue_output_buffer failed : buffer is null. \n", context->instance_index);
	}


	SC_LOGD("%s idx: %d, successful", context->encoder ? "Encode" : "Decode", context->instance_index);
	return ret;
}

void *vp_decode_work_func(void *param)
{
	tsThread *privThread = (tsThread*)param;
	vp_decode_param_t *decode_param = (vp_decode_param_t *)(privThread->pvThreadData);
	int32_t error = 0;
	AVFormatContext *avContext = NULL;
	AVPacket avpacket = {0};
	int32_t video_idx = -1;
	uint8_t *seqHeader = NULL;
	int32_t seqHeaderSize = 0;
	int32_t firstPacket = 1;
	bool eos = false;
	media_codec_context_t *context = NULL;
	ImageFrame frame = {0};

	frame.frame_buffer = (media_codec_buffer_t *)malloc(sizeof(media_codec_buffer_t));
	if (frame.frame_buffer == NULL)
	{
		SC_LOGE("malloc media_codec_buffer_t failed.");
		goto exit;
	}

	SC_LOGD("frame_buffer: %p", frame.frame_buffer);

	frame.buffer_info = (media_codec_output_buffer_info_t *)malloc(sizeof(media_codec_output_buffer_info_t));
	if (frame.buffer_info == NULL)
	{
		SC_LOGE("malloc media_codec_output_buffer_info_t failed.");
		free(frame.frame_buffer);
		goto exit;
	}

	mThreadSetName(privThread, __func__);

	SC_LOGD("stream_path: %s", decode_param->stream_path);

	context = decode_param->context;

	video_idx = AV_open_stream(decode_param, &avContext, &avpacket);
	if (video_idx < 0)
	{
		SC_LOGE("failed to AV_open_stream");
		goto err_av_open;
	}

	while(privThread->eState == E_THREAD_RUNNING) {
		mc_inter_status_t pstStatus;
		hb_mm_mc_get_status(context, &pstStatus);

		// wait for each frame for decoding
		usleep(30 * 1000);

		if (!avpacket.size)
		{
			error = av_read_frame(avContext, &avpacket);
		}

		if (error < 0)
		{
			if (error == AVERROR_EOF || avContext->pb->eof_reached == true)
			{
				SC_LOGI("No more input data available, avpacket.size: %d."
					" Re-cycling to send again.", avpacket.size);
				// if decode done, continue decode current file
				eos = false;
			}
			else
			{
				SC_LOGE("Failed to av_read_frame error(0x%08x)", error);
			}

			if (avContext)
			{
				avformat_close_input(&avContext);
			}
			if (decode_param->stream_path != NULL)
			{
				avContext = NULL;
				memset(&avpacket, 0, sizeof(avpacket));
				video_idx = AV_open_stream(decode_param, &avContext, &avpacket);
				if (video_idx < 0)
				{
					SC_LOGE("failed to AV_open_stream");
					goto err_av_open;
				}
			}
			else
			{
				eos = true;
			}
		}
		else
		{
			seqHeaderSize = 0;
			if (firstPacket)
			{
				AVCodecParameters *codec;
				int32_t retSize = 0;
				codec = avContext->streams[video_idx]->codecpar;
				seqHeader = (uint8_t *)calloc(1U, codec->extradata_size + 1024);
				if (seqHeader == NULL)
				{
					SC_LOGE("Failed to mallock seqHeader");
					eos = true;
					break;
				}

				seqHeaderSize = AV_build_dec_seq_header(seqHeader,
												context->codec_id,
												avContext->streams[video_idx], &retSize);
				if (seqHeaderSize < 0)
				{
					SC_LOGE("Failed to build seqHeader");
					eos = true;
					break;
				}
				firstPacket = 0;
			}
			if (avpacket.size <= context->video_dec_params.bitstream_buf_size)
			{
				if (seqHeaderSize)
				{
					frame.data[0] = (void *)seqHeader;
					frame.data_size[0] = seqHeaderSize;
					vp_codec_set_input(context, &frame, eos);
				}
				else
				{
					frame.data[0] = (void *)avpacket.data;
					frame.data_size[0] = avpacket.size;
					vp_codec_set_input(context, &frame, eos);
					if (log_ctrl_level_get(NULL) == LOG_TRACE) {
						print_avpacket_info(&avpacket);
						vp_codec_print_media_codec_output_buffer_info(&frame);
					}
					av_packet_unref(&avpacket);
					avpacket.size = 0;
				}
			}
			else
			{
				SC_LOGE("The external stream buffer is too small!"
						"avpacket.size:%d, buffer size:%d",
						avpacket.size,
						context->video_dec_params.bitstream_buf_size);
				eos = true;
				break;
			}

			if (seqHeader)
			{
				free(seqHeader);
				seqHeader = NULL;
			}
		}
	}

	if (eos)
	{
		frame.data[0] = (void *)seqHeader;
		frame.data_size[0] = seqHeaderSize;
		vp_codec_set_input(context, &frame, eos);
	}

	if (seqHeader)
	{
		free(seqHeader);
		seqHeader = NULL;
	}

err_av_open:
	if (avContext)
		avformat_close_input(&avContext);
exit:
	mThreadFinish(privThread);
	return NULL;
}

// 根据参数类型打印不同 union 的值的函数
void vp_codec_print_media_codec_output_buffer_info(ImageFrame *frame)
{
	media_codec_buffer_t *frame_buffer = (media_codec_buffer_t *)frame->frame_buffer;
	media_codec_output_buffer_info_t *buffer_info = (media_codec_output_buffer_info_t *)frame->buffer_info;
	switch (frame_buffer->type) {
		case MC_VIDEO_FRAME_BUFFER:
			printf("/**\n"
				"* Define the H264/H265 output frame information.\n"
				"*/\n");
			printf("typedef struct _mc_h264_h265_output_frame_info {\n");

			// 打印每个字段的名称和值
			printf("    decode_result: %d\n", buffer_info->video_frame_info.decode_result);
			printf("    frame_display_index: %d\n", buffer_info->video_frame_info.frame_display_index);
			printf("    frame_decoded_index: %d\n", buffer_info->video_frame_info.frame_decoded_index);
			printf("    stream_start_addr: %lu\n", buffer_info->video_frame_info.stream_start_addr);
			printf("    stream_size: %d\n", buffer_info->video_frame_info.stream_size);
			printf("    nalu_type: %d\n", buffer_info->video_frame_info.nalu_type);
			printf("    err_mb_in_frame_decoded: %d\n", buffer_info->video_frame_info.err_mb_in_frame_decoded);
			printf("    total_mb_in_frame_decoded: %d\n", buffer_info->video_frame_info.total_mb_in_frame_decoded);
			printf("    err_mb_in_frame_display: %d\n", buffer_info->video_frame_info.err_mb_in_frame_display);
			printf("    total_mb_in_frame_display: %d\n", buffer_info->video_frame_info.total_mb_in_frame_display);
			printf("    display_rect: (x=%d, y=%d, width=%d, height=%d)\n", buffer_info->video_frame_info.display_rect.x_pos, buffer_info->video_frame_info.display_rect.y_pos, buffer_info->video_frame_info.display_rect.width, buffer_info->video_frame_info.display_rect.height);
			printf("    display_width: %d\n", buffer_info->video_frame_info.display_width);
			printf("    display_height: %d\n", buffer_info->video_frame_info.display_height);
			printf("    decoded_rect: (x=%d, y=%d, width=%d, height=%d)\n", buffer_info->video_frame_info.decoded_rect.x_pos, buffer_info->video_frame_info.decoded_rect.y_pos, buffer_info->video_frame_info.decoded_rect.width, buffer_info->video_frame_info.decoded_rect.height);
			printf("    aspect_rate_info: %d\n", buffer_info->video_frame_info.aspect_rate_info);
			printf("    frame_rate_numerator: %d\n", buffer_info->video_frame_info.frame_rate_numerator);
			printf("    frame_rate_denominator: %d\n", buffer_info->video_frame_info.frame_rate_denominator);
			printf("    display_poc: %d\n", buffer_info->video_frame_info.display_poc);
			printf("    decoded_poc: %d\n", buffer_info->video_frame_info.decoded_poc);
			printf("    error_reason: %d\n", buffer_info->video_frame_info.error_reason);
			printf("    warn_info: %d\n", buffer_info->video_frame_info.warn_info);
			printf("    sequence_no: %u\n", buffer_info->video_frame_info.sequence_no);
			printf("    temporal_id: %d\n", buffer_info->video_frame_info.temporal_id);
			printf("    output_flag: %d\n", buffer_info->video_frame_info.output_flag);
			printf("    ctu_size: %d\n", buffer_info->video_frame_info.ctu_size);
			printf("} mc_h264_h265_output_frame_info_t;\n");

			printf("/**\n"
				"* Define the information of video frame buffer(H264/H265/MJPEG/JPEG).\n"
				"*/\n");
			printf("typedef struct _mc_video_frame_buffer_info {\n");
			// 打印每个字段的名称和值
			printf("    vir_ptr[0]: %p\n", frame_buffer->vframe_buf.vir_ptr[0]);
			printf("    vir_ptr[1]: %p\n", frame_buffer->vframe_buf.vir_ptr[1]);
			printf("    vir_ptr[2]: %p\n", frame_buffer->vframe_buf.vir_ptr[2]);
			printf("    phy_ptr[0]: %lu\n", frame_buffer->vframe_buf.phy_ptr[0]);
			printf("    phy_ptr[1]: %lu\n", frame_buffer->vframe_buf.phy_ptr[1]);
			printf("    phy_ptr[2]: %lu\n", frame_buffer->vframe_buf.phy_ptr[2]);
			printf("    size: %u\n", frame_buffer->vframe_buf.size);
			printf("    compSize[0]: %u\n", frame_buffer->vframe_buf.compSize[0]);
			printf("    compSize[1]: %u\n", frame_buffer->vframe_buf.compSize[1]);
			printf("    compSize[2]: %u\n", frame_buffer->vframe_buf.compSize[2]);
			printf("    width: %d\n", frame_buffer->vframe_buf.width);
			printf("    height: %d\n", frame_buffer->vframe_buf.height);
			printf("    pix_fmt: %d\n", frame_buffer->vframe_buf.pix_fmt);
			printf("    stride: %d\n", frame_buffer->vframe_buf.stride);
			printf("    vstride: %d\n", frame_buffer->vframe_buf.vstride);
			printf("    fd[0]: %d\n", frame_buffer->vframe_buf.fd[0]);
			printf("    fd[1]: %d\n", frame_buffer->vframe_buf.fd[1]);
			printf("    fd[2]: %d\n", frame_buffer->vframe_buf.fd[2]);
			printf("    pts: %lu\n", frame_buffer->vframe_buf.pts);
			printf("    src_idx: %d\n", frame_buffer->vframe_buf.src_idx);
			printf("    frame_end: %d\n", frame_buffer->vframe_buf.frame_end);
			printf("    qp_map_valid: %d\n", frame_buffer->vframe_buf.qp_map_valid);
			// printf("    qp_map_array: %u\n", frame_buffer->vframe_buf.qp_map_array);
			printf("    qp_map_array_count: %u\n", frame_buffer->vframe_buf.qp_map_array_count);
			printf("    flags: %d\n", frame_buffer->vframe_buf.flags);

			printf("} mc_video_frame_buffer_info_t;\n");
			break;
		case MC_VIDEO_STREAM_BUFFER:
			printf("/**\n"
				"*  Define the coded H264 stream information.\n"
				"*/\n");
			printf("typedef struct mc_h264_h265_output_stream_info_t {\n");
			printf("    frame_index: %d\n", buffer_info->video_stream_info.frame_index);
			printf("    frame_start_addr: %lu\n", buffer_info->video_stream_info.frame_start_addr);
			printf("    frame_size: %d\n", buffer_info->video_stream_info.frame_size);
			printf("    nalu_type: %d\n", buffer_info->video_stream_info.nalu_type);
			printf("    slice_idx: %u\n", buffer_info->video_stream_info.slice_idx);
			printf("    slice_num: %u\n", buffer_info->video_stream_info.slice_num);
			printf("    dependent_slice_num: %u\n", buffer_info->video_stream_info.dependent_slice_num);
			printf("    independent_slice_num: %u\n", buffer_info->video_stream_info.independent_slice_num);
			printf("    pic_skipped: %u\n", buffer_info->video_stream_info.pic_skipped);
			printf("    intra_block_num: %u\n", buffer_info->video_stream_info.intra_block_num);
			printf("    skip_block_num: %u\n", buffer_info->video_stream_info.skip_block_num);
			printf("    avg_mb_qp: %u\n", buffer_info->video_stream_info.avg_mb_qp);
			printf("    enc_pic_byte: %u\n", buffer_info->video_stream_info.enc_pic_byte);
			printf("    enc_gop_pic_idx: %d\n", buffer_info->video_stream_info.enc_gop_pic_idx);
			printf("    enc_pic_poc: %d\n", buffer_info->video_stream_info.enc_pic_poc);
			printf("    enc_src_idx: %u\n", buffer_info->video_stream_info.enc_src_idx);
			printf("    enc_pic_cnt: %u\n", buffer_info->video_stream_info.enc_pic_cnt);
			printf("    enc_error_reason: %d\n", buffer_info->video_stream_info.enc_error_reason);
			printf("    enc_warn_info: %d\n", buffer_info->video_stream_info.enc_warn_info);
			printf("    frame_cycle: %u\n", buffer_info->video_stream_info.frame_cycle);
			printf("    temporal_id: %u\n", buffer_info->video_stream_info.temporal_id);
			printf("    longterm_ref_type: %u\n", buffer_info->video_stream_info.longterm_ref_type);
			printf("} mc_h264_h265_output_stream_info_t;\n");
			printf("/**\n"
				"* Define the information of video stream buffers(H264/H265/MJPEG/JPEG).\n"
				"*/\n");
			printf("typedef struct mc_video_stream_buffer_info_t {\n");
			printf("    vir_ptr: %p\n", frame_buffer->vstream_buf.vir_ptr);
			printf("    phy_ptr: %lu\n", frame_buffer->vstream_buf.phy_ptr);
			printf("    size: %u\n", frame_buffer->vstream_buf.size);
			printf("    pts: %lu\n", frame_buffer->vstream_buf.pts);
			printf("    fd: %d\n", frame_buffer->vstream_buf.fd);
			printf("    src_idx: %d\n", frame_buffer->vstream_buf.src_idx);
			printf("    stream_end: %d\n", frame_buffer->vstream_buf.stream_end);
			printf("} mc_video_stream_buffer_info_t;\n");
			break;
		case MC_AUDIO_FRAME_BUFFER:
			printf("Audio Frame Info:\n");
			// 打印 video_stream_info 的值
			// 注意：这里省略了打印 video_stream_info 的代码
			break;
		case MC_AUDIO_STREAM_BUFFER:
			printf("Audio Stream Info:\n");
			// 打印 video_stream_info 的值
			// 注意：这里省略了打印 video_stream_info 的代码
			break;
		default:
			printf("Unsupported Codec Buffer Type.\n");
			break;
	}
}
