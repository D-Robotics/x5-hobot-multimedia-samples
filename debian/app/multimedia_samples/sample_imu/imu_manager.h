/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

// 定义传感器数据结构
typedef struct {
    float ax, ay, az;     // 加速度计数据
    float gx, gy, gz;     // 陀螺仪数据
    float mx, my, mz;     // 磁力计数据
    uint64_t timestamp;       // 时间戳
    int status;           // 状态码
} ImuData;

// 传感器句柄类型
typedef void* SensorHandle;

// 列出所有可用的传感器
void list_available_sensors(void);

// 初始化指定类型的传感器
SensorHandle init_sensor(const char* sensor_type, const char* params);

// 读取传感器数据
int read_sensor_data(SensorHandle handle, ImuData* data);

// 释放传感器资源
void release_sensor(SensorHandle handle);

// 打印IMU数据
void print_imu_data(const ImuData* data);

// 获取默认传感器名称
const char* get_default_imu_name();

#endif // SENSOR_MANAGER_H