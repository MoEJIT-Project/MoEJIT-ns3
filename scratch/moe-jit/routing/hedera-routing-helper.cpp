#include <ranges>

#include "hedera-routing-helper.h"

NS_LOG_COMPONENT_DEFINE("HederaRoutingHelper");

ns3::HederaRoutingHelper::FlowEstimations ns3::HederaRoutingHelper::estimateFlow(const FlowCountMap &forwardFlows, const FlowCountMap &backwardFlows) const {
    FlowEstimations flowEstimations;
    std::unordered_map<Flow, bool> converged;
    bool flag;
    do {
        flag = false;
        // source search
        for (auto host: topology->hosts | std::views::filter([&](auto h) { return forwardFlows.contains(h); })) {
            double demand = 0;
            std::size_t unconverged = 0;
            for (const auto &[dst, count]: forwardFlows.at(host)) {
                Flow flow{host, dst};
                if (converged[flow]) {
                    demand += flowEstimations[flow] * count;
                } else {
                    unconverged += count;
                }
            }
            if (unconverged <= 0) {
                continue;
            }
            auto estimation = (1.0 - demand) / unconverged;
            for (const auto &[dst, count]: forwardFlows.at(host)) {
                Flow flow{host, dst};
                if (converged[flow]) {
                    continue;
                }
                if (fabs(flowEstimations[flow] - estimation) > 1e-6) {
                    flowEstimations[flow] = estimation;
                    flag = true;
                }
            }
        }
        // destination search
        for (auto host: topology->hosts | std::views::filter([&](auto h) { return backwardFlows.contains(h); })) {
            std::unordered_map<Flow, bool> exceeded;
            double demand = 0;
            std::size_t remaining = 0;
            for (const auto &[src, count]: backwardFlows.at(host)) {
                Flow flow{src, host};
                exceeded[flow] = true;
                demand += flowEstimations[flow] * count;
                remaining += count;
            }
            if (demand <= 1.0) {
                continue;
            }
            demand = 0;
            double estimation = 1.0 / remaining;
            bool unconverged;
            do {
                remaining = 0;
                unconverged = false;
                for (const auto &[src, count]: backwardFlows.at(host)) {
                    Flow flow{src, host};
                    if (flowEstimations[flow] < estimation) {
                        demand += flowEstimations[flow] * count;
                        exceeded[flow] = false;
                        unconverged = true;
                    } else {
                        remaining += count;
                    }
                }
                if (remaining <= 0) {
                    NS_LOG_ERROR("the count of remaining unconverged flows should be greater than 0");
                    break;
                }
                estimation = (1.0 - demand) / remaining;
            } while (unconverged);
            for (const auto &[src, count]: backwardFlows.at(host)) {
                Flow flow{src, host};
                if (exceeded[flow]) {
                    if (fabs(flowEstimations[flow] - estimation) > 1e-6) {
                        flowEstimations[flow] = estimation;
                        flag = true;
                    }
                    converged[flow] = true;
                }
            }
        }
    } while (flag);
    return flowEstimations;
}

ns3::HederaRoutingHelper::PodMap ns3::HederaRoutingHelper::calculatePods(const FatTreeTopology &topology) {
    PodMap podMap{};
    for (const auto &pod: topology.pods) {
        auto podNodes = std::make_shared<std::unordered_set<NS3NodeID>>();
        for (const auto &node: pod) {
            podMap[node] = podNodes;
            auto [_, inserted] = podNodes->insert(node);
            if (!inserted) {
                continue;
            }
            auto edgeSwitch = topology.hostToEdge.at(node);
            std::tie(_, inserted) = podNodes->insert(edgeSwitch);
            podMap[edgeSwitch] = podNodes;
            if (!inserted) {
                continue;
            }
            for (const auto &aggregationSwitch: topology.edgeToAggregation.at(edgeSwitch)) {
                podNodes->insert(aggregationSwitch);
                podMap[aggregationSwitch] = podNodes;
            }
        }
    }
    auto emptyPod = std::make_shared<std::unordered_set<NS3NodeID>>();
    for (auto coreSwitch: topology.coreSwitches) {
        podMap[coreSwitch] = emptyPod;
    }
    return podMap;
}

