/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

#include "hb_mem_mgr.h"
#include "common_utils.h"
#include "./feedback_vse.h"
#include "./mem_ion.h"

#define VSE_MAX_CHANNELS 6
#define VSE_ALL_CHN_MASK ((1 << (VSE_MAX_CHANNELS)) -1)
#define MAX_CHANELS VSE_MAX_CHANNELS
#define BIT(i) (1 << (i))

#define FEEDBACK_VSE	1
#define FEEDBACK_GDC	2
#define FEEDBACK_VSE_BIND_GDC	3

static int create_vse_node(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info, uint32_t output_chn_mask);
static int feedback_vse_do(hbn_vflow_handle_t vnode, scaler_info_s *scaler_info,
			uint32_t output_chn_mask, uint32_t flag);
static int custom_alloc_vse_capture_buf(hbn_vflow_handle_t vnode,
				uint32_t output_chn_mask, hbn_vnode_image_t *imagev);
static int custom_free_vse_capture_buf(uint32_t output_chn_mask, hbn_vnode_image_t *imagev);

static int create_vse_node(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info, uint32_t output_chn_mask)
{
	int ret = 0;
	int i;
	vse_attr_t vse_attr = {0};
	vse_ichn_attr_t vse_ichn_attr = {0};
	vse_ochn_attr_t vse_ochn_attr[VSE_MAX_CHANNELS] = {0};
	uint32_t ichn_id = 0;
	uint32_t hw_id = 0;
	uint32_t input_width = 0;
	uint32_t input_height = 0;
	hbn_vnode_handle_t *vse_node_handle = &pipe_contex->vse_node_handle;

	input_width = scaler_info->input_width;
	input_height = scaler_info->input_height;

	printf("ichn input width = %d\n", input_width);
	printf("ichn input height = %d\n", input_height);

	vse_ichn_attr.width = input_width;
	vse_ichn_attr.height = input_height;
	vse_ichn_attr.fmt = FRM_FMT_NV12;
	vse_ichn_attr.bit_width = 8;

	for (i = 0; i < VSE_MAX_CHANNELS; ++i) {
		vse_ochn_attr[i].chn_en = CAM_TRUE;
		vse_ochn_attr[i].roi.x = 0;
		vse_ochn_attr[i].roi.y = 0;
		vse_ochn_attr[i].roi.w = input_width;
		vse_ochn_attr[i].roi.h = input_height;
		vse_ochn_attr[i].fmt = FRM_FMT_NV12;
		vse_ochn_attr[i].bit_width = 8;
	}

	// 输出原分辨率
	vse_ochn_attr[0].target_w = input_width;
	vse_ochn_attr[0].target_h = input_height;

	// 输出 16 像素对齐的常用算法图像使用的分辨率
	vse_ochn_attr[1].target_w = 512;
	vse_ochn_attr[1].target_h = 512;

	// 输出非 16 像素对齐的常用算法图像使用的分辨率
	vse_ochn_attr[2].target_w = 224;
	vse_ochn_attr[2].target_h = 224;

	// 设置VSE通道2输出属性，ROI为原图中心点不变，宽、高各裁剪一半，输出图像宽、高等于ROI区域宽高
	vse_ochn_attr[3].roi.x = input_width / 2 - input_width / 4;
	vse_ochn_attr[3].roi.y = input_height / 2 - input_height / 4;
	vse_ochn_attr[3].roi.w = input_width / 2;
	vse_ochn_attr[3].roi.h = input_height / 2;

	// 缩小到支持的最小分辨率
	vse_ochn_attr[3].target_w = 64;
	vse_ochn_attr[3].target_h = 64;

	// 放大到支持的最大分辨率
	vse_ochn_attr[4].target_w = 672;
	vse_ochn_attr[4].target_h = 672;
	vse_ochn_attr[5].target_w = (input_width * 2) > 4096 ? 4096 : (input_width * 2);
	vse_ochn_attr[5].target_h = (input_height * 2) > 3076 ? 3076 : (input_height * 2);

	ret = hbn_vnode_open(HB_VSE, hw_id, AUTO_ALLOC_ID, vse_node_handle);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_attr(*vse_node_handle, &vse_attr);
	ERR_CON_EQ(ret, 0);

	ret = hbn_vnode_set_ichn_attr(*vse_node_handle, ichn_id, &vse_ichn_attr);
	ERR_CON_EQ(ret, 0);

	for (int i = 0; i < VSE_MAX_CHANNELS; ++i) {
		if ((output_chn_mask & BIT(i)) == 0) {
			continue;
		}
		printf("hbn_vnode_set_ochn_attr: %d, %dx%d\n", i, vse_ochn_attr[i].target_w, vse_ochn_attr[i].target_h);
		ret = hbn_vnode_set_ochn_attr(*vse_node_handle, i, &vse_ochn_attr[i]);
		ERR_CON_EQ(ret, 0);
	}

	printf("%s done\n", __func__);
	return 0;
}

