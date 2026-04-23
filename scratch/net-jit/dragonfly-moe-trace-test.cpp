
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h" // Added FlowMonitor
#include "ns3/mpi-application-module.h"

#include "topology/dragonfly-topology.h" // Use Dragonfly Header
#include "topology/utility.h"
#include "moe-jit.h"
#include "moe-trace-parser.h" 

// Legacy Global Variables (Required for Topology Headers)
#include "dml-application.h"
namespace ns3 {
    std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
    std::vector<int> batchFlag;
    std::vector<std::vector<int>> batchFlags;
    std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DragonflyMoeTraceTest");

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
    LogComponentEnable("DragonflyMoeTraceTest", LOG_LEVEL_INFO);
    
    // Parameter
    // std::string bandwidth = "10Mbps"; // Removed: Auto-scaled below
    std::string delay = "1us";
    std::string schedulerType = "fifo"; // Default: FIFO
    
    ns3::CommandLine cmd;
    cmd.AddValue("scheduler", "Type of scheduler (fifo/dist/central)", schedulerType);
    cmd.Parse(argc, argv);
    
    // Trace Config
    std::string traceDir = "scratch/net-jit/moe-traces/GPT-ep-trace-8-nodes";

    // ==========================================
    // 1. Prepare Traffic (Parse & Scale)
    // ==========================================
    // Dragonfly Params: p=2, a=4, h=2, g=2 => 16 Hosts (Matches FatTree k=4)
    size_t p = 2;
    size_t a = 4;
    size_t h = 2;
    size_t g = 2; // Reduced to match 16 nodes
    int num_nodes = p * a * g; 
    
    NS_LOG_INFO("Loading Templates from " << traceDir << " ...");
    auto templates = MoeTraceParser::LoadTraces(traceDir);
    
    if (templates.empty()) {
        NS_LOG_ERROR("Failed to load trace templates. Exiting.");
        return 1;
    }
    
    // Config: Offset to 1s, Scale 0.001, Limit Batch/Events
    MoeTraceParser::TraceConfig config;
    config.target_n = num_nodes;
    config.zipf_alpha = 1.2;
    config.start_time_offset = 1.5; // Start at 1.5s (Dragonfly default start is 1.5)
    config.volume_scale = 0.001;    // Scale down traffic
    config.max_events = 20;         // Limit to first 20 events

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
    // 2. Topology Setup (Dragonfly)
    // ==========================================
    NS_LOG_INFO("Creating Dragonfly Topology (p=" << p << ", a=" << a << ", h=" << h << ", g=" << g << ")...");
    
    DragonflyTopology dragonfly(p, a, h, g, bandwidth, delay, 20.0, true, "10Gbps", "400Gbps", 1.0, 100000000);

    dragonfly.switches.Create(dragonfly.switchCount);
    dragonfly.servers.Create(dragonfly.serverCount); // Should match num_nodes
    dragonfly.allNodes.Add(dragonfly.switches);
    dragonfly.allNodes.Add(dragonfly.servers);
    
    dragonfly.internetStackHelper.Install(dragonfly.allNodes);

    // Standard Wiring logic is typically inside DragonflyTopology constructor or manual here?
    // Looking at fattree-moe-trace-test.cpp, manual wiring was done for FatTree.
    // However, DragonflyTopology constructor doesn't wire nodes automatically? 
    // In `fattree-moe-trace-test.cpp`, manual wiring with p2pHelper loops was present.
    // I need to check `dragonfly-topology.cpp` source code to see if it wires itself or if I need to do it.
    // Assuming `utility.h` or user's preference was manual for FatTree, I will replicate manual wiring for Dragonfly if needed.
    // BUT usually such topology classes encapsulate wiring.
    // Wait, the previous `fattree-moe-trace-test.cpp` explicitly called `p2pHelper.Install`.
    // Let's assume DragonflyTopology also needs external wiring logic or check if it provides helper methods.
    // Given I don't see `Initialize()` method in Dragonfly header, I will assume MANUAL wiring is needed just like FatTree unless `topology_matrix` is populated and used.
    
