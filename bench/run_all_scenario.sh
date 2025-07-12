#!/bin/bash

EXECUTABLE="./orderMatchingBench"

TEST_SCENARIOS=(
  "flat"
  "nonlinear"
  "fanout"
  "skewed"
  "layered"
  "randomwalk"
)

if [ ! -x "$EXECUTABLE" ]; then
  echo "Error: Executable not found or not executable at '$EXECUTABLE'"
  echo "Please compile your C++ code and update the EXECUTABLE variable in this script."
  exit 1
fi

TOTAL_SCENARIOS=${#TEST_SCENARIOS[@]}
CURRENT_SCENARIO=1

for scenario in "${TEST_SCENARIOS[@]}"; do
  echo "========================================================================"
  echo "Running benchmark for scenario: $scenario ($CURRENT_SCENARIO/$TOTAL_SCENARIOS)"
  echo "========================================================================"
  
  "$EXECUTABLE" "$scenario"
  
  echo ""
  
  ((CURRENT_SCENARIO++))
done

echo "========================================================================"
echo "All benchmark scenarios have completed."
echo "========================================================================"
