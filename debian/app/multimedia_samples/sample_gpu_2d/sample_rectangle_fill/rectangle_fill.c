/***************************************************************************
 *					  COPYRIGHT NOTICE
 *			 Copyright(C) 2024, D-Robotics Co., Ltd.
 *					 All rights reserved.
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
struct PerformanceTestParam g_rectangle_fill_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "rectangle_fill"
};

n2d_error_t rectangle_fill(n2d_buffer_t *src, n2d_rectangle_t *rect, n2d_color_t color)
{
	n2d_error_t error = N2D_SUCCESS;
	error = n2d_fill(src, rect, color, N2D_BLEND_SRC_OVER);
	if (N2D_IS_ERROR(error))
	{
		printf("fill error, error=%d.\n", error);
		return error;
	}

	error = n2d_commit();
	if (N2D_IS_ERROR(error))
	{
		printf("fill commit error, error=%d.\n", error);
		return error;
	}

	return N2D_SUCCESS;
}
n2d_error_t rectangle_fill_sample(){
	n2d_error_t error = N2D_SUCCESS;

	//创建buffer
	n2d_buffer_t  src;
	error = n2d_util_allocate_buffer(
		640,
		480,
		N2D_BGRA8888,
		N2D_0,
		N2D_LINEAR,
		N2D_TSC_DISABLE,
		&src);
	if (N2D_IS_ERROR(error))
	{
		printf("create buffer error=%d.\n", error);
		return error;
	}

	char *input_file_name = "../resource/R5G6B5_640x640.bmp";
	error = n2d_util_load_buffer_from_file(input_file_name, &src);
	if (N2D_IS_ERROR(error))
	{
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_error;
	}

	 //矩形框1: 蓝色不透明
	n2d_rectangle_t rect_upper_left;
	rect_upper_left.x	  = src.width / 8;
	rect_upper_left.y	  = src.height / 8;
	rect_upper_left.width  = src.width / 4;
	rect_upper_left.height = src.height / 4;
	n2d_color_t color_blue_opaque = N2D_COLOR_BGRA8(255, 0, 0, 255);
	error = rectangle_fill(&src, &rect_upper_left, color_blue_opaque);
	if (N2D_IS_ERROR(error))
	{
		printf("rectangle_fill failed! error=%d.\n", error);
		goto on_error;
	}
	//矩形框2: 绿色半透明
	n2d_rectangle_t rect_upper_right;
	rect_upper_right.x	  = src.width / 8 * 5;
	rect_upper_right.y	  = src.height / 8;
	rect_upper_right.width  = src.width / 4;
	rect_upper_right.height = src.height / 4;
	n2d_color_t color_green_half_transparent = N2D_COLOR_BGRA8(128, 0, 255, 0);
	error = rectangle_fill(&src, &rect_upper_right, color_green_half_transparent);
	if (N2D_IS_ERROR(error))
	{
		printf("rectangle_fill failed! error=%d.\n", error);
		goto on_error;
	}

	//矩形框3: 绿色透明
	n2d_rectangle_t rect_lower_left;
	rect_lower_left.x	  = src.width / 8;
	rect_lower_left.y	  = src.height / 8 * 5;
	rect_lower_left.width  = src.width / 4;
	rect_lower_left.height = src.height / 4;
	n2d_color_t color_green_transparent = N2D_COLOR_BGRA8(0, 0, 255, 0);
	error = rectangle_fill(&src, &rect_lower_left, color_green_transparent);
	if (N2D_IS_ERROR(error))
	{
		printf("rectangle_fill failed! error=%d.\n", error);
		goto on_error;
	}

	//矩形框4: 绿色不透明
	n2d_rectangle_t rect_lower_right;
	rect_lower_right.x	  = src.width / 8 * 5;
	rect_lower_right.y	  = src.height / 8 * 5;
	rect_lower_right.width  = src.width / 4;
	rect_lower_right.height = src.height / 4;
	n2d_color_t color_green_opaque = N2D_COLOR_BGRA8(255, 0, 255, 0);
	error = rectangle_fill(&src, &rect_lower_right, color_green_opaque);
	if (N2D_IS_ERROR(error))
	{
		printf("rectangle_fill failed! error=%d.\n", error);
		goto on_error;
	}

	char *output_file_name = "./R5G6B5_640x640_rectangle_fill.bmp";
	error = n2d_util_save_buffer_to_file(&src, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("rectangle_fill failed! error=%d.\n", error);
		goto on_error;
	}
	printf("Save file to [%s].\n", output_file_name);

on_error:
	error = n2d_free(&src);
	if (N2D_IS_ERROR(error))
	{
		printf("free buffer failed! error=%d.\n", error);
	}
	return error;
}

void rectangle_fill_performance_test(struct PerformanceTestParam *param){
	n2d_error_t error = N2D_SUCCESS;
	//图像: 黑底色
	n2d_buffer_t src;
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &src));

	n2d_rectangle_t full_image_rect;
	full_image_rect.x	  = 0;
	full_image_rect.y	  = 0;
	full_image_rect.width  = src.width;
	full_image_rect.height = src.height;

	int run_count = 0;
	performance_test_start(param);
	while (run_count < param->iteration_number){
		//alphablend
		N2D_ON_ERROR(n2d_fill(&src, &full_image_rect, n2d_red, N2D_BLEND_SRC_OVER););
		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	//时间计算
	performance_test_stop(param);

	//保存图片
	performance_test_save_to_file(param, &src);
on_error:
	if(N2D_INVALID_HANDLE != src.handle){
		n2d_free(&src);
	}

	return;
}
int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_rectangle_fill_sample);
	if(ret != 0){
		return -1;
	}
	/* Open the context. */
	printf("Start !!!\n");
	n2d_error_t error = n2d_open();
	if (N2D_IS_ERROR(error))
	{
		printf("open context failed! error=%d.\n", error);
		goto on_close;
	}
		/* switch to default device and core */
	N2D_ON_ERROR(n2d_switch_device(N2D_DEVICE_0));
	N2D_ON_ERROR(n2d_switch_core(N2D_CORE_0));
	if(g_rectangle_fill_sample.mode == 0){
		N2D_ON_ERROR(rectangle_fill_sample());
	}else{
		rectangle_fill_performance_test(&g_rectangle_fill_sample);
	}
on_error:
	/* Close the context. */
	error = n2d_close();
	if (N2D_IS_ERROR(error))
	{
		printf("close context failed! error=%d.\n", error);
		goto on_error;
	}

on_close:
	printf("Stop !!!\n");
	return 0;
}