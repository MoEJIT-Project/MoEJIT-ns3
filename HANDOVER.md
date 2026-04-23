# NetJIT Project Handover Guide

## 1. Project Overview
This project simulates **Mixture-of-Experts (MoE)** training workloads on a high-performance **Spine-Leaf** network using NS-3. 
The core research goal is **"NetJIT" (Network Just-In-Time)** scheduling: mitigating network congestion (Incast) during global `AllToAllV` communication phases by using probabilistic admission control and backoff strategies.

## 2. Key Files & Structure
Paths are relative to the NS-3 root directory.

### Source Code
*   **Simulation Runner**: `scratch/net-jit/spine-leaf-trace-test.cpp`
    *   Setups the Spine-Leaf topology and installs MoE applications.
    *   Can be used to run comparison tests against baseline scheduling.
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
*   **Traces**: `scratch/net-jit/moe-traces/`
    *   Contains the MoE training trace files (e.g., `ep_trace_*.jsonl`) used for realistic workload simulation.

## 3. How to Run
### Single Test (Debugging)
To run a specific Spine-Leaf simulation:
```bash
cmake --build build --target "spine-leaf-trace-test"
./build/spine-leaf-trace-test
```
