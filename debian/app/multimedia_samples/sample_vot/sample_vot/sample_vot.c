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
#define RESOURCE_FILE_WIDTH 1920
#define RESOURCE_FILE_HEIGHT 1080
#define RESOURCE_FILE_PATH "../resource/nv12_1920x1080.yuv"
typedef struct param_config_s{
	int width;			//connector's width
	int height;			//connector's height

	char *output;		//hdmi, dsi
	int input; 			// 0: memory buffer 1:file

	int list_connector_rosulotions;
}param_config_t;

typedef struct display_context_s
{
	int width;
	int height;

	int drm_fd;

	uint32_t crtc_id;
	uint32_t plane_id;
	uint32_t connector_id;

	drmModeAtomicReq *req;

	const char *input_file;
	const int input_file_width;
	const int input_file_height;

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
	printf("  -o <output>                       Specify display(hdmi, dsi(current is not support))\n");
	printf("  -c <image_width>                  Specify display width(column)\n");
	printf("  -r <image_height>                 Specify display height(row)\n");
	printf("  -l <list_connector_rosulotions>   Specify function is only list display's resulotions\n");
	printf("  -f <file>                         Specify input data is file (default: memory data(R,G,B)\n");
	printf("  -h <help>              Show this help message\n");
	printf("     For Example, list display's resulotions: ./%s -l\n", test_case);
	printf("     For Example, use file as input         : ./%s -f\n", test_case);
	printf("     For Example, use memory data as input  : ./%s \n", test_case);
}

