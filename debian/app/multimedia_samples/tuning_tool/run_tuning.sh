#!/bash
CUR_TEST_SHELL=$(readlink -f $0)
COMMON_DIR=$(pwd)

FEEDBACK_IMG_H="1080"
FEEDBACK_IMG_W="1920"
RAW_FORMAT="raw10"

function print_usage() {
	echo "run_tuning.sh --list: list all case"
	echo "run_tuning.sh --run [sensor_index]: run this sensor"
	echo "run_tuning.sh --online/offline/mcm/: Open the data stream in online/offline/mcm/ mode"
	echo "run_tuning.sh --tune 0/1: close/open tuning_server"
	echo "run_tuning.sh --log 0/1: increase/decrease log level in logcat"
	echo "run with [-w 2]: dump 20 yuv from the start"
	echo "run with [-r 1]: send raw to hbplayer"
	echo "run with [-f xx]: feedback raw list xx times"
	exit 1
}

function get_case() {
	if [ -z "$1" ]; then
		echo "Please input the index of the sensor which you want to run."
		echo "eg: --run 1"
		exit 1
	fi
}

function suit_case_run() {
	local sensor_args=()   # 存储 sensor 参数
	local extra_args=()    # 存储额外参数（如 -w 2）
	local mode="--offline" # 默认模式为 offline
	local sensor_count=0   # 统计 sensor 数量
	local has_f_param=0    # 标记是否出现 -f 参数
	local dummy_index=$(./isp_tuning | grep "sensor_name: dummy" | awk '{print $2}' | tr -d :) # 获取dummy_index

	# dummy sensor
	local user_height=${FEEDBACK_IMG_H}
	local user_width=${FEEDBACK_IMG_W}
	local user_format=${RAW_FORMAT}
	local user_specified_params=0  # 是否用户指定了-H -W -F

	while [[ $# -gt 0 ]]; do
		case "$1" in
			"--online")
				if [[ "$sensor_count" -gt 1 ]]; then
					echo "Error: --online modes only support one sensor!"
					exit 1
				fi
				mode="$1"
				;;
			"--offline")
				mode="--offline"
				;;
			"--mcm")
				mode="--mcm"
				;;
			"-H")
				if [[ $# -gt 1 ]]; then
					user_height="$2"
					user_specified_params=1
					shift
				fi
				;;
			"-W")
				if [[ $# -gt 1 ]]; then
					user_width="$2"
					user_specified_params=1
					shift
				fi
				;;
			"-F")
				if [[ $# -gt 1 ]]; then
					user_format="$2"
					user_specified_params=1
					shift
				fi
				;;
			[0-9]*)  # 纯数字 (sensor ID)
				if [[ "$1" =~ ^[0-9]+$ ]]; then
					sensor_args+=("-s" "sensor=$1")
					((sensor_count++))
					if [[ "$mode" == "--offline" && "$sensor_count" -gt 4 ]]; then
						echo "Error: --offline mode supports up to 4 sensors!"
						exit 1
					fi
					if [[ "$mode" == "--mcm" && "$sensor_count" -gt 4 ]]; then
						echo "Error: --mcm mode supports up to 4 sensors!"
						exit 1
					fi
					# 如果是 dummy sensor 且未指定 -H -W -F，则使用默认值
					if [[ "$1" == "$dummy_index" && "$user_specified_params" -eq 0 ]]; then
						extra_args+=(-H $user_height -W $user_width -F $user_format -w 1)
					fi
				else
					extra_args+=("$1")
				fi
				;;
			"-f")  # 处理 `-f` 参数
				has_f_param=1  # 标记 -f 存在
				extra_args+=("$1")
				if [[ $# -gt 1 ]]; then
					extra_args+=("$2")
					shift
				fi
				;;
			-*)  # 处理 `-w 2` 这种额外参数
				if [[ $# -gt 1 && "$2" =~ ^[0-9]+$ ]]; then
					extra_args+=("$1" "$2")
					shift
				else
					extra_args+=("$1")
				fi
				;;
			*)
				extra_args+=("$1")  # 其他参数
				;;
		esac
		shift
	done

	if [[ ${#sensor_args[@]} -eq 0 ]]; then
		echo "Please input the index of the sensor which you want to run."
		echo "eg: --run 1"
		exit 1
	fi

	echo "Executing: ./isp_tuning $mode ${sensor_args[*]} ${extra_args[*]}"
	./isp_tuning "$mode" "${sensor_args[@]}" "${extra_args[@]}"
}

function vtuner_control() {
	if [ -z "$1" ]; then
		echo "Please input --tune 1 to open the vtuner_server."
		exit 1
	fi

	if [ "$1" == "1" ]; then
		echo 1 > /sys/kernel/debug/isp/tune
		echo "Open vtuner_server Success! Please connect VtunerClient"
	elif [ "$1" == "0" ]; then
		echo 0 > /sys/kernel/debug/isp/tune
		echo "Close vtuner_server done!"
	else
		echo "Please input --tune 0/1 to close/open the vtuner_server."
		exit 1
	fi
}

function log_control() {
	if [ -z "$1" ]; then
		echo "Please input --log 1 to print the camera_server log."
		exit 1
	fi

	if [ "$1" == "1" ]; then
		echo 31 31 31 > /sys/kernel/debug/isp/log
		echo "logcat show verbose log"
	elif [ "$1" == "0" ]; then
		echo 3 3 3 > /sys/kernel/debug/isp/log
		echo "logcat just show import log"
	else
		echo "Please input --log 0/1 to control loglevel."
		exit 1
	fi
}

function specify_command() {
	local _command=$1
	shift 1
	if [ "$_command" == "--list" ]; then
		./isp_tuning
	elif [ "$_command" == "--run" ]; then
		suit_case_run "$@"
	elif [ "$_command" == "--help" ]; then
		print_usage
	elif [ "$_command" == "--tune" ]; then
		vtuner_control "$@"
	elif [ "$_command" == "--log" ]; then
		log_control "$@"
	else
		echo "invaild cmd input : $_command "
		print_usage
	fi
}

specify_command "$@"
