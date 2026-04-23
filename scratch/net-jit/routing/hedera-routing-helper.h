#ifndef NS3_HEDERA_ROUTING_HELPER_H
#define NS3_HEDERA_ROUTING_HELPER_H

#include <memory>
#include <random>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <ns3/internet-module.h>

#include "fat-tree-topology.h"
#include "hedera-routing.h"
#include "ipv4-routing-utility.h"

namespace ns3 {
    class HederaRoutingHelper : public Ipv4RoutingHelper {
    private:
        using Flow = HederaRouting::Flow;
        using Link = HederaRouting::Link;
        using RouteMap = HederaRouting::RouteMap;
        using LinkMap = HederaRouting::LinkMap;
        using AddressMap = HederaRouting::AddressMap;

        using PodMap = std::unordered_map<NS3NodeID, std::shared_ptr<std::unordered_set<NS3NodeID>>>;
        using FlowMap = std::unordered_map<NS3NodeID, std::unordered_set<Flow>>;
        using FlowCountMap = std::unordered_map<NS3NodeID, std::unordered_map<NS3NodeID, std::size_t>>;
        using LinkUsage = std::unordered_map<Link, double>;
        using FlowEstimations = std::unordered_map<Flow, double>;
        using NeighborState = std::tuple<NS3NodeID, NS3NodeID, double, LinkUsage>; // node1, node2, delta, affected links usage delta

        std::shared_ptr<FatTreeTopology> topology; // the fat tree topology
        std::mt19937 randomGenerator;
        std::size_t initialTemperature; // the initial temperature in simulated annealing
        double c; // the constant in simulated annealing
        bool respondToLinkChanges; // whether to respond to link changes

        std::shared_ptr<RouteMap> routes; // destination -> core switch
        std::shared_ptr<LinkMap> links; // link map: current node -> next hop -> the addresses connecting current node and next hop
        std::shared_ptr<AddressMap> addresses; // address map: address -> node id
        PodMap podMap; // pod map: node -> set of hosts/edge switches/aggregation switches in the same pod


        FlowEstimations estimateFlow(const FlowCountMap &forwardFlows, const FlowCountMap &backwardFlows) const;

        static PodMap calculatePods(const FatTreeTopology &topology);

        LinkUsage calculateLinkUsage(const FlowEstimations &flowEstimations) const;

        NeighborState neighbor(const FlowEstimations &flowEstimations, const LinkUsage &linkUsage, const FlowMap &destinationToFlowMap);

    public:
        HederaRoutingHelper(FatTreeTopology topology, std::mt19937 randomGenerator, std::size_t initialTemperature, double c, bool respondToLinkChanges);

        HederaRoutingHelper *Copy() const override;

        Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override;

        void updateRoutes(const std::vector<Flow> &flows);

        void resetRoutes();

        void notifyLinkChanges() const;
    };
}

#endif //NS3_HEDERA_ROUTING_HELPER_H
