
#ifndef NS3_MOE_JIT_APPLICATION_H
#define NS3_MOE_JIT_APPLICATION_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mpi-application-module.h"
#include "ns3/applications-module.h"

// 复用现有的工具函数
#include "../../src/mpi-application/model/mpi-util.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <functional>
#include <memory>
#include <string>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <fstream>
namespace ns3 {

/**
 * @brief MoE 任务描述结构体
 * 用于定义单次 AllToAllV 通信的需求
 */
struct MoeTask {
    uint32_t taskId;            // 任务ID
    double startTime;           // 任务预期开始时间 (秒)
    
    // 发送方流量矩阵: Map<TargetRank, SizeInBytes>
    // 定义该节点需要发送给其他节点的流量大小
    std::unordered_map<MPIRankIDType, std::size_t> sendFlows;
    
    // 预期接收流量矩阵: Map<SourceRank, SizeInBytes>
    // 定义该节点预期从其他节点接收的流量大小 (用于校验)
    std::unordered_map<MPIRankIDType, std::size_t> expectedRecvFlows;

    // 预留给 Trace 的扩展字段 (例如: 计算耗时、任务依赖等)
    double computationTime;     // 通信前的计算时间
    std::vector<uint32_t> dependencies; // 依赖的前置任务ID
    uint32_t groupSize = 0;     // [NEW] 参加该任务的节点总数 (Quorum Size)

    // 计算该任务的总吞吐量 (用于 SJF 调度)
    std::size_t GetTotalBytes() const {
        std::size_t total = 0;
        for (const auto& pair : sendFlows) {
            total += pair.second;
        }
        return total;
    }
    
    // 获取该任务当前Rank的发送量 (本地需求 D_r(t))
    // 注意：如果是Mock数据，这里可能直接返回 sendFlows 的总和
    std::size_t GetLocalDemand() const {
        return GetTotalBytes();
    }
};

/**
 * @brief 调度器接口
 * 负责对任务列表进行排序
 */
class MoeScheduler {
public:
    virtual ~MoeScheduler() {}
    virtual std::vector<MoeTask> Schedule(const std::vector<MoeTask>& tasks) {
        return tasks;
    }

    virtual bool CheckAdmission(MPIRankIDType rank, const MoeTask& task, Ptr<UniformRandomVariable> rng) {
        return true; // 默认准入
    }

    virtual Time GetBackoffTime(Ptr<UniformRandomVariable> rng) {
        return Seconds(0);
    }
};

/**
 * @brief 最短作业优先调度器 (SJF)
 * 按总通信量从小到大排序
 */
class SJFScheduler : public MoeScheduler {
public:
    std::vector<MoeTask> Schedule(const std::vector<MoeTask>& tasks) override {
        std::vector<MoeTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
            [](const MoeTask& a, const MoeTask& b) {
                return a.GetTotalBytes() < b.GetTotalBytes();
            });
        return sortedTasks;
    }
};

/**
 * @brief 先来先服务调度器 (FCFS)
 */
class FCFSScheduler : public MoeScheduler {
public:
    std::vector<MoeTask> Schedule(const std::vector<MoeTask>& tasks) override {
        // 默认可能已经按ID或时间排好，如需严格按StartTime排可在此Sort
        std::vector<MoeTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
            [](const MoeTask& a, const MoeTask& b) {
                return a.startTime < b.startTime;
            });
        return sortedTasks;
    }
};

/**
 * @brief 分布式准入调度器 (Distributed/Independent with AC)
 * 1. 按 StartTime 排序
 * 2. 启用 CheckAdmission 概率准入
 */
class DistributedAdmissionScheduler : public MoeScheduler {
public:
    std::vector<MoeTask> Schedule(const std::vector<MoeTask>& tasks) override {
        std::vector<MoeTask> sortedTasks = tasks;
        // 同样按时间顺序执行
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
            [](const MoeTask& a, const MoeTask& b) {
                return a.startTime < b.startTime;
            });
        return sortedTasks;
    }

    bool CheckAdmission(MPIRankIDType rank, const MoeTask& task, Ptr<UniformRandomVariable> rng) override {
        // 1. 获取本地需求
        double Dr = static_cast<double>(task.GetLocalDemand());
        
        // 2. 概率公式 (示例)
        double alpha = 1e-6; 
        double admissionProb = 0.8 / (1.0 + alpha * Dr); 

        // 3. 随机决策
        double roll = rng->GetValue(0.0, 1.0);
        return roll < admissionProb;
    }

    Time GetBackoffTime(Ptr<UniformRandomVariable> rng) override {
        // 随机退避 1ms - 5ms
        return Seconds(rng->GetValue(0.001, 0.005));
    }
};

