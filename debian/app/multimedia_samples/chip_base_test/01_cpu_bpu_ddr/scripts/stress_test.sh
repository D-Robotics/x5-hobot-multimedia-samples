#!/bin/sh

# Determine the directory where the script is located
script_dir=$(cd "$(dirname "$0")" && pwd)

# Default values
stress_time_default="48h"  # Default stress test time in hours
memory_size_default=100    # Default memory size in MB
io_threads_default=4       # Default I/O threads
bpu_core_default=0         # Default BPU core
bpu_portion_default=100    # Default BPU test portion
output_dir=$(realpath "$script_dir/../../log")

# Function to display help information
show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -t <time>        Set the test duration (e.g., 2h for hours, 30m for minutes; default: 48h)."
    echo "  -m <size>        Set the memory size for stress test in MB (default: 100)."
    echo "  -i <threads>     Set the I/O threads for stress test (default: 4)."
    echo "  -b <bpu_core>    Specify the BPU core to use (default: 0)."
    echo "  -p <portion>     Set the BPU portion value (default: 100)."
    echo "  -o <directory>   Set the output directory for logs (default: ../../log)."
    echo "  -h, --help       Show this help message and exit."
    echo
    echo "Example:"
    echo "  $0 -t 24h -m 200 -i 8 -b 80"
    echo "  This runs a stress test for 24 hours with 200MB memory, 8 I/O threads, and 80% BPU load."
    exit 0
}

# Function to parse and validate time duration
# Function to parse duration in minutes or hours
parse_time() {
    input="$1"  # Assign input directly to a variable
    case "$input" in
        *h)
            echo $((${input%h} * 60))  # Convert hours to minutes
            ;;
        *m)
            echo "${input%m}"  # Return minutes directly
            ;;
        *)
            echo "$input"  # Assume it's already in minutes
            ;;
    esac
}

# Parse command-line options
while getopts ":t:m:i:b:p:o:h" opt; do
    case $opt in
        t) user_stress_time="$OPTARG" ;;
        m) user_memory_size="$OPTARG" ;;
        i) user_io_threads="$OPTARG" ;;
        b) user_bpu_core="$OPTARG" ;;
        p) user_bpu_portion="$OPTARG" ;;
        o) output_dir="$OPTARG" ;;
        h) show_help ;;
        \?) echo "Error: Invalid option -$OPTARG"; show_help ;;
        :) echo "Error: Option -$OPTARG requires an argument."; show_help ;;
    esac
done

# Set default values if not provided by user or INI
stress_time=${user_stress_time:-$stress_time_default}
memory_size=${user_memory_size:-$memory_size_default}
io_threads=${user_io_threads:-$io_threads_default}
bpu_core=${user_bpu_core:-$bpu_core_default}
bpu_portion=${user_bpu_portion:-$bpu_portion_default}

# Parse and validate looptime
looptime_in_minutes=$(parse_time "$stress_time")
if [ -z "$looptime_in_minutes" ] || ! [ "$looptime_in_minutes" -eq "$looptime_in_minutes" ] 2>/dev/null; then
    echo "Invalid looptime. Using default: 48h (2880 minutes)."
    looptime_in_minutes=2880  # Default to 48 hours
fi

# Calculate total duration in seconds
stime=$((looptime_in_minutes * 60))

echo "Running stress test with the following configuration:"
echo "  Stress Time: $stress_time hours ($stime seconds)"
echo "  Memory Size: $memory_size MB"
echo "  I/O Threads: $io_threads"
echo "  BPU Core: $bpu_core"
echo "  BPU Portion: $bpu_portion%"
echo "  Output directory: $output_dir"

# Ensure the output directory exists
mkdir -p "$output_dir"
num=1

# Find available log files
while true; do
    bpulogfile="$output_dir/bpu-stress$num.log"
    cpulogfile="$output_dir/cpu-stress$num.log"

    if [ -e "$bpulogfile" ] && [ -e "$cpulogfile" ]; then
        num=$((num + 1))
    else
        # Start CPU stress test
        echo "Starting CPU stress test..."
        echo 4 > /proc/sys/vm/drop_caches
        nohup "${script_dir}/stressapptest" -s "$stime" -M "$memory_size" -f /tmp/sat.io1 -f /tmp/sat.io2 \
            -i "$io_threads" -m 8 -C 2 -W >> "$cpulogfile" &

        # Start BPU stress test
        sleep 2
        echo "Starting BPU stress test..."
        "${script_dir}/run-portion.sh" -b "$bpu_core" -p "$bpu_portion" >> "$bpulogfile" &

        break
    fi
done

# Monitor stressapptest
while pgrep -x stressapptest >/dev/null; do
    hrut_somstatus
    sleep 1
done

# Cleanup after tests
pkill -f tc_hbdk3
pkill -f run-portion.sh
pkill -f run.sh

echo "CPU, BPU, and DDR stress tests completed!"
