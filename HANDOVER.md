# NetJIT Project Handover Guide

## 1. Project Overview
This project simulates **Mixture-of-Experts (MoE)** training workloads on a high-performance FatTree network using NS-3. 
The core research goal is **"NetJIT" (Network Just-In-Time)** scheduling: mitigating network congestion (Incast) during global `AllToAllV` communication phases by using probabilistic admission control and backoff strategies.

## 2. Key Files & Structure
Paths are relative to the NS-3 root directory (`/home/dichox/ns-3/`).

### Source Code
*   **Simulation Runner**: `scratch/net-jit/fattree-moe-multijob-test.cpp`
    *   Setups the FatTree topology and installs MoE applications.
    *   Currently configured to run 4 concurrent jobs.
*   **Application Logic**: `scratch/net-jit/moe-jit.h`
    *   Defines `MoeJITApplication`.
    *   Contains the **Admission Control** logic (`CheckAdmission`, `backoff`).
*   **Trace Parser**: `scratch/net-jit/moe-trace-parser.h`
    *   Parses `.jsonl` trace files.
    *   **Note**: Recently updated to ignore `GATE_END` events.
*   **MPI Model**: `src/mpi-application/model/mpi-communicator.h`
    *   Simulates `AllToAllV` and logs `[FlowTx]`/`[FlowRx]` for metric calculation.

### Scripts
*   **Automated Test Suite**: `scratch/net-jit/run_multijob_detailed_full_test.sh`
    *   **MAIN SCRIPT**. Runs the full parameter sweep (Backoff: 10us/100us/1ms, Prob: 0.1-1.0).
*   **Analysis Logic**: `scratch/net-jit/analyze_detailed_flows_full.py`
    *   Parses simulation logs to calculate FCT, Task Duration, and Group Efficiency.

### Data
*   **Traces**: `scratch/net-jit/moe-traces/new/`
    *   Contains the latest `ep_trace_20260203_*.jsonl` files.

## 3. How to Run
### Full Regression Test
To replicate the latest results, simply run:
```bash
./scratch/net-jit/run_multijob_detailed_full_test.sh
```
*   **Output**: `scratch/net-jit/logs/detailed_full/detailed_summary.csv`
*   **Logs**: `scratch/net-jit/logs/detailed_full/run_<backoff>_<prob>.log`

### Single Test (Debugging)
To run a specific configuration (e.g., trying a new parameter):
```bash
./fattree-moe-multijob-test --scheduler=fixed --prob=0.4 --backoff=1ms --jobs=4
```

## 4. Key Metrics & Interpretation
The summary CSV contains the following columns:

1.  **Avg/Max Flow FCT (ms)**: 
    *   Time from "Start Send" to "Finish Recv" for individual flows.
    *   **Significance**: Measures network congestion. Pure network latency.
2.  **Avg/Max Rank Task Duration (ms)**:
    *   Time for a single Rank to complete one AllToAllV operation.
3.  **Avg/Max Group Task Duration (ms)**:
    *   Time from the *first* Rank starting a task to the *last* Rank finishing it within a Job.
    *   **Significance**: Measures **Synchronization Overhead**. If Ranks are out of sync (due to excessive backoff), this value spikes, hurting training performance.
4.  **Avg Pkt Delay (ms)**:
    *   Physical packet switching delay.
5.  **Makespan (s)**:
    *   Total simulation completion time.

### Current Best Performance
*   **Configuration**: Backoff = 1ms, Probability = 0.4
*   **Characteristics**: Lowest Flow FCT (~6ms) with minimal Group Synchronization overhead (~35ms).

## 5. Recent Changes & Status
*   **Git Branch**: `moe_jit` (Already pushed to origin).
*   **Trace Format**: The parser now handles `GATE_END` events by ignoring them (they contain no traffic).
*   **Metric Update**: Added "Group Task Duration" to detect desynchronization issues caused by low admission probabilities.

## 6. Next Steps / TODO
*   **Dynamic Probability**: Currently using fixed probability. Future work could implement dynamic adjustment based on congestion signals.
*   **Topology Scaling**: Validated on 16 nodes (k=4 FatTree). Consider scaling to larger k.
