#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "utils/utils_log.h"

#include "vpp_preparam.h"
#include "solution_config.h"

// 支持时间、文字、画线、位图
void* vpp_osd_set_timestamp_thread(void *ptr)
{
	tsThread *privThread = (tsThread*)ptr;
	time_t tt,tt_last=0;

	mThreadSetName(privThread, __func__);

	// 获取区域配置
	// SC_LOGI("Handle: %d", Handle);

	while(privThread->eState == E_THREAD_RUNNING) {
		tt = time(0);
		if (tt > tt_last) {
			tt_last = tt;
		}
		usleep(100000); // 每秒设置一次
	}
	mThreadFinish(privThread);
	return NULL;
}

void vpp_graphic_buf_to_bpu_buffer_info(const hbn_vnode_image_t *src, bpu_buffer_info_t *dst)
{
	if (!src || !dst) return;

	const hbn_frame_info_t *image_info = &src->info;
	const hb_mem_graphic_buf_t *graphic_buf = &src->buffer;

	dst->plane_cnt = graphic_buf->plane_cnt;
	dst->width = graphic_buf->width;
	dst->height = graphic_buf->height;
	dst->w_stride = graphic_buf->stride;

	int plane_count = graphic_buf->plane_cnt;
	for (int i = 0; i < plane_count; ++i) {
		dst->addr[i] = graphic_buf->virt_addr[i];
		dst->paddr[i] = graphic_buf->phys_addr[i];
	}

	dst->tv.tv_sec = image_info->timestamps / 1000000000;
	dst->tv.tv_usec = (image_info->timestamps % 1000000000) / 1000;

	// Set remaining addr and paddr to NULL and 0 respectively
	for (int i = plane_count; i < MAX_GRAPHIC_BUF_COMP; ++i) {
		dst->addr[i] = NULL;
		dst->paddr[i] = 0;
	}
}

void vpp_video_frame_buffer_info_to_bpu_buffer_info(const mc_video_frame_buffer_info_t *src,
	bpu_buffer_info_t *dst)
{
	if (!src || !dst) return;

	dst->plane_cnt = 1;
	dst->width = src->width;
	dst->height = src->height;
	dst->w_stride = src->stride;

	dst->tv.tv_sec = src->pts / 1000000;
	dst->tv.tv_usec = src->pts % 1000000;

	for (int i = 0; i < 1; ++i) {
		dst->addr[i] = src->vir_ptr[i];
		dst->paddr[i] = src->phy_ptr[i];
	}
}

void vpp_video_frame_buffer_info_to_vnode_image(const mc_video_frame_buffer_info_t *src,
	hbn_vnode_image_t *dst)
{
	if (!src || !dst) return;

	dst->buffer.plane_cnt = 1;
	dst->buffer.format = FRM_FMT_NV12; // src->pix_fmt;
	dst->buffer.width = src->width;
	dst->buffer.height = src->height;
	 dst->buffer.stride = src->stride;
	dst->buffer.vstride = src->vstride;
	// dst->buffer.flags = 0; // Default flags
	dst->buffer.size[0] = src->compSize[0]; // Y component size
	dst->buffer.size[1] = src->compSize[1]; // U/V component size
	dst->buffer.size[2] = src->compSize[2]; // V/U component size

	dst->info.tv.tv_sec = src->pts / 1000000;
	dst->info.tv.tv_usec = src->pts % 1000000;

	for (int i = 0; i < 3; ++i) {
		dst->buffer.virt_addr[i] = src->vir_ptr[i];
		dst->buffer.phys_addr[i] = src->phy_ptr[i];
	}

	// Set metadata to NULL
	dst->metadata = NULL;
}
