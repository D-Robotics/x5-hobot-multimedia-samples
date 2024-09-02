#include <sys/time.h>
#include "performance_test_util.h"

const n2d_color_t n2d_blue = N2D_COLOR_BGRA8(0x80, 0x00, 0x00, 0xff);
const n2d_color_t n2d_black = N2D_COLOR_BGRA8(0x80, 0x00, 0x00, 0x00);
const n2d_color_t n2d_green = N2D_COLOR_BGRA8(0x80, 0x00, 0xff, 0x00);
const n2d_color_t n2d_red = N2D_COLOR_BGRA8(0x80, 0xff, 0x00, 0x00);
const n2d_color_t n2d_white = N2D_COLOR_BGRA8(0x80, 0xff, 0xff, 0xff);
const n2d_color_t n2d_grey = N2D_COLOR_BGRA8(0x80, 0x0f, 0x0f, 0x0f);
const n2d_color_t n2d_light_grey = N2D_COLOR_BGRA8(0x80, 0x20, 0x20, 0x20);

uint64_t get_timestamp_ms()
{
	uint64_t timestamp;
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_usec / 1000;
	return timestamp;
}

uint64_t get_timestamp_us()
{
	uint64_t timestamp;
	struct timeval ts;

	gettimeofday(&ts, NULL);
	timestamp = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_usec;
	return timestamp;
}
int n2d_buffer_is_yuv(n2d_buffer_format_t format){
	int is_yuv = 0;
	switch(format)
	{
		case N2D_AYUV:
		case N2D_RGB888_PLANAR:
		case N2D_YV12:
		case N2D_I420:
		case N2D_NV21:
		case N2D_NV12:
		case N2D_NV21_10BIT:
		case N2D_NV12_10BIT:
		case N2D_NV61:
		case N2D_NV16:
			is_yuv = 1;
			break;

		default:
			is_yuv = 0;
			break;
	}
	return is_yuv;
}

char *convert_format_to_string(n2d_buffer_format_t	format)
{
	switch(format)
	{
		case N2D_AYUV:
			return "ayuv";
		case N2D_RGB888_PLANAR:
			return "bgr_planer";
		case N2D_YV12:
			return "yv12";
		case N2D_I420:
			return "I420";
		case N2D_NV21:
			return "nv21";
		case N2D_NV12:
			return "nv12";
		case N2D_NV21_10BIT:
			return "nv21_10bit";
		case N2D_NV12_10BIT:
			return "nv12_10bit";
		case N2D_NV61:
			return "nv61";
		case N2D_NV16:
			return "nv16";
		case N2D_RGB888:
			return "rgb888";
		case N2D_BGRA8888:
			return "rgba888";
		default:
			return "unsupport";
	}
	return "unsupport";
}

void print_help(char *test_case)
{
	printf("Usage: %s [OPTIONS]\n", test_case);
	printf("Options:\n");
	printf("  -m <mode>              Specify running mode(0:sample, 1:performance test)\n");
	printf("  -c <image_width>       Specify image width(column)\n");
	printf("  -r <image_height>      Specify image height(row)\n");
	printf("  -i <iteration_number>  Specify frames per second\n");
	printf("  -h <help>              Show this help message\n");
	printf("     For Example, performance test: ./%s -m 1 -c 1920 -r 1080 -i 1000\n", test_case);
	printf("     For Example, sample          : ./%s (not need input param)\n", test_case);
}

