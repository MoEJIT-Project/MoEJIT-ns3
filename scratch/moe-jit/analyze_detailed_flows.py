import sys
import re
import statistics

def parse_log(log_files):
    # flow_stats[task_id][src][dst] = {'start': time, 'end': time}
    flow_stats = {}
    
    # rank_task_stats[rank][task_id] = {'start': time, 'end': time} -> derived from flows?
    # No, Rank Duration is logged as "Duration: ... ns". 
    # But user wants "Time from First Packet Sent to Last Packet Recv" per rank per task.
    # We can approximate this by:
    #   RankTaskStart = Min(FlowTx.Time of flows from Rank)
    #   RankTaskEnd = Max(FlowRx.Time of flows to Rank)
    #   Wait, "Rank Task Duration" is usually "Wall time spent in AllToAll".
    #   The C++ code logs "Task X Finished. Duration: Y ns". This is the app-level view.
    #   Let's collect that first.
    
    app_durations = {} # task_id -> rank -> duration_ns
    
    # Flow Parsing
    # [FlowTx] <TaskId> <Src> <Dst> <Time>
    # [FlowRx] <TaskId> <Src> <Dst> <Time>
    
    for log_file in log_files:
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
                    # Parts: [FlowTx], TaskId, Src, Dst, Time
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
                    # Parts: [FlowRx], TaskId, Src, Dst, Time
                    # Note: Src is Sender, Dst is Receiver (MyRank)
                    if len(parts) >= 5:
                        task_id = int(parts[1])
                        src = int(parts[2])
                        dst = int(parts[3]) # This is MyRank (Receiver)
                        time = float(parts[4])
                        
                        if task_id not in flow_stats: flow_stats[task_id] = {}
                        if src not in flow_stats[task_id]: flow_stats[task_id][src] = {}
                        if dst not in flow_stats[task_id][src]: flow_stats[task_id][src][dst] = {}
                        
                        flow_stats[task_id][src][dst]['end'] = time

    # Calculate Flow Durations
    flow_durations = []
    
    print(f"{'TaskID':<8} {'Src':<4} {'Dst':<4} {'Flow_Time(ms)':<15}")
    print("-" * 35)
    
    for tid, srcs in flow_stats.items():
        for src, dsts in srcs.items():
            for dst, times in dsts.items():
                if 'start' in times and 'end' in times:
                    duration = (times['end'] - times['start']) * 1000.0 # ms
                    flow_durations.append(duration)
                    # Optional: Print every flow? Too spammy for large run.
                    # Print only outliers?
                    if duration > 100.0: # Print slow flows
                        print(f"{tid:<8} {src:<4} {dst:<4} {duration:<15.2f} (*)")
                
    if not flow_durations:
        print("No paired FlowTx/FlowRx found.")
    else:
        avg_flow = statistics.mean(flow_durations)
        max_flow = max(flow_durations)
        min_flow = min(flow_durations)
        stdev_flow = statistics.stdev(flow_durations) if len(flow_durations) > 1 else 0
        
        print("-" * 35)
        print(f"Flow Duration Stats (N={len(flow_durations)}):")
        print(f"  Avg: {avg_flow:.2f} ms")
        print(f"  Min: {min_flow:.2f} ms")
        print(f"  Max: {max_flow:.2f} ms")
        print(f"  Stdev: {stdev_flow:.2f} ms")
        
    print("\n")
    print("Rank Task Duration Stats:")
    all_rank_durs = []
    for tid, ranks in app_durations.items():
        for r, d in ranks.items():
            all_rank_durs.append(d)
            
    if all_rank_durs:
        print(f"  Avg: {statistics.mean(all_rank_durs):.2f} ms")
        print(f"  Max: {max(all_rank_durs):.2f} ms")
        print(f"  Min: {min(all_rank_durs):.2f} ms")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_detailed_flows.py <log_file>")
        sys.exit(1)
    parse_log([sys.argv[1]])
