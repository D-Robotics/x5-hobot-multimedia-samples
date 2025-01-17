#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "vp_ion.h"
#include "utils/utils_log.h"

#define ION_HEAPS_INFO_PATH "/sys/kernel/debug/ion/heaps"
#define ION_HEAPS_ALL_INFO_PATH "/sys/kernel/debug/ion/heaps/all_heap_info"
#define ION_HEAPS_CLIENT_INFO_PATH "/sys/kernel/debug/ion/clients"

static char* g_ion_heap_name[ION_SENTRY] = {
	"cma_reserved",
	"carveout",
	"ion_cma"
};
static char* g_ion_client_name[] = {
	"display-0",
	"galcore-0",
	"jpu-0",
	"osd_driver_ion-0",
	"vio_driver_ion-0",
	"vpu-0",
	"vsi_cam_drv_ion-0",
};
static char* g_ion_key_name[] = {
	"bpu",
};

#define ALIGN_4(size) (((size) + 0x3) & ~0x3)
#define ALIGN_32(size) (((size) + 0x1F) & ~0x1F)
#define ALIGN_64(size) (((size) + 0x3F) & ~0x3F)
#define ALIGN_256(size) (((size) + 0xFF) & ~0xFF)
#define ALIGN_4K(size) (((size) + 0xFFF) & ~0xFFF)

// sunrise_camera             7485              bpu           229376 0
static int vp_parse_ion_heap_info_by_key(vp_ion_info_by_key_t *vp_ion_single_info, const char *key_name){

	FILE *file = fopen(ION_HEAPS_ALL_INFO_PATH, "r");
	if (!file) {
		SC_LOGE("Error opening file %s.", ION_HEAPS_ALL_INFO_PATH);
		return -1;
	}

	char line[256];
	while(fgets(line, sizeof(line), file)){
		// printf("parse key[%s] from line: %s\n", key_name, line);
		int client_id = 0;
		int flag = 0;
		int bpu_size = 0;
		if(strstr(line, key_name) != NULL){
			sscanf(line, "%*s %d %*s %d %d", &client_id, &bpu_size, &flag);
			vp_ion_single_info->total_used += bpu_size;
			// printf("bpu size:%d, client_id:%d flag:%d total:%d\n", bpu_size, client_id, flag, vp_ion_single_info->total_used);
		}
	}
	fclose(file);

	return 0;
}

int vp_ion_all_key_get_current_status(vp_ion_all_info_by_key_t *vp_ion_all_info){
	int ret = 0;
	memset(vp_ion_all_info, 0, sizeof(vp_ion_all_info_by_key_t));
	for(int i = 0; i < ION_KEY_SENTRY; i++){
		vp_ion_all_info->ion_infos[i].name = g_ion_key_name[i];
		vp_ion_all_info->ion_infos[i].type = (ion_type_t)i;
		ret = vp_parse_ion_heap_info_by_key(&vp_ion_all_info->ion_infos[i], g_ion_key_name[i]);
		if(ret != 0){

			printf("[%s] process [%s] failed.\n", ION_HEAPS_ALL_INFO_PATH, g_ion_key_name[i]);
			return -1;
		}
	}
	vp_ion_all_info->is_inited = 1;
	return 0;
}
void vp_ion_all_key_info_printf(vp_ion_all_info_by_key_t *vp_ion_all_info){
	printf("\n\n");
	SC_LOGI("show all key ion info:");
	for(int i = 0; i < ION_KEY_SENTRY; i++){
		vp_ion_info_by_key_t *info = &vp_ion_all_info->ion_infos[i];
		printf("[%s]:\n", info->name);
		printf("\t total_used: %ld\n", info->total_used);
	}
	printf("\n\n");
}

/**
 * 	解析如下内容:
	-------------------------------------------------------------------------
	the heap id is 4
	[...]
			ion_cma  heap total size        201326592
	[...]
	total orphaned                0
			total                 0
	-------------------------------------------------------------------------
 */