int parser_params(int argc, char **argv, struct PerformanceTestParam *param)
{
	struct option const long_options[] = {
		{"mode", optional_argument, NULL, 'm'},
		{"image_width", optional_argument, NULL, 'c'},
		{"image_height", optional_argument, NULL, 'r'},
		{"iteration_number", optional_argument, NULL, 'i'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, NULL, 0}};

	int opt_index = 0;
	int c = 0;

	while ((c = getopt_long(argc, argv, "m:c:r:i:h",
							long_options, &opt_index)) != -1){
		switch (c)
		{
		case 'm':
			param->mode = atoi(optarg);
			break;
		case 'c':
			param->image_width = atoi(optarg);
			break;
		case 'r':
			param->image_height = atoi(optarg);
			break;
		case 'i':
			param->iteration_number = atoi(optarg);
			break;
		case 'h':
		default:
			print_help(param->test_case);
			return -1;
		}
	}
	if(param->mode){
		printf("\nRun PerformanceTest[%s] width*height:%d*%d iteration number:%d.\n\n",
			param->test_case,
			param->image_width, param->image_height,
			param->iteration_number);

	}else{
		printf("\nRun Sample[%s](sample mode ignore input param).\n\n", param->test_case);
	}

	return 0;
}

void performance_test_start(struct PerformanceTestParam *param)
{
	param->test_start_time_us = get_timestamp_us();
}

void performance_test_stop_with_resolution_info(struct PerformanceTestParam *param,
	struct ResolutionInformation* resolution_info)
{
	param->test_end_time_us = get_timestamp_us();
	uint64_t diff_time_sum_us = param->test_end_time_us - param->test_start_time_us;
	float fps = 1000000.0 * param->iteration_number / diff_time_sum_us;

	struct ResolutionInformation res_info = {
		.input_batch = 1,
		.input_width = param->image_width,
		.input_height = param->image_height,
		.output_width = param->image_width,
		.output_height = param->image_height,
	};

	if(resolution_info != NULL){
		res_info = *resolution_info;
	}

	printf("\n[Case %-30s] [Input %1d*%-4d*%-4d] [Output %4d*%-4d] [TestTimes %5d] [TotalConsume %-10ldus] [AverageConsume %-7ldus] [FPS %7.1f]\n",
		   param->test_case,
		   res_info.input_batch, res_info.input_width, res_info.input_height,
		   res_info.output_width, res_info.output_height,
		   param->iteration_number, diff_time_sum_us,
		   diff_time_sum_us / param->iteration_number, fps);
}

void performance_test_stop(struct PerformanceTestParam *param)
{
	performance_test_stop_with_resolution_info(param, NULL);
}

n2d_error_t performance_test_create_buffer_with_color(struct PerformanceTestParam *param,
												 n2d_buffer_format_t format, n2d_buffer_t *src,
												 n2d_color_t color){
	int image_width = 0;
	int image_height = 0;
	if((src->width > 0) && (src->height > 0)){
		image_width = src->width;
		image_height = src->height;
	}else{
		image_width = param->image_width;
		image_height = param->image_height;
	}

	n2d_error_t error = N2D_SUCCESS;
	error = n2d_util_allocate_buffer(
		image_width,
		image_height,
		format,
		N2D_0,
		N2D_LINEAR,
		N2D_TSC_DISABLE,
		src);
	if (N2D_IS_ERROR(error))
	{
		printf("create buffer error=%d. %d*%d\n", error, image_width, image_height);
		return error;
	}
	error = n2d_fill(src, N2D_NULL, color, N2D_BLEND_NONE);
	if (N2D_IS_ERROR(error))
	{
		printf("n2d_fill buffer error=%d.\n", error);
		return error;
	}
	error = n2d_commit();
	if (N2D_IS_ERROR(error))
	{
		printf("n2d_commit error=%d.\n", error);
		return error;
	}

	return N2D_SUCCESS;
}