ns3::HederaRoutingHelper::LinkUsage ns3::HederaRoutingHelper::calculateLinkUsage(const FlowEstimations &flowEstimations) const {
    LinkUsage linkUsage;
    for (const auto &[flow, estimation]: flowEstimations) {
        auto [source, destination] = flow;
        if (podMap.at(source) == podMap.at(destination)) {
            continue;
        }
        auto route = HederaRouting::calculateRoute(flow, *topology, routes->at(destination));
        for (auto [start, end]: route | std::views::adjacent<2>) {
            linkUsage[{start, end}] += estimation;
        }
    }
    return linkUsage;
}

ns3::HederaRoutingHelper::NeighborState ns3::HederaRoutingHelper::neighbor(const FlowEstimations &flowEstimations, const LinkUsage &linkUsage, const FlowMap &destinationToFlowMap) {
    NS3NodeID affectedDestination1;
    NS3NodeID affectedDestination2;
    std::vector<NS3NodeID> affectedDestinations;
    std::ranges::sample(destinationToFlowMap | std::views::keys, std::back_inserter(affectedDestinations), 2, randomGenerator);
    NS_ASSERT_MSG(affectedDestinations.size() == 2, "the number of nodes should be equal to 2");
    affectedDestination1 = affectedDestinations[0];
    affectedDestination2 = affectedDestinations[1];
    double delta = 0;
    LinkUsage affectedLinks{};
    for (auto flow: destinationToFlowMap.at(affectedDestination1)) {
        auto originalRoute = HederaRouting::calculateRoute(flow, *topology, routes->at(affectedDestination1));
        auto newRoute = HederaRouting::calculateRoute(flow, *topology, routes->at(affectedDestination2));
        for (auto [start, end]: originalRoute | std::views::adjacent<2>) {
            affectedLinks[{start, end}] -= flowEstimations.at(flow);
        }
        for (auto [start, end]: newRoute | std::views::adjacent<2>) {
            affectedLinks[{start, end}] += flowEstimations.at(flow);
        }
    }
    for (auto flow: destinationToFlowMap.at(affectedDestination2)) {
        auto originalRoute = HederaRouting::calculateRoute(flow, *topology, routes->at(affectedDestination2));
        auto newRoute = HederaRouting::calculateRoute(flow, *topology, routes->at(affectedDestination1));
        for (auto [start, end]: originalRoute | std::views::adjacent<2>) {
            affectedLinks[{start, end}] -= flowEstimations.at(flow);
        }
        for (auto [start, end]: newRoute | std::views::adjacent<2>) {
            affectedLinks[{start, end}] += flowEstimations.at(flow);
        }
    }
    for (const auto &[link, usage]: affectedLinks) {
        auto originalUsage = 0.0;
        if (linkUsage.contains(link)) {
            originalUsage = linkUsage.at(link);
        }
        delta += std::exp(usage - 1.0);
        delta -= std::exp(originalUsage - 1.0);
    }
    return {affectedDestination1, affectedDestination2, delta, affectedLinks};
}

ns3::HederaRoutingHelper::HederaRoutingHelper(FatTreeTopology topology, std::mt19937 randomGenerator, std::size_t initialTemperature, double c, bool respondToLinkChanges) :
        Ipv4RoutingHelper(),
        topology(std::make_shared<FatTreeTopology>(std::move(topology))),
        randomGenerator(std::move(randomGenerator)),
        initialTemperature(initialTemperature),
        c(c),
        respondToLinkChanges(respondToLinkChanges),
        routes(std::make_shared<RouteMap>()),
        links(std::make_shared<LinkMap>()),
        addresses(std::make_shared<AddressMap>()),
        podMap(calculatePods(*this->topology)) {
    resetRoutes();
}

