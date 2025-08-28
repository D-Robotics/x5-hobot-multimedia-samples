/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <math.h>
#include <sys/sysmacros.h>
#include "v4l2_common_utils.h"

#define MEDIA_DEVNODE "/dev/media0"
uvc_camera_context TestContext[V4L2_MAX_PIPE_NUM];
int g_pipe_num, t_pipe_num;
pthread_mutex_t chance_test = PTHREAD_MUTEX_INITIALIZER;

cam_format_e str_to_format(const char *str)
{
	if (!strncmp(str, "RAW8", 4))
		return CAM_FMT_RAW8;
	if (!strncmp(str, "RAW10", 5))
		return CAM_FMT_RAW10;
	if (!strncmp(str, "RAW12", 5))
		return CAM_FMT_RAW12;
	if (!strncmp(str, "YUYV", 4))
		return CAM_FMT_YUYV;
	if (!strncmp(str, "NV12", 4))
		return CAM_FMT_NV12;
	if (!strncmp(str, "NV16", 4))
		return CAM_FMT_NV16;
	if (!strncmp(str, "RGB888X", 7))
		return CAM_FMT_RGB888X;
	return CAM_FMT_NULL;
}

void print_control_range(int fd, unsigned int id, const char* name) {
	struct v4l2_queryctrl queryctrl;

	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = id;

	if (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0) {
		printf("%s range: min=%d max=%d step=%d default=%d\n",
			   name,
			   queryctrl.minimum,
			   queryctrl.maximum,
			   queryctrl.step,
			   queryctrl.default_value);
	} else {
		printf("Failed to query %s control\n", name);
	}
}

int v4l2_dr_isp_expsoure_test(uvc_camera_context * const ptc)
{
	struct v4l2_control ctrl;
	int rc;

	// Print exposure
	print_control_range(ptc->fd, V4L2_CID_EXPOSURE_ABSOLUTE, "exposure_absolute");

	// Set manual exposure mode
	ctrl.id = V4L2_CID_EXPOSURE_AUTO;
	ctrl.value = V4L2_EXPOSURE_MANUAL;
	rc = ioctl(ptc->fd, VIDIOC_S_CTRL, &ctrl);
	if (rc < 0) {
		fprintf(stderr, "Failed to set manual exposure mode: %s\n", strerror(errno));
		return -1;
	}

	// Get current exposure value
	ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
	rc = ioctl(ptc->fd, VIDIOC_G_CTRL, &ctrl);
	if (rc < 0) {
		fprintf(stderr, "Failed to get exposure value: %s\n", strerror(errno));
		return -1;
	}
	printf("Original exposure: %d\n", ctrl.value);

	// Set test exposure value
	rc = ioctl(ptc->fd, VIDIOC_S_CTRL, &ctrl);
	if (rc < 0) {
		fprintf(stderr, "Failed to set exposure value: %s\n", strerror(errno));
		return -1;
	}
	printf("Set exposure to: %d\n", ctrl.value);

	return 0;
}

int v4l2_dr_isp_awb_test(uvc_camera_context * const ptc)
{
	struct v4l2_control ctrl;
	int rc;

	// Print white balance control
	print_control_range(ptc->fd, V4L2_CID_WHITE_BALANCE_TEMPERATURE, "white_balance_temperature");

	// Disable auto white balance (0 = disabled)
	ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
	ctrl.value = 0;
	rc = ioctl(ptc->fd, VIDIOC_S_CTRL, &ctrl);
	if (rc < 0) {
		fprintf(stderr, "Failed to disable auto white balance: %s\n", strerror(errno));
		return -1;
	}

	// Get current white balance temperature
	ctrl.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
	rc = ioctl(ptc->fd, VIDIOC_G_CTRL, &ctrl);
	if (rc < 0) {
		fprintf(stderr, "Failed to get white balance temperature: %s\n", strerror(errno));
		return -1;
	}
	printf("Original white balance temperature: %d\n", ctrl.value);

	// Set test white balance temperature
	rc = ioctl(ptc->fd, VIDIOC_S_CTRL, &ctrl);
	if (rc < 0) {
		fprintf(stderr, "Failed to set white balance temperature: %s\n", strerror(errno));
		return -1;
	}
	printf("Set white balance temperature to: %d\n", ctrl.value);

	return 0;
}

