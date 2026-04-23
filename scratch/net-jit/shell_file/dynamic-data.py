'''
scale_factor = 0.001
1. 总吞吐量曲线
    1. 打开application_end.txt,记录最大的line[0] = application_end
    1. cd throughput_curve_rx文件夹
    2. 创建throughput_curve,对每一个文件遍历,对第i行,如果line[0] > application_end,则结束,
    反之,throughput_curve[i] += line[1]
    3. 从结束到开始,throughput_curve[i] -= throughput_curve[i-1]
    4. throughput_curve_1s: 每十个throughput_curve[i]相加得到，例如throughput_curve_1s[0] = sum(throughput_curve[0:10])
    , throughput_curve_1s[1] = sum(throughput_curve[10:20]), throughput_curve_1s[2] = sum(throughput_curve[20:30])
    5. 返回throughput_curve_1s =/ scale_factor

2. 带宽利用率曲线（(吞吐量曲线:)吞吐量总量相差 / (优化的时间点:)加权带宽）
    1. throughput_curve_1s如上获得
    2. len(throughput_curve_1s) = h, bd_opt = [], len(bd_opt) = h
    3. 打开optimization_timestamp.txt,对每一行,记录optimization_timestamp[i] = line[1]
       打开restore_timestamp.txt,对每一行,记录restore_timestamp[i] = line[1]
    4. bd_opt[i] = 100 * 1e9
    5. 检查len(optimization_timestamp) == len(restore_timestamp)
    6. for i in range(len(optimization_timestamp)):
        opt_index = optimization_timestamp[i] / 1e9
        restore_index = restore_timestamp[i] / 1e9
            for j in range(opt_index, restore_index + 1):
                if j == opt_index:
                    bd_opt[j] += (double) (400 * 1e9 - 100 * 1e9) ((j + 1) * 1e9 - opt_index * 1e9) / 1e9
                elif j == restore_index:
                    bd_opt[j] += (double) (400 * 1e9 - 100 * 1e9) (restore_timestamp[i] - j * 1e9) / 1e9
                else:
                    bd_opt[j] += (double) (400 * 1e9 - 100 * 1e9) ((j + 1) * 1e9 - j * 1e9) / 1e9
    7. 返回[throughput_curve_1s[i] / bd_opt[i]]

                

3. 总吞吐量曲线和实际带宽曲线重合图
    # 1. throughput_curve_1s如上获得、
    2. bd_opt如上获得

'''

import os
import statistics

scale_factor = 0.001


def get_total_throughput_curve(application_end_file, throughput_curve_folder):
    # 步骤1：获取应用结束时间
    application_end = 0
    with open(application_end_file, 'r') as f:
        for line in f:
            application_end = max(application_end, float(line.split()[0]))

    # 步骤2：处理吞吐量曲线文件
    os.chdir(throughput_curve_folder)
    throughput_curve = [0] * (int(application_end / 1e8) + 1)
    for filename in os.listdir():
        with open(filename, 'r') as f:
            for line in f:
                time, throughput = map(float, line.split())
                if time > application_end:
                    break
                throughput_curve[int(time / 1e8)] += throughput
    os.chdir('..')

    # 步骤3：计算每秒的吞吐量
    for i in range(len(throughput_curve) - 1, 0, -1):
        throughput_curve[i] -= throughput_curve[i - 1]
    throughput_curve_1s = [10 * statistics.mean(throughput_curve[i:i + 10]) for i in range(0, len(throughput_curve), 10)]

    # 步骤4：应用比例因子
    throughput_curve_1s = [throughput * 8 / scale_factor / 1e9 for throughput in throughput_curve_1s]

    return throughput_curve_1s


def get_real_bandwidth(status_change_timestamp, begin_timestamp, end_timestamp):
    result = 0.0
    last_timestamp = begin_timestamp
    for timestamp, is_opt in status_change_timestamp:
        if timestamp >= begin_timestamp and timestamp <= end_timestamp:
            if is_opt:
                result += 100 * (timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
            else:
                result += 400 * (timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
            last_timestamp = timestamp
        elif begin_timestamp < last_timestamp <= end_timestamp:
            if is_opt:
                result += 100 * (end_timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
            else:
                result += 400 * (end_timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
            last_timestamp = end_timestamp
    if last_timestamp <= end_timestamp:
        if is_opt:
            result += 400 * (end_timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
        else:
            result += 100 * (end_timestamp - last_timestamp) / (end_timestamp - begin_timestamp)
    return result


def get_bandwidth_utilization_curve(throughput_curve_1s, optimization_timestamp_file, restore_timestamp_file):
    # 不存在则创建optimization_timestamp.txt和restore_timestamp.txt
    if not os.path.exists(optimization_timestamp_file) or not os.path.exists(restore_timestamp_file):
        h = len(throughput_curve_1s)
        bd_opt = [100] * h
        bandwidth_utilization_curve = [throughput_curve_1s[i] / bd_opt[i] for i in range(h)]
        return bandwidth_utilization_curve, bd_opt

    # 步骤1：获取优化和恢复时间戳
    optimization_timestamp = []
    with open(optimization_timestamp_file, 'r') as f:
        for line in f:
            optimization_timestamp.append(float(line.split()[1]))

    restore_timestamp = []
    with open(restore_timestamp_file, 'r') as f:
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

    # 步骤2：初始化带宽利用率曲线
    h = len(throughput_curve_1s)
    bd_opt = [100] * h

    # 步骤4：计算带宽利用率曲线
    for i in range(h):
        bd_opt[i] = get_real_bandwidth(status_change_timestamp, i * 1e9, (i + 1) * 1e9)

    # 步骤5：计算每秒的带宽利用率
    bd_opt = [bd_opt[i] for i in range(h)]
    bandwidth_utilization_cfurve = [throughput_curve_1s[i] / bd_opt[i] for i in range(h)]

    return bandwidth_utilization_curve, bd_opt


def main():
    for folder in os.listdir('.'):
        if os.path.isdir(folder):
            os.chdir(folder)  # 进入当前文件夹
            total_throughput_curve = get_total_throughput_curve("application_end.txt", "throughput_curve_rx")
            bandwidth_utilization_curve, bd_opt = get_bandwidth_utilization_curve(total_throughput_curve, "optimize_timestamp.txt", "restore_timestamp.txt")

            with open("dynamic-data.txt", "w") as f:
                f.write("吞吐量曲线Gbps\n")
                for i in range(len(total_throughput_curve)):
                    f.write(f"{total_throughput_curve[i]}" + " ")
                f.write("\n")
                f.write("带宽利用率\n")
                for i in range(len(bandwidth_utilization_curve)):
                    f.write(f"{bandwidth_utilization_curve[i]}" + " ")
                f.write("\n")
                f.write("实际带宽Gbps\n")
                for i in range(len(bd_opt)):
                    f.write(f"{bd_opt[i]}" + " ")
                f.write("\n")

            os.chdir('..')  # 返回上一级目录


def collect():
    with open("dynamic-data.txt", "w") as file1:
        # 遍历当前路径下的所有文件夹
        for folder in os.listdir('.'):
            if os.path.isdir(folder):
                file1.write("Folder Name: " + folder + "\n")
                # 打开static-data.txt文件并将内容添加到file1
                static_data_path = os.path.join(folder, "dynamic-data.txt")
                if os.path.exists(static_data_path):
                    with open(static_data_path, "r") as static_file:
                        file1.write(static_file.read() + "\n")


if __name__ == "__main__":
    main()
    collect()
