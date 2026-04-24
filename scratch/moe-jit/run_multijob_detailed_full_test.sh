#!/bin/bash
LOG_DIR="scratch/net-jit/logs/detailed_full"
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "Running FatTree 4-Job Detailed Flow Test (Full Suite)"
echo "Logs will be saved to: $LOG_DIR"
echo "=========================================="


# Ensure NS-3 libraries can be found (Critical for Docker execution)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/lib

# Header with clear descriptions
echo "Metric Definitions:" > "$LOG_DIR/detailed_summary.txt"
echo "  Avg/Max Flow FCT: End-to-End Latency of single flows (network perspective, excludes scheduling wait)" >> "$LOG_DIR/detailed_summary.txt"
echo "  Avg/Max Rank Task Duration: Application-level Task Interval (includes some waiting if blocked on receive)" >> "$LOG_DIR/detailed_summary.txt"
echo "  Avg/Max Group Task Duration: Duration from first Rank starting to last Rank finishing within a Job Group (Wait + Exec + Sync)" >> "$LOG_DIR/detailed_summary.txt"
echo "  Avg Pkt Delay: Basic packet switching delay (from FlowMonitor)" >> "$LOG_DIR/detailed_summary.txt"
echo "  Makespan: Time when the last flow finished (global completion)" >> "$LOG_DIR/detailed_summary.txt"
echo "" >> "$LOG_DIR/detailed_summary.txt"

# CSV Header
echo "| Backoff | Probability | Avg Flow FCT (ms) | Max Flow FCT (ms) | Avg Rank Task Duration (ms) | Max Rank Task Duration (ms) | Avg Group Task Duration (ms) | Max Group Task Duration (ms) | Avg Pkt Delay (ms) | Workload Makespan (s) |" > "$LOG_DIR/detailed_summary.csv"

jobs="4"

for backoff in "10us" "100us" "1ms"
do
    echo "--- Testing Backoff: $backoff ---"
    for prob in 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
    do
        echo "[Jobs=$jobs, Backoff=$backoff, Prob=$prob] Running..."
        

        LOG_FILE="$LOG_DIR/run_${backoff}_${prob}.log"
        
        # Determine Executable Path (Prioritize Build Artifact)
        if [ -f "./build/fattree-moe-multijob-test" ]; then
            EXE="./build/fattree-moe-multijob-test"
        elif [ -f "./fattree-moe-multijob-test" ]; then
            EXE="./fattree-moe-multijob-test"
        else
            echo "Error: specific executable 'fattree-moe-multijob-test' not found in ./build/ or ."
            exit 1
        fi

        echo "Using Executable: $EXE"
        echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"

        # Run Simulation
        $EXE --scheduler=fixed --prob=$prob --backoff=$backoff --jobs=$jobs > "$LOG_FILE" 2>&1
        
        # Analyze and append to summary
        RESULT=$(python3 scratch/net-jit/analyze_detailed_flows_full.py "$LOG_FILE")
        
        # Result format expected: <AvgFlow> | <MaxFlow> | <AvgRank> | <MaxRank> | <AvgGroup> | <MaxGroup> | <AvgPktDelay> | <Makespan>
        echo "| $backoff | $prob | $RESULT |" >> "$LOG_DIR/detailed_summary.csv"
        
        echo "   -> Result: $RESULT"
    done
done

echo "=========================================="
echo "Full Tests Complete. Summary:"
cat "$LOG_DIR/detailed_summary.txt"
cat "$LOG_DIR/detailed_summary.csv"