static int vp_parse_ion_heap_info(vp_ion_heap_info_t *vp_ion_single_info, const char *file_path){
	typedef enum{
		HEAP_ID = 0,
		TOTAL_SIZE,
		USED_ORPHANED,
		USED,
		STEP_SENTRY
	}ion_parse_step_t;

	int ret = -1;
	FILE *file = fopen(file_path, "r");
	if (!file) {
		SC_LOGE("Error opening file %s.", file_path);
		return -1;
	}

	char line[256];
	char parse_tmp[256];
	ion_parse_step_t step = HEAP_ID;
	while(fgets(line, sizeof(line), file)){
		// printf("step %d, line: %s\n", step, line);
		switch (step){
			case HEAP_ID:{
				const char *format_str = "the heap id is ";
				if(strstr(line, format_str) != NULL){
					sscanf(line, "the heap id is %d", &vp_ion_single_info->heap_id);
					step = TOTAL_SIZE;
				}
				break;
			}
			case TOTAL_SIZE:{
				const char *format_str = "heap total size ";
				if(strstr(line, format_str) != NULL){
					sscanf(line, "%s %s %s %s %ld", parse_tmp, parse_tmp, parse_tmp, parse_tmp, &vp_ion_single_info->total);
					step = USED_ORPHANED;
				}
				break;
			}
			case USED_ORPHANED:{
				const char *format_str = "total orphaned ";
				if(strstr(line, format_str) != NULL){
					sscanf(line, "%s %ld", parse_tmp, &vp_ion_single_info->used_orphaned);
					step = USED;
				}
				break;
			}

			case USED:{
				const char *format_str = "total ";
				if(strstr(line, format_str) != NULL){
					sscanf(line, "%s %ld", parse_tmp, &vp_ion_single_info->used);
					step = STEP_SENTRY;
				}
				break;
			}
			default:
				SC_LOGE("unsupport step %d.", step);
				break;
		}
		if(step == STEP_SENTRY){
			ret = 0;
			SC_LOGD("[%s] parse ok.", file_path);
			break;
		}
	}
	fclose(file);

	if(ret != 0){
		printf("ion file parser failed, step is %d.\n", step);
	}
	return ret;
}

int vp_ion_heap_get_current_status(vp_ion_all_heap_info_t *vp_ion_all_info){
	int ret = 0;
	char ion_sys_node_file_path[200];
	memset(vp_ion_all_info, 0, sizeof(vp_ion_all_heap_info_t));

	for(int i = 0; i < ION_SENTRY; i++){
		snprintf(ion_sys_node_file_path, sizeof(ion_sys_node_file_path),
			"%s/%s", ION_HEAPS_INFO_PATH, g_ion_heap_name[i]);
		vp_ion_all_info->ion_infos[i].name = g_ion_heap_name[i];
		vp_ion_all_info->ion_infos[i].type = (ion_type_t)i;
		ret = vp_parse_ion_heap_info(&vp_ion_all_info->ion_infos[i], ion_sys_node_file_path);
		if(ret != 0){

			printf("[%s] process failed.\n", ion_sys_node_file_path);
			return -1;
		}
	}
	vp_ion_all_info->is_inited = 1;
	return 0;
}
int vp_ion_get_remain_size(vp_ion_all_heap_info_t *vp_ion_all_info){
	int remian_size = 0;
	for(int i = 0; i< ION_SENTRY; i++){
		remian_size += vp_ion_all_info->ion_infos[i].total - vp_ion_all_info->ion_infos[i].used;
	}
	return remian_size;
}

void vp_ion_all_heap_printf(vp_ion_all_heap_info_t *vp_ion_all_info){
	printf("\n\n");
	SC_LOGI("show all heap ion info:");
	for(int i = 0; i < ION_SENTRY; i++){
		vp_ion_heap_info_t *info = &vp_ion_all_info->ion_infos[i];
		printf("[%s]:\n", info->name);
		printf("\t head id: %d\n", info->heap_id);
		printf("\t total: %ld\n", info->total);
		printf("\t used: %ld\n", info->used);
		printf("\t used orphanned: %ld\n", info->used_orphaned);
	}
	printf("\n\n");
}