int v4l2_uvc_camera_device_open(uvc_camera_context *ptc)
{
	int fd, rc;
	struct v4l2_capability caps;
	char dev_name[64];

	ptc->fd = -1;
	snprintf(dev_name, sizeof(dev_name), "/dev/video%d", ptc->video_id);
	fd = open(dev_name, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		printf("cannot open video device %s\n", dev_name);
		return -1;
	}

	rc = ioctl(fd, VIDIOC_QUERYCAP, &caps);
	if (rc < 0) {
		printf("failed to get device caps for %s (%d=%s)\n", dev_name, errno, strerror(errno));
		close(fd);
		return -1;
	}

	printf("open device: %s (fd=%d)\n", dev_name, fd);
	printf("     driver: %s\n", caps.driver);

	ptc->fd = fd;
	return 0;
}

uint32_t uvc_camera_get_pixelformat(uint32_t pixelformat, uint32_t format)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		if (format == CAM_FMT_RAW8)
			return pixelformat;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		if (format == CAM_FMT_RAW10)
			return pixelformat;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		if (format == CAM_FMT_RAW12)
			return pixelformat;
		break;
	case V4L2_PIX_FMT_YUYV:
		if (format == CAM_FMT_YUYV)
			return pixelformat;
		break;
	case V4L2_PIX_FMT_NV12:
		if (format == CAM_FMT_NV12)
			return pixelformat;
		break;
	case V4L2_PIX_FMT_NV16:
		if (format == CAM_FMT_NV16)
			return pixelformat;
		break;
	case V4L2_PIX_FMT_RGB32:
		if (format == CAM_FMT_RGB888X)
			return pixelformat;
		break;
	default:
		break;
	}
	return 0;
}

