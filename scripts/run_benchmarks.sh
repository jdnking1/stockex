#!/bin/bash
#
# This script automates the process of building, running, and visualizing
# the full benchmark suite for both OrderQueue implementations.

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJECT_ROOT="$SCRIPT_DIR/.."

cd "$PROJECT_ROOT"

echo "--- Starting StockeX Benchmark & Visualization Suite ---"

RESULTS_DIR="benchmark_results"
mkdir -p $RESULTS_DIR
echo "Benchmark results will be saved in the '$RESULTS_DIR' directory."

build_project() {
    local impl_name=$1
    local build_flag=$2 

    echo
    echo "============================================================"
    echo "CONFIGURING PROJECT FOR: $impl_name"
    echo "============================================================"

    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_BITMAP=$build_flag &> /dev/null

    echo
    echo "BUILDING: $impl_name (Compiler output hidden)"
    echo "------------------------------------------------------------"
    cmake --build build -j &> /dev/null
    echo "Build complete."
}

run_benchmarks() {
    local impl_name=$1
    local bin_dir="./build/bench"

    if [ ! -f "$bin_dir/marketSimulation" ]; then
        echo "Error: Benchmark executables not found in '$bin_dir'."
        exit 1
    fi

    echo
    echo "------------------------------------------------------------"
    echo "RUNNING BENCHMARKS FOR: $impl_name"
    echo "------------------------------------------------------------"

    $bin_dir/marketSimulation "${impl_name}" add_heavy 500.0
    $bin_dir/marketSimulation "${impl_name}" cancel_heavy 500.0
    $bin_dir/marketSimulation "${impl_name}" match_heavy 500.0
    $bin_dir/fragmentation_benchmark "${impl_name}" 10000 500 1
    $bin_dir/sweep_benchmark "${impl_name}"

    mv latencies_*.txt $RESULTS_DIR/
    echo "Latency data for '$impl_name' saved to '$RESULTS_DIR'."
}

build_project "bitmap" "ON"
run_benchmarks "bitmap"

build_project "linear_scan" "OFF"
run_benchmarks "linear_scan"

echo
echo "============================================================"
echo "📊 GENERATING VISUALIZATION PLOTS"
echo "============================================================"

VISUALIZE_SCRIPT="scripts/visualize_results.py"

if [ -f "$VISUALIZE_SCRIPT" ]; then
    if command -v python3 &> /dev/null; then
        python3 "$VISUALIZE_SCRIPT"
    elif command -v python &> /dev/null; then
        python "$VISUALIZE_SCRIPT"
    else
        echo "Warning: Python not found. Skipping visualization."
    fi
else
    echo "Warning: '$VISUALIZE_SCRIPT' not found. Skipping visualization."
fi

echo
echo "============================================================"
echo "✅ Suite finished successfully!"
echo "All results are in '$RESULTS_DIR' and plots are in 'benchmark_plots'."
echo "============================================================"
