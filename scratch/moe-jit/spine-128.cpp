#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "topology/spine-leaf-topology.h"
#include "moe-jit.h"
#include "moe-trace-parser.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpineLeafComparison");

// Global Caches for Scaled Events
static std::vector<MoeGlobalEvent> g_gpt_events;

// Global variables for nodes
static uint32_t g_num_nodes = 128; 
static uint32_t g_servers_per_leaf = 8;
static uint32_t g_leaf_count = 16;
static uint32_t g_active_leaf_count = 4; // 新增：活跃的Leaf数量

// Global Caches for Scaled Events (4 Apps)
static std::vector<MoeGlobalEvent> g_app1_events;
static std::vector<MoeGlobalEvent> g_app2_events;
static std::vector<MoeGlobalEvent> g_app3_events;
static std::vector<MoeGlobalEvent> g_app4_events;

void PrintTopologyAndApps(uint32_t leaf_count, uint32_t servers_per_leaf, uint32_t spine_count, uint32_t active_leaf_count) {
    std::cout << "\n================================================================================\n";
    std::cout << "[Spine-Leaf Topology Info]\n";
    std::cout << "  - Spines (Core)     : " << spine_count << "\n";
    std::cout << "  - Leaves (Edge)     : " << leaf_count << "\n";
    std::cout << "  - Servers per Leaf  : " << servers_per_leaf << "\n";
    std::cout << "  - Total Servers     : " << leaf_count * servers_per_leaf << "\n";
    std::cout << "  - Active Leaves     : " << active_leaf_count << " (Apps only installed here)\n";
    std::cout << "================================================================================\n";
    
    int nodes_per_app_per_leaf = servers_per_leaf / 4; 
    std::cout << "[Application Deployment Map]\n";
    
    for (uint32_t l = 0; l < active_leaf_count; ++l) {
        std::cout << "  - Leaf " << l << " connects to [ ";
        for (uint32_t s = 0; s < servers_per_leaf; ++s) {
            int app_id = s / nodes_per_app_per_leaf;
            uint32_t global_rank = l * servers_per_leaf + s;
            std::string app_name = "";
            if (app_id == 0) app_name = "App1(GPT_b10)";
            else if (app_id == 1) app_name = "App2(GPT_b90)";
            else if (app_id == 2) app_name = "App3(T5_b10)";
            else if (app_id == 3) app_name = "App4(BERT_b10)";
            else app_name = "Unknown";
            
            std::cout << "S" << s << ": R" << global_rank << "(" << app_name << ") ";
            if (s != servers_per_leaf - 1) std::cout << "| ";
        }
        std::cout << "]\n";
    }
    
    if (leaf_count > active_leaf_count) {
        std::cout << "  ... (Leaf " << active_leaf_count << " to " << leaf_count - 1 << " are IDLE / NO APPS)\n";
    }
    
    std::cout << "================================================================================\n\n";
}

