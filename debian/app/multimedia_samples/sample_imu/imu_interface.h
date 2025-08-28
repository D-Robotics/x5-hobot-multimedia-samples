/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright(C) 2024-2025, D-Robotics Co., Ltd.
 *                     All rights reserved.
 ***************************************************************************/

#ifndef SAMPLE_IMU_H
#define SAMPLE_IMU_H

#include "imu_manager.h"

// 传感器上下文类型（由具体实现定义和实现，可以理解为一个占位的虚拟类型）
typedef void* SensorContext;

// 传感器驱动函数类型
typedef SensorContext (*SensorInitFunc)(const char* params, const char* dev_path);
typedef int (*SensorReadFunc)(SensorContext context, ImuData* data);
typedef void (*SensorReleaseFunc)(SensorContext context);

// 传感器驱动结构
typedef struct SensorDriver {
    const char* name;
    SensorInitFunc init;
    SensorReadFunc read;
    SensorReleaseFunc release;
} SensorDriver;

// 传感器实例结构（内部使用）
typedef struct SensorInstance {
    const SensorDriver* driver;
    SensorContext context;
} SensorInstance;

#endif // SENSOR_INTERFACE_H