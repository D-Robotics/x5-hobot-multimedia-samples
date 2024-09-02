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

struct PerformanceTestParam g_format_convert_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "format_convert"
};

n2d_buffer_format_t yuv_format[] =
{
	N2D_YUYV,
	N2D_UYVY,
	N2D_YV12,
	N2D_I420,
	N2D_NV12,
	N2D_NV21,
	N2D_NV16,
	N2D_NV61,
	N2D_P010_MSB,
	N2D_P010_LSB,
	N2D_I010
};

void format_convert_performance_test(struct PerformanceTestParam *param){

	n2d_buffer_t src = {0};
	n2d_buffer_t dst = {0};
	n2d_error_t error = N2D_SUCCESS;

	//原图: 黑底 + 蓝色矩形框
	N2D_ON_ERROR(performance_test_create_buffer_with_rect(param, N2D_BGRA8888, &src));
	//存储转换后的图: 初始化为黑底
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_NV12, &dst));

	int run_count = 0;
	performance_test_start(param);
	while (run_count < param->iteration_number){
		//foramt convert: N2D_BGRA8888 -> N2D_NV12
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

//rgb => yuyv/uyvy/yv12/I420/nv21/nv12/nv16/nv61/p010_msb/p010_lsb/I010
n2d_error_t format_convert(n2d_buffer_t *src)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_buffer_t tmpbuffer1 = {0};
	n2d_buffer_t tmpbuffer2 = {0};

	if(!n2d_is_feature_support(N2D_FEATURE_YUV420_OUTPUT))
	{
		printf("Not supported YUV!!!\n");
		return N2D_SUCCESS;
	}

	char output_file_name[128];
	for(int i = 0; i<sizeof(yuv_format)/sizeof(yuv_format[0]); i++)
	{
		//从 yuv_format 数组中选择一个格式
		N2D_ON_ERROR(n2d_util_allocate_buffer(
			src->width,
			src->height,
			yuv_format[i],
			N2D_0,
			N2D_LINEAR,
			N2D_TSC_DISABLE,
			&tmpbuffer1));

		//将RGBA(源文件的格式) 转换成yuv_format[i] 格式
		error = n2d_blit(&tmpbuffer1, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE);
		if (N2D_IS_ERROR(error))
		{
			printf("blit error, error=%d.\n", error);
			goto on_error;
		}

		N2D_ON_ERROR(n2d_util_allocate_buffer(
			src->width,
			src->height,
			src->format,
			N2D_0,
			N2D_LINEAR,
			N2D_TSC_DISABLE,
			&tmpbuffer2));

		//将yuv_format[i]格式再转换成RGBA(源文件的格式)
		error = n2d_blit(&tmpbuffer2, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE);
		if (N2D_IS_ERROR(error))
		{
			printf("blit error, error=%d.\n", error);
			goto on_error;
		}

		N2D_ON_ERROR(n2d_commit());

		//保存图片
		memset(output_file_name, 0, sizeof(output_file_name));
		sprintf(output_file_name, "./yuv_format_%d.bmp", i);
		error = n2d_util_save_buffer_to_file(&tmpbuffer2, output_file_name);
		if (N2D_IS_ERROR(error))
		{
			printf("alphablend failed! error=%d.\n", error);
		}
		n2d_free(&tmpbuffer1);
		n2d_free(&tmpbuffer2);
	}

on_error:
	return error;
}


int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_format_convert_sample);
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
	char *input_file_name = "../resource/RGBA8888_640x480.bmp";
	error = n2d_util_load_buffer_from_file(input_file_name, &src);
	if (N2D_IS_ERROR(error))
	{
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_close;
	}
	if(g_format_convert_sample.mode == 0){
		error = format_convert(&src);
		if (N2D_IS_ERROR(error))
		{
			printf("format_convert failed! error=%d.\n", error);
			goto on_free_src;
		}
	}else{
		format_convert_performance_test(&g_format_convert_sample);
	}

on_free_src:
	error = n2d_free(&src);
	if (N2D_IS_ERROR(error))
	{
		printf("free buffer failed! error=%d.\n", error);
		goto on_close;
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