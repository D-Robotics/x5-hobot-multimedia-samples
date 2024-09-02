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
#include <sys/prctl.h>

#include "common_utils.h"
#include "param_parser.h"
#include "vp_pipeline.h"
#include "gpu_2d_wraper.h"
#include "vp_codec.h"
#include "vp_display.h"
#include "synchronous_queue.h"
#include "create_n2d_buffer_wraper.h"
#include "performance_test_util.h"

typedef struct {
	param_config_t param_config;

	pipe_contex_t pipe_contex[MAX_PIPE_NUM];
	pipe_contex_t vse_feed_back_pipeline;
	vp_vse_feedback_pipeline_info_t vp_vse_pipeline_info;

	//output: hdmi
	int hdmi_output_width;
	int hdmi_output_height;
	vp_drm_context_t vp_drm_context;

	//output: file
	media_codec_context_t encode_context;

	sync_queue_t vse_to_n2d;
	sync_queue_t n2d_to_output;

	int output_width;
	int output_height;
	int is_running;
	pthread_t get_vse_data_thread;
	pthread_t get_stitch_data_thread;
	pthread_t output_thread;

	int vse_counter;
	int stitch_counter;
	int codec_counter;

} multi_pipe_stitch_info_t;

int init_media_codec_buffer_from_hbm(media_codec_buffer_t *dst_codec_buffer, hb_mem_graphic_buf_t *src_hbm_graphic){
	int data_size = src_hbm_graphic->size[0];
	if(src_hbm_graphic->plane_cnt == 2){
		data_size += src_hbm_graphic->size[1];
	}

	dst_codec_buffer->type = MC_VIDEO_FRAME_BUFFER;
	dst_codec_buffer->vframe_buf.width = src_hbm_graphic->width;
	dst_codec_buffer->vframe_buf.height = src_hbm_graphic->height;
	dst_codec_buffer->vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
	dst_codec_buffer->vframe_buf.size = data_size;
	// dst_codec_buffer->vframe_buf.pts = hbn_vnode_image->info.timestamps / 1000;
	dst_codec_buffer->vframe_buf.pts = 0;
	dst_codec_buffer->vframe_buf.vir_ptr[0] = src_hbm_graphic->virt_addr[0];
	dst_codec_buffer->vframe_buf.vir_ptr[1] = src_hbm_graphic->virt_addr[1];
	dst_codec_buffer->vframe_buf.phy_ptr[0] = src_hbm_graphic->phys_addr[0];
	dst_codec_buffer->vframe_buf.phy_ptr[1] = src_hbm_graphic->phys_addr[1];
	dst_codec_buffer->user_ptr = NULL;

	return 0;
}
int init_media_codec_buffer_from_n2d_buffer(media_codec_buffer_t *dst_codec_buffer, n2d_buffer_t *src_n2d_buffer){

	dst_codec_buffer->type = MC_VIDEO_FRAME_BUFFER;
	dst_codec_buffer->vframe_buf.width = src_n2d_buffer->width;
	dst_codec_buffer->vframe_buf.height = src_n2d_buffer->height;
	dst_codec_buffer->vframe_buf.pix_fmt = MC_PIXEL_FORMAT_NV12;
	dst_codec_buffer->vframe_buf.size = src_n2d_buffer->width * src_n2d_buffer->height * 1.5;

	// dst_codec_buffer->vframe_buf.pts = hbn_vnode_image->info.timestamps / 1000;
	dst_codec_buffer->vframe_buf.vir_ptr[0] = src_n2d_buffer->uv_memory[0];
	dst_codec_buffer->vframe_buf.vir_ptr[1] = src_n2d_buffer->uv_memory[1];
	dst_codec_buffer->vframe_buf.phy_ptr[0] = src_n2d_buffer->uv_gpu[0];
	dst_codec_buffer->vframe_buf.phy_ptr[1] = src_n2d_buffer->uv_gpu[1];
	dst_codec_buffer->user_ptr = NULL;

	return 0;
}