/**
       heap_name:    size_in_bytes :  handle refcount :    handle import :       buffer ptr :  buffer refcount :  buffer share id : buffer share count
        carveout:            10000 :                1 :                1 :         60564ad6 :                2:               63 :                1

        carveout:            10000 :                1 :                1 :         b7d0e7f3 :                2:               64 :                1

-------------------------------------------------------------------------
          total             20000
-------------------------------------------------------------------------
 */
static int vp_parse_ion_client_info(vp_ion_client_info_t *vp_ion_client_info, const char *file_path){

	typedef enum{
		USED,
		STEP_SENTRY
	}ion_parse_step_t;

	int ret = -1;
	FILE *file = fopen(file_path, "r");
	if (!file) {
		SC_LOGE("Error opening file %s.", file_path);
		return -1;
	}

	char line[256];
	char parse_tmp[256];
	ion_parse_step_t step = USED;
	while(fgets(line, sizeof(line), file)){
		// printf("step %d, line: %s\n", step, line);
		switch (step){
			case USED:{
				const char *format_str = "total ";
				if(strstr(line, format_str) != NULL){
					sscanf(line, "%s %lx", parse_tmp, &vp_ion_client_info->total_used);
					step = STEP_SENTRY;
				}
				break;
			}
			default:
				SC_LOGE("unsupport step %d.", step);
				break;
		}
		if(step == STEP_SENTRY){
			ret = 0;
			// SC_LOGI("[%s] parse ok.", file_path);
			break;
		}
	}
	fclose(file);

	return ret;
}
int vp_ion_client_get_current_status(vp_ion_all_client_info_t *vp_ion_all_client_info){
	int ret = 0;
	char tmp_path[200];
	memset(vp_ion_all_client_info, 0, sizeof(vp_ion_all_client_info_t));
	for(int i = 0; i < ION_CLIENT_SENTRY; i++){
		snprintf(tmp_path, sizeof(tmp_path),
			"%s/%s", ION_HEAPS_CLIENT_INFO_PATH, g_ion_client_name[i]);
		vp_ion_all_client_info->ion_infos[i].name = g_ion_client_name[i];
		vp_ion_all_client_info->ion_infos[i].type = (ion_client_type_t)i;
		ret = vp_parse_ion_client_info(&vp_ion_all_client_info->ion_infos[i], tmp_path);
		if(ret != 0){
			SC_LOGE("parse client [%s] failed.",  g_ion_client_name[i]);
			return -1;
		}
	}
	vp_ion_all_client_info->is_inited = 1;
	return 0;
}

void vp_ion_all_client_printf(vp_ion_all_client_info_t *vp_ion_all_info){
	printf("\n\n");
	SC_LOGI("show all client ion info:");
	for(int i = 0; i < ION_CLIENT_SENTRY; i++){
		vp_ion_client_info_t *info = &vp_ion_all_info->ion_infos[i];
		printf("[%s]:\n", info->name);
		printf("\t total used: 0x%lx (%ld) \n", info->total_used, info->total_used);
	}
	printf("\n\n");
}

static int vp_ion_camera_service_calculator(vp_ion_camera_service_param_t *param){
	int	total_size = param->width * param->height * 2;

	int mcm_buf_size = 0;
	if(param->is_mcm_mode){
		mcm_buf_size = ALIGN_4K(total_size) * 4;
	}

	int for_3dnr_buffer_size = 0;
	if(param->is_enable_3dnr){
		for_3dnr_buffer_size = ALIGN_4K(total_size) + ALIGN_4K(total_size / 4);
	}

	int isp_buffer_size = 0;
	if(param->is_enable_isp){
		isp_buffer_size = (0x1A000 + 0x1A000 + 0x4000);
	}

	int vse_buffer_size = 0;
	if(param->is_enable_vse){
		vse_buffer_size = 0x8000;
	}

	printf("camera service ion : [mcm_buf_size:%d] [for_3dnr_buffer_size:%d] [isp_buffer_size:%d] [vse_buffer_size:%d]\n",
		mcm_buf_size, for_3dnr_buffer_size, isp_buffer_size, vse_buffer_size);
	return (mcm_buf_size + for_3dnr_buffer_size + isp_buffer_size + vse_buffer_size);
}

