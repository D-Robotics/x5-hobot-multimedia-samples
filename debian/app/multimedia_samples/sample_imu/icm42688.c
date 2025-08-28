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
#define MAX_DEVICES 10  // 最大支持的设备数量

// 全局共享数据结构
typedef struct {
    int ref_count;                   // 引用计数
    int initialized;                 // 初始化标志
    float accel_scale;               // 加速度计量程
    float gyro_scale;                // 陀螺仪量程
    char accel_dev_path[MAX_PATH_LEN]; // 加速度计设备路径
    char gyro_dev_path[MAX_PATH_LEN];  // 陀螺仪设备路径
} Icm42688_Data;

static Icm42688_Data* g_shared_data = NULL; // 全局共享实例

/* 读取原始数据（有符号整数）*/
static int read_raw_signed(const char *dev_path, const char *type,
    const char *axis, int *val) {
    char raw_path[MAX_PATH_LEN];
    FILE *fp;

    snprintf(raw_path, sizeof(raw_path),
    "%s/in_%s_%s_raw", dev_path, type, axis);

    fp = fopen(raw_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open raw file: %s (error: %s)\n",
                raw_path, strerror(errno));
        return -1;
    }

    if (fscanf(fp, "%d", val) != 1) {
        fclose(fp);
        fprintf(stderr, "Failed to read value from: %s\n", raw_path);
        return -1;
    }
    fclose(fp);
    return 0;
}

/* 读取比例因子 */
static int read_sensor_scale(const char *dev_path, const char *type, float *scale) {
    char scale_path[MAX_PATH_LEN];
    FILE *fp;

    snprintf(scale_path, sizeof(scale_path),
    "%s/in_%s_scale", dev_path, type);

    fp = fopen(scale_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open scale file: %s (error: %s)\n",
                scale_path, strerror(errno));
        return -1;
    }

    if (fscanf(fp, "%f", scale) != 1) {
        fclose(fp);
        fprintf(stderr, "Failed to read scale from: %s\n", scale_path);
        return -1;
    }
    fclose(fp);
    return 0;
}

/* 获取当前时间戳（微秒） */
static uint64_t get_current_timestamp_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/* 从设备路径读取设备名称 */
static int read_device_name(const char *dev_path, char *name, size_t name_size) {
    char name_path[MAX_PATH_LEN];
    FILE *fp;

    snprintf(name_path, sizeof(name_path), "%s/name", dev_path);
    fp = fopen(name_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open name file: %s (error: %s)\n",
                name_path, strerror(errno));
        return -1;
    }

    if (!fgets(name, name_size, fp)) {
        fclose(fp);
        fprintf(stderr, "Failed to read name from: %s\n", name_path);
        return -1;
    }

    // 移除换行符
    name[strcspn(name, "\n")] = '\0';
    fclose(fp);
    return 0;
}

/* 查找所有IIO设备 */
static int find_all_iio_devices(char devices[][MAX_PATH_LEN], int max_devices) {
    DIR *dir;
    struct dirent *ent;
    int count = 0;

    dir = opendir(SYSFS_PATH);
    if (!dir) {
        fprintf(stderr, "Failed to open %s: %s\n", SYSFS_PATH, strerror(errno));
        return -1;
    }

    printf("\nScanning IIO devices in %s:\n", SYSFS_PATH);
    while ((ent = readdir(dir)) != NULL && count < max_devices) {
        if (strstr(ent->d_name, "iio:device")) {
            snprintf(devices[count], MAX_PATH_LEN, "%s%s", SYSFS_PATH, ent->d_name);

            // 读取并打印设备名称
            char name[64];
            if (read_device_name(devices[count], name, sizeof(name)) == 0) {
                printf("  Found device: %s (%s)\n", devices[count], name);
            } else {
                printf("  Found device: %s (could not read name)\n", devices[count]);
            }

            count++;
        }
    }
    closedir(dir);
    printf("Found %d IIO devices\n\n", count);
    return count;
}

/* 根据设备名称查找设备路径 */
static int find_device_by_name(const char *name, char *found_path) {
    char devices[MAX_DEVICES][MAX_PATH_LEN];
    int count = find_all_iio_devices(devices, MAX_DEVICES);

    if (count <= 0) {
        fprintf(stderr, "No IIO devices found\n");
        return -1;
    }

    printf("Searching for device with name: %s\n", name);

    for (int i = 0; i < count; i++) {
        char current_name[64];
        if (read_device_name(devices[i], current_name, sizeof(current_name)) == 0) {
            printf("  Checking device %s: name=%s\n", devices[i], current_name);

            // 检查名称是否匹配
            if (strcmp(current_name, name) == 0) {
                printf("  Found matching device: %s\n", devices[i]);
                strncpy(found_path, devices[i], MAX_PATH_LEN);
                return 0;
            }
        }
    }

    fprintf(stderr, "No device found with name: %s\n", name);
    return -1;
}

