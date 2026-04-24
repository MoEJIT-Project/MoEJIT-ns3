'''
0-1线——(3张图 measure method * 2根线(通信开始/结束 + 优化开始/结束))：fattree bert 1ms
输出zero-one-curve下的原始文件:
1. staic_communication.txt
2. static_opt.txt
3. slide_communication.txt
4. slide_opt.txt
5. net-jit_communication.txt
6. net-jit_opt.txt
'''
import os

opt_delay = 1000000000
curve_time_gap = 0.001
names = ["static", "slide", "net-jit"]


def compute_communication_curve(communication_curve):
    if os.path.exists("batch_communication_start.txt"):
        with open("batch_communication_start.txt", 'r') as f:
            for line in f:
                if ((int)(line[1]) == 0):  # 只查看node 0
                    timestamp = float(line.split()[0])
                    timestamp_index = int(timestamp / (1e9 * curve_time_gap))
                    if (timestamp_index < len(communication_curve)):
                        communication_curve[timestamp_index] = 2
    if os.path.exists("batch_communication_end.txt"):
        with open("batch_communication_end.txt", 'r') as f:
            for line in f:
                if ((int)(line[1]) == 0):  # 只查看node 0
                    timestamp = float(line.split()[0])
                    timestamp_index = int(timestamp / (1e9 * curve_time_gap))
                    if (timestamp_index < len(communication_curve)):
                        communication_curve[timestamp_index] = 3
    is_communication = False
    for i in range(len(communication_curve)):
        if communication_curve[i] == 2:
            is_communication = True
            communication_curve[i] = 1
        elif is_communication and communication_curve[i] == 3:
            is_communication = False
            optimization_curve[i] = 1
        elif is_communication:
            communication_curve[i] = 1
    return communication_curve


def compute_optimization_curve(optimization_curve):
    if os.path.exists("optimize_timestamp.txt"):
        with open("optimize_timestamp.txt", 'r') as f:
            for line in f:
                timestamp = float(line.split()[1])
                timestamp_index = int(timestamp / (1e9 * curve_time_gap))
                if (timestamp_index < len(optimization_curve)):
                    optimization_curve[timestamp_index] = 2
    if os.path.exists("restore_timestamp.txt"):
        with open("restore_timestamp.txt", 'r') as f:
            for line in f:
                timestamp = float(line.split()[1])
                timestamp_index = int(timestamp / (1e9 * curve_time_gap))
                if (timestamp_index < len(optimization_curve)):
                    optimization_curve[timestamp_index] = 3
    is_optimization = False
    for i in range(len(optimization_curve)):
        if optimization_curve[i] == 2:
            is_optimization = True
            optimization_curve[i] = 1
        elif is_optimization and optimization_curve[i] == 3:
            is_optimization = False
            optimization_curve[i] = 1
        elif is_optimization:
            optimization_curve[i] = 1
    return optimization_curve


os.makedirs("./zero-one-curve", exist_ok=True)
# 遍历当前路径下的文件夹
for folder in os.listdir('.'):
    # 检查folder name
    if os.path.isdir(folder) and "opt_delay_" + str(opt_delay) in folder:
        os.chdir(folder)  # 进入当前文件夹
        # 查看运行时间范围
        with open("application_end.txt", 'r') as f:
            application_end = max(float(line.split()[0]) for line in f)
        application_end_index = int(application_end / (1e9 * curve_time_gap))
        communication_curve = [0] * (application_end_index + 1)
        optimization_curve = [0] * (application_end_index + 1)
        communication_curve = compute_communication_curve(communication_curve)
        optimization_curve = compute_optimization_curve(optimization_curve)
        for name in names:
            if name in folder:
                with open("../zero-one-curve/"+name + "_communication.txt", 'w') as f:
                    for i in range(len(communication_curve)):
                        f.write(str(communication_curve[i]) + '\n')
                with open("../zero-one-curve/"+name + "_opt.txt", 'w') as f:
                    for i in range(len(optimization_curve)):
                        f.write(str(optimization_curve[i]) + '\n')
                break
        os.chdir('..')  # 返回上一级目录
