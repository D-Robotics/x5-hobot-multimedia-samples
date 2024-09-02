/***************************************************************************
 *					  create_n2d_bufferRIGHT NOTICE
 *			 create_n2d_bufferright(C) 2024, D-Robotics Co., Ltd.
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
#include "create_n2d_buffer_wraper.h"

struct PerformanceTestParam g_create_n2d_buffer_sample = {
	.mode = 0,
	.iteration_number = 10000,
	.image_width = 640,
	.image_height = 480,
	.test_case = "create_n2d_buffer"
};

void create_n2d_buffer_performance_test(struct PerformanceTestParam *param)
{

	n2d_buffer_t src = {0};
	n2d_buffer_t dst = {0};
	n2d_error_t error = N2D_SUCCESS;
	/*
		flags 必须是 HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF
		1. NV12的数据是 2个plane
		2. 每个 plane 都会对应一个物理地址
		3. 设置了HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF，保证 两个物理地址是连续的
	*/
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;

	hb_mem_module_open();

	//1. src: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	int src_width = param->image_width;
	int src_height = param->image_height;
	hb_mem_graphic_buf_t hbn_mem_src;
	int ret = hb_mem_alloc_graph_buf(src_width, src_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_src);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_error;
	}

	error = create_n2d_buffer_from_hbm_graphic(&src, &hbn_mem_src);
	if (N2D_IS_ERROR(error)){
		printf("create_n2d_buffer_from_hbm_graphic failed! error=%d.\n", error);
		goto on_error;
	}
	error = performance_test_add_rect(param, &src, TOP_LEFT, n2d_red);
	if (N2D_IS_ERROR(error)){
		printf("performance_test_add_rect failed! error=%d.\n", error);
		goto on_error;
	}

	//2. dst: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	int dst_width = src.width;
	int dst_height = src.height;
	hb_mem_graphic_buf_t hbn_mem_dst;
	ret = hb_mem_alloc_graph_buf(dst_width, dst_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_dst);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_error;
	}
	error = create_n2d_buffer_from_hbm_graphic(&dst, &hbn_mem_dst);
	if (N2D_IS_ERROR(error)){
		printf("create_n2d_buffer_from_hbm_graphic failed! error=%d.\n", error);
		goto on_error;
	}

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
		N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, N2D_BLEND_NONE));
		N2D_ON_ERROR(n2d_commit());
		run_count++;
	}
	performance_test_stop_with_resolution_info(param, &res_info);

	char performance_test_file_name[128];

	//注意！！！： 目前不支持 将转换后的 n2d_buffer 通过 n2d_util_save_buffer_to_vimg 去保存文件
#if 0
	memset(performance_test_file_name, 0, sizeof(performance_test_file_name));
	sprintf(performance_test_file_name, "%d_%d_create_n2d_buffer_to_%d_%d",
		src.width, src.height, dst.width, dst.height);
	performance_test_save_to_file_width_name(param, &dst, performance_test_file_name);
#else
	memset(performance_test_file_name, 0, sizeof(performance_test_file_name));
	sprintf(performance_test_file_name, "./performance_test_%d_%d_create_n2d_buffer_to_%d_%d.nv12",
		src.width, src.height, dst.width, dst.height);
	ret = dump_image_to_file(performance_test_file_name, hbn_mem_dst.virt_addr[0], dst.width*dst.height*1.5);
	if(ret != 0){
		printf("save file %s failed.\n", performance_test_file_name);
		goto on_error;
	}
#endif
on_error:
	if (N2D_INVALID_HANDLE != src.handle)
	{
		n2d_free(&src);
	}

	if (N2D_INVALID_HANDLE != dst.handle)
	{
		n2d_free(&dst);
	}
	hb_mem_module_close();
}

n2d_error_t create_n2d_buffer_and_copy_sample()
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_buffer_t src = {0};

	hb_mem_module_open();

	//1. src: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	int src_width = 1920;
	int src_height = 1080;
	hb_mem_graphic_buf_t hbn_mem_src;
	memset(&hbn_mem_src, 0, sizeof(hb_mem_graphic_buf_t));
	/*
		flags 必须是 HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF
		1. NV12的数据是 2个plane
		2. 每个 plane 都会对应一个物理地址
		3. 设置了HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF，保证 两个物理地址是连续的
	*/
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	int ret = hb_mem_alloc_graph_buf(src_width, src_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_src);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto hb_mem_close;
	}

	char *input_file_name = "../resource/nv12_1920x1080.yuv";
	ret = read_nv12_image_to_graphic_buffer(input_file_name, &hbn_mem_src, src_width, src_height);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_free_src_hbm;
	}

	error = create_n2d_buffer_from_hbm_graphic(&src, &hbn_mem_src);
	if (N2D_IS_ERROR(error)){
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_src_hbm;
	}

	//2. dst: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	n2d_buffer_t dst = {0};
	int dst_width = src.width;
	int dst_height = src.height;
	hb_mem_graphic_buf_t hbn_mem_dst;
	ret = hb_mem_alloc_graph_buf(dst_width, dst_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_dst);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_free_src;
	}
	error = create_n2d_buffer_from_hbm_graphic(&dst, &hbn_mem_dst);
	if (N2D_IS_ERROR(error)){
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_dst_hbm;
	}

	//3. 执行内存拷贝
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

	//注意！！！： 目前不支持 将转换后的 n2d_buffer 通过 n2d_util_save_buffer_to_vimg 去保存文件
