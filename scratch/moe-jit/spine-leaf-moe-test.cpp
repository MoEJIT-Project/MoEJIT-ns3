#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "topology/spine-leaf-topology.h"
#include "moe-jit.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpineLeafMoeTest");

// Custom MoE Task Generator (from fattree-moe-scheduling-test.cpp)
std::vector<MoeTask> CreateMixedTasks(MPIRankIDType rank) {
    std::vector<MoeTask> tasks;
    uint32_t nNodes = 16; // k * k = 4 * 4
    uint32_t nTasks = 5; 
    
    uint32_t top_k = 2; 
    uint32_t dataSize = 100 * 1024; // 100KB

    for (uint32_t taskId = 0; taskId < nTasks; ++taskId) {
        MoeTask t;
        t.taskId = taskId;
        t.computationTime = 0.0; 
        
        ns3::Ptr<ns3::UniformRandomVariable> jitterRng = ns3::CreateObject<ns3::UniformRandomVariable>();
        jitterRng->SetStream(rank * 100 + taskId);
        double jitter = jitterRng->GetValue(0.0, 0.05); 
        
        t.startTime = 1.0 + taskId * 0.5 + jitter; 

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
    LogComponentEnable("SpineLeafMoeTest", LOG_LEVEL_INFO);
    
    uint32_t k = 4;
    uint32_t subscription = 2;
    std::string bandwidth = "10Gbps";
    std::string delay = "1us";
    
    CommandLine cmd;
    cmd.AddValue ("k", "Number of leaf switches and servers per leaf", k);
    cmd.AddValue ("subscription", "Number of spine switches", subscription);
    cmd.Parse (argc, argv);

    NS_LOG_INFO("Creating Spine-Leaf Topology (k=" << k << ", sub=" << subscription << ")...");
    
    SpineLeafTopology topo(k, subscription, bandwidth, delay);

    NS_LOG_INFO("Configuring MoE JIT Application...");

    MoeJITApplicationHelper moeHelper;
    moeHelper.SetTraceLoader(CreateMixedTasks);
    
    // Use default FCFSScheduler (FIFO)
    // moeHelper.SetScheduler(std::make_shared<FCFSScheduler>()); // This is already default

    auto scheduler = std::make_shared<DistributedAgingScheduler>(k, k); // k servers per leaf, k leaves
    moeHelper.SetScheduler(scheduler);

    NS_LOG_INFO("Installing Applications on " << topo.GetTotalServers() << " servers...");
    ApplicationContainer apps = moeHelper.Install(topo.servers);
    
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(4.0));
    Simulator::Stop(Seconds(4.1));

    NS_LOG_INFO("Starting Simulation...");
    Simulator::Run();
    NS_LOG_INFO("Simulation Run phase complete.");
    Simulator::Destroy();
    NS_LOG_INFO("Full Simulation Finished and Destroyed.");
    return 0;
}
