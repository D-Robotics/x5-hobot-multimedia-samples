// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>

//for hbm
#include "hb_mem_mgr.h"
#include "common_utils.h"

//for display
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#define DRM_MAX_PLANES 3

typedef struct param_config_s{
	int width;
	int height;
	char *output;		//hdmi, dsi
}param_config_t;

typedef struct display_context_s
{
	int width;
	int height;

	int drm_fd;

	const uint32_t crtc_id;
	const uint32_t plane_id;
	uint32_t connector_id;

	int connector_type;

	param_config_t param_config;
}display_context_t;

typedef struct drm_frame_buffer_info_s{

	int dump_handle;

	int frame_buffer_id;
	int frame_buffer_size;
	void *frame_buffer_vaddr;
}drm_frame_buffer_info_t;

void print_help(char *test_case)
{
	printf("Usage: %s [OPTIONS]\n", test_case);
	printf("Options:\n");
	printf("  -o <output>                       Specify display connector: hdmi, dsi, (hdmi is default)\n");
	printf("  -h <help>                         Show this help message\n");
	printf("     For Example, specific hdmi as display: ./%s -o hdmi\n", test_case);
	printf("     For Example, specific dsi as display : ./%s -o dsi\n", test_case);

}

int parser_params(int argc, char **argv, param_config_t *param)
{
	struct option const long_options[] = {

		{"output", optional_argument, NULL, 'o'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, NULL, 0}};

	int opt_index = 0;
	int c = 0;

	param->width = -1;
	param->height = -1;
	param->output = "hdmi";

	while ((c = getopt_long(argc, argv, "o:h",
							long_options, &opt_index)) != -1){
		switch (c)
		{
		case 'o':
			param->output = optarg;
			break;
		case 'h':
		default:
			print_help("sample_rotation_square");
			return -1;
		}
	}
	if((strcmp(param->output, "hdmi") != 0) && (strcmp(param->output, "dsi") != 0)){
		printf("display connector current only support hdmi and dsi, but input param is [%s].\n", param->output);
		return -1;
	}
	printf("\nPrint param Config:\n");
	printf("\tOutput   :\n");
	if((param->width == -1) || ((param->height == -1))){
		printf("\t\t Resolution: select connector's first config from EDID\n");
	}else{
		printf("\t\t Resolution: %d*%d\n", param->width, param->height);
	}
	printf("\t\t Connector: %s\n", param->output);

	return 0;
}

void printf_insmod_driver_cmd(void){
		perror("drmOpen failed, maybe display driver is not loaded, please execute the following command:");
		printf("\tmodprobe panel-jc-050hd134\n");
		printf("\tmodprobe galcore\n");
		printf("\tmodprobe vio_n2d\n");
		printf("\tmodprobe lontium_lt8618\n");
		printf("\tmodprobe vs-x5-syscon-bridge\n");
		printf("\tmodprobe vs_drm\n");
		printf("\n\n");
}

static uint32_t get_bpp_from_format(uint32_t format)
{
    switch (format)
    {
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_XBGR4444:
        return 16; // 16 bits per pixel (4 bits per channel)
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:
        return 16; // 16 bits per pixel (5, 6, 5 bits per channel)
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_XBGR1555:
        return 16; // 16 bits per pixel (1 bit alpha, 5 bits per RGB)
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
        return 32; // 32 bits per pixel (8 bits per channel)
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        return 24; // 24 bits per pixel (8 bits per channel, no alpha)
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
        return 16; // 16 bits per pixel for YUV 4:2:2
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
        return 12; // 12 bits per pixel for YUV 4:2:0
    default:
        return 0; // Unsupported format
    }
}

