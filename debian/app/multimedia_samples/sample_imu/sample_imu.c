/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "imu_manager.h"

#define DEFAULT_IMU_NAME "bmi08x"

static void print_help() {
    printf("Usage: sample_imu [OPTIONS]\n");
    printf("Options:\n");
    printf("  -n <imu_name>         Specify IMU name (default: %s)\n", DEFAULT_IMU_NAME);
    printf("  -h                    Show this help message\n");
    list_available_sensors();
}

static void command_help() {
    printf("\n");
    printf("***************  Command Lists  ***************\n");
    printf(" g    -- Get a single frame of imu data\n");
    printf(" l    -- Get multiple frames of imu data\n");
    printf(" q    -- Quit the program\n");
    printf(" h    -- Print this help message\n");
}

int main(int argc, char** argv) {
    int opt_index = 0;
    int c = 0;
    int num_frames = 1;
    int read_loop = 0;/* 这个变量暂时没有使用，可以先保留一下，下一版更新或许会添加持续读取的功能，会用到该变量 */
    const char* imu_name = NULL;

    static struct option long_options[] = {
        {"imu_name", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    // 解析命令行参数
    while((c = getopt_long(argc, argv, "n:h", long_options, &opt_index)) != -1) {
        switch (c)
        {
        case 'n':
            imu_name = optarg;
            break;
        case 'h':
        default:
            print_help();
            return 0;
        }
    }

    if (optind < argc) {
        printf("Unknown arguments found. Please check your input.\n");
        print_help();
        return 0;
    }

    // 如果用户没有指定传感器类型，使用默认值
    if (!imu_name) {
        imu_name = DEFAULT_IMU_NAME;
        printf("No IMU specified, using default: %s\n", imu_name);
    }

    printf("Using IMU: %s\n", imu_name);

    // 查找并初始化IMU
    SensorHandle handle = init_sensor(imu_name, "");
    if (!handle) {
        printf("Error: init IMU '%s' failed !!! Quit Now\n", imu_name);
        return 1;
    }

    char cmd;
    do {
        command_help();
        printf("Enter command: ");
        scanf(" %c", &cmd);

        switch (cmd) {
            case 'g':
                num_frames = 1;
                read_loop = 0;
                break;
            case 'l':
                printf("Enter number of frames to read: ");
                scanf("%d", &num_frames);
                read_loop = 0;
                break;
            case 'q':
                break;
            case 'h':
                print_help();
                continue;
            default:
                printf("Unknown command. Please try again.\n");
                continue;
        }

        if (cmd != 'q') {
            if (read_loop) {
                while (1) {
                    ImuData data;
                    if (read_sensor_data(handle, &data) == 0) {
                        printf("Data received:\n");
                        print_imu_data(&data);
                    } else {
                        printf("Error: Failed to read data from IMU\n");
                    }
                }
            } else {
                for (int i = 0; i < num_frames; ++i) {
                    ImuData data;
                    if (read_sensor_data(handle, &data) == 0) {
                        printf("Data received (Frame %d):\n", i + 1);
                        print_imu_data(&data);
                    } else {
                        printf("Error: Failed to read data from IMU (Frame %d)\n", i + 1);
                    }
                }
            }
        }
    } while (cmd != 'q');

    // 释放IMU资源
    release_sensor(handle);
    printf("IMU resources released\n");

    return 0;
}