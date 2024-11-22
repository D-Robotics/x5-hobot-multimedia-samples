/*
 * UVC gadget test application
 *
 * Copyright (C) 2010 Ideas on board SPRL <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

#include "uvc_gadget.h"
#include "utils.h"

/* Enable debug prints. */
#define ENABLE_BUFFER_DEBUG
#define ENABLE_BUFFER_TRACE
#define ENABLE_USB_REQUEST_DEBUG
#undef ENABLE_BUFFER_DEBUG	/* uncomment for buffer debug */
#undef ENABLE_BUFFER_TRACE	/* uncomment for fill buffer interval trace */
#undef ENABLE_USB_REQUEST_DEBUG	/* uncomment for request debug */

/*
 * The UVC webcam gadget kernel driver (g_webcam.ko) supports changing
 * the Brightness attribute of the Processing Unit (PU). by default. If
 * the underlying video capture device supports changing the Brightness
 * attribute of the image being acquired (like the Virtual Video, VIVI
 * driver), then we should route this UVC request to the respective
 * video capture device.
 *
 * Incase, there is no actual video capture device associated with the
 * UVC gadget and we wish to use this application as the final
 * destination of the UVC specific requests then we should return
 * pre-cooked (static) responses to GET_CUR(BRIGHTNESS) and
 * SET_CUR(BRIGHTNESS) commands to keep command verifier test tools like
 * UVC class specific test suite of USBCV, happy.
 *
 * Note that the values taken below are in sync with the VIVI driver and
 * must be changed for your specific video capture device. These values
 * also work well in case there in no actual video capture device.
 */
#define PU_BRIGHTNESS_MIN_VAL 0
#define PU_BRIGHTNESS_MAX_VAL 255
#define PU_BRIGHTNESS_STEP_SIZE 1
#define PU_BRIGHTNESS_DEFAULT_VAL 127

#define UVC_MAX_TRB_SIZE	0x400000 /* check dwc3/gadget.c */
#define UVC_BRINGUP

/* ---------------------------------------------------------------------------
 * UVC specific stuff
 */

struct uvc_frame_info {
	unsigned int width;
	unsigned int height;
	unsigned int intervals[8];
	unsigned int maxpkt_size;
};

struct uvc_format_info {
	unsigned int fcc;
	struct uvc_frame_info *frames;
};


#if 0
multi bandwidth setting
reference: usb/gadget/function/f_uvc.c

#define MAX_ALTSETTING_NUM			11
static u16 isoc_max_packet_size[MAX_ALTSETTING_NUM] =
	{0xc0, 0x180, 0x200, 0x280, 0x320, 0x3b0, 0xa80, 0xb20, 0xbe0, 0x13c0, 0x1400};
	/* {1x192, 1x384, 1x512, 1x640, 1x800, 1x944, 2x640, 2x800, 2x992, 3x960, 3x1024} */
	/* {192, 384, 512, 640, 800, 944, 1280, 1600, 1984, 2880, 3072} */
#endif

#ifdef UVC_BRINGUP
static struct uvc_frame_info uvc_frames_yuyv[] = {
	{ 1280, 720, { 333333, 0 }, 3072 }, /* Note: 720p */
	{ 0, 0, { 0, }, 0},
};

static struct uvc_frame_info uvc_frames_mjpeg[] = {
	{ 1280, 720, { 333333, 0 }, 384 }, /* Note: 720p, 80KB */
	{ 1920, 1080, { 333333, 0 }, 800 }, /* Note: 1080p, 200KB */
	{ 0, 0, { 0, }, },
};

static struct uvc_frame_info uvc_frames_h264[] = {
	{ 1280, 720, { 333333, 0 }, 3072 }, /* Note: 720p */
	{ 1920, 1080, { 333333, 0 }, 3072 }, /* Note: 1080p */
	{ 0, 0, { 0, }, },
};

static struct uvc_format_info uvc_formats[] = {
	// {V4L2_PIX_FMT_NV12, uvc_frames_nv12},
	{V4L2_PIX_FMT_YUYV, uvc_frames_yuyv},
	{V4L2_PIX_FMT_H264, uvc_frames_h264},
	{V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg},
	// {V4L2_PIX_FMT_H265, uvc_frames_h265},
};