/* 检查设备是否包含指定类型的通道 */
static int device_has_channels(const char *dev_path, const char *type) {
    char test_path[MAX_PATH_LEN];
    const char *axes[] = {"x", "y", "z"};
    int found = 0;

    printf("Checking channels for %s in %s:\n", type, dev_path);

    for (int i = 0; i < 3; i++) {
        // 构建路径
        int len = snprintf(test_path, sizeof(test_path), "%s/in_%s_%s_raw", dev_path, type, axes[i]);
        if (len >= sizeof(test_path)) {
            fprintf(stderr, "Path too long: %s/in_%s_%s_raw\n", dev_path, type, axes[i]);
            continue;
        }

        if (access(test_path, F_OK) == 0) {
            printf("  Found: %s\n", test_path);
            found++;
        } else {
            printf("  Missing: %s (error: %s)\n", test_path, strerror(errno));
        }
    }

    return found == 3;  // 需要所有三个轴都存在
}

// 改进的通用初始化函数
static SensorContext icm42688_common_init(const char* params, const char* dev_path) {
    // 如果全局数据已存在，增加引用计数
    if (g_shared_data) {
        g_shared_data->ref_count++;
        printf("ICM42688: Reusing existing instance (ref count: %d)\n", g_shared_data->ref_count);
        return g_shared_data;
    }

    // 创建新的共享数据结构
    Icm42688_Data* data = (Icm42688_Data*)malloc(sizeof(Icm42688_Data));
    if (!data) {
        fprintf(stderr, "Failed to allocate memory for ICM42688 data\n");
        return NULL;
    }
    memset(data, 0, sizeof(Icm42688_Data));
    data->ref_count = 1;

    // 打印传入的设备路径
    printf("\nInitializing ICM42688 with device path: %s\n", dev_path);

    // 从传入路径读取设备名称
    char device_name[64];
    if (read_device_name(dev_path, device_name, sizeof(device_name)) != 0) {
        fprintf(stderr, "Failed to read device name from: %s\n", dev_path);
        free(data);
        return NULL;
    }
    printf("Device name: %s\n", device_name);

    // 检查设备类型
    int is_accel = device_has_channels(dev_path, "accel");
    int is_gyro = device_has_channels(dev_path, "anglvel");

    printf("Device type: ");
    if (is_accel && is_gyro) {
        printf("Combined accelerometer and gyroscope\n");
        // 如果是组合设备，使用同一个路径
        strncpy(data->accel_dev_path, dev_path, MAX_PATH_LEN);
        strncpy(data->gyro_dev_path, dev_path, MAX_PATH_LEN);
    } else if (is_accel) {
        printf("Accelerometer\n");
        strncpy(data->accel_dev_path, dev_path, MAX_PATH_LEN);

        // 查找陀螺仪设备 - 尝试不同的命名模式
        printf("\nSearching for gyroscope device...\n");
        char found_path[MAX_PATH_LEN];
        if (find_device_by_name("icm42688-gyro", found_path)) {
            fprintf(stderr, "Failed to find gyroscope device\n");
            free(data);
            return NULL;
        }

        if (!device_has_channels(found_path, "anglvel")) {
            fprintf(stderr, "Found gyroscope device but missing channels\n");
            free(data);
            return NULL;
        }

        strncpy(data->gyro_dev_path, found_path, MAX_PATH_LEN);
    } else if (is_gyro) {
        printf("Gyroscope\n");
        strncpy(data->gyro_dev_path, dev_path, MAX_PATH_LEN);

        // 查找加速度计设备 - 尝试不同的命名模式
        printf("\nSearching for accelerometer device...\n");
        char found_path[MAX_PATH_LEN];
        if (find_device_by_name("icm42688-accel", found_path)) {
            fprintf(stderr, "Failed to find accelerometer device\n");
            free(data);
            return NULL;
        }

        if (!device_has_channels(found_path, "accel")) {
            fprintf(stderr, "Found accelerometer device but missing channels\n");
            free(data);
            return NULL;
        }

        strncpy(data->accel_dev_path, found_path, MAX_PATH_LEN);
    } else {
        fprintf(stderr, "Device has neither accelerometer nor gyroscope channels\n");
        free(data);
        return NULL;
    }

    // 读取加速度计比例因子
    if (read_sensor_scale(data->accel_dev_path, "accel", &data->accel_scale) != 0) {
        fprintf(stderr, "Using default accel scale: 0.004788403\n");
        data->accel_scale = 0.004788403f; // ±16g时的默认值
    }

    // 读取陀螺仪比例因子
    if (read_sensor_scale(data->gyro_dev_path, "anglvel", &data->gyro_scale) != 0) {
        fprintf(stderr, "Using default gyro scale: 0.001065\n");
        data->gyro_scale = 0.001065f; // ±2000dps时的默认值
    }

    printf("\nICM42688: Initialization complete\n");
    printf("  Accel path: %s, scale: %f\n", data->accel_dev_path, data->accel_scale);
    printf("  Gyro path: %s, scale: %f\n", data->gyro_dev_path, data->gyro_scale);

    // 验证通道可访问性
    printf("\nVerifying channel accessibility:\n");
    const char *axes[] = {"x", "y", "z"};
    for (int i = 0; i < 3; i++) {
        char test_path[MAX_PATH_LEN];

        // 加速度计通道 - 安全构建路径
        int len = snprintf(test_path, sizeof(test_path), "%s/in_accel_%s_raw", data->accel_dev_path, axes[i]);
        if (len >= sizeof(test_path)) {
            fprintf(stderr, "Path too long: %s/in_accel_%s_raw\n", data->accel_dev_path, axes[i]);
        } else {
            if (access(test_path, R_OK) == 0) {
                printf("  Accel %s: accessible\n", axes[i]);
            } else {
                fprintf(stderr, "  Accel %s: not accessible (%s)\n", axes[i], strerror(errno));
            }
        }

        // 陀螺仪通道 - 安全构建路径
        len = snprintf(test_path, sizeof(test_path), "%s/in_anglvel_%s_raw", data->gyro_dev_path, axes[i]);
        if (len >= sizeof(test_path)) {
            fprintf(stderr, "Path too long: %s/in_anglvel_%s_raw\n", data->gyro_dev_path, axes[i]);
        } else {
            if (access(test_path, R_OK) == 0) {
                printf("  Gyro %s: accessible\n", axes[i]);
            } else {
                fprintf(stderr, "  Gyro %s: not accessible (%s)\n", axes[i], strerror(errno));
            }
        }
    }

    data->initialized = 1;
    g_shared_data = data; // 设置全局共享实例
    return data;
}

