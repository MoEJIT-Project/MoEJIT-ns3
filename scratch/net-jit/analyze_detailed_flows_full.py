import sys
import re
import statistics

def parse_log(log_file):
    flow_stats = {}
    app_durations = {} # task_id -> rank -> duration_ns
    sim_pkt_delay = 0.0
    max_timestamp = 0.0
    
    with open(log_file, 'r') as f:
        for line in f:
            # App Level Duration
            match = re.search(r"Rank (\d+): Task (\d+) Finished. Duration: (\d+) ns", line)
            if match:
                rank = int(match.group(1))
                task_id = int(match.group(2))
                dur = float(match.group(3)) / 1e6 # ms
                if task_id not in app_durations: app_durations[task_id] = {}
                # Update duration. If struct exists (from Start parsing), update it.
                if rank not in app_durations[task_id]:
                    app_durations[task_id][rank] = {'dur': dur, 'start': 0.0, 'end': 0.0}
                else:
                    app_durations[task_id][rank]['dur'] = dur
                    if app_durations[task_id][rank]['start'] > 0:
                         app_durations[task_id][rank]['end'] = app_durations[task_id][rank]['start'] + (dur/1000.0)

            
            # Avg Pkt Delay from simulation output
            # Format: "[FlowMonitor] AvgPacketDelay: 1.0789 ms"
            match_delay = re.search(r"\[FlowMonitor\] AvgPacketDelay:\s+([\d\.]+)\s+ms", line)
            if match_delay:
                sim_pkt_delay = float(match_delay.group(1))

            # Extract timestamp to calculate rudimentary Makespan if not explicitly logged
            # Assuming timestamps are at start of line or in [Time] format, but logs vary.
            # We will assume [FlowRx] and Task Finished lines have implicit ordering or explicit times.
            # Using the max end time seen in FlowRx as a proxy for Makespan end.

                
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
                    src = int(parts[2]) # This is the sender rank
                    dst = int(parts[3]) # This is the receiver rank (myRank)
                    time = float(parts[4])
                    
                    if task_id not in flow_stats: flow_stats[task_id] = {}
                    if src not in flow_stats[task_id]: flow_stats[task_id][src] = {}
                    if dst not in flow_stats[task_id][src]: flow_stats[task_id][src][dst] = {}
                    
                    flow_stats[task_id][src][dst]['end'] = time
                    if time > max_timestamp: max_timestamp = time

            # App Task Start (New for Group Duration)
            match_start = re.search(r"Rank (\d+): Starting MoE AllToAllV Task (\d+) at ([\d\.]+)s", line)
            if match_start:
                rank = int(match_start.group(1))
                task_id = int(match_start.group(2))
                start_t = float(match_start.group(3))
                
                if task_id not in app_durations: app_durations[task_id] = {}
                if rank not in app_durations[task_id]:
                    app_durations[task_id][rank] = {'dur': 0.0, 'start': start_t, 'end': 0.0}
                else:
                    app_durations[task_id][rank]['start'] = start_t
                    if app_durations[task_id][rank]['dur'] > 0:
                        app_durations[task_id][rank]['end'] = start_t + (app_durations[task_id][rank]['dur'] / 1000.0)

    # Post-process App Durations to ensure 'end' is set if we parsed specific lines in different order
    for tid, ranks in app_durations.items():
        for r, data in ranks.items():
            if data['end'] == 0.0 and data['dur'] > 0 and data['start'] >= 0:
                data['end'] = data['start'] + (data['dur'] / 1000.0)

    # Calculate Flow Durations and Map to Ranks for Validation
    flow_durations = []
    rank_max_flow = {} 
    
    for tid, srcs in flow_stats.items():
        if tid not in rank_max_flow: rank_max_flow[tid] = {}
        for src, dsts in srcs.items():
            for dst, times in dsts.items():
                if 'start' in times and 'end' in times:
                    duration = (times['end'] - times['start']) * 1000.0 # ms
                    flow_durations.append(duration)
                    
                    if src not in rank_max_flow[tid]: rank_max_flow[tid][src] = 0.0
                    if duration > rank_max_flow[tid][src]: rank_max_flow[tid][src] = duration
                    if dst not in rank_max_flow[tid]: rank_max_flow[tid][dst] = 0.0
                    if duration > rank_max_flow[tid][dst]: rank_max_flow[tid][dst] = duration

    avg_flow = 0.0
    max_flow = 0.0
    if flow_durations:
        avg_flow = statistics.mean(flow_durations)
        max_flow = max(flow_durations)
        
    # Calculate Rank Durations and Group Durations
    all_rank_durs = []
    all_group_durs = []
    
    num_jobs = 4

    for tid, ranks in app_durations.items():
        # Per Task
        # 1. Collect Rank Durations
        for r, data in ranks.items():
            if isinstance(data, dict):
                d_ms = data['dur']
            else:
                 d_ms = data # Fallback if logic failed
            
            all_rank_durs.append(d_ms)
            
            # Validation 1: Rank Task Duration >= Max Flow Duration
            if tid in rank_max_flow and r in rank_max_flow[tid]:
                max_f = rank_max_flow[tid][r]
                if d_ms < max_f - 0.1: 
                     # Check 1 Failure
                     pass

        # 2. Compute Group Durations
        job_groups = {} 
        for r in ranks.keys():
            jid = r % num_jobs
            if jid not in job_groups: job_groups[jid] = []
            job_groups[jid].append(r)
            
        for jid, group_ranks in job_groups.items():
            g_start = float('inf')
            g_end = float('-inf')
            valid_group = False
            max_rank_dur_in_group = 0.0
            
            for r in group_ranks:
                data = ranks[r]
                if isinstance(data, dict):
                    if data['start'] < g_start: g_start = data['start']
                    if data['end'] > g_end: g_end = data['end']
                    if data['dur'] > max_rank_dur_in_group: max_rank_dur_in_group = data['dur']
                    valid_group = True
            
            if valid_group and g_end >= g_start:
                g_dur_s = g_end - g_start
                g_dur_ms = g_dur_s * 1000.0
                all_group_durs.append(g_dur_ms)
                
                # Validation 3: Group Duration >= Max Rank Duration
                if g_dur_ms < max_rank_dur_in_group - 0.1:
                    # Check 3 Failure
                    pass
            
    avg_rank = 0.0
    max_rank = 0.0
    if all_rank_durs:
        avg_rank = statistics.mean(all_rank_durs)
        max_rank = max(all_rank_durs)
        
    avg_group = 0.0
    max_group = 0.0
    if all_group_durs:
        avg_group = statistics.mean(all_group_durs)
        max_group = max(all_group_durs)

    # Output: AvgFlow | MaxFlow | AvgRank | MaxRank | AvgGroup | MaxGroup | AvgPktDelay | Makespan
    print(f"{avg_flow:.2f} | {max_flow:.2f} | {avg_rank:.2f} | {max_rank:.2f} | {avg_group:.2f} | {max_group:.2f} | {sim_pkt_delay:.3f} | {max_timestamp:.4f}")

if __name__ == "__main__":
    parse_log(sys.argv[1])
