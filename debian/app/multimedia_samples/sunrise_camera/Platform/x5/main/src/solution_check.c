#include <string.h>
#include "solution_check.h"
#include "utils/utils_log.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"

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

const char *get_video_codec_type(const char *url)
{
	const char *codec_type = NULL;
	AVFormatContext *format_ctx = NULL;
	AVCodecParameters *codec_params = NULL;
	AVDictionary *option = NULL;
	av_dict_set(&option, "stimeout", "2000000", 0);
	av_dict_set(&option, "bufsize", "1024000", 0);
	av_dict_set(&option, "rtsp_transport", "tcp", 0);

	const AVCodec *codec = NULL;
	av_log_set_level(AV_LOG_FATAL); // 只输出严重错误
	avformat_network_init();
	if (avformat_open_input(&format_ctx, url, NULL, &option) < 0)
	{
		fprintf(stderr, "无法打开文件或 URL: %s\n", url);
		return "error";
	}
	if (avformat_find_stream_info(format_ctx, NULL) < 0)
	{
		fprintf(stderr, "无法获取流信息: %s\n", url);
		avformat_close_input(&format_ctx);
		return "unsupport";
	}

	for (unsigned int i = 0; i < format_ctx->nb_streams; i++)
	{
		codec_params = format_ctx->streams[i]->codecpar;
		if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			codec = avcodec_find_decoder(codec_params->codec_id);
			if (codec == NULL)
			{
				codec_type = "unsupport";
				fprintf(stderr, "未找到解码器: %s\n", url);
				break;
			}

			if (codec_params->codec_id == AV_CODEC_ID_H264)
			{
				codec_type = "h264";
			}
			else if (codec_params->codec_id == AV_CODEC_ID_H265)
			{
				codec_type = "h265";
			}
			else if (codec_params->codec_id == AV_CODEC_ID_MJPEG)
			{
				codec_type = "jpeg";
			}
			else
			{
				codec_type = "unsupport";
			}
			break;
		}
	}
	avformat_close_input(&format_ctx);
	return codec_type;
}

void solution_check_decode_param_is_match(solution_decode_param_info_t* decode_param,
	solution_decode_param_check_info_t *check_result){
	check_result->not_match_count = 0;
	for(int i = 0; i< decode_param->valid_count; i++){
		const char *codec_type = get_video_codec_type(decode_param->params[i].input_file);
		if(strcmp(codec_type, "error") == 0){
			check_result->codec_info[i].actual_codec_type = "error";
		}else if(strcmp(codec_type, "unsupport") == 0){
			check_result->codec_info[i].actual_codec_type = "unsupport";
		}else if(strcmp(codec_type, decode_param->params[i].codec_type) == 0){
			continue;
		}else{
			check_result->codec_info[i].actual_codec_type = codec_type;
		}
		check_result->codec_info[i].pipeline_id = i;
		check_result->codec_info[i].config_codec_type = decode_param->params[i].codec_type;

		check_result->not_match_count++;
	}
}