void *get_vse_data(void *context){
	int ret = 0;
	prctl(PR_SET_NAME, "get_vse_data");
	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;

	data_item_t *data_item = NULL;
	while (multi_pipe_stitch_info->is_running){

		sync_queue_t* vse_to_n2d = &multi_pipe_stitch_info->vse_to_n2d;
		ret = sync_queue_get_unused_object(vse_to_n2d, 5000, &data_item);
		if(ret != 0){
			break;
		}

		int get_vse_frame_count = 0;
		for (int i = 0; i < param_config->sensor_config_count; i++){
			sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
			pipe_contex_t *pipe_contex = &multi_pipe_stitch_info->pipe_contex[i];
			hbn_vnode_handle_t vse_node_handle = pipe_contex->vse_node_handle;

			if(i >= data_item->item_count){
				printf("sensor config index %d >= sync queue data item count %d\n", i, data_item->item_count);
				break;
			}

			hbn_vnode_image_t *vse_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;
			memset(vse_chn_frame, 0, sizeof(hbn_vnode_image_t));
			ret = hbn_vnode_getframe(vse_node_handle, sensor_param_config->vse_bind_n2d_chn, 1000, vse_chn_frame);
			if (ret != 0){
				printf("hbn_vnode_getframe VSE channel %d failed, error code %d, vse handle %ld\n",
					sensor_param_config->vse_bind_n2d_chn, ret, vse_node_handle);
				break;
			}
			get_vse_frame_count++;
		}
		if(get_vse_frame_count != param_config->sensor_config_count){
			break;
		}

		ret = sync_queue_save_inused_object(vse_to_n2d, 5000 /*5s: 极限情况*/, data_item);
		if(ret != 0){
			printf("sync_queue_get_unused_object vse_to_n2d failed\n");
			break;;
		}
		multi_pipe_stitch_info->vse_counter++;

		// printf("vse:%d\n", multi_pipe_stitch_info->vse_counter);
	}

	//stop other thread
	multi_pipe_stitch_info->is_running = 0;

	printf("get_vse_data thread is exit.\n");
	return NULL;
}