int v4l2_uvc_camera_device_init(uvc_camera_context *ptc)
{
	struct v4l2_capability caps;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_frmsizeenum frmsizeenum;
	struct v4l2_format format;
	uint32_t pixelformat = 0;
	int i, j, rc;

	if (ptc->fd < 0)
		return -1;

	memset(&caps, 0, sizeof(caps));
	rc = ioctl(ptc->fd, VIDIOC_QUERYCAP, &caps);
	if (rc < 0) {
		printf("failed to get device caps(%d=%s)\n", errno, strerror(errno));
		return rc;
	}

	printf("       card: %s\n", caps.card);
	printf("    version: %u.%u.%u\n", (caps.version >> 16) & 0xFF, (caps.version >> 8) & 0xFF,
		 (caps.version) & 0xFF);
	printf("   all caps: %08x\n", caps.capabilities);
	printf("device caps: %08x\n", caps.device_caps);

	if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
		!(caps.capabilities & V4L2_CAP_STREAMING)) {
		printf("streaming capture not supported\n");
		return -1;
	}

	memset(&fmtdesc, 0, sizeof(fmtdesc));
	memset(&frmsizeenum, 0, sizeof(frmsizeenum));
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	i            = 0;
	while (i < 20) {
		fmtdesc.index = i;
		rc            = ioctl(ptc->fd, VIDIOC_ENUM_FMT, &fmtdesc);
		if (rc < 0) {
			break;
		}

		if (!pixelformat)
			pixelformat = uvc_camera_get_pixelformat(fmtdesc.pixelformat, ptc->pic_format);

		printf("%2d: %s 0x%08x 0x%x\n", i, fmtdesc.description, fmtdesc.pixelformat, fmtdesc.flags);

		j = 0;
		while (j < 20) {
			frmsizeenum.index        = j;
			frmsizeenum.pixel_format = fmtdesc.pixelformat;
			rc                       = ioctl(ptc->fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum);
			if (rc < 0) {
				break;
			}

			if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE)
				printf("%02d: video=/dev/video%d, width=%d,height=%d\n",  j, ptc->video_id, frmsizeenum.discrete.width,
					 frmsizeenum.discrete.height);
			else
				printf("%02d: video=/dev/video%d, width=[%d %d],height=[%d %d]\n", j, ptc->video_id, frmsizeenum.stepwise.min_width,
					 frmsizeenum.stepwise.max_width, frmsizeenum.stepwise.min_height,
					 frmsizeenum.stepwise.max_height);
			j++;
		}

		i++;
	}

	if (!pixelformat) {
		printf("video=/dev/video%d, Unsupported pixel format!\n", ptc->video_id);
		return -1;
	}

	memset(&format, 0, sizeof(format));
	format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = pixelformat;
	format.fmt.pix.width       = ptc->pic_width;
	format.fmt.pix.height      = ptc->pic_height;
	rc                         = ioctl(ptc->fd, VIDIOC_S_FMT, &format);
	if (rc < 0) {
		printf("video=/dev/video%d, VIDIOC_S_FMT: %s\n", ptc->video_id, strerror(errno));
		return rc;
	} else if (format.fmt.pix.pixelformat != pixelformat || format.fmt.pix.width != ptc->pic_width ||
			   format.fmt.pix.height != ptc->pic_height) {
		printf("video=/dev/video%d, VIDIOC_S_FMT: format (0x%x) / resolution (%dx%d) not supported\n",
			ptc->video_id, pixelformat, ptc->pic_width, ptc->pic_height);
		printf("video=/dev/video%d, VIDIOC_S_FMT: format (0x%x) / resolution (%dx%d) preferred\n",
			ptc->video_id, format.fmt.pix.pixelformat, format.fmt.pix.width, format.fmt.pix.height);
		return -1;
	}

	printf("video=/dev/video%d, VIDIOC_S_FMT: format (0x%x) / resolution (%dx%d) preferred\n",
		ptc->video_id, format.fmt.pix.pixelformat, format.fmt.pix.width, format.fmt.pix.height);

	return 0;
}

