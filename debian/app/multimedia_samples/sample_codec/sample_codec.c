/***************************************************************************
 * COPYRIGHT NOTICE
 * Copyright 2024 D-Robotics, Inc.
 * All rights reserved.
 ***************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"

#include "common_utils.h"
#include "sample_codec.h"

atomic_int running = 1;
static int verbose = 0;
static int decode_output_exit = 0;

static struct option const long_options[] = {
	{"config_file", required_argument, NULL, 'f'},
	{"encode", required_argument, NULL, 'e'},
	{"decode", required_argument, NULL, 'd'},
	{"verbose", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void print_help() {
	printf("Usage: sample_codec -f config_file [-e encode_option] [-d decode_option] [-v] [-h]\n");
	printf("Options:\n");
	printf("  -f, --config_file FILE     Set the configuration file\n");
	printf("  -e, --encode [OPTION]      Set the encoding option (optional), override encode_streams option\n");
	printf("  -d, --decode [OPTION]      Set the decoding option (optional), override decode_streams option\n");
	printf("  -v, --verbose              Enable verbose mode\n");
	printf("  -h, --help                 Print this help message\n");
	printf("\n");
	printf("Examples:\n");
	printf("  Start codec video with codec_config.ini:\n");
	printf("    sample_codec\n");
	printf("\n");
	printf("  Start the specified encoding stream in codec_config.ini:\n");
	printf("    sample_codec -e 0x1  -- Start the venc_stream1\n");
	printf("    sample_codec -e 0x3  -- Start the venc_stream1 and venc_stream2\n");
	printf("\n");
	printf("  Start the specified decoding stream in codec_config.ini:\n");
	printf("    sample_codec -d 0x1  -- Start the vdec_stream1\n");
	printf("    sample_codec -d 0x3  -- Start the vdec_stream1 and vdec_stream2\n");
	printf("\n");
	printf("  Enable verbose mode for detailed logging:\n");
	printf("    sample_codec -v\n");
	printf("\n");
	printf("  Display this help message:\n");
	printf("    sample_codec -h\n");
}

static void print_encode_params(EncodeParams *params) {
	printf("Encode params...\n codec_type: %d, width: %d, height: %d, frame_rate: %d, "
			"bit_rate: %u, input_file: %s, output_file: %s, frame_num: %d, profile: %s, "
			"external_buffer: %d performance_test:%d\n",
			params->codec_type, params->width, params->height, params->frame_rate,
			params->bit_rate, params->input, params->output, params->frame_num,
			params->profile, params->external_buffer, params->performance_test);
}

static void print_decode_params(DecodeParams *params) {
	printf("Decode params...\n codec_type: %d, width: %d, height: %d, input_file: %s, output_file: %s\n",
			params->codec_type, params->width, params->height, params->input, params->output);
}
// Define JPEG start and end markers
#define JPEG_START_MARKER 0xFFD8
#define JPEG_END_MARKER 0xFFD9

// Macro to combine two bytes into a 16-bit value
#define MAKEWORD(a, b) ((uint16_t)(((a) << 8) | (b)))

// Function to read the next JPEG image from the file and return its data and size
int extract_jpeg(FILE *file, uint8_t **image_data, size_t *image_size) {
	uint8_t buffer[1024];
	size_t bytes_read;
	int in_jpeg = 0;
	size_t jpeg_size = 0;
	size_t jpeg_capacity = 1024;
	uint8_t *jpeg_data = (uint8_t *)malloc(jpeg_capacity);
	if (!jpeg_data) {
		perror("Unable to allocate memory");
		return -1;
	}

	while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0 || !feof(file)) {
		for (size_t i = 0; i < bytes_read; i++) {
			// Check for JPEG start marker
			if (i < bytes_read - 1 && MAKEWORD(buffer[i], buffer[i + 1]) == JPEG_START_MARKER) {
				in_jpeg = 1;
				jpeg_size = 0; // Reset JPEG size for new image
			}

			// Write data to the current JPEG buffer
			if (in_jpeg) {
				if (jpeg_size >= jpeg_capacity) {
					jpeg_capacity *= 2;
					jpeg_data = (uint8_t *)realloc(jpeg_data, jpeg_capacity);
					if (!jpeg_data) {
						perror("Unable to reallocate memory");
						return -1;
					}
				}
				jpeg_data[jpeg_size++] = buffer[i];
			}

			// Check for JPEG end marker
			if (i > 0 && MAKEWORD(buffer[i - 1], buffer[i]) == JPEG_END_MARKER && in_jpeg) {
				*image_data = jpeg_data;
				*image_size = jpeg_size;

				// Reset file position to start of the next JPEG
				fseek(file, -(long)(bytes_read - i - 1), SEEK_CUR);
				return 0;
			}
		}
		if (feof(file)) {
			if (jpeg_data)
				free(jpeg_data);
			return -1;
		}
	}

	free(jpeg_data);
	return -1;
}


// av_open_stream: 打开视频流并找到最佳视频流索引
// p_param: 视频解码工作函数参数
// p_avContext: 保存 AVFormatContext 指针的指针
// p_avpacket: AVPacket 结构指针
// 返回值: 返回找到的最佳视频流索引，如果失败返回 -1
int32_t av_open_stream(DecodeParams *p_param,
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
	av_dict_set(&option, "probesize", "10000000", 0); // 设置 probesize 为 10M

	// 循环尝试打开视频流，最多重试 10 次
	do
	{
		ret = avformat_open_input(p_avContext, p_param->input, 0, &option);
		if (ret != 0)
		{
			printf("avformat_open_input: %d, retry\n", ret);
		}
	} while (--retry && ret != 0);

	if (!retry)
	{
		printf("Failed to avformat open %s\n", p_param->input);
		goto exit;
	}

	// 查找视频流的信息
	ret = avformat_find_stream_info(*p_avContext, 0);
	if (ret < 0)
	{
		printf("avformat_find_stream_info failed\n");
		return -1;
	}

	// 查找最佳视频流索引
	video_idx = av_find_best_stream(*p_avContext, AVMEDIA_TYPE_VIDEO,
									-1, -1, NULL, 0);
	if (video_idx < 0)
	{
		printf("av_find_best_stream failed, ret: %d\n", video_idx);
		return -1;
	}

	// 获取视频流的帧数
	p_param->frame_num = (*p_avContext)->streams[video_idx]->codec_info_nb_frames;
	printf("p_param->frame_num: %d\n", p_param->frame_num);

exit:
	return video_idx;
}

#define SET_BYTE(_p, _b) \
	*_p++ = (unsigned char)_b;

#define SET_BUFFER(_p, _buf, _len) \
	memcpy(_p, _buf, _len); \
	(_p) += (_len);

int32_t av_build_dec_seq_header(uint8_t *pbHeader,
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

static int32_t h264_parse_profile(const char *str_profile, H264Profile *h264_profile)
{
	char *s, *stringp, *profile, *level;
	if (str_profile == NULL || h264_profile == NULL || strlen(str_profile) == 0)
		return -EINVAL;

	stringp = strdup(str_profile);
	s = stringp;

	if (!stringp)
		return -EINVAL;

	/*
	 * format: profile@level%tier
	 * example:
	 *	h264_main@L4
	 */
	profile = strsep(&s, "@");
	level = s;
	if (!profile || ! level)
		return -EINVAL;

	if (strcmp(profile, "h264_main") == 0)
		h264_profile->profile = MC_H264_PROFILE_MP;
	else if (strcmp(profile, "h264_baseline") == 0)
		h264_profile->profile = MC_H264_PROFILE_BP;
	else if (strcmp(profile, "h264_high") == 0)
		h264_profile->profile = MC_H264_PROFILE_HP;
	else if (strcmp(profile, "h264_high10") == 0)
		h264_profile->profile = MC_H264_PROFILE_HIGH10;
	else if (strcmp(profile, "h264_high422") == 0)
		h264_profile->profile = MC_H264_PROFILE_HIGH422;
	else if (strcmp(profile, "h264_high444") == 0)
		h264_profile->profile = MC_H264_PROFILE_HIGH444;
	else if (strcmp(profile, "h264_extended") == 0)
		h264_profile->profile = MC_H264_PROFILE_EXTENDED;
	else
		h264_profile->profile = MC_H264_PROFILE_UNSPECIFIED;

	if (strcmp(level, "L1") == 0)
		h264_profile->level = MC_H264_LEVEL1;
	else if (strcmp(level, "L1B") == 0)
		h264_profile->level = MC_H264_LEVEL1b;
	else if (strcmp(level, "L1_1") == 0)
		h264_profile->level = MC_H264_LEVEL1_1;
	else if (strcmp(level, "L1_2") == 0)
		h264_profile->level = MC_H264_LEVEL1_2;
	else if (strcmp(level, "L1_3") == 0)
		h264_profile->level = MC_H264_LEVEL1_3;
	else if (strcmp(level, "L2") == 0)
		h264_profile->level = MC_H264_LEVEL2;
	else if (strcmp(level, "L2_1") == 0)
		h264_profile->level = MC_H264_LEVEL2_1;
	else if (strcmp(level, "L3") == 0)
		h264_profile->level = MC_H264_LEVEL3;
	else if (strcmp(level, "L3_1") == 0)
		h264_profile->level = MC_H264_LEVEL3_1;
	else if (strcmp(level, "L3_2") ==0 )
		h264_profile->level = MC_H264_LEVEL3_2;
	else if (strcmp(level, "L4") == 0)
		h264_profile->level = MC_H264_LEVEL4;
	else if (strcmp(level, "L4_1") == 0)
		h264_profile->level = MC_H264_LEVEL4_1;
	else if (strcmp(level, "L4_2") == 0)
		h264_profile->level = MC_H264_LEVEL4_2;
	else if (strcmp(level, "L5") == 0)
		h264_profile->level = MC_H264_LEVEL5;
	else if (strcmp(level, "L5_1") == 0)
		h264_profile->level = MC_H264_LEVEL5_1;
	else if (strcmp(level, "L5_2") == 0)
		h264_profile->level = MC_H264_LEVEL5_2;
	else
		h264_profile->level = MC_H264_LEVEL_UNSPECIFIED;

	free(stringp);

	return 0;
}

