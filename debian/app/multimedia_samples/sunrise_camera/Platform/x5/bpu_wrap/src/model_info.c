#include <string.h>
#include "model_info.h"
#include "utils/utils_log.h"

static bpu_model_info_t bpu_model_infos[] = {
	{
		.model_file_name = "yolov5s_672x672_nv12.bin",
		.model_file_size = 7864320, //7869634,
		.heap_region_size = 4259840 * 2, //优化后会是 1个

		.ouput_calculator_size_is_fixed = 0,
		.ouput_calculator_size = 9486336,
	},
	{
		.model_file_name = "mobilenetv2_224x224_nv12.bin",
		.model_file_size = 3866624, //3872784,
		.heap_region_size = 0,

		.ouput_calculator_size_is_fixed = 1,
		.ouput_calculator_size = 16384 ,
	},
	{
		.model_file_name = "fcos_efficientnetb0_512x512_nv12.bin",
		.model_file_size = 4259840,//4279776,
		.heap_region_size = 1441792 * 2, //优化后会是 1个

		.ouput_calculator_size_is_fixed = 0,
		.ouput_calculator_size = 0,
	},
};

const bpu_model_info_t* bpu_wrap_model_info(const char *model_name){
	int model_info_count = sizeof(bpu_model_infos) / sizeof(bpu_model_info_t);

	const bpu_model_info_t* ret = NULL;

	int model_name_len = strlen(model_name);
	for(int i = 0; i< model_info_count; i++){
		if(strncmp(bpu_model_infos[i].model_file_name, model_name, model_name_len) == 0){
			ret = &bpu_model_infos[i];
			break;
		}
	}
	if(ret == NULL){
		SC_LOGE("bpu model info func not support model name :[%s]", model_name);
	}
	return ret;
}