n2d_error_t performance_test_create_buffer_black(struct PerformanceTestParam *param,
												 n2d_buffer_format_t format, n2d_buffer_t *src)
{
	return performance_test_create_buffer_with_color(param, format, src, n2d_black);
}
n2d_error_t performance_test_create_buffer_with_rect(struct PerformanceTestParam *param,
													 n2d_buffer_format_t format, n2d_buffer_t *src)
{
	n2d_error_t error = N2D_SUCCESS;
	error = performance_test_create_buffer_black(param, format, src);
	if (N2D_IS_ERROR(error)){
		return error;
	}

	// src 填充 蓝色矩形框
	n2d_int32_t deltaX = param->image_width / 8;
	n2d_int32_t deltaY = param->image_height / 8;
	n2d_rectangle_t src_rect;
	src_rect.x = deltaX;
	src_rect.y = deltaY;
	src_rect.width = deltaX * 5;
	src_rect.height = deltaY * 3;

	n2d_uint8_t src_alpha = 0x80;
	n2d_color_t src_color = N2D_COLOR_BGRA8(src_alpha, 0x00, 0x00, 0xff); // 蓝色
	error = n2d_fill(src, &src_rect, src_color, N2D_BLEND_NONE);
	if (N2D_IS_ERROR(error))
	{
		printf("n2d_fill buffer error=%d.\n", error);
		return error;
	}
	error = n2d_commit();
	if (N2D_IS_ERROR(error))
	{
		printf("n2d_commit error=%d.\n", error);
		return error;
	}
	return N2D_SUCCESS;
}
n2d_error_t performance_test_add_rect(struct PerformanceTestParam *param,
	n2d_buffer_t *src, enum RectRelationPosition position, n2d_color_t color)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_int32_t x_unity = param->image_width / 8;
	n2d_int32_t y_unity = param->image_height / 8;

	n2d_rectangle_t src_rect;
	switch(position){
		case TOP_LEFT:
		src_rect.x = x_unity;
		src_rect.y = y_unity;
		src_rect.width =x_unity;
		src_rect.height = y_unity;
		break;

		case TOP_RIGHT:
		src_rect.x = x_unity * (8 - 2);
		src_rect.y = y_unity;
		src_rect.width =x_unity;
		src_rect.height = y_unity;

		break;
		case BOTTOM_LEFT:
			src_rect.x = x_unity;
			src_rect.y = y_unity * (8 - 2);
			src_rect.width =x_unity;
			src_rect.height = y_unity;
		break;
		case BOTTOM_RIGHT:
			src_rect.x = x_unity * (8 - 2);
			src_rect.y = y_unity * (8 - 2);
			src_rect.width =x_unity;
			src_rect.height = y_unity;
		break;
		default:

		break;
	}

	error = n2d_fill(src, &src_rect, color, N2D_BLEND_NONE);
	if (N2D_IS_ERROR(error))
	{
		printf("n2d_fill buffer error=%d.\n", error);
		return error;
	}
	error = n2d_commit();
	if (N2D_IS_ERROR(error))
	{
		printf("n2d_commit error=%d.\n", error);
		return error;
	}
	return N2D_SUCCESS;
}
n2d_error_t performance_test_save_to_file_width_name(struct PerformanceTestParam *param,
	n2d_buffer_t *src, char *test_case){
	char output_file_name[128];
	memset(output_file_name, 0, sizeof(output_file_name));

	char *format_string = convert_format_to_string(src->format);
	n2d_error_t error = N2D_SUCCESS;

	if(n2d_buffer_is_yuv(src->format)){
		if(test_case == NULL){
			sprintf(output_file_name, "./performance_test_%s_%d_%d.%s",
				test_case, param->image_width, param->image_height,
				format_string);
		}else{
			sprintf(output_file_name, "./performance_test_%s.%s",
				test_case, format_string);
		}
		error = n2d_util_save_buffer_to_vimg(src, output_file_name);
	}else{
		if(test_case == NULL){
			sprintf(output_file_name, "./performance_test_%s_%d_%d_%s.bmp",
				test_case, param->image_width, param->image_height,
				format_string);
		}else{
			sprintf(output_file_name, "./performance_test_%s_%s.bmp",
				test_case, format_string);
		}

		error = n2d_util_save_buffer_to_file(src, output_file_name);
	}
	printf("performance test save result to file [%s]\n", output_file_name);

	if (N2D_IS_ERROR(error))
	{
		printf("alphablend failed! error=%d.\n", error);
	}
	return error;
}
n2d_error_t performance_test_save_to_file(struct PerformanceTestParam *param, n2d_buffer_t *src){
	return performance_test_save_to_file_width_name(param, src, NULL);
}