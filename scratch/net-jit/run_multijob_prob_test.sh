#!/bin/bash
LOG_DIR="scratch/net-jit/logs"
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "Running FatTree MultiJob Fixed Probability Tests"
echo "Logs will be saved to: $LOG_DIR"
echo "=========================================="

echo "| Backoff | Probability | Avg Task Duration (ms) | Avg FCT (ms) | Avg Pkt Delay (ms) | Workload Makespan (s) |" > "$LOG_DIR/multijob_prob_summary.csv"

# Test different Backoff Times
for backoff in "10us" "100us" "1ms"
do
    echo "--- Testing Backoff: $backoff ---"
    # Test Probabilities 0.1 to 1.0
    for prob in 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
    do
        echo "[Backoff=$backoff, Prob=$prob] Running..."
        
        # Log filename safe
        LOG_FILE="$LOG_DIR/fattree_multijob_${backoff}_${prob}.log"
        
        ./build/fattree-moe-multijob-test --scheduler=fixed --prob=$prob --backoff=$backoff > "$LOG_FILE" 2>&1
        
        # Analyze
        # We need a slightly modified analyze script or we can just parse the single line output
        # Let's reuse python3 scratch/net-jit/analyze_single.py but prepend the Backoff column
        
        # Run python analysis
        # Assuming analyze_single.py prints "| Label | Dur | FCT | Delay | Makespan |"
        # We want: "| $backoff | $prob | ... "
        
        RESULT=$(python3 scratch/net-jit/analyze_single.py "$LOG_FILE" "$prob")
        # Extract the part after the first pipe
        # Example output: "| 0.1 | 100.00 | 50.00 | 2.00 | 5.0000 |"
        # We want to replace the first col " 0.1 " with "  |  "
        
        CLEAN_RESULT=$(echo "$RESULT" | sed 's/^| [^|]* |/| '"$backoff"' | '"$prob"' |/')
        echo "$CLEAN_RESULT" >> "$LOG_DIR/multijob_prob_summary.csv"
    done
done

echo "=========================================="
echo "Tests Complete. Summary:"
cat "$LOG_DIR/multijob_prob_summary.csv"
