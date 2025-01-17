#!/bin/sh

SCRIPT_DIR=$(dirname "$(realpath "$0")")

export BPLAT_BUSY_THRES=1
portion=100
bpu_core=0

usage="Usage: $0 [-b <bpu_core>] [-p <portion>]

Options:
  -b <bpu_core>   Specify the BPU core to use. On the X5 platform, only '0' is supported.
                  Example: use '-b 0' to select BPU core 0.
  -p <portion>    Set the portion parameter. This should be a valid numeric value representing the portion.

Examples:
  $0 -b 0 -p 100   Select BPU core 0 and set portion to 100.

Notes:
  - The '-b' option must be set to '0' on the X5 platform; other values are not supported.
  - Ensure the portion is a valid numeric value."

error_bpu_usage="Error: On the X5 platform, only '-b 0' is supported. Please use:
Example: $0 -b 0"

while getopts ":b:p:h" opt; do
    case $opt in
        b)
            if [ "$OPTARG" != "0" ]; then
                echo "$error_bpu_usage"
                exit 1
            fi
            bpu_core="$OPTARG"
            ;;
        p)
            portion="$OPTARG"
            ;;
        h)
            echo "$usage"
            exit 0
            ;;
        \?)
            echo "Error: Invalid option: -$OPTARG"
            echo "$usage"
            exit 1
            ;;
        :)
            echo "Error: Option -$OPTARG requires an argument."
            echo "$usage"
            exit 1
            ;;
    esac
done
echo "BPU Core: ${bpu_core:-'Not specified'}"
echo "Portion: ${portion:-'Not specified'}"

if [ "$bpu_core" -eq 0 ]; then
    "$SCRIPT_DIR"/../models/HBDK3_MODEL_2K/run.sh "$portion" "$bpu_core"
else
    echo "Error: Invalid BPU core selected. On the X5 platform, only BPU core '0' is supported."
    echo "Usage: Use '-b 0' to select BPU core 0."
    exit 1
fi