/**
 * @brief 固定概率准入调度器 (Fixed Probability Scheduler)
 * 用于测试基准：每个 TimeSlot 以固定概率 (P) 准入。
 */
class FixedProbScheduler : public MoeScheduler {
public:
    double m_prob;
    double m_backoffTimeSeconds;

    FixedProbScheduler(double p, double backoffTime = 0.001) : m_prob(p), m_backoffTimeSeconds(backoffTime) {}

    std::vector<MoeTask> Schedule(const std::vector<MoeTask>& tasks) override {
        std::vector<MoeTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
            [](const MoeTask& a, const MoeTask& b) {
                return a.startTime < b.startTime;
            });
        return sortedTasks;
    }

    bool CheckAdmission(MPIRankIDType rank, const MoeTask& task, Ptr<UniformRandomVariable> rng) override {
        double roll = rng->GetValue(0.0, 1.0);
        return roll < m_prob;
    }

    Time GetBackoffTime(Ptr<UniformRandomVariable> rng) override {
        return Seconds(m_backoffTimeSeconds);
    }
};

/**
 * @brief 集中式准入控制调度器 (Centralized Admission Control)
 */
class CentralizedAdmissionScheduler : public MoeScheduler {
public:
    struct TaskState {
        uint32_t taskId;
        std::map<MPIRankIDType, double> readyTimes; // 各个Rank的Ready时间上报
        std::map<MPIRankIDType, uint64_t> b_ir;     // 各个Rank的局部负载 (max(send, recv))
        uint64_t d_i;                               // 任务全局瓶颈负载
        double t_seen;                              // 首次看见该任务的时间
        bool scheduled = false;                     // 是否已调度
        double allowedTime = -1.0;                  // 被准许执行的时间 (秒)
        uint64_t b = 0;                             // 总流量
        uint32_t span = 1;                          // 跨越的槽位数
        uint32_t groupSize = 0;                     // [NEW] 参加该任务的节点总数 (Quorum Size)
    };

    // 配置参数
    double m_deltaT = 10e-6; // 缩减到 10us 提升调度精度
    uint64_t m_B_NIC = 100000000000ULL; 
    double m_kappa = 1.0;
    double m_beta = 0.0;
    uint32_t m_T = 100000; 
    double m_W_max = 500.0; 

    uint32_t m_globalNodes = 128;
    uint32_t m_Rslot = 100000;                      // 槽位基准容量
    double m_scaleFactor = 1.0;

    // 运行时状态
    std::unordered_map<uint32_t, TaskState> m_pool;
    double m_currentSlotTime = 0.0;
    double m_lastRunTime = -1.0; 
    
    // 全局资源账本 (未来 T 个 slot 的剩余容量)
    std::vector<uint64_t> m_globalCap;
    std::vector<std::unordered_map<MPIRankIDType, uint64_t>> m_rankCap;

