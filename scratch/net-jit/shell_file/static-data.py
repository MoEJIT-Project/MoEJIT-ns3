'''
1. 统计平均吞吐量= 最终吞吐量(from 吞吐量曲线) / （最后一个batch的结束时间点 - 第一个batch的开始时间点）
    1. 最终吞吐量：
        1. cd throughput_curve_rx文件夹
        2. 对每一个文件, 遍历,获取最大的line[1]
        3. 加起来是最终吞吐量
    2. batch总时长
        1. 读取batch_start.txt
        2. 获取最小的line[0] = all_batch_start
        3. 读取batch_end.txt
        4. 获取最大的line[0] = all_batch_end

2.平均包延时:flowmonitor
    1. 执行cp ../parse-results.py ./
    2. 运行parse-results.py flow.xml
    3. 读取result.txt
    4. 获取有“delay”的一行的line[1] !! ns
    
3.带宽利用率 = 最终吞吐量(from 吞吐量曲线) / 加权带宽(from 优化的时间点)
    1. 最终吞吐量：已经得到
    2. 加权带宽 
        1. 打开application_start.txt, 记录最小的line[0] == application_start
        2. 打开application_end.txt, 记录最大的line[0] == application_end
        3. 判断是否有optimization_timestamp.txt文件
        4. 如果有，则读取进line[1] optimization_timestamp[],如果line[1] > application_end抛弃
        5. 打开restore_timestamp.txt
        6. 读取进line[1]  restore_timestamp[],如果line[1] > application_end抛弃
        7. 总优化时间 = sum(restore_timestamp - application_start) 
        5. 计算加权带宽=  400 * 1e9* 总优化时间 + 100 * 1e9 * ((最后一个app end时间 - 第一个app start时间) - 总优化时间) 
        6. 最终吞吐量 * 8 / 加权带宽
    

4.平均batch时间(from batch的开始&结束时间点)
    1. 创建一个dict node_batch_to_time = {}
    1. 读取batch_start.txt
    2. 遍历每一行,记录进node_batch_to_time[line[1]][line[2]][0] = line[0]
    3. 读取batch_end.txt
    4. 遍历每一行,记录进node_batch_to_time[line[1]][line[2]][1] = line[0]
    5. 获得平均时间 = average(node_batch_to_time[i][j][1] - node_batch_to_time[i][j][0]) / len(batch_end) 检查[0]和[1]都存在

'''

import os
scale_factor = 0.001

