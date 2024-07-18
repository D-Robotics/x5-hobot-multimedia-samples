# sample_codec 使用说明

## 简介

sample_codec 是一个用于编解码视频的示例程序。它可以根据配置文件（`codec_config.ini`）进行视频编码和解码，用于调试编解码器。

## 编译

本模块代码存放在 SDK 的 `app/samples/platform_samples` 目录下，进入目录后，make即可生成 sample_codec 可执行程序。

```
cd $SDK_PATH/app/samples/platform_samples/sample_codec
make
```
## 使用说明

把编译生成的 sample_codec 、 codec_config.ini 和测试需要用到的的图像、视频码流文件（1920x1080.yuv、 *.264、*.265等）上传到开发板的 /userdata 目录下，然后执行 sample_codec 命令即可运行。

### 命令行选项
- -f, --config_file FILE：指定配置文件的路径。

- -e, --encode [OPTION]：设置编码选项（可选）。如果使用此选项，则会覆盖配置文件中的 `encode_streams` 选项。
- -d, --decode [OPTION]：设置解码选项（可选）。如果使用此选项，则会覆盖配置文件中的 `decode_streams` 选项。
- -v, --verbose：启用详细模式，显示更多日志信息。
- -h, --help：显示帮助信息。

### 使用示例

- 默认使用 `codec_config.ini` 进行编解码，执行程序时不需要带任何参数：

```
./sample_codec
```

- 启动指定的编码流（在 `codec_config.ini` 中定义）：

```
sample_codec -e 0x1  # 启动 venc_stream1
sample_codec -e 0x3  # 启动 venc_stream1 和 venc_stream2
```

- 启动指定的解码流（在 `codec_config.ini` 中定义）：

```
sample_codec -d 0x1  # 启动 vdec_stream1
sample_codec -d 0x3  # 启动 vdec_stream1 和 vdec_stream2
```

- 启用详细模式以获取更多日志信息：

```
sample_codec -v
```

- 显示帮助信息：

```
sample_codec -h
```

## 配置文件

在 `codec_config.ini` 配置中定义了不同的视频编码和解码参数，设置默认启动的编解码通道数量。

其中编码参数选项说明如下：

```
[encode]
; 启用编码, 按位运算
; 0x0 表示不启用编码
; 0x01表示只启用 venc_stream1 编码流
; 0x02表示只启用 venc_stream2 编码流
; 0x03表示只启用前两路编码流（venc_stream1 和 venc_stream2）, 0x07表示启用前三路编码流, 0x0f表示启用前三路编码流，以此类推
encode_streams = 0x1

[venc_stream1]
; 编码类型（0：H264，1：H265，2：MJPEG， 3：JPEG）
codec_type = 0
width = 1920
height = 1080
frame_rate = 30
bit_rate = 8192
input = 1920x1080.yuv
output = 1920x1080_30fps.264
frame_num = 100

[decode]
; 启用解码，按位运算
; 0x0 表示不启用解码
; 0x01表示只启用 vdec_stream1 解码流
; 0x02表示只启用 vdec_stream2 解码流
; 0x03表示只启用前两路解码流（vdec_stream1 和 vdec_stream2）, 0x07表示启用前三路解码流, 0x0f表示启动前四路解码流，以此类推
decode_streams = 0x0

[vdec_stream1]
; 编码类型（0：H264，1：H265，2：MJPEG， 3：JPEG）
codec_type = 0
width = 1920
height = 1080
input = 1920x1080_30fps.264
output = 1920x1080.yuv
```

### [encode]

- **encode_streams**：此选项用于指定要启用的编码流。采用按位运算的方式表示。例如，`0x1` 表示只启用 `venc_stream1` 编码流，`0x3` 表示启用前两路编码流（`venc_stream1` 和 `venc_stream2`）。**本选项的值会被命令行参数 -e 所覆盖**

### [venc_stream]

- **codec_type**：指定编码的类型，可选值为 `0`（H264）、`1`（H265）、`2`（MJPEG）和 `3`（JPEG）。
- **width**：视频帧的宽度。
- **height**：视频帧的高度。
- **frame_rate**：视频的帧率。
- **bit_rate**：视频的比特率。
- **input**: 输入图像文件，仅支持 NV12 格式的 yuv 图像。一个文件中可以连续存放多帧图像，编码时会顺序、循环读取每一帧图像。
- **output**: 输出编码后的视频文件。
- **frame_num**: 要编码的视频帧数。如果输入图像文件中的图像帧数少于本参数的值，编码时会循环读取图像文件，直到达到或超过 frame_num 指定的帧数。

### [decode]

- **decode_streams**：此部分用于指定要启用的解码流。采用按位运算的方式表示。例如，`0x1` 表示只启用 `vdec_stream1` 解码流，`0x3` 表示启用前两路解码流（`vdec_stream1` 和 `vdec_stream2`）。

### [vdec_stream]

- **codec_type**：指定解码的视频编码类型，可选值为 `0`（H264）、`1`（H265）、`2`（MJPEG）和 `3`（JPEG）。
- **width**：解码后视频帧的宽度。
- **height**：解码后视频帧的高度。
- **input**：输入待解码的视频文件路径，根据 `codec_type` 的设置，可以使用码流文件和rtsp码流。
- **output**：输出解码后的图像文件路径，仅支持输出 NV12 格式的 yuv 图像。解码后的图像会连续保存到一个文件中，因此请确保输出路径有足够的磁盘空间。请注意，YUV 图像通常占用较大的磁盘空间，特别是对于高分辨率和长时长的视频文件，可能会占用大量存储空间。在选择输出路径时，请确保目标存储设备有足够的可用空间。