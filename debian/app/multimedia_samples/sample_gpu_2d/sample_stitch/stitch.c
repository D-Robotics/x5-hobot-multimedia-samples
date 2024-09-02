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

struct PerformanceTestParam g_stitch_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "stitch"
};

void stitch_performance_test(struct PerformanceTestParam *param, int stitch_count)
{

	n2d_buffer_t src[4] = {0};
	n2d_color_t colors[4] = {n2d_red, n2d_blue, n2d_green, n2d_white};
	n2d_buffer_t dst = {0};
	n2d_error_t error = N2D_SUCCESS;

	// 原图: 创建stitch_count张图片
	for(int i = 0; i< stitch_count; i++){
		N2D_ON_ERROR(performance_test_create_buffer_with_color(param, N2D_BGRA8888, &src[i], n2d_light_grey));
		N2D_ON_ERROR(performance_test_add_rect(param, &src[i], TOP_LEFT, colors[i]));
	}

	// 输出图片：stitch后的图片
	if(stitch_count == 4){
		N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &dst));
	}else if(stitch_count == 2){
		dst.width = param->image_width;
		dst.height = param->image_height / 2;

		N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &dst));
	}else{
		printf("not support stitch_count %d\n", stitch_count);
		exit(-1);
	}


	int srcNum = stitch_count;
	struct ResolutionInformation res_info = {
		.input_batch = srcNum,
		.input_width = param->image_width,
		.input_height = param->image_height,
		.output_width = dst.width,
		.output_height = dst.height,
	};

	int run_count = 0;
	performance_test_start(param);
	while (run_count < param->iteration_number)
	{
		if(stitch_count == 4){
			int src_index = 0;
			n2d_rectangle_t dstrect;
			dstrect.x = 0;
			dstrect.y = 0;
			dstrect.width  = dst.width;
			dstrect.height = dst.height;
			for (int y = 0; y < 2; y++)
			{
				for (int x = 0; x < 2; x++)
				{
					dstrect.x = x * (dst.width / 2) + 5;
					dstrect.y = y * (dst.height / 2) + 5;
					dstrect.width  = dst.width / 2 - 10;
					dstrect.height = dst.height / 2 - 10;

					N2D_ON_ERROR(n2d_blit(&dst, &dstrect, &src[src_index], N2D_NULL, N2D_BLEND_NONE));
					src_index++;
				}
			}
		}else if(stitch_count == 2){
			int width = dst.width / 2 - 10;
			int height = dst.height - 10;
			n2d_rectangle_t dstrect1 = {5, 5, width, height};
			N2D_ON_ERROR(n2d_blit(&dst, &dstrect1, &src[0], N2D_NULL, N2D_BLEND_NONE));

			int x_offset = (dst.width / 2) + 5;
			n2d_rectangle_t dstrect2 = {x_offset, 5, width, height};
			N2D_ON_ERROR(n2d_blit(&dst, &dstrect2, &src[1], N2D_NULL, N2D_BLEND_NONE));

		}else{
			printf("not support stitch_count %d\n", stitch_count);
			exit(-1);
		}

		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	performance_test_stop_with_resolution_info(param, &res_info);

	char performance_test_file_name[128];
	memset(performance_test_file_name, 0, sizeof(performance_test_file_name));
	sprintf(performance_test_file_name, "%d_%d_%d_stitch_to_%d_%d",
		stitch_count, param->image_width, param->image_height, dst.width, dst.height);
	performance_test_save_to_file_width_name(param, &dst, performance_test_file_name);

on_error:
	for(int i = 0; i < stitch_count; i++){
		n2d_free(&src[i]);
	}

	n2d_free(&dst);
}

n2d_error_t stitch_sample()
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_buffer_t src[4] = {0};
	n2d_buffer_t dst = {0};

	// 读取文件
	N2D_ON_ERROR(n2d_util_load_buffer_from_file("../resource/scenery01.bmp", &src[0]));
	N2D_ON_ERROR(n2d_util_load_buffer_from_file("../resource/scenery02.bmp", &src[1]));
	N2D_ON_ERROR(n2d_util_load_buffer_from_file("../resource/scenery03.bmp", &src[2]));
	N2D_ON_ERROR(n2d_util_load_buffer_from_file("../resource/scenery04.bmp", &src[3]));

	// 创建存储 stitch后的buffer
	int dst_width = src[0].width * 2;
	int dst_height =  src[0].height * 2;
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
		printf("n2d_util_allocate_buffer failed! error=%d.\n", error);
		goto on_free_src;
	}
	N2D_ON_ERROR(n2d_fill(&dst, N2D_NULL, 0x0, N2D_BLEND_NONE));


	int src_index = 0;
	n2d_rectangle_t dstrect;
	dstrect.x = 0;
    dstrect.y = 0;
    dstrect.width  = dst.width;
    dstrect.height = dst.height;
    for (int y = 0; y < 2; y++)
    {
        for (int x = 0; x < 2; x++)
        {
			dstrect.x = x * (dst.width / 2) + 5;
            dstrect.y = y * (dst.height / 2) + 5;
            dstrect.width  = dst.width / 2 - 10;
            dstrect.height = dst.height / 2 - 10;

			N2D_ON_ERROR(n2d_blit(&dst, &dstrect, &src[src_index], N2D_NULL, N2D_BLEND_NONE));
			src_index++;
		}
	}
	N2D_ON_ERROR(n2d_commit());

	// 保存图片
	char output_file_name[128];
	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./stitch_sample_%d_%d.bmp", dst.width, dst.height);
	error = n2d_util_save_buffer_to_file(&dst, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
		goto on_error;
	}

	printf("stitch input file %s [%d*%d] ==> output file %s [%d*%d]\n",
		"pip0x", src[0].width, src[0].height,
		output_file_name, dst.width, dst.height);
on_error:
	n2d_free(&dst);
on_free_src:
	n2d_free(&src[0]);
	n2d_free(&src[1]);
	n2d_free(&src[2]);
	n2d_free(&src[3]);

	return error;
}

int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_stitch_sample);
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

	if (g_stitch_sample.mode == 0)
	{
		error = stitch_sample();
		if (N2D_IS_ERROR(error))
		{
			goto on_close;
		}
	}
	else
	{
		stitch_performance_test(&g_stitch_sample, 4);
		stitch_performance_test(&g_stitch_sample, 2);
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