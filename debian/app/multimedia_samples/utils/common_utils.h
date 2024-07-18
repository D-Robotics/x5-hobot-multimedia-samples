/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef __COMMMON_UTILS__
#define __COMMMON_UTILS__

#include "hb_camera_interface.h"
#include "hbn_api.h"
#include "vp_sensors.h"
#include "vse_cfg.h"
#include "codec_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#define ERR_CON_EQ(ret, a) do {\
		if ((ret) != (a)) {\
			printf("%s(%d) failed, ret %d\n", __func__, __LINE__, (int32_t)(ret));\
			return (ret);\
		}\
	} while(0)\

#define ERR_CON_NE(ret, a) do {\
		if ((ret) == (a)) {\
			printf("%s(%d) failed, ret %ld\n", __func__, __LINE__, (ret));\
			return (ret);\
		}\
	} while(0)\

#define VSE_MAX_CHANNELS 6

typedef struct pipe_contex_s {
	hbn_vflow_handle_t vflow_fd;
	hbn_vnode_handle_t vin_node_handle;
	hbn_vnode_handle_t isp_node_handle;
	hbn_vnode_handle_t vse_node_handle;
	hbn_vnode_handle_t gdc_node_handle;
	hbn_vnode_handle_t vpu_node_handle;
	hbn_vnode_handle_t codec_node_handle;
	camera_handle_t cam_fd;
	vp_sensor_config_t *sensor_config;
	vp_csi_config_t csi_config;
} pipe_contex_t;

int32_t read_yuv420_file(const char *filename, char *addr0, char *addr1, uint32_t y_size);
int32_t read_yuvv_nv12_file(const char *filename, char *addr0, char *addr1, uint32_t y_size);
int32_t dump_image_to_file(char *filename, uint8_t *src_buffer, uint32_t size);
int32_t dump_2plane_yuv_to_file(char *filename, uint8_t *src_buffer, uint8_t *src_buffer1,
		uint32_t size, uint32_t size1);
int32_t vpm_hb_mem_init(void);
void vpm_hb_mem_deinit(void);
int32_t alloc_graphic_buffer(hbn_vnode_image_t *img, uint32_t width,
			     uint32_t height, uint32_t cached, int32_t format);
uint64_t vio_test_gettime_us(void);
uint32_t load_file_2_buff_nosize(const char *path, char *filebuff);
char* get_program_name();
void configure_vse_max_resolution(int32_t channel, uint32_t input_width, uint32_t input_height,
	uint32_t *output_width, uint32_t *output_height);

#ifdef __cplusplus
	}
#endif	/* __cplusplus */

#endif

