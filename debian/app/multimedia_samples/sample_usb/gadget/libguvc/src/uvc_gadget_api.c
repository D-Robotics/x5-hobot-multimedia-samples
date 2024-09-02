/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * uvc_gadget_api.c
 *	uvc gadget api implementation for uvc gadget application development.
 *
 * Copyright (C) 2019 Horizon Robotics, Inc.
 *
 * Contact: jianghe xu<jianghe.xu@horizon.ai>
 */

#include <stdio.h>
#include <pthread.h>
#include <dirent.h>

#include "utils.h"
#include "uvc_gadget_api.h"

/* function declare */
static void *uvc_loop(void *arg);
static char *udc_find_video_device(const char *udc, const char *function);
static char *uvc_configfs_get_options(const char *dir, char *option);

static char *path_join(const char *dirname, const char *name)
{
	char *path;
	int ret;

	/* path is malloc in heap, return string to caller, who needs to free it... */
	ret = asprintf(&path, "%s/%s", dirname, name);

	if (ret < 0)
		path = NULL;

	return path;
}

/* function definitions */
void uvc_gadget_user_params_init(struct uvc_params *params)
{
	/* default user params
	 * YUY2, VGA360p, io_method_userptr, isoc mode, ping-pong buffers,
	 * usb2.0 high speed...
	 */
	params->width = 1280;
	params->height = 720;
	params->format = UVC_FORMAT_MJPEG;
	params->io_method = IO_METHOD_MMAP;
	params->bulk_mode = 0;
	params->nbufs = 2;	/* Ping-Pong buffers */
	params->mult = 0;
	params->burst = 0;
	params->speed = 1;	/* high speed as default */
	params->mult_alts = 0;	/* no need streaming multi altsetting */
	params->h264_quirk = 0;  /* h264_quirk: 0 - default, 1 - combine sps, pps with IDR */
	params->maxpkt_quirk = 3072;
}

