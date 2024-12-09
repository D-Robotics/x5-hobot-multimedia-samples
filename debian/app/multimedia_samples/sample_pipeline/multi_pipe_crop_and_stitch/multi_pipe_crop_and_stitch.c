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
#include <ctype.h>
#include <sys/prctl.h>
#include <stdio.h>
#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#include "bpu_common.h"
#include "util.h"
#include "bpu_wraper.h"
#include "common_utils.h"
#include "param_parser.h"
#include "vp_pipeline.h"
#include "gpu_2d_wraper.h"
#include "vp_codec.h"
#include "vp_display.h"
#include "synchronous_queue.h"
#include "create_n2d_buffer_wraper.h"
#include "performance_test_util.h"

#ifdef PTS_ENABLE
#include <sys/time.h>
static uint64_t get_timestamp_us()
{
	uint64_t timestamp;
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_usec;
	return timestamp;
}
#endif

#define HDMI_DISPLAY_QUEUE_COUNT 5
#define BPU_RESULT_MAX_COUNT 4
typedef struct {
	int channel;
	int obj_count;
	pthread_mutex_t lock;
	detect_object_t objs[BPU_RESULT_MAX_COUNT]; //1个通道最多存放的目标个数
}bpu_result_t;

typedef struct {
	param_config_t param_config;
	//bpu
	bpu_handle_t bpu_handle[MAX_PIPE_NUM];
	pipe_contex_t vse_feed_back_for_bpu[MAX_PIPE_NUM];
	vp_vse_feedback_pipeline_info_t vp_vse_feedback_bpu_info[MAX_PIPE_NUM];
	bpu_result_t bpu_results[MAX_PIPE_NUM];

	int pipe_contex_need_vse[MAX_PIPE_NUM];
	pipe_contex_t pipe_contex[MAX_PIPE_NUM];
	pipe_contex_t vse_feed_back_for_hdmi;
	vp_vse_feedback_pipeline_info_t vp_vse_feedback_hdmi_info;

	//output: hdmi
	int is_hdmi_output;
	int hdmi_is_need_use_vse_scale;
	int hdmi_output_width;
	int hdmi_output_height;
	vp_drm_context_t vp_drm_context;

	//output: file
	media_codec_context_t encode_context;

	sync_queue_t vse_to_n2d;
	int sync_queue_user_flag_n2d;
	int sync_queue_user_flag_bpu_vse_fb;

	sync_queue_t n2d_to_output;

	int output_width;
	int output_height;
	int is_running;
	pthread_t get_data_from_pipeline_thread;
	pthread_t get_stitch_data_thread;
	pthread_t output_thread;
	pthread_t get_data_from_feedback_vse_thread;

	int vse_counter;
	int stitch_counter;
	int codec_counter;

	int display_frame_index;
	int n2d_frame_index;

	uint64_t hdmi_wakeup_time_us;
	int enable_isp_online; //两路都需要VSE时，才能Online

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

void vpp_graphic_buf_to_bpu_buffer_info(const hbn_vnode_image_t *src, bpu_buffer_info_t *dst)
{
	if (!src || !dst) return;

	const hbn_frame_info_t *image_info = &src->info;
	const hb_mem_graphic_buf_t *graphic_buf = &src->buffer;

	dst->plane_cnt = graphic_buf->plane_cnt;
	dst->width = graphic_buf->width;
	dst->height = graphic_buf->height;
	dst->w_stride = graphic_buf->stride;

	int plane_count = graphic_buf->plane_cnt;
	for (int i = 0; i < plane_count; ++i) {
		dst->addr[i] = graphic_buf->virt_addr[i];
		dst->paddr[i] = graphic_buf->phys_addr[i];
	}

	dst->tv.tv_sec = image_info->timestamps / 1000000000;
	dst->tv.tv_usec = (image_info->timestamps % 1000000000) / 1000;

	// Set remaining addr and paddr to NULL and 0 respectively
	for (int i = plane_count; i < MAX_GRAPHIC_BUF_COMP; ++i) {
		dst->addr[i] = NULL;
		dst->paddr[i] = 0;
	}
}

static int pipeline_file_encodec_data(sensor_outfile_config_t *outfile, hbn_vnode_image_t *chn_frame)
{
	int ret;
	if (!outfile) {
		printf("Param Empty.\n");
		return -1;
	}

	if (!outfile->enable) {
		return 0;
	}

	media_codec_context_t *media_context = &outfile->encode_context;
	media_codec_buffer_t input_buffer = {0};
	media_codec_buffer_t output_buffer = {0};
	media_codec_output_buffer_info_t info;
	// int frame_width = 3840;
	// int frame_height = 2160;

	memset(&input_buffer, 0x00, sizeof(media_codec_buffer_t));
	//input_buffer.type = MC_VIDEO_FRAME_BUFFER;
	ret = hb_mm_mc_dequeue_input_buffer(media_context, &input_buffer, 2000);
	if (ret != 0){
		printf("hb_mm_mc_dequeue_input_buffer failed ret = %d\n", ret);
		return -1;
	}

	init_media_codec_buffer_from_hbm(&input_buffer, &chn_frame->buffer);

	ret = hb_mm_mc_queue_input_buffer(media_context, &input_buffer, 2000);
	if (ret != 0){
		printf("hb_mm_mc_queue_input_buffer failed, ret = 0x%x\n", ret);
		return -1;
	}

	memset(&output_buffer, 0x00, sizeof(media_codec_buffer_t));
	memset(&info, 0x00, sizeof(media_codec_output_buffer_info_t));
	ret = vp_codec_get_output(media_context, &output_buffer, &info, 2000);
	if(ret != 0){
		printf("vp_codec_get_output failed %d.\n", ret);
		return -1;
	}

	if (outfile->enable_save_file && outfile->fp) {
		fwrite(output_buffer.vstream_buf.vir_ptr, output_buffer.vstream_buf.size, 1, outfile->fp);
	}

#ifdef PTS_ENABLE
	// outfile->cur_timestamps = output_buffer.vstream_buf.pts;
	outfile->cur_timestamps = get_timestamp_us();
	printf("file[%s] pts cnt [%ld]\n", outfile->filename, outfile->cur_timestamps - outfile->last_timestamps);
	outfile->last_timestamps = outfile->cur_timestamps;
#endif

	ret = vp_codec_release_output(media_context, &output_buffer);
	if(ret != 0){
		printf("vp_codec_release_output failed %d.\n", ret);
		return -1;
	}

	return 0;
}

static void *pipeline_codec_thread(void *context)
{
	int ret = 0;
	prctl(PR_SET_NAME, "pipeline_codec");
	char performace_test_case[64] = {0};

	sensor_outfile_config_t *outfile_cfg = (sensor_outfile_config_t *)context;
	data_item_t *data_item = NULL;
	int chn = outfile_cfg->chn;

	sprintf(performace_test_case, "chn%d_encode_%s", chn, outfile_cfg->type);
	struct PerformanceTestParamSimple performace_test_param_codec = {
		.iteration_number = 30 * 60,
		.test_case = performace_test_case,
		.run_count = 0,
		.test_count = 0,
	};

	if (!outfile_cfg->enable) {
		return NULL;
	}

	if (outfile_cfg->enable_save_file) {
		outfile_cfg->fp = fopen(outfile_cfg->filename, "w+b");
		if (NULL == outfile_cfg->fp) {
			printf("Failed to open output file: [%s]\n", outfile_cfg->filename);
			return NULL;
		}
	}

	while (outfile_cfg->is_running){

		sync_queue_t* vse_to_n2d = outfile_cfg->data_queue;
		int user_flag = outfile_cfg->queue_user_flag;
		ret = sync_queue_obtain_inused_object_width_user(vse_to_n2d, 5000, &data_item, user_flag);
		if(ret == -1){
			printf("vse feedback sync_queue_obtain_inused_object vse_to_n2d failed\n");
			break;
		}else if(ret == 1){
			// printf("vse feedback thread get same item, so ignore it. %d:%d\n", data_item->inused_frame_index, last_inused_frame_index);
			usleep(1000);
			continue;
		}else{
			//do nothing
		}

		for (size_t i = 0; i < data_item->item_count; i++){
			if (i != chn) {
				continue;
			}
			performance_test_start_simple(&performace_test_param_codec);

			// codec
			hbn_vnode_image_t *pipeline_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;
			pipeline_file_encodec_data(outfile_cfg, pipeline_chn_frame);

			performance_test_stop_simple(&performace_test_param_codec);
		}
		ret = sync_queue_repay_unused_object(vse_to_n2d, 2000, data_item);
		if(ret != 0){
			printf("sync_queue_repay_unused_object failed\n");
			break;
		}
	}

	if (outfile_cfg->fp) {
		fclose(outfile_cfg->fp);
	}
	printf("pipeline_codec_thread is exit.\n");
	return NULL;
}

static int pipeline_codec_init(int chn, const char *type, sensor_outfile_config_t *codec_config, camera_config_info_t *camera_config_info)
{
	if (!codec_config->enable) {
		return 0;
	}
	media_codec_context_t *encode_context = &codec_config->encode_context;

	codec_config->chn = chn;
	strcpy(codec_config->type, type);

	printf("Chn[%d] create codec.\n", chn);
	printf("\n\nCodec param:\n");
	printf("\tcodec type :%s\n", type);
	printf("\tcodec fps :%d\n", camera_config_info->fps);
	printf("\tcodec width :%d\n", camera_config_info->width);
	printf("\tcodec height :%d\n", camera_config_info->height);
	int ret = vp_codec_encoder_create_and_start(encode_context, camera_config_info);
	if (ret != 0){
		printf("create_encodec failed:%d\n", ret);
		return -1;
	}

	printf("\tcreate Codec Thread.\n");
	codec_config->is_running = 1;
	ret = pthread_create(&codec_config->codec_thread, NULL, (void *)pipeline_codec_thread,
									(void *)codec_config);
	if(ret != 0){
		printf("\n\nFailed: pthread_create.\n\n");
		return -1;
	}

	#if 0
	uint8_t uuid[] = "dc45e9bd-e6d948b7-962cd820-d923eeef+SEI_D-Robotics";
	uint32_t length = sizeof(uuid) / sizeof(uuid[0]);
	ret = hb_mm_mc_insert_user_data(encode_context, uuid, length);
	if (ret != 0)
	{
		printf("#### insert user data failed. ret(%d) ####\n", ret);
		return -1;
	}
	#endif
	return 0;
}

static void pipeline_codec_deinit(sensor_outfile_config_t *codec_config)
{
	if (!codec_config || !codec_config->enable) {
		return;
	}

	codec_config->is_running = 0;
	pthread_join(codec_config->codec_thread, NULL);

	media_codec_context_t *encode_context = &codec_config->encode_context;
	if (encode_context) {
		vp_codec_encoder_destroy_and_stop(encode_context);
	}
}

void *get_data_from_pipeline(void *context){
	int ret = 0;
	prctl(PR_SET_NAME, "get_data_from_pipeline");
	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;

	data_item_t *data_item = NULL;

	struct PerformanceTestParamSimple performace_total_test_param_simple = {
		.iteration_number = 30 * 60,
		.test_case = "pipeline_thread_total",
		.run_count = 0,
		.test_count = 0,

	};

	struct PerformanceTestParam performace_test_param_get_queue = {
		.iteration_number = 30 * 60,
		.test_case = "get_queue",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	struct PerformanceTestParam performace_test_param_save_queue = {
		.iteration_number = 30 * 60,
		.test_case = "save_queue",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	struct PerformanceTestParam performace_test_param_get_pipeline = {
		.iteration_number = 30 * 60,
		.test_case = "get_pipeline",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	struct PerformanceTestParam performace_test_param_get_pipeline_0 = {
		.iteration_number = 30 * 60,
		.test_case = "get_pipeline0",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	struct PerformanceTestParam performace_test_param_get_pipeline_1 = {
		.iteration_number = 30 * 60,
		.test_case = "get_pipeline1",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	while (multi_pipe_stitch_info->is_running){
		// uint64_t start_tmp = get_timestamp_ms();

		performance_test_start_simple(&performace_total_test_param_simple);

		sync_queue_t* vse_to_n2d = &multi_pipe_stitch_info->vse_to_n2d;
		performance_test_start(&performace_test_param_get_queue);
		ret = sync_queue_get_unused_object(vse_to_n2d, 5000, &data_item);
		if(ret != 0){
			break;
		}
		performance_test_stop(&performace_test_param_get_queue);

		performance_test_start(&performace_test_param_get_pipeline);
		int get_vse_frame_count = 0;
		for (int i = 0; i < param_config->sensor_config_count; i++){
			sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
			pipe_contex_t *pipe_contex = &multi_pipe_stitch_info->pipe_contex[i];

			if(i >= data_item->item_count){
				printf("sensor config index %d >= sync queue data item count %d\n", i, data_item->item_count);
				break;
			}

			int pipeline_node_channel;
			hbn_vnode_handle_t pipeline_node_handle;
			char *pipeline_node_name = NULL;
			if(multi_pipe_stitch_info->pipe_contex_need_vse[i]){
				pipeline_node_name = "VSE";
				pipeline_node_handle = pipe_contex->vse_node_handle;
				pipeline_node_channel = sensor_param_config->vse_bind_n2d_chn;
			}else{
				if(sensor_param_config->gdc_enable){
					pipeline_node_name = "GDC";
					pipeline_node_handle = pipe_contex->gdc_node_handle;
					pipeline_node_channel = 0;
				}else{
					pipeline_node_name = "ISP";
					pipeline_node_handle = pipe_contex->isp_node_handle;
					pipeline_node_channel = 0;
				}
			}

			hbn_vnode_image_t *pipeline_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;
			if(!data_item->is_init_added){
				// printf("vnode release frame.\n");
				hbn_vnode_releaseframe(pipeline_node_handle, pipeline_node_channel, pipeline_chn_frame);
			}else{
				// printf("[%d] found init added buffer\n", i);
			}

			struct PerformanceTestParam *performace_test_param = NULL;
			if(i == 0){
				performace_test_param = &performace_test_param_get_pipeline_0;
			}else if(i == 1){
				performace_test_param = &performace_test_param_get_pipeline_1;
			}else{
				printf("not support channel %d\n", i);
			}

			memset(pipeline_chn_frame, 0, sizeof(hbn_vnode_image_t));

			performance_test_start(performace_test_param);
			ret = hbn_vnode_getframe(pipeline_node_handle, pipeline_node_channel, 1000, pipeline_chn_frame);
			if (ret != 0){
				printf("[%d] hbn_vnode_getframe %s channel %d failed, error code %d, handle %ld\n",
					i, pipeline_node_name, pipeline_node_channel, ret, pipeline_node_handle);
				break;
			}

			performance_test_stop(performace_test_param);
			get_vse_frame_count++;
		}
		if(get_vse_frame_count != param_config->sensor_config_count){
			break;
		}
		performance_test_stop(&performace_test_param_get_pipeline);

		performance_test_start(&performace_test_param_save_queue);
		ret = sync_queue_save_inused_object(vse_to_n2d, 5000 /*5s: 极限情况*/, data_item);
		if(ret != 0){
			printf("sync_queue_save_inused_object vse_to_n2d failed\n");
			break;;
		}
		performance_test_stop(&performace_test_param_save_queue);

		multi_pipe_stitch_info->vse_counter++;
		performance_test_stop_simple(&performace_total_test_param_simple);
		// uint64_t stop_tmp = get_timestamp_ms();
		// printf("vflow diff:%ld\n", stop_tmp - start_tmp);
		// printf("vse:%d\n", multi_pipe_stitch_info->vse_counter);
	}

	//stop other thread
	multi_pipe_stitch_info->is_running = 0;

	printf("get_data_from_pipeline thread is exit.\n");
	return NULL;
}

int32_t vp_dump_2plane_yuv_to_file(char *filename, uint8_t *src_buffer, uint8_t *src_buffer1,
		uint32_t size, uint32_t size1)
{
	int yuv_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (yuv_fd == -1) {
		printf("Error opening file(%s)", filename);
		return -1;
	}

	ssize_t bytes_written = write(yuv_fd, src_buffer, size);
	if (bytes_written != size) {
		printf("Error writing to file");
		close(yuv_fd);
		return -1;
	}

	bytes_written = write(yuv_fd, src_buffer1, size1);
	if (bytes_written != size1) {
		printf("Error writing to file");
		close(yuv_fd);
		return -1;
	}

	close(yuv_fd);

	// SC_LOGI("Dump yuv to file(%s), size(%d) + size1(%d) succeeded\n", filename, size, size1);
	return 0;
}

void *get_data_from_feedback_vse(void *context){
	int ret = 0;
	prctl(PR_SET_NAME, "get_data_from_feedback_vse");
	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;
	param_config_t *param_config = &multi_pipe_stitch_info->param_config;

	int run_period_ms = 1000 / param_config->bpu_fps;
	data_item_t *data_item = NULL;
	struct PerformanceTestParamSimple performace_test_param_for_vse_feedback_total = {
		.iteration_number = 30 * 60,
		.test_case = "vse_feedback_total",
		.run_count = 0,
		.test_count = 0,
	};

	struct PerformanceTestParam performace_test_param_for_send_vse = {
		.iteration_number = 30 * 60 / 5,
		.test_case = "vse_feedback",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	int count = 0;
	int channel_status[MAX_PIPE_NUM];
	hbn_vnode_image_t bpu_image[MAX_PIPE_NUM];

	uint64_t last_time_ms = get_timestamp_ms();
	int sync_queue_user_flag = multi_pipe_stitch_info->sync_queue_user_flag_bpu_vse_fb;
	// int last_inused_frame_index = -1;

	printf("bpu start period is %dms\n", run_period_ms);
	while (multi_pipe_stitch_info->is_running){
		performance_test_start_simple(&performace_test_param_for_vse_feedback_total);

		sync_queue_t* vse_to_n2d = &multi_pipe_stitch_info->vse_to_n2d;
		ret = sync_queue_obtain_inused_object_width_user(vse_to_n2d, 5000, &data_item, sync_queue_user_flag);
		if(ret == -1){
			printf("vse feedback sync_queue_obtain_inused_object vse_to_n2d failed\n");
			break;
		}else if(ret == 1){
			// printf("vse feedback thread get same item, so ignore it. %d:%d\n", data_item->inused_frame_index, last_inused_frame_index);
			usleep(1000);
			continue;
		}else{
			//do nothing
		}
		// last_inused_frame_index = data_item->inused_frame_index;

		uint64_t current_time_ms = get_timestamp_ms();
		if(current_time_ms > last_time_ms + run_period_ms){
			last_time_ms = last_time_ms + run_period_ms;
			performance_test_start(&performace_test_param_for_send_vse);
			for (size_t i = 0; i < data_item->item_count; i++){
				hbn_vnode_image_t *pipeline_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;

				pipe_contex_t *bpu_vse_feedback_pipeline = &multi_pipe_stitch_info->vse_feed_back_for_bpu[i];
				int vse_channel = multi_pipe_stitch_info->vp_vse_feedback_bpu_info->vse_channel;
				ret = vp_send_to_vse_feedback(bpu_vse_feedback_pipeline, vse_channel, pipeline_chn_frame);
				if(ret != 0){
					printf("vp_send_to_vse_feedback for bpu failed, for channel [%ld]\n", i);
					channel_status[i] = 0;
					continue;
				}
				channel_status[i] = 1;
			}
			performance_test_stop(&performace_test_param_for_send_vse);
			ret = sync_queue_repay_unused_object(vse_to_n2d, 2000, data_item);
			if(ret != 0){
				printf("sync_queue_repay_unused_object failed\n");
				break;
			}

			for (size_t i = 0; i < data_item->item_count; i++){
				if(!channel_status[i]){
					continue;
				}

				pipe_contex_t *bpu_vse_feedback_pipeline = &multi_pipe_stitch_info->vse_feed_back_for_bpu[i];
				int vse_channel = multi_pipe_stitch_info->vp_vse_feedback_bpu_info->vse_channel;

				ret = vp_get_from_vse_feedback(bpu_vse_feedback_pipeline, vse_channel, &bpu_image[i]);
				if(ret != 0){
					printf("vp_get_from_vse_feedback for bpu failed.\n");
					break;
				}
#if 1
				bpu_buffer_info_t bpu_input_buffer;
				memset(&bpu_input_buffer, 0, sizeof(bpu_buffer_info_t));
				vpp_graphic_buf_to_bpu_buffer_info(&bpu_image[i], &bpu_input_buffer);
				bpu_handle_t *bpu_handle = &multi_pipe_stitch_info->bpu_handle[i];
				ret = bpu_wrap_send_frame(bpu_handle, &bpu_input_buffer);
				if(ret != 0){
					printf("send data to bpu failed\n");
				}
#else
	printf("no bpu\n");
#endif
				ret = vp_release_vse_feedback(bpu_vse_feedback_pipeline, vse_channel, &bpu_image[i]);
				if(ret != 0){
					printf("vp_release_vse_feedback failed.\n");
					break;
				}
				// char nv12_file_name[128];
				// sprintf(nv12_file_name, "./%ld_%04d.yuv", i, count);
				// vp_dump_2plane_yuv_to_file(nv12_file_name, bpu_image_hbm[i].virt_addr[0], bpu_image_hbm[i].virt_addr[1], 704 *704, 704 *704 / 2);
			}

		}else{
			usleep(1000);
			ret = sync_queue_repay_unused_object(vse_to_n2d, 2000, data_item);
			if(ret != 0){
				printf("sync_queue_repay_unused_object failed\n");
				break;
			}

		}

		performance_test_stop_simple(&performace_test_param_for_vse_feedback_total);
		count++;
	}
	multi_pipe_stitch_info->is_running = 0;
	return NULL;
}

#ifdef GPU_ENABLE
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
#if 0
	hb_mem_graphic_buf_t croped_hbm[croped_image_count];
#endif
	for (int i = 0; i < croped_image_count; i++){
#if 0
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
		.min_diff = 1000,
		.max_diff = 0,
	};
	struct PerformanceTestParam performace_test_param_get_vse_n2d_queue = {
		.iteration_number = 30 * 60,
		.test_case = "crop_and_stitch_get_vse_n2d_queue",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};
	struct PerformanceTestParam performace_test_param_get_n2d_trans = {
		.iteration_number = 30 * 60,
		.test_case = "crop_and_stitch_get_n2d_trans",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	struct PerformanceTestParam performace_test_param_save_queue = {
		.iteration_number = 30 * 60,
		.test_case = "crop_and_stitch_save_queue",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
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

	int count = 0;
	bpu_result_t *bpu_result = multi_pipe_stitch_info->bpu_results;
	int sync_queue_user_flag = multi_pipe_stitch_info->sync_queue_user_flag_n2d;
	// int last_inused_frame_index = -1;
	while (multi_pipe_stitch_info->is_running){

		performance_test_start_simple(&performace_test_param_simple);
		performance_test_start(&performace_test_param_get_vse_n2d_queue);
		//1. get source image from vse
		//uint64_t start_tmp = get_timestamp_ms();
		sync_queue_t* vse_to_n2d = &multi_pipe_stitch_info->vse_to_n2d;
		ret = sync_queue_obtain_inused_object_width_user(vse_to_n2d, 5000, &data_item, sync_queue_user_flag);
		if(ret == -1){
			printf("n2d sync_queue_obtain_inused_object vse_to_n2d failed\n");
			break;
		}else if(ret == 1){
			// printf("n2d feedback thread get same item, so ignore it. %d:%d\n", data_item->inused_frame_index, last_inused_frame_index);
			usleep(1000);
			continue;
		}else{
			//do nothing
		}
		// last_inused_frame_index = data_item->inused_frame_index;

		//uint64_t stop_tmp = get_timestamp_ms();
		// printf("n2d diff:%ld\n", stop_tmp - start_tmp);
		performance_test_stop(&performace_test_param_get_vse_n2d_queue);

		performance_test_start(&performace_test_param_get_n2d_trans);
		//1.1 wraper vse image to gpu 2d n2d_buffer
		for (int i = 0; i < data_item->item_count; i++){
			hbn_vnode_image_t *pipeline_chn_frame = ((hbn_vnode_image_t*)data_item->items) + i;
			error = create_n2d_buffer_from_hbm_graphic(&n2d_buffer[i], &pipeline_chn_frame->buffer);
			if(error != N2D_SUCCESS){
				printf("create n2d buffer from hbm graphic faield.\n");
				break;
			}
		}
		performance_test_stop(&performace_test_param_get_n2d_trans);

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
		multi_pipe_stitch_info->n2d_frame_index = n2d_data_item->index;
		if(multi_pipe_stitch_info->display_frame_index == multi_pipe_stitch_info->n2d_frame_index){
			// printf("n2d thread found overlay :%d\n", multi_pipe_stitch_info->display_frame_index);
		}
		if(param_config->bpu_postporcess_enable){
			for (int i = 0; i < data_item->item_count; i++){
				int croped_count_per_image = croped_image_count / data_item->item_count;
				pthread_mutex_lock(&bpu_result[i].lock);
				for (int j = 0; j < bpu_result[i].obj_count; j++){
					n2d_rectangle_t *rect = &crop_rect[croped_count_per_image * i + j];
					rect->x = bpu_result[i].objs[j].x;
					rect->y = bpu_result[i].objs[j].y;
					rect->width = bpu_result[i].objs[j].width;
					rect->height = bpu_result[i].objs[j].height;
				}
				pthread_mutex_unlock(&bpu_result[i].lock);
				for (int k = bpu_result[i].obj_count; k < croped_count_per_image; k++){
					n2d_rectangle_t *rect = &crop_rect[croped_count_per_image * i + k];
					rect->x = -1;
					rect->y = -1;
					rect->width = -1;
					rect->height = -1;
				}
			}
		}else{
			//do nothing
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
#if 0
			for(int j = 0; j < croped_count_per_image; j++){
				char image_name[1024];
				int width = croped[croped_count_per_image * i + j].width;
				int height = croped[croped_count_per_image * i + j].height;
				if((crop_rect[croped_count_per_image * i + j].x < 0) || (crop_rect[croped_count_per_image * i + j].width <= 0) || (crop_rect[croped_count_per_image * i + j].height <= 0) || (crop_rect[croped_count_per_image * i + j].y < 0)){
					continue;
				}
				snprintf(image_name, 1024, "%d_%d_%d_crop_%d_%d.yuv", count, i, j, width, height);
				n2d_util_save_buffer_to_vimg(&croped[croped_count_per_image * i + j], image_name);
			}
#endif
		}

		//2.2 stitch little image
		ret = gpu_2d_stitch_multi_source(croped, stitch_little_rect, croped_image_count, &stitch_dst_n2d);
		if(ret != 0){
			printf("gpu_2d_stitch_multi_source failed, croped_image_count=%d.\n", croped_image_count);
			break;
		}

		// performance_test_start(&performace_test_param_src_update);
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
		}
		n2d_free(&stitch_dst_n2d);
		multi_pipe_stitch_info->n2d_frame_index = -1;
		//4. sync queue process
		performance_test_start(&performace_test_param_save_queue);
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
		performance_test_stop(&performace_test_param_save_queue);
#if 0
		if((multi_pipe_stitch_info->is_hdmi_output) && (!multi_pipe_stitch_info->hdmi_is_need_use_vse_scale)){
			performance_test_start(&performace_test_param_hdmi);
			data_item_t *n2d_data_item_tmp = NULL;
			ret = sync_queue_obtain_inused_object(n2d_to_output, 5000, &n2d_data_item_tmp);
			if(ret != 0){
				printf("sync_queue_obtain_inused_object from n2d_to_output failed.\n");
				continue;;
			}
			ret = vp_display_set_frame(&multi_pipe_stitch_info->vp_drm_context, (hb_mem_graphic_buf_t*)n2d_data_item_tmp->items);
			if(ret != 0){
				printf("vp_display_set_frame for hdmi  failed.\n");
			}
			ret = sync_queue_repay_unused_object(n2d_to_output, 5000, n2d_data_item_tmp);
			if(ret != 0){
				printf("sync_queue_repay_unused_object from n2d_to_output failed.\n");
				break;
			}
			performance_test_stop(&performace_test_param_hdmi);
		}
#endif

		multi_pipe_stitch_info->stitch_counter++;

		if(param_config->verbose_flag){
			performance_test_stop_simple(&performace_test_param_simple);
		}
		// printf("stitch:%d\n", multi_pipe_stitch_info->stitch_counter);
		count++;
	}
	//stop other thread
	multi_pipe_stitch_info->is_running = 0;

	for (int i = 0; i < croped_image_count ; i++){
		n2d_free(&croped[i]);
#if 0
		hb_mem_free_buf(croped_hbm[i].fd[0]);
#endif
	}

	ret = gpu_2d_close();
	if(ret != 0){
		printf("gpu 2d close failed.\n");
	}

	printf("get_stitch_data thread is exit.\n");
	return NULL;
}
#endif

void *send_to_hdmi_display(void *context){
	int ret = 0;
	multi_pipe_stitch_info_t *multi_pipe_stitch_info = (multi_pipe_stitch_info_t *)context;

	// param_config_t *param_config = &multi_pipe_stitch_info->param_config;
	struct PerformanceTestParamSimple performace_total_test_param_simple = {
		.iteration_number = 30 * 60,
		.test_case = "hdmi_thread_total",
		.run_count = 0,
		.test_count = 0,
	};

	struct PerformanceTestParam performace_test_param_for_hdmi = {
		.iteration_number = 30 * 60,
		.test_case = "hdmi_thread",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	struct PerformanceTestParam performace_test_param_for_hdmi_get_queue = {
		.iteration_number = 30 * 60,
		.test_case = "hdmi_get_queue",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

#if 0
	cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(pthread_self(), sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        return NULL;
    }
#endif

	prctl(PR_SET_NAME, "send_to_hdmi_display");
	pipe_contex_t *vse_feedback_pipeline = &multi_pipe_stitch_info->vse_feed_back_for_hdmi;
	sync_queue_t* n2d_to_output = &multi_pipe_stitch_info->n2d_to_output;

	uint64_t last_processed_time_ms = get_timestamp_ms();
	int64_t data_process_delay_max_ms = -1;
	int count = 0;
	multi_pipe_stitch_info->display_frame_index = -1;

	int inused_queue_count_thresold = (3 > (HDMI_DISPLAY_QUEUE_COUNT - 1))? (HDMI_DISPLAY_QUEUE_COUNT - 1): 3;

	printf("hdmi thread inused queue threold is %d.\n", inused_queue_count_thresold);
	while (multi_pipe_stitch_info->is_running){
		data_item_t *data_item = NULL;
		performance_test_start_simple(&performace_total_test_param_simple);
		performance_test_start(&performace_test_param_for_hdmi_get_queue);

		if(n2d_to_output->inused_queue_count >= inused_queue_count_thresold){
			printf("hdmi process too slow, so drop frame: %d >= %d\n", n2d_to_output->inused_queue_count, inused_queue_count_thresold);
			ret = sync_queue_obtain_inused_object(n2d_to_output, 5000, &data_item);
			if(ret != 0){
				printf("sync_queue_obtain_inused_object from n2d_to_output failed.\n");
				continue;;
			}
			ret = sync_queue_repay_unused_object(n2d_to_output, 5000, data_item);
			if(ret != 0){
				printf("sync_queue_repay_unused_object from n2d_to_output failed.\n");
				break;
			}
			continue;
		}

		ret = sync_queue_obtain_inused_object(n2d_to_output, 5000, &data_item);
		if(ret != 0){
			printf("sync_queue_obtain_inused_object from n2d_to_output failed.\n");
			continue;;
		}
		if(data_item->item_count != 1){
			printf("sync_queue_obtain_inused_object get data_item->itemcount is error %d. \n", data_item->item_count);
			break;
		}
		performance_test_stop(&performace_test_param_for_hdmi_get_queue);

		int64_t data_process_delay_ms = get_timestamp_ms() - data_item->inused_update_time_ms;
		if(data_process_delay_ms >= 20){
			if(last_processed_time_ms > data_process_delay_ms){
				// printf("hdmi process delay %ld, because last hdmi process delay %ld\n",
				// 	data_process_delay_ms, last_processed_time_ms - data_process_delay_ms);
			}else{
				printf("hdmi process delay %ld.\n", data_process_delay_ms);
			}

		}
		if(data_process_delay_ms > data_process_delay_max_ms){
			data_process_delay_max_ms = data_process_delay_ms;
			printf("hdmi process delay max time is updated :%ldms.\n", data_process_delay_ms);
		}

		if((multi_pipe_stitch_info->display_frame_index == multi_pipe_stitch_info->n2d_frame_index) &&
			(multi_pipe_stitch_info->display_frame_index != -1)){
			printf("display thread found overlay :%d\n", multi_pipe_stitch_info->display_frame_index);
		}

		if((multi_pipe_stitch_info->hdmi_is_need_use_vse_scale)){
			hbn_vnode_image_t vse_src_image;
			hbn_vnode_image_t vse_resized_image;
			performance_test_start(&performace_test_param_for_hdmi);
			hb_mem_graphic_buf_t *hb_mem_graphic_buf = (hb_mem_graphic_buf_t*)data_item->items;
			vse_src_image.buffer = *hb_mem_graphic_buf;
			int vse_channel = multi_pipe_stitch_info->vp_vse_feedback_hdmi_info.vse_channel;
			ret = vp_send_to_vse_feedback(vse_feedback_pipeline, vse_channel, &vse_src_image);
			if(ret != 0){
				printf("vp_send_to_vse_feedback for hdmi failed.\n");
				break;
			}
			ret = vp_get_from_vse_feedback(vse_feedback_pipeline, vse_channel, &vse_resized_image);
			if(ret != 0){
				printf("vp_get_from_vse_feedback for hdmi  failed.\n");
				break;
			}
			ret = vp_display_set_frame(&multi_pipe_stitch_info->vp_drm_context, &vse_resized_image.buffer);
			if(ret != 0){
				printf("vp_display_set_frame for hdmi failed %d.\n", ret);
			}
			multi_pipe_stitch_info->display_frame_index = data_item->index;

			ret = vp_release_vse_feedback(vse_feedback_pipeline, vse_channel, &vse_resized_image);
			if(ret != 0){
				printf("vp_release_vse_feedback for hdmi  failed.\n");
				break;
			}
			performance_test_stop(&performace_test_param_for_hdmi);
		}else{
			performance_test_start(&performace_test_param_for_hdmi);
			ret = vp_display_set_frame(&multi_pipe_stitch_info->vp_drm_context, (hb_mem_graphic_buf_t*)data_item->items);
			if(ret != 0){
				printf("vp_display_set_frame for hdmi failed %d.\n", ret);
			}
			performance_test_stop(&performace_test_param_for_hdmi);
			multi_pipe_stitch_info->display_frame_index = data_item->index;
		}

#if 0
		if(count % 10 == 0){
			char output_file_name[128];
			hb_mem_graphic_buf_t *hb_mem_graphic_buf_tmp = (hb_mem_graphic_buf_t*)data_item->items;
			memset(output_file_name, 0, sizeof(output_file_name));
			sprintf(output_file_name, "/tmp/%d_3840_2160_nv12.yuv", count);
			dump_image_to_file(output_file_name, hb_mem_graphic_buf_tmp->virt_addr[0],
				hb_mem_graphic_buf_tmp->size[0] + hb_mem_graphic_buf_tmp->size[1]);
		}
#endif
		last_processed_time_ms = get_timestamp_ms();
		ret = sync_queue_repay_unused_object(n2d_to_output, 5000, data_item);
		if(ret != 0){
			printf("sync_queue_repay_unused_object from n2d_to_output failed.\n");
			break;
		}
		count++;
		if(count % 60 == 0){
			count = 0;
		}
		performance_test_stop_simple(&performace_total_test_param_simple);
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

	struct PerformanceTestParamSimple performace_total_test_param_simple = {
		.iteration_number = 30 * 60,
		.test_case = "codec_thread_total",
		.run_count = 0,
		.test_count = 0,
	};

	struct PerformanceTestParam performace_test_param_for_codec = {
		.iteration_number = 30 * 60,
		.test_case = "codec_thread",
		.run_count = 0,
		.consumu_time_sum_us = 0,
		.test_count = 0,
		.min_diff = 1000,
		.max_diff = 0,
	};

	FILE *fp_output = fopen(param_config->output_file_name, "w+b");
	if (NULL == fp_output) {
		printf("Failed to open output file: [%s]\n", param_config->output_file_name);
		return NULL;
	}

	sync_queue_t* n2d_to_output = &multi_pipe_stitch_info->n2d_to_output;

	while (multi_pipe_stitch_info->is_running){
		performance_test_start_simple(&performace_total_test_param_simple);
		data_item_t *data_item = NULL;
		ret = sync_queue_obtain_inused_object(n2d_to_output, 5000, &data_item);
		if(ret != 0){
			printf("save file: sync_queue_obtain_inused_object from n2d_to_output failed.\n");
			break;
		}
		if(data_item->item_count != 1){
			printf("sync_queue_obtain_inused_object get data_item->itemcount is error %d. \n", data_item->item_count);
			break;
		}

		performance_test_start(&performace_test_param_for_codec);
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
		performance_test_stop(&performace_test_param_for_codec);

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
		performance_test_stop_simple(&performace_total_test_param_simple);
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

int bpu_cb(detect_object_array_t* result, void *userdata){
	if(result == NULL){
		return -1;
	}
	bpu_result_t *bpu_result = (bpu_result_t *)userdata;
	pthread_mutex_lock(&bpu_result->lock);
	bpu_result->obj_count = 0;
	for (int i = 0; i < result->valid_count; i++){

		if(strcmp(result->detect_objects[i].label, "person") == 0){
			bpu_result->objs[bpu_result->obj_count] = result->detect_objects[i];
			bpu_result->obj_count++;
			// printf("bpu result: %d  [%s]\n", bpu_result->obj_count, result->detect_objects[i].label);
		}

		if(bpu_result->obj_count >= BPU_RESULT_MAX_COUNT){
			break;
		}

	}
	pthread_mutex_unlock(&bpu_result->lock);

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
		.queue_len = PILELINE_OUT_BUFFER_COUNT - PILELINE_OUT_BUFFER_RELEASE_COUNT,
		.data_item_size = sizeof(hbn_vnode_image_t),
		.data_item_count = param_config->sensor_config_count,

		.item_data_init_param = NULL,
		.item_data_init_func = NULL,
		.item_data_deinit_param = NULL,
		.item_data_deinit_func = NULL,
	};
	ret = sync_queue_create_multi_user(&multi_pipe_stitch_info->vse_to_n2d, &sync_queue_info_vse);
	if(ret < 0){
		printf("sync queue create failed for vse.\n");
		return -1;
	}

#ifdef GPU_ENABLE
	if (param_config->gpu_enable) {
		multi_pipe_stitch_info->sync_queue_user_flag_n2d = sync_queue_add_user(&multi_pipe_stitch_info->vse_to_n2d);
	}
#endif

	const stitch_item_data_init_param_t stitch_item_data_init_param = {
		.width = multi_pipe_stitch_info->output_width,
		.height = multi_pipe_stitch_info->output_height,
	};

	sync_queue_info_t sync_queue_info_n2d = {
		.productor_name = "n2d",
		.consumer_name = "codec",

		.is_need_malloc_in_advance = 0,
		.is_external_buffer = 0,

		.queue_len = HDMI_DISPLAY_QUEUE_COUNT,
		.data_item_size = sizeof(hb_mem_graphic_buf_t),
		.data_item_count = 1,

		.item_data_init_param = (void*)&stitch_item_data_init_param,
		.item_data_init_func = stitch_sync_queue_item_data_init_func,

		.item_data_deinit_param = NULL,
		.item_data_deinit_func = stitch_sync_queue_item_data_deinit_func,

	};
	ret = sync_queue_create(&multi_pipe_stitch_info->n2d_to_output, &sync_queue_info_n2d);
	if(ret != 0){
		printf("sync queue create failed for n2d.\n");
		return -1;
	}

	if(param_config->bpu_enable){
		//vse 在其他线程完成
		multi_pipe_stitch_info->sync_queue_user_flag_bpu_vse_fb = sync_queue_add_user(&multi_pipe_stitch_info->vse_to_n2d);

		for (int i = 0; i < param_config->sensor_config_count; i++){
			bpu_handle_t *bpu_handle = &multi_pipe_stitch_info->bpu_handle[i];
			sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
			ret = bpu_wrap_model_init(bpu_handle, "yolov5s");
			if(ret != 0){
				printf("[bpu_wrap_model_init] vp create and start pipeline for camera [%d] [%s]failed\n",
					i, sensor_param_config->sensor_config->camera_config->name);
				return -1;
			}
			vp_vse_feedback_pipeline_info_t vp_vse_feedback_bpu_info = {
				.input_width = 3840,
				.input_height = 2160,
				.output_width = bpu_handle->m_image_info.m_model_w,
				.output_height = bpu_handle->m_image_info.m_model_h,
				.vse_channel = 0,
				.pipeline_id = i,
			};
			multi_pipe_stitch_info->vp_vse_feedback_bpu_info[i] = vp_vse_feedback_bpu_info;
			ret = vp_create_start_vse_feedback_pieline(&multi_pipe_stitch_info->vse_feed_back_for_bpu[i], &vp_vse_feedback_bpu_info);
			if(ret != 0){
				printf("\n\nFailed: vp_create_start_vse_feedback_pieline for bpu.\n\n");
				return -1;
			}
			multi_pipe_stitch_info->bpu_results[i].channel = i;

			bpu_wrap_callback_register(bpu_handle, bpu_cb, (void *)&multi_pipe_stitch_info->bpu_results[i]);

			bpu_handle->post_processs_enable = param_config->bpu_postporcess_enable;
			ret = bpu_wrap_start(bpu_handle);
			if(ret != 0){
				printf("[bpu_wrap_start] vp create and start pipeline for camera [%d] [%s]failed\n",
					i, sensor_param_config->sensor_config->camera_config->name);
				return -1;
			}
		}
	}

	int min_fps = 100;
	//2. start pipeline
	printf("[2] start pipeline.\n");
	for (int i = 0; i < param_config->sensor_config_count; i++){
		sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
		pipe_contex_t *pipe_contex = &multi_pipe_stitch_info->pipe_contex[i];

		//2.1 init pipe_contex
		pipe_contex->sensor_config = sensor_param_config->sensor_config;
		pipe_contex->csi_config = sensor_param_config->csi_config;
		vp_pipeline_info_t vp_pipeline_info = {
			.channel = i,
			.active_mipi_host = sensor_param_config->active_mipi_host,
			.vse_bind_index = sensor_param_config->vse_bind_n2d_chn,
			.sensor_mode = sensor_param_config->sensor_mode,
			.enable_gdc = sensor_param_config->gdc_enable,
			.enable_online = multi_pipe_stitch_info->enable_isp_online,
			.enable_vse = multi_pipe_stitch_info->pipe_contex_need_vse[i],
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

		//2.2 init codec_contex
		if (sensor_param_config->h264_outfile.enable) {
			camera_config_info_t camera_config_info = {
				.width = 3840,
				.height = 2160,
				.fps = 30,
				.encode_type = MEDIA_CODEC_ID_H264,
			};

			sensor_param_config->h264_outfile.data_queue = &multi_pipe_stitch_info->vse_to_n2d;
			sensor_param_config->h264_outfile.queue_user_flag = sync_queue_add_user(&multi_pipe_stitch_info->vse_to_n2d);
			pipeline_codec_init(i, "h264", &sensor_param_config->h264_outfile, &camera_config_info);
		}

		if (sensor_param_config->mjpeg_outfile.enable) {
			camera_config_info_t camera_config_info = {
				.width = 3840,
				.height = 2160,
				.fps = 30,
				.encode_type = MEDIA_CODEC_ID_MJPEG,
			};
			sensor_param_config->mjpeg_outfile.data_queue = &multi_pipe_stitch_info->vse_to_n2d;
			sensor_param_config->mjpeg_outfile.queue_user_flag = sync_queue_add_user(&multi_pipe_stitch_info->vse_to_n2d);
			pipeline_codec_init(i, "mjpeg", &sensor_param_config->mjpeg_outfile, &camera_config_info);
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
		if((multi_pipe_stitch_info->hdmi_output_width != 3840) || (multi_pipe_stitch_info->hdmi_output_height != 2160)){
			vp_vse_feedback_pipeline_info_t vp_vse_feedback_hdmi_info = {
				.input_width = multi_pipe_stitch_info->output_width,
				.input_height = multi_pipe_stitch_info->output_height,
				.output_width = multi_pipe_stitch_info->hdmi_output_width,
				.output_height = multi_pipe_stitch_info->hdmi_output_height,
				.vse_channel = 0,
			};
			multi_pipe_stitch_info->hdmi_is_need_use_vse_scale = 1;
			ret = vp_create_start_vse_feedback_pieline(&multi_pipe_stitch_info->vse_feed_back_for_hdmi, &vp_vse_feedback_hdmi_info);
			if(ret != 0){
				printf("\n\nFailed: vp_create_start_vse_feedback_pieline.\n\n");
				return -1;
			}
			multi_pipe_stitch_info->vp_vse_feedback_hdmi_info = vp_vse_feedback_hdmi_info;
		}else{
			multi_pipe_stitch_info->hdmi_is_need_use_vse_scale = 0;
			printf("hdmi support 4K resolution, so dont use feedback vse\n");
		}
	}

	//4. start all thread
	multi_pipe_stitch_info->is_running = 1;
	ret = pthread_create(&multi_pipe_stitch_info->get_data_from_pipeline_thread, NULL, (void *)get_data_from_pipeline,
							(void *)multi_pipe_stitch_info);
	ERR_CON_EQ(ret, 0);
#ifdef GPU_ENABLE
	if (param_config->gpu_enable) {
		ret = pthread_create(&multi_pipe_stitch_info->get_stitch_data_thread, NULL, (void *)get_stitch_data,
								(void *)multi_pipe_stitch_info);
		ERR_CON_EQ(ret, 0);
	}
#endif

	//设置为实时线程
	pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    param.sched_priority = 99;
    pthread_attr_setschedparam(&attr, &param);
	errno = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if(errno != 0){
		perror("setinherit failed\n");
		return -1;
	}
#if 0
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(3,&cpu_set);
	if (pthread_attr_setaffinity_np(&attr,sizeof(cpu_set_t),&cpu_set) != 0){
		printf("pthread_attr_setaffinity_np fail\n");
		return -1;
	}
#endif
	if(strcmp(param_config->output, "file") == 0){
		multi_pipe_stitch_info->is_hdmi_output = 0;
		ret = pthread_create(&multi_pipe_stitch_info->output_thread, &attr, (void *)get_codec_data_save_file,
									(void *)multi_pipe_stitch_info);
	}else if(strcmp(param_config->output, "hdmi") == 0){
		multi_pipe_stitch_info->is_hdmi_output = 1;
		ret = pthread_create(&multi_pipe_stitch_info->output_thread, &attr, (void *)send_to_hdmi_display,
							(void *)multi_pipe_stitch_info);
	}else{
		printf("Nothing output\n");
	}
	ERR_CON_EQ(ret, 0);

	if(param_config->bpu_enable){
		ret = pthread_create(&multi_pipe_stitch_info->get_data_from_feedback_vse_thread, NULL, (void *)get_data_from_feedback_vse,
							(void *)multi_pipe_stitch_info);
		ERR_CON_EQ(ret, 0);
	}
	return 0;
}
int pipeline_stop(multi_pipe_stitch_info_t *multi_pipe_stitch_info){
	int ret = 0;

	//1. wait thread stop
	multi_pipe_stitch_info->is_running = 0;
	pthread_join(multi_pipe_stitch_info->get_data_from_pipeline_thread, NULL);

#ifdef GPU_ENABLE
	if (multi_pipe_stitch_info->param_config.gpu_enable) {
		pthread_join(multi_pipe_stitch_info->get_stitch_data_thread, NULL);
		pthread_join(multi_pipe_stitch_info->output_thread, NULL);
	}
#endif
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

		// 4.1 deinit pipeline codec
		sensor_param_config_t* sensor_param_config = &param_config->sensor_param_config[i];
		pipeline_codec_deinit(&sensor_param_config->h264_outfile);
		pipeline_codec_deinit(&sensor_param_config->mjpeg_outfile);

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
	if(param_config->bpu_enable){
		for (int i = 0; i < param_config->sensor_config_count; i++){
			bpu_handle_t *bpu_handle = &multi_pipe_stitch_info->bpu_handle[i];
			bpu_wrap_stop(bpu_handle);
		}
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
		.enable_isp_online = 0,
	};
	for (size_t i = 0; i < MAX_PIPE_NUM; i++){
		pthread_mutex_init(&multi_pipe_stitch_info.bpu_results[i].lock, NULL);
		multi_pipe_stitch_info.bpu_results[i].obj_count = 0;
	}

	param_config_t *param_config = &multi_pipe_stitch_info.param_config;
	ret = param_process(argc, argv, param_config);
	if(ret != 0){
		return -1;
	}
	ret = check_camera_config(&multi_pipe_stitch_info.param_config,
		multi_pipe_stitch_info.pipe_contex_need_vse,
		&multi_pipe_stitch_info.enable_isp_online);
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