#!/bin/bash

set -e
set -o pipefail

# 定义错误退出函数
error_exit() {
    echo "❌ 错误: $1" >&2
    exit 1
}
# 检查 emcc 是否存在
if ! command -v emcc >/dev/null 2>&1; then
    error_exit "emcc 未找到！请先安装 Emscripten 工具链：\n👉 https://emscripten.org/docs/getting_started/downloads.html"
fi

# 检查并删除 libffmpeg.wasm / libffmpeg.js（如果存在）
for file in libffmpeg.wasm libffmpeg.js; do
    if [ -f "$file" ]; then
        log_info "删除旧文件: $file"
        rm -f "$file" || error_exit "无法删除 $file"
    else
        log_info "$file 不存在，跳过删除"
    fi
done

export TOTAL_MEMORY=67108864
export EXPORTED_FUNCTIONS="[ \
    '_openDecoder', \
    '_closeDecoder', \
	'_decodeData', \
	'_flushDecoder', \
    '_main',
    '_malloc',
    '_free'
]"

echo "Running Emscripten..."
emcc decoder.c dist/lib/libavformat.a dist/lib/libavcodec.a dist/lib/libavutil.a dist/lib/libswscale.a \
    -O3 \
    -I "dist/include" \
    -s WASM=1 \
    -s TOTAL_MEMORY=${TOTAL_MEMORY} \
    -s EXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS}" \
    -s EXTRA_EXPORTED_RUNTIME_METHODS="['addFunction']" \
    -s RESERVED_FUNCTION_POINTERS=14 \
    -s FORCE_FILESYSTEM=1 \
    -o libffmpeg.js

echo "Finished Build"