int uvc_gadget_init(struct uvc_context **pctx, char *uvc_devname,
		    char *v4l2_devname, struct uvc_params *user_params)
{
	struct uvc_device *udev;
	struct v4l2_device *vdev;
	struct v4l2_format fmt;
	struct uvc_params params;
	struct uvc_context *ctx = NULL;
	char *control_intf = NULL;
	char *streaming_intf = NULL;
	const char *control_intf_path = "/sys/kernel/config/usb_gadget/g_comp/functions/uvc.0/control";
	const char *streaming_intf_path = "/sys/kernel/config/usb_gadget/g_comp/functions/uvc.0/streaming";
	char *devname = "/dev/video0";
	char *udc_name = "b2000000.dwc3";
	char *function1 = "g_webcam";
	char *function2 = "g_comp";
	int need_free = 0;
	int ret;

	trace_in();

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return -1;

	if (uvc_devname) {
		devname = uvc_devname;
	} else {
		printf("udc_find_video_device %s\n", function1);
		devname = udc_find_video_device(udc_name, function1);
		if (!devname) {
			printf("udc_find_video_device %s\n", function2);
			devname = udc_find_video_device(udc_name, function2);
			if (!devname) {
				printf("no matched uvc device(%s or %s) found!",
				       function1, function2);
				goto error1;
			}
		}

		need_free = 1;
	}

	printf("using uvc device: %s\n", devname);

	/* if no user_params, use default params */
	uvc_gadget_user_params_init(&params);
	if (user_params) {
		memcpy(&params, user_params, sizeof(params));
	}

	printf("###uvc_gadget_init###\n");
	printf("using uvc device: %s\n", devname);
	printf("width: %u\n", params.width);
	printf("height: %u\n", params.height);
	printf("format: %u\n", params.format);
	printf("io_method: %u\n", params.io_method);
	printf("bulk_mode: %u\n", params.bulk_mode);
	printf("nbufs: %u\n", params.nbufs);
	printf("mult: %u\n", params.mult);
	printf("burst: %u\n", params.burst);
	printf("speed: %u\n", params.speed);
	printf("mult_alts: %u\n", params.mult_alts);
	printf("h264_quirk: %d\n", params.h264_quirk);
	printf("maxpkt_quirk: %d\n", params.maxpkt_quirk);

	if (v4l2_devname) {
		/*
		 * Try to set the default format at the V4L2 video capture
		 * device as requested by the user.
		 */
		CLEAR(fmt);
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = params.width;
		fmt.fmt.pix.height = params.height;
		fmt.fmt.pix.field = V4L2_FIELD_ANY;
		switch (params.format) {
		case UVC_FORMAT_YUY2:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
			fmt.fmt.pix.sizeimage =
			    (uint32_t)(params.width * params.height * 2);
			break;
		case UVC_FORMAT_NV12:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
			fmt.fmt.pix.sizeimage =
			    (uint32_t)(params.width * params.height * 1.5);
			break;
		case UVC_FORMAT_MJPEG:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
			fmt.fmt.pix.sizeimage =
			    (uint32_t)(params.width * params.height * 1.5 / 2.0);
			break;
		case UVC_FORMAT_H264:
		case UVC_FORMAT_H265:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
			fmt.fmt.pix.sizeimage =
			    (uint32_t)(params.width * params.height * 1.5 / 2.0);
			break;
		}

		/* Open the V4L2 device. */
		ret = v4l2_open(&vdev, v4l2_devname, &fmt);
		if (vdev == NULL || ret < 0)
			goto error1;
	}

	/* Open the UVC device. */
	ret = uvc_open(&udev, devname);
	if (udev == NULL || ret < 0)
		goto error2;

	strncpy(udev->uvc_devname, devname, 128);
	udev->uvc_devname[127] = '\0';
	if (need_free)
		free(devname);

	udev->parent = ctx;

	if (v4l2_devname) {
		vdev->v4l2_devname = v4l2_devname;
		/* Bind UVC and V4L2 devices. */
		udev->vdev = vdev;
		vdev->udev = udev;
	}

	/* Set parameters as passed by user. */
	switch (params.format) {
	case UVC_FORMAT_YUY2:
		udev->fcc = V4L2_PIX_FMT_YUYV;
		udev->imgsize = (uint32_t)(udev->width * udev->height * 2);
		break;
	case UVC_FORMAT_NV12:
		udev->fcc = V4L2_PIX_FMT_NV12;
		udev->imgsize = (uint32_t)(udev->width * udev->height * 1.5);
		break;
	case UVC_FORMAT_MJPEG:
		udev->fcc = V4L2_PIX_FMT_MJPEG;
		udev->imgsize = (uint32_t)(udev->width * udev->height * 1.5 / 2.0);
		break;
	case UVC_FORMAT_H264:
		udev->fcc = V4L2_PIX_FMT_H264;
		udev->imgsize = (uint32_t)(udev->width * udev->height * 1.5 / 2.0);
		break;
	case UVC_FORMAT_H265:
		udev->fcc = V4L2_PIX_FMT_H265;
		udev->imgsize = (uint32_t)(udev->width * udev->height * 1.5 / 2.0);
		break;
	}
	udev->width = params.width;
	udev->height = params.height;
	udev->io = params.io_method;
	udev->bulk = params.bulk_mode;
	udev->nbufs = params.nbufs;
	udev->mult = params.mult;
	udev->burst = params.burst;
	udev->speed = params.speed;
	udev->mult_alts = params.mult_alts;
	udev->h264_quirk = params.h264_quirk;
	udev->maxpkt_quirk = params.maxpkt_quirk;

	if (!v4l2_devname) {
		/* UVC standalone setup. */
		udev->run_standalone = 1;
	}

	if (v4l2_devname) {
		/* UVC - V4L2 integrated path */
		vdev->nbufs = params.nbufs;

		/*
		 * IO methods used at UVC and V4L2 domains must be
		 * complementary to avoid any memcpy from the CPU.
		 */
		switch (params.io_method) {
		case IO_METHOD_MMAP:
			vdev->io = IO_METHOD_USERPTR;
			break;

		case IO_METHOD_USERPTR:
		default:
			vdev->io = IO_METHOD_MMAP;
			break;
		}
	}

	switch (params.speed) {
	case USB_SPEED_FULL:
		/* Full Speed. */
		if (params.bulk_mode)
			udev->maxpkt = 64;
		else
			udev->maxpkt = 1023;
		break;

	case USB_SPEED_HIGH:
		/* High Speed. */
		if (params.bulk_mode)
			udev->maxpkt = 512;
		else
			udev->maxpkt = 1024;
		break;

	case USB_SPEED_SUPER:
	default:
		/* Super Speed. */
		if (params.bulk_mode)
			udev->maxpkt = 1024;
		else
			udev->maxpkt = 1024;
		break;
	}

	if (v4l2_devname && (IO_METHOD_MMAP == vdev->io)) {
		/*
		 * Ensure that the V4L2 video capture device has already some
		 * buffers queued.
		 */
		v4l2_reqbufs(vdev, vdev->nbufs);
	}

	/* set max packet size for usb isoc */
	uvc_set_maxpkt_quirk(udev);

	/* Init UVC events. */
	uvc_events_init(udev);

	/* hard code to get configfs uvc function's control/streaming interface */
	udev->control_interface = UVC_INTF_CONTROL;
	udev->streaming_interface = UVC_INTF_STREAMING;
	control_intf = uvc_configfs_get_options(control_intf_path, "bInterfaceNumber");
	if (control_intf) {
		udev->control_interface = atoi(control_intf);
		free(control_intf);
	}

	streaming_intf = uvc_configfs_get_options(streaming_intf_path, "bInterfaceNumber");
	if (streaming_intf) {
		udev->streaming_interface = atoi(streaming_intf);
		free(streaming_intf);
	}

	ctx->udev = udev;
	if (v4l2_devname)
		ctx->vdev = vdev;
	else
		ctx->vdev = NULL;

	ctx->uvc_pid = 0;
	ctx->exit = 0;

	*pctx = ctx;

	trace_out();

	return 0;

error2:
	if (v4l2_devname)
		v4l2_close(vdev);
error1:
	if (need_free)
		free(devname);

	if (ctx)
		free(ctx);

	return -1;
}

