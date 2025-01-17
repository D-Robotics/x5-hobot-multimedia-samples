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


#include "hb_mem_mgr.h"
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"
#include "common_utils.h"
#include "./feedback_gdc.h"
#include "./mem_ion.h"

static int create_gdc_node(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info);
static int read_gdc_config(gdc_info_s * gdc_info, hb_mem_common_buf_t *bin_buf);
static int gdc_config_free(hb_mem_common_buf_t *bin_buf);
static int feedback_gdc_do(hbn_vflow_handle_t vnode, gdc_info_s *gdc_info,
						uint32_t flag);
static int custom_alloc_gdc_capture_buf(hbn_vflow_handle_t vnode, hbn_vnode_image_t *image);
static int custom_free_gdc_capture_buf(hbn_vnode_image_t *image);

static int feedback_gdc_do(hbn_vflow_handle_t vnode, gdc_info_s *gdc_info, uint32_t flag)
{
	int ret = 0;

	if ((flag == CAP_BUF_CUSTOM) || (flag == CAP_BUF_CUSTOM_EXT)) {
		ret = hbn_vnode_set_output_frame(vnode, 0, &gdc_info->output_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_sendframe(vnode, 0, &gdc_info->input_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_get_output_frame(vnode, 0, &gdc_info->output_image);
		ERR_CON_EQ(ret, 0);
	} else if (flag == CAP_BUF_INTERNAL) {
		ret = hbn_vnode_sendframe(vnode, 0, &gdc_info->input_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_getframe(vnode, 0, 1000, &gdc_info->output_image);
		ERR_CON_EQ(ret, 0);
		ret = hbn_vnode_releaseframe(vnode, 0,
					&gdc_info->output_image);
		ERR_CON_EQ(ret, 0);
	}

	return ret;
}

static int custom_free_gdc_capture_buf(hbn_vnode_image_t *image)
{
	int ret = 0;

	ret = hb_mem_free_buf(image->buffer.fd[0]);
	return ret;
}

static int custom_free_gdc_capture_buf_ext(hbn_vnode_image_t *image)
{
	int ret = 0;

	ret = mem_ion_free_graph_buf(&image->buffer);
	return ret;
}

static int custom_alloc_gdc_capture_buf(hbn_vflow_handle_t vnode, hbn_vnode_image_t *image)
{
	int ret = 0;
	int64_t alloc_flags = 0;
	gdc_ochn_attr_t gdc_ochn_attr = {0};

	memset(&gdc_ochn_attr, 0, sizeof(gdc_ochn_attr_t));
	ret = hbn_vnode_get_ochn_attr(vnode, 0, &gdc_ochn_attr);
	ERR_CON_EQ(ret, 0);
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
		HB_MEM_USAGE_PRIV_HEAP_RESERVED |
		HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN |
		HB_MEM_USAGE_CACHED |
		HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hb_mem_alloc_graph_buf(gdc_ochn_attr.output_width,
			gdc_ochn_attr.output_height, MEM_PIX_FMT_NV12, alloc_flags,
			gdc_ochn_attr.output_stride, gdc_ochn_attr.output_height, &image->buffer);
	ERR_CON_EQ(ret, 0);
	printf("gdc paddr[0] 0x%lx, paddr[1] 0x%lx, paddr[2] 0x%lx\n",
			image->buffer.phys_addr[0],
			image->buffer.phys_addr[1],
			image->buffer.phys_addr[2]
			);
	printf("buf share id %d\n ", image->buffer.share_id[0]);
	printf("buf bufferindex %d\n ", image->info.bufferindex);
	return ret;
}

static int custom_alloc_gdc_capture_buf_ext(hbn_vflow_handle_t vnode, hbn_vnode_image_t *image)
{
	int ret = 0;
	int64_t alloc_flags = 0;
	gdc_ochn_attr_t gdc_ochn_attr = {0};

	memset(image, 0, sizeof(hbn_vnode_image_t));
	memset(&gdc_ochn_attr, 0, sizeof(gdc_ochn_attr_t));
	ret = hbn_vnode_get_ochn_attr(vnode, 0, &gdc_ochn_attr);
	ERR_CON_EQ(ret, 0);
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED |
		HB_MEM_USAGE_PRIV_HEAP_RESERVED |
		HB_MEM_USAGE_CPU_READ_OFTEN |
		HB_MEM_USAGE_CPU_WRITE_OFTEN |
		// HB_MEM_USAGE_CACHED |
		HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;

	ret = mem_ion_alloc_graph_buf(gdc_ochn_attr.output_width,
			gdc_ochn_attr.output_height, MEM_PIX_FMT_NV12, alloc_flags,
			gdc_ochn_attr.output_stride, gdc_ochn_attr.output_height,
			&image->buffer);
	ERR_CON_EQ(ret, 0);
	printf("gdc paddr[0] 0x%lx, paddr[1] 0x%lx, paddr[2] 0x%lx\n",
			image->buffer.phys_addr[0],
			image->buffer.phys_addr[1],
			image->buffer.phys_addr[2]
			);
	image->info.bufferindex = 0;
	ret = mem_info_check(image);

	return ret;
}

static int read_gdc_config(gdc_info_s *gdc_info, hb_mem_common_buf_t *bin_buf) {
	int64_t alloc_flags = 0;
	int ret = 0;
	int offset = 0;
	char *gdc_bin_file = gdc_info->gdc_bin_file;
	char *cfg_buf = NULL;

	FILE *fp = fopen(gdc_bin_file, "r");
	if (fp == NULL) {
		printf("File %s open failed\n", gdc_bin_file);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	cfg_buf = malloc(file_size);
	int n = fread(cfg_buf, 1, file_size, fp);
	if (n != file_size) {
		printf("Read file size failed\n");
	}
	fclose(fp);

	memset(bin_buf, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(file_size, alloc_flags, bin_buf);
	if (ret != 0 || bin_buf->virt_addr == NULL) {
		printf("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		free(cfg_buf);
		return -1;
	}

	memcpy(bin_buf->virt_addr, cfg_buf, file_size);
	ret = hb_mem_flush_buf(bin_buf->fd, offset, file_size);
	ERR_CON_EQ(ret, 0);

	free(cfg_buf);
	return ret;
}

static int gdc_config_free(hb_mem_common_buf_t *bin_buf) {
	return hb_mem_free_buf(bin_buf->fd);
}

static int create_gdc_node(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info)
{
	int ret = 0;
	uint32_t chn_id = 0;
	uint32_t hw_id = 0;
	hbn_vnode_handle_t *gdc_vnode_handle = &pipe_contex->gdc_node_handle;
	gdc_attr_t gdc_attr = {0};

	printf("ichn input width = %d\n", gdc_info->input_width);
	printf("ichn input height = %d\n", gdc_info->input_height);
	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, gdc_vnode_handle);
	ERR_CON_EQ(ret, 0);

	memset((void *)&gdc_attr, 0, sizeof(gdc_attr_t));
	gdc_attr.config_addr = pipe_contex->bin_buf.phys_addr;
	gdc_attr.config_size = pipe_contex->bin_buf.size;
	gdc_attr.binary_ion_id = pipe_contex->bin_buf.share_id;
	gdc_attr.binary_offset = pipe_contex->bin_buf.offset;
	gdc_attr.total_planes = 2;
	gdc_attr.div_width = 0;
	gdc_attr.div_height = 0;
	ret = hbn_vnode_set_attr(*gdc_vnode_handle, &gdc_attr);
	ERR_CON_EQ(ret, 0);

	gdc_ichn_attr_t gdc_ichn_attr = {0};
	memset(&gdc_ichn_attr, 0, sizeof(gdc_ichn_attr_t));
	gdc_ichn_attr.input_width = gdc_info->input_width;
	gdc_ichn_attr.input_height = gdc_info->input_height;
	gdc_ichn_attr.input_stride = gdc_info->input_width;
	ret = hbn_vnode_set_ichn_attr(*gdc_vnode_handle, chn_id, &gdc_ichn_attr);
	ERR_CON_EQ(ret, 0);

	gdc_ochn_attr_t gdc_ochn_attr = {0};
	memset(&gdc_ochn_attr, 0, sizeof(gdc_ochn_attr));
	gdc_ochn_attr.output_width = gdc_info->output_width;
	gdc_ochn_attr.output_height = gdc_info->output_height;
	gdc_ochn_attr.output_stride = gdc_info->output_width;
	ret = hbn_vnode_set_ochn_attr(*gdc_vnode_handle, chn_id, &gdc_ochn_attr);
	ERR_CON_EQ(ret, 0);

	return 0;
}

static int internal_alloc_gdc_capture_buf(hbn_vflow_handle_t gdc_handle)
{
	int ret = 0;
	hbn_buf_alloc_attr_t alloc_attr = {0};

	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
					HB_MEM_USAGE_CPU_WRITE_OFTEN |
					HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(gdc_handle, 0, &alloc_attr);
	ERR_CON_EQ(ret, 0);

	printf(">>> %s %d done\n", __func__, __LINE__);

	return ret;
}

int feedback_gdc_create(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info, uint32_t flag)
{
	int ret = 0;

	ret = read_gdc_config(gdc_info, &pipe_contex->bin_buf);
	ERR_CON_EQ(ret, 0);
	ret = create_gdc_node(pipe_contex, gdc_info);
	ERR_CON_EQ(ret, 0);

	if (flag == CAP_BUF_CUSTOM) {
		ret = custom_alloc_gdc_capture_buf(pipe_contex->gdc_node_handle,
						&gdc_info->output_image);
		ERR_CON_EQ(ret, 0);
	} else if (flag == CAP_BUF_INTERNAL) {
		ret = internal_alloc_gdc_capture_buf(pipe_contex->gdc_node_handle);
		ERR_CON_EQ(ret, 0);
	} else if (flag == CAP_BUF_CUSTOM_EXT) {
		ret = custom_alloc_gdc_capture_buf_ext(pipe_contex->gdc_node_handle,
						&gdc_info->output_image);
		ERR_CON_EQ(ret, 0);
	}

	printf(">>>> %s %d handle %ld, flag %d\n",
		__func__, __LINE__, pipe_contex->gdc_node_handle, flag);

	return ret;
}

int feedback_gdc(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info, uint32_t flag)
{
	int ret = 0;

	// printf(">>>> %s %d handle %ld, flag %d\n",
	// 	__func__, __LINE__, pipe_contex->gdc_node_handle, flag);
	ret = feedback_gdc_do(pipe_contex->gdc_node_handle, gdc_info, flag);
	ERR_CON_EQ(ret, 0);

	return ret;
}

int feedback_gdc_destory(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info, uint32_t flag)
{
	int ret = 0;

	printf(">>>> %s %d handle %ld, flag %d\n",
		__func__, __LINE__, pipe_contex->gdc_node_handle, flag);
	if (flag == CAP_BUF_CUSTOM) {
		ret = custom_free_gdc_capture_buf(&gdc_info->output_image);
	} else if (flag == CAP_BUF_CUSTOM_EXT) {
		ret = custom_free_gdc_capture_buf_ext(&gdc_info->output_image);
	}
	gdc_config_free(&pipe_contex->bin_buf);

	return ret;
}