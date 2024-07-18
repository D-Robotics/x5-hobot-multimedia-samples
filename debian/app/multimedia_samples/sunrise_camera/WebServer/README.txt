WebServer
1. 使用lighttpd作为web服务
2. 支持fastcgi
3. 包含 web 页面文件
4. 支持 h264 视频流无插件解码
5. 通过websocket、fastcgi和主程序sunrise_camera通信

部署：
1. 临时测试使用：
	把WebServer整个程序目录软链接到 /userdata/web 目录，然后执行sc_lighttpd中的 lighttpd, 请参考 start_lighttpd.sh

2. 产品化部署：
	把程序拷贝到 /userdata/web，然后添加相应的自启动配置

编译 pcre：
下载 pcre-8.44.tar.bz2
解压后进入到目录中执行以下命令编译
CROSS_COMPILE=/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC=${CROSS_COMPILE}gcc
AR=${CROSS_COMPILE}ar
LD=${CROSS_COMPILE}ld
RANLIB=${CROSS_COMPILE}ranlib
STRIP=${CROSS_COMPILE}strip
./configure --prefix=`pwd`/pcre-8.44 CC=${CROSS_COMPILE}gcc --host=aarch64-linux-gnu

编译 fcgi-2.4.0
fcgi现在有更新的版本，但是建议还是下载2.4.0版本使用
解压后执行一下命令编译
CROSS_COMPILE=/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC=${CROSS_COMPILE}gcc
AR=${CROSS_COMPILE}ar
LD=${CROSS_COMPILE}ld
RANLIB=${CROSS_COMPILE}ranlib
STRIP=${CROSS_COMPILE}strip
./configure --prefix=`pwd`/fcgi-2.4.0 CC=${CROSS_COMPILE}gcc --host=aarch64-none-linux-gnu

编译lighttpd
下载最新版本的lighttpd-1.4.57
解压后，执行命令编译，编译选线需要设置上pcre的头文件和库路径
CROSS_COMPILE=/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC=${CROSS_COMPILE}gcc
AR=${CROSS_COMPILE}ar
LD=${CROSS_COMPILE}ld
RANLIB=${CROSS_COMPILE}ranlib
STRIP=${CROSS_COMPILE}strip
./configure --prefix=`pwd`/sc_lighttpd --host=aarch64-none-linux-gnu \
    --enable-shared --disable-static --disable-lfs --disable-ipv6 \
    --without-valgrind \
    --without-zlib --without-bzip2 --without-lua \
    --with-pcre CFLAGS=-I/home/hobot/2-x5-dev/01-x5-gerrit/out/deploy/system/usr/include \
    --with-openssl --with-openssl-includes=/home/hobot/2-x5-dev/01-x5-gerrit/out/deploy/system/usr/include/openssl/include/ \
    --with-openssl-libs=/openssl/lib \
    LDFLAGS=-L/home/hobot/2-x5-dev/01-x5-gerrit/out/deploy/system/usr/lib
