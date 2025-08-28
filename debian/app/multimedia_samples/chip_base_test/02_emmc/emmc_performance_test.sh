#!/bin/sh

# Determine the directory where the script is located
script_dir=$(cd "$(dirname "$0")" && pwd)

# Default values
looptime="48h"  # Default looptime (48 hours)
loop_duration=30  # Default duration between loops (30 seconds)
output_dir=$(realpath "$script_dir/../output")  # Default output directory

# Function to display help information
show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -t <time>       Set the test duration (e.g., 2h for hours, 30m for minutes; default: 48h)."
    echo "  -d <seconds>    Set the sleep time between loops in seconds (default: 30)."
    echo "  -o <directory>  Set the output directory for logs (default: script's '../output' folder)."
    echo "  -h              Show this help message and exit."
    echo
}

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

# Parse command-line arguments
while getopts "t:d:o:h" opt; do
    case "$opt" in
        t)
            looptime="$OPTARG"
            ;;
        d)
            loop_duration="$OPTARG"
            ;;
        o)
            output_dir=$(realpath "$OPTARG")  # Resolve the absolute path
            ;;
        h)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: -$OPTARG"
            show_help
            exit 1
            ;;
    esac
done

# Parse and validate looptime
looptime_in_minutes=$(parse_time "$looptime")
if [ -z "$looptime_in_minutes" ] || ! [ "$looptime_in_minutes" -eq "$looptime_in_minutes" ] 2>/dev/null; then
    echo "Invalid looptime. Using default: 48h (2880 minutes)."
    looptime_in_minutes=2880  # Default to 48 hours
fi

# Calculate total duration in seconds
total_duration=$((looptime_in_minutes * 60))  # Convert minutes to seconds

# Ensure the output directory exists
mkdir -p "$output_dir"

echo "eMMC performance test starting..."
echo "Test configuration:"
echo "  Test duration: $looptime_in_minutes minutes"
echo "  Sleep duration: $loop_duration seconds"
echo "  Output directory: $output_dir"

start_time=$(date +%s)
loop_num=0

# Test loop
while [ $(($(date +%s) - start_time)) -lt $total_duration ]; do
    loop_num=$((loop_num + 1))
    echo "loop_test: ${loop_num}"

    iozone -e -I -a -r 4K -r 16K -r 64K -r 256K -r 1M -r 4M -r 16M -s 16K -s 1M -s 16M -s 128M -s 256M -f "$output_dir/iozone_data" -Rb "$output_dir/test_iozone_emmc_performance_${loop_num}.xls"
    exit_code=$?

    if [ "$exit_code" -ne 0 ]; then
        echo "Test failed in loop ${loop_num} with error code $exit_code!" >> "$output_dir/test_iozone_emmc_performance.log"
        exit 1
    else
        echo "Test loop ${loop_num} succeeded!" >> "$output_dir/test_iozone_emmc_performance.log"
    fi

    sleep "$loop_duration"  # Sleep for the specified duration
done

echo "eMMC performance test completed!"
