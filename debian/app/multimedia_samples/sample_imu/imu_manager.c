/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#include "imu_manager.h"
#include "imu_interface.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define SYSFS_PATH "/sys/bus/iio/devices/"
#define MAX_PATH_LEN 512

// 前向声明具体传感器实现
extern const struct SensorDriver bmi08x_driver;
extern const struct SensorDriver icm42688_driver_gyro;
extern const struct SensorDriver icm42688_driver_accel;
// extern const struct SensorDriver asm330_driver;
// extern const struct SensorDriver iam20685_driver;

// 可用传感器驱动列表
static const struct SensorDriver* drivers[] = {
    &bmi08x_driver,
    &icm42688_driver_gyro,
    &icm42688_driver_accel,
    // &asm330_driver,
    // &iam20685_driver,
    NULL  // 列表结束标记
};

// 打印所有IIO设备信息
// 打印所有IIO设备信息
static void list_all_iio_devices(void) {
    printf("\n=== Detected IIO Devices ===\n");

    DIR *dp = opendir(SYSFS_PATH);
    if (!dp) {
        fprintf(stderr, "Can't open %s: %s\n", SYSFS_PATH, strerror(errno));
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dp))) {
        if (strstr(ent->d_name, "iio:device")) {
            char device_path[MAX_PATH_LEN];
            snprintf(device_path, sizeof(device_path),
                    SYSFS_PATH "%s", ent->d_name);

            // 读取设备名称
            char name_path[MAX_PATH_LEN];
            strcpy(name_path, device_path);
            strcat(name_path, "/name");

            FILE *name_fp = fopen(name_path, "r");

            char device_name[64] = "unknown";
            if (name_fp) {
                if (fgets(device_name, sizeof(device_name), name_fp)) {
                    // 移除换行符
                    device_name[strcspn(device_name, "\n")] = 0;
                }
                fclose(name_fp);
            }

            printf("  Device: %-15s | Name: %s\n", ent->d_name, device_name);
        }
    }
    closedir(dp);
    printf("============================\n\n");
}

// // 打印支持的传感器列表
// static void print_supported_sensors(void) {
//     printf("=== Supported Sensors ===\n");
//     for (int i = 0; drivers[i] != NULL; i++) {
//         printf("  - %s\n", drivers[i]->name);
//     }
//     printf("=========================\n\n");
// }

// 检查文件访问权限
static int check_file_access(const char *path) {
    if (access(path, F_OK)) {
        fprintf(stderr, "File not found: %s\n", path);
        return -1;
    }
    if (access(path, R_OK)) {
        fprintf(stderr, "No read permission: %s (try with sudo)\n", path);
        return -1;
    }
    return 0;
}

// 完整设备验证
static int validate_iio_device(const char *dev_path, const char *expected_name) {
    char test_path[MAX_PATH_LEN];
    struct stat st;
    int accel_found = 0;
    int gyro_found = 0;

    // 检查设备目录有效性
    if (stat(dev_path, &st) || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Invalid IIO device path: %s\n", dev_path);
        return -1;
    }

    // 验证设备名称
    snprintf(test_path, sizeof(test_path), "%s/name", dev_path);
    FILE *name_fp = fopen(test_path, "r");
    if (!name_fp) {
        perror("Failed to open name file");
        return -1;
    }

    char actual_name[64];
    if (!fgets(actual_name, sizeof(actual_name), name_fp) ||
        strcmp(actual_name, expected_name) != 0) {
        fprintf(stderr, "Unexpected device name: %s \nyour chose imu: %s\n",
                actual_name, expected_name);
        fclose(name_fp);
        return -1;
    }
    fclose(name_fp);

    const char *axes[] = {"x", "y", "z"};
    // 检查加速度计通道，找到一个就+1
    for (int i = 0; i < 3; i++) {
        snprintf(test_path, sizeof(test_path), "%s/in_accel_%s_raw",
                dev_path, axes[i]);
        if (check_file_access(test_path))
            accel_found += 1;
    }

    // 检查陀螺仪通道
    for (int i = 0; i < 3; i++) {
        snprintf(test_path, sizeof(test_path), "%s/in_anglvel_%s_raw",
                dev_path, axes[i]);
        if (check_file_access(test_path))
            gyro_found += 1;
    }

    // 验证设备至少有一个有效通道
    if (!accel_found && !gyro_found) {
        fprintf(stderr, "No valid IMU channels found at: %s\n", dev_path);
        return -1;
    }

    printf("Device validation passed at: %s\n", dev_path);
    return 0;
}