static int vp_ion_vflow_calculator(vp_ion_pipeline_param_t *param){
	//for gdc
	int gdc_bin_file_size = 0;
	int gdc_buffer_size = 0;
	if(param->is_enable_gdc){
		vp_ion_buffer_param_t *gdc = &param->gdc;
		gdc_bin_file_size = ALIGN_4K(param->gdc_bin_file_size);
		//（4K_ALIGN（宽 * 高* 1.5） + 4096 ）* output buffer count
		int gdc_buffer_single_size = ALIGN_4K(gdc->width * gdc->height * 3 / 2) + 4096;
		gdc_buffer_size = gdc_buffer_single_size * gdc->count;
	}

	//for vin
	vp_ion_buffer_param_t *vin = &param->vin;
	int vin_buffer_size = 0;
	int vin_buffer_single_size = 0;
	vin_buffer_single_size = ALIGN_4K(vin->width * vin->height * 2) + 4096;;
	vin_buffer_size = vin_buffer_single_size * vin->count;

	//for isp
	vp_ion_buffer_param_t *isp = &param->isp;
	int isp_buffer_size = 0;
	int isp_buffer_single_size = 0;
	isp_buffer_single_size = ALIGN_4K(isp->width * isp->height * 3 / 2) + 4096;;
	isp_buffer_size = isp_buffer_single_size * isp->count;

	//for vse

	int vse_buffer_size = 0;
	int vse_buffer_single_size = 0;
	for(int i = 0; i < param->vse_valid_count; i++){
		vp_ion_buffer_param_t *vse = &param->vse[i];
		vse_buffer_single_size = ALIGN_4K(vse->width * vse->height * 3 / 2) + 4096;;
		vse_buffer_size += vse_buffer_single_size * vse->count;
	}
	printf("vflow ion : [gdc_bin_file_size:%d] [gdc_buffer_size:%d] [vin_buffer_size:%d] [isp_buffer_size:%d] [vse_buffer_size:%d]\n",
		gdc_bin_file_size, gdc_buffer_size, vin_buffer_size, isp_buffer_size, vse_buffer_size);

	return gdc_bin_file_size + gdc_buffer_size + vin_buffer_size + isp_buffer_size + vse_buffer_size;
}
static int vp_ion_osd_calculator(vp_ion_pipeline_param_t *param){
	int osd_buf_size = 0;
	for(int i = 0; i < param->osd_valid_count; i++){
		osd_buf_size += ALIGN_4K(param->osd[i].width * param->osd[i].height) * 2 * param->osd[i].count;
	}
	printf("osd ion : [osd_buf_size:%d]\n", osd_buf_size);
	return osd_buf_size;
}

