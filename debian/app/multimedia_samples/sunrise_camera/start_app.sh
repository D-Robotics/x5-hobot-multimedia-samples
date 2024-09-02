#!/bin/sh

set -e

local_path=$(dirname "$(readlink -f "$0")")

# 配置cpu bpu ddr 降频的结温温度
echo 105000 > /sys/class/thermal/thermal_zone0/trip_point_0_temp
echo 105000 > /sys/class/thermal/thermal_zone1/trip_point_1_temp

# 设置cpu运行在高性能模式
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor

# Start web server
issue=$(cat /etc/issue)

if echo "$issue" | grep -q "Buildroot"; then
    echo "============= Start Web Server ==============="
    cd "${local_path}/WebServer" || exit 1
    ./start_lighttpd.sh || true
elif echo "$issue" | grep -q "Ubuntu"; then
    python -m http.server 80 -d "${local_path}/WebServer/sc_lighttpd/webpages" &
fi

modprobe panel-jc-050hd134
modprobe galcore
modprobe vio_n2d
modprobe lontium_lt8618
modprobe vs-x5-syscon-bridge
modprobe vs_drm

cd "${local_path}"/sunrise_camera/bin || exit 1
echo "============= Start Sunrise Camera ==============="
export LD_LIBRARY_PATH=../bin:"${LD_LIBRARY_PATH}"
./sunrise_camera