void *get_stitch_data(void *context){
	int ret = 0;
	n2d_error_t error;
	n2d_buffer_t n2d_buffer[MAX_PIPE_NUM];

	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;
	ret = gpu_2d_open();
	if(ret != 0){
		printf("gpu 2d open failed.\n");
		return NULL;
	}
	prctl(PR_SET_NAME, "get_stitch_data");


	//0. prepare buffer

	//0.1 prepare little image param
	//stitch: (3840 - 8 * 3) / 4 = 954    (2160 - 9 * 2) / 3  = 714
	//crop: 	954 / 6 * 5 = 795 (640)	       714 / 6 * 5 = 595 (480)
	const int x_duration = 8;
	const int y_duration = 9;
	const int little_image_croped_width = 640;
	const int little_image_croped_height = 480;
	const int little_image_width = (multi_pipe_stitch_info->output_width - x_duration * 3) / 4;
	const int little_image_height = (multi_pipe_stitch_info->output_height - y_duration * 2) / 3;

	//0.2 prepare croped n2d_buffer
	const int croped_image_count = 8;
	n2d_buffer_t croped[croped_image_count];
	hb_mem_graphic_buf_t croped_hbm[croped_image_count];
	for (int i = 0; i < croped_image_count; i++){
#if 1
		int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
		ret = hb_mem_alloc_graph_buf(little_image_croped_width, little_image_croped_height,
			MEM_PIX_FMT_NV12, flags, 0, 0, &croped_hbm[i]);
		if(ret != 0){
			printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
			return NULL;
		}
		error = create_n2d_buffer_from_hbm_graphic(&croped[i], &croped_hbm[i]);
		if (N2D_IS_ERROR(error)){
			printf("n2d_util_allocate_buffer failed! error=%d.\n", error);
			return NULL;
		}
#else
	error = n2d_util_allocate_buffer(
			little_image_croped_width,
			little_image_croped_height,
			N2D_NV12,
			N2D_0,
			N2D_LINEAR,
			N2D_TSC_DISABLE,
			&croped[i]);

		if (N2D_IS_ERROR(error)){
			printf("n2d_util_allocate_buffer failed! error=%d.\n", error);
			return NULL;
		}

	#endif
	}
	//0.3 prepare crop rect
	n2d_rectangle_t crop_rect[croped_image_count];
	int crop_rect_index = 0;
	int croped_count_per_image = croped_image_count / param_config->sensor_config_count;
	for (int i = 0; i < param_config->sensor_config_count; i++){
		for (size_t j = 0; j < croped_count_per_image; j++)
		{
			const float x_array[4] = {0, 0, 0.75, 0.75};
			const float y_array[4] = {0, 0.75, 0, 0.75};
			crop_rect[crop_rect_index].x = x_array[j % 4] * multi_pipe_stitch_info->output_width;
			crop_rect[crop_rect_index].y = y_array[j % 4] * multi_pipe_stitch_info->output_height;
			crop_rect[crop_rect_index].width = little_image_croped_width;
			crop_rect[crop_rect_index].height = little_image_croped_height;
			crop_rect_index++;
		}
	}
	//0.4 prepare stitch:little image rect
	n2d_rectangle_t stitch_little_rect[croped_image_count];
	for (int i = 0; i < croped_image_count ; i++){
		stitch_little_rect[i].x = i / 2 * (little_image_width + x_duration);
		stitch_little_rect[i].y = i % 2 * (little_image_height + y_duration);
		stitch_little_rect[i].width = little_image_width;
		stitch_little_rect[i].height = little_image_height;
	}

	struct PerformanceTestParamSimple performace_test_param_simple = {
		.iteration_number = 30 * 60,
		.test_case = "crop_and_stitch_total_thread",
		.run_count = 0,
		.test_count = 0,
	};
	struct PerformanceTestParam performace_test_param = {
		.iteration_number = 30 * 60,
		.test_case = "crop_and_stitch",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
	};

	//0.5 prepare stitch:src image rect
	int src_image_start_y = (little_image_height + y_duration) * 2;
	int src_image_duration_x = multi_pipe_stitch_info->output_width / param_config->sensor_config_count;
	n2d_rectangle_t stitch_src_image_rect[croped_image_count]; //2或4
	for (int i = 0; i < param_config->sensor_config_count; i++){
		stitch_src_image_rect[i].x = i * src_image_duration_x;
		//画布决定： 画布只是 4K的部分 设置为0，如果是整个4k, 应该设置成 src_image_start_y
		stitch_src_image_rect[i].y = src_image_start_y;
		stitch_src_image_rect[i].width = src_image_duration_x;
		stitch_src_image_rect[i].height = little_image_height;
	}

	//0.6 prepare Color space conversion
	int src_image_expend_x = src_image_duration_x * param_config->blend_ratio;
	n2d_buffer_t dst_rgba8888_n2d;
	n2d_rectangle_t blend_src_image_rect[croped_image_count]; //2或4
	if(param_config->blend_ratio != 0){
		int src_img_stitch_width = src_image_expend_x;
		int src_img_stitch_height = little_image_height;
		error = n2d_util_allocate_buffer(
				src_img_stitch_width,
				src_img_stitch_height, 				// 只拼接一部分
				N2D_BGRA8888,
				N2D_0,
				N2D_LINEAR,
				N2D_TSC_DISABLE,
				&dst_rgba8888_n2d);

		if (N2D_IS_ERROR(error)){
			printf("n2d_util_allocate_buffer failed! error=%d.\n", error);
			return NULL;
		}
		int blend_width = multi_pipe_stitch_info->output_width * param_config->blend_ratio;

		for (int i = 0; i < param_config->sensor_config_count; i++){
			if(i % 2 == 0){
				blend_src_image_rect[i].x = multi_pipe_stitch_info->output_width - blend_width;
			}else{
				blend_src_image_rect[i].x = blend_width;
			}

			blend_src_image_rect[i].y = 0;
			blend_src_image_rect[i].width = blend_width;
			blend_src_image_rect[i].height = multi_pipe_stitch_info->output_height;
		}
	}


	data_item_t *data_item = NULL;
	while (multi_pipe_stitch_info->is_running){

		performance_test_start_simple(&performace_test_param_simple);
		//1. get source image from vse
		sync_queue_t* vse_to_n2d = &multi_pipe_stitch_info->vse_to_n2d;
		ret = sync_queue_obtain_inused_object(vse_to_n2d, 5000, &data_item);
		if(ret != 0){
			printf("sync_queue_obtain_inused_object vse_to_n2d failed\n");
			break;
		}

		//1.1 wraper vse image to gpu 2d n2d_buffer
		for (int i = 0; i < data_item->item_count; i++){
			hbn_vnode_image_t *vse_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;
			error = create_n2d_buffer_from_hbm_graphic(&n2d_buffer[i], &vse_chn_frame->buffer);
			if(error != N2D_SUCCESS){
				printf("create n2d buffer from hbm graphic faield.\n");
				break;
			}
		}

		//2. get stitch destination buffer
		data_item_t *n2d_data_item = NULL;
		sync_queue_t* n2d_to_output = &multi_pipe_stitch_info->n2d_to_output;
		ret = sync_queue_get_unused_object(n2d_to_output, 5000, &n2d_data_item);
		if(ret != 0){
			printf("sync_queue_get_unused_object from n2d_to_output failed.\n");
			break;
		}

		performance_test_start(&performace_test_param);
		hb_mem_graphic_buf_t *stitch_dst_hbm = ((hb_mem_graphic_buf_t*)n2d_data_item->items);
		n2d_buffer_t stitch_dst_n2d;
		error = create_n2d_buffer_from_hbm_graphic(&stitch_dst_n2d, stitch_dst_hbm);
		if (N2D_IS_ERROR(error)){
			printf("create_n2d_buffer_from_hbm_graphic failed! error=%d.\n", error);
			break;
		}

		//2.1 crop eight little image
		for (int i = 0; i < data_item->item_count; i++){
			int croped_count_per_image = croped_image_count / data_item->item_count;
			ret = gpu_2d_crop_multi_rects(&n2d_buffer[i],
				&crop_rect[croped_count_per_image * i], croped_count_per_image, &croped[croped_count_per_image * i]);
			if(ret != 0){
				printf("gpu_2d_crop_multi_rects failed i = %d, croped_count_per_image=%d.\n", i, croped_count_per_image);
				break;
			}
		}

		//2.2 stitch little image
		ret = gpu_2d_stitch_multi_source(croped, stitch_little_rect, croped_image_count, &stitch_dst_n2d);
		if(ret != 0){
			printf("gpu_2d_stitch_multi_source failed, croped_image_count=%d.\n", croped_image_count);
			break;
		}

		ret = gpu_2d_stitch_multi_source(n2d_buffer, stitch_src_image_rect, data_item->item_count, &stitch_dst_n2d);
		if(ret != 0){
			printf("gpu_2d_stitch_multi_source failed, croped_image_count=%d.\n", croped_image_count);
			break;
		}

		if(param_config->blend_ratio != 0){
			//2.3 stitch src image
			ret = gpu_2d_stitch_multi_source_blend(n2d_buffer, blend_src_image_rect, data_item->item_count, &dst_rgba8888_n2d);
			if(ret != 0){
				printf("gpu_2d_stitch_multi_source failed, croped_image_count=%d.\n", croped_image_count);
				break;
			}

			n2d_rectangle_t dst_positions = {
				.x = src_image_duration_x - src_image_expend_x / 2,
				.y = src_image_start_y,
				.width = dst_rgba8888_n2d.width,
				.height = dst_rgba8888_n2d.height,
			};
			ret = gpu_2d_format_convert(&stitch_dst_n2d, &dst_rgba8888_n2d, 1, &dst_positions);
			if(ret != 0){
				printf("gpu_2d_format_convert failed\n");
				break;
			}
		}
		if(param_config->verbose_flag){
			performance_test_stop(&performace_test_param);
		}

		//3. release wrapered n2d_buffer form hbn memory
		for (int i = 0; i < data_item->item_count ; i++){
			n2d_free(&n2d_buffer[i]);
			sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
			pipe_contex_t *pipe_contex = &multi_pipe_stitch_info->pipe_contex[i];
			hbn_vnode_handle_t vse_node_handle = pipe_contex->vse_node_handle;
			hbn_vnode_image_t *vse_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;
			hbn_vnode_releaseframe(vse_node_handle, sensor_param_config->vse_bind_n2d_chn, vse_chn_frame);
		}
		n2d_free(&stitch_dst_n2d);

		//4. sync queue process
		ret = sync_queue_save_inused_object(n2d_to_output, 2000, n2d_data_item);
		if(ret != 0){
			printf("sync_queue_save_inused_object n2d_to_output failed\n");
			break;
		}
		ret = sync_queue_repay_unused_object(vse_to_n2d, 2000, data_item);
		if(ret != 0){
			printf("sync_queue_repay_unused_object failed\n");
			break;
		}
		multi_pipe_stitch_info->stitch_counter++;

		if(param_config->verbose_flag){
			performance_test_stop_simple(&performace_test_param_simple);
		}
		// printf("stitch:%d\n", multi_pipe_stitch_info->stitch_counter);

	}
	//stop other thread
	multi_pipe_stitch_info->is_running = 0;

	for (int i = 0; i < croped_image_count ; i++){
		n2d_free(&croped[i]);
		hb_mem_free_buf(croped_hbm[i].fd[0]);
	}

	ret = gpu_2d_close();
	if(ret != 0){
		printf("gpu 2d close failed.\n");
	}

	printf("get_stitch_data thread is exit.\n");
	return NULL;
}