std::vector<MoeTask> GetTasksForRank(MPIRankIDType rank) {
    std::vector<MoeTask> rank_tasks;
    
    // 【修改点】超过活跃节点范围的机器，返回空任务
    uint32_t active_nodes = g_active_leaf_count * g_servers_per_leaf;
    if (rank >= (MPIRankIDType)active_nodes) return rank_tasks;

    int local_index_in_leaf = rank % g_servers_per_leaf;
    int leaf_index = rank / g_servers_per_leaf;
    
    // Divide servers within a leaf into 4 equal segments for 4 apps
    int nodes_per_app_per_leaf = g_servers_per_leaf / 4; 
    int app_id = local_index_in_leaf / nodes_per_app_per_leaf; // 0, 1, 2, 3
    
    int logical_rank = leaf_index * nodes_per_app_per_leaf + (local_index_in_leaf % nodes_per_app_per_leaf);
    // 【修改点】逻辑节点总数基于 active_leaf_count 计算
    int logical_node_count = g_active_leaf_count * nodes_per_app_per_leaf;

    std::vector<MoeGlobalEvent>* current_events = nullptr;
    int task_id_offset = 0;

    if (app_id == 0) {
        current_events = &g_app1_events;      // App1: GPT b10
        task_id_offset = 0;
    } else if (app_id == 1) {
        current_events = &g_app2_events;      // App2: GPT b90
        task_id_offset = 100000;
    } else if (app_id == 2) {
        current_events = &g_app3_events;      // App3: T5 b10
        task_id_offset = 200000;
    } else if (app_id == 3) {
        current_events = &g_app4_events;      // App4: BERT b10
        task_id_offset = 300000;
    }

    if (!current_events) return rank_tasks;

    auto PhysicalRank = [nodes_per_app_per_leaf](int logical_id, int app) {
        int l_idx = logical_id / nodes_per_app_per_leaf;
        int local_idx = logical_id % nodes_per_app_per_leaf;
        return l_idx * g_servers_per_leaf + (app * nodes_per_app_per_leaf) + local_idx;
    };

    for (const auto& gev : *current_events) {
        MoeTask task;
        task.taskId = task_id_offset + gev.global_id;
        task.startTime = gev.start_time_s; 
        task.computationTime = gev.computation_delay;
        task.groupSize = logical_node_count; // [NEW] Set quorum size for the scheduler
        
        // 1. Fill Send Flows
        for (int logical_dst = 0; logical_dst < logical_node_count; ++logical_dst) {
            if (logical_rank >= (int)gev.traffic_matrix.size() || 
                logical_dst >= (int)gev.traffic_matrix[logical_rank].size()) continue;

            size_t bytes = gev.traffic_matrix[logical_rank][logical_dst] / 10;
            if (bytes > 0) {
                int physical_dst = PhysicalRank(logical_dst, app_id);
                task.sendFlows[physical_dst] = bytes;
            }
        }
        
        // 2. Fill Expected Recv
        for (int logical_src = 0; logical_src < logical_node_count; ++logical_src) {
            if (logical_src >= (int)gev.traffic_matrix.size() || 
                logical_rank >= (int)gev.traffic_matrix[logical_src].size()) continue;

            size_t bytes = gev.traffic_matrix[logical_src][logical_rank] / 10;
            if (bytes > 0) {
                int physical_src = PhysicalRank(logical_src, app_id);
                task.expectedRecvFlows[physical_src] = bytes;
            }
        }
        
        if (!task.sendFlows.empty() || !task.expectedRecvFlows.empty()) {
            rank_tasks.push_back(task);
        }
    }

    return rank_tasks;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("SpineLeafComparison", LOG_LEVEL_INFO);
    // 关闭包粒度的 ECMP 路由 (使用流级别的普通路由) - Packet level ECMP causes severe TCP out-of-order drop and timeouts
    Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue (false));
    

    // 原版的所有参数全保留
    uint32_t leaf_count = 16;
    uint32_t servers_per_leaf = 8;
    uint32_t spine_count = 4;
    std::string modeStr = "aging";
    bool print_topo = false;
    double scale_factor = 0.0001; 
    double app1_volume_factor = 1.0;
    double app2_volume_factor = 0.5;
    double app3_volume_factor = 1.0;
    double app4_volume_factor = 1.0;
    
    CommandLine cmd;
    cmd.AddValue ("leaves", "Number of leaf switches", leaf_count);
    cmd.AddValue ("servers_per_leaf", "Number of servers per leaf (must be mult of 4)", servers_per_leaf);
    cmd.AddValue ("spines", "Number of spine switches", spine_count);
    cmd.AddValue ("mode", "Scheduling mode: aging, fifo, sjf, srpt, ideal", modeStr);
    cmd.AddValue ("print_topo", "Print topology and application installation map", print_topo);
    cmd.AddValue ("scale_factor", "Proportionally scale down data volume and link bandwidth", scale_factor);
    cmd.AddValue ("app1_volume", "Volume factor for App 1 (GPT b10)", app1_volume_factor);
    cmd.AddValue ("app2_volume", "Volume factor for App 2 (GPT b90)", app2_volume_factor);
    cmd.AddValue ("app3_volume", "Volume factor for App 3 (T5 b10)", app3_volume_factor);
    cmd.AddValue ("app4_volume", "Volume factor for App 4 (BERT b10)", app4_volume_factor);
    cmd.Parse (argc, argv);

    g_num_nodes = leaf_count * servers_per_leaf;
    g_servers_per_leaf = servers_per_leaf;
    g_leaf_count = leaf_count;
    g_active_leaf_count = 4; // 只让前4个Leaf工作
    
    if (print_topo) {
        PrintTopologyAndApps(leaf_count, servers_per_leaf, spine_count, g_active_leaf_count);
    }
    
    // 【修改点】应用规模现在由前4个Leaf的活跃机器数决定
    uint32_t active_nodes = g_active_leaf_count * servers_per_leaf;
    int app_node_count = active_nodes / 4;

    NS_LOG_INFO("Loading Traces...");
    
    auto tpl_gpt = MoeTraceParser::LoadTraces("scratch/net-jit/moe-traces/GPT-ep-trace-8-nodes");
    auto tpl_bert = MoeTraceParser::LoadTraces("scratch/net-jit/moe-traces/bert_trace_64batch_32expert");
    auto tpl_t5 = MoeTraceParser::LoadTraces("scratch/net-jit/moe-traces/t5_trace_32bacth_32expert");

    if (tpl_gpt.empty() || tpl_bert.empty() || tpl_t5.empty()) {
        NS_LOG_ERROR("Failed to load trace templates. Exiting.");
        return 1;
    }
    
    MoeTraceParser::TraceConfig config_base;
    config_base.target_n = app_node_count; // 现在的 target_n 变成了 8
    config_base.zipf_alpha = 1.2;
    config_base.start_time_offset = 1.0; 
    config_base.volume_scale = scale_factor; 
    config_base.computation_scale = scale_factor;
    config_base.max_events = 250; //修改这里控制每个任务all2allv的数量
    
    // App 1: GPT Batch 10
    MoeTraceParser::TraceConfig config_app1 = config_base;
    config_app1.target_batch = 10;
    config_app1.volume_scale = scale_factor * app1_volume_factor;
    config_app1.computation_scale = scale_factor * app1_volume_factor;
    
    // App 2: GPT Batch 90
    MoeTraceParser::TraceConfig config_app2 = config_base;
    config_app2.target_batch = 90;
    config_app2.volume_scale = scale_factor * app2_volume_factor;
    config_app2.computation_scale = scale_factor * app2_volume_factor;
    
    // App 3: T5 Batch 10
    MoeTraceParser::TraceConfig config_app3 = config_base;
    config_app3.target_batch = 10;
    config_app3.volume_scale = scale_factor * app3_volume_factor;
    config_app3.computation_scale = scale_factor * app3_volume_factor;
    
    // App 4: BERT Batch 10
    MoeTraceParser::TraceConfig config_app4 = config_base;
    config_app4.target_batch = 10;
    config_app4.volume_scale = scale_factor * app4_volume_factor;
    config_app4.computation_scale = scale_factor * app4_volume_factor;

    g_app1_events = MoeTraceParser::GenerateScaledEvents(tpl_gpt, config_app1);
    g_app2_events = MoeTraceParser::GenerateScaledEvents(tpl_gpt, config_app2);
    g_app3_events = MoeTraceParser::GenerateScaledEvents(tpl_t5, config_app3);
    g_app4_events = MoeTraceParser::GenerateScaledEvents(tpl_bert, config_app4);
    
    // Original bandwidth 100Gbps
    uint64_t scaledBandwidthBps = static_cast<uint64_t>(100000000000ULL * scale_factor);
    if (scaledBandwidthBps == 0) scaledBandwidthBps = 1;
    std::string bandwidth = std::to_string(scaledBandwidthBps) + "bps"; 
    std::string delay = "1us";
    
    NS_LOG_INFO("Running Spine-Leaf Comparison test in mode: " << modeStr << " | Total Nodes: " << g_num_nodes << " | Active Nodes: " << active_nodes);
    
    // 构建完整的物理拓扑（128节点等全部存在）
    SpineLeafTopology topo(leaf_count, servers_per_leaf, spine_count, bandwidth, delay);

    MoeJITApplicationHelper moeHelper;
    moeHelper.SetRunContext(modeStr, bandwidth, active_nodes); // RunContext 对应活跃任务数量
    moeHelper.SetTraceLoader(GetTasksForRank);
    
    std::shared_ptr<MoeScheduler> scheduler;
    if (modeStr == "ideal") {
        auto idealSched = std::make_shared<CentralizedAdmissionScheduler>(scale_factor, active_nodes);
        scheduler = idealSched;
    } else {
        DistributedAgingScheduler::Mode mode;
        if (modeStr == "fifo") mode = DistributedAgingScheduler::FIFO;
        else if (modeStr == "sjf") mode = DistributedAgingScheduler::SJF;
        else if (modeStr == "srpt") mode = DistributedAgingScheduler::SRPT;
        else mode = DistributedAgingScheduler::AGING;
        // 【修改点】调度器只考虑前 g_active_leaf_count (4) 个 Leaf
        scheduler = std::make_shared<DistributedAgingScheduler>(servers_per_leaf, g_active_leaf_count, mode, scale_factor); 
    }
    moeHelper.SetScheduler(scheduler);

    // 【修改点】仅将前 active_nodes (32) 台服务器过滤出来安装应用
    NodeContainer activeNodeContainer;
    for (uint32_t i = 0; i < active_nodes; ++i) {
        activeNodeContainer.Add(topo.servers.Get(i));
    }

    NS_LOG_INFO("Installing Applications on " << activeNodeContainer.GetN() << " active servers...");
    ApplicationContainer apps = moeHelper.Install(activeNodeContainer);
    
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(100.0));
    Simulator::Stop(Seconds(100.1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalDelay = 0;
    uint64_t totalRxPackets = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->second.rxPackets > 0) {
            totalDelay += i->second.delaySum.GetSeconds();
            totalRxPackets += i->second.rxPackets;
        }
    }

    double avgDelayMs = totalRxPackets > 0 ? (totalDelay / totalRxPackets) * 1000.0 : 0.0;

    moeHelper.SetExtraStats(0.0, avgDelayMs);
    moeHelper.ReportStatistics();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation Finished.");
    return 0;
}