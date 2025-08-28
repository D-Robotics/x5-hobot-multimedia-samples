/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include "imu_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <math.h>

#define SYSFS_PATH "/sys/bus/iio/devices/"
#define MAX_PATH_LEN 512

// BMI08x传感器私有数据结构
typedef struct {
    int initialized;
    const char* params;
    float accel_scale;
    float gyro_scale;
    char dev_path[MAX_PATH_LEN];  // 存储设备路径
} BMI08xData;


/* 带符号扩展的原始数据读取 */
static int read_raw_signed(const char *dev_path, const char *type,
                         const char *axis, int *val) {
    char raw_path[MAX_PATH_LEN];
    FILE *fp;
    unsigned raw_value;

    snprintf(raw_path, sizeof(raw_path),
            "%s/in_%s_%s_raw", dev_path, type, axis);

    // printf("check dev_path = %s\n",raw_path);

    fp = fopen(raw_path, "r");
    if (!fp) {
        perror("Failed to open raw file");
        return -1;
    }

    if (fscanf(fp, "%u", &raw_value) != 1) { // 读取无符号原始值
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* 执行16位补码转换 */
    *val = (int)(raw_value > 32767 ? raw_value - 65536 : raw_value);
    return 0;
}

/* 读取传感器时间戳（单位：微秒） */
static int read_sensor_time(const char *dev_path, uint64_t *timestamp) {
    char time_path[MAX_PATH_LEN];
    FILE *fp;
    uint32_t sensor_time;

    // 构建sensor_time文件路径
    snprintf(time_path, sizeof(time_path), "%s/sensor_time", dev_path);

    // 打开文件
    fp = fopen(time_path, "r");
    if (!fp) {
        perror("Failed to open sensor_time file");
        return -1;
    }

    // 读取时间戳值（格式：sensor_time:8835002）
    if (fscanf(fp, "sensor_time:%u", &sensor_time) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    // 将传感器时间转换为微秒（每个单位=39.0625μs）
    *timestamp = (uint64_t)sensor_time * 390625 / 10000;
    return 0;
}

/* 读取传感器范围设置并计算比例因子 */
static int read_sensor_ranges(const char *dev_path, float *accel_scale, float *gyro_scale) {
    char path[MAX_PATH_LEN];
    FILE *fp;
    char line[256];
    int acc_range = -1, gyro_range = -1;

    // 1. 读取加速度计配置
    snprintf(path, sizeof(path), "%s/acc_config", dev_path);
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            // 解析格式: "acc_conf odr:8 bw:10 range:1"
            char *range_ptr = strstr(line, "range:");
            if (range_ptr) {
                if (sscanf(range_ptr, "range:%d", &acc_range) != 1) {
                    fprintf(stderr, "Failed to parse acc_range value\n");
                    acc_range = -1;
                }
            }
        }
        fclose(fp);
    } else {
        perror("Failed to open acc_config file");
    }

    // 2. 读取陀螺仪配置
    snprintf(path, sizeof(path), "%s/gyr_config", dev_path);
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            // 解析格式: "gyro_conf odr:8 bw:10 range:0"
            char *range_ptr = strstr(line, "range:");
            if (range_ptr) {
                if (sscanf(range_ptr, "range:%d", &gyro_range) != 1) {
                    fprintf(stderr, "Failed to parse gyro_range value\n");
                    gyro_range = -1;
                }
            }
        }
        fclose(fp);
    } else {
        perror("Failed to open gyro_config file");
    }

    // 3. 设置加速度计比例因子
    if (acc_range >= 0 && acc_range <= 3) {
        const float ranges[] = {3.0f, 6.0f, 12.0f, 24.0f};  // 对应 range:0-3
        *accel_scale = (2 * ranges[acc_range]) / 65536.0f;
        printf("Accelerometer range: ±%dg, scale: %f g/LSB\n",
               (int)ranges[acc_range], *accel_scale);
    } else {
        *accel_scale = (2 * 6.0f) / 65536.0f;  // 默认 ±6g (range:1)
        fprintf(stderr, "Using default acc_range: ±6g\n");
    }

    // 4. 设置陀螺仪比例因子
    if (gyro_range >= 0 && gyro_range <= 4) {
        // 分辨率表（LSB/°/s）对应 range:0-4
        const float resolutions[] = {
            16.384f,    // ±2000°/s (range:0)
            32.768f,    // ±1000°/s (range:1)
            65.536f,    // ±500°/s (range:2)
            131.072f,   // ±250°/s (range:3)
            262.144f    // ±125°/s (range:4)
        };
        const float ranges[] = {2000.0f, 1000.0f, 500.0f, 250.0f, 125.0f};

        *gyro_scale = (1.0f / resolutions[gyro_range]) * (M_PI / 180.0f);
        printf("Gyroscope range: ±%d°/s, scale: %f rad/s/LSB\n",
               (int)ranges[gyro_range], *gyro_scale);
    } else {
        *gyro_scale = (1.0f / 16.384f) * (M_PI / 180.0f);  // 默认 ±2000°/s (range:0)
        fprintf(stderr, "Using default gyro_range: ±2000°/s\n");
    }

    return 0;
}

