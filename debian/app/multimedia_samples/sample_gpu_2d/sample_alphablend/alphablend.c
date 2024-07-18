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

n2d_blend_t blend_modes[] =
{
    N2D_BLEND_SRC_OVER, // S + (1 - Sa) * D
    N2D_BLEND_DST_OVER, // (1 – Da) * S + D
    N2D_BLEND_SRC_IN,   // Da * S
    N2D_BLEND_DST_IN,   // Sa * D
    N2D_BLEND_ADDITIVE, // S + D
    N2D_BLEND_SUBTRACT, // D * (1 – S)
};
const char *blend_mode_names[] = {
    "src_over",
    "dst_over",
    "src_in",
    "dst_in",
    "additive",
    "subtract"
};
void alphablend_test(int image_width, int image_height){
    n2d_error_t error = N2D_SUCCESS;
    n2d_int32_t deltaX = image_width / 8;
    n2d_int32_t deltaY = image_height / 8;

    //图像1相关变量
    n2d_buffer_t  src;
    n2d_uint8_t src_alpha;
    n2d_color_t src_color;
    n2d_rectangle_t src_rect;
    src_rect.x = deltaX;
    src_rect.y = deltaY;
    src_rect.width  = deltaX * 5;
    src_rect.height = deltaY * 3;
    error = n2d_util_allocate_buffer(
        image_width,
        image_height,
        N2D_BGRA8888,
        N2D_0,
        N2D_LINEAR,
        N2D_TSC_DISABLE,
        &src);
    if (N2D_IS_ERROR(error))
    {
        printf("create buffer error=%d.\n", error);
        return;
    }

    //图像2相关变量
    n2d_buffer_t  dst;
    n2d_uint8_t dst_alpha;
    n2d_color_t dst_color;
    n2d_rectangle_t dst_rect;
    dst_rect.x = deltaX * 3;
    dst_rect.y = deltaY * 2;
    dst_rect.width  = deltaX * 4;
    dst_rect.height = deltaY * 5;
    error = n2d_util_allocate_buffer(
        image_width,
        image_height,
        N2D_BGRA8888,
        N2D_0,
        N2D_LINEAR,
        N2D_TSC_DISABLE,
        &dst);
    if (N2D_IS_ERROR(error))
    {
        printf("create buffer error=%d.\n", error);
        return;
    }

    char output_file_name[128];
    //遍历每种模式
    for (int n = 0; n < N2D_COUNTOF(blend_modes); n++)
    {
        src_alpha = 0x80;
        dst_alpha = 0x80;

        for (int i = 0; i < 8; i++)
        {
            src_color = N2D_COLOR_BGRA8(src_alpha, 0x00, 0x00, 0xff);//蓝色
            dst_color = N2D_COLOR_BGRA8(dst_alpha, 0xff, 0x00, 0x00);//红色

            //src 填充0
            N2D_ON_ERROR(n2d_fill(&src, N2D_NULL, 0x0, N2D_BLEND_NONE));
            //src 填充 蓝色矩形框
            N2D_ON_ERROR(n2d_fill(&src, &src_rect, src_color, N2D_BLEND_NONE));

            //dst 填充0
            N2D_ON_ERROR(n2d_fill(&dst, N2D_NULL, 0x0, N2D_BLEND_NONE));
            //dst 填充 红色矩形框
            N2D_ON_ERROR(n2d_fill(&dst, &dst_rect, dst_color, N2D_BLEND_NONE));

            //alphablend
            N2D_ON_ERROR(n2d_blit(&dst, N2D_NULL, &src, N2D_NULL, blend_modes[n]));
            N2D_ON_ERROR(n2d_commit());

            //保存图片
            memset(output_file_name, 0, sizeof(output_file_name));
            sprintf(output_file_name, "./%s_mode_index_%d_%d-%d.bmp", blend_mode_names[n] , i,
                src_alpha, dst_alpha);
            error = n2d_util_save_buffer_to_file(&dst, output_file_name);
            if (N2D_IS_ERROR(error))
            {
                printf("alphablend failed! error=%d.\n", error);
            }

            //颜色渐变
            src_alpha += 0x10;
            dst_alpha -= 0x10;
        }
    }
on_error:
    return;
}

int main(int argc, char **argv)
{
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

    alphablend_test(640, 480);
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