static int32_t h265_parse_profile(const char *str_profile, H265Profile *h265_profile)
{
	char *s, *stringp, *profile, *level, *tier;
	if (str_profile == NULL || h265_profile == NULL || strlen(str_profile) == 0)
		return -EINVAL;

	stringp = strdup(str_profile);
	s = stringp;

	if (!stringp)
		return -EINVAL;

	/*
	 * format: profile@level%tier
	 * example:
	 *	h265_main@L4%high_tier
	 */
	profile = strsep(&s, "@");
	level = strsep(&s, "%");
	tier = s;

	if (!profile || !level)
		return -EINVAL;

	if (strcmp(profile, "h265_main") == 0)
		h265_profile->main_still_picture_profile_enable = 0;
	else if (strcmp(profile, "h265_main_still") == 0)
		h265_profile->main_still_picture_profile_enable = 1;
	else
		h265_profile->main_still_picture_profile_enable = 0;

	if (strcmp(level, "L1") == 0)
		h265_profile->level = MC_H265_LEVEL1;
	else if (strcmp(level, "L2") == 0)
		h265_profile->level = MC_H265_LEVEL2;
	else if (strcmp(level, "L2_1") == 0)
		h265_profile->level = MC_H265_LEVEL2_1;
	else if (strcmp(level, "L3") == 0)
		h265_profile->level = MC_H265_LEVEL3;
	else if (strcmp(level, "L3_1") == 0)
		h265_profile->level = MC_H265_LEVEL3_1;
	else if (strcmp(level, "L4") == 0)
		h265_profile->level = MC_H265_LEVEL4;
	else if (strcmp(level, "L4_1") == 0)
		h265_profile->level = MC_H265_LEVEL4_1;
	else if (strcmp(level, "L5") == 0)
		h265_profile->level = MC_H265_LEVEL5;
	else if (strcmp(level, "L5_1") == 0)
		h265_profile->level = MC_H265_LEVEL5_1;
	else
		h265_profile->level = MC_H265_LEVEL_UNSPECIFIED;

	if (tier) {
		if (strcmp(tier, "main_tier") == 0)
			h265_profile->tier = 0;
		else if (strcmp(tier, "high_tier") == 0)
			h265_profile->tier = 1;
		else
			h265_profile->tier = 0;
	}

	free(stringp);

	return 0;
}


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
	int32_t width, int32_t height, int32_t frame_rate, uint32_t bit_rate, const char *str_profile, bool external_buffer)
{
	mc_video_codec_enc_params_t *params;
	H264Profile h264_profile = { 0, };
	H265Profile h265_profile = { 0, };
	int r;

	memset(context, 0x00, sizeof(media_codec_context_t));
	context->encoder = true;
	params = &context->video_enc_params;
	params->width = width;
	params->height = height;
	params->pix_fmt = MC_PIXEL_FORMAT_NV12;
	params->bitstream_buf_size = (width * height * 3 / 2  + 0x3ff) & ~0x3ff;
	params->frame_buf_count = 3;
	params->external_frame_buf = external_buffer;
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

		r = h264_parse_profile(str_profile, &h264_profile);
		if (r == 0) {
			params->h264_enc_config.h264_profile = h264_profile.profile;
			params->h264_enc_config.h264_level = h264_profile.level;
		}
		break;
	case MEDIA_CODEC_ID_H265:
		context->codec_id = MEDIA_CODEC_ID_H265;
		params->rc_params.mode = MC_AV_RC_MODE_H265CBR;
		get_rc_params(context, &params->rc_params);
		params->rc_params.h265_cbr_params.frame_rate = frame_rate;
		params->rc_params.h265_cbr_params.bit_rate = bit_rate;

		r = h265_parse_profile(str_profile, &h265_profile);
		if (r == 0) {
			params->h265_enc_config.main_still_picture_profile_enable = h265_profile.main_still_picture_profile_enable;
			params->h265_enc_config.h265_level = h265_profile.level;
			params->h265_enc_config.h265_tier = h265_profile.tier;
		}
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

int32_t vp_decode_config_param(media_codec_context_t *context, media_codec_id_t codec_type,
	int32_t width, int32_t height)
{
	mc_video_codec_dec_params_t *params;
	context->encoder = false; // decoder output
	params = &context->video_dec_params;
	params->feed_mode = MC_FEEDING_MODE_FRAME_SIZE;
	params->pix_fmt = MC_PIXEL_FORMAT_NV12;
	params->bitstream_buf_size = (width * height * 3 / 2  + 0x3ff) & ~0x3ff;
	params->bitstream_buf_count = 3;
	params->frame_buf_count = 3;

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
		printf("Not Support decoding type: %d!\n",
				codec_type);
		return -1;
	}

	return 0;
}