static int vp_ion_bpu_calculator(vp_ion_bpu_param_t *param){
	int input_buffer_size = 0;
	int input_buffer_single_size = ALIGN_4K(param->input_width * param->input_height) + ALIGN_4K(param->input_width * param->input_height / 2);
	input_buffer_size = input_buffer_single_size * param->input_queue_count;

	int output_buffer_size = 0;
	int output_buffer_single_size = 0;
	for(int i = 0; i < param->output_dimensions; i++){
		output_buffer_single_size += ALIGN_4K(param->output_size[i]);
	}
	output_buffer_size = output_buffer_single_size * param->output_queue_count;

	int output_calcualtor_size = 0;
	output_calcualtor_size = param->ouput_calculator_size_dynamic;

	printf("bpu ion : [input_buffer_size:%d] [output_buffer_size:%d] [output_calcualtor_size:%d] \n",
		input_buffer_size, output_buffer_size, output_calcualtor_size);
	return (input_buffer_size + output_buffer_size + output_calcualtor_size);
}
static int vp_ion_vpu_calculator(vp_ion_vpu_param_t *param){
	typedef struct {
		int width;
		int height;
		int task_buffer_size;
		int reconstruct_and_eference;
	}camera_resolution_info_t;

	//按照大小顺序存放
	camera_resolution_info_t h264_cam_res_infos[] = {
		{1088, 1280, 16494592, 2088960 * 2},
		{1600, 1200, 16547840, 2883584 * 2},
		{1920, 1080, 16560128 , 3133440 * 2},
		{3840, 2160, 61501440, 12443648 * 2}
	};
	camera_resolution_info_t h265_cam_res_infos[] = {
		{1088, 1280, 16445440, 2088960 * 2},
		{1600, 1200, 16457728, 2883584 * 2},
		{1920, 1080, 16461824, 3112960 * 2},
		{3840, 2160, 60968960, 12443648 * 2}
	};
	if((param->type == ION_H264_DECODEC) || (param->type == ION_H265_DECODEC)){
		SC_LOGE("vp ion calculator not support decodec for %d.", param->type);
		return 0;
	}

	int cam_res_infos_count = sizeof(h264_cam_res_infos) / sizeof(camera_resolution_info_t);
	camera_resolution_info_t *cam_res_infos = h264_cam_res_infos;
	if(ION_H265_ENCODEC == param->type){
		cam_res_infos = h265_cam_res_infos;
		cam_res_infos_count = sizeof(h265_cam_res_infos) / sizeof(camera_resolution_info_t);
	}

	int sei_size = 16384 * 5;
	int custom_map_size = 262144; //h264 only

	int vui_size = 16384;
	int work_queue = 131072;

	//4K_ALIGN(（Allign64(长) * Allign64(宽) / 32）* 2 ) + 4096
	int motion_vector = ALIGN_4K(ALIGN_64(param->width) * ALIGN_64(param->height) / 32 * 2) + 4096;
	int fbc_luma = ALIGN_4K(ALIGN_256(param->width) * ALIGN_64(param->height) / 32 * 2) + 4096;
	int fbc_chroma = ALIGN_4K(ALIGN_256(param->width / 2) * ALIGN_64(param->height) / 32 * 2) + 4096;
	int sub_sampled = ALIGN_4K(ALIGN_32(param->width / 4) * ALIGN_4(param->height / 4) * 2) + 4096;

	int input_buffer_size = ALIGN_4K(param->width * param->height * 3 / 2) * param->input_buffer_count;
	int output_buffer_size = ALIGN_4K(param->width * param->height * 3 / 2) * param->output_buffer_count;

	int camera_res_size = param->width * param->height;
	int camera_res_diff = INT_MAX;
	int camera_res_be_close_to_index = 0;
	int camera_res_index = -1;
	for(int i = 0; i < cam_res_infos_count; i++){
		//找到完全匹配的
		if((cam_res_infos[i].width == param->width) && (cam_res_infos[i].height == param->height)){
			camera_res_index = i;
			break;
		}
		//没有找到完全匹配的，就选择一个接近的
		int diff_tmp = camera_res_size - (cam_res_infos[i].width * cam_res_infos[i].height);
		if(diff_tmp < 0){
			diff_tmp = -diff_tmp;
		}

		if(diff_tmp < camera_res_diff){
			camera_res_diff = diff_tmp;
			camera_res_be_close_to_index = i;
		}
	}

	if(camera_res_index == -1){
		camera_res_index = camera_res_be_close_to_index;
		SC_LOGI("vp ion calculator not found camera[%d*%d], so use [%d*%d] replace it .",
			param->width, param->height,
			cam_res_infos[camera_res_index].width, cam_res_infos[camera_res_index].height);
	}

	if(ION_H265_ENCODEC == param->type){
		return sei_size + vui_size + work_queue + motion_vector + fbc_luma + fbc_chroma + sub_sampled
			+ input_buffer_size + output_buffer_size
			+ cam_res_infos[camera_res_index].reconstruct_and_eference + cam_res_infos[camera_res_index].task_buffer_size;
	}else{

		printf("vpu ion :[%d %d] %d %d %d %d %d %d %d %d\n", param->width, param->height,
			sei_size, custom_map_size, vui_size, work_queue,
			motion_vector, fbc_luma, fbc_chroma, sub_sampled);

		printf("	%d %d %d %d %d %d\n",
			input_buffer_size, param->input_buffer_count, output_buffer_size, param->output_buffer_count,
			cam_res_infos[camera_res_index].reconstruct_and_eference, cam_res_infos[camera_res_index].task_buffer_size);
		return sei_size + custom_map_size + vui_size /*+ work_queue*/ + motion_vector + fbc_luma + fbc_chroma + sub_sampled
			+ input_buffer_size + output_buffer_size
			+ cam_res_infos[camera_res_index].reconstruct_and_eference + cam_res_infos[camera_res_index].task_buffer_size;
	}
	return 0;
}

