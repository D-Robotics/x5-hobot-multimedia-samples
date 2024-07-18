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

#include "common_utils.h"

#include "GC820/nano2D.h"
#include "GC820/nano2D_util.h"

n2d_error_t rectangle_fill(n2d_buffer_t *src, n2d_rectangle_t *rect, n2d_color_t color)
{
    n2d_error_t error = N2D_SUCCESS;
    error = n2d_fill(src, rect, color, N2D_BLEND_SRC_OVER);
    if (N2D_IS_ERROR(error))
    {
        printf("fill error, error=%d.\n", error);
        return error;
    }

    error = n2d_commit();
    if (N2D_IS_ERROR(error))
    {
        printf("fill commit error, error=%d.\n", error);
        return error;
    }

    return N2D_SUCCESS;
}

int main(int argc, char **argv)
{
    /* Open the context. */
    printf("Start !!!\n");
    n2d_error_t error = n2d_open();
    if (N2D_IS_ERROR(error))
    {
        printf("open context failed! error=%d.\n", error);
        goto on_error;
    }
        /* switch to default device and core */
    N2D_ON_ERROR(n2d_switch_device(N2D_DEVICE_0));
    N2D_ON_ERROR(n2d_switch_core(N2D_CORE_0));

    //创建buffer
    n2d_buffer_t  src;
    error = n2d_util_allocate_buffer(
        640,
        480,
        N2D_BGRA8888,
        N2D_0,
        N2D_LINEAR,
        N2D_TSC_DISABLE,
        &src);
    if (N2D_IS_ERROR(error))
    {
        printf("create buffer error=%d.\n", error);
        goto on_close;
    }

    char *input_file_name = "../resource/R5G6B5_640x640.bmp";
    error = n2d_util_load_buffer_from_file(input_file_name, &src);
    if (N2D_IS_ERROR(error))
    {
        printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
        goto on_close;
    }

    //矩形框1: 蓝色不透明
    n2d_rectangle_t rect_upper_left;
    rect_upper_left.x      = src.width / 8;
    rect_upper_left.y      = src.height / 8;
    rect_upper_left.width  = src.width / 4;
    rect_upper_left.height = src.height / 4;
    n2d_color_t color_blue_opaque = N2D_COLOR_BGRA8(255, 0, 0, 255);
    error = rectangle_fill(&src, &rect_upper_left, color_blue_opaque);
    if (N2D_IS_ERROR(error))
    {
        printf("rectangle_fill failed! error=%d.\n", error);
        goto on_free_src;
    }
    //矩形框2: 绿色半透明
    n2d_rectangle_t rect_upper_right;
    rect_upper_right.x      = src.width / 8 * 5;
    rect_upper_right.y      = src.height / 8;
    rect_upper_right.width  = src.width / 4;
    rect_upper_right.height = src.height / 4;
    n2d_color_t color_green_half_transparent = N2D_COLOR_BGRA8(128, 0, 255, 0);
    error = rectangle_fill(&src, &rect_upper_right, color_green_half_transparent);
    if (N2D_IS_ERROR(error))
    {
        printf("rectangle_fill failed! error=%d.\n", error);
        goto on_free_src;
    }

    //矩形框3: 绿色透明
    n2d_rectangle_t rect_lower_left;
    rect_lower_left.x      = src.width / 8;
    rect_lower_left.y      = src.height / 8 * 5;
    rect_lower_left.width  = src.width / 4;
    rect_lower_left.height = src.height / 4;
    n2d_color_t color_green_transparent = N2D_COLOR_BGRA8(0, 0, 255, 0);
    error = rectangle_fill(&src, &rect_lower_left, color_green_transparent);
    if (N2D_IS_ERROR(error))
    {
        printf("rectangle_fill failed! error=%d.\n", error);
        goto on_free_src;
    }

    //矩形框4: 绿色不透明
    n2d_rectangle_t rect_lower_right;
    rect_lower_right.x      = src.width / 8 * 5;
    rect_lower_right.y      = src.height / 8 * 5;
    rect_lower_right.width  = src.width / 4;
    rect_lower_right.height = src.height / 4;
    n2d_color_t color_green_opaque = N2D_COLOR_BGRA8(255, 0, 255, 0);
    error = rectangle_fill(&src, &rect_lower_right, color_green_opaque);
    if (N2D_IS_ERROR(error))
    {
        printf("rectangle_fill failed! error=%d.\n", error);
        goto on_free_src;
    }

    char *output_file_name = "./R5G6B5_640x640_rectangle_fill.bmp";
    error = n2d_util_save_buffer_to_file(&src, output_file_name);
    if (N2D_IS_ERROR(error))
    {
        printf("rectangle_fill failed! error=%d.\n", error);
        goto on_free_src;
    }
    printf("Save file to [%s].\n", output_file_name);
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