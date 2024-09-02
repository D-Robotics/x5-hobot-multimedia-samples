/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#include "common_utils.h"

#include "GC820/nano2D.h"
#include "GC820/nano2D_util.h"

#include "performance_test_util.h"

struct PerformanceTestParam g_alphablend_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "alphablend"
};

n2d_blend_t blend_modes[] =
{
	N2D_BLEND_SRC_OVER, // S + (1 - Sa) * D
	N2D_BLEND_DST_OVER, // (1 – Da) * S + D
	N2D_BLEND_SRC_IN,	// Da * S
	N2D_BLEND_DST_IN,   // Sa * D
	N2D_BLEND_ADDITIVE, // S + D
	N2D_BLEND_SUBTRACT, // D * (1 – S)
};
const char *blend_mode_names[] = {
	"src_over",
	"dst_over",
	"src_in",
	"dst_in",
	"additive",
	"subtract"
};
void alphablend_performance_test(struct PerformanceTestParam *param){

	n2d_error_t error = N2D_SUCCESS;

	n2d_int32_t x_unity = param->image_width / 8;
	n2d_int32_t y_unity = param->image_height / 8;

	//图像1: 黑底色 + 蓝色矩形
	n2d_buffer_t  src;
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &src));

	n2d_uint8_t src_alpha = 0x80;
	n2d_color_t src_color = N2D_COLOR_BGRA8(src_alpha, 0x00, 0x00, 0xff);//蓝色
	n2d_rectangle_t src_rect = {.x = x_unity, .y = y_unity, .width = x_unity * 5, .height = y_unity * 3};
	N2D_ON_ERROR(n2d_fill(&src, &src_rect, src_color, N2D_BLEND_NONE));//src 填充 蓝色矩形框

	//图像2: 黑底色 + 红色矩形
	n2d_buffer_t  dst;
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &dst));

	n2d_uint8_t dst_alpha = 0x80;
	n2d_color_t dst_color = N2D_COLOR_BGRA8(dst_alpha, 0xff, 0x00, 0x00);//红色
	n2d_rectangle_t dst_rect = {.x = x_unity * 3, .y = y_unity * 2, .width = x_unity * 4, .height = y_unity * 5};
	N2D_ON_ERROR(n2d_fill(&dst, &dst_rect, dst_color, N2D_BLEND_NONE)); //dst 填充 红色矩形框

	int run_count = 0;
	performance_test_start(param);
	while (run_count < param->iteration_number){
		//alphablend
		N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, N2D_BLEND_DST_OVER));
		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	//时间计算
	performance_test_stop(param);

	//保存图片
	performance_test_save_to_file(param, &dst);
on_error:
	if(N2D_INVALID_HANDLE != src.handle){
		n2d_free(&src);
	}

	if(N2D_INVALID_HANDLE != dst.handle){
		n2d_free(&dst);
	}
	return;
}
void alphablend_sample(int image_width, int image_height){
	n2d_error_t error = N2D_SUCCESS;
	n2d_int32_t deltaX = image_width / 8;
	n2d_int32_t deltaY = image_height / 8;

	//图像1相关变量
	n2d_buffer_t  src;
	n2d_uint8_t src_alpha;
	n2d_color_t src_color;
	n2d_rectangle_t src_rect;
	src_rect.x = deltaX;
	src_rect.y = deltaY;
	src_rect.width  = deltaX * 5;
	src_rect.height = deltaY * 3;
	error = n2d_util_allocate_buffer(
		image_width,
		image_height,
		N2D_BGRA8888,
		N2D_0,
		N2D_LINEAR,
		N2D_TSC_DISABLE,
		&src);
	if (N2D_IS_ERROR(error))
	{
		printf("create buffer error=%d.\n", error);
		return;
	}

	//图像2相关变量
	n2d_buffer_t  dst;
	n2d_uint8_t dst_alpha;
	n2d_color_t dst_color;
	n2d_rectangle_t dst_rect;
	dst_rect.x = deltaX * 3;
	dst_rect.y = deltaY * 2;
	dst_rect.width  = deltaX * 4;
	dst_rect.height = deltaY * 5;
	error = n2d_util_allocate_buffer(
		image_width,
		image_height,
		N2D_BGRA8888,
		N2D_0,
		N2D_LINEAR,
		N2D_TSC_DISABLE,
		&dst);
	if (N2D_IS_ERROR(error))
	{
		printf("create buffer error=%d.\n", error);
		return;
	}

	char output_file_name[128];
	//遍历每种模式
	for (int n = 0; n < N2D_COUNTOF(blend_modes); n++)
	{
		src_alpha = 0x80;
		dst_alpha = 0x80;

		for (int i = 0; i < 8; i++)
		{
			src_color = N2D_COLOR_BGRA8(src_alpha, 0x00, 0x00, 0xff);//蓝色
			dst_color = N2D_COLOR_BGRA8(dst_alpha, 0xff, 0x00, 0x00);//红色

			//src 填充0
			N2D_ON_ERROR(n2d_fill(&src, N2D_NULL, 0x0, N2D_BLEND_NONE));
			//src 填充 蓝色矩形框
			N2D_ON_ERROR(n2d_fill(&src, &src_rect, src_color, N2D_BLEND_NONE));

			//dst 填充0
			N2D_ON_ERROR(n2d_fill(&dst, N2D_NULL, 0x0, N2D_BLEND_NONE));
			//dst 填充 红色矩形框
			N2D_ON_ERROR(n2d_fill(&dst, &dst_rect, dst_color, N2D_BLEND_NONE));

			//alphablend
			N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, blend_modes[n]));
			N2D_ON_ERROR(n2d_commit());

			//保存图片
			memset(output_file_name, 0, sizeof(output_file_name));
			sprintf(output_file_name, "./%s_mode_index_%d_%d-%d.bmp", blend_mode_names[n] , i,
				src_alpha, dst_alpha);
			error = n2d_util_save_buffer_to_file(&dst, output_file_name);
			if (N2D_IS_ERROR(error))
			{
				printf("alphablend failed! error=%d.\n", error);
			}

			//颜色渐变
			src_alpha += 0x10;
			dst_alpha -= 0x10;
		}
	}
on_error:
	if(N2D_INVALID_HANDLE != src.handle)
	{
		n2d_free(&src);
	}

	if(N2D_INVALID_HANDLE != dst.handle)
	{
		n2d_free(&dst);
	}
	return;
}

int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_alphablend_sample);
	if(ret != 0){
		return -1;
	}
	/* parse input param */
	printf("Start !!!\n");
	/* Open the context. */
	n2d_error_t error = n2d_open();
	if (N2D_IS_ERROR(error))
	{
		printf("open context failed! error=%d.\n", error);
		goto on_error;
	}
	/* switch to default device and core */
	N2D_ON_ERROR(n2d_switch_device(N2D_DEVICE_0));
	N2D_ON_ERROR(n2d_switch_core(N2D_CORE_0));
	if(g_alphablend_sample.mode == 0){
		alphablend_sample(g_alphablend_sample.image_width, g_alphablend_sample.image_height);
	}else{
		alphablend_performance_test(&g_alphablend_sample);
	}

	/* Close the context. */
	error = n2d_close();
	if (N2D_IS_ERROR(error))
	{
		printf("close context failed! error=%d.\n", error);
		goto on_error;
	}

on_error:
	printf("Stop !!!\n");
	return 0;
}