void uvc_set_prepare_data_handler(struct uvc_context *ctx,
				  uvc_prepare_buffer_callback_fn cb_fn,
				  void *userdata)
{
	if (!ctx || !ctx->udev)
		return;

	struct uvc_device *udev = ctx->udev;

	udev->prepare_cb.cb = cb_fn;
	udev->prepare_cb.userdata = userdata;

	return;
}

void uvc_set_release_data_handler(struct uvc_context *ctx,
				  uvc_release_buffer_callback_fn cb_fn,
				  void *userdata)
{
	if (!ctx || !ctx->udev)
		return;

	struct uvc_device *udev = ctx->udev;

	udev->release_cb.cb = cb_fn;
	udev->release_cb.userdata = userdata;

	return;
}

void uvc_set_streamon_handler(struct uvc_context *ctx,
			      uvc_streamon_callback_fn cb_fn, void *userdata)
{
	if (!ctx || !ctx->udev)
		return;

	struct uvc_device *udev = ctx->udev;

	udev->streamon_cb.cb = cb_fn;
	udev->streamon_cb.userdata = userdata;

	return;
}

void uvc_set_event_handler(struct uvc_context *ctx,
				  uvc_event_setup_callback_fn setup_f,
				  uvc_event_data_callback_fn data_f,
				  unsigned int mask,
				  void *userdata)
{
	if (!ctx || !ctx->udev)
		return;

	struct uvc_device *udev = ctx->udev;

	udev->event_cb.setup_f = setup_f;
	udev->event_cb.data_f = data_f;
	udev->event_cb.userdata = userdata;
	udev->event_mask = mask;

	return;
}

int uvc_gadget_start(struct uvc_context *ctx)
{
	int ret = -1;

	trace_in();

	if (!ctx)
		return -1;

	ret = pthread_create(&ctx->uvc_pid, NULL, uvc_loop, ctx);
	if (ret < 0) {
		fprintf(stderr, "uvc_loop thread launch failed\n");
		return -1;
	}

	trace_out();

	return 0;
}

int uvc_gadget_stop(struct uvc_context *ctx)
{
	struct uvc_device *udev;
	struct v4l2_device *vdev;
	uvc_streamon_callback_fn streamon_notify;
	void *userdata;

	trace_in();

	if (!ctx)
		return -1;

	udev = ctx->udev;
	vdev = ctx->vdev;

	ctx->exit = 1;
	if (pthread_join(ctx->uvc_pid, NULL) < 0) {
		fprintf(stderr, "phtread join failed\n");
		return -1;
	}

	if (vdev && vdev->is_streaming) {
		/* Stop V4L2 streaming... */
		v4l2_stop_capturing(vdev);
		v4l2_uninit_device(vdev);
		v4l2_reqbufs(vdev, 0);
		vdev->is_streaming = 0;
	}

	if (udev->is_streaming) {
		/* ... and now UVC streaming.. */
		uvc_video_stream(udev, 0);
		uvc_uninit_device(udev);
		uvc_video_reqbufs(udev, 0);
		udev->is_streaming = 0;

		/* notify app streamoff */
		streamon_notify = udev->streamon_cb.cb;
		userdata = udev->streamon_cb.userdata;
		if (streamon_notify)
			streamon_notify(udev->parent, 0, userdata);
	}

	trace_out();

	return 0;
}

void uvc_gadget_deinit(struct uvc_context *ctx)
{
	struct uvc_device *udev;
	struct v4l2_device *vdev;

	trace_in();

	if (!ctx)
		return;

	udev = ctx->udev;
	vdev = ctx->vdev;

	if (vdev)
		v4l2_close(vdev);

	uvc_close(udev);

	free(ctx);
	ctx = NULL;

	trace_out();
}

