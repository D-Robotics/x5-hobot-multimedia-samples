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

function config_noc_qos_mid()
{
    noc_base=$1
    noc_reg=$(( $noc_base + 12 ))
    echo "update noc qos to fixed(0)"
    devmem ${noc_reg} 32 0x0

    noc_reg=$(( $noc_base + 8 ))
    echo "update noc p1/p0 as mid(4)"
    devmem ${noc_reg} 32 0x00000404
}

function config_noc_qos_min()
{
    noc_base=$1
    noc_reg=$(( $noc_base + 12 ))
    echo "update noc qos to fixed(0)"
    devmem ${noc_reg} 32 0x0

    noc_reg=$(( $noc_base + 8 ))
    echo "update noc p1/p0 as min(0)"
    devmem ${noc_reg} 32 0x00000000
}

echo 1 > /sys/kernel/debug/clk/bt1120_aclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/dc8000_aclk/clk_prepare_enable

# misc base :"bt1120 dc8000"
noc_list="0x20510000 0x20510080"
for i in ${noc_list}; do
    echo "update bt1120 noc qos: $i"
    config_noc_qos_max ${i}
done

# power on isp
echo 1 > /sys/kernel/debug/drobot_pd/isp

# enable isp noc clk
echo 1 > /sys/kernel/debug/clk/isp_clk/clk_prepare_enable

# dw230
echo 1 > /sys/kernel/debug/clk/vse_axi_clk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/dewarp_axi_clk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/dewarp_core_clk/clk_prepare_enable

# isp:      "    axi5  |    axi4  |    axi3  |    axi1 "
noc_isp_noc="0x20510280 0x20510300 0x20510380 0x20510480"

for i in ${noc_isp_noc}; do
    echo "update isp noc qos: $i"
    config_noc_qos_mid ${i}
done

# dw230 base :"dw230 axi0|dw230 axi1|dw230 axi2|"
noc_dw230_noc="0x20510100 0x20510180 0x20510200"
for i in ${noc_dw230_noc}; do
    echo "update dw230 noc qos: $i"
    config_noc_qos_mid ${i}
done

# echo 1 > /sys/kernel/debug/clk/bt1120_aclk/clk_prepare_enable
# echo 1 > /sys/kernel/debug/clk/dc8000_aclk/clk_prepare_enable

# sif
echo 1 > /sys/kernel/debug/clk/sif_axi_clk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/disp_sif_aclk/clk_prepare_enable

# sif base :"    sif0      sif1       sif2       sif3       sif_disp"
noc_sif_noc="0x20510500 0x20510580 0x20510600 0x20510680 0x20510700"
for i in ${noc_sif_noc}; do
    echo "update misc noc qos: $i"
    config_noc_qos_mid ${i}
done

echo "set sif 7, use sys node."
echo 7 > /sys/bus/platform/drivers/noc_qos/20510500.sif0_qos/write_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510500.sif0_qos/read_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510580.sif1_qos/write_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510580.sif1_qos/read_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510600.sif2_qos/write_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510600.sif2_qos/read_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510680.sif3_qos/write_priority_qos_ctrl/priority
echo 7 > /sys/bus/platform/drivers/noc_qos/20510680.sif3_qos/read_priority_qos_ctrl/priority


# force override isp qos as 0
# dw230_axi0
# reg_write32( 0x3d0b0060, 0x3);
devmem 0x3d0b0060 32 0x3

# dw230_axi1
# reg_write32( 0x3d0b0064, 0x2);
devmem 0x3d0b0064 32 0x2

# dw230_axi2
# reg_write32( 0x3d0b0068, 0x1);
devmem 0x3d0b0068 32 0x1

# sif0
# reg_write32( 0x3d0b006c, 0x2);
devmem 0x3d0b006c 32 0x2

# sif1
# reg_write32( 0x3d0b0070, 0x2);
devmem 0x3d0b0070 32 0x2

# sif2
# reg_write32( 0x3d0b0074, 0x2);
devmem 0x3d0b0074 32 0x2

# sif3
# reg_write32( 0x3d0b0078, 0x2);
devmem 0x3d0b0078 32 0x2

# isp_axi1
# reg_write32( 0x3d0b007c, 0x2);
devmem 0x3d0b007c 32 0x2

# isp_axi3
# reg_write32( 0x3d0b0080, 0x3);
devmem 0x3d0b0080 32 0x3

# isp_axi4
# reg_write32( 0x3d0b0084, 0x3);
devmem 0x3d0b0084 32 0x3

# isp_axi5
# reg_write32( 0x3d0b0088, 0x3);
devmem 0x3d0b0088 32 0x3

# vpu
echo 1 > /sys/kernel/debug/drobot_pd/video
echo 1 > /sys/kernel/debug/clk/video_noc_pclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/codec_cclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/codec_noc_clk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/codec_bclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/jpeg_aclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/jpeg_cclk/clk_prepare_enable

# misc base :"video jpeg"
noc_list="0x20530000 0x20530080"
for i in ${noc_list}; do
   echo "update video noc qos: $i"
   config_noc_qos_min ${i}
done

# bpu
# power on bpu
echo 1 > /sys/kernel/debug/drobot_pd/bpu
echo 1 > /sys/kernel/debug/clk/bpu_noc_clk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/bpu_noc_pclk/clk_prepare_enable
echo 1 > /sys/kernel/debug/clk/bpu_mclk_2x/clk_prepare_enable
sleep 1

# gpu
noc_list="0x20520000"
for i in ${noc_list}; do
    echo "update gpu noc qos: $i"
    config_noc_qos_min ${i}
done

# #bpu
# echo ""
# echo ""
# echo ""
# echo "set bpu limit."
# devmem 0x2052000c
# devmem 0x2052000c 32 1
# devmem 0x20520010 32 0xd6
# devmem 0x2052000c

echo ""
echo ""
echo ""

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
#./multi_pipe_crop_and_stitch -c "sensor=8" -c "sensor=8" -o hdmi -b -v -p -g