void *send_to_hdmi_display(void *context){
	int ret = 0;
	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;

	// param_config_t *param_config = &multi_pipe_stitch_info->param_config;

	prctl(PR_SET_NAME, "send_to_hdmi_display");
	int vse_channel =multi_pipe_stitch_info->vp_vse_pipeline_info.vse_channel;
	pipe_contex_t *vse_feedback_pipeline = &multi_pipe_stitch_info->vse_feed_back_pipeline;
	sync_queue_t* n2d_to_output = &multi_pipe_stitch_info->n2d_to_output;
	while (multi_pipe_stitch_info->is_running){
		data_item_t *data_item = NULL;
		ret = sync_queue_obtain_inused_object(n2d_to_output, 5000, &data_item);
		if(ret != 0){
			printf("sync_queue_obtain_inused_object from n2d_to_output failed.\n");
			break;
		}
		if(data_item->item_count != 1){
			printf("sync_queue_obtain_inused_object get data_item->itemcount is error %d. \n", data_item->item_count);
			break;
		}

		hbn_vnode_image_t vse_src_image;
		hbn_vnode_image_t vse_resized_image;
		hb_mem_graphic_buf_t *hb_mem_graphic_buf = (hb_mem_graphic_buf_t*)data_item->items;
		vse_src_image.buffer = *hb_mem_graphic_buf;
		ret = vp_send_to_vse_feedback(vse_feedback_pipeline, vse_channel, &vse_src_image);
		if(ret != 0){
			printf("vp_send_to_vse_feedback failed.\n");
			break;
		}
		ret = vp_get_from_vse_feedback(vse_feedback_pipeline, vse_channel, &vse_resized_image);
		if(ret != 0){
			printf("vp_get_from_vse_feedback failed.\n");
			break;
		}

		ret = vp_display_set_frame(&multi_pipe_stitch_info->vp_drm_context, &vse_resized_image.buffer);
		if(ret != 0){
			printf("vp_display_set_frame failed.\n");
			break;
		}

		ret = vp_release_vse_feedback(vse_feedback_pipeline, vse_channel, &vse_resized_image);
		if(ret != 0){
			printf("vp_release_vse_feedback failed.\n");
			break;
		}

		ret = sync_queue_repay_unused_object(n2d_to_output, 5000, data_item);
		if(ret != 0){
			printf("sync_queue_repay_unused_object from n2d_to_output failed.\n");
			break;
		}
	}
	multi_pipe_stitch_info->is_running = 0;

	printf("send_to_hdmi_display thread is exit.\n");
	return NULL;
}

