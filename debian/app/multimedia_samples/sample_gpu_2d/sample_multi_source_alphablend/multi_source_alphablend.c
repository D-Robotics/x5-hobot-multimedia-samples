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

struct PerformanceTestParam g_multi_source_alphablend_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "multi_source_alphablend"};

void multi_source_alphablend_performance_test(struct PerformanceTestParam *param)
{

	n2d_buffer_t src[4] = {0};
	n2d_buffer_t dst = {0};
	n2d_error_t error = N2D_SUCCESS;

	// 1. 输入: 创建4张图片
	int srcNum = sizeof(src) / sizeof(n2d_buffer_t);
	N2D_ON_ERROR(performance_test_create_buffer_with_color(param, N2D_BGRA8888, &src[0], n2d_black));
	N2D_ON_ERROR(performance_test_create_buffer_with_color(param, N2D_BGRA8888, &src[1], n2d_red));
	N2D_ON_ERROR(performance_test_create_buffer_with_color(param, N2D_BGRA8888, &src[2], n2d_blue));
	N2D_ON_ERROR(performance_test_create_buffer_with_color(param, N2D_BGRA8888, &src[3], n2d_green));

	// 2. 输出：存储 多源混合的结果
	N2D_ON_ERROR(performance_test_create_buffer_black(param, N2D_BGRA8888, &dst));

	//3. n2d_multisource_blit接口需要的配置参数
	n2d_state_config_t clipRect = {.state = N2D_SET_CLIP_RECTANGLE, .config.clipRect = {0}}; 		//源数据进行裁剪
	n2d_state_config_t srcIndex = {.state = N2D_SET_MULTISOURCE_INDEX};								//指定在多个源数据中的顺序
	n2d_state_config_t blend = {.state = N2D_SET_ALPHABLEND_MODE};									//指定混合模式
	n2d_state_config_t multisrcAndDstRect = {.state = N2D_SET_MULTISRC_DST_RECTANGLE};				//
	n2d_state_config_t rop = {.state = N2D_SET_ROP, .config.rop = {.fg_rop = 0xCC, .bg_rop = 0xCC}};
	n2d_state_config_t globalAlpha = {.state = N2D_SET_GLOBAL_ALPHA,
						.config.globalAlpha = {N2D_GLOBAL_ALPHA_ON, N2D_GLOBAL_ALPHA_ON, 0xAA, 0xAA}};
	clipRect.config.clipRect.x = 0;
	clipRect.config.clipRect.y = 0;
	clipRect.config.clipRect.width = dst.width;
	clipRect.config.clipRect.height = dst.height;

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
		//4. 设置n2d_multisource_blit接口接口需要的配置参数
		for (int j = 0; j < srcNum; j++)
		{
			n2d_int32_t x_offset = param->image_width / 8 * 3;
			n2d_int32_t y_offset = param->image_height / 8 * 3;

			n2d_int32_t rect_width = param->image_width / 2;
			n2d_int32_t rect_height = param->image_height / 2;
			n2d_rectangle_t rectSrc = {0, 0, rect_width, rect_height};
			n2d_rectangle_t rectDst = {0};

			switch (j)
			{
			case 0:
				//第1个输入：整个图参与 n2d_multisource_blit
				rectDst.x = 0;
				rectDst.y = 0;
				rectDst.width = dst.width;
				rectDst.height = dst.height;
				blend.config.alphablendMode = N2D_BLEND_NONE; 		//不融合，作为背景
				break;

			case 1:
				//第2个输入：从起始点(24, 208)，选择 rect_width *rect_height 的矩形框，参与 n2d_multisource_blit
				rectDst.x = x_offset;
				rectDst.y = rect_height - y_offset;
				rectDst.width = rect_width;
				rectDst.height = rect_height;
				blend.config.alphablendMode = N2D_BLEND_ADDITIVE;
				break;

			case 2:
				//第3个输入：从起始点(0, 0)，选择 rect_width *rect_height 的矩形框，参与 n2d_multisource_blit
				rectDst.x = x_offset;
				rectDst.y = y_offset;
				rectDst.width = rect_width;
				rectDst.height = rect_height;
				blend.config.alphablendMode = N2D_BLEND_ADDITIVE;
				break;

			case 3:
				//第4个输入：从起始点(296, 32)，选择 rect_width *rect_height 的矩形框，参与 n2d_multisource_blit
				rectDst.x = rect_width - x_offset;
				rectDst.y = y_offset;
				rectDst.width = rect_width;
				rectDst.height = rect_height;
				blend.config.alphablendMode = N2D_BLEND_ADDITIVE;
				break;
			}

			multisrcAndDstRect.config.multisrcAndDstRect.source = &src[j];
			multisrcAndDstRect.config.multisrcAndDstRect.srcRect = rectSrc;
			multisrcAndDstRect.config.multisrcAndDstRect.dstRect = rectDst;

			srcIndex.config.multisourceIndex = j;
			N2D_ON_ERROR(n2d_set(&srcIndex));
			N2D_ON_ERROR(n2d_set(&globalAlpha));
			N2D_ON_ERROR(n2d_set(&clipRect));
			N2D_ON_ERROR(n2d_set(&blend));
			N2D_ON_ERROR(n2d_set(&multisrcAndDstRect));
			N2D_ON_ERROR(n2d_set(&rop));
		}

		//5. 执行多源融合
		/**
		 * 	参数0x0F的含义：
		 * 	1. n2d_multisource_blit 最多支持8个输入源
		 * 	2. 每个源是否生效可以通过对应位是否位1来使能，
		 * 		比如4个输入都要生效，参数就要设置成 0x0F
		 */
		N2D_ON_ERROR(n2d_multisource_blit(&dst, 0x0F));
		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	performance_test_stop_with_resolution_info(param, &res_info);

	char performance_test_file_name[128];
	memset(performance_test_file_name, 0, sizeof(performance_test_file_name));
	sprintf(performance_test_file_name, "4_%d_%d_multi_source_alphablend_to_%d_%d",
			param->image_width, param->image_height, dst.width, dst.height);
	performance_test_save_to_file_width_name(param, &dst, performance_test_file_name);

on_error:
	n2d_free(&src[0]);
	n2d_free(&src[1]);
	n2d_free(&src[2]);
	n2d_free(&src[3]);

	n2d_free(&dst);
}