#if 0
	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./create_n2d_buffer_sample_n2d_%d_%d.yuv", dst.width, dst.height);
	error = n2d_util_save_buffer_to_vimg(&dst, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
		goto on_error;
	}
	printf("create_n2d_buffer input file %s [%d*%d] ==> output file %s [%d*%d]\n",
		input_file_name, src.width, src.height,
		output_file_name, dst.width, dst.height);
#endif

	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./create_n2d_buffer_copy_sample_hbn_%d_%d.yuv", dst.width, dst.height);
	ret = dump_image_to_file(output_file_name, hbn_mem_dst.virt_addr[0], dst.width*dst.height*1.5);
	if(ret != 0){
		printf("save file %s failed.\n", output_file_name);
		goto on_error;
	}
on_error:
	n2d_free(&dst);
on_free_dst_hbm:
	hb_mem_free_buf(hbn_mem_dst.fd[0]);
on_free_src:
	n2d_free(&src);
on_free_src_hbm:
	hb_mem_free_buf(hbn_mem_src.fd[0]);
hb_mem_close:
	hb_mem_module_close();
	return error;
}

n2d_error_t create_n2d_buffer_and_stitch_sample()
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_buffer_t src = {0};

	hb_mem_module_open();

	//1. src: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	int src_width = 1920;
	int src_height = 1080;
	hb_mem_graphic_buf_t hbn_mem_src;
	memset(&hbn_mem_src, 0, sizeof(hb_mem_graphic_buf_t));
	/*
		flags 必须是 HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF
		1. NV12的数据是 2个plane
		2. 每个 plane 都会对应一个物理地址
		3. 设置了HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF，保证 两个物理地址是连续的
	*/
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	int ret = hb_mem_alloc_graph_buf(src_width, src_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_src);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto hb_mem_close;
	}

	char *input_file_name = "../resource/nv12_1920x1080.yuv";
	ret = read_nv12_image_to_graphic_buffer(input_file_name, &hbn_mem_src, src_width, src_height);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_free_src_hbm;
	}

	error = create_n2d_buffer_from_hbm_graphic(&src, &hbn_mem_src);
	if (N2D_IS_ERROR(error)){
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_src_hbm;
	}

	//2. dst: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	n2d_buffer_t dst = {0};
	int dst_width = src.width;
	int dst_height = src.height * 2;
	hb_mem_graphic_buf_t hbn_mem_dst;
	ret = hb_mem_alloc_graph_buf(dst_width, dst_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_dst);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_free_src;
	}
	error = create_n2d_buffer_from_hbm_graphic(&dst, &hbn_mem_dst);
	if (N2D_IS_ERROR(error)){
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_dst_hbm;
	}

	//3. 拼接 上半部分（src 放到 dst 的上半部分）
	n2d_rectangle_t dst_rect;
	dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width  = dst.width;
    dst_rect.height = dst.height /2 ;
	N2D_ON_ERROR(n2d_blit(&dst, &dst_rect, &src, NULL, N2D_BLEND_NONE));

	//3. 拼接 下半部分（src 放到 dst 的下半部分）
	dst_rect.x = 0;
    dst_rect.y = dst.height / 2 -1;
    dst_rect.width  = dst.width;
    dst_rect.height = dst.height / 2;
	N2D_ON_ERROR(n2d_blit(&dst, &dst_rect, &src, NULL, N2D_BLEND_NONE));

	// 提交命令并等待执行完成
	N2D_ON_ERROR(n2d_commit());

	// 保存图片
	char output_file_name[128];

	//注意！！！： 目前不支持 将转换后的 n2d_buffer 通过 n2d_util_save_buffer_to_vimg 去保存文件
#if 0
	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./create_n2d_buffer_sample_n2d_%d_%d.yuv", dst.width, dst.height);
	error = n2d_util_save_buffer_to_vimg(&dst, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
		goto on_error;
	}
	printf("create_n2d_buffer input file %s [%d*%d] ==> output file %s [%d*%d]\n",
		input_file_name, src.width, src.height,
		output_file_name, dst.width, dst.height);
#endif

	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./create_n2d_buffer_stitch_sample_hbn_%d_%d.yuv", dst.width, dst.height);
	ret = dump_image_to_file(output_file_name, hbn_mem_dst.virt_addr[0], dst.width*dst.height*1.5);
	if(ret != 0){
		printf("save file %s failed.\n", output_file_name);
		goto on_error;
	}