static int feedback_vse_do(hbn_vflow_handle_t vnode, scaler_info_s *scaler_info,
			uint32_t output_chn_mask, uint32_t flag)
{
	int i, ret = 0;

	if ((flag == CAP_BUF_CUSTOM) || (flag == CAP_BUF_CUSTOM_EXT)) {
		for (i = 0; i < VSE_MAX_CHANNELS; i++) {
			if ((output_chn_mask & BIT(i)) == 0)
				continue;
			ret = hbn_vnode_set_output_frame(vnode, i,
						&scaler_info->output_image[i]);
			ERR_CON_EQ(ret, 0);
		}

		ret = hbn_vnode_sendframe(vnode, 0, &scaler_info->input_image);
		ERR_CON_EQ(ret, 0);

		for (i = 0; i < VSE_MAX_CHANNELS; i++) {
			if ((output_chn_mask & BIT(i)) == 0)
				continue;
			ret = hbn_vnode_get_output_frame(vnode, i,
						&scaler_info->output_image[i]);
			ERR_CON_EQ(ret, 0);
		}
	} else if (flag == CAP_BUF_INTERNAL) {
		ret = hbn_vnode_sendframe(vnode, 0, &scaler_info->input_image);
		ERR_CON_EQ(ret, 0);
		for (i = 0; i < VSE_MAX_CHANNELS; i++) {
			if ((output_chn_mask & BIT(i)) == 0)
				continue;
			ret = hbn_vnode_getframe(vnode, i, 1000,
						&scaler_info->output_image[i]);
			ERR_CON_EQ(ret, 0);
		}
		// do something
		for (i = 0; i < VSE_MAX_CHANNELS; i++) {
			if ((output_chn_mask & BIT(i)) == 0)
				continue;
			// printf("*** scaler_info->output_image[i] %d\n",
			// 	scaler_info->output_image[i].info.bufferindex);
			ret = hbn_vnode_releaseframe(vnode, i,
						&scaler_info->output_image[i]);
			ERR_CON_EQ(ret, 0);
		}

	}
	// printf("%s done\n", __func__);
	return ret;
}

static int custom_free_vse_capture_buf(uint32_t output_chn_mask, hbn_vnode_image_t *imagev)
{
	int ret = 0, i;

	for (i = 0; i < MAX_CHANELS; i++) {
		if (!(output_chn_mask & BIT(i))) {
			continue;
		}
		ret = hb_mem_free_buf(imagev[i].buffer.fd[0]);
	}

	return ret;
}

static int custom_free_vse_capture_buf_ext(uint32_t output_chn_mask, hbn_vnode_image_t *imagev)
{
	int ret = 0, i;

	for (i = 0; i < MAX_CHANELS; i++) {
		if (!(output_chn_mask & BIT(i))) {
			continue;
		}
		ret = mem_ion_free_graph_buf(&imagev[i].buffer);
	}

	return ret;
}

static int internal_alloc_vse_capture_buf(hbn_vflow_handle_t vnode,
				uint32_t output_chn_mask)
{
	int ret = 0, i;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	for (i = 0; i < MAX_CHANELS; i++) {
		if ((output_chn_mask & BIT(i)) == 0) {
			continue;
		}
		memset(&alloc_attr, 0, sizeof(hbn_buf_alloc_attr_t));
		alloc_attr.buffers_num = 3;
		alloc_attr.is_contig = 1;
		alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
		ret = hbn_vnode_set_ochn_buf_attr(vnode, i, &alloc_attr);
		ERR_CON_EQ(ret, 0);
	}
	return ret;
}

static int custom_alloc_vse_capture_buf(hbn_vflow_handle_t vnode,
				uint32_t output_chn_mask, hbn_vnode_image_t *imagev)
{
	int ret = 0, i;
	int64_t alloc_flags = 0;
	vse_ochn_attr_t vse_ochn_attr = {0};

	for (i = 0; i < MAX_CHANELS; i++) {
		if (!(output_chn_mask & BIT(i))) {
			continue;
		}
		memset(&vse_ochn_attr, 0, sizeof(vse_ochn_attr_t));
		ret = hbn_vnode_get_ochn_attr(vnode, i, &vse_ochn_attr);
		ERR_CON_EQ(ret, 0);
		alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
			HB_MEM_USAGE_PRIV_HEAP_RESERVED |
			HB_MEM_USAGE_CPU_READ_OFTEN |
			HB_MEM_USAGE_CPU_WRITE_OFTEN |
			HB_MEM_USAGE_CACHED |
			HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
		ret = hb_mem_alloc_graph_buf(vse_ochn_attr.target_w,
				vse_ochn_attr.target_h, MEM_PIX_FMT_NV12, alloc_flags,
				vse_ochn_attr.target_w, vse_ochn_attr.target_h, &imagev[i].buffer);
		ERR_CON_EQ(ret, 0);
		printf("vse chn %d: paddr[0] 0x%lx, paddr[1] 0x%lx, paddr[2] 0x%lx\n",
				i,
				imagev[i].buffer.phys_addr[0],
				imagev[i].buffer.phys_addr[1],
				imagev[i].buffer.phys_addr[2]
				);
	}

	return ret;
}