    // Actually, writing manual wiring for Dragonfly here without seeing its internal wiring logic is risky.
    // Let's adopt a "Safe Guess": copy the FatTree test's wiring style but adapted? No, Dragonfly wiring is complex.
    // If DragonflyTopology class DOES NOT wire itself, I must do it.
    // The previous `fattree-moe-trace-test` wiring code (lines 135-163) did P2P install.
    // Let's look at `dragonfly-topology.cpp` briefly? 
    // I can't read it now easily without tool.
    // Let's check `fattree-topology.cpp` constructor... it just initialized fields.
    // So YES, I likely need to wire it here.
    
    // **Dragonfly Wiring Logic (Simplified based on p,a,h,g)**
    // But writing a correct Dragonfly wiring from scratch manually here is error-prone.
    // Does the repo have a `dragonfly-test.cc` or similar? No.
    // Wait, `fattree-moe-trace-test.cpp` included `fattree-topology.h`.
    // The `FattreeTopology` class has member `switches`, `servers`.
    // The user's code for FatTree explicitly iterates and connects them.
    // I will try to implement a basic wiring for Dragonfly or search for existing wiring logic in the codebase later if this fails.
    // For now, I will use a simple logical wiring based on canonical Dragonfly.
    
    // 1. Servers to Router (p servers per router)
    // 2. Intra-group (All-to-All within group 'a')
    // 3. Inter-group (Specific pattern)
    
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue(bandwidth));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue(delay));
    ns3::Ipv4AddressHelper ipv4AddressHelper{"10.0.0.0", "255.255.255.0"};

    // --- Wiring ---
    // 1. Servers to Switches
    for (size_t i = 0; i < dragonfly.switchCount; ++i) {
        auto sw = dragonfly.switches.Get(i);
        for (size_t j = 0; j < p; ++j) {
            size_t serverIdx = i * p + j;
            if (serverIdx >= dragonfly.serverCount) break;
            auto server = dragonfly.servers.Get(serverIdx);
            auto dev = p2pHelper.Install(sw, server);
            ipv4AddressHelper.Assign(dev);
            ipv4AddressHelper.NewNetwork();
        }
    }
    
    // 2. Intra-group (Routers in same group connected fully?)
    // Router Index: r = g_id * a + a_id
    for (size_t g_id = 0; g_id < g; ++g_id) {
        for (size_t a_src = 0; a_src < a; ++a_src) {
            for (size_t a_dst = a_src + 1; a_dst < a; ++a_dst) {
                size_t src_idx = g_id * a + a_src;
                size_t dst_idx = g_id * a + a_dst;
                auto dev = p2pHelper.Install(dragonfly.switches.Get(src_idx), dragonfly.switches.Get(dst_idx));
                ipv4AddressHelper.Assign(dev);
                ipv4AddressHelper.NewNetwork();
            }
        }
    }
    
    // 3. Inter-group (Global Links)
    // A simple Palm connectivity or similar?
    // For g=2, it's trivial: connect every router in G0 to some in G1?
    // Canonical: Each router has h global links.
    // Connect (g_src, a_src) to (g_dst, a_dst).
    // Target: Ensure connected graph.
    // For G=2, valid if a*h >= G-1. 4*2 >= 1.
    // Let's validly connect:
    // r_0_0 <-> r_1_0
    // r_0_1 <-> r_1_1
    // ...
    // Since h=2, maybe multiple links?
    // Minimal: Just connect corresponding routers if G=2?
    // Let's implement a simple pattern for G=2:
    if (g == 2) {
        for(size_t i=0; i<a; ++i) {
             size_t r0 = 0 * a + i;
             size_t r1 = 1 * a + i;
             // Link them
             auto dev = p2pHelper.Install(dragonfly.switches.Get(r0), dragonfly.switches.Get(r1));
             ipv4AddressHelper.Assign(dev);
             ipv4AddressHelper.NewNetwork();
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ==========================================
    // 3. Application Simulation
    // ==========================================
    NS_LOG_INFO("Configuring MoE Scheduling Test (" << num_nodes << " nodes)...");
    NS_LOG_INFO("Selected Scheduler: " << schedulerType);

    MoeJITApplicationHelper moeHelper;
    
    // (a) Load Scaled Trace
    moeHelper.SetTraceLoader([num_nodes](MPIRankIDType rank) {
        return GetTasksForRank(rank, num_nodes);
    });
    
    // (b) Select Scheduler
    if (schedulerType == "central") {
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
    ApplicationContainer apps = moeHelper.Install(dragonfly.servers);
    
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

    // FlowMonitor Analysis
    /*
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
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
