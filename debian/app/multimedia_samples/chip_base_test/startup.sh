#!/bin/sh

# Determine the directory where the script is located
script_dir=$(dirname "$(readlink -f "$0")")

# Configuration file path
CONFIG_FILE="${script_dir}/config/config.ini"

# Set environment variables
. /etc/profile.d/environment.sh

# Ensure the log directory exists
LOG_DIR="$script_dir/log"
mkdir -p "$LOG_DIR"

# Configure CPU boost based on the config file
configure_cpu_boost() {
    CPU_BOOST_STATUS=$(awk -F '=' '
        /^\[CpuBoost\]/ {section=1}
        section && $1 ~ /Status/ {print $2; exit}
    ' "$CONFIG_FILE")

    if [ "$CPU_BOOST_STATUS" = "enabled" ]; then
        echo "Enabling CPU boost mode..."
        echo 1 >/sys/devices/system/cpu/cpufreq/boost
    else
        echo "CPU boost mode is disabled in config.ini."
        echo 0 >/sys/devices/system/cpu/cpufreq/boost
    fi
}

# Set CPU performance mode
echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor

# Display usage instructions
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  -h, --help              Show this help message and exit"
    echo "  -t, --test TEST_NAME    Specify the test to execute (can be used multiple times)"
    echo
    echo "Available tests:"
    awk -F '=' '
        /^\[.*\]/ {section=substr($0, 2, length($0)-2)}
        $1 ~ /Description/ {desc=$2}
        $1 ~ /ExecStart/ && section && desc {
            printf "  %-20s %s\n", section, desc
        }
    ' "$CONFIG_FILE"
}

# Parse the config.ini file and load all enabled tests
load_tests() {
    # Extract enabled tests from the config file
    TESTS=$(awk -F '=' '
        /^\[.*\]/ {section=substr($0, 2, length($0)-2)}
        $1 ~ /Status/ && $2 ~ /enabled/ {enabled[section]=1}
        $1 ~ /ExecStart/ && section && enabled[section] {
            print section ":" $2
        }
    ' "$CONFIG_FILE")
}

# Execute the specified test
run_test() {
    test_name=$1
    test_command=""

    # Find the corresponding command for the test
    test_command=$(echo "$TESTS" | grep "^${test_name}:" | cut -d ':' -f 2-)

    if [ -n "$test_command" ]; then
        echo "Executing $test_name..."
        eval "$test_command"
        echo "Running $test_name...finish" >> "$LOG_DIR/status"
    else
        echo "Test '$test_name' not found or not enabled in config.ini"
        exit 1
    fi
}

# Main function
main() {
    # Default test names (empty for now)
    TEST_NAMES=""

    # Configure CPU boost based on the configuration file
    configure_cpu_boost

    # Load all enabled tests from the configuration file
    load_tests

    # Parse command-line arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            -h|--help)
                show_help
                exit 0
                ;;
            -t|--test)
                # Append the specified test to the TEST_NAMES string
                if [ -n "$TEST_NAMES" ]; then
                    TEST_NAMES="$TEST_NAMES $2"
                else
                    TEST_NAMES="$2"
                fi
                shift 2
                ;;
            *)
                echo "Invalid option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # If no specific tests are specified, run all enabled tests in order
    if [ -z "$TEST_NAMES" ]; then
        # Extract all enabled test names from the TESTS variable
        TEST_NAMES=$(echo "$TESTS" | cut -d ':' -f 1)
    fi

    # Execute all the selected tests
    for test_name in $TEST_NAMES; do
        run_test "$test_name"
    done

    echo "All tests completed successfully." >> "$LOG_DIR/status"
}

# Run the main function
main "$@"
