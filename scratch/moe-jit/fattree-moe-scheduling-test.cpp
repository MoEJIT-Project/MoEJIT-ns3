
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mpi-application-module.h"

#include "topology/fattree-topology.h"
#include "topology/utility.h"
#include "moe-jit.h"

// DML/Varys Global Variables
#include "dml-application.h"
namespace ns3 {
    std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
    std::vector<int> batchFlag;
    std::vector<std::vector<int>> batchFlags;
    std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeMoeSchedulingTest");

// 自定义任务生成器：生成类 MoE 的稀疏 AllToAll 流量
// 特征：
// 1. 严格顺序：所有 Rank 必须按相同顺序执行 Task (Task 0 -> Task 1 -> ...)，否则集合通信会死锁。
// 2. 稀疏性：MoE 中每个 Token 只路由到 Top-K 个 Expert。
//    这意味着对于每个 Rank (看作一组 Token)，它只会向少数几个 Target Rank (Experts) 发送较大数据，
//    而不是向所有节点发送数据。
std::vector<MoeTask> CreateMixedTasks(MPIRankIDType rank) {
    std::vector<MoeTask> tasks;
    uint32_t nNodes = 16;
    uint32_t nTasks = 5; // 生成 5 轮 MoE 调度
    
    // MoE 参数
    uint32_t top_k = 2; // 每个节点只选 2 个目标节点发送
    uint32_t dataSize = 100 * 1024; // 每次发送 100KB

    // 使用确定性随机数，确保这一轮所有 Rank 对"谁发给谁"有共识，以便计算 expectedRecv
    // 注意：在真实 Trace 中，expectedRecv 由 Trace 提供。
    // 这里我们必须模拟一个全局知晓的路由表。
    
    for (uint32_t taskId = 0; taskId < nTasks; ++taskId) {
        MoeTask t;
        t.taskId = taskId;
        
        // 设定统一的基础基准时间，所有人在 1.0s, 2.0s... 附近开始
        // 加上微小的随机抖动 (Jitter) 来模拟分布式不同步
        // 注意：这个随机性是 per-rank 的，用于触发分布式准入控制
        ns3::Ptr<ns3::UniformRandomVariable> jitterRng = ns3::CreateObject<ns3::UniformRandomVariable>();
        jitterRng->SetStream(rank * 100 + taskId);
        double jitter = jitterRng->GetValue(0.0, 0.05); // 0-50ms 抖动
        
        t.startTime = 1.0 + taskId * 0.5 + jitter; // 每 0.5s 一轮

        // --- 构建流量矩阵 (全局视角) ---
        // 为了让 Rank r 知道自己要发给谁 (sendFlows)，也要知道谁会发给自己 (expectedRecvFlows)，
        // 我们需要遍历所有节点对 (u, v) 来模拟路由决策。
        
        for (uint32_t distinct_src = 0; distinct_src < nNodes; ++distinct_src) {
            // 模拟源节点 distinct_src 的路由决策
            // 使用确定性 Hash 算法选择 Top-K 目标
            for (uint32_t k = 0; k < top_k; ++k) {
                // 简单的伪随机选择目标: target = (src + task + k + 1) % nNodes
                // 保证非自环
                uint32_t target_k = (distinct_src + taskId + k + 1) % nNodes;
                if (target_k == distinct_src) target_k = (target_k + 1) % nNodes;

                // 如果我是源 (Sender)
                if (rank == distinct_src) {
                    t.sendFlows[target_k] += dataSize;
                }

                // 如果我是目标 (Receiver)
                if (rank == target_k) {
                    t.expectedRecvFlows[distinct_src] += dataSize;
                }
            }
        }
        
        tasks.push_back(t);
    }
    
    // 关键：绝对不要再对 tasks 进行排序！
    // 必须保持 Task ID 0, 1, 2... 的顺序提交给 Scheduler。
    // Scheduler 内部也最好不要改变 Task 之间的相对顺序，除非它是无依赖的。
    // 在我们的 DistributedAdmissionScheduler 实现中，我们按照 StartTime 排序，
    // 由于我们在上面设置了 startTime = 1.0 + taskId * 0.5，大体顺序已经由 TaskID 决定了。
    // 哪怕有 Jitter，Task 0 (1.0s) 也肯定早于 Task 1 (1.5s)。
    
    return tasks;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("FatTreeMoeSchedulingTest", LOG_LEVEL_INFO);
    
    std::size_t k = 4;
    std::string bandwidth = "10Gbps";
    std::string delay = "1us";
    
    // ==========================================
    // Topology Setup
    // ==========================================
    NS_LOG_INFO("Creating FatTree Topology (k=" << k << ")...");
    
    ns3::FatTreeTopology topology_hedera;
    std::mt19937 mt(time(nullptr));
    auto hederaRoutingHelper = HederaRoutingHelper(topology_hedera, mt, 1000, 10000, false);

    FattreeTopology fattree(k, bandwidth, delay, 20.0, true, "10Gbps", "400Gbps", 1.0, 100000000, hederaRoutingHelper);

    fattree.core_switches.Create(fattree.number_of_core_switches);
    fattree.aggre_switches.Create(fattree.number_of_aggre_switches);
    fattree.edge_switches.Create(fattree.number_of_edge_switches);
    fattree.servers.Create(fattree.number_of_servers);

    fattree.switches.Add(fattree.core_switches);
    fattree.switches.Add(fattree.aggre_switches);
    fattree.switches.Add(fattree.edge_switches);
    fattree.allNodes.Add(fattree.switches);
    fattree.allNodes.Add(fattree.servers);

    fattree.internetStackHelper.Install(fattree.allNodes);

    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue(bandwidth));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue(delay));
    ns3::Ipv4AddressHelper ipv4AddressHelper{"10.0.0.0", "255.255.255.0"};

    // Wiring
    for (SwitchID i = 0; i < fattree.number_of_edge_switches; ++i) {
        auto tor = fattree.edge_switches.Get(i);
        for (ServerID j = 0; j < fattree.number_of_servers_per_edge_switch; ++j) {
            auto server = fattree.servers.Get(i * fattree.number_of_servers_per_edge_switch + j);
            auto dev = p2pHelper.Install(tor, server);
            ipv4AddressHelper.Assign(dev);
            ipv4AddressHelper.NewNetwork();
        }
    }
    for (GroupID pod = 0; pod < fattree.number_of_pod; ++pod) {
        for (SwitchID edge = 0; edge < fattree.number_of_edge_switches_per_pod; ++edge) {
            auto edgeNode = fattree.edge_switches.Get(pod * fattree.number_of_edge_switches_per_pod + edge);
            for (SwitchID agg = 0; agg < fattree.number_of_aggre_switches_per_pod; ++agg) {
                auto aggNode = fattree.aggre_switches.Get(pod * fattree.number_of_aggre_switches_per_pod + agg);
                auto dev = p2pHelper.Install(edgeNode, aggNode);
                ipv4AddressHelper.Assign(dev);
                ipv4AddressHelper.NewNetwork();
            }
        }
    }
    for (SwitchID core = 0; core < fattree.number_of_core_switches; ++core) {
        auto coreNode = fattree.core_switches.Get(core);
        for (GroupID pod = 0; pod < fattree.number_of_pod; ++pod) {
            auto aggNode = fattree.aggre_switches.Get(pod * fattree.number_of_aggre_switches_per_pod + (core / (k / 2)));
            auto dev = p2pHelper.Install(coreNode, aggNode);
            ipv4AddressHelper.Assign(dev);
            ipv4AddressHelper.NewNetwork();
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ==========================================
    // MoE Scheduling Test
    // ==========================================
    NS_LOG_INFO("Configuring MoE Distributed Test with Virtual Traffic...");

    MoeJITApplicationHelper moeHelper;
    
    // 1. 设置自定义 trace loader，生成虚拟小流量
    moeHelper.SetTraceLoader(CreateMixedTasks);
    
    // 2. 使用 DistributedAdmissionScheduler
    // 启用分布式概率准入控制，并按 StartTime 排序执行
    moeHelper.SetScheduler(std::make_shared<DistributedAdmissionScheduler>());
    
    NS_LOG_INFO("Installing Applications...");
    ApplicationContainer apps = moeHelper.Install(fattree.servers);
    
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    NS_LOG_INFO("Starting Simulation...");
    
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