void *get_codec_data_save_file(void *context){
	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;
	media_codec_context_t *media_context = &multi_pipe_stitch_info->encode_context;
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;

	prctl(PR_SET_NAME, "get_codec_data_save_file");

	uint8_t uuid[] = "dc45e9bd-e6d948b7-962cd820-d923eeef+SEI_D-Robotics";
	uint32_t length = sizeof(uuid) / sizeof(uuid[0]);
	int ret = hb_mm_mc_insert_user_data(media_context, uuid, length);
	if (ret != 0)
	{
		printf("#### insert user data failed. ret(%d) ####\n", ret);
		return NULL;
	}

	FILE *fp_output = fopen(param_config->output_file_name, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: [%s]\n", param_config->output_file_name);
		return NULL;
	}

	sync_queue_t* n2d_to_output = &multi_pipe_stitch_info->n2d_to_output;

	while (multi_pipe_stitch_info->is_running){
		data_item_t *data_item = NULL;
		ret = sync_queue_obtain_inused_object(n2d_to_output, 5000, &data_item);
		if(ret != 0){
			printf("sync_queue_obtain_inused_object from n2d_to_output failed.\n");
			break;
		}
		if(data_item->item_count != 1){
			printf("sync_queue_obtain_inused_object get data_item->itemcount is error %d. \n", data_item->item_count);
			break;
		}
		media_codec_buffer_t codec_buffer;;
		codec_buffer.type = MC_VIDEO_FRAME_BUFFER;
		ret = hb_mm_mc_dequeue_input_buffer(&multi_pipe_stitch_info->encode_context, &codec_buffer, 2000);
		if (ret != 0){
			printf("hb_mm_mc_dequeue_input_buffer failed ret = %d\n", ret);
			break;
		}

		init_media_codec_buffer_from_hbm(&codec_buffer, (hb_mem_graphic_buf_t*)data_item->items);
		ret = hb_mm_mc_queue_input_buffer(&multi_pipe_stitch_info->encode_context, &codec_buffer, 2000);
		if (ret != 0){
			printf("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
			break;
		}

		media_codec_buffer_t encode_buffer;
		media_codec_output_buffer_info_t info;
		ret = vp_codec_get_output(&multi_pipe_stitch_info->encode_context, &encode_buffer, &info, 2000);
		if(ret != 0){
			printf("vp_codec_get_output failed %d.\n", ret);
			break;
		}

		if (fp_output) {
			fwrite(encode_buffer.vstream_buf.vir_ptr, encode_buffer.vstream_buf.size, 1, fp_output);
		}
		ret = vp_codec_release_output(&multi_pipe_stitch_info->encode_context, &encode_buffer);
		if(ret != 0){
			printf("vp_codec_release_output failed %d.\n", ret);
			break;
		}

		ret = sync_queue_repay_unused_object(n2d_to_output, 5000, data_item);
		if(ret != 0){
			printf("sync_queue_repay_unused_object from n2d_to_output failed.\n");
			break;
		}
		multi_pipe_stitch_info->codec_counter++;
		// printf("codec:%d\n", multi_pipe_stitch_info->codec_counter);
	}
	if (fp_output) {
		fclose(fp_output);
	}

	//stop other thread
	multi_pipe_stitch_info->is_running = 0;

	printf("get_codec_data_save_file thread is exit.\n");
	return NULL;
}
typedef struct stitch_item_data_init_param_s{
	int width;
	int height;
}stitch_item_data_init_param_t;

static int stitch_sync_queue_item_data_init_func(void *param, void *item_data){
	hb_mem_graphic_buf_t *item_data_structed = (hb_mem_graphic_buf_t*)item_data;
	stitch_item_data_init_param_t *init_param = (stitch_item_data_init_param_t*)param;

	int64_t flags = HB_MEM_USAGE_CPU_READ_OFTEN | HB_MEM_USAGE_CPU_WRITE_OFTEN
				| HB_MEM_USAGE_CACHED |HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF;
	int ret = hb_mem_alloc_graph_buf(init_param->width, init_param->height, MEM_PIX_FMT_NV12, flags, 0, 0, item_data_structed);
	if(ret != 0){
		printf("hb_mem_alloc_graph_buf failed :%d\n", ret);
		return -1;
	}
	//初始化为 黑图
	memset(item_data_structed->virt_addr[0], 0, item_data_structed->size[0]);
	memset(item_data_structed->virt_addr[1], 128, item_data_structed->size[1]);

	ret = hb_mem_flush_buf_with_vaddr((uint64_t)item_data_structed->virt_addr[0], item_data_structed->size[0]);
	if (ret < 0) {
		hb_mem_free_buf(item_data_structed->fd[0]);
		printf("hb_mem_flush_buf_with_vaddr[0] failed :%d\n", ret);
		return -1;
	}
	ret = hb_mem_flush_buf_with_vaddr((uint64_t)item_data_structed->virt_addr[1], item_data_structed->size[1]);
	if (ret < 0) {
		hb_mem_free_buf(item_data_structed->fd[0]);
		printf("hb_mem_flush_buf_with_vaddr[1] failed :%d\n", ret);
		return -1;
	}

	return 0;
}
static int stitch_sync_queue_item_data_deinit_func(void *param, void *item_data){
	hb_mem_graphic_buf_t *item_data_structed = (hb_mem_graphic_buf_t*)item_data;
	hb_mem_free_buf(item_data_structed->fd[0]);
	return 0;
}

int pipeline_start(multi_pipe_stitch_info_t *multi_pipe_stitch_info){
	int ret = 0;
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;
	if((param_config->sensor_config_count != 2) && (param_config->sensor_config_count != 4)){
		printf("unsupport sensor count %d\n", param_config->sensor_config_count);
		exit(-1);
	}

	//1. queue
	printf("[1] create queue.\n");
	sync_queue_info_t sync_queue_info_vse = {
		.productor_name = "vse",
		.consumer_name = "n2d",
		.is_need_malloc_in_advance = 1,
		.is_external_buffer = 0,
		.queue_len = 6,
		.data_item_size = sizeof(hbn_vnode_image_t),
		.data_item_count = param_config->sensor_config_count,

		.item_data_init_param = NULL,
		.item_data_init_func = NULL,
		.item_data_deinit_param = NULL,
		.item_data_deinit_func = NULL,
	};
	ret = sync_queue_create(&multi_pipe_stitch_info->vse_to_n2d, &sync_queue_info_vse);

	const stitch_item_data_init_param_t stitch_item_data_init_param = {
		.width = multi_pipe_stitch_info->output_width,
		.height = multi_pipe_stitch_info->output_height,
	};

	sync_queue_info_t sync_queue_info_n2d = {
		.productor_name = "n2d",
		.consumer_name = "codec",

		.is_need_malloc_in_advance = 0,
		.is_external_buffer = 0,

		.queue_len = 6,
		.data_item_size = sizeof(hb_mem_graphic_buf_t),
		.data_item_count = 1,

		.item_data_init_param = (void*)&stitch_item_data_init_param,
		.item_data_init_func = stitch_sync_queue_item_data_init_func,

		.item_data_deinit_param = NULL,
		.item_data_deinit_func = stitch_sync_queue_item_data_deinit_func,

	};
	ret = sync_queue_create(&multi_pipe_stitch_info->n2d_to_output, &sync_queue_info_n2d);

	int min_fps = 100;
	//2. start pipeline
	printf("[2] start pipeline.\n");
	for (int i = 0; i < param_config->sensor_config_count; i++){
		sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
		pipe_contex_t *pipe_contex = &multi_pipe_stitch_info->pipe_contex[i];

		//3.1 init pipe_contex
		pipe_contex->sensor_config = sensor_param_config->sensor_config;
		pipe_contex->csi_config = sensor_param_config->csi_config;
		vp_pipeline_info_t vp_pipeline_info = {
			.active_mipi_host = sensor_param_config->active_mipi_host,
			.vse_bind_index = sensor_param_config->vse_bind_n2d_chn,
			.sensor_mode = sensor_param_config->sensor_mode,
			.enable_gdc = param_config->gdc_enable,
			.sensor_name = sensor_param_config->sensor_config->camera_config->name,
			.camera_config_info = {
				.width = multi_pipe_stitch_info->output_width,
				.height = multi_pipe_stitch_info->output_height,
				.fps = sensor_param_config->sensor_config->camera_config->fps
			}
		};
		if(sensor_param_config->sensor_config->camera_config->fps < min_fps){
			min_fps = sensor_param_config->sensor_config->camera_config->fps;
		}

		ret = vp_create_and_start_pipeline(pipe_contex, &vp_pipeline_info);
		if(ret != 0){
			printf("vp create and start pipeline for camera [%d] [%s]failed\n",
				i, sensor_param_config->sensor_config->camera_config->name);
			return -1;
		}
	}

	//3. output init
	if(strcmp(param_config->output, "file") == 0){
		camera_config_info_t camera_config_info = {
			.width = multi_pipe_stitch_info->output_width,
			.height = multi_pipe_stitch_info->output_height,
			.fps = min_fps,
			.encode_type = MEDIA_CODEC_ID_H265,
		};
		printf("create codec.\n");
		printf("\n\nCodec param:\n");
		printf("\tcodec type :H265\n");
		printf("\tcodec fps :%d\n", min_fps);
		printf("\tcodec width :%d\n", camera_config_info.width);
		printf("\tcodec height :%d\n", camera_config_info.height);

		media_codec_context_t *encode_context = &multi_pipe_stitch_info->encode_context;
		ret = vp_codec_encoder_create_and_start(encode_context, &camera_config_info);
		if (ret != 0){
			printf("create_encodec failed:%d\n", ret);
			return -1;
		}
	}else if(strcmp(param_config->output, "hdmi") == 0){
		ret = vp_display_check_hdmi_is_connected();
		if(ret == 0){
			printf("\n\nFailed: output form is hdmi, but not found hdmi connector.\n\n");
			return -1;
		}
		ret = vp_display_get_max_resolution_if_not_match(
			multi_pipe_stitch_info->output_width, multi_pipe_stitch_info->output_height,
			&multi_pipe_stitch_info->hdmi_output_width, &multi_pipe_stitch_info->hdmi_output_height);
		if(ret == 1){
			printf("hdmi support resolution %d*%d\n", multi_pipe_stitch_info->output_width, multi_pipe_stitch_info->output_height);
		}else if(ret == 0){
			printf("\nWarn !!! hdmi not found resolution %d*%d, usr max resolution %d*%d\n\n",
				multi_pipe_stitch_info->output_width, multi_pipe_stitch_info->output_height,
				multi_pipe_stitch_info->hdmi_output_width, multi_pipe_stitch_info->hdmi_output_height);
		}else{
			printf("hdmi not found appropriate resolution\n");
			return -1;
		}
		ret = vp_display_init(&multi_pipe_stitch_info->vp_drm_context,
			multi_pipe_stitch_info->hdmi_output_width, multi_pipe_stitch_info->hdmi_output_height);
		if(ret != 0){
			printf("hdmi init failed.\n");
			return -1;
		}

		vp_vse_feedback_pipeline_info_t vp_vse_pipeline_info = {
			 .input_width = multi_pipe_stitch_info->output_width,
			 .input_height = multi_pipe_stitch_info->output_height,
			 .output_width = multi_pipe_stitch_info->hdmi_output_width,
			 .output_height = multi_pipe_stitch_info->hdmi_output_height,
			 .vse_channel = 0,
		};

		ret = vp_create_start_vse_feedback_pieline(&multi_pipe_stitch_info->vse_feed_back_pipeline, &vp_vse_pipeline_info);
		if(ret != 0){
			printf("\n\nFailed: vp_create_start_vse_feedback_pieline.\n\n");
			return -1;
		}
		multi_pipe_stitch_info->vp_vse_pipeline_info = vp_vse_pipeline_info;
	}

	//4. start all thread
	multi_pipe_stitch_info->is_running = 1;
	ret = pthread_create(&multi_pipe_stitch_info->get_vse_data_thread, NULL, (void *)get_vse_data,
							(void *)multi_pipe_stitch_info);
	ERR_CON_EQ(ret, 0);
	ret = pthread_create(&multi_pipe_stitch_info->get_stitch_data_thread, NULL, (void *)get_stitch_data,
							(void *)multi_pipe_stitch_info);
	ERR_CON_EQ(ret, 0);
	if(strcmp(param_config->output, "file") == 0){
		ret = pthread_create(&multi_pipe_stitch_info->output_thread, NULL, (void *)get_codec_data_save_file,
									(void *)multi_pipe_stitch_info);
	}else if(strcmp(param_config->output, "hdmi") == 0){
		ret = pthread_create(&multi_pipe_stitch_info->output_thread, NULL, (void *)send_to_hdmi_display,
									(void *)multi_pipe_stitch_info);
	}else{
		printf("unspport output form:%s\n", param_config->output);
		return -1;
	}

	ERR_CON_EQ(ret, 0);

	return 0;
}
int pipeline_stop(multi_pipe_stitch_info_t *multi_pipe_stitch_info){
	int ret = 0;

	//1. wait thread stop
	multi_pipe_stitch_info->is_running = 0;
	pthread_join(multi_pipe_stitch_info->get_vse_data_thread, NULL);
	pthread_join(multi_pipe_stitch_info->get_stitch_data_thread, NULL);
	pthread_join(multi_pipe_stitch_info->output_thread, NULL);
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;

	//2. codec deinit
	if(strcmp(param_config->output, "file") == 0){
		media_codec_context_t *encode_context = &multi_pipe_stitch_info->encode_context;
		vp_codec_encoder_destroy_and_stop(encode_context);
	}else if(strcmp(param_config->output, "hdmi")){
		vp_display_deinit(&multi_pipe_stitch_info->vp_drm_context);
	}

	//4. pipeline stop
	for (size_t i = 0; i < param_config->sensor_config_count; i++){
		pipe_contex_t *pipe_contex = &multi_pipe_stitch_info->pipe_contex[i];
		vp_destroy_and_stop_pipeline(pipe_contex);
	}

	//5. destroy sync queue
	ret = sync_queue_destory(&multi_pipe_stitch_info->vse_to_n2d);
	if(ret != 0){
		printf("sync_queue_destory vse to n2d faield.\n");
		return -1;
	}

	ret = sync_queue_destory(&multi_pipe_stitch_info->n2d_to_output);
	if(ret != 0){
		printf("sync_queue_destory vse to n2d faield.\n");
		return -1;
	}

	return 0;
}
int main(int argc, char** argv) {
	int ret = 0;

	multi_pipe_stitch_info_t multi_pipe_stitch_info = {
		.output_width = 3840,
		.output_height = 2160,

		.vse_counter = 0,
		.stitch_counter = 0,
		.codec_counter = 0,
	};

	param_config_t *param_config = &multi_pipe_stitch_info.param_config;
	ret = param_process(argc, argv, param_config);
	if(ret != 0){
		return -1;
	}
	ret = check_camera_config(&multi_pipe_stitch_info.param_config);
	if(ret != 0){
		printf("camera param is invalid, so return.\n");
		return -1;
	}

	hb_mem_module_open();

	ret = pipeline_start(&multi_pipe_stitch_info);
	if(ret != 0){
		printf("pipeline start failed \n");
		goto hbm_close;
	}
	printf("\n\ninput q to stop :");
	char option = 'a';
	while ((option=getchar()) != EOF) {
		if(option == 'q'){
			printf("start to stop pipeline.\n");
			break;
		}
	}

	pipeline_stop(&multi_pipe_stitch_info);

hbm_close:
	hb_mem_module_close();
	return 0;
}