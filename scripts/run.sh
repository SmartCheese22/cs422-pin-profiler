#!/bin/bash

# ==============================================================================
# PIN Tool Runner Script
# Usage: ./scripts/run.sh <binary_path> <output_name> <fast_forward_billions> [args...]
# Example: ./scripts/run.sh tool/HW1/dummy32 dummy_test 0
# ==============================================================================

# 1. Project Paths (Edit these if you move folders)
PROJECT_ROOT=/home/prathamesh/CS422/pin-profiler-hw1
PIN_EXE="$PROJECT_ROOT/pin/pin"
TOOL_SO="$PROJECT_ROOT/tool/HW1/obj-ia32/HW1.so"
RESULTS_DIR="$PROJECT_ROOT/results"

# 2. Input Arguments
BINARY_PATH=$1
OUTPUT_NAME=$2
FF_BILLIONS=$3
shift 3 # Shift arguments so the rest are passed to the binary

# 3. Validation
if [ -z "$BINARY_PATH" ] || [ -z "$OUTPUT_NAME" ]; then
  echo "Usage: ./scripts/run.sh <binary_path> <output_name> <fast_forward_billions> [args...]"
  exit 1
fi

mkdir -p $RESULTS_DIR

# 4. Execution
echo "------------------------------------------------------------------"
echo "Running PIN Tool..."
echo "Target: $BINARY_PATH"
echo "Output: $RESULTS_DIR/$OUTPUT_NAME.out"
echo "Fast Fwd: $FF_BILLIONS Billion Instructions"
echo "------------------------------------------------------------------"

$PIN_EXE -t $TOOL_SO -o "$RESULTS_DIR/$OUTPUT_NAME-$(date +%H%M).out" -f $FF_BILLIONS -- $BINARY_PATH "$@"

echo "Done! Results saved to $RESULTS_DIR/$OUTPUT_NAME-$(date +%H%M).out"
