#!/bin/sh
#set cpufreq 1.8GHZ
export LD_LIBRARY_PATH=/usr/hobot/lib:/app/lib:/app/pub/lib:/middleware/lib:/middleware/pub/lib:/usr/hobot/lib/sensor:/system/lib:/system/usr/lib:/lib:$LD_LIBRARY_PATH
echo 1 >/sys/devices/system/cpu/cpufreq/boost
echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor


cd /userdata/chip_base_test/02_emmc
/bin/bash ./emmc_stability_test.sh
echo "Running emmc_stability_test.sh...finish" > /userdata/chip_base_test/log/status

cd /userdata/chip_base_test/03_nand_flash
/bin/bash ./nand_test.sh
echo "Running nand_test.sh...finish" > /userdata/chip_base_test/log/status

cd /userdata/chip_base_test/01_cpu_bpu_ddr/scripts
./stress_test.sh
echo "Running stress_test.sh...finish" >> /userdata/chip_base_test/log/status

cd /userdata/chip_base_test/04_uart_test
./uartstress.sh
echo "Running uartstress.sh...finish" >> /userdata/chip_base_test/log/status

cd /userdata/chip_base_test/05_spi_test/
./spistress.sh
echo "Running spistress.sh...finish" >> /userdata/chip_base_test/log/status

echo "All scripts executed successfully." >> /userdata/chip_base_test/log/status