static int32_t read_nv12_file(char *addr0, char *addr1, FILE *fd, uint32_t y_size)
{
	char *buffer = NULL;
	if (fd == NULL || addr0 == NULL || addr1 == NULL || y_size == 0) {
		printf("ERR(%s):null param.\n", __func__);
		return -1;
	}

	buffer = (char *)malloc(y_size + y_size / 2);

	if (fread(buffer, 1, y_size, fd) != y_size) {
		if (buffer)
			free(buffer);
		return -1;
	}

	if (fread(buffer + y_size, 1, y_size / 2, fd) != y_size / 2) {
		if (buffer)
			free(buffer);
		return -1;
	}

	memcpy(addr0, buffer, y_size);
	memcpy(addr1, buffer + y_size, y_size / 2);

	if (buffer)
		free(buffer);

	return 0;
}
static void on_encode_input_buffer_consumed(hb_ptr userdata, media_codec_buffer_t *inputBuffer)
{
	hb_mem_graphic_buf_t *buffer;

	if (!inputBuffer)
		return;

	printf("%s userdata(%p), inputBuffer->user_ptr(%p)\n", __func__,
			userdata, inputBuffer->user_ptr);

	if (inputBuffer->user_ptr) {
		buffer = (hb_mem_graphic_buf_t *)inputBuffer->user_ptr;

		hb_mem_free_buf(buffer->fd[0]);
		free(buffer);
	}
}

/* fill input buffer with external buffer */
static int32_t read_input_frame(media_codec_buffer_t *input_buffer, FILE *fd)
{
	hb_mem_graphic_buf_t *buffer;
	int64_t flags;
	uint32_t y_size;
	int32_t width, height;
	uint8_t *y_data;
	uint8_t *uv_data;
	int32_t ret;

	if (fd == NULL || input_buffer == NULL) {
		printf("ERR(%s):null param.\n", __func__);
		return -1;
	}

	width = input_buffer->vframe_buf.width;
	height = input_buffer->vframe_buf.height;
	y_size = input_buffer->vframe_buf.width * input_buffer->vframe_buf.height;

	buffer = malloc(sizeof(hb_mem_graphic_buf_t));
	memset(buffer, 0, sizeof(hb_mem_graphic_buf_t));

	flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_graph_buf(width, height, MEM_PIX_FMT_NV12, flags, 0, 0, buffer);
	if (ret < 0) {
		printf("hb_mem_alloc_graph_buf ret %d failed \n", ret);
		return ret;
	}

	y_data = buffer->virt_addr[0];
	uv_data = buffer->virt_addr[1];

#if 0
	printf("hb_mem alloc. y_data(%p), uv_data(%p), y_size(%d), size[0]: %lu, size[1]: %lu\n",
			y_data, uv_data, y_size, buffer->size[0], buffer->size[1]);
#endif

	if (fread(y_data, 1, y_size, fd) != y_size) {
		hb_mem_free_buf(buffer->fd[0]);
		free(buffer);
		return -1;
	}

	if (fread(uv_data, 1, y_size / 2, fd) != y_size / 2) {
		hb_mem_free_buf(buffer->fd[0]);
		free(buffer);
		return -1;
	}

	input_buffer->vframe_buf.vir_ptr[0] = y_data;
	input_buffer->vframe_buf.vir_ptr[1] = uv_data;
	input_buffer->vframe_buf.phy_ptr[0] = buffer->phys_addr[0];
	input_buffer->vframe_buf.phy_ptr[1] = buffer->phys_addr[1];

	/* set external buffer ptr to user_ptr, using on_input_buffer_consumed to release it */
	input_buffer->user_ptr = buffer;

	return 0;
}

