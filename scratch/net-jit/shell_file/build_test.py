'''
设置好超参

1. 在fattree文件夹下里运行该文件
2. 创建不同参数的文件夹
3. 把parse-results.py net-jit run_test.py放在该文件夹下
4. 生成sh命令来:创建不同参数的文件夹
5. 生成sh命令来:并行启动不同参数文件夹下的run_test.py
'''

# topology = "dragonfly"
topology = "fattree"
if topology == "fattree":   
    k = [4]
else:
    k = [2]
general_trace = "../../../scratch/net-jit/traces/"
model_name = ["resnet152", "gpt1", "gpt2", "bert"]
measure_method = ["net-jit", "static_window", "slide_window", "none", "ideal"]
general_measure_traceFilePath = "../../../scratch/net-jit/net-jit-traces/"
measure_traceFilename = "1.txt"
static_window_advance_time = [100000000, 500000000, 1000000000] #0.1s，0.5s，1s
optimization_delay = [100000000] #0.1s, 1s !!优化时延要小于等于提前粒度
bandwidth_opt = ["400Gbps"]
scale_factor = [0.001]

def write_shell_file(file, folder_name, run_test_parameter):
    file.write("mkdir " + folder_name + "\n")
    file.write("cd " + folder_name + "\n")
    file.write("rm -rf * \n")
    file.write("cp ../parse-results.py . \n")
    file.write("cp ../run_test.py . \n")
    file.write("cp ../net-jit . \n")
    file.write("python3 run_test.py " + run_test_parameter + " & \n")
    file.write("cd .. \n")
    file.write("\n")

for scale_factor_i in scale_factor:
    # 对不同的scale_factor和topology新建一个shell文件
    f = open(topology + '_scale_factor_' + str(scale_factor_i) + '.sh', 'w')
    folder_name = ""
    run_test_parameter = ""
    for k_i in k:
        for model_name_i in model_name:
            for measure_method_i in measure_method:
                for bandwidth_opt_i in bandwidth_opt:
                    for optimization_delay_i in optimization_delay:
                        if measure_method_i == "static_window":
                            # 非侵入式测量
                            for static_window_advance_time_i in static_window_advance_time:
                                if static_window_advance_time_i >= optimization_delay_i:
                                    tracePath = general_trace + str(model_name_i)
                                    measure_traceFilePath = general_measure_traceFilePath + str(model_name_i) + "/" + measure_traceFilename
                                    folder_name = str(topology) + '_scale_factor_'  + str(scale_factor_i) + '_k_' + str(k_i) + '_model_' + str(model_name_i) + '_measure_' + str(measure_method_i) + '_bd_opt_' + str(bandwidth_opt_i) + '_opt_delay_' + str(optimization_delay_i) + '_static_window_advance_time_' + str(static_window_advance_time_i)
                                    run_test_parameter = str(tracePath) + " --scale_factor=" + str(scale_factor_i) + " --k="+ str(k_i) + ' --measure_traceFilePath=' + str(measure_traceFilePath) + ' --measure_method=' + str(measure_method_i) + ' --bandwidth_opt=' + str(bandwidth_opt_i) + ' --optimization_delay=' + str(optimization_delay_i) + ' --static_window_advance_time=' + str(static_window_advance_time_i)
                                    write_shell_file(f, folder_name, run_test_parameter)
                                else:
                                    pass
                        else: # 不是非侵入式测量
                            tracePath = general_trace + str(model_name_i)
                            measure_traceFilePath = general_measure_traceFilePath + str(model_name_i) + "/" + measure_traceFilename
                            folder_name = str(topology) + '_scale_factor_'  + str(scale_factor_i) + '_k_' + str(k_i) + '_model_' + str(model_name_i) + '_measure_' + str(measure_method_i) + '_bd_opt_' + str(bandwidth_opt_i) + '_opt_delay_' + str(optimization_delay_i)
                            run_test_parameter = str(tracePath) + " --scale_factor=" + str(scale_factor_i) + " --k="+ str(k_i) + ' --measure_traceFilePath=' + str(measure_traceFilePath) + ' --measure_method=' + str(measure_method_i) + ' --bandwidth_opt=' + str(bandwidth_opt_i) + ' --optimization_delay=' + str(optimization_delay_i)
                            write_shell_file(f, folder_name, run_test_parameter)
                        if measure_method_i == "none":
                            break
    f.close()