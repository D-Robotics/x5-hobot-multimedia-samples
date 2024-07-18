#!/bin/sh

output_dir="../output"
echo "eMMC performance test starting..."
mkdir -p "$output_dir"
total_duration=$((48 * 60 * 60))  # 48 hours in seconds
loop_duration=30  # 30 seconds

start_time=$(date +%s)
loop_num=0

while [ $(($(date +%s) - start_time)) -lt $total_duration ]
do
	loop_num=$((loop_num + 1))
	echo "loop_test: ${loop_num}"

	iozone -e -I -a -r 4K -r 16K -r 64K -r 256K -r 1M -r 4M -r 16M -s 16K -s 1M -s 16M -s 128M -s 1G -f "$output_dir/iozone_data" -Rb "$output_dir/test_iozone_emmc_performance_${loop_num}.xls"
	exit_code=$?

	if [ "$exit_code" != 0 ]; then
		echo "Test fail loop ${loop_num} error!" >> "$output_dir/test_iozone_emmc_performance.log"
		exit 1
	else
		echo "Test loop ${loop_num} success!" >> "$output_dir/test_iozone_emmc_performance.log"
	fi

	sleep $loop_duration  # Sleep for 30 seconds between tests
done

echo "eMMC performance test completed!"
