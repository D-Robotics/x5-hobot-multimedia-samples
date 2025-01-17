#include <string.h>
#include "solution_check.h"
#include "utils/utils_log.h"

solution_ion_context_t solution_ion_context = {
	.need_check_ion_theory = 0,
};


int solution_check_ion_is_enough(solution_ion_param_info_t *solution_param_info){
	int ret = 0;
	vp_ion_all_info_t *ion_info = &solution_ion_context.ion_info;
	vp_ion_theory_calc_result_t *theory_result = &solution_ion_context.vp_ion_theory_calc_result;
	memset(theory_result, 0, sizeof(vp_ion_theory_calc_result_t));

	SC_LOGI("solution_check_ion ...");
	//1. 获取当前的ION 内存占用情况
	vp_ion_get_current_status(ion_info);

	//2. 理论计算: 动态变化的参数
	vp_ion_theory_calc_result_t tmp_theory_result;
	for (int i = 0; i < solution_param_info->pipeline_param_vaild_count; i++){
		vp_ion_pipeline_calculator(&solution_param_info->pipeline_params[i], &tmp_theory_result);

		theory_result->osd_size += tmp_theory_result.osd_size;
		theory_result->vpu_size += tmp_theory_result.vpu_size;
		theory_result->bpu_size += tmp_theory_result.bpu_size;
		theory_result->vflow_size += tmp_theory_result.vflow_size;
		theory_result->camera_service_size += tmp_theory_result.camera_service_size;
	}

	//3. 理论计算：固定的参数
	vp_ion_pipeline_fixed_calculator(&solution_param_info->extern_param, &tmp_theory_result);
	theory_result->osd_size += tmp_theory_result.osd_size;
	theory_result->vpu_size += tmp_theory_result.vpu_size;
	theory_result->bpu_size += tmp_theory_result.bpu_size;
	theory_result->vflow_size += tmp_theory_result.vflow_size;
	theory_result->camera_service_size += tmp_theory_result.camera_service_size;

	//4. 打印理算计算结果
	vp_ion_pipeline_theory_result_printf(theory_result);

	//5. 计算ION资源是否足够
	ret = vp_ion_check_is_enough(ion_info, theory_result);
	if(ret == 0){
		SC_LOGI("found ion is enough.");
		//只有在足够的情况下，才需要检查：不够时会用以前的配置
		solution_ion_context.need_check_ion_theory = 1;
	}else{
		if(ret < 0){
			ret = -ret;
		}
		SC_LOGI("found ion is lack %d", ret);
		solution_ion_context.need_check_ion_theory = 0;
	}
	return ret;
}

int solution_check_ion_theory_calc_result(){



	if(solution_ion_context.need_check_ion_theory){
		solution_ion_context.need_check_ion_theory = 0;
//TODO: 盒子模式完善后再打开
#if 0
		vp_ion_all_info_t *ion_info = &solution_ion_context.ion_info;
		vp_ion_theory_calc_result_t *theory_result = &solution_ion_context.vp_ion_theory_calc_result;
		vp_ion_check_theory_result(ion_info, theory_result);
#endif
	}
	return 0;
}

float solution_check_vpu_is_enough(solution_vpu_param_info_t *solution_param_info){
	int vpu_capbility = 3840 * 2160 * 60;
	int vpu_capbility_unit = 1920 * 1080 * 30;

	int theory_cal_result = 0;
	for (int i = 0; i < solution_param_info->valid_count; i++){
		vp_codec_usr_param_single_t *single_param = &solution_param_info->params[i];
		theory_cal_result += single_param->encode.fps * single_param->encode.height * single_param->encode.width;
		theory_cal_result += single_param->decode.fps * single_param->decode.height * single_param->decode.width;
	}

	float ret_tmp = (theory_cal_result - vpu_capbility) / vpu_capbility_unit;
	if(vpu_capbility >= theory_cal_result){
		SC_LOGI("vpu capbility is enough: remain:%d(equal:%f*1080P30) total:%d, theory:%d",
			vpu_capbility - theory_cal_result, -ret_tmp, vpu_capbility, theory_cal_result);
	}else{
		SC_LOGI("vpu capbility is not enough %f*1080P30(%d)", ret_tmp, theory_cal_result - vpu_capbility);
		return ret_tmp;
	}
	return 0.0;
}