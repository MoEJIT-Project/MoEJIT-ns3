#!/bin/bash
LOG_DIR="scratch/net-jit/logs"
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "Running FatTree 4-Job Detailed Flow Test"
echo "JobMode=4, Backoff=1ms, Prob=0.1"
echo "Logs will be saved to: $LOG_DIR"
echo "=========================================="

# Run single test
./build/fattree-moe-multijob-test --scheduler=fixed --prob=0.1 --backoff=1ms --jobs=4 > "$LOG_DIR/multijob_detailed.log" 2>&1

echo "Analyzing Detailed Flow Stats..."
# Python script to parse [FlowTx] and [FlowRx]
python3 scratch/net-jit/analyze_detailed_flows.py "$LOG_DIR/multijob_detailed.log"

echo "Done."
