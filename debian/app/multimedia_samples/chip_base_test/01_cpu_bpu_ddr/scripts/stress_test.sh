#!/bin/sh

if [ -f "../../config/ini_parser_functions.sh" ] && [ -f "../../config/config.ini" ]; then
	source ../../config/ini_parser_functions.sh
	# 定义INI文件路径
	ini_file="../../config/config.ini"

	# 定义要解析的section和键值
	target_section="CpuAndBpu"
	target_key="StressTime"

	# 执行解析并将输出保存到变量
	parsed_value=$(parse_ini_section "$target_section" "$target_key" "$ini_file")

	# 从解析结果中提取数值部分并保存到变量
	htime=$(echo "$parsed_value" | awk '{print $NF}')

	# 打印配置
	echo "Configure [CpuAndBpu] StressTime: $htime"
fi

#如果从INI读取到的数值为空则使用缺省参数
if [ -z "$htime" ]; then
	htime="48"
	echo "Use default looptime $htime"
fi

#从INI文件读取到的数值换算成秒
stime=$(echo "$htime * 60 * 60" | bc)
echo "please wait $stime s"
echo "CpuAndBpu test starting..."

mkdir -p ../../log
num=1

while true; do
	bpulogfile="../../log/bpu-stress$num.log"
	cpulogfile="../../log/cpu-stress$num.log"

	# 检查日志文件是否已存在
	if [ -e "$bpulogfile" ] && [ -e "$cpulogfile" ]; then
		((num++))
	else
		#1 cpu test
		echo 4 > /proc/sys/vm/drop_caches
		nohup ./stressapptest -s $stime -M 100 -f /tmp/sat.io1 -f /tmp/sat.io2 -i 4 -m 8 -C 2 -W >> ../../log/cpu-stress$num.log &
		#1 bpu test
		sleep 2
		./run-portion.sh -b 0 -p 100 >> ../../log/bpu-stress$num.log &
		hrut_somstatus
		break
	fi
done

while pgrep -x stressapptest >/dev/null; do
	sleep 1
done
pkill -f tc_hbdk3
pkill -f run-portion.sh
pkill -f run.sh
echo "CpuAndBpu test completed!"
