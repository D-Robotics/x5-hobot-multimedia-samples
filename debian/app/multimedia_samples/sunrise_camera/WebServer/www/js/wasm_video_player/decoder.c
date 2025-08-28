/**
 * @file
 * video decoding with libavcodec API example
 *
 * Optimized version:
 * 1. Removed unused variables and code
 * 2. Organized globals into a struct
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"

// Type definitions and constants
typedef void (*VideoCallback)(unsigned char *buff, int size, double timestamp);

typedef enum ErrorCode
{
	kErrorCode_Success = 0,
	kErrorCode_Invalid_Param,
	kErrorCode_Invalid_State,
	kErrorCode_Invalid_Data,
	kErrorCode_Invalid_Format,
	kErrorCode_NULL_Pointer,
	kErrorCode_Open_File_Error,
	kErrorCode_Eof,
	kErrorCode_FFmpeg_Error
} ErrorCode;

typedef enum LogLevel
{
	kLogLevel_None = 0,
	kLogLevel_Core,
	kLogLevel_All
} LogLevel;

typedef enum DecoderType
{
	kDecoderType_H264 = 0,
	kDecoderType_H265
} DecoderType;

// Decoder Context Structure (replaces globals)
typedef struct DecoderContext
{
	LogLevel logLevel;
	DecoderType decoderType;

	// FFmpeg components
	const AVCodec *codec;
	AVCodecParserContext *parser;
	AVCodecContext *codecCtx;
	AVPacket *pkt;
	AVFrame *frame;

	// Buffer management
	unsigned char *yuvBuffer;
	int yuvBufferSize;

	// Callback
	VideoCallback videoCallback;

	// State
	int isInitialized;
} DecoderContext;

static DecoderContext decoderCtx = {
	.logLevel = kLogLevel_None,
	.decoderType = kDecoderType_H265,

	//ffmpeg context
	.codec = NULL,
	.parser = NULL,
	.codecCtx = NULL,

	//ffmpeg data
	.pkt = NULL,
	.frame = NULL,

	//send to user
	.yuvBuffer = NULL,
	.yuvBufferSize = 0,
	.videoCallback = NULL,

	//flag
	.isInitialized = 0
};

// Utility functions
static void simpleLog(const char *format, ...)
{
	if (decoderCtx.logLevel == kLogLevel_None)
		return;

	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	printf("[Decoder] %s\n", buffer);
}

static void ffmpegLogCallback(void *ptr, int level, const char *fmt, va_list vl)
{
	if (decoderCtx.logLevel != kLogLevel_All || level > AV_LOG_DEBUG)
		return;

	char line[1024];
	AVClass *avc = ptr ? *(AVClass **)ptr : NULL;

	if (avc)
	{
		snprintf(line, sizeof(line), "[FFmpeg][%s] ", avc->item_name(ptr));
		vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);
		simpleLog("%s", line);
	}
}

static ErrorCode copyYuvData(AVFrame *frame)
{
	if (!frame || !decoderCtx.yuvBuffer)
	{
		return kErrorCode_Invalid_Param;
	}

	unsigned char *dst = decoderCtx.yuvBuffer;
	const int width = frame->width;
	const int height = frame->height;

	// Copy Y plane
	for (int i = 0; i < height; i++)
	{
		memcpy(dst, frame->data[0] + i * frame->linesize[0], width);
		dst += width;
	}

	// Copy U plane
	for (int i = 0; i < height / 2; i++)
	{
		memcpy(dst, frame->data[1] + i * frame->linesize[1], width / 2);
		dst += width / 2;
	}

	// Copy V plane
	for (int i = 0; i < height / 2; i++)
	{
		memcpy(dst, frame->data[2] + i * frame->linesize[2], width / 2);
		dst += width / 2;
	}

	return kErrorCode_Success;
}

// Core decoding functions
static ErrorCode decodePacket(AVFrame *frame)
{
	int ret = avcodec_send_packet(decoderCtx.codecCtx, decoderCtx.pkt);
	if (ret < 0)
	{
		simpleLog("Error sending packet: %s", av_err2str(ret));
		return kErrorCode_FFmpeg_Error;
	}

	while (ret >= 0)
	{
		//关键！！！： 函数内部会 自动调用 av_frame_unref(frame) 清除原有数据
		ret = avcodec_receive_frame(decoderCtx.codecCtx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			break;
		}
		if (ret < 0)
		{
			simpleLog("Error during decoding: %s", av_err2str(ret));
			return kErrorCode_FFmpeg_Error;
		}

		if (copyYuvData(frame) != kErrorCode_Success)
		{
			return kErrorCode_FFmpeg_Error;
		}

		if (decoderCtx.videoCallback)
		{
			decoderCtx.videoCallback(decoderCtx.yuvBuffer, decoderCtx.yuvBufferSize, frame->pts);
		}
	}
	return kErrorCode_Success;
}

// Public API
ErrorCode openDecoder(int codecType, int width, int height, long callback, int logLv)
{
	if (decoderCtx.isInitialized)
	{
		return kErrorCode_Success;
	}

	// Initialize context
	decoderCtx.decoderType = codecType;
	decoderCtx.logLevel = logLv;
	decoderCtx.videoCallback = (VideoCallback)callback;

	if (decoderCtx.logLevel == kLogLevel_All)
	{
		av_log_set_callback(ffmpegLogCallback);
	}
	av_log_set_level(1);

	// Initialize codec
	decoderCtx.codec = avcodec_find_decoder(
		decoderCtx.decoderType == kDecoderType_H264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_H265);

	if (!decoderCtx.codec)
	{
		simpleLog("Codec not found");
		return kErrorCode_FFmpeg_Error;
	}
	simpleLog("wasm decoder found codec is %s\n", decoderCtx.codec->name);

	decoderCtx.parser = av_parser_init(decoderCtx.codec->id);
	if (!decoderCtx.parser)
	{
		simpleLog("Parser not found");
		return kErrorCode_FFmpeg_Error;
	}

	decoderCtx.codecCtx = avcodec_alloc_context3(decoderCtx.codec);
	if (!decoderCtx.codecCtx || avcodec_open2(decoderCtx.codecCtx, decoderCtx.codec, NULL) < 0)
	{
		simpleLog("Could not open codec");
		return kErrorCode_FFmpeg_Error;
	}

	decoderCtx.frame = av_frame_alloc();
	decoderCtx.pkt = av_packet_alloc();
	if (!decoderCtx.frame || !decoderCtx.pkt)
	{
		simpleLog("Memory allocation failed");
		return kErrorCode_FFmpeg_Error;
	}

	decoderCtx.yuvBufferSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
	decoderCtx.yuvBuffer = (unsigned char *)av_mallocz(decoderCtx.yuvBufferSize);
	if (!decoderCtx.yuvBuffer)
	{
		simpleLog("Buffer initialization failed, alloc size is %d\n", decoderCtx.yuvBufferSize);
		return kErrorCode_FFmpeg_Error;
	}

	decoderCtx.isInitialized = 1;
	return kErrorCode_Success;
}

ErrorCode decodeData(unsigned char *data, size_t data_size, long pts)
{
	if (!decoderCtx.isInitialized)
	{
		return kErrorCode_Invalid_State;
	}

	// Parse and decode
	while (data_size > 0)
	{
		int ret = av_parser_parse2(
			decoderCtx.parser,
			decoderCtx.codecCtx,
			&decoderCtx.pkt->data,
			&decoderCtx.pkt->size,
			data,
			(int)data_size,
			pts,
			pts,
			0);

		if (ret < 0)
		{
			simpleLog("Parsing error");
			return kErrorCode_FFmpeg_Error;
		}

		data += ret;
		data_size -= ret;

		if (decoderCtx.pkt->size)
		{
			decoderCtx.pkt->pts = pts;
			ErrorCode err = decodePacket(decoderCtx.frame);
			if (err != kErrorCode_Success)
			{
				return err;
			}
		}
	}

	return kErrorCode_Success;
}

ErrorCode flushDecoder()
{
	if (!decoderCtx.isInitialized)
	{
		return kErrorCode_Invalid_State;
	}
	return decodePacket(decoderCtx.frame);
}

ErrorCode closeDecoder()
{
	if (!decoderCtx.isInitialized)
	{
		return kErrorCode_Success;
	}

	// Free resources
	if (decoderCtx.parser)
		av_parser_close(decoderCtx.parser);
	if (decoderCtx.codecCtx)
		avcodec_free_context(&decoderCtx.codecCtx);
	if (decoderCtx.frame)
		av_frame_free(&decoderCtx.frame);
	if (decoderCtx.pkt)
		av_packet_free(&decoderCtx.pkt);
	if (decoderCtx.yuvBuffer)
		av_freep(&decoderCtx.yuvBuffer);

	// Reset context
	memset(&decoderCtx, 0, sizeof(DecoderContext));
	return kErrorCode_Success;
}
int main()
{
	// simpleLog("Native loaded.");
	return 0;
}