on_error:
	n2d_free(&dst);
on_free_dst_hbm:
	hb_mem_free_buf(hbn_mem_dst.fd[0]);
on_free_src:
	n2d_free(&src);
on_free_src_hbm:
	hb_mem_free_buf(hbn_mem_src.fd[0]);
hb_mem_close:
	hb_mem_module_close();
	return error;
}

n2d_error_t create_n2d_buffer_from_normal_memory_sample()
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_buffer_t src = {0};
	int ret = 0;

	hb_mem_module_open();

	//1. src: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	int src_width = 1920;
	int src_height = 1080;

	uint8_t *normal_memory_addr = (uint8_t*)malloc(src_width * src_height * 1.5);
	if(normal_memory_addr == NULL){
		printf("malloc failed.\n");
		goto hb_mem_close;
	}
	char *input_file_name = "../resource/nv12_1920x1080.yuv";
	ret = read_nv12_image_to_normal_memory(input_file_name, normal_memory_addr, src_width, src_height);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_free_src_normal_memory;
	}

	hb_mem_graphic_buf_t hbn_mem_src;
	ret = create_hbm_graphic_buffer_from_normal_memory(&hbn_mem_src,
		src_width, src_height, MEM_PIX_FMT_NV12, normal_memory_addr);
	if(ret != 0){
		printf("create_hbm_graphic_buffer_from_normal_memory failed :%d\n", ret);
		goto on_free_src_normal_memory;
	}

	error = create_n2d_buffer_from_hbm_graphic(&src, &hbn_mem_src);
	if (N2D_IS_ERROR(error)){
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_src_hbm;
	}

	//2. dst: 基于 hb_mem_graphic_buf_t 创建 n2d_buffer_t
	n2d_buffer_t dst = {0};
	int dst_width = src.width;
	int dst_height = src.height;
	hb_mem_graphic_buf_t hbn_mem_dst;
	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
			| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	ret = hb_mem_alloc_graph_buf(dst_width, dst_height, MEM_PIX_FMT_NV12, flags, 0, 0, &hbn_mem_dst);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		goto on_free_src;
	}
	error = create_n2d_buffer_from_hbm_graphic(&dst, &hbn_mem_dst);
	if (N2D_IS_ERROR(error)){
		printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
		goto on_free_dst_hbm;
	}

	//3. 执行内存拷贝
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
	//注意！！！： 目前不支持 将转换后的 n2d_buffer 通过 n2d_util_save_buffer_to_vimg 去保存文件
#if 0
	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./create_n2d_buffer_sample_n2d_%d_%d.yuv", dst.width, dst.height);
	error = n2d_util_save_buffer_to_vimg(&dst, output_file_name);
	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
		goto on_error;
	}
	printf("create_n2d_buffer input file %s [%d*%d] ==> output file %s [%d*%d]\n",
		input_file_name, src.width, src.height,
		output_file_name, dst.width, dst.height);
#endif

	memset(output_file_name, 0, sizeof(output_file_name));
	sprintf(output_file_name, "./create_n2d_buffer_normal_memory_sample_hbn_%d_%d.yuv", dst.width, dst.height);
	ret = dump_image_to_file(output_file_name, hbn_mem_dst.virt_addr[0], dst.width*dst.height*1.5);
	if(ret != 0){
		printf("save file %s failed.\n", output_file_name);
		goto on_error;
	}
on_error:
	n2d_free(&dst);
on_free_dst_hbm:
	hb_mem_free_buf(hbn_mem_dst.fd[0]);
on_free_src:
	n2d_free(&src);
on_free_src_hbm:
	hb_mem_free_buf(hbn_mem_src.fd[0]);
on_free_src_normal_memory:
	free(normal_memory_addr);
hb_mem_close:
	hb_mem_module_close();
	return error;
}

int main(int argc, char **argv)
{
	int ret = parser_params(argc, argv, &g_create_n2d_buffer_sample);
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

	if (g_create_n2d_buffer_sample.mode == 0)
	{
		//将hbm 转换成 n2d_buffer 后执行内存拷贝
		error = create_n2d_buffer_and_copy_sample();
		if (N2D_IS_ERROR(error)){
			goto on_close;
		}

		//将hbm 转换成 n2d_buffer 后执行 拼接
		error = create_n2d_buffer_and_stitch_sample();
		if (N2D_IS_ERROR(error)){
			goto on_close;
		}

		//将malloc申请的内存，先拷贝到hbm，再转换成 n2d_buffer，执行内存拷贝
		error = create_n2d_buffer_from_normal_memory_sample();
		if (N2D_IS_ERROR(error)){
			goto on_close;
		}
	}
	else
	{
		create_n2d_buffer_performance_test(&g_create_n2d_buffer_sample);
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