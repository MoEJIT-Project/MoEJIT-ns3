
import sys
import re

def analyze_log(filename):
    task_start_times = {}
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

                    if (rank, task_id) in task_start_times:
                        start_t = task_start_times[(rank, task_id)]
                        finish_t = start_t + (duration_ns / 1e9)
                        all_finish_times.append(finish_t)

                # 3. Parse FCT
                m_fct = re.search(r'\[FCT\] (\d+)', line)
                if m_fct:
                    fct_values_ns.append(float(m_fct.group(1)))

                # 4. Parse Pkt Delay
                m_delay = re.search(r'\[FlowMonitor\] AvgPacketDelay: ([\d\.]+) ms', line)
                if m_delay:
                    avg_pkt_delay = float(m_delay.group(1))

    except FileNotFoundError:
        return None

    if not durations_ms:
        return None

    avg_dur = sum(durations_ms) / len(durations_ms)
    max_dur = max(durations_ms)
    
    makespan = 0
    if all_finish_times and all_start_times:
       makespan = max(all_finish_times) - min(all_start_times)

    avg_fct = 0
    if fct_values_ns:
        avg_fct = sum(fct_values_ns) / len(fct_values_ns) / 1e6

    return {
        "avg_dur": avg_dur,
        "avg_fct": avg_fct,
        "pkt_delay": avg_pkt_delay,
        "makespan": makespan
    }

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python analyze_single.py <log_file> <prob_label>")
        sys.exit(1)

    filename = sys.argv[1]
    prob = sys.argv[2]
    
    stats = analyze_log(filename)
    if stats:
        print(f"| {prob} | {stats['avg_dur']:.2f} | {stats['avg_fct']:.2f} | {stats['pkt_delay']:.2f} | {stats['makespan']:.4f} |")
    else:
        print(f"| {prob} | ERROR | - | - | - |")