int get_format_stride(uvc_camera_context *ptc)
{
	int stride;
	switch (ptc->pic_format) {
		case CAM_FMT_RAW24:
			stride = ptc->pic_width * 3;
			break;
		case CAM_FMT_RAW16:
			stride = ptc->pic_width * 2;
			break;
		case CAM_FMT_RAW12:
			stride = ptc->pic_width * 2;
			break;
		case CAM_FMT_RAW10:
			stride = ptc->pic_width * 2;
			break;
		case CAM_FMT_RAW8:
			stride = ptc->pic_width;
			break;
		default:
			stride = ptc->pic_width;
			break;
	}

	return stride;
}
int v4l2_uvc_camera_device_reqbufs(uvc_camera_context *ptc, unsigned int num)
{
	struct v4l2_requestbuffers bufrequest;
	struct v4l2_buffer buffer;
	struct v4l2_exportbuffer expbuf;
	int i, rc;

	// Request buffer allocation
	memset(&bufrequest, 0, sizeof(bufrequest));
	bufrequest.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count  = num;
	pthread_mutex_lock(&chance_test);
	rc                = ioctl(ptc->fd, VIDIOC_REQBUFS, &bufrequest);
	pthread_mutex_unlock(&chance_test);
	if (rc < 0) {
		printf("video=/dev/video%d, num = %d, VIDIOC_REQBUFS: %s\n", ptc->video_id, num, strerror(errno));
		return rc;
	}

	for (i = 0; i < num; i++) {
		// Query buffer information
		memset(&buffer, 0, sizeof(buffer));
		buffer.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index  = i;
		rc            = ioctl(ptc->fd, VIDIOC_QUERYBUF, &buffer);
		if (rc < 0) {
			printf("VIDIOC_QUERYBUF: %s\n", strerror(errno));
			return rc;
		}
		printf("buffer description:\n");
		printf("offset: %d\n", buffer.m.offset);
		printf("length: %d\n", buffer.length);

		// Queue buffer for capture
		rc = ioctl(ptc->fd, VIDIOC_QBUF, &buffer);
		if (rc < 0) {
			printf("VIDIOC_QBUF: %s\n", strerror(errno));
			return rc;
		}

		// Export buffer to user space
		memset(&expbuf, 0, sizeof(expbuf));
		expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		expbuf.index = i;
		expbuf.plane = 0;
		rc           = ioctl(ptc->fd, VIDIOC_EXPBUF, &expbuf);
		if (rc < 0) {
			printf("VIDIOC_EXPBUF: %s\n", strerror(errno));
			return rc;
		}
		printf("fd:     %d\n", expbuf.fd);
		printf("flags:  %d\n", expbuf.flags);
		ptc->dmabuf[i] = expbuf.fd;

		// Memory map buffer
		ptc->mplane_buffers[i].start[0] =
			mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, ptc->fd, buffer.m.offset);

		if (ptc->mplane_buffers[i].start[0] == MAP_FAILED) {
			printf("mmap: %s\n", strerror(errno));
			return -1;
		}

 		printf("map buffer %p\n", ptc->mplane_buffers[i].start[0]);

		 // Handle different image formats
		if (ptc->pic_format <= CAM_FMT_RAW16 && ptc->pic_format > CAM_FMT_NULL) {
			if (buffer.length == get_format_stride(ptc) * ptc->pic_height)
				ptc->mplane_buffers[i].length[0] = buffer.length;
			else {
				printf("trans to signal planes failed!!! buffer[%d].length = %d, pic_width = %d, pic_height = %d\n",
					buffer.index, buffer.length, ptc->pic_width, ptc->pic_height);
				return -1;
			}
		} else if (ptc->pic_format == CAM_FMT_NV12) {
			if (buffer.length == ptc->pic_width * ptc->pic_height * 3 / 2) {
				ptc->mplane_buffers[i].length[0] = ptc->pic_width * ptc->pic_height;
				ptc->mplane_buffers[i].start[1] = ptc->mplane_buffers[i].start[0] + ptc->mplane_buffers[i].length[0];
				ptc->mplane_buffers[i].length[1] = ptc->pic_width * ptc->pic_height / 2;
			} else {
				printf("trans to mult planes failed!!! buffer[%d].length = %d, pic_width = %d, pic_height = %d\n",
					buffer.index, buffer.length, ptc->pic_width, ptc->pic_height);
				return -1;
			}
		} else if (ptc->pic_format == CAM_FMT_YUYV) {
			if (buffer.length == ptc->pic_width * ptc->pic_height * 2) {
				ptc->mplane_buffers[i].length[0] = buffer.length;
			} else {
				printf("trans to mult planes failed!!! buffer[%d].length = %d, pic_width = %d, pic_height = %d\n",
					buffer.index, buffer.length, ptc->pic_width, ptc->pic_height);
				return -1;
			}
		}
		else {
			printf("Unknow format %d, don't support dump picture\n", ptc->pic_format);
		}
	}

	return 0;
}

int v4l2_uvc_camera_device_start(uvc_camera_context *ptc)
{
	int rc;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	rc = ioctl(ptc->fd, VIDIOC_STREAMON, &type);
	if (rc < 0) {
		printf("VIDIOC_STREAMON: %s\n", strerror(errno));
	}

	return rc;
}

int v4l2_uvc_camera_device_stop(uvc_camera_context *ptc)
{
	int rc;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	rc = ioctl(ptc->fd, VIDIOC_STREAMOFF, &type);
	if (rc < 0) {
		printf("VIDIOC_STREAMOFF: %s\n", strerror(errno));
	}

	return rc;
}

int v4l2_uvc_camera_device_close(uvc_camera_context *ptc)
{
	if (ptc->fd < 0)
		return -1;

	close(ptc->fd);
	ptc->fd = -1;
	return 0;
}