/* uvc main loop */
static void *uvc_loop(void *arg)
{
	struct uvc_device *udev;
	struct v4l2_device *vdev;
	struct timeval tv;
	struct uvc_context *ctx = (struct uvc_context *)arg;

	fd_set fdsv, fdsu;
	int ret, nfds;

	trace_in();

	if (!ctx) {
		fprintf(stderr, "no uvc context.\n");
		return (void *)0;
	}

	udev = ctx->udev;
	vdev = ctx->vdev;

	while (!ctx->exit) {
		if (vdev)
			FD_ZERO(&fdsv);

		FD_ZERO(&fdsu);

		/* We want both setup and data events on UVC interface.. */
		FD_SET(udev->uvc_fd, &fdsu);

		fd_set efds = fdsu;
		fd_set dfds = fdsu;

		/* ..but only data events on V4L2 interface */
		if (vdev)
			FD_SET(vdev->v4l2_fd, &fdsv);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		if (vdev) {
			nfds = max(vdev->v4l2_fd, udev->uvc_fd);
			ret = select(nfds + 1, &fdsv, &dfds, &efds, &tv);
		} else {
			ret = select(udev->uvc_fd + 1, NULL, &dfds, &efds, &tv);
		}

		if (ctx->exit)
			break;

		if (-1 == ret) {
			printf("select error %d, %s\n", errno, strerror(errno));
			if (EINTR == errno)
				continue;

			break;
		}

		if (0 == ret) {
			printf("select timeout\n");
			/* don't break util ctx->exit to support user switch/reopen camera */
			continue;
		}

		if (FD_ISSET(udev->uvc_fd, &efds))
			uvc_events_process(udev);
		if (FD_ISSET(udev->uvc_fd, &dfds))
			uvc_video_process(udev);
		if (vdev)
			if (FD_ISSET(vdev->v4l2_fd, &fdsv))
				v4l2_process_data(vdev);
	}

	trace_out();

	return (void *)0;
}

static char *udc_find_video_device(const char *udc, const char *function)
{
	DIR *dir;
	struct dirent *dirent;
	char fpath[128];
	char vpath[128];
	char func_name[64];
	char *video = NULL;
	int found = 0;
	ssize_t ret;
	int fd;

	snprintf(fpath, 128, "/sys/class/udc/%s/function", udc);
	snprintf(vpath, 128, "/sys/class/udc/%s/device/gadget/video4linux/",
		 udc);

	fd = open(fpath, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s failed\n", fpath);
		goto error;
	}

	ret = read(fd, func_name, 63);
	if (ret <= 0) {
		fprintf(stderr, "read %s failed\n", fpath);
		goto no_func;
	}

	func_name[ret] = '\0';
	close(fd);

	if (strncmp(func_name, function, strlen(function)) != 0) {
		fprintf(stderr, "function name not matched. %s:%s\n",
			func_name, function);
		goto error;
	}

	dir = opendir(vpath);
	if (!dir) {
		fprintf(stderr, "opendir %s failed\n", vpath);
		goto error;
	} else {
		while ((dirent = readdir(dir)) != NULL) {
			if (!strncmp(dirent->d_name, "video", 5)) {
				found = 1;
				break;
			}
		}

		if (found) {
			// TODO: video devname memleak, caller not free currently
			video = path_join("/dev", dirent->d_name);
		}

		closedir(dir);
	}

	return video;

no_func:
	close(fd);

error:
	return NULL;
}

static char *uvc_configfs_get_options(const char *dir, char *option)
{
	char path[128];
	char data[64];
	char *result = NULL;
	ssize_t ret;
	int fd;

	if (!dir || !option)
		return NULL;

	snprintf(path, 128, "%s/%s", dir, option);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s failed\n", path);
		goto error1;
	}

	ret = read(fd, data, 63);
	if (ret <= 0) {
		fprintf(stderr, "read %s failed\n", path);
		goto error2;
	}

	data[ret] = '\0';
	close(fd);

	/* result is malloc in heap, return string to caller, who needs to free it... */
	ret = asprintf(&result, "%s", data);
	if (ret < 0)
		result = NULL;

	return result;

error2:
	close(fd);

error1:
	return NULL;
}

unsigned int uvc_format_to_fcc(enum uvc_fourcc_format format)
{
	unsigned int fcc;
	switch (format) {
	case UVC_FORMAT_YUY2:
		fcc = V4L2_PIX_FMT_YUYV;
		break;
	case UVC_FORMAT_NV12:
		fcc = V4L2_PIX_FMT_NV12;
		break;
	case UVC_FORMAT_MJPEG:
		fcc = V4L2_PIX_FMT_MJPEG;
		break;
	case UVC_FORMAT_H264:
		fcc = V4L2_PIX_FMT_H264;
		break;
	case UVC_FORMAT_H265:
		fcc = V4L2_PIX_FMT_H265;
		break;
	default:
		fcc = 0;
		break;
	}

	return fcc;
}
