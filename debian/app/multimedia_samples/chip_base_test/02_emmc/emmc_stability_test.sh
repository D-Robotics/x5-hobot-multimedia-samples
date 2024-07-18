#!/bin/sh

if [ -f "../config/ini_parser_functions.sh" ] && [ -f "../config/config.ini" ]; then
	source ../config/ini_parser_functions.sh
	# 定义INI文件路径
	ini_file="../config/config.ini"

	# 定义要解析的section和键值
	target_section="EmmcFlash"
	target_key="StressTime"

	# 执行解析并将输出保存到变量
	parsed_value=$(parse_ini_section "$target_section" "$target_key" "$ini_file")

	# 从解析结果中提取数值部分并保存到变量
	looptime=$(echo "$parsed_value" | awk '{print $NF}')

	# 打印配置
	echo "Configure [EmmcFlash] StressTime: $looptime"
fi


#如果从INI读取到的数值为空则退出脚本
if [ -z "$looptime" ]; then
	looptime="48"
	echo "Use default looptime $looptime"
fi

#从INI文件读取到的数值换算成循环次数
total_duration=$(($looptime * 60 * 60))  # 48 hours in seconds

output_dir="../log"
echo "eMMC stability test starting..."
mkdir -p "$output_dir"

loop_duration=30  # 30 seconds

start_time=$(date +%s)
loop_num=0

while [ $(($(date +%s) - start_time)) -lt $total_duration ]
do
	loop_num=$((loop_num + 1))
	echo "loop_test: ${loop_num}"

	iozone -e -I -az -n 16m -g 2g -q 16m -f "$output_dir/iozone_data" -Rb "$output_dir/test_iozone_emmc_stability_${loop_num}.xls"
	exit_code=$?

	if [ "$exit_code" != 0 ]; then
		echo "Test fail loop ${loop_num} error!" >> "$output_dir/test_iozone_emmc_stability.log"
		exit 1
	else
		echo "Test loop ${loop_num} success!" >> "$output_dir/test_iozone_emmc_stability.log"
	fi

	sleep $loop_duration  # Sleep for 30 seconds between tests
done

echo "eMMC stability test completed!"
