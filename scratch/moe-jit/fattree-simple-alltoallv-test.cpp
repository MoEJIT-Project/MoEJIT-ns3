
#include "varys.h"

// RetrieveIPAddress helper
#include "../../src/mpi-application/model/mpi-util.h" 

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeSimpleAllToAllVTest");

// ==========================================
// 全局变量定义 (链接 DML/Varys 相关库所需)
// ==========================================
namespace ns3 {
    std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
    std::vector<int> batchFlag;
    std::vector<std::vector<int>> batchFlags;
    std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

int main(int argc, char** argv)
{
    // 日志设置
    LogComponentEnable("FatTreeSimpleAllToAllVTest", LOG_LEVEL_INFO);

    // 参数设置
    std::size_t k = 4;
    std::string bandwidth = "10Gbps";
    std::string delay = "1us";
    std::uint16_t appPort = 8000;
    
    // 兼容 FattreeTopology 构造函数的其他参数 (虽然本测试可能用不到高级特性)
    float stop_time = 10.0;
    bool ecmp = true;
    std::string app_bd = "10Gbps";
    std::string bandwidth_opt = "400Gbps"; // 不重要
    double scale_factor = 1.0;
    int64_t optimization_delay = 100000000;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("k", "FatTree parameter k", k);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating FatTree Topology with k=" << k);

    // 计算服务器数量: (k^3) / 4
    uint32_t nServers = (k * k * k) / 4;
    NS_LOG_INFO("Number of servers: " << nServers);

    // ==========================================
    // 拓扑构建 (参考 fattree-test-with-all2allv.cpp)
    // ==========================================
    
    // 初始化 Hedera 路由辅助 (必选参数但本测试不用)
    ns3::FatTreeTopology topology_hedera;
    std::mt19937 mt(time(nullptr));
    auto hederaRoutingHelper = HederaRoutingHelper(topology_hedera, mt, 1000, 10000, false);

    FattreeTopology fattree(k,
                            bandwidth,
                            delay,
                            stop_time,
                            ecmp,
                            app_bd,
                            bandwidth_opt,
                            scale_factor,
                            optimization_delay,
                            hederaRoutingHelper);

    // 创建节点
    fattree.core_switches.Create(fattree.number_of_core_switches);
    fattree.aggre_switches.Create(fattree.number_of_aggre_switches);
    fattree.edge_switches.Create(fattree.number_of_edge_switches);
    fattree.servers.Create(fattree.number_of_servers);

    fattree.switches.Add(fattree.core_switches);
    fattree.switches.Add(fattree.aggre_switches);
    fattree.switches.Add(fattree.edge_switches);
    fattree.allNodes.Add(fattree.switches);
    fattree.allNodes.Add(fattree.servers);

    // 安装协议栈
    fattree.internetStackHelper.Install(fattree.allNodes);

    // 配置链路
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue(bandwidth));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue(delay));

    // IP 地址分配
    ns3::Ipv4AddressHelper ipv4AddressHelper{"10.0.0.0", "255.255.255.0"};

    // 1. Server - ToR (Edge Switch) 连接
    for (SwitchID switch_index = 0; switch_index < fattree.number_of_edge_switches; ++switch_index)
    {
        auto tor = fattree.edge_switches.Get(switch_index);
        for (ServerID server_index = 0; server_index < fattree.number_of_servers_per_edge_switch; ++server_index)
        {
            auto server = fattree.servers.Get(switch_index * fattree.number_of_servers_per_edge_switch + server_index);
            auto devices = p2pHelper.Install(tor, server);
            
            // 分配 IP
            auto addresses = ipv4AddressHelper.Assign(devices);
            // addresses.GetAddress(0) 是 ToR 端口 IP, GetAddress(1) 是 Server 端口 IP
            auto serverIP = addresses.GetAddress(1); 
            
            // 记录关键信息供 MPI 使用
            // 注意: 这里并没有像原代码那样详细记录 backgroundLinkMap，因为简单测试只需要 Server 的 IP
            fattree.serverMap[server->GetId()] = {devices.Get(1), serverIP};

            ipv4AddressHelper.NewNetwork();
        }
    }

    // 2. Pod 内连接 (Edge - Aggregation)
    for (GroupID group_index = 0; group_index < fattree.number_of_pod; ++group_index) {
        for (SwitchID edge_idx = 0; edge_idx < fattree.number_of_edge_switches_per_pod; ++edge_idx) {
            auto edge = fattree.edge_switches.Get(group_index * fattree.number_of_edge_switches_per_pod + edge_idx);
            for (SwitchID agg_idx = 0; agg_idx < fattree.number_of_aggre_switches_per_pod; ++agg_idx) {
                auto agg = fattree.aggre_switches.Get(group_index * fattree.number_of_aggre_switches_per_pod + agg_idx);
                auto devices = p2pHelper.Install(edge, agg);
                ipv4AddressHelper.Assign(devices);
                ipv4AddressHelper.NewNetwork();
            }
        }
    }

    // 3. Core - Aggregation 连接
    for (SwitchID core_idx = 0; core_idx < fattree.number_of_core_switches; ++core_idx) {
        auto core = fattree.core_switches.Get(core_idx);
        for (GroupID pod_idx = 0; pod_idx < fattree.number_of_pod; ++pod_idx) {
            auto agg_idx_in_pod = core_idx / (k/2); // Core 分组规则
            auto agg = fattree.aggre_switches.Get(pod_idx * fattree.number_of_aggre_switches_per_pod + agg_idx_in_pod);
            auto devices = p2pHelper.Install(core, agg);
            ipv4AddressHelper.Assign(devices);
            ipv4AddressHelper.NewNetwork();
        }
    }

    // 路由表生成
    NS_LOG_INFO("Populating Routing Tables...");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ==========================================
    // 准备 MPI 应用所需的地址映射
    // ==========================================
    std::map<MPIRankIDType, Address> rankAddresses;
    std::map<Address, MPIRankIDType> addressRanks;

    // 假设 Rank ID 顺序就是 servers 容器中的顺序
    for (uint32_t i = 0; i < nServers; ++i) {
        Ptr<Node> serverNode = fattree.servers.Get(i);
        // 从 serverMap 中获取之前存储的 IP
        Ipv4Address ip = fattree.serverMap[serverNode->GetId()].address;
        Address addr = InetSocketAddress(ip, appPort);
        
        MPIRankIDType rank = i;
        rankAddresses[rank] = addr;
        addressRanks[retrieveIPAddress(addr)] = rank;
    }

    // ==========================================
    // 安装并运行 AllToAllV 测试应用
    // ==========================================
    NS_LOG_INFO("Installing MPI Applications...");

    for (uint32_t i = 0; i < nServers; ++i)
    {
        MPIRankIDType rank = i;
        std::queue<std::function<CoroutineOperation<void>(MPIApplication &)>> functions;

        // --- 任务 1: 初始化 ---
        functions.push([rank](MPIApplication &app) -> CoroutineOperation<void> {
            NS_LOG_INFO("Rank " << rank << ": Initializing...");
            co_await app.Initialize();
            NS_LOG_INFO("Rank " << rank << ": Initialized.");
        });

        // --- 任务 2: AllToAllV 测试 ---
        functions.push([rank, nServers](MPIApplication &app) -> CoroutineOperation<void> {
            NS_LOG_INFO("Rank " << rank << ": Starting AllToAllV...");
            auto &comm = app.communicator(WORLD_COMMUNICATOR);

            // 构造变长流量
            // 规则: Rank i -> Rank j 发送量 = (i+1)*(j+1)*100 Bytes
            std::unordered_map<MPIRankIDType, std::size_t> sendSizes;
            std::unordered_map<MPIRankIDType, std::size_t> recvSizes;

            for (uint32_t target = 0; target < nServers; ++target) {
                sendSizes[target] = (rank + 1) * (target + 1) * 100;
                recvSizes[target] = (target + 1) * (rank + 1) * 100;
            }

            auto start = Simulator::Now();
            auto initialRxBytes = comm.RxBytes();

            // 执行通信
            co_await comm.AllToAllV(FakePacket, sendSizes, recvSizes);

            auto end = Simulator::Now();
            auto finalRxBytes = comm.RxBytes();
            auto receivedBytes = finalRxBytes - initialRxBytes;
            auto duration = (end - start).GetNanoSeconds();

            NS_LOG_INFO("Rank " << rank << ": AllToAllV Finished! Duration: " << duration << " ns. Received: " << receivedBytes << " bytes");

            // --- 验证 ---
            size_t expectedRxBytes = 0;
            for (uint32_t src = 0; src < nServers; ++src) {
                if (src != rank) { // 剔除 Loopback
                     expectedRxBytes += (src + 1) * (rank + 1) * 100;
                }
            }

            if (receivedBytes == expectedRxBytes) {
                 NS_LOG_INFO("Rank " << rank << ": PASS");
            } else {
                 NS_LOG_ERROR("Rank " << rank << ": FAIL - Expected " << expectedRxBytes << ", got " << receivedBytes);
            }
        });

        // --- 任务 3: 结束 ---
        functions.push([rank](MPIApplication &app) -> CoroutineOperation<void> {
            app.Finalize();
            co_return;
        });

        Ptr<MPIApplication> app = CreateObject<MPIApplication>(rank, rankAddresses, addressRanks, std::move(functions));
        fattree.servers.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(stop_time));
    }

    NS_LOG_INFO("Starting Simulation...");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation Done.");

    return 0;
}