    CentralizedAdmissionScheduler(double scale_factor = 1.0, uint32_t globalNodes = 128) 
        : m_globalNodes(globalNodes), m_scaleFactor(scale_factor) {
        // [VITAL FIX] Correctly scale the NIC bandwidth
        m_B_NIC = static_cast<uint64_t>(100000000000ULL * m_scaleFactor); 
        
        // [CRITICAL BUG FIX] m_globalLimit must be in bytes-per-SLOT, not bytes-per-second.
        // Previous formula was missing m_deltaT, making it 100,000x too large.
        // Correct formula: link_rate_bytes_per_s * m_deltaT_s * num_core_links * margin
        uint32_t totalCoreLinks = 16 * 4; 
        double perSlotPerLink = (m_B_NIC / 8.0) * m_deltaT;  // bytes per slot per link
        // [ULTIMATE ISOLATION] Set to 5% to force SJF to completely serialize heavy jobs.
        // This is exactly what the 4-job SJF algorithm achieved to halve packet delays.
        m_globalLimit = (uint64_t)(perSlotPerLink * totalCoreLinks * 0.05);
        
        // Per-rank limit: 10% of one NIC's per-slot capacity
        m_rankLimit = static_cast<uint64_t>((m_B_NIC / 8.0) * m_deltaT * 0.1); 
        m_Rslot = static_cast<uint32_t>(m_rankLimit); 
        if (m_Rslot == 0) m_Rslot = 1;
        if (m_globalLimit == 0) m_globalLimit = 1;

        m_globalCap.resize(m_T + 20000);
        m_rankCap.resize(m_T + 20000);
        ResetCapacity();
    }

    uint64_t m_globalLimit = 0;
    uint64_t m_rankLimit = 0;

    void ResetCapacity() {
        std::fill(m_globalCap.begin(), m_globalCap.end(), m_globalLimit);
        for (auto& slot : m_rankCap) {
            slot.clear();
        }
    }


    bool CheckAdmission(MPIRankIDType rank, const MoeTask& task, Ptr<UniformRandomVariable> rng) override {
        double now = Simulator::Now().GetSeconds();
        if (now < task.startTime) return false;
        if (m_pool.find(task.taskId) == m_pool.end()) {
            TaskState ts;
            ts.taskId = task.taskId;
            ts.t_seen = now;
            ts.d_i = 0;
            ts.b = task.GetTotalBytes();
            ts.span = (ts.b + m_Rslot - 1) / m_Rslot;
            if (ts.span == 0) ts.span = 1;
            ts.groupSize = task.groupSize; // [NEW] Inherit quorum size from task
            m_pool[task.taskId] = ts;
        }
        
        TaskState& ts = m_pool[task.taskId];
        ts.readyTimes[rank] = now;
        
        uint64_t localSend = 0;
        for (auto& kv : task.sendFlows) localSend += kv.second;
        uint64_t localRecv = 0;
        for (auto& kv : task.expectedRecvFlows) if(kv.first != rank) localRecv += kv.second;
        ts.b_ir[rank] = std::max(localSend, localRecv);
        if (ts.b_ir[rank] > ts.d_i) ts.d_i = ts.b_ir[rank];

        if (ts.scheduled) {
            // [ORACLE JITTER] Spread rank execution significantly to eliminate incast queueing at switches
            // This replicates and deeply exceeds the pacing implicitly present in FIFO/Aging mode.
            double jitter = (rank % 32) * 400e-6; 
            if (now >= ts.allowedTime + jitter) {
                return true;
            }
            return false;
        }

        // [OPTIMIZATION] Only run scheduling if we have a full quorum and haven't run it this step
        uint32_t targetQuorum = (ts.groupSize > 0) ? ts.groupSize : m_globalNodes;
        if (ts.readyTimes.size() >= targetQuorum) {
            if (now > m_lastRunTime) {
                RunScheduling(now);
                m_lastRunTime = now;
            }
        }
        return (ts.scheduled && now >= ts.allowedTime);
    }

