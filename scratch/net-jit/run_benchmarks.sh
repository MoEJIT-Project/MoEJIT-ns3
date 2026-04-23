#!/bin/bash

# Definition of the log directory
LOG_DIR="scratch/net-jit/logs"

# Create log directory if it doesn't exist
mkdir -p "$LOG_DIR"

echo "=========================================="
echo "Running MoE Benchmark Suite"
echo "Logs will be saved to: $LOG_DIR"
echo "=========================================="

# 1. FatTree - Distributed
echo "[1/6] Running FatTree (Distributed)..."
./build/fattree-moe-trace-test --scheduler=dist > "$LOG_DIR/fattree_dist.log" 2>&1

# 2. FatTree - Centralized
echo "[2/6] Running FatTree (Centralized)..."
./build/fattree-moe-trace-test --scheduler=central > "$LOG_DIR/fattree_central.log" 2>&1

# 3. FatTree - Baseline (FIFO)
echo "[3/6] Running FatTree (Baseline/FIFO)..."
./build/fattree-moe-trace-test --scheduler=fifo > "$LOG_DIR/fattree_fifo.log" 2>&1

# 4. Dragonfly - Distributed
echo "[4/6] Running Dragonfly (Distributed)..."
./build/dragonfly-moe-trace-test --scheduler=dist > "$LOG_DIR/dragonfly_dist.log" 2>&1

# 5. Dragonfly - Centralized
echo "[5/6] Running Dragonfly (Centralized)..."
./build/dragonfly-moe-trace-test --scheduler=central > "$LOG_DIR/dragonfly_central.log" 2>&1

# 6. Dragonfly - Baseline (FIFO)
echo "[6/6] Running Dragonfly (Baseline/FIFO)..."
./build/dragonfly-moe-trace-test --scheduler=fifo > "$LOG_DIR/dragonfly_fifo.log" 2>&1

echo "=========================================="
echo "Simulations Complete. Running Analysis..."
echo "=========================================="

# Run Python Analysis
python3 scratch/net-jit/analyze_results.py