int parser_params(int argc, char **argv, param_config_t *param)
{
	struct option const long_options[] = {

		{"output", optional_argument, NULL, 'o'},
		{"width", optional_argument, NULL, 'c'},
		{"height", optional_argument, NULL, 'r'},
		{"list_connector_rosulotions", optional_argument, NULL, 'l'},
		{"file", optional_argument, NULL, 'f'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, NULL, 0}};

	int opt_index = 0;
	int c = 0;

	param->width = -1;
	param->height = -1;
	param->output = "hdmi";
	param->input = 0;
	param->list_connector_rosulotions = 0;

	while ((c = getopt_long(argc, argv, "o:c:r:lfh",
							long_options, &opt_index)) != -1){
		switch (c)
		{
		case 'o':
			param->output = optarg;
			break;
		case 'c':
			param->width = atoi(optarg);
			break;
		case 'r':
			param->height = atoi(optarg);
			break;
		case 'f':
			param->input = 1;
			break;
		case 'l':
			param->list_connector_rosulotions = 1;
			break;
		case 'h':
		default:
			print_help("vot_simple");
			return -1;
		}
	}
	if(strcmp(param->output, "hdmi") != 0){
		printf("display connector current only support hdmi, but input param is [%s].\n", param->output);
		return -1;
	}

	if(param->input == 1){
		if((param->width != -1) || ((param->height != -1))){
			printf("\n !!! file mode force display resolution is %d*%d (input file's resolution).\n",
				RESOURCE_FILE_WIDTH, RESOURCE_FILE_HEIGHT);
		}
		param->width = RESOURCE_FILE_WIDTH;
		param->height = RESOURCE_FILE_HEIGHT;
	}
	printf("\nPrint param Config:\n");
	if(param->list_connector_rosulotions == 1){
		printf("\tFunction : print connector support resolution.\n");
	}else {
		printf("\tFunction : display.\n");
		if(param->input == 1){
			printf("\tInput    : file(%s).\n", RESOURCE_FILE_PATH);
		}else{
			printf("\tInput    : default data (R, G, B).\n");
		}
		printf("\tOutput   :\n");
		if((param->width == -1) || ((param->height == -1))){
			printf("\t\t Resolution: select connector's first config from EDID\n");
		}else{
			printf("\t\t Resolution: %d*%d\n", param->width, param->height);
		}
		printf("\t\t Connector: %s\n", param->output);
	}

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
		printf("display connector is not connected.\n");
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
static float __mode_vrefresh(drmModeModeInfo *mode)
{
	unsigned int num, den;

	num = mode->clock;
	den = mode->htotal * mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		num *= 2;
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		den *= 2;
	if (mode->vscan > 1)
		den *= mode->vscan;

	return num * 1000.00 / den;
}

static int list_connector_support_resolution(display_context_t *display_context){
	int ret = 0;
	param_config_t *param_config = &display_context->param_config;

	drmModeRes *resources = drmModeGetResources(display_context->drm_fd);
	if (!resources){
		printf("drmModeGetResources failed.\n");
		return -1;
	}

	drmModeConnector* conn = NULL;
	for (int i = 0; i < resources->count_connectors; i++){
		conn = drmModeGetConnector(display_context->drm_fd, resources->connectors[i]);
		if (conn != NULL){
			if (conn->connector_type == DRM_MODE_CONNECTOR_HDMIA){
				break;
			} else {
				drmModeFreeConnector(conn);
				conn = NULL; // Reset conn to NULL if it's not what we're looking for
			}
		}
	}
	if(conn == NULL){
		printf("not support hdmi.\n");
		goto free_res;
	}

	if(conn->connection != DRM_MODE_CONNECTED){
		ret = -2;
		printf("display connector is not connected.\n");
		goto free_conn;
	}else if(conn->count_modes <= 0){
		ret = -3;
		printf("display connector not found mode info.\n");
		goto free_conn;
	}else{
		drmModeModeInfo *mode = NULL;
		printf("Print [%s] connector support resolution:\n", param_config->output);
		for (int i = 0; i < conn->count_modes; i++){
			mode = &conn->modes[i];
			printf("  [%d] %s %.2ffps\n", i, mode->name, __mode_vrefresh(mode));
		}
	}


free_conn:
	drmModeFreeConnector(conn);
free_res:
	drmModeFreeResources(resources);
	return ret;
}
int __wraper_dma_buffer_to_drm_frame_buffer(int drm_fd, uint32_t format, uint32_t width, uint32_t height, int dma_buf_fd,
	 drm_frame_buffer_info_t *fb_info){

	if(DRM_FORMAT_NV12 != format){
		printf("current only support nv12, but input format param is %d.\n", format);
		return -1;
	}
	struct drm_prime_handle prime_handle = {
		.fd = dma_buf_fd,
		.flags = 0,
		.handle = 0,
	};
	if (drmIoctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle) < 0){
		perror("DRM_IOCTL_PRIME_FD_TO_HANDLE");
		return -1;
	}

	uint32_t handles[4] = {0};
	uint32_t strides[4] = {0};
	uint32_t offsets[4] = {0};
	handles[0] = prime_handle.handle;
	strides[0] = width;
	offsets[0] = 0;

	handles[1] = prime_handle.handle;
	strides[1] = width;
	offsets[1] = width * height;

	uint32_t buf_id;
    if (drmModeAddFB2(drm_fd, width, height, format, handles, strides, offsets, &buf_id, 0)){
        perror("drmModeAddFB2");
        return -1;
    }
	fb_info->dump_handle = prime_handle.handle;
	fb_info->frame_buffer_id = buf_id;
	fb_info->frame_buffer_vaddr = NULL;
	fb_info->frame_buffer_size = -1;

	return 0;
}
void __unwraper_dma_buffer_to_drm_frame_buffer(int drm_fd, drm_frame_buffer_info_t *fb_info){
	drmModeRmFB(drm_fd, fb_info->frame_buffer_id);
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
			if (conn->connector_type == DRM_MODE_CONNECTOR_HDMIA){
				break;
			} else {
				drmModeFreeConnector(conn);
				conn = NULL; // Reset conn to NULL if it's not what we're looking for
			}
		}
	}
	if(conn == NULL){
		printf("not support hdmi.\n");
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
int main(int argc, char** argv) {
	display_context_t display_context = {
		.input_file = RESOURCE_FILE_PATH,
		.input_file_width = RESOURCE_FILE_WIDTH,
		.input_file_height = RESOURCE_FILE_HEIGHT,
	};

	param_config_t *param_config = &display_context.param_config;

	int ret = parser_params(argc, argv, param_config);
	if (ret != 0){
		return -1;
	}
	if(strcmp(param_config->output, "hdmi") == 0){
		display_context.connector_id = 75;
	}else{
		printf("not support display [%s]\n.", param_config->output);
	}
	display_context.crtc_id = 31;
	display_context.plane_id = 33;

	display_context.drm_fd = drmOpen("vs-drm", NULL);
	if (display_context.drm_fd < 0) {
		printf_insmod_driver_cmd();
		return -1;
	}

	if(param_config->list_connector_rosulotions == 1){
		list_connector_support_resolution(&display_context);
		goto close_drm;
	}

	ret = display_setup(&display_context);
	if(ret != 0){
		goto close_drm;
	}
	if(param_config->input == 0){
		uint32_t colors[3] = {0X00FF0000 /*red*/, 0X0000FF00 /*green*/, 0X000000FF /*blue*/};
		drm_frame_buffer_info_t drm_fb_info[3];
		for(int i = 0; i < 3; i++){
			ret = __create_and_mmap_drm_frame_buffer(display_context.drm_fd, DRM_FORMAT_ARGB8888,
				display_context.width, display_context.height, &drm_fb_info[i]);
			if(ret != 0){
				goto close_drm;
			}
		}

		for(int i = 0; i < 3; i++){
			uint32_t color = colors[i];
			uint32_t *buffer_vaddr = (uint32_t *)drm_fb_info[i].frame_buffer_vaddr;
			for(int j = 0; j < drm_fb_info->frame_buffer_size / 4; j++){
				buffer_vaddr[j] = color;
			}
		}
		int drm_fb_info_index = 0;

		while(1){
#if 1 //atomic 版本的接口
			drmModeAtomicReq *req = drmModeAtomicAlloc();
			ret = __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID", display_context.crtc_id);
			ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID", drm_fb_info[drm_fb_info_index].frame_buffer_id);

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
				break;
			}
			uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
			ret = drmModeAtomicCommit(display_context.drm_fd, req, flags, NULL);
			if(ret != 0){
				printf("drmModeAtomicCommit failed %d.\n", ret);
				break;
			}
			drmModeAtomicFree(req);
#else // legacy 版本的接口
			drmModeSetPlane(display_context.drm_fd, display_context.plane_id, display_context.crtc_id,
				drm_fb_info[drm_fb_info_index].frame_buffer_id, 0,
				0, 0, display_context.width, display_context.height,
				0 << 16, 0 << 16, display_context.width << 16, display_context.height << 16);
#endif
			sleep(2);
			drm_fb_info_index = (drm_fb_info_index + 1) % 3;
		}

		for(int i = 0; i< 3; i++){
			__destroy_and_unmmap_drm_frame_buffer(display_context.drm_fd, &drm_fb_info[i]);
		}

	}else{
		hb_mem_module_open();
		hb_mem_graphic_buf_t hb_mem_graphic_buf;
		int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
		ret = hb_mem_alloc_graph_buf(display_context.input_file_width, display_context.input_file_height,
			MEM_PIX_FMT_NV12, flags, 0, 0, &hb_mem_graphic_buf);
		if(ret != 0){
			printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
			goto close_mem_module;
		}
		ret = read_nv12_image_to_graphic_buffer(display_context.input_file, &hb_mem_graphic_buf,
			display_context.input_file_width, display_context.input_file_height);
		if(ret != 0){
			printf("read_nv12_image_to_graphic_buffer failed :%d\n", ret);
			goto free_hb_mem;
		}
		drm_frame_buffer_info_t fb_info;
		ret = __wraper_dma_buffer_to_drm_frame_buffer(display_context.drm_fd, DRM_FORMAT_NV12,
			display_context.width, display_context.height, hb_mem_graphic_buf.fd[0], &fb_info);
		if(ret != 0){
			printf("__wraper_dma_buffer_to_drm_frame_buffer failed :%d\n", ret);
			goto free_hb_mem;
		}
		drmModeAtomicReq *req = drmModeAtomicAlloc();
		if(req == NULL){
			printf("drmModeAtomicAlloc failed.\n");
			goto unraper_dma_buffer;
		}

		ret = __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID", display_context.crtc_id);
		ret |= __add_property(display_context.drm_fd, req, display_context.plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID", fb_info.frame_buffer_id);

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
			drmModeAtomicFree(req);
			goto unraper_dma_buffer;
		}
		flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
		ret = drmModeAtomicCommit(display_context.drm_fd, req, flags, NULL);
		if(ret != 0){
			printf("drmModeAtomicCommit failed %d.\n", ret);
			drmModeAtomicFree(req);
			goto unraper_dma_buffer;
		}
		drmModeAtomicFree(req);

		while(1){
			sleep(1);
		}
unraper_dma_buffer:
		__unwraper_dma_buffer_to_drm_frame_buffer(display_context.drm_fd, &fb_info);
free_hb_mem:
		hb_mem_free_buf(hb_mem_graphic_buf.fd[0]);
close_mem_module:
		hb_mem_module_close();
	}

close_drm:
	close(display_context.drm_fd);
	return 0;
}