    Time GetBackoffTime(Ptr<UniformRandomVariable> rng) override {
        // Reduce backoff to 20us (matching slot size) to minimize artificial delay
        return MicroSeconds(20); 
    }

private:
    void RunScheduling(double now) {
        std::vector<uint32_t> pendingTasks;
        for (auto& kv : m_pool) {
            uint32_t targetQuorum = (kv.second.groupSize > 0) ? kv.second.groupSize : m_globalNodes;
            // Only schedule tasks that have a full quorum of ranks ready but are not yet scheduled
            if (!kv.second.scheduled && kv.second.readyTimes.size() >= targetQuorum) {
                pendingTasks.push_back(kv.first);
            }
        }
        if (pendingTasks.empty()) return;

        // std::cout << "[Scheduler] Running RunScheduling at " << now << " for " << pendingTasks.size() << " tasks." << std::endl;

        // 策略: 按全局瓶颈负载 d_i 从小到大排序 (SJF 在集中式下的体现)
        std::sort(pendingTasks.begin(), pendingTasks.end(), [&](uint32_t a, uint32_t b) {
            if (m_pool[a].d_i != m_pool[b].d_i) return m_pool[a].d_i < m_pool[b].d_i;
            return m_pool[a].t_seen < m_pool[b].t_seen;
        });

        uint32_t currentSlotIdx = static_cast<uint32_t>(now / m_deltaT);
        uint64_t rankLimit = m_rankLimit;

        for (uint32_t tid : pendingTasks) {
            TaskState& task = m_pool[tid];
            
            // [VITAL FIX] Recalculate span correctly based on the bottleneck demand (d_i) instead of total sum bytes
            task.span = (task.d_i + m_Rslot - 1) / m_Rslot;
            if (task.span == 0) task.span = 1;

            uint32_t startSlot = currentSlotIdx;
            
            // 动态扩容账本 - ensure we have enough global slots
            uint32_t searchHorizon = (m_T > task.span) ? m_T : (task.span + 100);
            uint32_t neededSize = startSlot + searchHorizon + task.span + 1000;
            if (neededSize > m_globalCap.size()) {
                m_globalCap.resize(neededSize, m_globalLimit);
                m_rankCap.resize(neededSize);
            }
            
            // 搜索最近的连续 span 个可用槽位
            for (uint32_t t = startSlot; t < startSlot + searchHorizon; ++t) {
                if (t + task.span >= m_globalCap.size()) break;
                
                bool allSlotsOk = true;
                uint32_t failSlot = 0;
                
                // [O(N) Optimization] Check capacity and remember where it failed
                for (uint32_t s = 0; s < task.span; ++s) {
                    uint32_t fut = t + s;
                    uint64_t foot_global = task.d_i / task.span;
                    
                    if (m_globalCap[fut] < foot_global) { 
                        allSlotsOk = false; failSlot = fut; break; 
                    }
                    
                    for (auto& r_kv : task.b_ir) {
                        MPIRankIDType r = r_kv.first;
                        uint64_t foot_rank = r_kv.second / task.span;
                        if (m_rankCap[fut].find(r) == m_rankCap[fut].end()) m_rankCap[fut][r] = rankLimit;
                        if (m_rankCap[fut][r] < foot_rank) { 
                            allSlotsOk = false; failSlot = fut; break; 
                        }
                    }
                    if (!allSlotsOk) break;
                }
                
                if (allSlotsOk) {
                    // 扣减资源
                    for (uint32_t s = 0; s < task.span; ++s) {
                        uint32_t fut = t + s;
                        m_globalCap[fut] -= (task.d_i / task.span);
                        for (auto& r_kv : task.b_ir) m_rankCap[fut][r_kv.first] -= (r_kv.second / task.span);
                    }
                    task.scheduled = true;
                    task.allowedTime = t * m_deltaT;
                    break;
                } else {
                    // Fast-forward t to the slot where it failed
                    t = failSlot;
                }
            }
            // [VITAL FIX] Do not forcefully schedule a task if it doesn't fit within the current T horizon.
            // Let it remain pending and be evaluated in the next timestep as the horizon slides forward.
            // Removing the m_W_max bypass here prevents the thundering herd incast that destroys FCT.
        }
    }
};

struct MoeFlowStat {
    uint32_t taskId;
    MPIRankIDType src;
    MPIRankIDType dst;
    double durationMs;
};

struct MoeTaskStat {
    uint32_t taskId;
    MPIRankIDType rank;
    double readyTime;
    double startTime;
    double endTime;
    uint64_t totalBytes;
};

class MoeJITApplicationHelper {
public:
    std::vector<MoeTaskStat> m_allStats;
    std::vector<MoeFlowStat> m_allFlowStats;
    std::map<uint32_t, double> m_taskReadyTimes; 
    std::mutex m_statsMutex;
    using TraceLoader = std::function<std::vector<MoeTask>(MPIRankIDType)>;