// 查找并验证IIO设备
static const char* find_valid_iio_device(const char* sensor_type, char *dev_path) {
    DIR *dp = opendir(SYSFS_PATH);
    if (!dp) {
        fprintf(stderr, "Can't open %s: %s\n", SYSFS_PATH, strerror(errno));
        return NULL;
    }

    struct dirent *ent;
    while ((ent = readdir(dp))) {
        if (strstr(ent->d_name, "iio:device")) {
            char current_path[MAX_PATH_LEN];
            snprintf(current_path, sizeof(current_path),
                    SYSFS_PATH "%s", ent->d_name);

            char expected_name[64];
            snprintf(expected_name, sizeof(expected_name), "%s\n", sensor_type);
            if (validate_iio_device(current_path, expected_name) == 0) {
                strncpy(dev_path, current_path, MAX_PATH_LEN);
                closedir(dp);
                // printf("dev_path = %s , current_path = %s , sensor_type = %s\n",
                //     dev_path , current_path , sensor_type);
                return sensor_type;
            }
        }
    }

    closedir(dp);
    fprintf(stderr, "\nNo valid IIO device name found for '%s'.\n", sensor_type);
    return NULL;
}

// 列出所有可用的传感器，这里没有去搜索 IIO 中的内容，是列举程序中支持的传感器型号
void list_available_sensors(void) {
    printf("Supported sensors: ");
    for (int i = 0; drivers[i] != NULL; i++) {
        printf("%s ", drivers[i]->name);
    }
    printf("\n");
}

// 初始化指定类型的传感器
SensorHandle init_sensor(const char* sensor_type, const char* params) {
    // 列出所有IIO设备
    list_all_iio_devices();

    // // 打印支持的传感器列表
    // print_supported_sensors();

    // // 显示当前选择的传感器
    // printf("=== Current Selected Sensor ===\n");
    // printf("  - %s\n", sensor_type);
    // printf("===============================\n\n");

    char dev_path[MAX_PATH_LEN];
    const char* found_sensor = find_valid_iio_device(sensor_type, dev_path);
    if (!found_sensor) {
        return NULL;
    }

    for (int i = 0; drivers[i] != NULL; i++) {
        if (strcmp(drivers[i]->name, sensor_type) == 0) {
            // 分配SensorInstance结构体
            SensorInstance* instance = (SensorInstance*)malloc(sizeof(SensorInstance));
            if (!instance) {
                printf("Error: Failed to allocate sensor instance\n");
                return NULL;
            }
            instance->driver = drivers[i];
            // 传递设备路径作为额外参数
            instance->context = drivers[i]->init(params, dev_path);
            if (!instance->context) {
                free(instance);
                return NULL;
            }
            return (SensorHandle)instance; // 返回实例指针
        }
    }
    printf("Error: Unknown sensor type '%s'\n", sensor_type);
    return NULL;
}

// 读取传感器数据
int read_sensor_data(SensorHandle handle, ImuData* data) {
    // 从句柄中提取驱动指针
    struct SensorInstance* instance = (struct SensorInstance*)handle;
    if (!instance || !instance->driver || !instance->driver->read) {
        return -1;
    }

    return instance->driver->read(instance->context, data);
}

// 释放传感器资源
void release_sensor(SensorHandle handle) {
    struct SensorInstance* instance = (struct SensorInstance*)handle;
    if (instance && instance->driver && instance->driver->release) {
        instance->driver->release(instance->context);
        free(instance);
    }
}

// 将微秒时间戳转换为 时:分:秒.毫秒.微秒 格式并打印
void print_formatted_timestamp(uint64_t microseconds) {
    uint64_t hours = microseconds / 3600000000ULL;
    uint64_t minutes = (microseconds % 3600000000ULL) / 60000000ULL;
    uint64_t seconds = (microseconds % 60000000ULL) / 1000000ULL;
    uint64_t milliseconds = (microseconds % 1000000ULL) / 1000ULL;
    uint64_t microsec_part = microseconds % 1000ULL;

    printf("%02lu:%02lu:%02lu.%03lu.%03lu",
           hours, minutes, seconds, milliseconds, microsec_part);
}

// 打印IMU数据
void print_imu_data(const ImuData* data) {
    printf("  Accelerometer: [%f, %f, %f] m/s²\n", data->ax, data->ay, data->az);
    printf("  Gyroscope:     [%f, %f, %f] rad/s\n", data->gx, data->gy, data->gz);

    /* 有的传感器有磁力计，有的没有，可以根据实际情况来区分处理 */
    // printf("  Magnetometer:  [%f, %f, %f] μT\n", data->mx, data->my, data->mz);

    /* Timestamp 这里只是示例，实际情况可以根据需要来修改,比如 bmi08x 的IMU，
     * 我们通过代码可以获取到 us ，所以即可以直接打印 us ，也可以通过
     * print_formatted_timestamp 转换函数，将 us 转换成我们方便看的时间格式。
     */
    // printf("  Timestamp:     %ld\n", data->timestamp);
    printf("  Timestamp:     ");
    print_formatted_timestamp(data->timestamp);
    printf("\n");

    // printf("  Status:        %d\n", data->status);
}

// 获取默认传感器名称
const char* get_default_imu_name() {
    char dev_path[MAX_PATH_LEN];
    for (int i = 0; drivers[i] != NULL; i++) {
        if (find_valid_iio_device(drivers[i]->name, dev_path)) {
            return drivers[i]->name;
        }
    }
    return NULL;
}