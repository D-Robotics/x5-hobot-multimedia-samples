# 添加新Camera Sensor方法

vp_sensors 目录用于统一存放 camera sensor 的配置信息。本目录中的sensor配置会被vin、isp、pipeline、sunrise camera等模块使用。

## 目录结构

vp_sensors目录结构如下，其中sc1330t目录表示该目录下是sc1330t这颗sensor的不同配置文件，vp_sensors.c和vp_sensors.h包含sensor共用的结构体。

```shell
.
├── README.md
├── sc1330t
│   └── linear_1280x960_raw10_30fps_1lane.c
... (省略) ...
├── vp_sensors.c
└── vp_sensors.h
```

## Sensor添加步骤

1. 在vp_sensors目录下新建目录，命名为sensor名称

2. 在sensor名称目录下创建命名格式为 <mode>_<width>x<height>_<raw_bit>_<fps>_<lane>.c 的 sensor 配置文件。以linear_1280x960_raw10_30fps_1lane.c为例解释各个字段的含义：

 - mode: 表示sensor的工作模式。支持设置为 linear 或者 hdr 模式

 - width: 表示 sensor 的水平方向分辨率

 - height: 表示 sensor 的垂直方向分辨率

 - raw_bit: 表示 sensor 输出的图像数据位宽，如raw8，raw10，raw12，raw16

 - fps: 表示 sensor 输出的的帧率，如30fps

 - lane: 表示 sensor 使用到的MIPI CSI 的 data lane 数量

3. 在创建的sensor配置文件中填写 sesor 配置参数，可参考linear_1280x960_raw10_30fps_1lane.c，其中 vp_sensor_config_t 变量命名规则为sensor名称_配置文件名称，如 sc1330t_linear_1280x960_raw10_30fps_1lane

4. 在vp_sensors.c文件中增加新的sensor信息

```c
vp_sensor_config_t *vp_sensor_config_list[] = {
		&sc1330t_linear_1280x960_raw10_30fps_1lane,
		&sensor_config,
};

```