static int __add_property(int drm_fd, drmModeAtomicReq *req, uint32_t obj_id,
	uint32_t obj_type, const char *name, uint64_t value)
{
	drmModeObjectProperties *props =
		drmModeObjectGetProperties(drm_fd, obj_id, obj_type);
	if (!props)
	{
		fprintf(stderr, "Failed to get properties for object %u\n", obj_id);
		return -1;
	}

	uint32_t prop_id = 0;
	for (uint32_t i = 0; i < props->count_props; i++)
	{
		drmModePropertyRes *prop = drmModeGetProperty(drm_fd, props->props[i]);
		if (!prop){
			continue;
		}

		if (strcmp(prop->name, name) == 0){
			prop_id = prop->prop_id;
			drmModeFreeProperty(prop);
			break;
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);

	if (prop_id == 0){
		fprintf(stderr, "Property '%s' not found on object %u\n", name, obj_id);
		return -1;
	}

	if (drmModeAtomicAddProperty(req, obj_id, prop_id, value) < 0){
		fprintf(stderr, "Failed to add property '%s' on object %u: %s\n", name, obj_id, strerror(errno));
		return -1;
	}

	return 0;
}
static drmModeModeInfo *__get_valid_mode_from_connector(drmModeConnector* conn, int width, int height){
	drmModeModeInfo *mode = NULL;

	if(conn->connection != DRM_MODE_CONNECTED){
		printf("display connector type %d is not connected.\n", conn->connector_type);
		return mode;
	}else if(conn->count_modes <= 0){
		printf("display connector connector not found mode info.\n");
		return mode;
	}else{
		if((width == -1) || (height == -1)){
			return &conn->modes[0];;
		}
		for (int i = 0; i < conn->count_modes; i++){

			if((conn->modes[i].hdisplay == width) &&
				(conn->modes[i].vdisplay == height)){
				mode = &conn->modes[i];
				break;
			}
		}
		if(mode == NULL){
			printf("display connector not support resolution: %d*%d.\n",
				mode->hdisplay, mode->vdisplay);
			return mode;
		}
	}
	return mode;
}

int __create_and_mmap_drm_frame_buffer(int drm_fd, uint32_t format, uint32_t width, uint32_t height, drm_frame_buffer_info_t *fb_info){
    // 创建 dumb buffer
	struct drm_mode_create_dumb create_dumb = {0};
    create_dumb.width = width;
    create_dumb.height = height;
    create_dumb.bpp = get_bpp_from_format(format);

    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) < 0){
        perror("DRM_IOCTL_MODE_CREATE_DUMB");
        return -1;
    }

	uint32_t buf_id;
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {0};
    uint32_t offsets[4] = {0};
    handles[0] = create_dumb.handle;
    pitches[0] = create_dumb.pitch;
    offsets[0] = 0;
    if (drmModeAddFB2(drm_fd, width, height, format, handles, pitches, offsets, &buf_id, 0)){
        perror("drmModeAddFB2");
        return -1;
    }

    // 映射 dumb buffer
	struct drm_mode_map_dumb map_dumb = {0};
    map_dumb.handle = create_dumb.handle;
    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) < 0){
        perror("DRM_IOCTL_MODE_MAP_DUMB");
        return -1;
    }

    void *map = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map_dumb.offset);
    if (map == MAP_FAILED){
        perror("mmap");
        return -1;
    }
	memset(map, 0xFF, create_dumb.size);
	fb_info->dump_handle = create_dumb.handle;
	fb_info->frame_buffer_id = buf_id;
	fb_info->frame_buffer_vaddr = map;
	fb_info->frame_buffer_size = create_dumb.size;
    return 0;
}
void __destroy_and_unmmap_drm_frame_buffer(int drm_fd, drm_frame_buffer_info_t *fb_info){

	drmModeRmFB(drm_fd, fb_info->frame_buffer_id);

	munmap(fb_info->frame_buffer_vaddr, fb_info->frame_buffer_size);

	struct drm_mode_destroy_dumb destroy = {0};
	destroy.handle = fb_info->dump_handle;
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static int display_setup(display_context_t *display_context){
	int ret = -1;
	int connector_id = display_context->connector_id;
	int crtc_id = display_context->crtc_id;
	int drm_fd = display_context->drm_fd;
	param_config_t *param_config = &display_context->param_config;

	drmModeRes *resources = drmModeGetResources(drm_fd);
	if (!resources){
		perror("drmModeGetResources");
		return -1;
	}

	drmModeConnector* conn = NULL;
	for (int i = 0; i < resources->count_connectors; i++){
		conn = drmModeGetConnector(drm_fd, resources->connectors[i]);
		if (conn != NULL){
			if (conn->connector_type == display_context->connector_type){
				break;
			} else {
				drmModeFreeConnector(conn);
				conn = NULL; // Reset conn to NULL if it's not what we're looking for
			}
		}
	}
	if(conn == NULL){
		printf("not support %s, connector type is %d.\n", param_config->output, display_context->connector_type);
		goto free_res;
	}
	drmModeModeInfo* mode = __get_valid_mode_from_connector(conn, param_config->width, param_config->height);
	if(mode == NULL){
		goto free_conn;
	}
	display_context->width = mode->hdisplay;
	display_context->height = mode->vdisplay;

	printf("display select resolution :%d*%d .\n", display_context->width, display_context->height);
	if (drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) < 0){
		perror("drmSetClientCap DRM_CLIENT_CAP_ATOMIC");
		goto free_conn;
	}

	if (drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) < 0){
		perror("drmSetClientCap DRM_CLIENT_CAP_UNIVERSAL_PLANES");
		goto free_conn;
	}

	drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, crtc_id);
	if (!crtc){
		perror("drmModeGetCrtc");
		goto free_conn;
	}
	uint32_t blob_id;
	if (drmModeCreatePropertyBlob(drm_fd, mode, sizeof(*mode), &blob_id) < 0){
		perror("drmModeCreatePropertyBlob");
		goto free_crtc;
	}

	drmModeAtomicReq *req = drmModeAtomicAlloc();
	if (!req){
		perror("drmModeAtomicAlloc");
		goto free_crtc;
	}

	ret = __add_property(drm_fd, req, crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", 1);
	ret |= __add_property(drm_fd, req, crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", blob_id);
	ret |= __add_property(drm_fd, req, connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", crtc_id);
	if(ret != 0){
		goto free_req;
	}

	uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
	if (drmModeAtomicCommit(drm_fd, req, flags, NULL) < 0){
		perror("drmModeAtomicCommit");
		goto free_req;
	}
	ret = 0;

free_req:
	drmModeAtomicFree(req);
free_crtc:
	drmModeFreeCrtc(crtc);
free_conn:
	drmModeFreeConnector(conn);
free_res:
	drmModeFreeResources(resources);
	return ret;
}

static void fill_32bit_pixel_to_frame_buffer(void *fb_virtual_addr, int src_stride,
	int x, int y, int width, int height, uint32_t value){
	for (int i = 0; i < height; i++){
		uint32_t* start_addr = (uint32_t* )fb_virtual_addr + src_stride * (y + i) + x;
		for (int j = 0; j < width; j++){
			start_addr[j] = value;
		}
	}
}

int main(int argc, char** argv) {
	display_context_t display_context = {
		.crtc_id = 31,
		.plane_id = 40, //只有id = 40 的plane 支持翻转
	};

	param_config_t *param_config = &display_context.param_config;

	int ret = parser_params(argc, argv, param_config);
	if (ret != 0){
		return -1;
	}
	if(strcmp(param_config->output, "hdmi") == 0){
		display_context.connector_id = 75;
		display_context.connector_type = DRM_MODE_CONNECTOR_HDMIA;
	}else if(strcmp(param_config->output, "dsi") == 0){
		display_context.connector_id = 73;
		display_context.connector_type = DRM_MODE_CONNECTOR_DSI;
	}else{
		printf("not support display [%s]\n.", param_config->output);
	}
	//1. open drm device
	display_context.drm_fd = drmOpen("vs-drm", NULL);
	if (display_context.drm_fd < 0) {
		printf_insmod_driver_cmd();
		return -1;
	}

	//2. setup drm device
	ret = display_setup(&display_context);
	if(ret != 0){
		goto close_drm;
	}

	//3. clear the screen
	drm_frame_buffer_info_t drm_fb_info_for_clear;
	ret = __create_and_mmap_drm_frame_buffer(display_context.drm_fd, DRM_FORMAT_ARGB8888,
			display_context.width, display_context.height, &drm_fb_info_for_clear);
	if(ret != 0){
		goto close_drm;
	}
	{
		drmModeAtomicReq* req = drmModeAtomicAlloc();
		ret = __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID", display_context.crtc_id);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID", drm_fb_info_for_clear.frame_buffer_id);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W", display_context.width << 16);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H", display_context.height << 16);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W", display_context.width);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H", display_context.height);

		if(ret != 0){
			printf("__add_property failed\n");
			goto close_drm;
		}
		uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
		ret = drmModeAtomicCommit(display_context.drm_fd, req, flags, NULL);
		if(ret != 0){
			printf("drmModeAtomicCommit failed %d.\n", ret);
			goto close_drm;
		}
		drmModeAtomicFree(req);
	}
	__destroy_and_unmmap_drm_frame_buffer(display_context.drm_fd, &drm_fb_info_for_clear);

	//4. create and init rotation buffer
	int rotation_buffer_width = 480;
	int rotation_buffer_height = 480;
	drm_frame_buffer_info_t drm_fb_info;
	ret = __create_and_mmap_drm_frame_buffer(display_context.drm_fd, DRM_FORMAT_ARGB8888,
			rotation_buffer_width, rotation_buffer_height, &drm_fb_info);
	if(ret != 0){
		goto close_drm;
	}
	uint32_t *buffer_vaddr = (uint32_t *)drm_fb_info.frame_buffer_vaddr;
	int rect_width = 80;
	int rect_height = 80;
	int x_left = 16, x_right = rotation_buffer_width - rect_width - 16;
	int y_top = 16, y_bottom = rotation_buffer_height - rect_height - 16;
	fill_32bit_pixel_to_frame_buffer(buffer_vaddr, rotation_buffer_width,
		x_left, y_top, rect_width, rect_height, 0X00FF0000);												//左上角：填充红色
	fill_32bit_pixel_to_frame_buffer(buffer_vaddr, rotation_buffer_width,
		x_right, y_top, rect_width, rect_height, 0X0000FF00); 												//右上角：填充绿色
	fill_32bit_pixel_to_frame_buffer(buffer_vaddr, rotation_buffer_width,
		x_left, y_bottom, rect_width, rect_height, 0X000000FF);												//左下角：填充蓝色
	fill_32bit_pixel_to_frame_buffer(buffer_vaddr, rotation_buffer_width,
		x_right, y_bottom,
		rect_width, rect_height, 0X00F0F000);																//右小角：填充黄色

	int rotation_table_index = 0;
	int rotation_table[4] = {DRM_MODE_ROTATE_0, DRM_MODE_ROTATE_90, DRM_MODE_ROTATE_180, DRM_MODE_ROTATE_270};

	int center_x = display_context.width / 2 -  rotation_buffer_width / 2;
	int center_y = display_context.height / 2 -  rotation_buffer_height / 2;

