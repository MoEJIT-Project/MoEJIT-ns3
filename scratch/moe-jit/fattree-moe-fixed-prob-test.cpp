
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h" // Added FlowMonitor
#include "ns3/mpi-application-module.h"

#include "topology/fattree-topology.h"
#include "topology/utility.h"
#include "moe-jit.h"
#include "moe-trace-parser.h" // New Parser Header

// Legacy Global Variables
#include "dml-application.h"
namespace ns3 {
    std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
    std::vector<int> batchFlag;
    std::vector<std::vector<int>> batchFlags;
    std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeMoeTraceTest");

// ==========================================
// Adapter Logic: Global Matrix -> Local Tasks
// ==========================================

// Global Cache for Scaled Events (to avoid re-generating per rank)
static std::vector<MoeGlobalEvent> g_scaled_events;

/**
 * @brief 从全局事件列表提取特定 Rank 的任务流
 */
std::vector<MoeTask> GetTasksForRank(MPIRankIDType rank, int num_nodes) {
    std::vector<MoeTask> rank_tasks;
    
    // Safety check
    if (rank >= (MPIRankIDType)num_nodes) return rank_tasks;

    for (const auto& gev : g_scaled_events) {
        MoeTask task;
        task.taskId = gev.global_id;
        task.startTime = gev.start_time_s;
        task.computationTime = gev.computation_delay;
        
        // 1. Fill Send Flows: My Rank -> Others
        // Matrix Row 'rank'
        for (int dst = 0; dst < num_nodes; ++dst) {
            size_t bytes = gev.traffic_matrix[rank][dst];
            if (bytes > 0) {
                task.sendFlows[dst] = bytes;
            }
        }
        
        // 2. Fill Expected Recv: Others -> My Rank
        // Matrix Column 'rank'
        for (int src = 0; src < num_nodes; ++src) {
            size_t bytes = gev.traffic_matrix[src][rank];
            if (bytes > 0) {
                task.expectedRecvFlows[src] = bytes;
            }
        }
        
        rank_tasks.push_back(task);
    }
    return rank_tasks;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("FatTreeMoeTraceTest", LOG_LEVEL_INFO);
    
    // Parameter
    std::size_t k = 4; // FatTree k=4 => 16 Hosts
    // std::string bandwidth = "10Mbps"; // Removed: Auto-scaled below
    std::string delay = "1us";
    
    // Trace Config
    std::string traceDir = "scratch/net-jit/moe-traces/GPT-ep-trace-8-nodes";

    // ==========================================
    // 1. Prepare Traffic (Parse & Scale)
    // ==========================================
    int num_nodes = 16; // Target Scale
    NS_LOG_INFO("Loading Templates from " << traceDir << " ...");
    auto templates = MoeTraceParser::LoadTraces(traceDir);
    
    if (templates.empty()) {
        NS_LOG_ERROR("Failed to load trace templates. Exiting.");
        return 1;
    }
    
    // User requested: Offset to 1s, Scale 0.001, Limit Batch/Events
    MoeTraceParser::TraceConfig config;
    config.target_n = num_nodes;
    config.zipf_alpha = 1.2;
    config.start_time_offset = 1.0; // Start at 1.0s
    config.volume_scale = 0.001;    // Scale down traffic
    config.max_events = 20;         // Limit to first 20 events for quick test

    NS_LOG_INFO("Scaling Traces from 3 to " << num_nodes << " nodes (Zipf alpha=" << config.zipf_alpha << ")...");
    NS_LOG_INFO("  -> Time Offset: " << config.start_time_offset << "s");
    NS_LOG_INFO("  -> Vol Scale: " << config.volume_scale);
    NS_LOG_INFO("  -> Max Events: " << config.max_events);

    g_scaled_events = MoeTraceParser::GenerateScaledEvents(templates, config);
    NS_LOG_INFO("Generated " << g_scaled_events.size() << " scaled global events.");

    // config.volume_scale is 0.001
    double baseBandwidthBps = 10000000000.0; // 10Gbps
    double scaledBandwidthBps = baseBandwidthBps * config.volume_scale;
    std::string bandwidth = std::to_string((uint64_t)scaledBandwidthBps) + "bps";

    NS_LOG_INFO("Auto-Scaling Link Bandwidth: " << bandwidth << " (Scale: " << config.volume_scale << ")");

    // ==========================================
    // 2. Topology Setup (FatTree)
    // ==========================================
    NS_LOG_INFO("Creating FatTree Topology (k=" << k << ")...");
    
    // Default Routing (No Hedera)
    FattreeTopology fattree(k, bandwidth, delay, 20.0, true, "10Gbps", "400Gbps", 1.0, 100000000);

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

    // 4. Application Simulation
    // ==========================================
    std::string schedulerType = "fixed"; // Default: Fixed Prob
    double prob = 1.0;                   // Default Probability
    
    ns3::CommandLine cmd;
    cmd.AddValue("scheduler", "Type of scheduler (fifo/dist/central/fixed)", schedulerType);
    cmd.AddValue("prob", "Admission Probability (0.0-1.0) for fixed scheduler", prob);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Configuring MoE Scheduling Test (" << num_nodes << " nodes)...");
    NS_LOG_INFO("Selected Scheduler: " << schedulerType);

    MoeJITApplicationHelper moeHelper;
    
    // (a) Load Scaled Trace
    moeHelper.SetTraceLoader([num_nodes](MPIRankIDType rank) {
        return GetTasksForRank(rank, num_nodes);
    });
    
    // (b) Select Scheduler
    // (b) Select Scheduler
    if (schedulerType == "fixed") {
        NS_LOG_INFO("Using Fixed Probability Scheduler (P=" << prob << ")");
        auto fixedScheduler = std::make_shared<FixedProbScheduler>(prob);
        moeHelper.SetScheduler(fixedScheduler);
    } else if (schedulerType == "central") {
        auto centralScheduler = std::make_shared<CentralizedAdmissionScheduler>();
        centralScheduler->m_B_NIC = (uint64_t)scaledBandwidthBps; // Auto-Scaled Bandwidth
        centralScheduler->ResetCapacity(); 
        moeHelper.SetScheduler(centralScheduler);
    } else if (schedulerType == "dist") {
        auto distScheduler = std::make_shared<DistributedAdmissionScheduler>();
        moeHelper.SetScheduler(distScheduler);
    } else {
        // FIFO / Baseline
        auto fifoScheduler = std::make_shared<FCFSScheduler>();
        moeHelper.SetScheduler(fifoScheduler);
    }
    
    NS_LOG_INFO("Installing Applications...");
    ApplicationContainer apps = moeHelper.Install(fattree.servers);
    
    // Run long enough for the traces
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(15.0));

    // Lightweight Packet Monitor
    SetupLightweightPacketMonitor();
    
    // FlowMonitor installation
    // (Disabled to avoid hang, using Lightweight Monitor instead)
    /*
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    */
    NS_LOG_INFO("Starting Simulation...");
    
    Simulator::Run();

    // FlowMonitor Analysis (Disabled)
    /*
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    ...
    */

    double avgDelayMs = GetAveragePacketDelayMs();
    if (avgDelayMs > 0) {
        std::cout << "[FlowMonitor] AvgPacketDelay: " << avgDelayMs << " ms" << std::endl;
    } else {
        std::cout << "[FlowMonitor] AvgPacketDelay: 0.0 ms" << std::endl;
    }

    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
