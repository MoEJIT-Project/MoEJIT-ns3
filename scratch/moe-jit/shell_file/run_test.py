'''
1. 获取所有参数
2. 清空txt文件
3. mkdir throughput_curve_rx和throughput_curve_tx
4. 清空throughput_curve_rx和throughput_curve_tx
5. 运行./net-jit + 参数
'''
import sys
import subprocess

if __name__ == "__main__":    
    parameters = ' '.join(map(str, sys.argv[1:]))
    # 写入文件run.sh
    f = open('run.sh', 'w')
    f.write('rm *.txt\n')
    f.write('mkdir throughput_curve_rx\n')
    f.write('mkdir throughput_curve_tx\n')
    f.write('rm throughput_curve_rx/*\n')
    f.write('rm throughput_curve_tx/*\n')
    f.write('nohup ./net-jit ' + parameters + ' > output.txt & \n')
    f.close()
    
    # 运行run.sh
    subprocess.run(["sh ./run.sh"], shell=True, capture_output=True)
    
    