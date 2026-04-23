import sys
import re
import statistics

def parse_log(log_file):
    flow_stats = {}
    app_durations = {} # task_id -> rank -> duration_ns
    
    with open(log_file, 'r') as f:
        for line in f:
            # App Level Duration
            match = re.search(r"Rank (\d+): Task (\d+) Finished. Duration: (\d+) ns", line)
            if match:
                rank = int(match.group(1))
                task_id = int(match.group(2))
                dur = float(match.group(3)) / 1e6 # ms
                if task_id not in app_durations: app_durations[task_id] = {}
                app_durations[task_id][rank] = dur
                
            # Flow Tx
            if "[FlowTx]" in line:
                parts = line.strip().split()
                if len(parts) >= 5:
                    task_id = int(parts[1])
                    src = int(parts[2])
                    dst = int(parts[3])
                    time = float(parts[4])
                    if task_id not in flow_stats: flow_stats[task_id] = {}
                    if src not in flow_stats[task_id]: flow_stats[task_id][src] = {}
                    if dst not in flow_stats[task_id][src]: flow_stats[task_id][src][dst] = {}
                    flow_stats[task_id][src][dst]['start'] = time
                    
            # Flow Rx
            if "[FlowRx]" in line:
                parts = line.strip().split()
                if len(parts) >= 5:
                    task_id = int(parts[1])
                    src = int(parts[2])
                    dst = int(parts[3]) # MyRank
                    time = float(parts[4])
                    if task_id not in flow_stats: flow_stats[task_id] = {}
                    if src not in flow_stats[task_id]: flow_stats[task_id][src] = {}
                    if dst not in flow_stats[task_id][src]: flow_stats[task_id][src][dst] = {}
                    flow_stats[task_id][src][dst]['end'] = time

    # Calculate Flow Durations
    flow_durations = []
    for tid, srcs in flow_stats.items():
        for src, dsts in srcs.items():
            for dst, times in dsts.items():
                if 'start' in times and 'end' in times:
                    duration = (times['end'] - times['start']) * 1000.0 # ms
                    flow_durations.append(duration)
    
    avg_flow = 0.0
    max_flow = 0.0
    if flow_durations:
        avg_flow = statistics.mean(flow_durations)
        max_flow = max(flow_durations)
        
    # Calculate Rank Durations
    all_rank_durs = []
    for tid, ranks in app_durations.items():
        for r, d in ranks.items():
            all_rank_durs.append(d)
            
    avg_rank = 0.0
    max_rank = 0.0
    if all_rank_durs:
        avg_rank = statistics.mean(all_rank_durs)
        max_rank = max(all_rank_durs)
        
    # Output: AvgFlow | MaxFlow | AvgRank | MaxRank
    print(f"{avg_flow:.2f} | {max_flow:.2f} | {avg_rank:.2f} | {max_rank:.2f}")

if __name__ == "__main__":
    parse_log(sys.argv[1])
