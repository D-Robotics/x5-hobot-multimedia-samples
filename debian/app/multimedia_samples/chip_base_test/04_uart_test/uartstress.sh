#!/bin/sh

# Determine the directory where the script is located
script_dir=$(cd "$(dirname "$0")" && pwd)

# Default values
Baudrate="115200"
Device="/dev/ttyS2"
StressCount="100"
output_dir=$(realpath "$script_dir/../log")

# Function to display help information
show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -b <baudrate>    Set the UART baud rate (default: 115200)."
    echo "  -d <device>      Set the UART device (default: /dev/ttyS1)."
    echo "  -c <count>       Set the stress count (default: 100)."
    echo "  -o <directory>   Set the output directory for logs (default: ../log)."
    echo "  -h               Show this help message and exit."
    echo
}

# Parse command-line arguments
while getopts "b:d:c:o:h" opt; do
    case "$opt" in
        b)
            Baudrate="$OPTARG"
            ;;
        d)
            Device="$OPTARG"
            ;;
        c)
            StressCount="$OPTARG"
            ;;
        o)
            output_dir="$OPTARG"
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

# Ensure the output directory exists
mkdir -p "$output_dir"

echo "Uart test starting..."
echo "Test configuration:"
echo "  Baudrate: $Baudrate"
echo "  Device: $Device"
echo "  Stress count: $StressCount"
echo "  Output directory: $output_dir"

# Generate a unique log file
num=1
while true; do
    uart_test_log_file="$output_dir/uart_test_log$num.txt"
    if [ -e "$uart_test_log_file" ]; then
        num=$((num + 1))
    else
        break
    fi
done

echo "  Log file: $uart_test_log_file"
# Run the UART test command with the provided options
stdbuf -oL "${script_dir}/uart_test" -l -s 1024 -c "$StressCount" -b "$Baudrate" -d "$Device" > "$uart_test_log_file"
exit_code=$?

if [ "$exit_code" -ne 0 ]; then
    echo "UART test failed! Check log: $uart_test_log_file"
    exit 1
else
    echo "UART test completed successfully! Log saved to: $uart_test_log_file"
fi