// 解析配置文件
int parse_config(const char *filename,
		EncodeParams encode_params[], int *encode_streams,
		DecodeParams decode_params[], int *decode_streams)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		printf("Failed to open config file\n");
		return 0;
	}

	char line[MAX_LINE_LENGTH];
	char section[MAX_LINE_LENGTH] = "";
	int venc_stream_index = -1;
	int vdec_stream_index = -1;

	while (fgets(line, sizeof(line), file)) {
		// printf("line:%s\n",line);
		// 忽略空行和注释行
		if (line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\r')
			continue;

		// 去除行尾的换行符
		line[strcspn(line, "\r\n")] = 0;

		// 检测是否为新的节
		if (line[0] == '[') {
			sscanf(line, "[%[^]]", section);
			if (strncmp(section, "venc_stream", strlen("venc_stream")) == 0) {
				venc_stream_index++;
			}
			if (strncmp(section, "vdec_stream", strlen("vdec_stream")) == 0) {
				vdec_stream_index++;
			}
			continue;
		}

		char key[256], value[256];
		if (sscanf(line, "%[^=]=%[^\n]", key, value) != 2)
			continue;

		// 去除首尾空格
		char *trimmed_key = strtok(key, " \t");
		char *trimmed_value = strtok(value, " \t");

		// 解析参数
		if (strcmp(section, "encode") == 0) {
			if (strstr(line, "encode_streams") != NULL) {
				sscanf(line, "encode_streams = 0x%x", encode_streams);
			}
		}
		// 解析参数并填充结构体
		if (strncmp(section, "venc_stream", strlen("venc_stream")) == 0) {
			EncodeParams *params = &encode_params[venc_stream_index];
			if (strcmp(trimmed_key, "codec_type") == 0)
				params->codec_type = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "width") == 0)
				params->width = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "height") == 0)
				params->height = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "frame_rate") == 0)
				params->frame_rate = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "bit_rate") == 0)
				params->bit_rate = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "input") == 0)
				strcpy(params->input, trimmed_value);
			else if (strcmp(trimmed_key, "output") == 0)
				strcpy(params->output, trimmed_value);
			else if (strcmp(trimmed_key, "frame_num") == 0)
				params->frame_num = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "external_buffer") == 0)
				params->external_buffer = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "performance_test") == 0)
				params->performance_test = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "profile") == 0)
				strcpy(params->profile, trimmed_value);
		}
		if (strcmp(section, "decode") == 0) {
			if (strstr(line, "decode_streams") != NULL) {
				sscanf(line, "decode_streams = 0x%x", decode_streams);
			}
		}
		// 解析参数并填充结构体
		if (strncmp(section, "vdec_stream", strlen("vdec_stream")) == 0) {
			DecodeParams *params = &decode_params[vdec_stream_index];
			if (strcmp(trimmed_key, "codec_type") == 0)
				params->codec_type = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "width") == 0)
				params->width = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "height") == 0)
				params->height = atoi(trimmed_value);
			else if (strcmp(trimmed_key, "input") == 0)
				strcpy(params->input, trimmed_value);
			else if (strcmp(trimmed_key, "output") == 0)
				strcpy(params->output, trimmed_value);
		}
	}

	fclose(file);
	return 1;
}

