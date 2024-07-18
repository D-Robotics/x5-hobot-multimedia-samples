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

// static n2d_orientation_t orientations[] =
// {
//     N2D_0,
//     N2D_90,
//     N2D_180,
//     N2D_270,
//     N2D_FLIP_X,
//     N2D_FLIP_Y,
// };

n2d_error_t rotation(n2d_buffer_t *src, n2d_buffer_t *dst, n2d_orientation_t orientation)
{
    n2d_error_t error = N2D_SUCCESS;
    src->orientation = N2D_0;
    dst->orientation = orientation;

    error = n2d_blit(dst, N2D_NULL, src, N2D_NULL, N2D_BLEND_NONE);
    if (N2D_IS_ERROR(error))
    {
        printf("blit error, error=%d.\n", error);
        goto on_error;
    }

    error = n2d_commit();
    if (N2D_IS_ERROR(error))
    {
        printf("blit error, error=%d.\n", error);
        goto on_error;
    }

    return N2D_SUCCESS;
on_error:
    if(N2D_INVALID_HANDLE != src->handle)
    {
        n2d_free(src);
    }

    return error;
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

    //读取文件
    n2d_buffer_t  src;
    char *input_file_name = "../resource/R5G6B5_640x640.bmp";
    error = n2d_util_load_buffer_from_file(input_file_name, &src);
    if (N2D_IS_ERROR(error))
    {
        printf("load buffer from file %s failed! error=%d.\n", input_file_name, error);
        goto on_close;
    }

    /* Allocate the buffer. */
    n2d_buffer_t  dst;
    error = n2d_util_allocate_buffer(
        src.width,
        src.height,
        N2D_BGRA8888,
        N2D_0,
        N2D_LINEAR,
        N2D_TSC_DISABLE,
        &dst);
    if (N2D_IS_ERROR(error))
    {
        printf("allocate buffer failed! error=%d.\n", error);
        goto on_free_src;
    }

    /* Run the test case. */
    error = rotation(&src, &dst, N2D_90);
    if (N2D_IS_ERROR(error))
    {
        printf("rotation failed! error=%d.\n", error);
    }

    char *output_file_name = "./R5G6B5_640x640_rotated.bmp";
    error = n2d_util_save_buffer_to_file(&dst, output_file_name);
    if (N2D_IS_ERROR(error))
    {
        printf("rotation failed! error=%d.\n", error);
    }
    printf("Save file to [%s].\n", output_file_name);

on_free_src:
    error = n2d_free(&dst);
    if (N2D_IS_ERROR(error))
    {
        printf("free buffer failed! error=%d.\n", error);
    }
    error = n2d_free(&src);
    if (N2D_IS_ERROR(error))
    {
        printf("free buffer failed! error=%d.\n", error);
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