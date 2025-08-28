wasm_video_player 中包含3部分程序
# 基于wasm的FFMPEG的编译环境
1. dist: ffmpeg源码编译后的安装目录：包含库和头文件
2. build_decoder.sh： 编译FFMPEG源码库的脚本
3. build_decoder_wasm.sh：编译decode.c的脚本

注意：build_decoder.sh 与 build_decoder_wasm.sh需要提前安装emsdk：https://emscripten.org/docs/getting_started/downloads.html

# WasmVideoPlayer
1. 解码器相关：WasmDecoder.js decoder.c libffmpeg.js libffmpeg.wasm RingBuffer.js DecoderCmd.js
2. 显示器相关：webgl.js
3. 播放器相关：WasmVideoPlayer.js
4. 拉流器相关：LiveStreamPullerCmd.js LiveStreamPuller.js

# 单测环境
1. server：server.js package.json  package-lock.json
2. 资源文件：index.html styles