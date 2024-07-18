/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-02-23 14:01:59
 * @LastEditTime: 2023-03-05 15:57:48
 ***************************************************************************/

#include "vp_gdc.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gdc_cfg.h"
#include "gdc_bin_cfg.h"

#include "utils/utils_log.h"

#include "vp_wrap.h"

static gdc_list_info_t g_gdc_list_info[] = {
    {
        .sensor_name = "sc202cs",
        .gdc_file_name = "../gdc_bin/sc202cs_gdc.bin",
        .is_valid = -1
    },
	{
        .sensor_name = "sc230ai",
        .gdc_file_name = "../gdc_bin/sc230ai_gdc.bin",
        .is_valid = -1
    }
};

static int get_gdc_config(const char *gdc_bin_file, hb_mem_common_buf_t *bin_buf) {
	int64_t alloc_flags = 0;
	int ret = 0;
	int offset = 0;
	char *cfg_buf = NULL;

	FILE *fp = fopen(gdc_bin_file, "r");
	if (fp == NULL) {
		SC_LOGE("File %s open failed\n", gdc_bin_file);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	cfg_buf = malloc(file_size);
	int n = fread(cfg_buf, 1, file_size, fp);
	if (n != file_size) {
        free(cfg_buf);
		SC_LOGE("Read file size failed\n");
        fclose(fp);
        return -1;
	}
	fclose(fp);

	memset(bin_buf, 0, sizeof(hb_mem_common_buf_t));
	alloc_flags = HB_MEM_USAGE_MAP_INITIALIZED | HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD | HB_MEM_USAGE_CPU_READ_OFTEN |
				HB_MEM_USAGE_CPU_WRITE_OFTEN | HB_MEM_USAGE_CACHED;
	ret = hb_mem_alloc_com_buf(file_size, alloc_flags, bin_buf);
	if (ret != 0 || bin_buf->virt_addr == NULL) {
        free(cfg_buf);
		SC_LOGE("hb_mem_alloc_com_buf for bin failed, ret = %d\n", ret);
		return -1;
	}

	memcpy(bin_buf->virt_addr, cfg_buf, file_size);
	ret = hb_mem_flush_buf(bin_buf->fd, offset, file_size);
	if (ret != 0 || bin_buf->virt_addr == NULL) {
        free(cfg_buf);
		SC_LOGE("hb_mem_flush_buf for bin failed, ret = %d\n", ret);
		return -1;
	}

    free(cfg_buf);

	return ret;
}

const char * vp_gdc_get_bin_file(const char *sensor_name){
    char *gdc_file = NULL;

    for(int i = 0; i < sizeof(g_gdc_list_info)/sizeof(gdc_list_info_t); i++){
        int config_sensor_name_len = strlen(g_gdc_list_info[i].sensor_name);
        int ret = strncmp(sensor_name, g_gdc_list_info[i].sensor_name, config_sensor_name_len);
        if(ret == 0){
            gdc_file = g_gdc_list_info[i].gdc_file_name;
            break;
        }
    }


	if ((gdc_file != NULL) && access(gdc_file, F_OK) != 0) {
		SC_LOGE("not found gdc file %s, so return null.", gdc_file);
		return NULL;
	}
    //TODO:检测文件是否有效
    return gdc_file;
}


int32_t vp_gdc_init(vp_vflow_contex_t *vp_vflow_contex){
    SC_LOGI("gdc init.");

	int ret = 0;
	vp_vflow_contex->gdc_info.gdc_fd = 0;
	vp_vflow_contex->gdc_info.bin_buf_is_valid = -1;

	if(vp_vflow_contex->gdc_info.status != GDC_STATUS_OPEN){
		SC_LOGI("%s not enable gdc: %d, so return direct.",
			vp_vflow_contex->gdc_info.sensor_name, vp_vflow_contex->gdc_info.status);
		return 0;
	}

    const char* gdc_bin_file = vp_gdc_get_bin_file(vp_vflow_contex->gdc_info.sensor_name);
	if(gdc_bin_file == NULL){
		SC_LOGE("%s is enable gdc, but gdc bin file is not set.", vp_vflow_contex->gdc_info.sensor_name);
		return -1;
	}
    ret = get_gdc_config(gdc_bin_file, &vp_vflow_contex->gdc_info.bin_buf);
	if(ret != 0){
		SC_LOGE("%s is enable gdc, but gdc bin file [%s] is not valid.",
			vp_vflow_contex->gdc_info.sensor_name, gdc_bin_file);
		return -1;
	}
	vp_vflow_contex->gdc_info.bin_buf_is_valid = 1;

	uint32_t hw_id = 0;
	ret = hbn_vnode_open(HB_GDC, hw_id, AUTO_ALLOC_ID, &(vp_vflow_contex->gdc_info.gdc_fd));
	if(ret != 0){
		SC_LOGE("%s is enable gdc and gdc bin file [%s] is valid, but open failed %d.",
			vp_vflow_contex->gdc_info.sensor_name, gdc_bin_file, ret);
		return -1;
	}
	gdc_attr_t gdc_attr = {0};
	gdc_attr.config_addr = vp_vflow_contex->gdc_info.bin_buf.phys_addr;
	gdc_attr.config_size = vp_vflow_contex->gdc_info.bin_buf.size;
	gdc_attr.binary_ion_id = vp_vflow_contex->gdc_info.bin_buf.share_id;
	gdc_attr.binary_offset = vp_vflow_contex->gdc_info.bin_buf.offset;
	gdc_attr.total_planes = 2;
	gdc_attr.div_width = 0;
	gdc_attr.div_height = 0;
	ret = hbn_vnode_set_attr(vp_vflow_contex->gdc_info.gdc_fd, &gdc_attr);
	if(ret != 0){
		SC_LOGE("%s is enable gdc and gdc bin file [%s] is valid, but set attr failed %d.",
			vp_vflow_contex->gdc_info.sensor_name, gdc_bin_file, ret);
		return -1;
	}

	uint32_t chn_id = 0;

	gdc_ichn_attr_t gdc_ichn_attr = {0};
	gdc_ichn_attr.input_width = vp_vflow_contex->gdc_info.input_width;
	gdc_ichn_attr.input_height = vp_vflow_contex->gdc_info.input_height;
	gdc_ichn_attr.input_stride = vp_vflow_contex->gdc_info.input_width;
	ret = hbn_vnode_set_ichn_attr(vp_vflow_contex->gdc_info.gdc_fd, chn_id, &gdc_ichn_attr);
	if(ret != 0){
		SC_LOGE("%s is enable gdc and gdc bin file [%s] is valid, but set ichn failed %d.",
			vp_vflow_contex->gdc_info.sensor_name, gdc_bin_file, ret);
		return -1;
	}

	gdc_ochn_attr_t gdc_ochn_attr = {0};
	gdc_ochn_attr.output_width = vp_vflow_contex->gdc_info.input_width;
	gdc_ochn_attr.output_height = vp_vflow_contex->gdc_info.input_height;
	gdc_ochn_attr.output_stride = vp_vflow_contex->gdc_info.input_width;
	ret = hbn_vnode_set_ochn_attr(vp_vflow_contex->gdc_info.gdc_fd, chn_id, &gdc_ochn_attr);
	if(ret != 0){
		SC_LOGE("%s is enable gdc and gdc bin file [%s] is valid, but set ochn failed %d.",
			vp_vflow_contex->gdc_info.sensor_name, gdc_bin_file, ret);
		return -1;
	}
	hbn_buf_alloc_attr_t alloc_attr = {0};
	alloc_attr.buffers_num = 3;
	alloc_attr.is_contig = 1;
	alloc_attr.flags = HB_MEM_USAGE_CPU_READ_OFTEN |
					HB_MEM_USAGE_CPU_WRITE_OFTEN |
					HB_MEM_USAGE_CACHED;
	ret = hbn_vnode_set_ochn_buf_attr(vp_vflow_contex->gdc_info.gdc_fd, chn_id, &alloc_attr);
	if(ret != 0){
		SC_LOGE("%s is enable gdc and gdc bin file [%s] is valid, but set ochn buffer failed %d.",
			vp_vflow_contex->gdc_info.sensor_name, gdc_bin_file, ret);
		return -1;
	}

    return 0;
}

int32_t vp_gdc_start(vp_vflow_contex_t *vp_vflow_contex){
	SC_LOGI("gdc start.");
    return 0;
}
int32_t vp_gdc_stop(vp_vflow_contex_t *vp_vflow_contex){
	SC_LOGI("gdc stop.");
    return 0;
}
int32_t vp_gdc_deinit(vp_vflow_contex_t *vp_vflow_contex){
	if(vp_vflow_contex->gdc_info.gdc_fd != 0){
		hbn_vnode_close(vp_vflow_contex->gdc_info.gdc_fd);
	}

	if(vp_vflow_contex->gdc_info.bin_buf_is_valid != -1){
		hb_mem_free_buf(vp_vflow_contex->gdc_info.bin_buf.fd);
	}
    return 0;
}



int32_t vp_gdc_send_frame(vp_vflow_contex_t *vp_vflow_contex, hbn_vnode_image_t *image_frame)
{
	int32_t ret = 0;
	hbn_vnode_handle_t gdc_handle = vp_vflow_contex->gdc_info.bin_buf.fd;
	ret = hbn_vnode_sendframe(gdc_handle, 0, image_frame);
	if (ret != 0) {
		SC_LOGE("hbn_vnode_sendframe failed(%d)", ret);
		return -1;
	}
	return ret;
}

int32_t vp_gdc_get_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame)
{
	int32_t ret = 0;
	hbn_vnode_handle_t gdc_handle = vp_vflow_contex->gdc_info.bin_buf.fd;

	ret = hbn_vnode_getframe(gdc_handle, ochn_id, VP_GET_FRAME_TIMEOUT, frame->hbn_vnode_image);
	if (ret != 0) {
		SC_LOGE("hbn_vnode_getframe gdc channel %d failed(%d)", ochn_id, ret);
	}

	return ret;
}

int32_t vp_gdc_release_frame(vp_vflow_contex_t *vp_vflow_contex,
	int32_t ochn_id, ImageFrame *frame)
{
	int32_t ret = 0;
	hbn_vnode_handle_t gdc_handle = vp_vflow_contex->gdc_info.bin_buf.fd;

	ret = hbn_vnode_releaseframe(gdc_handle, ochn_id, frame->hbn_vnode_image);

	return ret;
}
