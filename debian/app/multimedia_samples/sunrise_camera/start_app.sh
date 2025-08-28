#!/bin/sh

set -e

local_path=$(dirname "$(readlink -f "$0")")

# 配置cpu bpu ddr 降频的结温温度
echo 105000 > /sys/class/thermal/thermal_zone0/trip_point_0_temp
echo 105000 > /sys/class/thermal/thermal_zone1/trip_point_1_temp

# 设置cpu运行在高性能模式
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor

cd "${local_path}"/sunrise_camera/bin || exit 1
echo "============= Start Sunrise Camera ==============="
export LD_LIBRARY_PATH=../bin:"${LD_LIBRARY_PATH}"
if [ "$#" -eq 0 ]; then
	./sunrise_camera
else
	gdb -ex "handle SIGUSR2 nostop" -ex "handle SIGPIPE nostop" -ex "run"  ./sunrise_camera
fi