int vp_ion_pipeline_calculator(vp_ion_pipeline_param_t *param, vp_ion_theory_calc_result_t *result)
{
	memset(result, 0, sizeof(vp_ion_theory_calc_result_t));
	//1. camera_service
	result->camera_service_size = vp_ion_camera_service_calculator(&param->camera_service);
	result->vpu_size = vp_ion_vpu_calculator(&param->vpu);
	result->bpu_size = vp_ion_bpu_calculator(&param->bpu);
	result->vflow_size = vp_ion_vflow_calculator(param);
	result->osd_size = vp_ion_osd_calculator(param);

	printf("pipeline ion(dynamic): [camera_service_size:%ld] [vpu_size:%ld] [bpu_size:%ld] [vflow_size:%ld] [osd_size:%ld]\n",
		result->camera_service_size, result->vpu_size, result->bpu_size, result->vflow_size, result->osd_size);
	return 0;
}

//固定占用
static int vp_ion_camera_service_extern_calculator(vp_ion_camera_service_extern_param_t *param){
	int isp_buffer_size = 0;
	if(param->is_used_isp){
		isp_buffer_size = 0x100000 + 0x1000 + 0x1000;
	}

	int vse_buffer_size = 0;
	if(param->is_used_vse){
		vse_buffer_size = 0x1000 + 0x1000;
	}

	int camera_service_fixed = 0xAF1000;
	return (isp_buffer_size + vse_buffer_size + camera_service_fixed);
}
static int vp_ion_bpu_extern_calculator(vp_ion_bpu_extern_param_t *param){
	int bpu_fixed_size = 0;
	if(param->is_used_bpu){
		bpu_fixed_size = 32768 * 2;
	}
	int bpu_model_file_size = 0;
	int heap_region_size = 0;
	int ouput_calculator_size_static = 0;
	for(int i = 0; i< param->item_count; i++){
		bpu_model_file_size += ALIGN_4K(param->item_param[i].model_file_sizes) + 4096 * 3;
		heap_region_size += param->item_param[i].heap_region_size;
		ouput_calculator_size_static += param->item_param[i].ouput_calculator_size_static;
	}
	printf("pipeline ion(static for bpu): [bpu_fixed_size:%d] [bpu_model_file_size:%d] [heap_region_size:%d] [ouput_calculator_size_static:%d]\n",
		bpu_fixed_size, bpu_model_file_size, heap_region_size, ouput_calculator_size_static);

	return (bpu_fixed_size + bpu_model_file_size + heap_region_size + ouput_calculator_size_static);
}

