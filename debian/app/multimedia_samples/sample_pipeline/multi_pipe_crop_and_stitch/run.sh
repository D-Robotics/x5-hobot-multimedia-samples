#!/bin/bash

function config_noc_qos_max()
{
    noc_base=$1
    noc_reg=$(( $noc_base + 12 ))
    echo "update noc qos to fixed(0)"
    devmem ${noc_reg} 32 0x0

    noc_reg=$(( $noc_base + 8 ))
    echo "update noc p1/p0 as max(7)"
    devmem ${noc_reg} 32 0x00000707
}

echo 1 > /sys/kernel/debug/clk/bt1120_aclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/dc8000_aclk/clk_prepare_enable

# misc base :"bt1120 dc8000"
noc_list="0x20510000 0x20510080"
for i in ${noc_list}; do
    echo "update bt1120 noc qos: $i"
    config_noc_qos_max ${i}
done

# 配置cpu bpu ddr 降频的结温温度
echo 105000 > /sys/class/thermal/thermal_zone0/trip_point_0_temp
echo 105000 > /sys/class/thermal/thermal_zone1/trip_point_1_temp

# 设置cpu运行在高性能模式
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor

./multi_pipe_crop_and_stitch -c "sensor=8" -c "sensor=8" -o hdmi -b -v -g