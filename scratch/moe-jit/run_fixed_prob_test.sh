#!/bin/bash
LOG_DIR="scratch/net-jit/logs"
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "Running FatTree Fixed Probability Tests"
echo "Logs will be saved to: $LOG_DIR"
echo "=========================================="

echo "| Probability | Avg Task Duration (ms) | Avg FCT (ms) | Avg Pkt Delay (ms) | Workload Makespan (s) |" > "$LOG_DIR/fixed_prob_summary.csv"

for prob in 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
    echo "[Prob = $prob] Running..."
    ./build/fattree-moe-fixed-prob-test --scheduler=fixed --prob=$prob > "$LOG_DIR/fattree_fixed_$prob.log" 2>&1
    
    # Analyze immediately
    python3 scratch/net-jit/analyze_single.py "$LOG_DIR/fattree_fixed_$prob.log" $prob >> "$LOG_DIR/fixed_prob_summary.csv"
done

echo "=========================================="
echo "Tests Complete. Summary:"
cat "$LOG_DIR/fixed_prob_summary.csv"
