
import re

def analyze_log(filename):
    task_start_times = {}  # (rank, task_id) -> start_time_s
    all_finish_times = []
    all_start_times = []
    durations_ms = []
    fct_values_ns = []
    avg_pkt_delay = 0.0

    try:
        with open(filename, 'r') as f:
            for line in f:
                # 1. Parse Start Time
                m_start = re.search(r'Rank (\d+): Starting MoE AllToAllV Task (\d+) at ([\d\.]+)s', line)
                if m_start:
                    rank = int(m_start.group(1))
                    task_id = int(m_start.group(2))
                    start_t = float(m_start.group(3))
                    
                    task_start_times[(rank, task_id)] = start_t
                    all_start_times.append(start_t)

                # 2. Parse Task Finish Time & Duration
                m_fin = re.search(r'Rank (\d+): Task (\d+) Finished. Duration: (\d+) ns', line)
                if m_fin:
                    rank = int(m_fin.group(1))
                    task_id = int(m_fin.group(2))
                    duration_ns = float(m_fin.group(3))
                    duration_ms = duration_ns / 1e6
                    durations_ms.append(duration_ms)

                    # Calculate absolute finish time
                    if (rank, task_id) in task_start_times:
                        start_t = task_start_times[(rank, task_id)]
                        finish_t = start_t + (duration_ns / 1e9)
                        all_finish_times.append(finish_t)

                # 3. Parse FCT
                # [FCT] 123456
                m_fct = re.search(r'\[FCT\] (\d+)', line)
                if m_fct:
                    fct_values_ns.append(float(m_fct.group(1)))

                # 4. Parse Pkt Delay
                # [FlowMonitor] AvgPacketDelay: 12.34 ms
                m_delay = re.search(r'\[FlowMonitor\] AvgPacketDelay: ([\d\.]+) ms', line)
                if m_delay:
                    avg_pkt_delay = float(m_delay.group(1))

    except FileNotFoundError:
        return None

    if not durations_ms:
        return {"avg_dur": 0, "max_dur": 0, "makespan": 0, "count": 0, "avg_fct": 0, "pkt_delay": 0}

    avg_dur = sum(durations_ms) / len(durations_ms)
    max_dur = max(durations_ms)
    
    # Meaningful Makespan
    makespan = 0
    if all_finish_times and all_start_times:
       makespan = max(all_finish_times) - min(all_start_times)

    # Average FCT
    avg_fct = 0
    if fct_values_ns:
        avg_fct = sum(fct_values_ns) / len(fct_values_ns) / 1e6 # ms

    return {
        "avg_dur": avg_dur,
        "max_dur": max_dur,
        "makespan": makespan,
        "count": len(durations_ms),
        "avg_fct": avg_fct,
        "pkt_delay": avg_pkt_delay
    }

files = {
    "FatTree / Dist": "scratch/net-jit/logs/fattree_dist.log",
    "FatTree / Central": "scratch/net-jit/logs/fattree_central.log",
    "FatTree / Baseline (FIFO)": "scratch/net-jit/logs/fattree_fifo.log",
    "Dragonfly / Dist": "scratch/net-jit/logs/dragonfly_dist.log",
    "Dragonfly / Central": "scratch/net-jit/logs/dragonfly_central.log",
    "Dragonfly / Baseline (FIFO)": "scratch/net-jit/logs/dragonfly_fifo.log"
}

print(f"| Topology / Scheduler | Avg Task Duration (ms) | Avg FCT (ms) | Avg Pkt Delay (ms) | Workload Makespan (s) |")
print(f"|---|---|---|---|---|")

for name, path in files.items():
    stats = analyze_log(path)
    if stats and stats["count"] > 0:
        print(f"| {name} | {stats['avg_dur']:.2f} | {stats['avg_fct']:.2f} | {stats['pkt_delay']:.2f} | {stats['makespan']:.4f} |")