// 读取函数
static int icm42688_common_read(SensorContext context, ImuData* data) {
    Icm42688_Data* icm_data = (Icm42688_Data*)context;
    if (!icm_data || !icm_data->initialized) {
        fprintf(stderr, "ICM42688 not initialized\n");
        return -1;
    }

    int accel_x_raw, accel_y_raw, accel_z_raw;
    int gyro_x_raw, gyro_y_raw, gyro_z_raw;

    // 读取加速度计数据
    int accel_read = 0;
    if (read_raw_signed(icm_data->accel_dev_path, "accel", "x", &accel_x_raw) == 0 &&
        read_raw_signed(icm_data->accel_dev_path, "accel", "y", &accel_y_raw) == 0 &&
        read_raw_signed(icm_data->accel_dev_path, "accel", "z", &accel_z_raw) == 0) {
        accel_read = 1;
    } else {
        fprintf(stderr, "Failed to read accelerometer data\n");
    }

    // 读取陀螺仪数据
    int gyro_read = 0;
    if (read_raw_signed(icm_data->gyro_dev_path, "anglvel", "x", &gyro_x_raw) == 0 &&
        read_raw_signed(icm_data->gyro_dev_path, "anglvel", "y", &gyro_y_raw) == 0 &&
        read_raw_signed(icm_data->gyro_dev_path, "anglvel", "z", &gyro_z_raw) == 0) {
        gyro_read = 1;
    } else {
        fprintf(stderr, "Failed to read gyroscope data\n");
    }

    // 转换为实际物理值
    if (accel_read) {
        data->ax = accel_x_raw * icm_data->accel_scale;
        data->ay = accel_y_raw * icm_data->accel_scale;
        data->az = accel_z_raw * icm_data->accel_scale;
    } else {
        data->ax = 0.0f;
        data->ay = 0.0f;
        data->az = 0.0f;
    }

    if (gyro_read) {
        data->gx = gyro_x_raw * icm_data->gyro_scale;
        data->gy = gyro_y_raw * icm_data->gyro_scale;
        data->gz = gyro_z_raw * icm_data->gyro_scale;
    } else {
        data->gx = 0.0f;
        data->gy = 0.0f;
        data->gz = 0.0f;
    }

    // 使用系统时间作为时间戳
    data->timestamp = get_current_timestamp_us();

    // ICM42688没有磁力计
    data->mx = 0.0f;
    data->my = 0.0f;
    data->mz = 0.0f;

    // 状态正常
    data->status = 0;

    return (accel_read && gyro_read) ? 0 : 1; // 部分成功返回1，完全成功返回0
}

// 释放函数
static void icm42688_common_release(SensorContext context) {
    Icm42688_Data* icm_data = (Icm42688_Data*)context;
    if (!icm_data) return;

    printf("ICM42688: Release called (current ref count: %d)\n", icm_data->ref_count);
    icm_data->ref_count--;

    if (icm_data->ref_count <= 0) {
        printf("ICM42688: Releasing resources\n");
        free(icm_data);
        g_shared_data = NULL; // 清除全局引用
    }
}

// ICM42688传感器驱动实现（陀螺仪）
const SensorDriver icm42688_driver_gyro = {
    .name = "icm42688-gyro",
    .init = icm42688_common_init,
    .read = icm42688_common_read,
    .release = icm42688_common_release
};

// ICM42688传感器驱动实现（加速度计）
const SensorDriver icm42688_driver_accel = {
    .name = "icm42688-accel",
    .init = icm42688_common_init,
    .read = icm42688_common_read,
    .release = icm42688_common_release
};