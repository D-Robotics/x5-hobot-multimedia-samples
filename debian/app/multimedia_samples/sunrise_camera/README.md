# X5 Platform SDK应用参考示例

基于X5 Platform SDK的应用参考示例，包含单个模块功能示例以及多模块组合复杂场景示例，方便用户快速体验及开发自己的应用。

## 工程结构

工程结构目录如下，按照功能和场景进行命名区分。其中：

- gdc、isp、sif、video_codec等目录中是对应的IP使用示例代码。
- pipelines目录中包含多个IP串联为图像通路的示例代码。
- utils目录包含通用的函数及结构体
- Makefile为顶层Makefile，在该路径下执行make可一次完成全部示例代码编译

```shell
.
├── gdc
├── isp
├── Makefile
├── Makefile.in
├── pipelines
├── README.md
├── sensors
├── sif
├── utils
└── video_codec
```

## 示例功能说明

### SENSORS

该目录下包含已支持的Sensor和配置类型，不产生单独的示例。Sensor添加方式可参考sensors/README.md

### VIN

#### read_raw_data

read_raw_data示例获取sensor发出的RAW数据。

使用方式：

	1. read_raw_data -h 查看当前已支持Sensor及配置类型

	2. read_raw_data -s/--sensor sensor_index 选择对应的sensor_index号运行示例

当前示例每个60帧保存一张RAW图至当前运行目录下。

### ISP

#### read_isp_data

read_isp_data示例获取Sensor经过ISP处理后的图像。

使用方式：

	1. read_isp_data -h 查看当前已支持Sensor及配置类型

	2. read_isp_data -s/--sensor sensor_index 选择对应的sensor_index号运行示例

当前示例每个60帧保存一张NV12图至当前运行目录下。

### GDC

#### basic

basic示例读取本地NV12图片，使用指定的gdc.bin配置文件，送入GDC处理后将结果保存为本地NV12图片

使用方式：

	gdc_basic --file gdc_file --image image_file --iw input_width --ih input_height --ow output_width --oh output_height

其中，

	gdc_file为gdc.bin配置文件路径

	image_file为输入NV12图像路径

	input_width为输入图像水平方向分辨率

	input_height为输入图像垂直方向分辨率

	output_width为输出图像水平方向分辨率

	output_height为输出图像垂直方向分辨率

#### generate_bin

generate_bin示例读取本地custom_config.txt配置文件，生成对应的gdc.bin文件。custom_config.txt生成方式可参考custom_config目录下的generate_custom_txt.py在PC上运行实现。

使用方式：

	genereate_bin -c json_config_file [-o output_file]

其中，

	json_config_file为json配置文件路径，参考配置在custom_config/gdc_bin_custom_config.json

	output_file为输出文件名子，默认为gdc.bin

### VSE

### VIDEO_CODEC

### PIPELINES

#### single_pipe_vin_isp_vse

single_pipe_vin_isp_vse示例串联VIN、ISP、VSE三个模块，是最基础的模块串联示例之一。sensor图像经过VIN、ISP处理后到达VSE模块，VSE模块配置了三个输出通道功能分别如下：
- 0通道输出ISP处理后的原尺寸图像
- 1通道输出ISP处理后的原尺寸的一半缩放图像 
- 2通道输出ISP处理后的以原图中心为中心，宽高各一半的剪裁图像

使用方式：

	1. single_pipe_vin_isp_vse -h 查看当前已支持Sensor及配置类型

	2. single_pipe_vin_isp_vse -s/--sensor sensor_index 选择对应的sensor_index号运行示例

当前示例每个60帧保存一张个输出通道的NV12图至当前运行目录下。