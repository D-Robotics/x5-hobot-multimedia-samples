// Copyright (c) 2024，D-Robotics.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

struct PerformanceTestParam g_rotation_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "rotation"
};

void rotation_performance_test(struct PerformanceTestParam *param){
	n2d_buffer_t src = {0};
	n2d_buffer_t dst = {0};
	n2d_error_t error = N2D_SUCCESS;

	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &src)); 	//原图: 黑底
	N2D_ON_ERROR(performance_test_add_rect(param, &src,TOP_LEFT, n2d_blue)); 		//左上 蓝色
	N2D_ON_ERROR(performance_test_add_rect(param, &src,TOP_RIGHT, n2d_white)); 		//右上 白色
	N2D_ON_ERROR(performance_test_add_rect(param, &src,BOTTOM_LEFT, n2d_green)); 	//左下 绿色
	N2D_ON_ERROR(performance_test_add_rect(param, &src,BOTTOM_RIGHT, n2d_red)); 	//右下 红色

	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &dst)); //原图: 黑底

	int run_count = 0;
	performance_test_start(param);
	while (run_count < param->iteration_number){
		src.orientation = N2D_0;
		dst.orientation = N2D_90;

		N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, N2D_BLEND_NONE));
		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	performance_test_stop(param);

	performance_test_save_to_file(param, &dst);
on_error:
	if(N2D_INVALID_HANDLE != src.handle){
		n2d_free(&src);
	}

	if(N2D_INVALID_HANDLE != dst.handle){
		n2d_free(&dst);
	}
}

n2d_error_t rotation_sample(n2d_buffer_t *src, n2d_buffer_t *dst, n2d_orientation_t orientation)
{
	n2d_error_t error = N2D_SUCCESS;
	src->orientation = N2D_0;
	dst->orientation = orientation;

	error = n2d_blit(dst, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE);
	if (N2D_IS_ERROR(error))
	{
		printf("blit error, error=%d.\n", error);
		goto on_error;
	}

	error = n2d_commit();
	if (N2D_IS_ERROR(error))
	{
		printf("blit error, error=%d.\n", error);
		goto on_error;
	}

	return N2D_SUCCESS;
on_error:
	if(N2D_INVALID_HANDLE != src->handle)
	{
		n2d_free(src);
	}

	return error;
}

int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_rotation_sample);
	if(ret != 0){
		return -1;
	}

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

	//读取文件
	n2d_buffer_t  src;
	char *input_file_name = "../resource/R5G6B5_640x640.bmp";
	error = n2d_util_load_buffer_from_file(input_file_name, &src);
	if (N2D_IS_ERROR(error))
	{
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_close;
	}

	/* Allocate the buffer. */
	n2d_buffer_t  dst;
	error = n2d_util_allocate_buffer(
		src.width,
		src.height,
		N2D_BGRA8888,
		N2D_0,
		N2D_LINEAR,
		N2D_TSC_DISABLE,
		&dst);
	if (N2D_IS_ERROR(error))
	{
		printf("allocate buffer failed! error=%d.\n", error);
		goto on_free_src;
	}
	if(g_rotation_sample.mode == 0){
		/*
			Run the test case.
			N2D_0,			// 不旋转
			N2D_90,			// 顺时针旋转90度
			N2D_180,		// 顺时针旋转180度
			N2D_270,		// 顺时针旋转270度
			N2D_FLIP_X,		// X 轴镜像
			N2D_FLIP_Y,		// Y 轴镜像
		*/
		error = rotation_sample(&src, &dst, N2D_90);
		if (N2D_IS_ERROR(error))
		{
			printf("rotation failed! error=%d.\n", error);
			goto on_free_src;
		}
		char *output_file_name = "./R5G6B5_640x640_rotated.bmp";
		error = n2d_util_save_buffer_to_file(&dst, output_file_name);
		if (N2D_IS_ERROR(error))
		{
			printf("rotation failed! error=%d.\n", error);
		}
		printf("Save file to [%s].\n", output_file_name);
	}else{
		rotation_performance_test(&g_rotation_sample);
	}



on_free_src:
	error = n2d_free(&dst);
	if (N2D_IS_ERROR(error))
	{
		printf("free buffer failed! error=%d.\n", error);
	}
	error = n2d_free(&src);
	if (N2D_IS_ERROR(error))
	{
		printf("free buffer failed! error=%d.\n", error);
	}

on_close:
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