#else
static struct uvc_frame_info uvc_frames_yuyv[] = {
	{ 640, 360, { 333333, 666666, 1000000, 0 }, 1984 }, /* Note: 360p */
	{ 800, 600, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 600p */
	{ 1024, 576, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 576p */
	{ 1280, 720, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 720p */
	{ 1920, 1080, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1080p */
	{ 2560, 1440, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1440p */
	{ 3840, 2160, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 2160p */
	{ 0, 0, { 0, }, 0},
};

static struct uvc_frame_info uvc_frames_nv12[] = {
	{ 640, 360, { 333333, 666666, 1000000, 0 }, 1600 }, /* Note: 360p */
	{ 800, 600, { 333333, 666666, 1000000, 0 }, 2880 }, /* Note: 600p */
	{ 1024, 576, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 576p */
	{ 1280, 720, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 720p */
	{ 1920, 1080, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1080p */
	{ 2560, 1440, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1440p */
	{ 3840, 2160, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 2160p */
	{ 0, 0, { 0, }, },
};

static struct uvc_frame_info uvc_frames_mjpeg[] = {
	{ 640, 360, { 333333, 666666, 1000000, 0 }, 192 }, /* Note: 360p, 20KB - 1 picture */
	{ 800, 600, { 333333, 666666, 1000000, 0 }, 192 }, /* Note: 600p, 40KB */
	{ 1024, 576, { 333333, 666666, 1000000, 0 }, 384 }, /* Note: 576p, 60KB */
	{ 1280, 720, { 333333, 666666, 1000000, 0 }, 384 }, /* Note: 720p, 80KB */
	{ 1920, 1080, { 333333, 666666, 1000000, 0 }, 800 }, /* Note: 1080p, 200KB */
	{ 2560, 1440, { 333333, 666666, 1000000, 0 }, 1600 }, /* Note: 1440p, 400KB */
	{ 3840, 2160, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 2160p, 600KB */
	{ 0, 0, { 0, }, },
};

static struct uvc_frame_info uvc_frames_h264[] = {
	{ 640, 360, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 360p */
	{ 800, 600, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 600p */
	{ 1024, 576, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 576p */
	{ 1280, 720, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 720p */
	{ 1920, 1080, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1080p */
	{ 2560, 1440, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1440p */
	{ 3840, 2160, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 2160p */
	{ 0, 0, { 0, }, },
};

static struct uvc_frame_info uvc_frames_h265[] = {
	{ 640, 360, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 360p */
	{ 800, 600, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 600p */
	{ 1024, 576, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 576p */
	{ 1280, 720, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 720p */
	{ 1920, 1080, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1080p */
	{ 2560, 1440, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 1440p */
	{ 3840, 2160, { 333333, 666666, 1000000, 0 }, 3072 }, /* Note: 2160p */
	{ 0, 0, { 0, }, },
};

static struct uvc_format_info uvc_formats[] = {
	{V4L2_PIX_FMT_NV12, uvc_frames_nv12},
	{V4L2_PIX_FMT_YUYV, uvc_frames_yuyv},
	{V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg},
	{V4L2_PIX_FMT_H264, uvc_frames_h264},
	// {V4L2_PIX_FMT_H265, uvc_frames_h265},
};
#endif

NATIVE_ATTR_ int uvc_set_maxpkt_quirk(struct uvc_device *dev)
{
	int i, j;
	int maxpkt_size;
	int uvc_size = sizeof(uvc_formats) / sizeof(uvc_formats[0]);

	if (!dev) {
		return -1;
	}

	maxpkt_size = dev->maxpkt_quirk;
	if (maxpkt_size > 3072 || maxpkt_size < 0) {
		return -1;
	}

	printf("uvc_size=%d, maxpkt_size=%d\n", uvc_size, maxpkt_size);
	for (i = 0; i < uvc_size; i++) {
		struct uvc_frame_info *uvc_frames = uvc_formats[i].frames;

		for (j = 0 ; ; j++) {
			if (uvc_frames[j].width == 0) {
				break;
			}

			if (uvc_frames[j].maxpkt_size > maxpkt_size) {
				uvc_frames[j].maxpkt_size = maxpkt_size;
			}
		}
	}

	return 0;
}

/* ---------------------------------------------------------------------------
 * V4L2 streaming related
 */

NATIVE_ATTR_ int v4l2_uninit_device(struct v4l2_device *dev)
{
	unsigned int i;
	int ret;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		for (i = 0; i < dev->nbufs; ++i) {
			ret = munmap(dev->mem[i].start, dev->mem[i].length);
			if (ret < 0) {
				printf("V4L2: munmap failed\n");
				return ret;
			}
		}

		free(dev->mem);
		break;

	case IO_METHOD_USERPTR:
	default:
		break;
	}

	return 0;
}

static int v4l2_reqbufs_mmap(struct v4l2_device *dev, int nbufs)
{
	struct v4l2_requestbuffers req;
	unsigned int i = 0;
	int ret;

	CLEAR(req);

	req.count = nbufs;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(dev->v4l2_fd, VIDIOC_REQBUFS, &req);
	if (ret < 0) {
		if (ret == -EINVAL)
			printf("V4L2: does not support memory mapping\n");
		else
			printf("V4L2: VIDIOC_REQBUFS error %s (%d).\n",
			       strerror(errno), errno);
		goto err;
	}

	if (!req.count)
		return 0;

	if (req.count < 2) {
		printf("V4L2: Insufficient buffer memory.\n");
		ret = -EINVAL;
		goto err;
	}

	/* Map the buffers. */
	dev->mem = calloc(req.count, sizeof dev->mem[0]);
	if (!dev->mem) {
		printf("V4L2: Out of memory\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < req.count; ++i) {
		memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

		dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
		dev->mem[i].buf.index = i;

		ret = ioctl(dev->v4l2_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
		if (ret < 0) {
			printf("V4L2: VIDIOC_QUERYBUF failed for buf %d: "
			       "%s (%d).\n", i, strerror(errno), errno);
			ret = -EINVAL;
			goto err_free;
		}

		dev->mem[i].start =
		    mmap(NULL /* start anywhere */ , dev->mem[i].buf.length,
			 PROT_READ | PROT_WRITE /* required */ ,
			 MAP_SHARED /* recommended */ , dev->v4l2_fd,
			 dev->mem[i].buf.m.offset);

		if (MAP_FAILED == dev->mem[i].start) {
			printf("V4L2: Unable to map buffer %u: %s (%d).\n", i,
			       strerror(errno), errno);
			dev->mem[i].length = 0;
			ret = -EINVAL;
			goto err_free;
		}

		dev->mem[i].length = dev->mem[i].buf.length;
		printf("V4L2: Buffer %u mapped at address %p.\n", i,
		       dev->mem[i].start);
	}

	dev->nbufs = req.count;
	printf("V4L2: %u buffers allocated.\n", req.count);

	return 0;

err_free:
	free(dev->mem);
err:
	return ret;
}

static int v4l2_reqbufs_userptr(struct v4l2_device *dev, int nbufs)
{
	struct v4l2_requestbuffers req;
	int ret;

	CLEAR(req);

	req.count = nbufs;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	ret = ioctl(dev->v4l2_fd, VIDIOC_REQBUFS, &req);
	if (ret < 0) {
		if (ret == -EINVAL)
			printf("V4L2: does not support user pointer i/o\n");
		else
			printf("V4L2: VIDIOC_REQBUFS error %s (%d).\n",
			       strerror(errno), errno);
		return ret;
	}

	dev->nbufs = req.count;
	printf("V4L2: %u buffers allocated.\n", req.count);

	return 0;
}

NATIVE_ATTR_ int v4l2_reqbufs(struct v4l2_device *dev, int nbufs)
{
	int ret = 0;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		ret = v4l2_reqbufs_mmap(dev, nbufs);
		break;

	case IO_METHOD_USERPTR:
		ret = v4l2_reqbufs_userptr(dev, nbufs);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int v4l2_qbuf_mmap(struct v4l2_device *dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < dev->nbufs; ++i) {
		memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

		dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
		dev->mem[i].buf.index = i;

		ret = ioctl(dev->v4l2_fd, VIDIOC_QBUF, &(dev->mem[i].buf));
		if (ret < 0) {
			printf("V4L2: VIDIOC_QBUF failed : %s (%d).\n",
			       strerror(errno), errno);
			return ret;
		}

		dev->qbuf_count++;
	}

	return 0;
}

static int v4l2_qbuf(struct v4l2_device *dev)
{
	int ret = 0;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		ret = v4l2_qbuf_mmap(dev);
		break;

	case IO_METHOD_USERPTR:
		/* Empty. */
		ret = 0;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

NATIVE_ATTR_ int v4l2_process_data(struct v4l2_device *dev)
{
	int ret;
	struct v4l2_buffer vbuf;
	struct v4l2_buffer ubuf;

	/* Return immediately if V4l2 streaming has not yet started. */
	if (!dev->is_streaming)
		return 0;

	if (dev->udev->first_buffer_queued)
		if (dev->dqbuf_count >= dev->qbuf_count)
			return 0;

	/* Dequeue spent buffer rom V4L2 domain. */
	CLEAR(vbuf);

	vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	switch (dev->io) {
	case IO_METHOD_USERPTR:
		vbuf.memory = V4L2_MEMORY_USERPTR;
		break;

	case IO_METHOD_MMAP:
	default:
		vbuf.memory = V4L2_MEMORY_MMAP;
		break;
	}

	ret = ioctl(dev->v4l2_fd, VIDIOC_DQBUF, &vbuf);
	if (ret < 0) {
		return ret;
	}

	dev->dqbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
	printf("Dequeueing buffer at V4L2 side = %d\n", vbuf.index);
#endif

	/* Queue video buffer to UVC domain. */
	CLEAR(ubuf);

	ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	switch (dev->udev->io) {
	case IO_METHOD_MMAP:
		ubuf.memory = V4L2_MEMORY_MMAP;
		ubuf.length = vbuf.length;
		ubuf.index = vbuf.index;
		ubuf.bytesused = vbuf.bytesused;
		break;

	case IO_METHOD_USERPTR:
	default:
		ubuf.memory = V4L2_MEMORY_USERPTR;
		ubuf.m.userptr = (unsigned long)dev->mem[vbuf.index].start;
		ubuf.length = (uint32_t)(dev->mem[vbuf.index].length);
		ubuf.index = vbuf.index;
		ubuf.bytesused = vbuf.bytesused;
		break;
	}

	ret = ioctl(dev->udev->uvc_fd, VIDIOC_QBUF, &ubuf);
	if (ret < 0) {
		printf("UVC: Unable to queue buffer %d: %s (%d).\n",
		       ubuf.index, strerror(errno), errno);
		/* Check for a USB disconnect/shutdown event. */
		if (errno == ENODEV) {
			dev->udev->uvc_shutdown_requested = 1;
			printf("UVC: Possible USB shutdown requested from "
			       "Host, seen during VIDIOC_QBUF\n");
			return 0;
		} else {
			return ret;
		}
	}

	dev->udev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
	printf("Queueing buffer at UVC side = %d\n", ubuf.index);
#endif

	if (!dev->udev->first_buffer_queued && !dev->udev->run_standalone) {
		uvc_video_stream(dev->udev, 1);
		dev->udev->first_buffer_queued = 1;
		dev->udev->is_streaming = 1;
	}

	return 0;
}

/* ---------------------------------------------------------------------------
 * V4L2 generic stuff
 */

static int v4l2_get_format(struct v4l2_device *dev)
{
	struct v4l2_format fmt;
	int ret;

	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = ioctl(dev->v4l2_fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		printf("V4L2: Unable to get format: %s (%d).\n",
		       strerror(errno), errno);
		return ret;
	}

	printf("V4L2: Getting current format: %c%c%c%c %ux%u\n",
	       pixfmtstr(fmt.fmt.pix.pixelformat), fmt.fmt.pix.width,
	       fmt.fmt.pix.height);

	return 0;
}

static int v4l2_set_format(struct v4l2_device *dev, struct v4l2_format *fmt)
{
	int ret;

	ret = ioctl(dev->v4l2_fd, VIDIOC_S_FMT, fmt);
	if (ret < 0) {
		printf("V4L2: Unable to set format %s (%d).\n", strerror(errno),
		       errno);
		return ret;
	}

	printf("V4L2: Setting format to: %c%c%c%c %ux%u\n",
	       pixfmtstr(fmt->fmt.pix.pixelformat), fmt->fmt.pix.width,
	       fmt->fmt.pix.height);

	return 0;
}

static int v4l2_set_ctrl(struct v4l2_device *dev, int new_val, int ctrl)
{
	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;
	int ret;

	CLEAR(queryctrl);

	switch (ctrl) {
	case V4L2_CID_BRIGHTNESS:
		queryctrl.id = V4L2_CID_BRIGHTNESS;
		ret = ioctl(dev->v4l2_fd, VIDIOC_QUERYCTRL, &queryctrl);
		if (-1 == ret) {
			if (errno != EINVAL)
				printf("V4L2: VIDIOC_QUERYCTRL"
				       " failed: %s (%d).\n",
				       strerror(errno), errno);
			else
				printf("V4L2_CID_BRIGHTNESS is not"
				       " supported: %s (%d).\n",
				       strerror(errno), errno);

			return ret;
		} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
			printf("V4L2_CID_BRIGHTNESS is not supported.\n");
			ret = -EINVAL;
			return ret;
		} else {
			CLEAR(control);
			control.id = V4L2_CID_BRIGHTNESS;
			control.value = new_val;

			ret = ioctl(dev->v4l2_fd, VIDIOC_S_CTRL, &control);
			if (-1 == ret) {
				printf("V4L2: VIDIOC_S_CTRL failed: %s (%d).\n",
				       strerror(errno), errno);
				return ret;
			}
		}
		printf("V4L2: Brightness control changed to value = 0x%x\n",
		       new_val);
		break;

	default:
		/* TODO: We don't support any other controls. */
		return -EINVAL;
	}

	return 0;
}

NATIVE_ATTR_ int v4l2_start_capturing(struct v4l2_device *dev)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret;

	ret = ioctl(dev->v4l2_fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		printf("V4L2: Unable to start streaming: %s (%d).\n",
		       strerror(errno), errno);
		return ret;
	}

	printf("V4L2: Starting video stream.\n");

	return 0;
}

NATIVE_ATTR_ int v4l2_stop_capturing(struct v4l2_device *dev)
{
	enum v4l2_buf_type type;
	int ret;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		ret = ioctl(dev->v4l2_fd, VIDIOC_STREAMOFF, &type);
		if (ret < 0) {
			printf("V4L2: VIDIOC_STREAMOFF failed: %s (%d).\n",
			       strerror(errno), errno);
			return ret;
		}

		break;
	default:
		/* Nothing to do. */
		break;
	}

	return 0;
}

NATIVE_ATTR_ int v4l2_open(struct v4l2_device **v4l2, char *devname,
	      struct v4l2_format *s_fmt)
{
	struct v4l2_device *dev;
	struct v4l2_capability cap;
	int fd;
	int ret = -EINVAL;

	fd = open(devname, O_RDWR | O_NONBLOCK, 0);
	if (fd == -1) {
		printf("V4L2: device open failed: %s (%d).\n", strerror(errno),
		       errno);
		return ret;
	}

	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		printf("V4L2: VIDIOC_QUERYCAP failed: %s (%d).\n",
		       strerror(errno), errno);
		goto err;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		printf("V4L2: %s is no video capture device\n", devname);
		goto err;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		printf("V4L2: %s does not support streaming i/o\n", devname);
		goto err;
	}

	dev = calloc(1, sizeof *dev);
	if (dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	printf("V4L2 device is %s on bus %s\n", cap.card, cap.bus_info);

	dev->v4l2_fd = fd;

	/* Get the default image format supported. */
	ret = v4l2_get_format(dev);
	if (ret < 0)
		goto err_free;

	/*
	 * Set the desired image format.
	 * Note: VIDIOC_S_FMT may change width and height.
	 */
	ret = v4l2_set_format(dev, s_fmt);
	if (ret < 0)
		goto err_free;

	/* Get the changed image format. */
	ret = v4l2_get_format(dev);
	if (ret < 0)
		goto err_free;

	printf("v4l2 open succeeded, file descriptor = %d\n", fd);

	*v4l2 = dev;

	return 0;

err_free:
	free(dev);
err:
	close(fd);

	return ret;
}

NATIVE_ATTR_ void v4l2_close(struct v4l2_device *dev)
{
	close(dev->v4l2_fd);
	free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC generic stuff
 */

NATIVE_ATTR_ int uvc_video_set_format(struct uvc_device *dev)
{
	struct v4l2_format fmt;
	int ret;

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = dev->width;
	fmt.fmt.pix.height = dev->height;
	fmt.fmt.pix.pixelformat = dev->fcc;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	switch (dev->fcc) {
	case V4L2_PIX_FMT_YUYV:
		fmt.fmt.pix.sizeimage = (uint32_t)(dev->width * dev->height * 2);
		break;
	case V4L2_PIX_FMT_NV12:
		fmt.fmt.pix.sizeimage = (uint32_t)(dev->width * dev->height * 1.5);
		break;
	case V4L2_PIX_FMT_MJPEG:
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
		fmt.fmt.pix.sizeimage = (uint32_t)(dev->width * dev->height * 1.5);
		break;
	default:
		printf("%s, not support v4l2 pixel format(0x%x)\n",
				__func__, dev->fcc);
	}

	ret = ioctl(dev->uvc_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		printf("UVC: Unable to set format %s (%d).\n", strerror(errno),
		       errno);
		return ret;
	}

	printf("UVC: Setting format to: %c%c%c%c %ux%u\n", pixfmtstr(dev->fcc),
	       dev->width, dev->height);

	return 0;
}

NATIVE_ATTR_ int uvc_video_stream(struct uvc_device *dev, int enable)
{
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	int ret;

	if (!enable) {
		ret = ioctl(dev->uvc_fd, VIDIOC_STREAMOFF, &type);
		if (ret < 0) {
			printf("UVC: VIDIOC_STREAMOFF failed: %s (%d).\n",
			       strerror(errno), errno);
			return ret;
		}

		printf("UVC: Stopping video stream.\n");

		return 0;
	}

	ret = ioctl(dev->uvc_fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		printf("UVC: Unable to start streaming %s (%d).\n",
		       strerror(errno), errno);
		return ret;
	}

	printf("UVC: Starting video stream.\n");

	dev->uvc_shutdown_requested = 0;

	return 0;
}

NATIVE_ATTR_ int uvc_uninit_device(struct uvc_device *dev)
{
	unsigned int i;
	int ret;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		for (i = 0; i < dev->nbufs; ++i) {
			ret = munmap(dev->mem[i].start, dev->mem[i].length);
			if (ret < 0) {
				printf("UVC: munmap failed\n");
				return ret;
			}
		}

		free(dev->mem);
		break;

	case IO_METHOD_USERPTR:
	default:
		if (dev->run_standalone) {
			for (i = 0; i < dev->nbufs; ++i)
				free(dev->dummy_buf[i].start);

			free(dev->dummy_buf);
		}
		break;
	}

	return 0;
}

NATIVE_ATTR_ int uvc_open(struct uvc_device **uvc, char *devname)
{
	struct uvc_device *dev;
	struct v4l2_capability cap;
	int fd;
	int ret = -EINVAL;

	fd = open(devname, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		printf("UVC: device open failed: %s (%d).\n", strerror(errno),
		       errno);
		return ret;
	}

	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		printf("UVC: unable to query uvc device: %s (%d)\n",
		       strerror(errno), errno);
		goto err;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
		printf("UVC: %s is no video output device\n", devname);
		goto err;
	}

	dev = calloc(1, sizeof *dev);
	if (dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	printf("uvc device is %s on bus %s\n", cap.card, cap.bus_info);
	printf("uvc open succeeded, file descriptor = %d\n", fd);

	dev->uvc_fd = fd;
	*uvc = dev;

	return 0;

err:
	close(fd);
	return ret;
}

NATIVE_ATTR_ void uvc_close(struct uvc_device *dev)
{
	close(dev->uvc_fd);
	free(dev->imgdata);
	free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC streaming related
 */
static void uvc_video_release_buffer(struct uvc_device *dev)
{
	uvc_release_buffer_callback_fn do_release;
	void *userdata;

	/* reset videobuf & bufsize */
	dev->videobuf = NULL;
	dev->bufsize = 0;

	/* invoke release buffer callback to release entity */
	do_release = dev->release_cb.cb;
	if (do_release && dev->entity) {
		userdata = dev->prepare_cb.userdata;
		do_release(dev->parent, &dev->entity, userdata);
	}

	return;
}

static int uvc_video_prepare_buffer(struct uvc_device *dev)
{
	uvc_prepare_buffer_callback_fn do_prepare;
	void *userdata;
	int ret = -1;

	do_prepare = dev->prepare_cb.cb;
	if (do_prepare && !dev->entity) {
		userdata = dev->prepare_cb.userdata;
		ret = do_prepare(dev->parent, &dev->videobuf,
				 (int *)(&dev->bufsize), &dev->entity, userdata);
	}

	/* xj3 sequence header(sps, pps) is a independent frame,
	 * but uvc seems needs a sequence header + IDR frame as
	 * the first frame of GOP. Therefore, store the sequence header!
	 */
	if (!ret && dev->h264_quirk && dev->videobuf &&
			dev->fcc == V4L2_PIX_FMT_H264) {
		char *nalu = dev->videobuf;

		/* == priority is higher than &... Don't forget the brackets like (a&b)==c */
		if ((nalu[4] & 0x1f) == H264_NALU_SPS && !dev->got_spspps) {
			if (dev->bufsize > 128) {
				printf("exceed than 128 bytes, might include "
						"IDR frame. Feed directly\n");
			} else {
				printf("sps pps got!! save it\n");

				memcpy(dev->sps_pps, nalu, dev->bufsize);
				dev->sps_pps_size = dev->bufsize;
				dev->got_spspps = 1;

				/* don't feed the independent sequence head to uvc */
				uvc_video_release_buffer(dev);

				/* modify the ret and let uvc_loop try again */
				ret = -EAGAIN;
			}
		}
	}

	return ret;
}

static void uvc_video_fill_buffer(struct uvc_device *dev,
				  struct v4l2_buffer *buf)
{
	size_t length;
	unsigned int bpl, i;
	unsigned int sequence_head_size = 0;

	if (dev->videobuf && dev->bufsize) {
		/* fill prepared buffer */

		/* copy sps, pps before IDR frame (h264 special) */
		if (dev->h264_quirk && dev->got_spspps &&
				dev->fcc == V4L2_PIX_FMT_H264) {
			char *nalu = dev->videobuf;

			/* add sps pps for beginning 3 IDR frames...(as it seems
			 * that the beginning 2 sequence header will be dropped.
			 * Anyway, if only add sps, pps for the beginning 2 IDR
			 * frames, the host(android) uvc-app can't preview it.
			 * black screen...)
			 */
			if ((nalu[4] & 0x1f) == H264_NALU_IDR
					&& dev->idr_idx < 3) {
				memcpy(dev->mem[buf->index].start,
					dev->sps_pps, dev->sps_pps_size);
				sequence_head_size = dev->sps_pps_size;
				dev->idr_idx++;
			}
		}

		if (dev->bufsize + sequence_head_size > dev->mem[buf->index].length) {
			printf("buffer execeed... to be handled...\n");
		}

		length = (dev->bufsize < dev->mem[buf->index].length) ?
		    dev->bufsize : dev->mem[buf->index].length;

		memcpy(dev->mem[buf->index].start + sequence_head_size,
				dev->videobuf, length);
		buf->bytesused = dev->bufsize + sequence_head_size;
	} else {
		/* default fill buffer behavior */
		switch (dev->fcc) {
		case V4L2_PIX_FMT_YUYV:
			/* Fill the buffer with video data. */
			bpl = dev->width * 2;
			for (i = 0; i < dev->height; ++i)
				memset(dev->mem[buf->index].start + i * bpl,
				       dev->color++, bpl);

			buf->bytesused = bpl * dev->height;
			break;

		case V4L2_PIX_FMT_NV12:
			/* Fill the buffer with video data. */
			bpl = (unsigned int)(dev->width * 1.5);
			for (i = 0; i < dev->height; ++i)
				memset(dev->mem[buf->index].start + i * bpl,
				       dev->color++, bpl);

			buf->bytesused = bpl * dev->height;
			break;

		case V4L2_PIX_FMT_MJPEG:
			if (dev->imgdata) {
				memcpy(dev->mem[buf->index].start, dev->imgdata,
				       dev->imgsize);
				buf->bytesused = dev->imgsize;
			}
			break;
		}
	}
}

NATIVE_ATTR_ int uvc_video_process(struct uvc_device *dev)
{
	struct v4l2_buffer ubuf;
	struct v4l2_buffer vbuf;
	unsigned int i;
	int ret;
	/*
	 * Return immediately if UVC video output device has not started
	 * streaming yet.
	 */
	if (!dev->is_streaming)
		return 0;
	/* Prepare a v4l2 buffer to be dequeued from UVC domain. */
	CLEAR(ubuf);

	ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	switch (dev->io) {
	case IO_METHOD_MMAP:
		ubuf.memory = V4L2_MEMORY_MMAP;
		break;

	case IO_METHOD_USERPTR:
	default:
		ubuf.memory = V4L2_MEMORY_USERPTR;
		break;
	}
	if (dev->run_standalone) {
		/* UVC stanalone setup. */
		ret = uvc_video_prepare_buffer(dev);
		if (ret < 0)
			goto release_buffer;

		ret = ioctl(dev->uvc_fd, VIDIOC_DQBUF, &ubuf);
		if (ret < 0)
			goto release_buffer;

		dev->dqbuf_count++;
#ifdef ENABLE_BUFFER_DEBUG
		printf("DeQueued buffer at UVC side = %d\n", ubuf.index);
#endif

#ifdef ENABLE_BUFFER_TRACE
		interval_cost_trace_in();
#endif
		uvc_video_fill_buffer(dev, &ubuf);

		ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &ubuf);
		if (ret < 0) {
			printf("UVC: Unable to queue buffer: %s (%d).\n",
			       strerror(errno), errno);
			goto release_buffer;
		}

		dev->qbuf_count++;

#ifdef ENABLE_BUFFER_TRACE
		interval_cost_trace_out(0);
#endif

#ifdef ENABLE_BUFFER_DEBUG
		printf("ReQueueing buffer at UVC side = %d\n", ubuf.index);
#endif
	} else {
		/* UVC - V4L2 integrated path. */

		/*
		 * Return immediately if V4L2 video capture device has not
		 * started streaming yet or if QBUF was not called even once on
		 * the UVC side.
		 */
		if (!dev->vdev->is_streaming || !dev->first_buffer_queued)
			return 0;

		/*
		 * Do not dequeue buffers from UVC side until there are atleast
		 * 2 buffers available at UVC domain.
		 */
		if (!dev->uvc_shutdown_requested)
			if ((dev->dqbuf_count + 1) >= dev->qbuf_count)
				return 0;

		/* Dequeue the spent buffer from UVC domain */
		ret = ioctl(dev->uvc_fd, VIDIOC_DQBUF, &ubuf);
		if (ret < 0) {
			printf("UVC: Unable to dequeue buffer: %s (%d).\n",
			       strerror(errno), errno);
			return ret;
		}

		if (dev->io == IO_METHOD_USERPTR)
			for (i = 0; i < dev->nbufs; ++i)
				if (ubuf.m.userptr ==
				    (unsigned long)dev->vdev->mem[i].start
				    && ubuf.length == dev->vdev->mem[i].length)
					break;

		dev->dqbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
		printf("DeQueued buffer at UVC side=%d\n", ubuf.index);
#endif

		/*
		 * If the dequeued buffer was marked with state ERROR by the
		 * underlying UVC driver gadget, do not queue the same to V4l2
		 * and wait for a STREAMOFF event on UVC side corresponding to
		 * set_alt(0). So, now all buffers pending at UVC end will be
		 * dequeued one-by-one and we will enter a state where we once
		 * again wait for a set_alt(1) command from the USB host side.
		 */
		if (ubuf.flags & V4L2_BUF_FLAG_ERROR) {
			dev->uvc_shutdown_requested = 1;
			printf("UVC: Possible USB shutdown requested from "
			       "Host, seen during VIDIOC_DQBUF\n");
			return 0;
		}

		/* Queue the buffer to V4L2 domain */
		CLEAR(vbuf);

		vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vbuf.memory = V4L2_MEMORY_MMAP;
		vbuf.index = ubuf.index;

		ret = ioctl(dev->vdev->v4l2_fd, VIDIOC_QBUF, &vbuf);
		if (ret < 0) {
			printf("V4L2: Unable to queue buffer: %s (%d).\n",
			       strerror(errno), errno);
			return ret;
		}

		dev->vdev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
		printf("ReQueueing buffer at V4L2 side = %d\n", vbuf.index);
#endif
	}

	return 0;

release_buffer:
	uvc_video_release_buffer(dev);

	return ret;
}

static int uvc_video_qbuf_mmap(struct uvc_device *dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < dev->nbufs; ++i) {
		memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

		dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
		dev->mem[i].buf.index = i;

		/* UVC standalone setup. */
		if (dev->run_standalone)
			uvc_video_fill_buffer(dev, &(dev->mem[i].buf));

		ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &(dev->mem[i].buf));
		if (ret < 0) {
			printf("UVC: VIDIOC_QBUF failed : %s (%d).\n",
			       strerror(errno), errno);
			return ret;
		}

		dev->qbuf_count++;
	}

	return 0;
}

static int uvc_video_qbuf_userptr(struct uvc_device *dev)
{
	unsigned int i;
	int ret;

	/* UVC standalone setup. */
	if (dev->run_standalone) {
		for (i = 0; i < dev->nbufs; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.m.userptr = (unsigned long)dev->dummy_buf[i].start;
			buf.length = (uint32_t)(dev->dummy_buf[i].length);
			buf.index = i;

			ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &buf);
			if (ret < 0) {
				printf("UVC: VIDIOC_QBUF failed : %s (%d).\n",
				       strerror(errno), errno);
				return ret;
			}

			dev->qbuf_count++;
		}
	}

	return 0;
}

static int uvc_video_qbuf(struct uvc_device *dev)
{
	int ret = 0;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		ret = uvc_video_qbuf_mmap(dev);
		break;

	case IO_METHOD_USERPTR:
		ret = uvc_video_qbuf_userptr(dev);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int uvc_video_reqbufs_mmap(struct uvc_device *dev, int nbufs)
{
	struct v4l2_requestbuffers rb;
	unsigned int i;
	int ret;

	CLEAR(rb);

	rb.count = nbufs;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	rb.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		if (ret == -EINVAL)
			printf("UVC: does not support memory mapping\n");
		else
			printf("UVC: Unable to allocate buffers: %s (%d).\n",
			       strerror(errno), errno);
		goto err;
	}

	if (!rb.count)
		return 0;

	if (rb.count < 2) {
		printf("UVC: Insufficient buffer memory.\n");
		ret = -EINVAL;
		goto err;
	}

	/* Map the buffers. */
	dev->mem = calloc(rb.count, sizeof dev->mem[0]);
	if (!dev->mem) {
		printf("UVC: Out of memory\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < rb.count; ++i) {
		memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

		dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
		dev->mem[i].buf.index = i;

		ret = ioctl(dev->uvc_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
		if (ret < 0) {
			printf("UVC: VIDIOC_QUERYBUF failed for buf %d: "
			       "%s (%d).\n", i, strerror(errno), errno);
			ret = -EINVAL;
			goto err_free;
		}
		dev->mem[i].start =
		    mmap(NULL /* start anywhere */ , dev->mem[i].buf.length,
			 PROT_READ | PROT_WRITE /* required */ ,
			 MAP_SHARED /* recommended */ , dev->uvc_fd,
			 dev->mem[i].buf.m.offset);

		if (MAP_FAILED == dev->mem[i].start) {
			printf("UVC: Unable to map buffer %u: %s (%d).\n", i,
			       strerror(errno), errno);
			dev->mem[i].length = 0;
			ret = -EINVAL;
			goto err_free;
		}

		dev->mem[i].length = dev->mem[i].buf.length;
		printf("UVC: Buffer %u mapped at address %p, length: %d.\n", i,
		       dev->mem[i].start, dev->mem[i].buf.length);
	}

	dev->nbufs = rb.count;
	printf("UVC: %u buffers allocated.\n", rb.count);

	return 0;

err_free:
	free(dev->mem);
err:
	return ret;
}

static int uvc_video_reqbufs_userptr(struct uvc_device *dev, int nbufs)
{
	struct v4l2_requestbuffers rb;
	unsigned int i, j, bpl, payload_size;
	int ret;

	CLEAR(rb);

	rb.count = nbufs;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	rb.memory = V4L2_MEMORY_USERPTR;

	ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0) {
		if (ret == -EINVAL)
			printf("UVC: does not support user pointer i/o\n");
		else
			printf("UVC: VIDIOC_REQBUFS error %s (%d).\n",
			       strerror(errno), errno);
		goto err1;
	}

	if (!rb.count)
		return 0;

	dev->nbufs = rb.count;
	printf("UVC: %u buffers allocated.\n", rb.count);

	if (dev->run_standalone) {
		/* Allocate buffers to hold dummy data pattern. */
		dev->dummy_buf = calloc(rb.count, sizeof dev->dummy_buf[0]);
		if (!dev->dummy_buf) {
			printf("UVC: Out of memory\n");
			ret = -ENOMEM;
			goto err1;
		}

		switch (dev->fcc) {
		case V4L2_PIX_FMT_YUYV:
			bpl = dev->width * 2;
			payload_size = (unsigned int)(dev->width * dev->height * 2);
			break;
		case V4L2_PIX_FMT_NV12:
			bpl = (unsigned int)(dev->width * 1.5);
			payload_size = (unsigned int)(dev->width * dev->height * 1.5);
			break;
		case V4L2_PIX_FMT_MJPEG:
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_H265:
			payload_size = (unsigned int)(dev->width * dev->height * 1.5);
			break;
		default:
			printf("%s: unknown fcc(0x%x)\n", __func__, dev->fcc);
			ret = -EINVAL;
			goto err2;
		}

		for (i = 0; i < rb.count; ++i) {
			dev->dummy_buf[i].length = payload_size;
			dev->dummy_buf[i].start = malloc(payload_size);
			if (!dev->dummy_buf[i].start) {
				printf("UVC: Out of memory\n");
				ret = -ENOMEM;
				goto err2;
			}

			if (V4L2_PIX_FMT_YUYV == dev->fcc ||
					V4L2_PIX_FMT_NV12 == dev->fcc) {
				for (j = 0; j < dev->height; ++j)
					memset(dev->dummy_buf[i].start +
					       j * bpl, dev->color++, bpl);
			}

			if (V4L2_PIX_FMT_MJPEG == dev->fcc) {
				if (dev->imgdata)
					memcpy(dev->dummy_buf[i].start,
					       dev->imgdata, dev->imgsize);
			}
		}

		dev->mem = dev->dummy_buf;
	}

	return 0;

err2:
	if (dev->dummy_buf)
		free(dev->dummy_buf);

err1:
	return ret;
}

NATIVE_ATTR_ int uvc_video_reqbufs(struct uvc_device *dev, int nbufs)
{
	int ret = 0;

	switch (dev->io) {
	case IO_METHOD_MMAP:
		ret = uvc_video_reqbufs_mmap(dev, nbufs);
		break;

	case IO_METHOD_USERPTR:
		ret = uvc_video_reqbufs_userptr(dev, nbufs);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * stream off event handler
 */
static int uvc_handle_streamoff_event(struct uvc_device *dev)
{
	uvc_streamon_callback_fn streamon_notify;
	void *userdata;

	/* Stop V4L2 streaming... */
	if (!dev->run_standalone && dev->vdev->is_streaming) {
		/* UVC - V4L2 integrated path. */
		v4l2_stop_capturing(dev->vdev);
		dev->vdev->is_streaming = 0;
	}

	/* ... and now UVC streaming.. */
	if (dev->is_streaming) {
		uvc_video_stream(dev, 0);
		uvc_uninit_device(dev);
		uvc_video_reqbufs(dev, 0);
		dev->is_streaming = 0;
		dev->first_buffer_queued = 0;

		/* h264 sequence header (special case)*/
		dev->sps_pps_size = 0;
		dev->got_spspps = 0;
		dev->idr_idx = 0;
		memset(dev->sps_pps, 0, 128);

		/* reset videobuf & bufsize */
		dev->videobuf = NULL;
		dev->bufsize = 0;
		dev->entity = NULL;
	}

	/* notify app streamoff */
	streamon_notify = dev->streamon_cb.cb;
	userdata = dev->streamon_cb.userdata;
	if (streamon_notify)
		streamon_notify(dev->parent, 0, userdata);

	return 0;
}

/*
 * This function is called in response to either:
 * 	- A SET_ALT(interface 1, alt setting 1) command from USB host,
 * 	  if the UVC gadget supports an ISOCHRONOUS video streaming endpoint
 * 	  or,
 *
 *	- A UVC_VS_COMMIT_CONTROL command from USB host, if the UVC gadget
 *	  supports a BULK type video streaming endpoint.
 */
static int uvc_handle_streamon_event(struct uvc_device *dev)
{
	uvc_streamon_callback_fn streamon_notify;
	void *userdata;
	int prepared = 0;
	int i, ret;

	/* sometimes host app's streamon/off event is not a pair (eg. amcap) */
	if (dev->is_streaming)
		uvc_handle_streamoff_event(dev);

	/* notify app stream on */
	streamon_notify = dev->streamon_cb.cb;
	userdata = dev->streamon_cb.userdata;
	printf("streamon_notify = %p\n", streamon_notify);
	if (streamon_notify) {
		printf("notify streamon!!\n");
		streamon_notify(dev->parent, 1, userdata);
	}

	/* prepare video frames in frontend */
	for (i = 0; i < 20; i++) {
		ret = uvc_video_prepare_buffer(dev);
		if (!ret) {
			printf("uvc video prepare buffer succeed\n");
			prepared = 1;
			break;
		}
		usleep(5 * 1000);  // wait 5 ms, totally 100ms
	}

	if (!prepared)
		printf("failed to prepare buffer before streamon...\n");

	ret = uvc_video_reqbufs(dev, dev->nbufs);
	if (ret < 0)
		goto err;

	if (!dev->run_standalone) {
		/* UVC - V4L2 integrated path. */
		if (IO_METHOD_USERPTR == dev->vdev->io) {
			/*
			 * Ensure that the V4L2 video capture device has already
			 * some buffers queued.
			 */
			ret = v4l2_reqbufs(dev->vdev, dev->vdev->nbufs);
			if (ret < 0)
				goto err;
		}

		ret = v4l2_qbuf(dev->vdev);
		if (ret < 0)
			goto err;

		/* Start V4L2 capturing now. */
		ret = v4l2_start_capturing(dev->vdev);
		if (ret < 0)
			goto err;

		dev->vdev->is_streaming = 1;
	}

	/* Common setup. */

	/* Queue buffers to UVC domain and start streaming. */
	ret = uvc_video_qbuf(dev);
	if (ret < 0)
		goto err;

	if (prepared) {
		uvc_video_release_buffer(dev);
		prepared = 0;
	}

	if (dev->run_standalone) {
		uvc_video_stream(dev, 1);
		dev->first_buffer_queued = 1;
		dev->is_streaming = 1;
	}

	printf("STREAM ON! show video info:\n");
	printf("==============================================\n");
	printf("fcc: %s(0x%x)\n", fcc_to_string(dev->fcc), dev->fcc);
	printf("run_standalone: %d\n", dev->run_standalone);
	printf("uvc_devname: %s\n", dev->uvc_devname);
	printf("mem: %p\n", dev->mem);
	printf("dummy_buf: %p\n", dev->dummy_buf);
	printf("nbufs: %u\n", dev->nbufs);
	printf("width: %u\n", dev->width);
	printf("height: %u\n", dev->height);
	printf("bulk: %d\n", dev->bulk);
	printf("imgsize: %u\n", dev->imgsize);
	printf("imgdata: %p\n", dev->imgdata);
	printf("maxpkt: %d\n", dev->maxpkt);
	printf("speed: %u\n", dev->speed);
	printf("mult_alts: %u\n", dev->mult_alts);
	printf("h264_quirk: %d\n", dev->h264_quirk);
	printf("maxpkt_quirk: %d\n", dev->maxpkt_quirk);
	printf("==============================================\n");

	return 0;

err:
	if (prepared)
		uvc_video_release_buffer(dev);

	return ret;
}

/* ---------------------------------------------------------------------------
 * UVC Request processing
 */

static void
uvc_fill_streaming_control(struct uvc_device *dev,
			   struct uvc_streaming_control *ctrl, int iframe,
			   int iformat)
{
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	unsigned int nframes;

	if (iformat < 0)
		iformat = ARRAY_SIZE(uvc_formats) + iformat;
	if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats))
		return;
	format = &uvc_formats[iformat];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	if (iframe < 0)
		iframe = nframes + iframe;
	if (iframe < 0 || iframe >= (int)nframes)
		return;
	frame = &format->frames[iframe];

	memset(ctrl, 0, sizeof *ctrl);

	ctrl->bmHint = 1;
	ctrl->bFormatIndex = (uint8_t)(iformat + 1);
	ctrl->bFrameIndex = (uint8_t)(iframe + 1);
	ctrl->dwFrameInterval = frame->intervals[0];
	switch (format->fcc) {
	case V4L2_PIX_FMT_YUYV:
		ctrl->dwMaxVideoFrameSize = (uint32_t)(frame->width * frame->height * 2);
		break;
	case V4L2_PIX_FMT_NV12:
		ctrl->dwMaxVideoFrameSize = (uint32_t)(frame->width * frame->height * 1.5);
		break;
	case V4L2_PIX_FMT_MJPEG:
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
		ctrl->dwMaxVideoFrameSize =
		    (uint32_t)(frame->width * frame->height * 1.5);
		break;
	}

	/* TODO: the UVC maxpayload transfer size should be filled
	 * by the driver.
	 */
	if (!dev->bulk) {
		if (dev->mult_alts)
			ctrl->dwMaxPayloadTransferSize = frame->maxpkt_size;
		else if (dev->conn_speed >= USB_SPEED_SUPER)	/* Burst only for SuperSpeed mode */
			ctrl->dwMaxPayloadTransferSize =
				(dev->maxpkt) * (dev->mult + 1) * (dev->burst + 1);
		else
			ctrl->dwMaxPayloadTransferSize =
				(dev->maxpkt) * (dev->mult + 1);
	} else {
		if (ctrl->dwMaxVideoFrameSize < UVC_MAX_TRB_SIZE)
			ctrl->dwMaxPayloadTransferSize = ctrl->dwMaxVideoFrameSize;
		else
			ctrl->dwMaxPayloadTransferSize = UVC_MAX_TRB_SIZE;
	}

	ctrl->bmFramingInfo = 3;
	ctrl->bPreferedVersion = 1;
	ctrl->bMaxVersion = 1;
}

static inline void
uvc_set_control_context(struct uvc_device *dev, uint8_t req, uint8_t cs, uint8_t id, uint8_t intf)
{
	dev->context.req = req;
	dev->context.cs = cs;
	dev->context.id = id;
	dev->context.intf = intf;
}

static inline void
uvc_get_control_context(struct uvc_device *dev, uint8_t *req, uint8_t *cs, uint8_t *id, uint8_t *intf)
{
	*req = dev->context.req;
	*cs = dev->context.cs;
	*id = dev->context.id;
	*intf = dev->context.intf;
}

static void
uvc_events_process_standard(struct uvc_device *dev,
			    struct usb_ctrlrequest *ctrl,
			    struct uvc_request_data *resp)
{
	printf("standard request\n");
	(void)dev;
	(void)ctrl;
	(void)resp;
}

static void uvc_events_process_control(struct uvc_device *dev, uint8_t req,
				       uint8_t cs, uint8_t entity_id,
				       uint8_t len,
				       struct uvc_request_data *resp)
{
	uvc_event_setup_callback_fn setup_ops;
	void *userdata;
	int ret;

	/* set cur control context */
	if (req == UVC_SET_CUR) {
		uvc_set_control_context(dev, req, cs, entity_id,
			(uint8_t)(dev->control_interface));;
	}

	/* notify event to app */
	setup_ops = dev->event_cb.setup_f;
	userdata = dev->event_cb.userdata;

	switch (entity_id) {
	case UVC_CTRL_INTERFACE_ID:
		switch (cs) {
		case UVC_VC_REQUEST_ERROR_CODE_CONTROL:
			/* Send the request error code last prepared. */
			resp->data[0] = dev->request_error_code.data[0];
			resp->length = dev->request_error_code.length;

			printf("send real error code last prepared(%02x)\n",
					resp->data[0]);
			break;

		default:
			/*
			 * If we were not supposed to handle this
			 * 'cs', prepare an error code response.
			 */
			dev->request_error_code.data[0] = 0x06;
			dev->request_error_code.length = 1;
			break;
		}
		break;
	case UVC_CTRL_CAMERA_TERMINAL_ID:
	case UVC_CTRL_PROCESSING_UNIT_ID:
	case UVC_CTRL_EXTENSION_UNIT_ID:
	case UVC_CTRL_OUTPUT_TERMINAL_ID:
	default:
		if (((0x1 << entity_id) & dev->event_mask) && setup_ops) {
			ret = setup_ops(dev->parent, req, cs, entity_id, resp, userdata);
			if (ret) {
				/*
				 * We don't support this control, so STALL the
				 * control ep.
				 */
				resp->length = -EL2HLT;
				/*
				 * For every unsupported control request
				 * set the request error code to appropriate
				 * value.
				 */
				dev->request_error_code.data[0] = 0x6;
				dev->request_error_code.length = 1;
				break;
			} else {
				/*
				 * For every successfully handled control
				 * request set the request error code to no
				 * error.
				 */
				dev->request_error_code.data[0] = 0x00;
				dev->request_error_code.length = 1;
			}
		} else {
			printf("dev->mask (0x%x), entity_id(%x), setup_ops(%p), app doesn't care\n",
					dev->event_mask, entity_id, setup_ops);
			/*
			 * We don't support this control, so STALL the
			 * control ep.
			 */
			resp->length = -EL2HLT;
			/*
			 * For every unsupported control request
			 * set the request error code to appropriate
			 * value.
			 */
			dev->request_error_code.data[0] = 0x6;
			dev->request_error_code.length = 1;
			break;
		}
		break;
	}

	printf("control request (entity_id %02x req %02x cs %02x)\n",
			entity_id, req, cs);
}

static void uvc_events_process_streaming(struct uvc_device *dev, uint8_t req,
					 uint8_t cs,
					 struct uvc_request_data *resp)
{
	struct uvc_streaming_control *ctrl;

	if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
		return;

	ctrl = (struct uvc_streaming_control *)&resp->data;
	resp->length = sizeof *ctrl;

	switch (req) {
	case UVC_SET_CUR:
		dev->control = cs;
		uvc_set_control_context(dev, req, cs, UVC_CTRL_INTERFACE_ID,
			(uint8_t)(dev->streaming_interface));;
		resp->length = 34;
		break;

	case UVC_GET_CUR:
		if (cs == UVC_VS_PROBE_CONTROL)
			memcpy(ctrl, &dev->probe, sizeof *ctrl);
		else
			memcpy(ctrl, &dev->commit, sizeof *ctrl);
		break;

	case UVC_GET_MIN:
		uvc_fill_streaming_control(dev, ctrl, 0, 0);
		break;

	case UVC_GET_MAX:
		uvc_fill_streaming_control(dev, ctrl, -1, -1);
		break;

	case UVC_GET_DEF:
		/* default set as mjpg format */
		uvc_fill_streaming_control(dev, ctrl, 0, 0);
		break;

	case UVC_GET_RES:
		CLEAR(ctrl);
		break;

	case UVC_GET_LEN:
		resp->data[0] = 0x00;
		resp->data[1] = 0x22;
		resp->length = 2;
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0x03;
		resp->length = 1;
		break;
	}

	printf("streaming request (req %02x cs %02x)\n", req, cs);
}

static void
uvc_events_process_class(struct uvc_device *dev, struct usb_ctrlrequest *ctrl,
			 struct uvc_request_data *resp)
{
	unsigned int interface = ctrl->wIndex & 0xff;

	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
		return;

	if (interface == dev->control_interface)
		uvc_events_process_control(dev, ctrl->bRequest,
					   (uint8_t)(ctrl->wValue >> 8),
					   (uint8_t)(ctrl->wIndex >> 8),
					   (uint8_t)(ctrl->wLength), resp);
	else if (interface == dev->streaming_interface)
		uvc_events_process_streaming(dev, (uint8_t)(ctrl->bRequest),
					     (uint8_t)(ctrl->wValue >> 8), resp);
}

static void
uvc_events_process_setup(struct uvc_device *dev, struct usb_ctrlrequest *ctrl,
			 struct uvc_request_data *resp)
{
	dev->control = 0;

#ifdef ENABLE_USB_REQUEST_DEBUG
	printf("\nbRequestType %02x bRequest %02x wValue %04x wIndex %04x "
	       "wLength %04x\n",
	       ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex,
	       ctrl->wLength);
#endif
	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		uvc_events_process_standard(dev, ctrl, resp);
		break;

	case USB_TYPE_CLASS:
		uvc_events_process_class(dev, ctrl, resp);
		break;

	default:
		break;
	}
}

static int
uvc_events_process_control_data(struct uvc_device *dev, uint8_t cs,
				uint8_t entity_id,
				struct uvc_request_data *data)
{
	switch (entity_id) {
		/* Processing unit 'UVC_VC_PROCESSING_UNIT'. */
	case 2:
		switch (cs) {
			/*
			 * We support only 'UVC_PU_BRIGHTNESS_CONTROL' for Processing
			 * Unit, as our bmControls[0] = 1 for PU.
			 */
		case UVC_PU_BRIGHTNESS_CONTROL:
			memcpy(&dev->brightness_val, data->data, data->length);
			/* UVC - V4L2 integrated path. */
			if (!dev->run_standalone)
				/*
				 * Try to change the Brightness attribute on
				 * Video capture device. Note that this try may
				 * succeed or end up with some error on the
				 * video capture side. By default to keep tools
				 * like USBCV's UVC test suite happy, we are
				 * maintaining a local copy of the current
				 * brightness value in 'dev->brightness_val'
				 * variable and we return the same value to the
				 * Host on receiving a GET_CUR(BRIGHTNESS)
				 * control request.
				 *
				 * FIXME: Keeping in view the point discussed
				 * above, notice that we ignore the return value
				 * from the function call below. To be strictly
				 * compliant, we should return the same value
				 * accordingly.
				 */
				v4l2_set_ctrl(dev->vdev, dev->brightness_val,
					      V4L2_CID_BRIGHTNESS);
			break;

		default:
			break;
		}

		break;

	default:
		break;
	}

	printf("Control Request data phase (cs %02x entity %02x)\n", cs,
	       entity_id);

	return 0;
}

static int uvc_events_process_vc_data(struct uvc_device *dev,
				   struct uvc_request_data *data)
{
	uvc_event_data_callback_fn data_ops;
	void *userdata;
	uint8_t req;
	uint8_t cs;
	uint8_t entity_id;
	uint8_t intf;
	int ret;

	uvc_get_control_context(dev, &req, &cs, &entity_id, &intf);
	(void)intf;

	/* notify event to app */
	data_ops = dev->event_cb.data_f;
	userdata = dev->event_cb.userdata;

	switch (entity_id) {
	case UVC_CTRL_INTERFACE_ID:
		printf("%s interface entity id\n", __func__);
		break;
	case UVC_CTRL_CAMERA_TERMINAL_ID:
	case UVC_CTRL_PROCESSING_UNIT_ID:
	case UVC_CTRL_EXTENSION_UNIT_ID:
	case UVC_CTRL_OUTPUT_TERMINAL_ID:
	default:
		if (((0x1 << entity_id) & dev->event_mask) && data_ops) {
			ret = data_ops(dev->parent, req, cs, entity_id, data, userdata);

			if (ret)
				printf("%s vc data notify handle failed\n",
						__func__);
		} else {
			fprintf(stderr, "%s, dev->mask (0x%x), entity_id(%x), data_ops(%p) app doesn't care\n",
					__func__, dev->event_mask, entity_id, data_ops);
		}
		break;
	}

	return 0;
}

static int uvc_events_process_vs_data(struct uvc_device *dev,
				   struct uvc_request_data *data)
{
	struct uvc_streaming_control *target;
	struct uvc_streaming_control *ctrl;
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	const unsigned int *interval;
	unsigned int iformat, iframe;
	unsigned int nframes;
	unsigned int *val = (unsigned int *)data->data;
	int ret;

	switch (dev->control) {
	case UVC_VS_PROBE_CONTROL:
		printf("setting probe control, length = %d\n", data->length);
		target = &dev->probe;
		break;

	case UVC_VS_COMMIT_CONTROL:
		printf("setting commit control, length = %d\n", data->length);
		target = &dev->commit;
		break;

	default:
		printf("setting unknown control(%d), length = %d\n",
				dev->control, data->length);

		/*
		 * As we support only BRIGHTNESS control, this request is
		 * for setting BRIGHTNESS control.
		 * Check for any invalid SET_CUR(BRIGHTNESS) requests
		 * from Host. Note that we support Brightness levels
		 * from 0x0 to 0x10 in a step of 0x1. So, any request
		 * with value greater than 0x10 is invalid.
		 */
		if (*val > PU_BRIGHTNESS_MAX_VAL) {
			return -EINVAL;
		} else {
			ret =
			    uvc_events_process_control_data(dev,
							    UVC_PU_BRIGHTNESS_CONTROL,
							    2, data);
			if (ret < 0)
				goto err;

			return 0;
		}
	}

	ctrl = (struct uvc_streaming_control *)&data->data;
	iformat =
	    clamp((unsigned int)ctrl->bFormatIndex, 1U,
		  (unsigned int)ARRAY_SIZE(uvc_formats));
	format = &uvc_formats[iformat - 1];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	iframe = clamp((unsigned int)ctrl->bFrameIndex, 1U, nframes);
	frame = &format->frames[iframe - 1];
	interval = frame->intervals;

	while (interval[0] < ctrl->dwFrameInterval && interval[1])
		++interval;

	target->bFormatIndex = (uint8_t)iformat;
	target->bFrameIndex = (uint8_t)iframe;
	switch (format->fcc) {
	case V4L2_PIX_FMT_YUYV:
		target->dwMaxVideoFrameSize = (uint32_t)(frame->width * frame->height * 2);
		break;
	case V4L2_PIX_FMT_NV12:
		target->dwMaxVideoFrameSize = (uint32_t)(frame->width * frame->height * 1.5);
		break;
	case V4L2_PIX_FMT_MJPEG:
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
		target->dwMaxVideoFrameSize =
		    (uint32_t)(frame->width * frame->height * 1.5);
		break;
	}
	target->dwFrameInterval = *interval;

	if (!dev->bulk) {
		if (dev->mult_alts)
			target->dwMaxPayloadTransferSize = frame->maxpkt_size;
		else if (dev->conn_speed >= USB_SPEED_SUPER)	/* Burst only for SuperSpeed mode */
			target->dwMaxPayloadTransferSize =
				(dev->maxpkt) * (dev->mult + 1) * (dev->burst + 1);
		else
			target->dwMaxPayloadTransferSize =
				(dev->maxpkt) * (dev->mult + 1);
	} else {
		if (target->dwMaxVideoFrameSize < UVC_MAX_TRB_SIZE)
			target->dwMaxPayloadTransferSize = target->dwMaxVideoFrameSize;
		else
			target->dwMaxPayloadTransferSize = UVC_MAX_TRB_SIZE;
	}

	if (dev->control == UVC_VS_COMMIT_CONTROL) {
		dev->fcc = format->fcc;
		dev->width = frame->width;
		dev->height = frame->height;

		printf("##UVC VS COMMIT CONTROL## "
				"target->dwMaxPayloadTransferSize(%u)\n",
				target->dwMaxPayloadTransferSize);

		ret = uvc_video_set_format(dev);
		if (ret < 0)
			goto err;

		if (dev->bulk) {
			ret = uvc_handle_streamon_event(dev);
			if (ret < 0)
				goto err;
		}
	}

	return 0;

err:
	return ret;
}

static int uvc_events_process_data(struct uvc_device *dev,
				   struct uvc_request_data *data)
{
	unsigned int interface = dev->context.intf;
	int ret = -1;

	if (interface == dev->control_interface)
		ret = uvc_events_process_vc_data(dev, data);
	else if (interface == dev->streaming_interface)
		ret = uvc_events_process_vs_data(dev, data);
	else
		printf("#%s interface %d is not exist\n",
				__func__, dev->context.intf);

	return ret;
}

NATIVE_ATTR_ void uvc_events_process(struct uvc_device *dev)
{
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	struct uvc_request_data resp;
	int ret;

	ret = ioctl(dev->uvc_fd, VIDIOC_DQEVENT, &v4l2_event);
	if (ret < 0) {
		printf("VIDIOC_DQEVENT failed: %s (%d)\n", strerror(errno),
		       errno);
		return;
	}

	memset(&resp, 0, sizeof resp);
	resp.length = -EL2HLT;

	switch (v4l2_event.type) {
	case UVC_EVENT_CONNECT:
		dev->conn_speed = uvc_event->speed;
		printf("UVC: connect speed: %u\n", dev->conn_speed);
		return;

	case UVC_EVENT_DISCONNECT:
		dev->uvc_shutdown_requested = 1;
		printf("UVC: Possible USB shutdown requested from "
		       "Host, seen via UVC_EVENT_DISCONNECT\n");
		return;

	case UVC_EVENT_SETUP:
		uvc_events_process_setup(dev, &uvc_event->req, &resp);
		break;

	case UVC_EVENT_DATA:
		ret = uvc_events_process_data(dev, &uvc_event->data);
		if (ret < 0)
			break;
		return;

	case UVC_EVENT_STREAMON:
		if (!dev->bulk)
			uvc_handle_streamon_event(dev);
		return;

	case UVC_EVENT_STREAMOFF:
		uvc_handle_streamoff_event(dev);
		return;
	}

	ret = ioctl(dev->uvc_fd, UVCIOC_SEND_RESPONSE, &resp);
	if (ret < 0) {
		printf("UVCIOC_S_EVENT failed: %s (%d)\n", strerror(errno),
		       errno);
		return;
	}
}

NATIVE_ATTR_ void uvc_events_init(struct uvc_device *dev)
{
	struct v4l2_event_subscription sub;
	unsigned int payload_size;
	(void)payload_size;

	switch (dev->fcc) {
	case V4L2_PIX_FMT_YUYV:
		payload_size = (unsigned int)(dev->width * dev->height * 2);
		break;
	case V4L2_PIX_FMT_NV12:
		payload_size = (unsigned int)(dev->width * dev->height * 1.5);
		break;
	case V4L2_PIX_FMT_MJPEG:
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
		payload_size = (unsigned int)(dev->width * dev->height * 1.5);
		break;
	}

	uvc_fill_streaming_control(dev, &dev->probe, 0, 0);
	uvc_fill_streaming_control(dev, &dev->commit, 0, 0);

#if 0	/* FIXME: below is for default dwMaxPayloadTransferSize, must be accurate */
	if (dev->bulk) {
		/* FIXME Crude hack, must be negotiated with the driver. */
		dev->probe.dwMaxPayloadTransferSize =
		    dev->commit.dwMaxPayloadTransferSize = payload_size;
	}
#endif

	memset(&sub, 0, sizeof sub);
	sub.type = UVC_EVENT_CONNECT;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_SETUP;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_DATA;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_STREAMON;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_STREAMOFF;
	ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

const char *fcc_to_string(unsigned int fcc)
{
	switch (fcc) {
	case V4L2_PIX_FMT_YUYV:
		return "yuyv";
	case V4L2_PIX_FMT_NV12:
		return "nv12";
	case V4L2_PIX_FMT_MJPEG:
		return "mjpeg";
	case V4L2_PIX_FMT_H264:
		return "h264";
	case V4L2_PIX_FMT_H265:
		return "h265";
	default:
		return "unknown";
	}
}
