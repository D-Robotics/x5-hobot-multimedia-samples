#ifndef __FEEDBACK_GDC_H__
#define __FEEDBACK_GDC_H__

#include "common_utils.h"
// #define GDC_CUSTOM_BUF 0
// #define GDC_INTERNAL_BUF 1

typedef struct gdc_info
{
	uint32_t input_width;
	uint32_t input_height;
	uint32_t output_width;
	uint32_t output_height;
	char* gdc_bin_file;
	char* input_file;
	char* output_file;
	hbn_vnode_image_t input_image;
  hbn_vnode_image_t output_image;
} gdc_info_s;

int feedback_gdc_create(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info, uint32_t flag);
int feedback_gdc(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info, uint32_t flag);
int feedback_gdc_destory(pipe_contex_t *pipe_contex, gdc_info_s *gdc_info, uint32_t flag);

#endif // __FEEDBACK_GDC_H__