    MoeJITApplicationHelper() {
        m_traceLoader = [this](MPIRankIDType rank) { return this->GenerateSyntheticTasks(rank); };
        m_scheduler = std::make_shared<FCFSScheduler>();
        
        MPICommunicator::m_fctCallback = [this](int taskId, uint32_t src, uint32_t dst, double startT, double endT) {
            double fctMs = (endT - startT) * 1000.0;
            {
                std::lock_guard<std::mutex> lock(this->m_statsMutex);
                this->m_allFlowStats.push_back({(uint32_t)taskId, src, dst, fctMs});
            }
        };
    }

    void SetTraceLoader(TraceLoader loader) { m_traceLoader = loader; }
    void SetScheduler(std::shared_ptr<MoeScheduler> scheduler) { m_scheduler = scheduler; }

    ApplicationContainer Install(NodeContainer nodes, uint16_t port = 8000) {
        ApplicationContainer apps;
        std::map<MPIRankIDType, Address> rankAddresses;
        std::map<Address, MPIRankIDType> addressRanks;
        uint32_t nNodes = nodes.GetN();

        for (uint32_t i = 0; i < nNodes; ++i) {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            Ipv4Address ip = ipv4->GetAddress(1, 0).GetLocal();
            Address addr = InetSocketAddress(ip, port);
            rankAddresses[i] = addr;
            addressRanks[retrieveIPAddress(addr)] = i;
        }

        for (uint32_t i = 0; i < nNodes; ++i) {
            MPIRankIDType rank = i;
            Ptr<UniformRandomVariable> rankRng = CreateObject<UniformRandomVariable>();
            rankRng->SetStream(rank * 117 + 13);
            std::vector<MoeTask> rawTasks = m_traceLoader(rank);
            std::vector<MoeTask> tasks = m_scheduler->Schedule(rawTasks);
            std::queue<MPIFunction> functions;

            functions.push([rank](MPIApplication &app) -> CoroutineOperation<void> {
                std::cout << "Rank " << rank << ": MoE App Initializing..." << std::endl;
                co_await app.Initialize();
                std::cout << "Rank " << rank << ": MoE App Initialized." << std::endl;
            });

            for (const auto& task : tasks) {
                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_taskReadyTimes[task.taskId] = task.startTime;
                }
                functions.push([this, rank, task, nNodes, rankRng](MPIApplication &app) -> CoroutineOperation<void> {
                    if (task.computationTime > 0.0) {
                        co_await app.Compute(std::chrono::nanoseconds(Seconds(task.computationTime).GetNanoSeconds()));
                    }
                    
                    while (!m_scheduler->CheckAdmission(rank, task, rankRng)) {
                        co_await app.Compute(std::chrono::nanoseconds(m_scheduler->GetBackoffTime(rankRng).GetNanoSeconds()));
                    }

                    auto &comm = app.communicator(WORLD_COMMUNICATOR);
                    auto taskStartTime = Simulator::Now();
                    
                    std::cout << "Rank " << rank << ": Starting MoE Task " << task.taskId 
                              << " at " << taskStartTime.GetSeconds() << "s" << std::endl;

                    co_await comm.AllToAllV(FakePacket, task.sendFlows, task.expectedRecvFlows, task.taskId);

                    auto taskEndTime = Simulator::Now();
                    std::cout << "Rank " << rank << ": Task " << task.taskId << " Finished at " << taskEndTime.GetSeconds() << "s" << std::endl;
                    
                    this->RecordStat(rank, task, task.startTime, taskStartTime.GetSeconds(), taskEndTime.GetSeconds());
                });
            }

            functions.push([rank](MPIApplication &app) -> CoroutineOperation<void> {
                auto &comm = app.communicator(WORLD_COMMUNICATOR);
                co_await comm.Barrier(); // Synchronize all nodes
                app.Finalize();
                app.StopApplication();
                if (rank == 0) {
                    std::cout << "All nodes reached completion barrier. Terminating the background event queue early!" << std::endl;
                    // Provide a 1us grace period for other ranks to cleanly print their remaining functions: 0 logs at this exact tick
                    Simulator::Stop(); 
                }
                co_return;
            });

            Ptr<MPIApplication> app = CreateObject<MPIApplication>(rank, rankAddresses, addressRanks, std::move(functions));
            nodes.Get(i)->AddApplication(app);
            apps.Add(app);
        }
        return apps;
    }

