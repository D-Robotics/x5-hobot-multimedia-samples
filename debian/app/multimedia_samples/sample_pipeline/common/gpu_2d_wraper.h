#ifndef __GPU_2D_WRAPWER__H
#define __GPU_2D_WRAPWER__H
#include "GC820/nano2D.h"
#include "GC820/nano2D_util.h"
extern const n2d_color_t n2d_blue;
extern const n2d_color_t n2d_black;
extern const n2d_color_t n2d_black_opaque;
extern const n2d_color_t n2d_green;
extern const n2d_color_t n2d_red;
extern const n2d_color_t n2d_white;
extern const n2d_color_t n2d_grey;
extern const n2d_color_t n2d_light_grey;

int gpu_2d_open();
int gpu_2d_close();
int gpu_2d_fill(n2d_buffer_t* src, n2d_color_t color);
int gpu_2d_resize_multi_source(n2d_buffer_t* src, int count, n2d_buffer_t*dst);
int gpu_2d_crop_multi_rects(n2d_buffer_t* src, n2d_rectangle_t* rects, int count, n2d_buffer_t*crops);
int gpu_2d_format_convert(n2d_buffer_t*dst, n2d_buffer_t* src, int src_count, n2d_rectangle_t *dst_positions);
int gpu_2d_stitch_multi_source(n2d_buffer_t* src_images, n2d_rectangle_t *src_positions , int src_count, n2d_buffer_t*dst);
int gpu_2d_stitch_multi_source_blend(n2d_buffer_t* src_images, n2d_rectangle_t *dst_positions , int src_count, n2d_buffer_t*dst);
#endif