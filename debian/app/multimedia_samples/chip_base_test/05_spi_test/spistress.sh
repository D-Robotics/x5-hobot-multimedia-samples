#!/bin/sh

# Determine the directory where the script is located
script_dir=$(cd "$(dirname "$0")" && pwd)

# Default values
Device="/dev/spidev2.0"         # Default SPI device
StressCount="100"               # Default stress test count
spi_speed="12000000"            # Default SPI speed
output_dir=$(realpath "$script_dir/../log")  # Default log directory

# Function to display help information
show_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -d <device>      Set the SPI device to test (default: /dev/spidev0.0)."
    echo "  -c <count>       Set the stress test count (default: 100)."
    echo "  -s <speed>       Set the SPI speed in Hz (default: 12000000)."
    echo "  -o <directory>   Set the output directory for logs (default: '../log')."
    echo "  -h               Show this help message and exit."
    echo
}

# Parse command-line arguments
while getopts "d:c:s:o:h" opt; do
    case "$opt" in
        d) Device="$OPTARG" ;;
        c) StressCount="$OPTARG" ;;
        s) spi_speed="$OPTARG" ;;
        o) output_dir=$(realpath "$OPTARG") ;;
        h) show_help; exit 0 ;;
        *)
            echo "Unknown option: -$OPTARG"
            show_help
            exit 1
            ;;
    esac
done

# Ensure the output directory exists
mkdir -p "$output_dir"

echo "SPI test starting..."
echo "Test configuration:"
echo "  Device: $Device"
echo "  Stress Count: $StressCount"
echo "  SPI Speed: $spi_speed Hz"
echo "  Output Directory: $output_dir"

# Generate a unique log file
num=1
while true; do
    spi_test_log_file="$output_dir/spi_test_log$num.txt"
    if [ -e "$spi_test_log_file" ]; then
        num=$((num + 1))
    else
        break
    fi
done

echo "  Log file: $spi_test_log_file"
# Run the SPI test
"${script_dir}/spidev_tc" -D "$Device" -s "$spi_speed" -I "$StressCount" -e 3 -S 32 > "$spi_test_log_file" 2>&1
exit_code=$?

if [ "$exit_code" -ne 0 ]; then
    echo "SPI test failed! Check log: $spi_test_log_file"
    exit 1
else
    echo "SPI test completed successfully! Log saved to: $spi_test_log_file"
fi