def get_real_bandwidth(status_change_timestamp, begin_timestamp, end_timestamp):
    result = 0.0
    last_timestamp = begin_timestamp
    begin_index = 0
    begin_status = status_change_timestamp[begin_index][1]
    while begin_index < len(status_change_timestamp) and status_change_timestamp[begin_index][0] <= begin_timestamp:
        begin_status = status_change_timestamp[begin_index][1]
        begin_index = begin_index + 1
    end_index = - 1
    end_status = status_change_timestamp[end_index][1]
    while end_index >= -len(status_change_timestamp) and status_change_timestamp[end_index][0] >= end_timestamp:
        end_status = status_change_timestamp[end_index][1]
        end_index = end_index - 1
    last_status = begin_status
    timestamps = status_change_timestamp[begin_index:end_index + 1] if end_index < -1 else status_change_timestamp[begin_index:]
    for timestamp, optimized in timestamps:
        if last_status:
            result += 400 * (timestamp - max(last_timestamp, begin_timestamp)) / (end_timestamp - begin_timestamp)
        else:
            result += 100 * (timestamp - max(last_timestamp, begin_timestamp)) / (end_timestamp - begin_timestamp)
        last_status = optimized
        last_timestamp = timestamp
    if last_status:
        result += 400 * (end_timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
    else:
        result += 100 * (end_timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
    return result 


# 1. 计算平均吞吐量
def calculate_throughput(start_batch, end_batch):
    # 获取batch总时长
    with open("batch_start.txt", 'r') as start_file, open("batch_end.txt", 'r') as end_file:
        all_batch_start = min(float(line.split()[0]) for line in start_file if int(line.split()[2]) >= start_batch)
        all_batch_end = max(float(line.split()[0]) for line in end_file if int(line.split()[2]) <= end_batch)
        
    throughput_folder = "throughput_curve_rx"
    max_throughputs = []
    min_throughputs = []

    # 找到每个文件中的最大吞吐量
    for filename in os.listdir(throughput_folder):
        with open(os.path.join(throughput_folder, filename), 'r') as file:
            lines = file.readlines()
            
            max_throughput = 0
            for line in lines:
                if int(line.split()[0]) <= all_batch_end:
                    max_throughput = max(max_throughput, float(line.split()[1]))
                else:
                    break
            max_throughputs.append(max_throughput)
            
            for line in lines:
                if int(line.split()[0]) >= all_batch_start:
                    min_throughputs.append(float(line.split()[1]))
                    break
    
    assert len(max_throughputs) == len(min_throughputs),"The length of max_throughputs and min_throughputs should be equal"
    final_throughput = sum(max_throughputs) - sum(min_throughputs)

    batch_duration = all_batch_end - all_batch_start

    print("final throughput: ", final_throughput)
    print("Batch duration: ", batch_duration / 1e9, "s")
    average_throughput = final_throughput / (batch_duration / 1e9)  # final_throughput(Byte), batch_duration(ns)， Byte * 1e9 / ns = Bps 
    return average_throughput / scale_factor * 8, all_batch_start, all_batch_end # B * 8 = b

# 2. 计算平均包延时
def calculate_average_delay(all_batch_start):
    if not os.path.exists("parse-results.py"):
        os.system("cp ../parse-results.py ./")
        os.system(f"python3 parse-results.py flow.xml {all_batch_start}")

    with open("result.txt", 'r') as result_file:
        for line in result_file:
            if "!!delay" in line:
                delay = float(line.split()[1])
                return delay # ns

# 3. 计算带宽利用率
def calculate_bandwidth_utilization(final_throughput, all_batch_start, all_batch_end):
    application_start = all_batch_start
    application_end = all_batch_end
    
     # 获取优化和恢复时间戳
    # 判断是否存在'optimize_timestamp.txt'
    optimization_timestamp = []
    restore_timestamp = []
    if not os.path.exists('optimize_timestamp.txt'):
        pass
    else:
        with open('optimize_timestamp.txt', 'r') as f:
            for line in f:
                optimization_timestamp.append(float(line.split()[1]))

    if not os.path.exists('restore_timestamp.txt'):
        pass
    else:
        with open('restore_timestamp.txt', 'r') as f:
            for line in f:
                restore_timestamp.append(float(line.split()[1]))

    optimization_timestamp.sort()
    restore_timestamp.sort()
    optimization_timestamp.reverse()
    restore_timestamp.reverse()

    # input:optimization_timestamp, restore_timestamp
    # output: status_change_timestamp
    status_change_timestamp = [(0, False)]
    is_opt = False
    while len(optimization_timestamp) > 0 or len(restore_timestamp) > 0:
        if len(optimization_timestamp) == 0:
            timestamp = restore_timestamp.pop()
            is_opt = False
        elif len(restore_timestamp) == 0:
            timestamp = optimization_timestamp.pop()
            is_opt = True
        else:
            if optimization_timestamp[-1] < restore_timestamp[-1]:
                timestamp = optimization_timestamp.pop()
                is_opt = True
            else:
                timestamp = restore_timestamp.pop()
                is_opt = False
        if is_opt != status_change_timestamp[-1][1]:
            status_change_timestamp.append((timestamp, is_opt))
    
    weighted_bandwidth = get_real_bandwidth(status_change_timestamp, application_start, application_end)
    print(os.getcwd())
    print("weighted_bandwidth: ", weighted_bandwidth)
    bandwidth_utilization = final_throughput / 1e9 / weighted_bandwidth  # throughput: b, bd: b
    return bandwidth_utilization

# 4. 计算平均batch时间
def calculate_average_batch_time(all_batch_start, all_batch_end):
    node_batch_to_time = {}

    # 读取batch_start.txt
    with open("batch_start.txt", 'r') as start_file:
        for line in start_file:
            timestamp, node, batch = map(int, line.split())
            if node not in node_batch_to_time:
                node_batch_to_time[node] = {}
            if batch not in node_batch_to_time[node]:
                node_batch_to_time[node][batch] = [timestamp, None]

    # 读取batch_end.txt
    with open("batch_end.txt", 'r') as end_file:
        for line in end_file:
            timestamp, node, batch = map(int, line.split())
            if node in node_batch_to_time and batch in node_batch_to_time[node]:
                node_batch_to_time[node][batch][1] = timestamp

    total_time = 0
    num_batches = 0

    # 计算平均时间
    for node, batches in node_batch_to_time.items():
        for batch, timestamps in batches.items():
            if timestamps[0] is not None and timestamps[1] is not None and timestamps[0] > all_batch_start and timestamps[1] < all_batch_end:
                total_time += timestamps[1] - timestamps[0]
                num_batches += 1

    if num_batches == 0:
        return None

    average_time = total_time / num_batches
    return average_time # ns

def main():
    for folder in os.listdir('.'):
        if os.path.isdir(folder) and ("opt_delay_1000000000" in folder or "none" in folder):
            os.chdir(folder)  # 进入当前文件夹

            with open("static-data.txt", "w") as f:
                f.write("平均吞吐量(Gbps) 平均包延时(s) 带宽利用率 平均batch时间(s) \n")
            
            for batch_id in range(1, 21):
                start_batch = batch_id
                end_batch = batch_id
                # 计算各项指标
                average_throughput, all_batch_start, all_batch_end = calculate_throughput(start_batch, end_batch)
                average_delay = calculate_average_delay(all_batch_start)
                bandwidth_utilization = calculate_bandwidth_utilization(average_throughput, all_batch_start, all_batch_end)
                average_batch_time = calculate_average_batch_time(all_batch_start, all_batch_end)
    
                # 将结果写入static-data.txt
                with open("static-data.txt", "a") as f:
                    f.write(str(average_throughput / 1e9) + " ")
                    f.write(str(average_delay/ 1e9) + " ")
                    f.write(str(bandwidth_utilization) + " ")
                    f.write(str(average_batch_time / 1e9) + " ")
                    f.write("\n")

            os.chdir('..')  # 返回上一级目录
            
# collect all static data in static-data.txt
def collect():
    with open("static-data.txt", "w") as file1:
        # 遍历当前路径下的所有文件夹
        for folder in os.listdir('.'):
            if os.path.isdir(folder) and ("opt_delay_1000000000" in folder or "none" in folder):
                file1.write("Folder Name: " + folder + "\n")
                # 打开static-data.txt文件并将内容添加到file1
                static_data_path = os.path.join(folder, "static-data.txt")
                if os.path.exists(static_data_path):
                    with open(static_data_path, "r") as static_file:
                        file1.write(static_file.read() + "\n")

if __name__ == "__main__":
    main()
    collect()