void v4l2_filename_get(char *name, const char *path, struct v4l2_buffer *pbuffer,
		uvc_camera_context *ptc, int32_t raw, df_module_name_index_e mni)
{
	struct tm *t;
	time_t tt;
	const char *suffix = "yuv";
	const char *df_name = get_module_name(mni);
	static uint32_t file_cnt = 0;

	int rc = mkdir(path, 0777);
	if (rc != 0 && errno != EEXIST) {
		printf("Failed to create directory (%s): %s\n", path, strerror(errno));
	}

	time(&tt);
	t = localtime(&tt);

	if (raw)
		suffix = "raw";

	snprintf(name, MAX_VIO_FILE_NAME, "%s/%s_%d_s%d_c%d_b%d_f%d_%02d%02d%02d.%s", path, df_name, file_cnt++, ptc->video_id,
		ptc->video_id, pbuffer->index, ptc->work_info.work_count, t->tm_hour, t->tm_min, t->tm_sec, suffix);
}

int v4l2_isp_uvc_camera_device_dumpframe(uvc_camera_context *ptc)
{
	int rc;
	fd_set fds;
	uint32_t local_loop_cnt = 0;
	char file_name[MAX_VIO_FILE_NAME] = {0};
	static struct timeval timeout = {0, TIMEOUT_US};
	struct v4l2_buffer buffer;

	if (!ptc)
		return -1;

	buffer.type         = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	do {
		FD_ZERO(&fds);
		FD_SET(ptc->fd, &fds);
		timeout.tv_sec  = TIMEOUT_S;
		timeout.tv_usec = 0;

		rc = select(ptc->fd + 1, &fds, NULL, NULL, &timeout);
		if (rc < 0)
			printf("failed to call select: %s\n", strerror(errno));
		else if (!rc)
			printf("select timeout!!!\n");
		else {
			rc = ioctl(ptc->fd, VIDIOC_DQBUF, &buffer);
			if (rc < 0) {
				printf("VIDIOC_DQBUF: %s\n", strerror(errno));
			} else {
				printf("VIDIOC_DQBUF success\n");
				if ((ptc->pic_format <= CAM_FMT_RAW16 && ptc->pic_format > CAM_FMT_NULL)) {

					if (ptc->dump_mask == 1) {
						v4l2_filename_get(file_name, RAW_DUMP_PATH, &buffer, ptc, RAW_DATA, ISP_MNI);
						dumpToFile(file_name, (char *)ptc->mplane_buffers[buffer.index].start[0], ptc->mplane_buffers[buffer.index].length[0]);
					}

				} else if (ptc->pic_format == CAM_FMT_YUYV) {
					if (ptc->dump_mask == 1) {
						v4l2_filename_get(file_name, YUV_DUMP_PATH, &buffer, ptc, YUV_DATA, ISP_MNI);
						dumpToFile(file_name, (char *)ptc->mplane_buffers[buffer.index].start[0], ptc->mplane_buffers[buffer.index].length[0]);
					}
				}
				else if (ptc->pic_format == CAM_FMT_NV12) {

					if (ptc->dump_mask == 1) {
						v4l2_filename_get(file_name, YUV_DUMP_PATH, &buffer, ptc, YUV_DATA, ISP_MNI);
						dumpToFile2plane(file_name, (char *)ptc->mplane_buffers[buffer.index].start[0], (char *)ptc->mplane_buffers[buffer.index].start[1],
							ptc->mplane_buffers[buffer.index].length[0], ptc->mplane_buffers[buffer.index].length[1]);
					}

				} else
					printf("Unknow format %d, don't support dump and stream\n", ptc->pic_format);
				if (ptc->isp_info_mask) {
					if ((ptc->loop_cnt > 0) || (local_loop_cnt++ % 200 == 0)) {
						v4l2_dr_isp_expsoure_test(ptc);
						v4l2_dr_isp_awb_test(ptc);
					}
				}

				ioctl(ptc->fd, VIDIOC_QBUF, &buffer);
			}
		}
	} while(!runtime_end(ptc));

	return 0;
}
