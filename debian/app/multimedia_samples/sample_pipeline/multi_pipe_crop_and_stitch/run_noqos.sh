#!/bin/bash
# 配置cpu bpu ddr 降频的结温温度
echo 105000 > /sys/class/thermal/thermal_zone0/trip_point_0_temp
echo 105000 > /sys/class/thermal/thermal_zone1/trip_point_1_temp

# 设置cpu运行在高性能模式
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor


modprobe panel-jc-050hd134
modprobe galcore
modprobe vio_n2d
modprobe lontium_lt8618
modprobe vs-x5-syscon-bridge
modprobe vs_drm
# ./multi_pipe_crop_and_stitch -c "sensor=8" -c "sensor=8" -o hdmi -b -v -g
./multi_pipe_crop_and_stitch -c "sensor=8" -c "sensor=8" -o hdmi -b -v -p -g