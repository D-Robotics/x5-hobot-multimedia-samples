#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

echo "build script path: ${SCRIPT_DIR}"

#获取代码
git clone https://gitee.com/xia-chu/ZLMediaKit
cd ${SCRIPT_DIR}/ZLMediaKit
git submodule update --init
git checkout 479a3fb9bbc4e18d935032769e042c17300bf1bf

#给ZLMediaKit 打patch
git am ${SCRIPT_DIR}/0001-Compatible-with-X5.patch

#给media-server子仓库打Patch
cd ${SCRIPT_DIR}/ZLMediaKit/3rdpart/media-server
git am ${SCRIPT_DIR}/0001-Compatible-with-X5-for-media-server.patch

#编译并安装
cd ${SCRIPT_DIR}/ZLMediaKit
./build_x5.sh