int vp_ion_pipeline_fixed_calculator(vp_ion_pipeline_fixed_param_t *param, vp_ion_theory_calc_result_t* result){
	memset(result, 0, sizeof(vp_ion_theory_calc_result_t));

	result->camera_service_size = vp_ion_camera_service_extern_calculator(&param->camera_service);
	result->bpu_size = vp_ion_bpu_extern_calculator(&param->bpu);

	printf("pipeline ion(static): [camera_service_size:%ld] [bpu_size:%ld]\n", result->camera_service_size, result->bpu_size);
	return 0;
}

//调试
void vp_ion_pipeline_theory_result_printf(vp_ion_theory_calc_result_t *vp_ion_theory_calc_result){
	printf("\n\n");
	printf("pipeline ion info:\n");
	printf("	osd: %ld 0x%08lx\n", vp_ion_theory_calc_result->osd_size, vp_ion_theory_calc_result->osd_size);
	printf("	vpu: %ld 0x%08lx\n", vp_ion_theory_calc_result->vpu_size, vp_ion_theory_calc_result->vpu_size);
	printf("	bpu: %ld 0x%08lx\n", vp_ion_theory_calc_result->bpu_size, vp_ion_theory_calc_result->bpu_size);
	printf("	vflow: %ld 0x%08lx\n", vp_ion_theory_calc_result->vflow_size, vp_ion_theory_calc_result->vflow_size);
	printf("	camera_service: %ld 0x%08lx\n", vp_ion_theory_calc_result->camera_service_size, vp_ion_theory_calc_result->camera_service_size);
	printf("\n\n");
}

int vp_ion_check_theory_result(vp_ion_all_info_t *before_ion_info, vp_ion_theory_calc_result_t *theory_result){

	int ret = 0;
	vp_ion_all_client_info_t all_clients_current;
	ret = vp_ion_client_get_current_status(&all_clients_current);
	if(ret != 0){
		SC_LOGE("ion client parser failed.\n");
		return -1;
	}else{
		vp_ion_all_client_printf(&all_clients_current);
	}
	vp_ion_all_info_by_key_t all_key_info_current;
	ret = vp_ion_all_key_get_current_status(&all_key_info_current);
	if(ret != 0){
		SC_LOGE("ion key parser failed.\n");
		return -1;
	}else{
		vp_ion_all_key_info_printf(&all_key_info_current);
	}

	/*
		1. osd
		2. vflow
		3. vpu
		4. camera_service
		5. bpu
	*/

	int osd_actual_used = all_clients_current.ion_infos[ION_CLIENT_OSD].total_used -
		before_ion_info->client.ion_infos[ION_CLIENT_OSD].total_used;
	int vflow_actual_used = all_clients_current.ion_infos[ION_CLIENT_VFLOW].total_used -
		before_ion_info->client.ion_infos[ION_CLIENT_VFLOW].total_used;
	int vpu_actual_used = all_clients_current.ion_infos[ION_CLIENT_VPU].total_used -
		before_ion_info->client.ion_infos[ION_CLIENT_VPU].total_used;
	int camera_service_actual_used = all_clients_current.ion_infos[ION_CLIENT_CAM_SERVICE].total_used -
		before_ion_info->client.ion_infos[ION_CLIENT_CAM_SERVICE].total_used;
	int bpu_actual_used = all_key_info_current.ion_infos[ION_KEY_BPU].total_used -
		before_ion_info->key.ion_infos[ION_KEY_BPU].total_used;

	if(osd_actual_used != theory_result->osd_size){
		ret = -1;
		SC_LOGE("pipeline ion calculator(osd) error: before used %d actual used %d, theory value: %d",
			before_ion_info->client.ion_infos[ION_CLIENT_OSD].total_used,
			osd_actual_used, theory_result->osd_size);
	}

	if(vflow_actual_used != theory_result->vflow_size){
		ret = -1;
		SC_LOGE("pipeline ion calculator(vflow) error: before used %d actual used %d, theory value: %d",
			before_ion_info->client.ion_infos[ION_CLIENT_VFLOW].total_used,
			vflow_actual_used, theory_result->vflow_size);
	}

	if(vpu_actual_used != theory_result->vpu_size){
		ret = -1;
		SC_LOGE("pipeline ion calculator(vpu) error: before used %d actual used %d, theory value: %d",
			before_ion_info->client.ion_infos[ION_CLIENT_VPU].total_used,
			vpu_actual_used, theory_result->vpu_size);
	}

	if((camera_service_actual_used != theory_result->camera_service_size) &&
		(all_clients_current.ion_infos[ION_CLIENT_CAM_SERVICE].total_used != theory_result->camera_service_size)){
		ret = -1;
		SC_LOGE("pipeline ion calculator(camera_service) error: total used %d, actual used %d, theory value: %d",
			all_clients_current.ion_infos[ION_CLIENT_CAM_SERVICE].total_used, camera_service_actual_used, theory_result->camera_service_size);
	}

	if(bpu_actual_used != theory_result->bpu_size){
		int diff_value = theory_result->bpu_size - bpu_actual_used;
		if(diff_value < 0){
			diff_value = -diff_value;
		}
		if(diff_value > 2891776){ //2891776 是测试出来的值： yolov5、mobilenet、fcos 同时运行时，实际测试到的值
			ret = -1;
			SC_LOGE("pipeline ion calculator(bpu) error: before used %d actual used %d, theory value: %d",
			before_ion_info->key.ion_infos[ION_KEY_BPU].total_used,
			bpu_actual_used, theory_result->bpu_size);
		}else{
			SC_LOGI("pipeline ion calculator(bpu) not equal, but within the unreachable range (%d <= 2891776): before used %d actual used %d, theory value: %d",
			diff_value, before_ion_info->key.ion_infos[ION_KEY_BPU].total_used, bpu_actual_used, theory_result->bpu_size);
		}
	}

	if(ret == 0){
		SC_LOGI("pipeline ion theory calculate result is correct.");
	}

	return ret;
}

