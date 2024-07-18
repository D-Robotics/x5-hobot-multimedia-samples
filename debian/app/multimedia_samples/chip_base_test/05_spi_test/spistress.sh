#!/bin/sh

if [ -f "../config/ini_parser_functions.sh" ] && [ -f "../config/config.ini" ]; then
	source ../config/ini_parser_functions.sh
	# 定义INI文件路径
	ini_file="../config/config.ini"

	# 定义要解析的section和键值
	target_section="SPI"
	target_key_device="Device"
	target_key_count="StressCount"

	# 执行解析并将输出保存到变量
	parsed_device=$(parse_ini_section "$target_section" "$target_key_device" "$ini_file")
	parsed_count=$(parse_ini_section "$target_section" "$target_key_count" "$ini_file")

	# 从解析结果中提取数值部分并保存到变量
	Device=$(echo "$parsed_device" | awk '{print $NF}')
	StressCount=$(echo "$parsed_count" | awk '{print $NF}')

	# 打印配置
	echo "Configure Device: $Device"
	echo "Configure StressCount: $StressCount"
fi

#如果从INI读取到的数值为空则使用缺省参数
if [ -z "$Device" ]; then
	Device="/dev/spidev0.0"
	echo "Use default Device $Device"
fi

if [ -z "$StressCount" ]; then
	StressCount="100"
	echo "Use default count $StressCount"
fi

echo "SPI test starting..."
mkdir -p ../log
num=1

while true; do
	spiloopfile="../log/spiloop$num.txt"

	# 检查日志文件是否已存在
	if [ -e "$spiloopfile" ]; then
		((num++))
	else
		#./spidev_tc -D /dev/spidev0.0 -s 12000000 -I 10 -e 3 -S 32
		./spidev_tc -D $Device -s 12000000 -I $StressCount -e 3 -S 32 > ../log/spiloop$num.txt
		break
	fi
done

echo "SPI test completed!"
