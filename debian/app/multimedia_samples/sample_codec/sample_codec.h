/***************************************************************************
 * COPYRIGHT NOTICE
 * Copyright 2024 D-Robotics, Inc.
 * All rights reserved.
 ***************************************************************************/
#ifndef __SAMPLE_CODEC_H
#define __SAMPLE_CODEC_H

#include <stdatomic.h>

#include "hbn_api.h"
#include "hb_media_codec.h"
#include "hb_media_error.h"
#include "hbmem.h"

#define TAG "[SAMPLE_CODEC]"

#define MAX_STREAMS 32 // 最大支持的编码和解码流数量
#define MAX_LINE_LENGTH 256 // 最大行长度

// 视频编码参数结构体
typedef struct {
	media_codec_id_t codec_type;
	int32_t width;
	int32_t height;
	int32_t frame_rate;
	uint32_t bit_rate;
	char input[MAX_LINE_LENGTH];
	char output[MAX_LINE_LENGTH];
	int32_t frame_num;
	int32_t external_buffer;
	int32_t performance_test;
	char profile[32];
} EncodeParams;

typedef struct {
	mc_h264_profile_t profile;
	mc_h264_level_t level;
} H264Profile;

typedef struct {
	hb_bool main_still_picture_profile_enable;
	mc_h265_level_t level;
	hb_s32 tier;
} H265Profile;

// 视频解码参数结构体
typedef struct {
	media_codec_id_t codec_type;
	int32_t width;
	int32_t height;
	char input[MAX_LINE_LENGTH];
	char output[MAX_LINE_LENGTH];
	int32_t frame_num;
	media_codec_context_t decode_context;
} DecodeParams;

#endif
