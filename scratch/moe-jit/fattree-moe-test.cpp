
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mpi-application-module.h"

#include "topology/fattree-topology.h"
#include "topology/utility.h" // for ContainerPattern, etc.
#include "moe-jit.h"          // 刚刚创建的头文件

// DML/Varys 全局变量定义 (链接所需)
#include "dml-application.h"
namespace ns3 {
    std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
    std::vector<int> batchFlag;
    std::vector<std::vector<int>> batchFlags;
    std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeMoeAllToAllVTest");

int main(int argc, char *argv[])
{
    LogComponentEnable("FatTreeMoeAllToAllVTest", LOG_LEVEL_INFO);
    // MoE Helper 目前没有定义 LOG_COMPONENT，通过标准输出查看
    
    // 参数
    std::size_t k = 4;
    std::string bandwidth = "10Gbps";
    std::string delay = "1us";
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("k", "FatTree parameter k", k);
    cmd.Parse(argc, argv);

    // ==========================================
    // 拓扑构建 (Standard FatTree Setup)
    // ==========================================
    NS_LOG_INFO("Creating FatTree Topology (k=" << k << ")...");
    
    // 省略掉复杂的 Hedera 设置，直接用默认构造
    ns3::FatTreeTopology topology_hedera;
    std::mt19937 mt(time(nullptr));
    auto hederaRoutingHelper = HederaRoutingHelper(topology_hedera, mt, 1000, 10000, false);

    FattreeTopology fattree(k, bandwidth, delay, 10.0, true, "10Gbps", "400Gbps", 1.0, 100000000, hederaRoutingHelper);

    // 节点创建
    fattree.core_switches.Create(fattree.number_of_core_switches);
    fattree.aggre_switches.Create(fattree.number_of_aggre_switches);
    fattree.edge_switches.Create(fattree.number_of_edge_switches);
    fattree.servers.Create(fattree.number_of_servers);

    fattree.switches.Add(fattree.core_switches);
    fattree.switches.Add(fattree.aggre_switches);
    fattree.switches.Add(fattree.edge_switches);
    fattree.allNodes.Add(fattree.switches);
    fattree.allNodes.Add(fattree.servers);

    // 协议栈
    fattree.internetStackHelper.Install(fattree.allNodes);

    // 物理连接
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue(bandwidth));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue(delay));
    ns3::Ipv4AddressHelper ipv4AddressHelper{"10.0.0.0", "255.255.255.0"};

    // 1. Edge - Server
    // 我们的 MoeJITApplicationHelper 需要服务器有一个 Ipv4 接口
    for (SwitchID i = 0; i < fattree.number_of_edge_switches; ++i) {
        auto tor = fattree.edge_switches.Get(i);
        for (ServerID j = 0; j < fattree.number_of_servers_per_edge_switch; ++j) {
            auto server = fattree.servers.Get(i * fattree.number_of_servers_per_edge_switch + j);
            auto dev = p2pHelper.Install(tor, server);
            ipv4AddressHelper.Assign(dev);
            ipv4AddressHelper.NewNetwork();
        }
    }

    // 2. Pod内 Edge - Aggregation
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

    // 3. Core - Aggregation
    for (SwitchID core = 0; core < fattree.number_of_core_switches; ++core) {
        auto coreNode = fattree.core_switches.Get(core);
        for (GroupID pod = 0; pod < fattree.number_of_pod; ++pod) {
            auto aggNode = fattree.aggre_switches.Get(pod * fattree.number_of_aggre_switches_per_pod + (core / (k / 2)));
            auto dev = p2pHelper.Install(coreNode, aggNode);
            ipv4AddressHelper.Assign(dev);
            ipv4AddressHelper.NewNetwork();
        }
    }

    NS_LOG_INFO("Populating Routing Tables...");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ==========================================
    // MoE 应用安装
    // ==========================================
    NS_LOG_INFO("Installing MoE Applications using MoeJITApplicationHelper...");

    MoeJITApplicationHelper moeHelper;
    
    // 你可以在这里 SetTraceLoader 来自定义流量
    // 目前使用默认的合成流量 (i+1)*(j+1)*100
    
    ApplicationContainer apps = moeHelper.Install(fattree.servers);
    
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // ==========================================
    // 运行仿真
    // ==========================================
    NS_LOG_INFO("Starting Simulation...");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
