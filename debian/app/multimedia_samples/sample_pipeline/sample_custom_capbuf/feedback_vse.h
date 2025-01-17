#ifndef __FEEDBACK_VSE_H__
#define __FEEDBACK_VSE_H__

#include "common_utils.h"
// #define VSE_CUSTOM_BUF 0
// #define VSE_INTERNAL_BUF 1

typedef struct scaler_info
{
	uint32_t input_width;
	uint32_t input_height;
	char* yuv_file;
	hbn_vnode_image_t input_image;
	hbn_vnode_image_t output_image[VSE_MAX_CHANNELS];
} scaler_info_s;

int feedback_vse_create(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info, uint32_t output_chn_mask, uint32_t flag);
int feedback_vse(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info,
			uint32_t output_chn_mask, uint32_t flag);
int feedback_vse_destory(pipe_contex_t *pipe_contex, scaler_info_s *scaler_info, uint32_t output_chn_mask, uint32_t flag);

#endif // __FEEDBACK_VSE_H__