n2d_error_t multi_source_alphablend_sample()
{
	n2d_error_t error = N2D_SUCCESS;

	//1. 输入：读取多个文件
	char *sBasicFile[] = {
		"../resource/scenery01.bmp",
		"../resource/scenery02.bmp",
		"../resource/scenery03.bmp",
		"../resource/scenery04.bmp",
	};
	int srcNum = sizeof(sBasicFile) / sizeof(char *);
	n2d_buffer_t src[4] = {0};
	for (int j = 0; j < srcNum; j++)
	{
		error = n2d_util_load_buffer_from_file(sBasicFile[j], &src[j]);
		if (N2D_IS_ERROR(error))
		{
			printf("load No.%d file error, error=%d.\n", j, error);
			goto on_error;
		}
	}

	//2. 输出：存储 多源混合的结果
	n2d_buffer_t dst = {0};
	int dst_width = src[0].width;
	int dst_height = src[0].height;
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


	/*
		n2d_multisource_blit接口支持的 多种config（n2d_state_config_t），n2d_state_config_t 使用方法如下：
		1. 成员config是一个联合体：包含了多种配置
		2. 成员state 用来指定联合体config 使用的是什么类型
		3. 统一使用 n2d_set 接口设置这些配置
	 */

	//3. n2d_multisource_blit接口需要的配置参数
	n2d_state_config_t clipRect = {.state = N2D_SET_CLIP_RECTANGLE, .config.clipRect = {0}}; 		//源数据进行裁剪
	n2d_state_config_t srcIndex = {.state = N2D_SET_MULTISOURCE_INDEX};								//指定在多个源数据中的顺序
	n2d_state_config_t blend = {.state = N2D_SET_ALPHABLEND_MODE};									//指定混合模式
	n2d_state_config_t multisrcAndDstRect = {.state = N2D_SET_MULTISRC_DST_RECTANGLE};				//
	n2d_state_config_t rop = {.state = N2D_SET_ROP, .config.rop = {.fg_rop = 0xCC, .bg_rop = 0xCC}};
	n2d_state_config_t globalAlpha = {.state = N2D_SET_GLOBAL_ALPHA,
						.config.globalAlpha = {N2D_GLOBAL_ALPHA_ON, N2D_GLOBAL_ALPHA_ON, 0xAA, 0xAA}};
	clipRect.config.clipRect.x = 0;
	clipRect.config.clipRect.y = 0;
	clipRect.config.clipRect.width = dst.width;
	clipRect.config.clipRect.height = dst.height;

	//4. 设置n2d_multisource_blit接口接口需要的配置参数
	for (int j = 0; j < srcNum; j++)
	{
		n2d_rectangle_t rectSrc = {0};
		n2d_rectangle_t rectDst = {0};
		n2d_int32_t x_offset = 24;
		n2d_int32_t y_offset = 32;

		switch (j)
		{
		case 0:
			//第1个输入：整个图参与 n2d_multisource_blit
			rectDst.x = 0;
			rectDst.y = 0;
			rectDst.width = dst.width;
			rectDst.height = dst.height;
			blend.config.alphablendMode = N2D_BLEND_NONE; 		//不融合，作为背景
			break;

		case 1:
			//第2个输入：从起始点(24, 208)，选择 320 *240 的矩形框，参与 n2d_multisource_blit
			rectDst.x = x_offset;
			rectDst.y = 240 - y_offset;
			rectDst.width = 320;
			rectDst.height = 240;
			blend.config.alphablendMode = N2D_BLEND_SRC_OVER;
			break;

		case 2:
			//第3个输入：从起始点(0, 0)，选择 320 *240 的矩形框，参与 n2d_multisource_blit
			rectDst.x = x_offset;
			rectDst.y = y_offset;
			rectDst.width = 320;
			rectDst.height = 240;
			blend.config.alphablendMode = N2D_BLEND_SRC_OVER;
			break;

		case 3:
			//第4个输入：从起始点(296, 32)，选择 320 *240 的矩形框，参与 n2d_multisource_blit
			rectDst.x = 320 - x_offset;
			rectDst.y = y_offset;
			rectDst.width = 320;
			rectDst.height = 240;
			blend.config.alphablendMode = N2D_BLEND_SRC_OVER;
			break;
		}
		rectSrc = rectDst;
		rectSrc.width = N2D_MIN(rectSrc.width, src[j].width - rectSrc.x);
		rectSrc.height = N2D_MAX(rectSrc.height, src[j].height - rectSrc.y);

		multisrcAndDstRect.config.multisrcAndDstRect.source = &src[j];
		multisrcAndDstRect.config.multisrcAndDstRect.srcRect = rectSrc;
		multisrcAndDstRect.config.multisrcAndDstRect.dstRect = rectDst;

		srcIndex.config.multisourceIndex = j;
		N2D_ON_ERROR(n2d_set(&srcIndex));
		N2D_ON_ERROR(n2d_set(&globalAlpha));
		N2D_ON_ERROR(n2d_set(&clipRect));
		N2D_ON_ERROR(n2d_set(&blend));
		N2D_ON_ERROR(n2d_set(&multisrcAndDstRect));
		N2D_ON_ERROR(n2d_set(&rop));
	}

	//5. 执行多源融合
	/**
	 * 	参数0x0F的含义：
	 * 	1. n2d_multisource_blit 最多支持8个输入源
	 * 	2. 每个源是否生效可以通过对应位是否位1来使能，
	 * 		比如4个输入都要生效，参数就要设置成 0x0F
	 */
	N2D_ON_ERROR(n2d_multisource_blit(&dst, 0x0F));
	N2D_ON_ERROR(n2d_commit());

	//6. 结果保存
	char output_file_name[128];
	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./multi_source_alphablend_sample_%d_%d.bmp", dst.width, dst.height);
	error = n2d_util_save_buffer_to_file(&dst, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
		goto on_error;
	}

	printf("multi_source_alphablend input file %s [%d*%d] ==> output file %s [%d*%d]\n",
		   "scenery0x", src[0].width, src[0].height,
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
	int ret = parser_params(argc, argv, &g_multi_source_alphablend_sample);
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

	if (g_multi_source_alphablend_sample.mode == 0)
	{
		error = multi_source_alphablend_sample();
		if (N2D_IS_ERROR(error))
		{
			goto on_close;
		}
	}
	else
	{
		multi_source_alphablend_performance_test(&g_multi_source_alphablend_sample);
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