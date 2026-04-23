# NS-3 Net-JIT 项目指令速查表

本文档汇总了构建、运行和测试 Net-JIT MoE 仿真项目的常用指令。

## 1. 编译 (Build)

### 编译所有目标
```bash
cmake --build build -j 4
```

### 编译特定测试
```bash
# FatTree 测试
cmake --build build --target fattree-moe-multijob-test -j 4

# Dragonfly 测试
cmake --build build --target dragonfly-moe-trace-test -j 4
```

## 2. 运行仿真 (Run Simulations)

### 手动运行 (FatTree)
```bash
# 使用分布式调度器 (默认)
./build/fattree-moe-trace-test --scheduler=dist

# 使用集中式调度器
./build/fattree-moe-trace-test --scheduler=central
```

### 手动运行 (Dragonfly)
```bash
./build/dragonfly-moe-trace-test --scheduler=dist
./build/dragonfly-moe-trace-test --scheduler=central
```

## 3. 自动化基准测试 (Automated Benchmark Suite)

我们提供了一个脚本，可自动运行所有 4 种组合（2 种拓扑 x 2 种调度器）并生成性能报告。

```bash
# 添加执行权限 (仅需一次)
chmod +x scratch/net-jit/run_benchmarks.sh

# 运行基准测试
./scratch/net-jit/run_benchmarks.sh
```

**脚本功能说明：**
1.  创建 `scratch/net-jit/logs/` 目录。
2.  分别运行 FatTree 和 Dragonfly 仿真（含分布式和集中式调度）。
3.  将日志保存到 `logs/` 目录（会自动覆盖旧日志）。
4.  运行 `analyze_results.py` 解析日志并打印 Markdown 格式的汇总表格。

## 4. 其他工具

### 手动解析/分析结果
如果你已经有了现成的日志文件（在 `scratch/net-jit/logs/` 下）：
```bash
python3 scratch/net-jit/analyze_results.py
```

### 清理日志
```bash
rm -rf scratch/net-jit/logs/*
```

###运行固定概率策略
```bash

cmake -B build -S . && cmake --build build --target fattree-moe-fixed-prob-test -j 4 && ./scratch/net-jit/run_fixed_prob_test.sh
```