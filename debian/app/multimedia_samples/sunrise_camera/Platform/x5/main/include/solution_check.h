#ifndef _SOLUTION_CHECK_H_
#define _SOLUTION_CHECK_H_
#include "vp_ion.h"

#define SOLUTION_MAX_PIPELINE_COUNT (32)
typedef struct {
	int pipeline_param_vaild_count;
	vp_ion_pipeline_fixed_param_t extern_param;
	vp_ion_pipeline_param_t pipeline_params[SOLUTION_MAX_PIPELINE_COUNT];
}solution_ion_param_info_t;

typedef struct {
	int need_check_ion_theory;
	vp_ion_all_info_t ion_info;
	vp_ion_theory_calc_result_t vp_ion_theory_calc_result;
}solution_ion_context_t;

int solution_check_ion_theory_calc_result();
int solution_check_ion_is_enough(solution_ion_param_info_t *solution_param_info);

typedef struct{
	int width;
	int height;
	int fps;
}vp_codec_usr_param_t;

typedef struct{
	vp_codec_usr_param_t encode;
	vp_codec_usr_param_t decode;
}vp_codec_usr_param_single_t;

typedef struct{
	int valid_count;
	vp_codec_usr_param_single_t params[SOLUTION_MAX_PIPELINE_COUNT];
}solution_vpu_param_info_t;
float solution_check_vpu_is_enough(solution_vpu_param_info_t *solution_param_info);
#endif