#include "gpu_2d_wraper.h"

const n2d_color_t n2d_blue = N2D_COLOR_BGRA8(0x80, 0x00, 0x00, 0xff);
const n2d_color_t n2d_black = N2D_COLOR_BGRA8(0x80, 0x00, 0x00, 0x00);
const n2d_color_t n2d_black_opaque = N2D_COLOR_BGRA8(0x00, 0x00, 0x00, 0x00); //不透明
const n2d_color_t n2d_green = N2D_COLOR_BGRA8(0x80, 0x00, 0xff, 0x00);
const n2d_color_t n2d_red = N2D_COLOR_BGRA8(0x80, 0xff, 0x00, 0x00);
const n2d_color_t n2d_white = N2D_COLOR_BGRA8(0x80, 0xff, 0xff, 0xff);
const n2d_color_t n2d_grey = N2D_COLOR_BGRA8(0x80, 0x0f, 0x0f, 0x0f);
const n2d_color_t n2d_light_grey = N2D_COLOR_BGRA8(0x80, 0x20, 0x20, 0x20);

int gpu_2d_crop_multi_rects(n2d_buffer_t* src, n2d_rectangle_t* rects, int count, n2d_buffer_t*crops){
	n2d_error_t error = N2D_SUCCESS;
	for (int i = 0; i < count; i++){
		N2D_ON_ERROR(n2d_blit(&crops[i], N2D_NULL, src, &rects[i], N2D_BLEND_NONE));
	}
	N2D_ON_ERROR(n2d_commit());
#if 0
	static int count_croped = 0;
	count_croped++;
	for (int i = 0; i < count; i++)
	{
		char output_file_name[90];
		memset(output_file_name, 0, sizeof(output_file_name));
		sprintf(output_file_name, "./croped%d_%d_%d_%d.yuv", count_croped, i, crops[i].width, crops[i].height);
		error = n2d_util_save_buffer_to_vimg(&crops[i], output_file_name);
		if (N2D_IS_ERROR(error))
		{
			printf("alphablend failed! error=%d.\n", error);
			goto on_error;
		}
	}
#endif
	return 0;
on_error:
	return -1;
}

int gpu_2d_resize_multi_source(n2d_buffer_t* src, int count, n2d_buffer_t*dst){
	n2d_error_t error = N2D_SUCCESS;

	for (int i = 0; i < count; i++){
		n2d_rectangle_t dstrect;
		dstrect.x = 0;
		dstrect.y = 0;
		dstrect.width  = dst[i].width;
		dstrect.height = dst[i].height;

		n2d_rectangle_t srcrect;
		srcrect.x = 0;
		srcrect.y = 0;
		srcrect.width  = src[i].width;
		srcrect.height = src[i].height;
		N2D_ON_ERROR(n2d_filterblit(&dst[i],&dstrect, N2D_NULL, &src[i], &srcrect, N2D_BLEND_NONE));
	}

	N2D_ON_ERROR(n2d_commit());
	return 0;
on_error:
	return -1;
}

int gpu_2d_stitch_multi_source(n2d_buffer_t* src_images, n2d_rectangle_t *dst_positions , int src_count, n2d_buffer_t*dst){
	n2d_error_t error = N2D_SUCCESS;

	for (int i = 0; i < src_count; i++){
		N2D_ON_ERROR(n2d_blit(dst, &dst_positions[i], &src_images[i], N2D_NULL, N2D_BLEND_NONE));
	}

	N2D_ON_ERROR(n2d_commit());
	return 0;
on_error:
	return -1;
}

int gpu_2d_stitch_multi_source_blend(n2d_buffer_t* src_images, n2d_rectangle_t *src_positions , int src_count, n2d_buffer_t*dst){
	n2d_error_t error = N2D_SUCCESS;
 	n2d_state_config_t globalAlpha = {.state = N2D_SET_GLOBAL_ALPHA};
	globalAlpha.config.globalAlpha.srcMode = N2D_GLOBAL_ALPHA_ON;
	globalAlpha.config.globalAlpha.dstMode = N2D_GLOBAL_ALPHA_OFF;
	globalAlpha.config.globalAlpha.srcValue = 128; //0: S + D , 255: S + 0 *D, 128 : S + 0.5 * D
	globalAlpha.config.globalAlpha.dstValue = 0;
    N2D_ON_ERROR(n2d_set(&globalAlpha));

	for (int i = 0; i < src_count; i++){
#if 0
		n2d_rectangle_t srcrect;
		srcrect.x = 0;
		srcrect.y = 0;
		srcrect.width  = src_images[i].width;
		srcrect.height = src_images[i].height;
		N2D_ON_ERROR(n2d_filterblit(dst, &dst_positions[i], N2D_NULL, &src_images[i], &srcrect, N2D_BLEND_ADDITIVE)); //
#else
		n2d_rectangle_t dstrect;
		dstrect.x = 0;
		dstrect.y = 0;
		dstrect.width  = dst->width;
		dstrect.height = dst->height;

		if(i == 0){
			N2D_ON_ERROR(n2d_blit(dst, &dstrect, &src_images[i], &src_positions[i], N2D_BLEND_NONE));
		}else{
			N2D_ON_ERROR(n2d_blit(dst, &dstrect, &src_images[i], &src_positions[i], N2D_BLEND_SRC_OVER));
		}

#endif
	}
	N2D_ON_ERROR(n2d_commit());
	return 0;
on_error:
	return -1;
}

int gpu_2d_format_convert(n2d_buffer_t*dst, n2d_buffer_t* src, int src_count, n2d_rectangle_t *dst_positions){
	n2d_error_t error = N2D_SUCCESS;
	for (int i = 0; i < src_count; i++){
		N2D_ON_ERROR(n2d_blit(&dst[i], dst_positions, &src[i], N2D_NULL, N2D_BLEND_NONE));
	}

	N2D_ON_ERROR(n2d_commit());
	return 0;
on_error:
	return -1;
}

int gpu_2d_fill(n2d_buffer_t* src, n2d_color_t color){
	n2d_error_t error = N2D_SUCCESS;
	n2d_rectangle_t src_rect = {
		.x = 0,
		.y = 0,
		.width = src->width,
		.height = src->height,
	};

	N2D_ON_ERROR(n2d_fill(src, &src_rect, color, N2D_BLEND_NONE));
	N2D_ON_ERROR(n2d_commit());

	return 0;
on_error:
	return -1;
}

int gpu_2d_open(){
	n2d_error_t error = n2d_open();
	if (N2D_IS_ERROR(error))
	{
		printf("open context failed! error=%d.\n", error);
		return -1;
	}
	N2D_ON_ERROR(n2d_switch_device(N2D_DEVICE_0));
	N2D_ON_ERROR(n2d_switch_core(N2D_CORE_0));

	return 0;
on_error:
	return -1;
}

int gpu_2d_close(){
	n2d_error_t error = n2d_close();
	if (N2D_IS_ERROR(error))
	{
		printf("close context failed! error=%d.\n", error);
		return -1;
	}
	return 0;
}