static int custom_alloc_vse_capture_buf_ext(hbn_vflow_handle_t vnode,
				uint32_t output_chn_mask, hbn_vnode_image_t *imagev)
{
	int ret = 0, i;
	int64_t alloc_flags = 0;
	vse_ochn_attr_t vse_ochn_attr = {0};

	for (i = 0; i < MAX_CHANELS; i++) {
		if (!(output_chn_mask & BIT(i))) {
			continue;
		}
		memset(&vse_ochn_attr, 0, sizeof(vse_ochn_attr_t));
		ret = hbn_vnode_get_ochn_attr(vnode, i, &vse_ochn_attr);
		ERR_CON_EQ(ret, 0);
		alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
			HB_MEM_USAGE_PRIV_HEAP_RESERVED |
			HB_MEM_USAGE_CPU_READ_OFTEN |
			HB_MEM_USAGE_CPU_WRITE_OFTEN |
			// HB_MEM_USAGE_CACHED |
			HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
		ret = mem_ion_alloc_graph_buf(vse_ochn_attr.target_w,
				vse_ochn_attr.target_h, MEM_PIX_FMT_NV12, alloc_flags,
				vse_ochn_attr.target_w, vse_ochn_attr.target_h, &imagev[i].buffer);
		ERR_CON_EQ(ret, 0);
		printf("vse chn %d: paddr[0] 0x%lx, paddr[1] 0x%lx, paddr[2] 0x%lx\n",
				i,
				imagev[i].buffer.phys_addr[0],
				imagev[i].buffer.phys_addr[1],
				imagev[i].buffer.phys_addr[2]
				);
		imagev[i].info.bufferindex = 0;
		ret = mem_info_check(&imagev[i]);
	}

	return ret;
}

int feedback_vse_create(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info,
			uint32_t output_chn_mask, uint32_t flag)
{
	int ret = 0;

	ret = create_vse_node(pipe_contex, scaler_info, output_chn_mask);
	ERR_CON_EQ(ret, 0);

	if (flag == CAP_BUF_CUSTOM) {
		ret = custom_alloc_vse_capture_buf(pipe_contex->vse_node_handle,
						output_chn_mask,
						(hbn_vnode_image_t *)&scaler_info->output_image);
		ERR_CON_EQ(ret, 0);
	} else if (flag == CAP_BUF_INTERNAL) {
		internal_alloc_vse_capture_buf(pipe_contex->vse_node_handle, output_chn_mask);
	} else if (flag == CAP_BUF_CUSTOM_EXT) {
		ret = custom_alloc_vse_capture_buf_ext(pipe_contex->vse_node_handle,
						output_chn_mask,
						(hbn_vnode_image_t *)&scaler_info->output_image);
		ERR_CON_EQ(ret, 0);
	}

	printf(">>>> %s %d handle %ld, ochn_mask 0x%x flag %d \n",
		__func__, __LINE__, pipe_contex->vse_node_handle, output_chn_mask, flag);
	return ret;
}


int feedback_vse(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info,
			uint32_t output_chn_mask, uint32_t flag)
{
	int ret = 0;

	ret = feedback_vse_do(pipe_contex->vse_node_handle, scaler_info,
				output_chn_mask, flag);
	ERR_CON_EQ(ret, 0);

	return ret;
}

int feedback_vse_destory(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info,
			uint32_t output_chn_mask, uint32_t flag)
{
	int ret = 0;

	printf(">>>> %s %d handle %ld, ochn_mask 0x%x flag %d \n",
		__func__, __LINE__, pipe_contex->vse_node_handle, output_chn_mask, flag);
	if (flag == CAP_BUF_CUSTOM) {
		ret = custom_free_vse_capture_buf(output_chn_mask,
					(hbn_vnode_image_t *)&scaler_info->output_image);
	} else if (flag == CAP_BUF_CUSTOM_EXT) {
		ret = custom_free_vse_capture_buf_ext(output_chn_mask,
					(hbn_vnode_image_t *)&scaler_info->output_image);
	}

	return ret;
}
