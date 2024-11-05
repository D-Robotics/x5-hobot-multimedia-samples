/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * uvc_app.c
 *	uvc app's main entry
 *
 * Copyright (C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */

#include <time.h>
#include <pthread.h>
#include "uvc_gadget_api.h"
#include "uvc_event_handler.h"
#include "camera_lib.h"
#include "video_queue.h"
#include "utils.h"
#include "libavformat/avformat.h"

#define UVC_DEBUG
// #define UVC_DUMP
// #define UVC_DUMP_EARLY

struct media_info {
	enum uvc_fourcc_format format;
	unsigned int width;
	unsigned int height;
	unsigned int image_size;
	unsigned int offset;
	void *image_data;

	AVFormatContext *fmt_ctx;
	pthread_t demux_tid;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	queue *uvc_q;
	AVPacket pkt;

	int video_index;
	int streamon;
	int has_stream;

	pthread_t rawvideo_demux_tid;
	unsigned int frame_size;
	void *rawvideo_data;
	int rawvideo_fd;

	pthread_t image_demux_tid;

#ifdef UVC_DUMP
	int dump_fd;
#endif
};

static int pattern_get_frame(struct uvc_context *ctx,
				void **buf_to, int *buf_len, void **entity,
				void *userdata)
{
	/* just return succeed, libguvc supply the ddr test pattern */
	return 0;
}

static void pattern_release_frame(struct uvc_context *ctx,
				     void **entity, void *userdata)
{
	return;
}

static int image_load(struct media_info *info, const char *img)
{
	off_t offset = 0;
	int fd = -1;

	if (img == NULL)
		return -1;

	fd = open(img, O_RDONLY);
	if (fd < 0) {
		printf("Unable to open MJPEG image '%s\n'", img);
		return -1;
	}

	offset = lseek(fd, 0, SEEK_END);
	if (offset > UINT32_MAX) {
		printf("file too big. size(%ldbytes)\n", offset);
		close(fd);

		return -1;
	}

	info->image_size = (unsigned int)offset;
	lseek(fd, 0, SEEK_SET);
	info->image_data = malloc(info->image_size);
	if (!info->image_data) {
		printf("Unable to allocate memory for MJPEG image\n");
		info->image_size = 0;
		close(fd);

		return -1;
	}

	read(fd, info->image_data, info->image_size);
	close(fd);

	info->offset = 0;

	printf("image_load succeed. data: %p, size: %u\n",
	       info->image_data, info->image_size);

	return 0;
}

static void image_close(struct media_info *info)
{
	if (!info)
		return;

	printf("image close\n");
	if (info->image_data)
		free(info->image_data);

	info->image_data = 0;

	return;
}

static int image_get_uvc_frame(struct uvc_context *ctx,
				void **buf_to, int *buf_len, void **entity,
				void *userdata)
{
	struct media_info *info = (struct media_info *)userdata;
	struct uvc_device *dev;
	unsigned int width, height, fcc;

	if (!info || !ctx || !ctx->udev)
		return -EINVAL;

#ifdef UVC_DEBUG
	interval_cost_trace_in();
#endif

	dev = ctx->udev;
	fcc = dev->fcc;
	width = dev->width;
	height = dev->height;

	/* mjpeg & h264 go through another case */
	if (fcc == V4L2_PIX_FMT_MJPEG || fcc == V4L2_PIX_FMT_H264)
		return 0;

	/* check request stream format */
	if (fcc != uvc_format_to_fcc(info->format) ||
	    width != info->width || height != info->height)
		return -EINVAL;

	/* apply frame to *buf_to */
	*buf_to = info->image_data;
	*buf_len = info->image_size;
	*entity = 0;

#ifdef UVC_DEBUG
	interval_cost_trace_out(*buf_len);
#endif

	return 0;
}

static void image_release_uvc_frame(struct uvc_context *ctx,
				     void **entity, void *userdata)
{
	if (!ctx || !entity)
		return;

	return;
}

static int image_queue_get_frame(struct uvc_context *ctx,
				void **buf_to, int *buf_len, void **entity,
				void *userdata)
{
	struct media_info *info = (struct media_info *)userdata;
	struct uvc_device *dev;
	void *deque_frame;
	unsigned int frame_size = 0;
	unsigned int width, height, fcc;

	if (!info || !ctx || !ctx->udev)
		return -EINVAL;

#ifdef UVC_DEBUG
	interval_cost_trace_in();
#endif

	dev = ctx->udev;
	fcc = dev->fcc;
	width = dev->width;
	height = dev->height;

	/* still keep yuyv & nv12 to used default pattern */
	if (fcc == V4L2_PIX_FMT_MJPEG || fcc == V4L2_PIX_FMT_H264)
		return 0;

	/* check request stream format */
	if (fcc != uvc_format_to_fcc(info->format) ||
	    width != info->width || height != info->height)
		return -EINVAL;

	/* read frames from the file */
	/* dequeue buffer from uvc_q */
	deque_frame = dequeue(info->uvc_q, &frame_size);

	if (!deque_frame || !frame_size) {
#if 0
		printf("buffer underflow!!! deque_frame(%p), frame_size(%u)\n",
		       deque_frame, frame_size);
#endif
		return -EFAULT;
	}

	/* apply frame to *buf_to */
	*buf_to = deque_frame;
	*buf_len = frame_size;
	*entity = deque_frame;

#ifdef UVC_DEBUG
	interval_cost_trace_out(*buf_len);
#endif

	return 0;
}

static void image_queue_release_frame(struct uvc_context *ctx,
				     void **entity, void *userdata)
{
	if (!ctx || !entity || !(*entity))
		return;

	void *frame = *entity;
	free(frame);

	*entity = 0;

	return;
}

static void *image_demux_routine(void *param)
{
	struct media_info *info = (struct media_info *)param;

	if (!info)
		return (void *)0;

	/* wait stream on condition */
	pthread_mutex_lock(&info->mutex);
	pthread_cond_wait(&info->cond, &info->mutex);
	pthread_mutex_unlock(&info->mutex);

	while (info->streamon && info->has_stream) {
		/* check video queue status */
		if (queue_is_full(info->uvc_q)) {
			usleep(5 * 1000);
			continue;
		}

		/* enqueue video queue */
		enqueue(info->uvc_q, info->image_data, info->image_size);

		/* keep queue as 30 fps... */
		usleep(33 * 1000);
	}

	return (void *)0;
}

static int image_start_demux(struct media_info *info)
{
	int r = -1;

	if (!info)
		return r;

	/* create video queue */
	info->uvc_q = queue_create(3);	/* 3 elements in video queue */

	r = pthread_create(&info->image_demux_tid, NULL, image_demux_routine, info);
	if (r < 0)
		fprintf(stderr, "create image demux routine failed\n");

	return r;
}

static void image_stop_demux(struct media_info *info)
{
	if (!info)
		return;

	if (info->streamon)
		info->streamon = 0;
	info->has_stream = 0;

	/* send condition signal */
	pthread_mutex_lock(&info->mutex);
	pthread_cond_signal(&info->cond);
	pthread_mutex_unlock(&info->mutex);

	if (pthread_join(info->image_demux_tid, NULL) < 0)
		printf("pthread join failed\n");

	/* destroy video queue */
	queue_destroy(info->uvc_q);
	info->uvc_q = NULL;

	return;
}

static int stream_load(struct media_info *info, const char *stream_file)
{
	int r;

	if (!stream_file || !info)
		return 1;

	printf("stream_load file_name: %s\n", stream_file);
	/* open input file, and allocate format context */
	if (avformat_open_input(&info->fmt_ctx, stream_file, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", stream_file);
		return 1;
	}

	printf("probesize: %ld\n", info->fmt_ctx->probesize);

	/* retrieve stream information */
	if (avformat_find_stream_info(info->fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		return 1;
	}

	r = av_find_best_stream(info->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL,
				0);
	if (r < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
			av_get_media_type_string(AVMEDIA_TYPE_VIDEO),
			stream_file);
		return 1;
	}
	info->video_index = r;

	/* dump input information to stderr */
	av_dump_format(info->fmt_ctx, 0, stream_file, 0);

	/* av_init_packet api is deprecated, use av_packet_unref instead to init packet */
	av_packet_unref(&info->pkt);

#ifdef UVC_DUMP_EARLY
	printf("uvc_dump_early!!\n");
	int count = 0;
	int fd = open("/userdata/h264_dump.h264", O_CREAT | O_RDWR, 0755);
	if (!fd)
		return 1;
	/* read frames from the file */
	AVPacket *pkt = &info->ptk;
	while (av_read_frame(info->fmt_ctx, pkt) >= 0) {
		printf("av_read_frame succeed, stream_index = %d, "
		       "info->video_index = %d, data [%p], size [%d]\n",
		       pkt->stream_index, info->video_index,
		       pkt->data, pkt->size);
		if (pkt->stream_index == info->video_index) {
			count++;
			write(fd, pkt->data, pkt->size);
		}
	}

	printf("av_read_frame succeed totally %d frames\n", count);
	close(fd);
#endif

#ifdef UVC_DUMP
	info->dump_fd = open("/userdata/uvc.dump", O_CREAT | O_RDWR, 0755);
	if (!info->dump_fd)
		printf("open /userdata/uvc.dump failed\n");
#endif

	return 0;
}

static void stream_close(struct media_info *info)
{
	if (!info || !info->fmt_ctx)
		return;

	printf("stream_close\n");
	avformat_close_input(&info->fmt_ctx);

#ifdef UVC_DUMP
	if (info->dump_fd) {
		close(info->dump_fd);
		info->dump_fd = 0;
	}
#endif

	return;
}

static int stream_get_uvc_frame(struct uvc_context *ctx,
				void **buf_to, int *buf_len, void **entity,
				void *userdata)
{
	struct media_info *info = (struct media_info *)userdata;
	struct uvc_device *dev;
	void *deque_frame;
	unsigned int frame_size = 0;
	unsigned int width, height, fcc;

	if (!info || !ctx || !ctx->udev)
		return -EINVAL;

#ifdef UVC_DEBUG
	interval_cost_trace_in();
#endif

	dev = ctx->udev;
	fcc = dev->fcc;
	width = dev->width;
	height = dev->height;

	/* still keep yuyv & nv12 to used default pattern */
	if (fcc == V4L2_PIX_FMT_YUYV || fcc == V4L2_PIX_FMT_NV12)
		return 0;

	/* check request stream format */
	if (fcc != uvc_format_to_fcc(info->format) ||
	    width != info->width || height != info->height)
		return -EINVAL;

	/* read frames from the file */
	/* dequeue buffer from uvc_q */
	deque_frame = dequeue(info->uvc_q, &frame_size);

	if (!deque_frame || !frame_size) {
#if 0
		printf("buffer underflow!!! deque_frame(%p), frame_size(%u)\n",
		       deque_frame, frame_size);
#endif
		return -EFAULT;
	}

	/* apply frame to *buf_to */
	*buf_to = deque_frame;
	*buf_len = frame_size;
	*entity = deque_frame;

#ifdef UVC_DEBUG
	interval_cost_trace_out(*buf_len);
#endif

	return 0;
}

static void stream_release_uvc_frame(struct uvc_context *ctx,
				     void **entity, void *userdata)
{
	if (!ctx || !entity || !(*entity))
		return;

	void *frame = *entity;
	free(frame);

	*entity = 0;

	return;
}

static int get_uvc_frame(struct uvc_context *ctx,
			 void **buf_to, int *buf_len, void **entity,
			 void *userdata)
{
	struct media_info *info = (struct media_info *)userdata;
	struct uvc_device *dev;
	AVPacket *pkt;
	unsigned int width, height, fcc;
	static int cnt = 0;
	int got_frame = 0;

#ifdef UVC_DEBUG
	interval_cost_trace_in();
#endif

	if (!info || !ctx || !ctx->udev)
		return -EINVAL;

	dev = ctx->udev;
	fcc = dev->fcc;
	width = dev->width;
	height = dev->height;

	/* current just for h264@720p... */
	if (fcc != uvc_format_to_fcc(info->format) ||
	    width != info->width || height != info->height) {
#if 0
		printf
		    ("format not matched, request format: fcc(0x%x), width(%u), "
		     "height(%u), ready format: fcc(0x%x), width(%u), height(%u)\n",
		     dev->fcc, width, height, uvc_format_to_fcc(info->format),
		     info->width, info->height);
#endif
		return -EINVAL;
	}

	/* read frames from the file */
	pkt = &info->pkt;
	if (av_read_frame(info->fmt_ctx, pkt) >= 0) {
		got_frame = 1;
	}

	if (!got_frame)
		goto eof_handle;

#ifdef UVC_DEBUG
	printf("av_read_frame(%d) suceed, data: %p, size: %d\n",
	       cnt++, pkt->data, pkt->size);
#endif

#ifdef UVC_DUMP
	if (info->dump_fd)
		write(info->dump_fd, pkt->data, pkt->size);
#endif

	/* apply frame to *buf_to */
	*buf_to = pkt->data;
	*buf_len = pkt->size;
	*entity = pkt;

#ifdef UVC_DEBUG
	interval_cost_trace_out(*buf_len);
#endif

	return 0;

eof_handle:
	printf("doesn't got frame yet...maybe end of stream, "
	       "seek back to beginning\n");
	avformat_seek_file(info->fmt_ctx, -1, INT64_MIN, 0, INT64_MAX,
			   AVSEEK_FLAG_BYTE);

	return -EFAULT;
}

static void release_uvc_frame(struct uvc_context *ctx,
			      void **entity, void *userdata)
{
	if (!ctx || !entity || !(*entity))
		return;

	AVPacket *pkt = (AVPacket *) * entity;
	*entity = 0;

	av_packet_unref(pkt);

	return;
}

static void uvc_streamon_off(struct uvc_context *ctx, int is_on, void *userdata)
{
	struct media_info *info = (struct media_info *)userdata;
	struct uvc_device *dev;
	unsigned int width, height, fcc;

	if (!info || !ctx || !ctx->udev)
		return;

	dev = ctx->udev;
	fcc = dev->fcc;
	width = dev->width;
	height = dev->height;

	printf("##STREAMON(%d)## %s(%ux%u)\n", is_on,
	       fcc_to_string(fcc), width, height);

	/* yuyv & nv12 has default pattern, streamon anyway! */
	if ((fcc == uvc_format_to_fcc(info->format) &&
	    width == info->width && height == info->height)
	    || fcc == V4L2_PIX_FMT_YUYV
	    || fcc == V4L2_PIX_FMT_NV12) {
		printf("##REAL STREAMON(%d)## %s(%ux%u)\n", is_on,
		       fcc_to_string(fcc), width, height);
		/* stream off, do flush here.
		 * stream on is handled in fill 1st uvc buffer!!
		 */
		if (is_on) {
			printf("stream on!! send condition signal...\n");
			/* send condition signal */
			info->streamon = 1;
			if (fcc == uvc_format_to_fcc(info->format))
				info->has_stream = 1;

			pthread_mutex_lock(&info->mutex);
			pthread_cond_signal(&info->cond);
			pthread_mutex_unlock(&info->mutex);
		} else {
			printf
			    ("stream off!! seek back to beginning of the stream\n");
			printf("stream off!! flush the queue!\n");
			if (fcc == V4L2_PIX_FMT_MJPEG || fcc == V4L2_PIX_FMT_H264)
				avformat_seek_file(info->fmt_ctx, -1, INT64_MIN, 0,
						   INT64_MAX, AVSEEK_FLAG_BYTE);
		}
	}
}

static int uvc_event_setup_handle(struct uvc_context *ctx,
				uint8_t req, uint8_t cs, uint8_t entity_id,
				struct uvc_request_data *resp,
				void *userdata)
{
	int ret = -1;

	switch (entity_id) {
		case UVC_CTRL_CAMERA_TERMINAL_ID:
			ret = uvc_camera_terminal_setup_event(ctx, req, cs, resp, userdata);
			break;

		case UVC_CTRL_PROCESSING_UNIT_ID:
			ret = uvc_process_unit_setup_event(ctx, req, cs, resp, userdata);
			break;

		case UVC_CTRL_EXTENSION_UNIT_ID:
			ret = uvc_extension_unit_setup_event(ctx, req, cs, resp, userdata);
			break;

		default:
			printf("EVENT NOT CARE, entity_id(0x%x), req(0x%x), cs(0x%x)\n",
					entity_id, req, cs);
			break;
	}

	return ret;
}

static int uvc_event_data_handle(struct uvc_context *ctx,
				uint8_t req, uint8_t cs, uint8_t entity_id,
				struct uvc_request_data *data,
				void *userdata)
{
	int ret = -1;

	switch (entity_id) {
		case UVC_CTRL_CAMERA_TERMINAL_ID:
			ret = uvc_camera_terminal_data_event(ctx, req, cs, data, userdata);
			break;

		case UVC_CTRL_PROCESSING_UNIT_ID:
			ret = uvc_process_unit_data_event(ctx, req, cs, data, userdata);
			break;

		case UVC_CTRL_EXTENSION_UNIT_ID:
			ret = uvc_extension_unit_data_event(ctx, req, cs, data, userdata);
			break;

		default:
			printf("EVENT NOT CARE, entity_id(0x%x), req(0x%x), cs(0x%x)\n",
					entity_id, req, cs);
			break;
	}

	return ret;
}

static void *demux_routine(void *param)
{
	struct media_info *info = (struct media_info *)param;
	AVPacket *pkt;
	int got_frame;

	if (!info)
		return (void *)0;

	/* wait stream on condition */
	pthread_mutex_lock(&info->mutex);
	pthread_cond_wait(&info->cond, &info->mutex);
	pthread_mutex_unlock(&info->mutex);

	while (info->streamon && info->has_stream) {
		/* check video queue status */
		if (queue_is_full(info->uvc_q)) {
			usleep(5 * 1000);
			continue;
		}

		got_frame = 0;
		/* read frames from the file */
		pkt = &info->pkt;
		if (av_read_frame(info->fmt_ctx, pkt) >= 0) {
			got_frame = 1;
		}

		if (got_frame) {
			/* enqueue video queue */
			enqueue(info->uvc_q, pkt->data, pkt->size);
			av_packet_unref(pkt);
		} else {
			fprintf(stderr,
				"got frame failed. eof? av seek to beginning.\n");
			avformat_seek_file(info->fmt_ctx, -1, INT64_MIN, 0,
					   INT64_MAX, AVSEEK_FLAG_BYTE);
		}

		/* keep queue as 30 fps... */
		usleep(33 * 1000);
	}

	return (void *)0;
}

static int stream_start_demux(struct media_info *info)
{
	int r = -1;

	if (!info)
		return r;

	/* create video queue */
	info->uvc_q = queue_create(3);	/* 3 elements in video queue */

	r = pthread_create(&info->demux_tid, NULL, demux_routine, info);
	if (r < 0)
		fprintf(stderr, "create demux routine failed\n");

	return r;
}

static void stream_stop_demux(struct media_info *info)
{
	if (!info)
		return;

	if (info->streamon)
		info->streamon = 0;
	info->has_stream = 0;

	/* send condition signal */
	pthread_mutex_lock(&info->mutex);
	pthread_cond_signal(&info->cond);
	pthread_mutex_unlock(&info->mutex);

	if (pthread_join(info->demux_tid, NULL) < 0)
		printf("pthread join failed\n");

	/* destroy video queue */
	queue_destroy(info->uvc_q);
	info->uvc_q = NULL;

	return;
}

static int rawvideo_load(struct media_info *info, const char *rawvideo_file)
{
	int fd;

	if (!rawvideo_file || !info)
		return 1;

	printf("rawvideo_load file_name: %s\n", rawvideo_file);
	fd = open(rawvideo_file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Could not open rawvideo file %s\n", rawvideo_file);
		return 1;
	}

	info->image_size = (unsigned int)(lseek(fd, 0, SEEK_END));;
	lseek(fd, 0, SEEK_SET);
	info->rawvideo_fd = fd;

	if (info->format == UVC_FORMAT_NV12)
		info->frame_size = (unsigned int)(info->width * info->height * 1.5);
	else if (info->format == UVC_FORMAT_YUY2)
		info->frame_size = (unsigned int)(info->width * info->height * 2);

	info->rawvideo_data = calloc(1, info->frame_size);
	if (info->rawvideo_data < 0) {
		fprintf(stderr, "%s calloc failed\n", __func__);
		return 1;
	}

	return 0;
}

static void rawvideo_close(struct media_info *info)
{
	if (!info)
		return;

	printf("rawvideo_close\n");
	if (info->rawvideo_fd > 0)
		close(info->rawvideo_fd);
	info->rawvideo_fd = -1;

	if (info->rawvideo_data)
		free(info->rawvideo_data);
	info->rawvideo_data = 0;

	return;
}

static int rawvideo_get_uvc_frame(struct uvc_context *ctx,
				void **buf_to, int *buf_len, void **entity,
				void *userdata)
{
	struct media_info *info = (struct media_info *)userdata;
	struct uvc_device *dev;
	void *deque_frame;
	unsigned int frame_size = 0;
	unsigned int width, height, fcc;

	if (!info || !ctx || !ctx->udev)
		return -EINVAL;

#ifdef UVC_DEBUG
	interval_cost_trace_in();
#endif

	dev = ctx->udev;
	fcc = dev->fcc;
	width = dev->width;
	height = dev->height;

	/* mjpeg & h264 go through another case */
	if (fcc == V4L2_PIX_FMT_MJPEG || fcc == V4L2_PIX_FMT_H264)
		return 0;

	/* check request stream format */
	if (fcc != uvc_format_to_fcc(info->format) ||
	    width != info->width || height != info->height)
		return -EINVAL;

	/* read frames from the file */
	/* dequeue buffer from uvc_q */
	deque_frame = dequeue(info->uvc_q, &frame_size);

	if (!deque_frame || !frame_size) {
#if 0
		printf("buffer underflow!!! deque_frame(%p), frame_size(%u)\n",
		       deque_frame, frame_size);
#endif
		return -EFAULT;
	}

	/* apply frame to *buf_to */
	*buf_to = deque_frame;
	*buf_len = frame_size;
	*entity = deque_frame;

#ifdef UVC_DEBUG
	interval_cost_trace_out(*buf_len);
#endif

	return 0;
}

static void rawvideo_release_uvc_frame(struct uvc_context *ctx,
				     void **entity, void *userdata)
{
	if (!ctx || !entity || !(*entity))
		return;

	void *frame = *entity;
	free(frame);

	*entity = 0;

	return;
}

static void *rawvideo_demux_routine(void *param)
{
	struct media_info *info = (struct media_info *)param;
	ssize_t rd_size;
	int got_frame;

	if (!info && info->rawvideo_fd < 0)
		return (void *)0;

	/* wait stream on condition */
	pthread_mutex_lock(&info->mutex);
	pthread_cond_wait(&info->cond, &info->mutex);
	pthread_mutex_unlock(&info->mutex);

	while (info->streamon && info->has_stream) {
		/* check video queue status */
		if (queue_is_full(info->uvc_q)) {
			usleep(5 * 1000);
			continue;
		}

		got_frame = 0;
		/* read frames from the file */
		rd_size = read(info->rawvideo_fd, info->rawvideo_data, info->frame_size);
		if (rd_size == info->frame_size) {
			got_frame = 1;
		} else {
			printf("%s read frame size (%ld) < expect (%d). might meet EOS. "
					"seek to beginning\n",
					__func__, rd_size, info->frame_size);
		}

		if (got_frame) {
			/* enqueue video queue, this is a memcpy queue */
			enqueue(info->uvc_q, info->rawvideo_data, (uint32_t)rd_size);
		} else {
			fprintf(stderr,
				"got frame failed. eof? seek to beginning.\n");
			lseek(info->rawvideo_fd, 0, SEEK_SET);
		}

		/* keep queue as 30 fps... */
		usleep(33 * 1000);
	}

	return (void *)0;
}

static int rawvideo_start_demux(struct media_info *info)
{
	int r = -1;

	if (!info)
		return r;

	/* create video queue */
	info->uvc_q = queue_create(3);	/* 3 elements in video queue */

	r = pthread_create(&info->rawvideo_demux_tid, NULL, rawvideo_demux_routine, info);
	if (r < 0)
		fprintf(stderr, "create demux routine failed\n");

	return r;
}

static void rawvideo_stop_demux(struct media_info *info)
{
	if (!info)
		return;

	if (info->streamon)
		info->streamon = 0;
	info->has_stream = 0;

	/* send condition signal */
	pthread_mutex_lock(&info->mutex);
	pthread_cond_signal(&info->cond);
	pthread_mutex_unlock(&info->mutex);

	if (pthread_join(info->rawvideo_demux_tid, NULL) < 0)
		printf("pthread join failed\n");

	/* destroy video queue */
	queue_destroy(info->uvc_q);
	info->uvc_q = NULL;

	return;
}

static void usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [options]\n", argv0);
	fprintf(stderr, "Available options are\n");
	fprintf(stderr, " -b		Use bulk mode\n");
	fprintf(stderr, " -a		use streaming multi alternate setting\n");
	fprintf(stderr,
		" -f <format>    Select frame format\n\t"
		"0 = V4L2_PIX_FMT_YUYV\n\t"
		"1 = V4L2_PIX_FMT_NV12\n\t"
		"2 = V4L2_PIX_FMT_MJPEG\n\t"
		"3 = V4L2_PIX_FMT_H264\n\t"
		"4 = V4L2_PIX_FMT_H265\n");
	fprintf(stderr, " -h		Print this help screen and exit\n");
	fprintf(stderr, " -i image	MJPEG image\n");
	fprintf(stderr,
		" -m		Streaming mult for ISOC (b/w 0 and 2)\n");
	fprintf(stderr,
		" -n		Number of Video buffers (b/w 2 and 32)\n");
	fprintf(stderr,
		" -o <IO method> Select UVC IO method:\n\t" "0 = MMAP\n\t"
		"1 = USER_PTR\n");
	fprintf(stderr,
		" -r <resolution> Select frame resolution:\n\t"
		"0 = 360p, VGA (640x360)\n\t"
		"1 = 720p, WXGA (1280x720)\n\t"
		"2 = 1080p, FHD (1920x1080)\n\t"
		"3 = 2160p, UHD (3840x2160)\n");
	fprintf(stderr,
		" -s <speed>	Select USB bus speed (b/w 0 and 2)\n\t"
		"2 = Full Speed (FS)\n\t"
		"3 = High Speed (HS)\n\t"
		"5 = Super Speed (SS)\n\t"
		"? = Super Speed (SS)\n");
	fprintf(stderr, " -t		Streaming burst (b/w 0 and 15)\n");
	fprintf(stderr,
		" -c <callback type> fill/release callback\n\t"
		"0 = prepare/release with queue\n\t"
		"1 = prepare/release with av_read_frame\n");
	fprintf(stderr, " -p <pattern mode>\n\t"
		"0 = default mode(no test pattern mode)\n\t"
		"1 = single static picture mode\n\t"
		"2 = ddr test pattern mode\n");
	fprintf(stderr,
		" -q <h264 quirk flag>\n\t"
		"0 = default, no h264 quirk behavior\n\t"
		"1 = h264 quirk, will combine sps, pps and IDR frame and feed to uvc\n");
	fprintf(stderr, " -u device	UVC Video Output device\n");
	fprintf(stderr, " -v device	V4L2 Video Capture device\n");
}

int main(int argc, char *argv[])
{
	struct uvc_context *ctx = NULL;
	struct uvc_params params;
	struct media_info info;
	camera_comp_t *cam = NULL;
	char *uvc_devname = "/dev/video4";
	char *v4l2_devname = NULL;
	char *file_name = NULL;

	int default_resolution = 0;	/* VGA 360p */
	int cb_type = 0; /* prepare/release callback type */
	int pattern_mode = 0; /* 0-default, 1 - single picture, 2 - ddr test pattern */
	int opt;

	/* init user params with default value */
	uvc_gadget_user_params_init(&params);

	while ((opt = getopt(argc, argv, "badf:hi:m:n:o:r:s:t:c:q:u:v:p:")) != -1) {
		switch (opt) {
		case 'b':
			params.bulk_mode = 1;
			break;

		case 'a':
			params.mult_alts = 1;  /* uvc use streaming multi alternate settings */
			break;

		case 'f':
			if (atoi(optarg) < 0 || atoi(optarg) > 4) {
				usage(argv[0]);
				return 1;
			}

			params.format = atoi(optarg);
			break;

		case 'h':
			usage(argv[0]);
			return 1;

		case 'i':
			file_name = optarg;
			break;

		case 'm':
			if (atoi(optarg) < 0 || atoi(optarg) > 2) {
				usage(argv[0]);
				return 1;
			}

			params.mult = atoi(optarg);
			printf("Requested Mult value = %d\n", params.mult);
			break;

		case 'n':
			if (atoi(optarg) < 2 || atoi(optarg) > 32) {
				usage(argv[0]);
				return 1;
			}

			params.nbufs = atoi(optarg);
			printf("Number of buffers requested = %d\n",
			       params.nbufs);
			break;

		case 'o':
			if (atoi(optarg) < 0 || atoi(optarg) > 1) {
				usage(argv[0]);
				return 1;
			}

			params.io_method = atoi(optarg);
			printf("UVC: IO method requested is %s\n",
			       (params.io_method ==
				IO_METHOD_MMAP) ? "MMAP" : "USER_PTR");
			break;

		case 'r':
			if (atoi(optarg) < 0 || atoi(optarg) > 3) {
				usage(argv[0]);
				return 1;
			}

			default_resolution = atoi(optarg);
			break;

		case 's':
			if (atoi(optarg) < 0 || atoi(optarg) > 5) {
				usage(argv[0]);
				return 1;
			}

			params.speed = atoi(optarg);
			break;

		case 't':
			if (atoi(optarg) < 0 || atoi(optarg) > 15) {
				usage(argv[0]);
				return 1;
			}

			params.burst = atoi(optarg);
			printf("Requested Burst value = %d\n", params.burst);
			break;

		case 'c':
			if (atoi(optarg) < 0 || atoi(optarg) > 1) {
				usage(argv[0]);
				return 1;
			}

			cb_type = atoi(optarg);
			printf("Requested callback type= %d\n", cb_type);
			break;

		case 'p':
			if (atoi(optarg) < 0 || atoi(optarg) > 2) {
				usage(argv[0]);
				return 1;
			}

			pattern_mode = atoi(optarg);
			printf("Requested pattern mode = %d\n", pattern_mode);
			break;

		case 'q':
			if (atoi(optarg) < 0 || atoi(optarg) > 1) {
				usage(argv[0]);
				return 1;
			}

			params.h264_quirk = atoi(optarg);
			printf("h264 quirk = %d\n", params.h264_quirk);
			break;

		case 'u':
			uvc_devname = optarg;
			break;

		case 'v':
			v4l2_devname = optarg;
			break;

		default:
			printf("Invalid option '-%c'\n", opt);
			usage(argv[0]);
			return 1;
		}
	}

	switch (default_resolution) {
	case 0:
		params.width = 640;
		params.height = 320;
		break;
	case 1:
		params.width = 1280;
		params.height = 720;
		break;
	case 2:
		params.width = 1920;
		params.height = 1080;
		break;
	case 3:
		params.width = 3840;
		params.height = 2160;
		break;
	default:
		printf("Invalid resolution\n");
		return 1;
	}

	memset(&info, 0, sizeof(info));
	info.width = params.width;
	info.height = params.height;
	info.format = params.format;
	pthread_mutex_init(&info.mutex, NULL);
	pthread_cond_init(&info.cond, NULL);

	if (uvc_gadget_init(&ctx, uvc_devname, v4l2_devname, &params)) {
		fprintf(stderr, "uvc_gadget_init failed\n");
		return 1;
	}

	if (pattern_mode == 2) {
		uvc_set_prepare_data_handler(ctx, pattern_get_frame, &info);
		uvc_set_release_data_handler(ctx, pattern_release_frame, &info);

		uvc_set_streamon_handler(ctx, uvc_streamon_off, &info);
	} else if (file_name && pattern_mode == 1) {
		if (image_load(&info, file_name) < 0)
			return -1;

		switch (cb_type) {
		case 0:
			/* prepare/release buffer with video queue */
			uvc_set_prepare_data_handler(ctx, image_queue_get_frame, &info);
			uvc_set_release_data_handler(ctx, image_queue_release_frame, &info);
			break;
		case 1:
			/* prepare/release as soon as possible */
			uvc_set_prepare_data_handler(ctx, image_get_uvc_frame, &info);
			uvc_set_release_data_handler(ctx, image_release_uvc_frame, &info);
			break;
		}

		uvc_set_streamon_handler(ctx, uvc_streamon_off, &info);

		if (!cb_type && image_start_demux(&info) < 0) {
			fprintf(stderr, "stream_start_demux failed\n");
			return 1;
		}
	} else if (file_name && (params.format == UVC_FORMAT_H264 ||
	    params.format == UVC_FORMAT_MJPEG)) {
		if (stream_load(&info, file_name) < 0)
			return -1;

		switch (cb_type) {
		case 0:
			/* prepare/release buffer with video queue */
			uvc_set_prepare_data_handler(ctx, stream_get_uvc_frame,
						     &info);
			uvc_set_release_data_handler(ctx,
						     stream_release_uvc_frame,
						     &info);
			break;
		case 1:
			/* prepare/release with av_read_frame */
			uvc_set_prepare_data_handler(ctx, get_uvc_frame, &info);
			uvc_set_release_data_handler(ctx, release_uvc_frame,
						     &info);
			break;
		}

		uvc_set_streamon_handler(ctx, uvc_streamon_off, &info);

		/* launch stream demux thread and enqueue frame */
		if (stream_start_demux(&info) < 0) {
			fprintf(stderr, "stream_start_demux failed\n");
			return 1;
		}
	} else if (file_name && (params.format == UVC_FORMAT_NV12 ||
			params.format == UVC_FORMAT_YUY2))  {
		if (rawvideo_load(&info, file_name) < 0)
			return -1;

		printf("%s go rawvideo path\n", __func__);

		switch (cb_type) {
		case 0:
			/* prepare/release buffer with video queue */
			uvc_set_prepare_data_handler(ctx, rawvideo_get_uvc_frame,
						     &info);
			uvc_set_release_data_handler(ctx,
						     rawvideo_release_uvc_frame,
						     &info);
			break;
		case 1:
			fprintf(stderr, "cb_type(%d) not support for rawvideo format\n",
					cb_type);
			return 1;
		}

		uvc_set_streamon_handler(ctx, uvc_streamon_off, &info);

		if (camera_open(&cam) < 0) {
			fprintf(stderr, "camera_open failed\n");
			return 1;
		}

		/* launch rawvideo demux thread and enqueue frame */
		if (rawvideo_start_demux(&info) < 0) {
			fprintf(stderr, "rawvideo_start_demux failed\n");
			return 1;
		}
	}

	if (camera_open(&cam) < 0) {
		fprintf(stderr, "camera_open failed\n");
		return 1;
	}

	/* init detail control request function list... */
	uvc_control_request_param_init();
	uvc_control_request_callback_init();
	uvc_set_event_handler(ctx, uvc_event_setup_handle, uvc_event_data_handle,
			UVC_CTRL_CAMERA_TERMINAL_EVENT |
			UVC_CTRL_PROCESSING_UNIT_EVENT |
			UVC_CTRL_EXTENSION_UNIT_EVENT,
			cam);

	if (uvc_gadget_start(ctx) < 0) {
		fprintf(stderr, "uvc_gadget_start failed\n");
		return 1;
	}

	printf("'q' for exit\n");
	while (getchar() != 'q') ;

	if (uvc_gadget_stop(ctx) < 0)
		fprintf(stderr, "uvc_gdaget_stop failed\n");

	if (pattern_mode == 2) {
		// do nothing
	} else if (pattern_mode == 1 && file_name) {
		if (!cb_type)
			image_stop_demux(&info);

		image_close(&info);
	} else if (file_name && (params.format == UVC_FORMAT_H264 ||
				params.format == UVC_FORMAT_MJPEG)) {
		stream_stop_demux(&info);

		stream_close(&info);
	} else if (file_name && (params.format == UVC_FORMAT_NV12 ||
			params.format == UVC_FORMAT_YUY2))  {
		rawvideo_stop_demux(&info);

		rawvideo_close(&info);
	}

	camera_close(cam);

	uvc_gadget_deinit(ctx);

	return 0;
}
