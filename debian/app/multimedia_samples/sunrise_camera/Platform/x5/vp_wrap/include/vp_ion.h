/***************************************************************************
 * @COPYRIGHT NOTICE
 * @Copyright 2024 D-Robotics, Inc.
 * @All rights reserved.
 * @Date: 2023-01-30 11:27:41
 * @LastEditTime: 2023-03-05 15:57:35
 ***************************************************************************/
#ifndef VP_ION_H_
#define VP_ION_H_
#include <stdint.h>
#define PIPELINE_MAX_COUNT (32)
typedef enum {
	ION_RESERVED = 0,
	ION_CARVEOUT,
	ION_CMA,
	ION_SENTRY,
}ion_type_t;
typedef struct {
	int heap_id;
	int64_t total;
	int64_t used;
	int64_t used_orphaned;
	char *name;
	ion_type_t type;

}vp_ion_heap_info_t;

typedef struct {
	int is_inited;
	vp_ion_heap_info_t ion_infos[ION_SENTRY];
}vp_ion_all_heap_info_t;

void vp_ion_all_heap_printf(vp_ion_all_heap_info_t *vp_ion_all_info);
int vp_ion_heap_get_current_status(vp_ion_all_heap_info_t *vp_ion_all_info);

//994-0  display-0  galcore-0  jpu-0  osd_driver_ion-0  vio_driver_ion-0  vpu-0  vsi_cam_drv_ion-0
typedef enum {
	ION_CLIENT_DISPLAY = 0, //显示
	ION_CLIENT_GPU_3D,
	ION_CLIENT_JPU,
	ION_CLIENT_OSD,
	ION_CLIENT_VFLOW,
	ION_CLIENT_VPU,
	ION_CLIENT_CAM_SERVICE,
	ION_CLIENT_SENTRY,
}ion_client_type_t;
typedef struct {
	int64_t total_used;
	char *name;
	ion_client_type_t type;
}vp_ion_client_info_t;

typedef struct {
	int is_inited;
	vp_ion_client_info_t ion_infos[ION_CLIENT_SENTRY];
}vp_ion_all_client_info_t;

void vp_ion_all_client_printf(vp_ion_all_client_info_t *vp_ion_all_info);
int vp_ion_client_get_current_status(vp_ion_all_client_info_t *vp_ion_all_client_info);

typedef enum {
	ION_KEY_BPU,
	ION_KEY_SENTRY,
}ion_key_type_t;

typedef struct {
	char *name;
	int64_t total_used;
	ion_key_type_t type;
}vp_ion_info_by_key_t;

typedef struct {
	int is_inited;
	vp_ion_info_by_key_t ion_infos[ION_KEY_SENTRY];
}vp_ion_all_info_by_key_t;
void vp_ion_all_key_info_printf(vp_ion_all_info_by_key_t *vp_ion_all_info);
int vp_ion_all_key_get_current_status(vp_ion_all_info_by_key_t *vp_ion_all_client_info);
int vp_ion_get_remain_size(vp_ion_all_heap_info_t *vp_ion_all_info);

typedef struct {
	vp_ion_all_info_by_key_t key;
	vp_ion_all_heap_info_t heap;
	vp_ion_all_client_info_t client;
}vp_ion_all_info_t;
int vp_ion_get_current_status(vp_ion_all_info_t *info);

typedef enum {
	/*	frame  */
	ION_BUFFER_RAW,
	ION_BUFFER_RAW10,
	ION_BUFFER_RAW12,
	ION_BUFFER_NV12,
	ION_BUFFER_SENTRY,

	/*	osd buffer  */
	ION_OSD_BUFFER_VGA8,
	ION_OSD_BUFFER_NV12,
	ION_OSD_BUFFER_SW_VGA4,
	ION_OSD_BUFFER_POLYGON,
}vp_ion_buffer_format_t;
typedef struct {
	int count;
	int width;
	int height;
	vp_ion_buffer_format_t format;
}vp_ion_buffer_param_t;

//for camera service
typedef struct {
	int is_mcm_mode;
	int is_enable_3dnr;
	int is_enable_isp;
	int is_enable_vse;

	int width;
	int height;
	vp_ion_buffer_format_t format;
}vp_ion_camera_service_param_t;

