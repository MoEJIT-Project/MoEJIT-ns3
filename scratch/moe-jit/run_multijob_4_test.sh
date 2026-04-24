#!/bin/bash
LOG_DIR="scratch/net-jit/logs"
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "Running FatTree 4-Job Fixed Probability Tests"
echo "Logs will be saved to: $LOG_DIR"
echo "=========================================="

echo "| JobMode | Backoff | Probability | Avg Task Duration (ms) | Avg FCT (ms) | Avg Pkt Delay (ms) | Workload Makespan (s) |" > "$LOG_DIR/multijob_4_summary.csv"

# Test Backoff: 1ms Only (assuming previous results held true that 1ms is effective)
backoff="1ms"
jobs="4"

for prob in 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
    echo "[Jobs=$jobs, Backoff=$backoff, Prob=$prob] Running..."
    
    LOG_FILE="$LOG_DIR/fattree_multijob_${jobs}job_${backoff}_${prob}.log"
    
    ./build/fattree-moe-multijob-test --scheduler=fixed --prob=$prob --backoff=$backoff --jobs=$jobs > "$LOG_FILE" 2>&1
    
    # Python analyze
    RESULT=$(python3 scratch/net-jit/analyze_single.py "$LOG_FILE" "$prob")
    
    # Add JobMode and Backoff to result
    CLEAN_RESULT=$(echo "$RESULT" | sed 's/^| [^|]* |/| '"$jobs"' | '"$backoff"' | '"$prob"' |/')
    echo "$CLEAN_RESULT" >> "$LOG_DIR/multijob_4_summary.csv"
done

echo "=========================================="
echo "Tests Complete. Summary:"
cat "$LOG_DIR/multijob_4_summary.csv"
