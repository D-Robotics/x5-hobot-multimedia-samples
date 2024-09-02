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

struct PerformanceTestParam g_crop_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "crop"
};

void crop_performance_test(struct PerformanceTestParam *param)
{

	n2d_buffer_t src = {0};
	n2d_buffer_t dst = {0};
	n2d_error_t error = N2D_SUCCESS;

	//1. 创建输入图片：构造最大分辨率的图片作为输入(4K 3840*2160)
	src.width = 3840;
	src.height = 2160;
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &src));
	N2D_ON_ERROR(performance_test_add_rect(param, &src,TOP_LEFT, n2d_blue)); 		//左上角 添加 蓝色矩形框

	//2. 创建输出图片: 根据输入的参数从 输入的左上角裁剪
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &dst));

	//3. 裁剪的位置：根据输入参数大小
	n2d_rectangle_t src_rect;
	src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width  = dst.width;
    src_rect.height = dst.height;

	struct ResolutionInformation res_info = {
		.input_batch = 1,
		.input_width = src.width,
		.input_height = src.height,
		.output_width = dst.width,
		.output_height = dst.height,
	};

	int run_count = 0;
	performance_test_start(param);
	while (run_count < param->iteration_number)
	{
		N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, &src_rect, N2D_BLEND_NONE));
		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	performance_test_stop_with_resolution_info(param, &res_info);

	char performance_test_file_name[128];
	memset(performance_test_file_name, 0, sizeof(performance_test_file_name));
	sprintf(performance_test_file_name, "%d_%d_crop_to_%d_%d",
		src.width, src.height, dst.width, dst.height);
	performance_test_save_to_file_width_name(param, &dst, performance_test_file_name);

on_error:
	if (N2D_INVALID_HANDLE != src.handle)
	{
		n2d_free(&src);
	}

	if (N2D_INVALID_HANDLE != dst.handle)
	{
		n2d_free(&dst);
	}
}

n2d_error_t crop_sample()
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_buffer_t src = {0};
	n2d_buffer_t dst = {0};

	// 读取文件
	char *input_file_name = "../resource/RGBA8888_640x480.bmp";
	error = n2d_util_load_buffer_from_file(input_file_name, &src);
	if (N2D_IS_ERROR(error))
	{
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_loadfile_failed;
	}

	// 创建存储 crop后的buffer: 把原图的左上角图片，裁剪下来
	int dst_width = src.width / 4;
	int dst_height = src.height / 4;
	error = n2d_util_allocate_buffer(
		dst_width,
		dst_height,
		N2D_BGRA8888,
		N2D_0,
		N2D_LINEAR,
		N2D_TSC_DISABLE,
		&dst);
	if (N2D_IS_ERROR(error))
	{
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_src;
	}
	n2d_rectangle_t src_rect;
	src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width  = dst.width;
    src_rect.height = dst.height;

	//从src图中裁剪矩形框（src_rect）到 dst中
	N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, &src_rect, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_commit());

	// 保存图片
	char output_file_name[128];
	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./crop_sample_%d_%d.bmp", dst.width, dst.height);
	error = n2d_util_save_buffer_to_file(&dst, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
		goto on_error;
	}

	printf("crop input file %s [%d*%d] ==> output file %s [%d*%d]\n",
		input_file_name, src.width, src.height,
		output_file_name, dst.width, dst.height);
on_error:
	n2d_free(&dst);
on_free_src:
	n2d_free(&src);
on_loadfile_failed:
	return error;
}

int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_crop_sample);
	if (ret != 0)
	{
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

	if (g_crop_sample.mode == 0)
	{
		error = crop_sample();
		if (N2D_IS_ERROR(error))
		{
			goto on_close;
		}
	}
	else
	{
		crop_performance_test(&g_crop_sample);
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