#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "topology/spine-leaf-topology.h"
#include "moe-jit.h"
#include "moe-trace-parser.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpineLeafTraceTest");

// Global Cache for Scaled Events (to avoid re-generating per rank)
static std::vector<MoeGlobalEvent> g_scaled_events;

/**
 * @brief Adapt global scaled events into local MoeTask lists for a specific rank
 */
std::vector<MoeTask> GetTasksForRank(MPIRankIDType rank, uint32_t num_nodes) {
    std::vector<MoeTask> rank_tasks;
    
    if (rank >= (MPIRankIDType)num_nodes) return rank_tasks;

    for (const auto& gev : g_scaled_events) {
        MoeTask task;
        task.taskId = gev.global_id;
        task.startTime = gev.start_time_s; 
        task.computationTime = gev.computation_delay;
        
        // 1. Fill Send Flows: My Rank -> Others
        for (uint32_t dst = 0; dst < num_nodes; ++dst) {
            size_t bytes = gev.traffic_matrix[rank][dst];
            if (bytes > 0) {
                task.sendFlows[dst] = bytes;
            }
        }
        
        // 2. Fill Expected Recv: Others -> My Rank
        for (uint32_t src = 0; src < num_nodes; ++src) {
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
    LogComponentEnable("SpineLeafTraceTest", LOG_LEVEL_INFO);
    
    uint32_t k = 4;
    uint32_t subscription = 2;
    std::string bandwidth = "10Gbps";
    std::string delay = "1us";
    std::string traceDir = "scratch/net-jit/moe-traces/GPT-ep-trace-8-nodes";
    double volScale = 0.001; 
    int maxEvents = 10;
    
    CommandLine cmd;
    cmd.AddValue ("k", "Number of leaf switches and servers per leaf (total hosts = k*k)", k);
    cmd.AddValue ("subscription", "Number of spine switches", subscription);
    cmd.AddValue ("traceDir", "Directory containing .jsonl trace files", traceDir);
    cmd.AddValue ("volScale", "Scaling factor for traffic volume", volScale);
    cmd.AddValue ("maxEvents", "Maximum number of events to load", maxEvents);
    cmd.Parse (argc, argv);

    uint32_t num_nodes = k * k;

    // 1. Load Trace Templates
    NS_LOG_INFO("Loading Templates from " << traceDir << " ...");
    auto templates = MoeTraceParser::LoadTraces(traceDir);
    if (templates.empty()) {
        NS_LOG_ERROR("Failed to load trace templates. Exiting.");
        return 1;
    }

    // 2. Scale Events to Target Topology Size
    MoeTraceParser::TraceConfig config;
    config.target_n = num_nodes;
    config.zipf_alpha = 1.2;
    config.start_time_offset = 1.0; 
    config.volume_scale = volScale;
    config.max_events = maxEvents;

    NS_LOG_INFO("Scaling Traces for " << num_nodes << " nodes...");
    g_scaled_events = MoeTraceParser::GenerateScaledEvents(templates, config);
    NS_LOG_INFO("Generated " << g_scaled_events.size() << " global events.");

    // 3. Topology Setup
    NS_LOG_INFO("Creating Spine-Leaf Topology (k=" << k << ", sub=" << subscription << ")...");
    SpineLeafTopology topo(k, subscription, bandwidth, delay);

    // 4. Configure MoE Application & Scheduler
    NS_LOG_INFO("Configuring MoE JIT Application...");
    MoeJITApplicationHelper moeHelper;
    moeHelper.SetTraceLoader([num_nodes](MPIRankIDType rank) {
        return GetTasksForRank(rank, num_nodes);
    });

    // Use DistributedAgingScheduler
    auto scheduler = std::make_shared<DistributedAgingScheduler>(k, k); 
    moeHelper.SetScheduler(scheduler);

    // 5. Install & Run
    NS_LOG_INFO("Installing Applications on " << topo.GetTotalServers() << " servers...");
    ApplicationContainer apps = moeHelper.Install(topo.servers);
    
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(20.0));
    Simulator::Stop(Seconds(20.1));

    NS_LOG_INFO("Starting Simulation...");
    Simulator::Run();
    NS_LOG_INFO("Simulation Run phase complete.");
    Simulator::Destroy();
    NS_LOG_INFO("Full Simulation Finished and Destroyed.");
    return 0;
}