void ns3::HederaRoutingHelper::updateRoutes(const std::vector<Flow> &flows) {
    FlowMap forwardFlowMap;
    FlowCountMap forwardFlows;
    FlowMap backwardFlowMap;
    FlowCountMap backwardFlows;
    for (const auto &flow: flows) {
        auto [src, dst] = flow;
        forwardFlowMap[src].insert(flow);
        ++forwardFlows[src][dst];
        backwardFlowMap[dst].insert(flow);
        ++backwardFlows[dst][src];
    }
    auto flowEstimations = estimateFlow(forwardFlows, backwardFlows);
    auto linkUsage = calculateLinkUsage(flowEstimations);
    for (auto temperature = initialTemperature; temperature > 0; --temperature) {
        auto [affectedDestination1, affectedDestination2, delta, affectedLinks] = neighbor(flowEstimations, linkUsage, backwardFlowMap);
        if (delta < 0 || std::exp(-c * delta / temperature) > std::generate_canonical<double, std::numeric_limits<double>::digits>(randomGenerator)) {
            std::swap((*routes)[affectedDestination1], (*routes)[affectedDestination2]);
            for (const auto &[link, usage]: affectedLinks) {
                linkUsage[link] += usage;
            }
        }
    }
}

void ns3::HederaRoutingHelper::notifyLinkChanges() const {
    links->clear();
    addresses->clear();
    for (auto i = NodeList::Begin(); i < NodeList::End(); ++i) {
        auto localNode = *i;
        auto localNodeId = localNode->GetId();
        auto localIpv4 = localNode->GetObject<Ipv4>();
        for (uint32_t interface = 0; interface < localIpv4->GetNInterfaces(); ++interface) {
            if (localIpv4->GetNAddresses(interface) <= 0) {
                continue;
            }
            auto localAddress = localIpv4->GetAddress(interface, 0).GetLocal();
            (*addresses)[localAddress] = localNodeId;
            auto local = localIpv4->GetNetDevice(interface);
            if (DynamicCast<LoopbackNetDevice>(local)) {
                continue;
            }
            if (local->IsPointToPoint()) {
                auto remote = RemoteDevice(local);
                auto remoteNode = remote->GetNode();
                auto remoteNodeId = remoteNode->GetId();
                auto remoteIpv4 = remoteNode->GetObject<Ipv4>();
                auto remoteInterface = remoteIpv4->GetInterfaceForDevice(remote);
                if (remoteInterface < 0 || remoteIpv4->GetNAddresses(remoteInterface) <= 0) {
                    continue;
                }
                auto remoteAddress = remoteIpv4->GetAddress(remoteInterface, 0).GetLocal();
                if (isLinkEnabled(local, remote)) {
                    (*links)[localNodeId][remoteNodeId] = {localAddress, remoteAddress};
                }
            } else if (local->IsMulticast() || local->IsBroadcast()) {
                NS_LOG_WARN("multicast/broadcast links are not supported");
            }
        }
    }
}

void ns3::HederaRoutingHelper::resetRoutes() {
    for (auto &pod: topology->pods) {
        NS_ASSERT_MSG(pod.size() == topology->coreSwitches.size(), "the number of hosts in a pod should be equal to the number of core switches");
        std::optional<NS3NodeID> core = std::nullopt;
        std::ranges::sample(topology->coreSwitches, OptionalInserter{core}, 1, randomGenerator);
        for (auto host: pod) {
            (*routes)[host] = core.value();
        }
    }
}

ns3::HederaRoutingHelper *ns3::HederaRoutingHelper::Copy() const {
    return new HederaRoutingHelper(*this);
}

ns3::Ptr<ns3::Ipv4RoutingProtocol> ns3::HederaRoutingHelper::Create(Ptr <Node> node) const {
    HederaRouting::LinkChangeCallbak callback;
    if (respondToLinkChanges) {
        callback = std::bind(&HederaRoutingHelper::notifyLinkChanges, this);
    }
    auto routing = CreateObject<HederaRouting>(topology, routes, addresses, podMap.at(node->GetId()), links, callback);
    return routing;
}
