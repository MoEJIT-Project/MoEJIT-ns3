
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mpi-application-module.h"

#include "topology/fattree-topology.h"
#include "topology/utility.h"
#include "moe-jit.h"

// DML/Varys Global Variables (if needed) per legacy code structures
#include "dml-application.h"
namespace ns3 {
    std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
    std::vector<int> batchFlag;
    std::vector<std::vector<int>> batchFlags;
    std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeMoeCentralizedTest");

/**
 * @brief Generates MoE-like sparse traffic with consistent task ordering
 */
std::vector<MoeTask> CreateMoETasks(MPIRankIDType rank) {
    std::vector<MoeTask> tasks;
    uint32_t nNodes = 16;
    uint32_t nTasks = 5; 
    
    uint32_t top_k = 2; 
    uint32_t dataSize = 50 * 1024; // 50KB to allow more tasks to fit in slots

    for (uint32_t taskId = 0; taskId < nTasks; ++taskId) {
        MoeTask t;
        t.taskId = taskId;
        
        // Base start time + jitter
        ns3::Ptr<ns3::UniformRandomVariable> jitterRng = ns3::CreateObject<ns3::UniformRandomVariable>();
        jitterRng->SetStream(rank * 200 + taskId);
        double jitter = jitterRng->GetValue(0.0, 0.05); 
        
        t.startTime = 1.0 + taskId * 0.2 + jitter; // Tighter schedule (every 200ms)

        for (uint32_t distinct_src = 0; distinct_src < nNodes; ++distinct_src) {
            for (uint32_t k = 0; k < top_k; ++k) {
                uint32_t target_k = (distinct_src + taskId + k + 1) % nNodes;
                if (target_k == distinct_src) target_k = (target_k + 1) % nNodes;

                if (rank == distinct_src) {
                    t.sendFlows[target_k] += dataSize;
                }
                if (rank == target_k) {
                    t.expectedRecvFlows[distinct_src] += dataSize;
                }
            }
        }
        tasks.push_back(t);
    }
    return tasks;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("FatTreeMoeCentralizedTest", LOG_LEVEL_INFO);
    
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
    // (Skipping inter-switch wiring details for brevity, assuming standard FatTree setup is fine or copied from previous tests)
    // Actually, wiring IS required for connectivity. Let's copy strictly from previous successful test.
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
    // MoE Centralized Scheduling Test
    // ==========================================
    NS_LOG_INFO("Configuring MoE Centralized Scheduler Test...");

    MoeJITApplicationHelper moeHelper;
    
    // 1. 设置自定义 trace loader
    moeHelper.SetTraceLoader(CreateMoETasks);
    
    // 2. 使用 CentralizedAdmissionScheduler
    auto centralizedScheduler = std::make_shared<CentralizedAdmissionScheduler>();
    // Optional: Configure parameters if needed
    // centralizedScheduler->m_deltaT = 200e-6; 
    
    moeHelper.SetScheduler(centralizedScheduler);
    
    NS_LOG_INFO("Installing Applications...");
    ApplicationContainer apps = moeHelper.Install(fattree.servers);
    
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(5.0));

    NS_LOG_INFO("Starting Simulation...");
    
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
