#!/bin/bash

set -e
set -o pipefail

# 定义错误退出函数
error_exit() {
    echo "❌ 错误: $1" >&2
    exit 1
}

# 获取当前脚本的绝对路径（便于后续回退）
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

# 检查当前目录是否存在 ffmpeg 目录，不存在才克隆
if [ ! -d "ffmpeg" ]; then
    echo "🔄 ffmpeg 目录不存在，开始克隆..."

    # 克隆 ffmpeg
    git clone https://git.ffmpeg.org/ffmpeg.git || error_exit "git clone ffmpeg 失败"
    cd ffmpeg || error_exit "cd ffmpeg 失败"

    # 切换到指定 commit
    git checkout d1616046258acb990c61d82dd40218067f0a4de8 || error_exit "git checkout 失败"

    # 返回原来的目录（可选项，如果后续操作需要）
    cd "$SCRIPT_DIR" || error_exit "返回主目录失败"
else
    echo "✅ ffmpeg 目录已存在，跳过克隆"
fi

echo "Beginning Build:"
if [ ! -d "ffmpeg" ]; then
	rm -r dist
fi
mkdir -p dist

# 检查 emconfigure 是否存在
if ! command -v emconfigure >/dev/null 2>&1; then
    error_exit "emconfigure 未找到！请先安装 Emscripten 工具链：\n👉 https://emscripten.org/docs/getting_started/downloads.html"
fi

echo "emconfigure"
emconfigure ./configure --cc="emcc" --cxx="em++" --ar="emar" --ranlib="emranlib" --prefix=$(pwd)/../WasmVideoPlayer/dist --enable-cross-compile --target-os=none \
        --arch=x86_32 --cpu=generic --enable-gpl --enable-version3 --disable-avdevice --disable-swresample --disable-postproc --disable-avfilter \
        --disable-programs --disable-logging --disable-everything --enable-avformat --enable-decoder=hevc --enable-decoder=h264 --enable-decoder=aac \
        --disable-ffplay --disable-ffprobe --disable-asm --disable-doc --disable-devices --disable-network --disable-hwaccels \
        --disable-parsers --disable-bsfs --disable-debug --enable-protocol=file --enable-demuxer=mov --enable-demuxer=flv --disable-indevs --disable-outdevs --enable-parser=hevc
if [ -f "Makefile" ]; then
  echo "make clean"
  make clean
fi
echo "make"
make
echo "make install"
make install
cd ../WasmVideoPlayer
./build_decoder_wasm.sh
