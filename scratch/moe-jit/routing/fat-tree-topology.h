#ifndef NS3_FAT_TREE_TOPOLOGY_H
#define NS3_FAT_TREE_TOPOLOGY_H

#include <unordered_map>
#include <unordered_set>

namespace ns3 {
    struct FatTreeTopology {
        std::unordered_set<NS3NodeID> coreSwitches;
        std::unordered_set<NS3NodeID> aggregationSwitches;
        std::unordered_set<NS3NodeID> edgeSwitches;
        std::unordered_set<NS3NodeID> hosts;

        std::unordered_map<NS3NodeID, std::unordered_set<NS3NodeID>> coreToAggregation;
        std::unordered_map<NS3NodeID, std::unordered_set<NS3NodeID>> aggregationToCore;
        std::unordered_map<NS3NodeID, std::unordered_set<NS3NodeID>> aggregationToEdge;
        std::unordered_map<NS3NodeID, std::unordered_set<NS3NodeID>> edgeToAggregation;
        std::unordered_map<NS3NodeID, std::unordered_set<NS3NodeID>> edgeToHost;
        std::unordered_map<NS3NodeID, NS3NodeID> hostToEdge;

        std::vector<std::unordered_set<NS3NodeID>> pods; // each pod is a set of hosts
    };
}

#endif //NS3_FAT_TREE_TOPOLOGY_H
