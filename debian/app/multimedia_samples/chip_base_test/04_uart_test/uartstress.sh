#!/bin/sh

if [ -f "../config/ini_parser_functions.sh" ] && [ -f "../config/config.ini" ]; then
	source ../config/ini_parser_functions.sh
	# 定义INI文件路径
	ini_file="../config/config.ini"

	# 定义要解析的section和键值
	target_section="Uart"
	target_key_baudrate="Baudrate"
	target_key_device="Device"
	target_key_count="StressCount"

	# 执行解析并将输出保存到变量
	parsed_baudrate=$(parse_ini_section "$target_section" "$target_key_baudrate" "$ini_file")
	parsed_device=$(parse_ini_section "$target_section" "$target_key_device" "$ini_file")
	parsed_count=$(parse_ini_section "$target_section" "$target_key_count" "$ini_file")

	# 从解析结果中提取数值部分并保存到变量
	Baudrate=$(echo "$parsed_baudrate" | awk '{print $NF}')
	Device=$(echo "$parsed_device" | awk '{print $NF}')
	StressCount=$(echo "$parsed_count" | awk '{print $NF}')

	# 打印配置
	echo "Configure [Uart] Baudrate: $Baudrate"
	echo "Configure [Uart] Device: $Device"
	echo "Configure [Uart] StressCount: $StressCount"
fi

#如果从INI读取到的数值为空则使用缺省参数
if [ -z "$Baudrate" ]; then
	Baudrate="115200"
	echo "Use default baudrate $Baudrate"
fi

if [ -z "$Device" ]; then
	Device="/dev/ttyS1"
	echo "Use default interface $Device"
fi

if [ -z "$StressCount" ]; then
	StressCount="100"
	echo "Use default count $StressCount"
fi
echo "Uart test starting..."
mkdir -p ../log

num=1
while true; do
	uartloopfile="../log/uartloop$num.txt"

	# 检查日志文件是否已存在
	if [ -e "$uartloopfile" ]; then
		((num++))
	else
		#./uart_test -l -s 1024 -c 100 -b 115200 -d /dev/ttyS1 > ../log/uartloop$num.txt
		./uart_test -l -s 1024 -c $StressCount -b $Baudrate -d $Device > ../log/uartloop$num.txt
		break
	fi
done

echo "Uart test completed!"