int vp_ion_get_current_status(vp_ion_all_info_t *info){
	int ret = 0;
	int ret_tmp;

	ret_tmp = vp_ion_heap_get_current_status(&info->heap);
	if(ret_tmp != 0){
		SC_LOGE("ion heap parser failed.");
		ret = -1;
	}else{
		vp_ion_all_heap_printf(&info->heap);
	}

	ret_tmp = vp_ion_client_get_current_status(&info->client);
	if(ret_tmp != 0){
		SC_LOGE("ion client parser failed.");
		ret = -1;
	}else{
		vp_ion_all_client_printf(&info->client);
	}

	ret_tmp = vp_ion_all_key_get_current_status(&info->key);
	if(ret_tmp != 0){
		ret = -1;
		SC_LOGE("ion key parser failed.");
	}else{
		vp_ion_all_key_info_printf(&info->key);
	}
	return ret;
}

int64_t vp_ion_check_is_enough(vp_ion_all_info_t *ion_info, vp_ion_theory_calc_result_t *theory_result){
	int64_t total = 0;
	int64_t used = 0;

	for(int i = 0; i< ION_SENTRY; i++){
		total += ion_info->heap.ion_infos[i].total;
		used +=  ion_info->heap.ion_infos[i].used;
	}
	int64_t remain = total - used;
	int64_t need = theory_result->osd_size + theory_result->vpu_size +
		theory_result->bpu_size + theory_result->vflow_size + theory_result->camera_service_size;
	int64_t diff = remain - need;
	if(diff < 0){
		SC_LOGW("ion check: not enough, [total %ld] [used %ld] [remain %ld] [need %ld] [diff %ld]",
			total, used, remain, need, diff);
		return diff;
	}else{
		SC_LOGI("ion check: is enough, [total %ld] [used %ld] [remain %ld] [need %ld] [diff %ld]",
			total, used, remain, need, diff);
	}
	return 0;
}