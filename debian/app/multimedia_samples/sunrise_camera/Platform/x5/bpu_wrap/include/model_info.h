#ifndef __MODEL_INFO_HH_
#define __MODEL_INFO_HH_
#include <stdio.h>

typedef struct {
	// char* model_name;
	char* model_file_name;
	int model_file_size;
	int heap_region_size;  					// 在固定内存中包含

	int ouput_calculator_size_is_fixed; 	//yolov5 模型：根据运行路数增加， fcos模型不会变化
	int ouput_calculator_size;
}bpu_model_info_t;

const bpu_model_info_t* bpu_wrap_model_info(const char *model_name);

#endif