// BMI08x初始化函数
static SensorContext bmi08x_init(const char* params, const char* dev_path) {
    BMI08xData* data = (BMI08xData*)malloc(sizeof(BMI08xData));
    if (!data) {
        return NULL;
    }

    memset(data, 0, sizeof(BMI08xData));
    data->params = params;

    /*
    * 根据手册，bmi088 的 scale 因子可以通过读取 gyro_range 和 acc_range 两个节点来对应写入
    * 换算表如下：
    * 加速度计 (ACC_RANGE 0x41):
    * | acc_range | Range setting | 比例因子计算 (g/LSB) |
    * |-----------|---------------|----------------------|
    * | 0x00      | ±3g           | (6g)/65536  = 0.00009155  |
    * | 0x01      | ±6g           | (12g)/65536 = 0.0001831   |
    * | 0x02      | ±12g          | (24g)/65536 = 0.0003662   |
    * | 0x03      | ±24g          | (48g)/65536 = 0.0007324   |
    *
    * 陀螺仪 (GYRO_RANGE 0x0F):
    * | gyro_range | Full scale [°/s] | 分辨率 (LSB/°/s) | 比例因子计算 (rad/s/LSB) |
    * |------------|------------------|------------------|--------------------------|
    * | 0x00       | ±2000            | 16.384           | (1/16.384) * (π/180) = 0.001065 |
    * | 0x01       | ±1000            | 32.768           | (1/32.768) * (π/180) = 0.0005325 |
    * | 0x02       | ±500             | 65.536           | (1/65.536) * (π/180) = 0.0002663 |
    * | 0x03       | ±250             | 131.072          | (1/131.072) * (π/180) = 0.0001331 |
    * | 0x04       | ±125             | 262.144          | (1/262.144) * (π/180) = 0.00006656 |
    *
    * 注意：
    * 1. 当前使用默认设置：加速度计 acc_range=0x01(±6g)，陀螺仪 gyro_range=0x00(±2000°/s)
    * 2. 加速度计比例因子 = (2 × Range setting) / 65536
    * 3. 陀螺仪比例因子 = (1 / 分辨率) × (π/180) [转换为弧度制]
    * 4. 数字范围均为16位有符号整数（-32768 到 32767）
    */
    // 读取范围设置并计算比例因子
    if (read_sensor_ranges(dev_path, &data->accel_scale, &data->gyro_scale) != 0) {
        // 读取失败时使用默认值
        data->accel_scale = (2 * 6.0f) / 65536.0f;  // ±6g
        data->gyro_scale = (1.0f / 16.384f) * (M_PI / 180.0f);  // ±2000°/s
        fprintf(stderr, "Using default scale factors\n");
    }

    /* 调试的时候可以打开 */
    // printf("Accelerometer scale: %f g/LSB\n", data->accel_scale);
    // printf("Gyroscope scale: %f rad/s/LSB\n", data->gyro_scale);

    printf("BMI08x: Initializing with params: %s\n", params);

    // 使用传入的设备路径
    strncpy(data->dev_path, dev_path, MAX_PATH_LEN);

    data->initialized = 1;
    return data;
}

// BMI08x读取函数
static int bmi08x_read(SensorContext context, ImuData* data) {
    BMI08xData* bmi_data = (BMI08xData*)context;
    if (!bmi_data || !bmi_data->initialized) {
        return -1;
    }

    int accel_x_raw, accel_y_raw, accel_z_raw;
    int gyro_x_raw, gyro_y_raw, gyro_z_raw;

    // 读取加速度计数据（带符号扩展）
    if (read_raw_signed(bmi_data->dev_path, "accel", "x", &accel_x_raw) ||
        read_raw_signed(bmi_data->dev_path, "accel", "y", &accel_y_raw) ||
        read_raw_signed(bmi_data->dev_path, "accel", "z", &accel_z_raw)) {
        return -1;
    }

    // 读取陀螺仪数据（带符号扩展）
    if (read_raw_signed(bmi_data->dev_path, "anglvel", "x", &gyro_x_raw) ||
        read_raw_signed(bmi_data->dev_path, "anglvel", "y", &gyro_y_raw) ||
        read_raw_signed(bmi_data->dev_path, "anglvel", "z", &gyro_z_raw)) {
        return -1;
    }

    // 读取传感器时间戳
    if (read_sensor_time(bmi_data->dev_path, &data->timestamp) != 0) {
        return -1;
    }

    // 转换为实际物理值
    data->ax = accel_x_raw * bmi_data->accel_scale * 9.80665f; // 转换为m/s²
    data->ay = accel_y_raw * bmi_data->accel_scale * 9.80665f;
    data->az = accel_z_raw * bmi_data->accel_scale * 9.80665f;

    data->gx = gyro_x_raw * bmi_data->gyro_scale;
    data->gy = gyro_y_raw * bmi_data->gyro_scale;
    data->gz = gyro_z_raw * bmi_data->gyro_scale;

    // BMI08x没有内置磁力计，设置为0
    data->mx = 0.0f;
    data->my = 0.0f;
    data->mz = 0.0f;

    // 设置状态
    data->status = 0;

    return 0;
}

// BMI08x释放函数
static void bmi08x_release(SensorContext context) {
    BMI08xData* bmi_data = (BMI08xData*)context;
    if (bmi_data) {
        printf("BMI08x: Releasing resources\n");
        free(bmi_data);
    }
}

// BMI08x传感器驱动实现
const SensorDriver bmi08x_driver = {
    .name = "bmi08x",
    .init = bmi08x_init,
    .read = bmi08x_read,
    .release = bmi08x_release
};