typedef struct{
	int is_used_isp;
	int is_used_vse;
}vp_ion_camera_service_extern_param_t;

//for vpu
typedef enum{
	ION_H264_ENCODEC,
	ION_H265_ENCODEC,
	ION_H264_DECODEC,
	ION_H265_DECODEC,
	ION_CODEC_TYPE_SENTRY,
}vp_ion_codec_type_t;
typedef struct {
	vp_ion_codec_type_t type;

	int width;
	int height;

	int input_buffer_count;
	int output_buffer_count;

}vp_ion_vpu_param_t;

//for bpu
#define BPU_MAX_DIMENSION 32
typedef struct {
	//int model_file_size;    // 在固定内存中包含

	// int heap_region_size;  // 在固定内存中包含
	int ouput_calculator_size_dynamic; //yolov5 模式：根据运行路数增加

	int input_queue_count;
	int input_width;
	int input_height;

	int output_queue_count;
	int output_dimensions;
	int output_size[BPU_MAX_DIMENSION];
}vp_ion_bpu_param_t;
//
#define BPU_MAX_MODEL_FILE 32
#define BPU_MODEL_FILE_NAME_MAX 32
#if 1
typedef struct {
	char model_name[BPU_MODEL_FILE_NAME_MAX];
	int heap_region_size;				// 有的模型需要，有的不需要
	int ouput_calculator_size_static; 	//fcos 模式：不会根据运行路数增加

	int model_file_sizes;
}vp_ion_bpu_extern_single_param_t;
#endif
//遍历所有的通道，计算得到
typedef struct {
	int is_used_bpu;				  // 只要使用BPU 就会产生的占用，并且无论多少模型，只占用1份
#if 0
	int heap_region_size;			  // 有的模型需要，有的不需要
	int ouput_calculator_size_static; //fcos 模式：不会根据运行路数增加

	int model_file_count;
	int model_file_sizes[BPU_MAX_MODEL_FILE];
#else
	int item_count;
	vp_ion_bpu_extern_single_param_t item_param[PIPELINE_MAX_COUNT];
#endif
}vp_ion_bpu_extern_param_t;

#define VSE_MAX_CHANNLE 6
#define OSD_MAX_CHANNLE 6
typedef struct {
	//vpf
	vp_ion_buffer_param_t vin;
	vp_ion_buffer_param_t isp;

	int is_enable_gdc;
	vp_ion_buffer_param_t gdc;
	int gdc_bin_file_size;

	int vse_valid_count;
	vp_ion_buffer_param_t vse[VSE_MAX_CHANNLE];

	//osd
	int osd_valid_count;
	vp_ion_buffer_param_t osd[OSD_MAX_CHANNLE];

	//camera service
	vp_ion_camera_service_param_t camera_service;

	//vpu
	vp_ion_vpu_param_t vpu;

	//bpu
	vp_ion_bpu_param_t bpu;
}vp_ion_pipeline_param_t;

/*
 	1. osd_driver_ion-0
 	2. vpu-0
	3. vio_driver_ion-0
	4. vsi_cam_drv_ion-0
	5. bpu
*/
typedef struct {
	int64_t osd_size;
	int64_t vpu_size;
	int64_t bpu_size;
	int64_t vflow_size;
	int64_t camera_service_size;
}vp_ion_theory_calc_result_t;
int vp_ion_pipeline_calculator(vp_ion_pipeline_param_t *vp_ion_pipeline_param, vp_ion_theory_calc_result_t *result);

typedef struct {
	vp_ion_bpu_extern_param_t bpu;
	vp_ion_camera_service_extern_param_t camera_service;
}vp_ion_pipeline_fixed_param_t;

int vp_ion_pipeline_fixed_calculator(vp_ion_pipeline_fixed_param_t *param, vp_ion_theory_calc_result_t* result);

void vp_ion_pipeline_theory_result_printf(vp_ion_theory_calc_result_t *vp_ion_theory_calc_result);


int vp_ion_check_theory_result(vp_ion_all_info_t *before_ion_info, vp_ion_theory_calc_result_t *theory_result);

//返回值：缺少的ION内存
int64_t vp_ion_check_is_enough(vp_ion_all_info_t *before_ion_info, vp_ion_theory_calc_result_t *theory_result);
#endif /* extern "C" */