{
		drmModeAtomicReq* req = drmModeAtomicAlloc();

		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W", rotation_buffer_width << 16);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H", rotation_buffer_height << 16);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X", center_x);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y", center_y);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W", rotation_buffer_width);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H", rotation_buffer_height);

		if(ret != 0){
			printf("__add_property failed\n");
			goto close_drm;
		}
		uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
		ret = drmModeAtomicCommit(display_context.drm_fd, req, flags, NULL);
		if(ret != 0){
			printf("drmModeAtomicCommit failed %d.\n", ret);
			goto close_drm;
		}
		drmModeAtomicFree(req);
}

	while(1){
		//5. 旋转
		int rotation = rotation_table[rotation_table_index];
		drmModeAtomicReq* req = drmModeAtomicAlloc();
		ret = __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID", display_context.crtc_id);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID", drm_fb_info.frame_buffer_id);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "rotation", rotation);

		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y", 0);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W", rotation_buffer_width << 16);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H", rotation_buffer_height << 16);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X", center_x);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y", center_y);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W", rotation_buffer_width);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H", rotation_buffer_height);
		// only flush little square

		if(ret != 0){
			printf("__add_property failed\n");
			break;
		}

		uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
		ret = drmModeAtomicCommit(display_context.drm_fd, req, flags, NULL);
		if(ret != 0){
			printf("drmModeAtomicCommit failed %d.\n", ret);
			break;
		}
		drmModeAtomicFree(req);

		rotation_table_index = (rotation_table_index + 1) % 4;
		usleep(3000 * 1000);
	}
	__destroy_and_unmmap_drm_frame_buffer(display_context.drm_fd, &drm_fb_info);

close_drm:
	close(display_context.drm_fd);
	return 0;
}