static bool file_exists(const char *filename) {
	FILE *file = fopen(filename, "r");
	if (file) {
		fclose(file);
		return true;
	} else {
		return false;
	}
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

int32_t vp_codec_set_input(media_codec_context_t *context,
	media_codec_buffer_t *frame_buffer, uint8_t *data,
	uint32_t data_size, int32_t eos)
{
	int32_t ret = 0;
	media_codec_buffer_t *buffer = NULL;

	if ((context == NULL) || (frame_buffer == NULL) || (!eos && (data == NULL)))
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
		return -1;
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

// 视频编码函数
int32_t encode_video(media_codec_context_t *context, EncodeParams *params) {
	int32_t ret = 0;
	int32_t frame_count = 0;
	mc_av_codec_startup_params_t startup_params = {0};
	media_codec_buffer_t input_buffer = {0};
	media_codec_buffer_t ouput_buffer = {0};
	media_codec_output_buffer_info_t info;

	printf("%s...\n", __func__);

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

	ret = hb_mm_mc_start(context, &startup_params);
	if (ret != 0)
	{
		printf("%s:%d hb_mm_mc_start failed.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	printf("%s idx: %d, start successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);

	FILE *fp_input = fopen(params->input, "rb");
	if (NULL == fp_input) {
		printf("Failed to open input file: %s\n", params->input);
		return -1;
	}

	FILE *fp_output = fopen(params->output, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: %s\n", params->output);
		return -1;
	}

	while (frame_count < params->frame_num) {
		usleep(30*1000);
		memset(&input_buffer, 0x00, sizeof(media_codec_buffer_t));
		// input_buffer.type = MC_VIDEO_FRAME_BUFFER;
		ret = hb_mm_mc_dequeue_input_buffer(context, &input_buffer, 2000);
		if (ret != 0)
		{
			printf("hb_mm_mc_dequeue_input_buffer failed ret = %d\n", ret);
			goto venc_exit;
		}

		input_buffer.type = MC_VIDEO_FRAME_BUFFER;
		input_buffer.vframe_buf.width = context->video_enc_params.width;
		input_buffer.vframe_buf.height = context->video_enc_params.height;
		input_buffer.vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
		input_buffer.vframe_buf.size = input_buffer.vframe_buf.width * input_buffer.vframe_buf.height * 3 / 2;

		// 如果从 emmc 上读取数据，会在一定程度上影响性能
		ret = read_nv12_file((char *)input_buffer.vframe_buf.vir_ptr[0],
			(char *)input_buffer.vframe_buf.vir_ptr[1],
			fp_input, input_buffer.vframe_buf.width * input_buffer.vframe_buf.height);
		if (ret != 0 || feof(fp_input)) {
			clearerr(fp_input);
			rewind(fp_input);

			ret = read_nv12_file((char *)input_buffer.vframe_buf.vir_ptr[0],
			(char *)input_buffer.vframe_buf.vir_ptr[1],
			fp_input, input_buffer.vframe_buf.width * input_buffer.vframe_buf.height);
		}
		frame_count++;

		printf("%s idx: %d, frame= %d\n",
			context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);

		ret = hb_mm_mc_queue_input_buffer(context, &input_buffer, 2000);
		if (ret != 0)
		{
			printf("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
			goto venc_exit;
		}

		if (verbose) {
			printf("%s idx: %d, send frame %d successful\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);
		}

		memset(&ouput_buffer, 0x0, sizeof(media_codec_buffer_t));
		memset(&info, 0x0, sizeof(media_codec_output_buffer_info_t));
		ret = hb_mm_mc_dequeue_output_buffer(context, &ouput_buffer, &info, 2000);
		if (ret != 0)
		{
			printf("%s idx: %d, hb_mm_mc_dequeue_output_buffer failed ret = %d\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, ret);
			goto venc_exit;
		}

		if (verbose) {
			printf("%s idx: %d, get stream %d successful\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);
		}

		if (fp_output) {
			fwrite(ouput_buffer.vstream_buf.vir_ptr, ouput_buffer.vstream_buf.size, 1, fp_output);
		}

		ret = hb_mm_mc_queue_output_buffer(context, &ouput_buffer, 2000);
		if (ret != 0)
		{
			printf("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
			goto venc_exit;
		}
	}

venc_exit:
	if (fp_output) {
		fclose(fp_output);
	}

	if (fp_input) {
		fclose(fp_input);
	}

	ret = hb_mm_mc_pause(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_pause ret = %d \n", ret);
		return -1;
	}

	ret = hb_mm_mc_release(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_release ret = %d \n", ret);
		return -1;
	}

	return 0;
}

// 视频编码函数, external buffer and using callback to release external buffer
int32_t encode_video_external_buffer(media_codec_context_t *context, EncodeParams *params) {
	int32_t ret = 0;
	int32_t frame_count = 0;
	mc_av_codec_startup_params_t startup_params = {0};
	media_codec_buffer_t input_buffer = {0};
	media_codec_buffer_t ouput_buffer = {0};
	media_codec_output_buffer_info_t info;
	media_codec_callback_t callback;

	printf("%s...\n", __func__);

	ret = hb_mm_mc_initialize(context);
	if (0 != ret)
	{
		printf("hb_mm_mc_initialize failed.\n");
		return -1;
	}

	callback.on_input_buffer_consumed = on_encode_input_buffer_consumed;
	ret = hb_mm_mc_set_input_buffer_listener(context, &callback, NULL);
	if (0 != ret)
	{
		printf("hbmm_mc_set_input_buffer_listener failed.\n");
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

	ret = hb_mm_mc_start(context, &startup_params);
	if (ret != 0)
	{
		printf("%s:%d hb_mm_mc_start failed.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	printf("%s idx: %d, start successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);

	FILE *fp_input = fopen(params->input, "rb");
	if (NULL == fp_input) {
		printf("Failed to open input file: %s\n", params->input);
		return -1;
	}

	FILE *fp_output = fopen(params->output, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: %s\n", params->output);
		return -1;
	}

	while (frame_count < params->frame_num) {
		usleep(30*1000);
		memset(&input_buffer, 0x00, sizeof(media_codec_buffer_t));
		// input_buffer.type = MC_VIDEO_FRAME_BUFFER;
		ret = hb_mm_mc_dequeue_input_buffer(context, &input_buffer, 2000);
		if (ret != 0)
		{
			printf("hb_mm_mc_dequeue_input_buffer failed ret = %d\n", ret);
			goto venc_exit;
		}

		input_buffer.type = MC_VIDEO_FRAME_BUFFER;
		input_buffer.vframe_buf.width = context->video_enc_params.width;
		input_buffer.vframe_buf.height = context->video_enc_params.height;
		input_buffer.vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
		input_buffer.vframe_buf.size = input_buffer.vframe_buf.width * input_buffer.vframe_buf.height * 3 / 2;

		printf("dequeue input buffer. src_idx(%d), user_ptr(%p)\n", input_buffer.vframe_buf.src_idx, input_buffer.user_ptr);

		// 如果从 emmc 上读取数据，会在一定程度上影响性能
		ret = read_input_frame(&input_buffer, fp_input);
		if (ret != 0 || feof(fp_input)) {
			clearerr(fp_input);
			rewind(fp_input);

			ret = read_input_frame(&input_buffer, fp_input);
		}
		frame_count++;

		printf("%s idx: %d, frame= %d\n",
			context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);

		ret = hb_mm_mc_queue_input_buffer(context, &input_buffer, 2000);
		if (ret != 0)
		{
			printf("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
			goto venc_exit;
		}

		if (verbose) {
			printf("%s idx: %d, send frame %d successful\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);
		}

		memset(&ouput_buffer, 0x0, sizeof(media_codec_buffer_t));
		memset(&info, 0x0, sizeof(media_codec_output_buffer_info_t));
		ret = hb_mm_mc_dequeue_output_buffer(context, &ouput_buffer, &info, 2000);
		if (ret != 0)
		{
			printf("%s idx: %d, hb_mm_mc_dequeue_output_buffer failed ret = %d\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, ret);
			goto venc_exit;
		}

		if (verbose) {
			printf("%s idx: %d, get stream %d successful\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);
		}

		if (fp_output) {
			fwrite(ouput_buffer.vstream_buf.vir_ptr, ouput_buffer.vstream_buf.size, 1, fp_output);
		}

		ret = hb_mm_mc_queue_output_buffer(context, &ouput_buffer, 2000);
		if (ret != 0)
		{
			printf("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
			goto venc_exit;
		}
	}

venc_exit:
	if (fp_output) {
		fclose(fp_output);
	}

	if (fp_input) {
		fclose(fp_input);
	}

	ret = hb_mm_mc_pause(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_pause ret = %d \n", ret);
		return -1;
	}

	ret = hb_mm_mc_release(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_release ret = %d \n", ret);
		return -1;
	}

	return 0;
}

static int32_t create_hb_mem_graphic_buf_from_file(hb_mem_graphic_buf_t *input_buffer,
	int max_count, const char *file_name, int width, int height)
{
	uint32_t y_size = width * height;
	int meida_codec_buffer_count = 0;
	FILE *fp_output = fopen(file_name, "rb");
	if (NULL == fp_output) {
		printf("Failed to open output file: %s\n", file_name);
		return -1;
	}

	for (size_t i = 0; i < max_count; i++){

		int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
		int ret = hb_mem_alloc_graph_buf(width, height, MEM_PIX_FMT_NV12, flags, 0, 0, input_buffer + i);
		if (ret < 0) {
			printf("hb_mem_alloc_graph_buf ret %d failed \n", ret);
			return ret;
		}
		uint8_t *y_data = input_buffer[i].virt_addr[0];
		uint8_t *uv_data = input_buffer[i].virt_addr[1];
		if (fread(y_data, 1, y_size, fp_output) != y_size) {
			hb_mem_free_buf(input_buffer[i].fd[0]);
			break;
		}
		if (fread(uv_data, 1, y_size / 2, fp_output) != y_size / 2) {
			hb_mem_free_buf(input_buffer[i].fd[0]);
			break;
		}
		meida_codec_buffer_count++;
	}
	fclose(fp_output);
	return meida_codec_buffer_count;
}
static void release_hb_mem_graphic_buf(hb_mem_graphic_buf_t *input_buffer, int max_count){
	for (size_t i = 0; i < max_count; i++){
		hb_mem_free_buf(input_buffer[i].fd[0]);
	}
}

// 视频编码函数, for performance testing
int32_t encode_video_performance_test(media_codec_context_t *context, EncodeParams *params) {
	int32_t ret = 0;
	int32_t frame_count = 0;
	mc_av_codec_startup_params_t startup_params = {0};
	media_codec_buffer_t input_buffer = {0};
	media_codec_buffer_t ouput_buffer = {0};
	media_codec_output_buffer_info_t info;

	printf("%s...\n", __func__);

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

	ret = hb_mm_mc_start(context, &startup_params);
	if (ret != 0)
	{
		printf("%s:%d hb_mm_mc_start failed.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	printf("%s idx: %d, start successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);
	#define  MEDIA_CODEC_BUFFER_MAX_COUNT 10
	hb_mem_graphic_buf_t hb_mem_graphic_bufs[MEDIA_CODEC_BUFFER_MAX_COUNT];
	int meida_codec_buffer_count = create_hb_mem_graphic_buf_from_file(hb_mem_graphic_bufs,
		MEDIA_CODEC_BUFFER_MAX_COUNT, params->input, params->width, params->height);
	if(meida_codec_buffer_count <= 0){
		printf("file %s not have yuv data, so exit(-1).\n", params->input);
		exit(-1);
	}else{
		printf("use file %s  %d frames to encode.\n", params->output, meida_codec_buffer_count);
	}

	FILE *fp_output = fopen(params->output, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: %s\n", params->output);
		return -1;
	}
	int meida_codec_buffer_index = 0;
	hb_mem_graphic_buf_t *hb_mem_graphic_buf = NULL;
	while (frame_count < params->frame_num) {
		usleep(33*1000);

		//1. update input yuv data
		if(meida_codec_buffer_index >= meida_codec_buffer_count){
			meida_codec_buffer_index = 0;
		}
		hb_mem_graphic_buf = &hb_mem_graphic_bufs[meida_codec_buffer_index];
		meida_codec_buffer_index++;

		//2. init media_codec_buffer_t
		memset(&input_buffer, 0x00, sizeof(media_codec_buffer_t));
		ret = hb_mm_mc_dequeue_input_buffer(context, &input_buffer, 2000);
		if (ret != 0)
		{
			printf("hb_mm_mc_dequeue_input_buffer failed ret = %d\n", ret);
			goto venc_exit;
		}
		input_buffer.type = MC_VIDEO_FRAME_BUFFER;
		input_buffer.vframe_buf.width = context->video_enc_params.width;
		input_buffer.vframe_buf.height = context->video_enc_params.height;
		input_buffer.vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
		input_buffer.vframe_buf.size = input_buffer.vframe_buf.width * input_buffer.vframe_buf.height * 3 / 2;

		input_buffer.vframe_buf.vir_ptr[0] = hb_mem_graphic_buf->virt_addr[0];
		input_buffer.vframe_buf.vir_ptr[1] = hb_mem_graphic_buf->virt_addr[1];
		input_buffer.vframe_buf.phy_ptr[0] = hb_mem_graphic_buf->phys_addr[0];
		input_buffer.vframe_buf.phy_ptr[1] = hb_mem_graphic_buf->phys_addr[1];

		frame_count++;

		printf("%s idx: %d, frame= %d\n",
			context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);

		ret = hb_mm_mc_queue_input_buffer(context, &input_buffer, 2000);
		if (ret != 0)
		{
			printf("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
			goto venc_exit;
		}

		if (verbose) {
			printf("%s idx: %d, send frame %d successful\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);
		}

		memset(&ouput_buffer, 0x0, sizeof(media_codec_buffer_t));
		memset(&info, 0x0, sizeof(media_codec_output_buffer_info_t));
		ret = hb_mm_mc_dequeue_output_buffer(context, &ouput_buffer, &info, 2000);
		if (ret != 0)
		{
			printf("%s idx: %d, hb_mm_mc_dequeue_output_buffer failed ret = %d\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, ret);
			goto venc_exit;
		}

		if (verbose) {
			printf("%s idx: %d, get stream %d successful\n",
				context->encoder ? "Encode" : "Decode", context->instance_index, frame_count);
		}

		if (fp_output) {
			fwrite(ouput_buffer.vstream_buf.vir_ptr, ouput_buffer.vstream_buf.size, 1, fp_output);
		}

		ret = hb_mm_mc_queue_output_buffer(context, &ouput_buffer, 2000);
		if (ret != 0)
		{
			printf("idx: %d, hb_mm_mc_queue_output_buffer failed ret = %d \n", context->instance_index, ret);
			goto venc_exit;
		}
	}

venc_exit:
	if (fp_output) {
		fclose(fp_output);
	}

	release_hb_mem_graphic_buf(hb_mem_graphic_bufs, meida_codec_buffer_count);

	ret = hb_mm_mc_pause(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_pause ret = %d \n", ret);
		return -1;
	}

	ret = hb_mm_mc_release(context);
	if (ret != 0)
	{
		printf("Failed to hb_mm_mc_release ret = %d \n", ret);
		return -1;
	}

	return 0;
}

int32_t decode_output_video(media_codec_context_t *context, DecodeParams *params)
{
	int32_t ret = 0;
	media_codec_buffer_t ouput_buffer = {0};
	media_codec_output_buffer_info_t info;
	int32_t frame_count = 0;

	FILE *fp_output = fopen(params->output, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: %s\n", params->output);
		return -1;
	}

	while(decode_output_exit) {
		ret = vp_codec_get_output(context, &ouput_buffer, &info, 2000);
		if (ret != 0) {
			// wait for each frame for decoding
			// usleep(30 * 1000);
			printf("decode output continue\n");
			continue;
		}
		if (fp_output) {
			if (verbose) {
				frame_count++;
				printf("[%s][%d] frame_count:%d, size: %d, exp size: %d stride: %d vstride: %d\n",
					__func__, __LINE__,
					frame_count, ouput_buffer.vframe_buf.size,
					ouput_buffer.vframe_buf.width * ouput_buffer.vframe_buf.height * 3 / 2,
					ouput_buffer.vframe_buf.stride, ouput_buffer.vframe_buf.vstride);
				printf("ouput_buffer.vframe_buf.vir_ptr[0]: %p ouput_buffer.vframe_buf.vir_ptr[1]: %p\n",
					ouput_buffer.vframe_buf.vir_ptr[0], ouput_buffer.vframe_buf.vir_ptr[1]);
			}
			// Jpeg解码器输出的图像分辨率会被强制32字节对齐
			// 所以如果输入图像与输出图像的像素不一样的话，需要特殊处理，否则图像会有绿边
			if (params->codec_type == MEDIA_CODEC_ID_JPEG) {
				if (ouput_buffer.vframe_buf.width == params->width && ouput_buffer.vframe_buf.height == params->height) {
					// 输出图像与输入图像的分辨率相同，直接写入文件
					fwrite(
						ouput_buffer.vframe_buf.vir_ptr[0],
						ouput_buffer.vframe_buf.width * ouput_buffer.vframe_buf.height,
						1,
						fp_output
					);
					fwrite(
						ouput_buffer.vframe_buf.vir_ptr[1],
						ouput_buffer.vframe_buf.width * ouput_buffer.vframe_buf.height / 2,
						1,
						fp_output
					);
				} else {
					// 输出图像与输入图像的分辨率不同，需要特殊处理
					// 处理Y分量
					for (int y = 0; y < params->height; y++) {
						fwrite(
							ouput_buffer.vframe_buf.vir_ptr[0] + y * ouput_buffer.vframe_buf.width,
							params->width,
							1,
							fp_output
						);
					}

					// 处理UV分量
					for (int y = 0; y < params->height / 2; y++) {
						fwrite(
							ouput_buffer.vframe_buf.vir_ptr[1] + y * ouput_buffer.vframe_buf.width,
							params->width,
							1,
							fp_output
						);
					}
				}
			} else {
				fwrite(
					ouput_buffer.vframe_buf.vir_ptr[0],
					ouput_buffer.vframe_buf.width * ouput_buffer.vframe_buf.height,
					1,
					fp_output
				);
				fwrite(
					ouput_buffer.vframe_buf.vir_ptr[1],
					ouput_buffer.vframe_buf.width * ouput_buffer.vframe_buf.height / 2,
					1,
					fp_output
				);
			}
		}
		vp_codec_release_output(context, &ouput_buffer);
	}

	if (fp_output) {
		fclose(fp_output);
	}
	return 0;
}

// 视频解码函数
int32_t decode_h264_h265_mjpeg_video(media_codec_context_t *context, DecodeParams *params)
{
	int32_t ret = 0;
	media_codec_buffer_t input_buffer = {0};
	AVFormatContext *avContext = NULL;
	AVPacket avpacket = {0};
	int32_t video_idx = -1;
	int32_t firstPacket = 1;
	bool eos = false;
	int32_t seqHeaderSize = 0;
	uint8_t *seqHeader = NULL;

	video_idx = av_open_stream(params, &avContext, &avpacket);
	if (video_idx < 0)
	{
		printf("failed to av_open_stream\n");
		goto err_av_open;
	}

	while(1) {
		mc_inter_status_t pstStatus;
		hb_mm_mc_get_status(context, &pstStatus);

		// wait for each frame for decoding
		usleep(30 * 1000);

		if (!avpacket.size)
		{
			ret = av_read_frame(avContext, &avpacket);
		}

		if (ret < 0)
		{
			if (ret == AVERROR_EOF || avContext->pb->eof_reached == true)
			{
				printf("No more valid data available for decoding, "
					"avpacket.size: %d."
					" Decoder will exit due to timeout after fetching"
					" decoded output.\n", avpacket.size);

				eos = true;
			}
			else
			{
				printf("Failed to av_read_frame error(0x%08x)\n", ret);
			}
			break;
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
					printf("Failed to mallock seqHeader\n");
					eos = true;
					break;
				}

				seqHeaderSize = av_build_dec_seq_header(seqHeader,
												context->codec_id,
												avContext->streams[video_idx], &retSize);
				if (seqHeaderSize < 0)
				{
					printf("Failed to build seqHeader\n");
					eos = true;
					break;
				}
				firstPacket = 0;
			}
			if (avpacket.size <= context->video_dec_params.bitstream_buf_size)
			{
				if (seqHeaderSize)
				{
					vp_codec_set_input(context, &input_buffer, seqHeader, seqHeaderSize, eos);
				}
				else
				{
					vp_codec_set_input(context, &input_buffer, avpacket.data, avpacket.size, eos);
					// if (log_ctrl_level_get(NULL) == LOG_TRACE) {
					// 	print_avpacket_info(&avpacket);
					// 	vp_codec_print_media_codec_output_buffer_info(&frame);
					// }
					av_packet_unref(&avpacket);
					avpacket.size = 0;
				}
			}
			else
			{
				printf("The external stream buffer is too small!\n"
						"avpacket.size:%d, buffer size:%d\n",
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
		vp_codec_set_input(context, &input_buffer, seqHeader, seqHeaderSize, eos);
	}

	if (seqHeader)
	{
		free(seqHeader);
		seqHeader = NULL;
	}

err_av_open:
	if (avContext)
		avformat_close_input(&avContext);
	return 0;
}


// 解码一连串的jpeg图像
int32_t decode_jpeg_sequence(media_codec_context_t *context, DecodeParams *params)
{
	media_codec_buffer_t input_buffer = {0};
	uint8_t *image_data;
	size_t image_size;

	printf("%s idx: %d, start successful\n", context->encoder ? "Encode" : "Decode", context->instance_index);

	FILE *fp_input = fopen(params->input, "rb");
	if (NULL == fp_input) {
		printf("Failed to open input file: %s\n", params->input);
		return -1;
	}

	FILE *fp_output = fopen(params->output, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: %s\n", params->output);
		return -1;
	}

	while (1) {
		if (extract_jpeg(fp_input, &image_data, &image_size) == 0) {
			vp_codec_set_input(context, &input_buffer, image_data, image_size, 0);
			free(image_data);
		} else {
			break;
		}
	}

	if (fp_output) {
		fclose(fp_output);
	}

	if (fp_input) {
		fclose(fp_input);
	}

	return 0;
}

// 编码线程函数
void *encode_thread(void *arg) {
	int32_t ret = 0;
	EncodeParams *params = (EncodeParams *)arg;
	media_codec_context_t context = {0};

	printf("Encoding video...\n");
	print_encode_params(params);
	bool is_enable_external_buffer = false;
	if( params->external_buffer ||  params->performance_test){
		is_enable_external_buffer = true;
	}
	ret = vp_encode_config_param(&context,
		params->codec_type,
		params->width,
		params->height,
		params->frame_rate,
		params->bit_rate,
		params->profile,
		is_enable_external_buffer);
	if (ret != 0) {
		printf("Encode config param error, type:%d width:%d height:%d"
			" frame_rate: %d bit_rate:%d\n",
			params->codec_type,
			params->width,
			params->height,
			params->frame_rate,
			params->bit_rate);
	}

	if (params->performance_test)
		encode_video_performance_test(&context, params);
	else if (params->external_buffer)
		encode_video_external_buffer(&context, params);
	else
		encode_video(&context, params);
	pthread_exit(NULL);
}

void *decode_output_thread(void *arg) {
	DecodeParams *params = (DecodeParams *)arg;

	printf("Decoding output video...\n");
	decode_output_video(&params->decode_context, params);
	pthread_exit(NULL);
}

// 解码线程函数
void *decode_thread(void *arg) {
	DecodeParams *params = (DecodeParams *)arg;

	printf("Decoding video...\n");
	if (params->codec_type == MEDIA_CODEC_ID_JPEG) {
		decode_jpeg_sequence(&params->decode_context, params);
	} else {
		decode_h264_h265_mjpeg_video(&params->decode_context, params);
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	int32_t ret = 0;
	int opt;
	char *config_file = "codec_config.ini";
	int encode_option = -1;
	int decode_option = -1;
	// 解析配置文件
	EncodeParams encode_params[MAX_STREAMS];
	int encode_streams = 0x0;
	DecodeParams decode_params[MAX_STREAMS];
	int decode_streams = 0x0;

	while ((opt = getopt_long(argc, argv, "f:e:d:vh", long_options, NULL)) != -1) {
		switch (opt) {
			case 'f':
				config_file = optarg;
				break;
			case 'e':
				if (optarg[0] == '0' && optarg[1] == 'x') {
					encode_option = strtol(optarg, NULL, 16);
				} else {
					encode_option = strtol(optarg, NULL, 10);
				}
				break;
			case 'd':
				if (optarg[0] == '0' && optarg[1] == 'x') {
					decode_option = strtol(optarg, NULL, 16);
				} else {
					decode_option = strtol(optarg, NULL, 10);
				}
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				print_help();
				exit(EXIT_SUCCESS);
			default:
				fprintf(stderr, "Unknown option: %c\n", optopt);
				print_help();
				exit(EXIT_FAILURE);
		}
	}

	if (config_file == NULL) {
		fprintf(stderr, "Error: Missing required arguments\n");
		print_help();
		exit(EXIT_FAILURE);
	}

	// Do something with the parsed options
	printf("Config file: %s\n", config_file);

	if (!file_exists(config_file)) {
		fprintf(stderr, "Error: Configuration file '%s' does not exist.\n", config_file);
		exit(EXIT_FAILURE);
	}

	if (!parse_config(config_file, encode_params, &encode_streams,
		decode_params, &decode_streams)) {
		printf("Failed to parse config file\n");
		return 1;
	}

	// 根据用户命令行参数更新配置项
	if (encode_option != -1) {
		encode_streams = encode_option;
	}

	if (decode_option != -1) {
		decode_streams = decode_option;
	}

	printf("encode_streams: 0x%x\n", encode_streams);
	printf("decode_streams: 0x%x\n", decode_streams);

	// 创建编码线程
	pthread_t encode_threads[MAX_STREAMS];
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (encode_streams & (1 << i)) {
			if (!file_exists(encode_params[i].input)) {
				fprintf(stderr, "Error: encode input file '%s' does not exist.\n", encode_params[i].input);
				exit(EXIT_FAILURE);
			}
			pthread_create(&encode_threads[i], NULL, encode_thread, &encode_params[i]);
		}
	}

	// 初始化解码器
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (decode_streams & (1 << i)) {
			print_decode_params(&decode_params[i]);
			ret = vp_decode_config_param(&decode_params[i].decode_context,
				decode_params[i].codec_type,
				decode_params[i].width,
				decode_params[i].height);
			if (ret != 0) {
				printf("Decode config param error, type:%d width:%d height:%d\n",
					decode_params[i].codec_type,
					decode_params[i].width,
					decode_params[i].height);
			}
			ret = vp_codec_init(&decode_params[i].decode_context);
			if (ret != 0)
			{
				printf("Decode vp_codec_init error(%d)\n", i);
				return -1;
			}
			printf("Init video decode instance %d successful\n", decode_params[i].decode_context.instance_index);
			ret = vp_codec_start(&decode_params[i].decode_context);
			if (ret != 0)
			{
				printf("Encode vp_codec_start error\n");
				return -1;
			}
		}
	}

	// 创建解码输入线程
	pthread_t decode_threads[MAX_STREAMS];
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (decode_streams & (1 << i)) {
			if (!file_exists(decode_params[i].input)) {
				fprintf(stderr, "Error: decode input file '%s' does not exist.\n", decode_params[i].input);
				exit(EXIT_FAILURE);
			}
			pthread_create(&decode_threads[i], NULL, decode_thread, &decode_params[i]);
		}
	}
	decode_output_exit = 1;
	// 创建解码输出线程
	pthread_t decode_outout_threads[MAX_STREAMS];
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (decode_streams & (1 << i)) {
			pthread_create(&decode_outout_threads[i], NULL, decode_output_thread, &decode_params[i]);
		}
	}

	// 等待所有编码线程结束
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (encode_streams & (1 << i)) {
			pthread_join(encode_threads[i], NULL);
		}
	}

	// 等待所有解码输入线程结束
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (decode_streams & (1 << i)) {
			pthread_join(decode_threads[i], NULL);
		}
	}

	// 等待1000豪秒，让解码器完成所有帧的解码并被output
	usleep(1000*1000);
	decode_output_exit = 0;

	// 等待所有解码输出线程结束
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (decode_streams & (1 << i)) {
			pthread_join(decode_outout_threads[i], NULL);
		}
	}

	for (int i = 0; i < MAX_STREAMS; i++) {
		if (decode_streams & (1 << i)) {
			ret = vp_codec_stop(&decode_params[i].decode_context);
			if (ret != 0)
			{
				printf("Encode vp_codec_stop error\n");
				return -1;
			}
			ret = vp_codec_deinit(&decode_params[i].decode_context);
			if (ret != 0)
			{
				printf("Decode vp_codec_deinit error(%d)\n", i);
				return -1;
			}
			printf("deinit video decode instance %d successful\n", decode_params[i].decode_context.instance_index);
		}
	}

	return 0;
}