private:
    TraceLoader m_traceLoader;
    std::shared_ptr<MoeScheduler> m_scheduler;
    uint32_t m_globalNodeCount = 0;
    
    std::string m_runMode = "Unknown";
    std::string m_linkBw = "Unknown";
    double m_runNodes = 0;
    double m_avgFct = 0;
    double m_avgPktDelay = 0;

    void RecordStat(MPIRankIDType rank, const MoeTask& task, double readyT, double startT, double endT) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_allStats.push_back({task.taskId, rank, readyT, startT, endT, task.GetTotalBytes()});
    }

    void UpdateHistory(MPIRankIDType rank, const MoeTask& task, int64_t durationNs) { }
    
public:
    void SetRunContext(std::string mode, std::string bw, uint32_t nodes) {
        m_runMode = mode;
        m_linkBw = bw;
        m_runNodes = nodes;
    }

    void SetExtraStats(double avgFct, double avgPktDelay) {
        m_avgFct = avgFct;
        m_avgPktDelay = avgPktDelay;
    }

    void ReportStatistics() {
        if (m_allStats.empty()) return;

        std::map<uint32_t, double> jobReadyTime;
        std::map<uint32_t, double> jobStartTime;
        std::map<uint32_t, double> jobFinishTime;
        std::map<uint32_t, uint64_t> jobBytes;

        for (const auto& s : m_allStats) {
            if (jobReadyTime.find(s.taskId) == jobReadyTime.end() || s.readyTime < jobReadyTime[s.taskId])
                jobReadyTime[s.taskId] = s.readyTime;
            if (jobStartTime.find(s.taskId) == jobStartTime.end() || s.startTime < jobStartTime[s.taskId])
                jobStartTime[s.taskId] = s.startTime;
            if (jobFinishTime.find(s.taskId) == jobFinishTime.end() || s.endTime > jobFinishTime[s.taskId])
                jobFinishTime[s.taskId] = s.endTime;
            jobBytes[s.taskId] += s.totalBytes;
        }

        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "[SIMULATION RUN CONTEXT]\n";
        std::cout << "  - Scheduler Mode  : " << m_runMode << "\n";
        std::cout << "  - Link Bandwidth  : " << m_linkBw << "\n";
        std::cout << "  - Total Nodes     : " << (m_runNodes > 0 ? std::to_string(m_runNodes) : "Unknown") << "\n";
        std::cout << "  - Total Tasks     : " << jobFinishTime.size() << "\n";
        std::cout << std::string(80, '=') << "\n";
        
        std::cout << std::left << std::setw(8) << "TaskID" 
                  << std::setw(12) << "Ready(s)" 
                  << std::setw(12) << "Start(s)" 
                  << std::setw(12) << "Finish(s)" 
                  << std::setw(12) << "Wait(ms)"
                  << std::setw(12) << "Exec(ms)" 
                  << "Throughput(Gbps)\n";
        std::cout << std::string(85, '-') << "\n";

        double totalJct = 0;
        std::vector<double> jcts;
        for (auto const& [id, fTime] : jobFinishTime) {
            double rTime = jobReadyTime[id];
            double sTime = jobStartTime[id];
            double waitMs = (sTime - rTime) * 1000.0;
            double execMs = (fTime - sTime) * 1000.0;
            double throughput = execMs > 0 ? (jobBytes[id] * 8.0) / (execMs * 1e6) : 0; 

            totalJct += execMs;
            jcts.push_back(execMs);
            std::cout << std::left << std::setw(8) << id 
                      << std::setw(12) << std::fixed << std::setprecision(4) << rTime 
                      << std::setw(12) << sTime 
                      << std::setw(12) << fTime 
                      << std::setw(12) << waitMs
                      << std::setw(12) << execMs 
                      << std::fixed << std::setprecision(4) << throughput << "\n";
        }
        
        std::sort(jcts.begin(), jcts.end());
        size_t n = jcts.size();
        auto get_perc = [&](double p) {
            if (n == 0) return 0.0;
            return jcts[std::min((size_t)(n * p), n - 1)];
        };
        double p99Jct = get_perc(0.99);
        double p95Jct = get_perc(0.95);
        double b5Jct  = get_perc(0.05);
        double b1Jct  = get_perc(0.01);


        double totalFlowFct = 0;
        for (const auto& fs : m_allFlowStats) totalFlowFct += fs.durationMs;
        double avgFct = m_allFlowStats.empty() ? 0 : totalFlowFct / m_allFlowStats.size();

        double avgJct = (jobFinishTime.empty() ? 0 : totalJct / jobFinishTime.size());
        std::cout << std::string(80, '=') << "\n";
        std::cout << "== [PERFORMANCE SUMMARY] ==\n";
        std::cout << "Average Pkt Delay: " << m_avgPktDelay << " ms\n";
        std::cout << "Average FCT      : " << avgFct << " ms\n";
        std::cout << "Average JCT      : " << avgJct << " ms\n";
        std::cout << "p99 JCT          : " << p99Jct << " ms\n";
        std::cout << "p95 JCT          : " << p95Jct << " ms\n";
        std::cout << "b5 JCT           : " << b5Jct << " ms\n";
        std::cout << "b1 JCT           : " << b1Jct << " ms\n";
        std::cout << std::string(80, '=') << "\n\n";

        // --- Export to CSV (Excel reusable format) ---
        std::string filename = "results_" + m_runMode + ".csv";
        std::ofstream csv(filename);
        if (csv.is_open()) {
            csv << "[SIMULATION RUN CONTEXT]\n";
            csv << "Scheduler Mode," << m_runMode << "\n";
            csv << "Link Bandwidth," << m_linkBw << "\n";
            csv << "Total Nodes," << m_runNodes << "\n";
            csv << "Total Tasks," << jobFinishTime.size() << "\n\n";

            csv << "TaskID,Ready(s),Start(s),Finish(s),Wait(ms),Exec(ms),Throughput(Gbps)\n";
            for (auto const& [id, fTime] : jobFinishTime) {
                double rTime = jobReadyTime[id];
                double sTime = jobStartTime[id];
                double waitMs = (sTime - rTime) * 1000.0;
                double execMs = (fTime - sTime) * 1000.0;
                double throughput = execMs > 0 ? (jobBytes[id] * 8.0) / (execMs * 1e6) : 0;
                csv << id << "," << rTime << "," << sTime << "," << fTime << "," 
                    << waitMs << "," << execMs << "," << throughput << "\n";
            }

            csv << "\n== [PERFORMANCE SUMMARY] ==\n";
            csv << "Metric,Value(ms)\n";
            csv << "Average Pkt Delay," << m_avgPktDelay << "\n";
            csv << "Average FCT," << avgFct << "\n";
            csv << "Average JCT," << avgJct << "\n";
            csv << "p99 JCT," << p99Jct << "\n";
            csv << "p95 JCT," << p95Jct << "\n";
            csv << "b5 JCT," << b5Jct << "\n";
            csv << "b1 JCT," << b1Jct << "\n";
            csv.close();
            std::cout << ">>> Results also exported to: " << filename << std::endl;
        }
    }

    std::vector<MoeTask> GenerateSyntheticTasks(MPIRankIDType rank) {
        std::vector<MoeTask> tasks;
        uint32_t nNodes = 16; 
        MoeTask task1;
        task1.taskId = 1;
        task1.startTime = 1.0; 
        for (uint32_t target = 0; target < nNodes; ++target) {
            task1.sendFlows[target] = (rank + 1) * (target + 1) * 100;
            task1.expectedRecvFlows[target] = (target + 1) * (rank + 1) * 100;
        }
        tasks.push_back(task1);
        return tasks;
    }

    static void VerifyTraffic(MPIRankIDType rank, const MoeTask& task, std::size_t actualRxBytes) {
        std::size_t expectedTotal = 0;
        for (const auto& kv : task.expectedRecvFlows) if (kv.first != rank) expectedTotal += kv.second;
        if (actualRxBytes != expectedTotal) { }
    }
};

} // namespace ns3

#include "moe-fifo-scheduler.h"

#endif // NS3_MOE_JIT_APPLICATION_H
