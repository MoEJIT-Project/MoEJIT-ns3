#include "hedera-routing.h"

NS_LOG_COMPONENT_DEFINE("HederaRouting");

ns3::HederaRouting::HederaRouting(const std::shared_ptr<FatTreeTopology> &topology, const std::shared_ptr<RouteMap> &routes, const std::shared_ptr<AddressMap> &addressMap, const std::shared_ptr<Subnet> &subnet, const std::shared_ptr<LinkMap> &links, LinkChangeCallbak linkChangeCallback) :
        Ipv4RoutingProtocol(),
        topology(topology),
        routes(routes),
        address_map(addressMap),
        subnet(subnet),
        links(links),
        linkChangeCallback(linkChangeCallback) {}

ns3::Ptr<ns3::Ipv4Route> ns3::HederaRouting::RouteOutput(Ptr <Packet> p, const Ipv4Header &header, Ptr <NetDevice> oif, Socket::SocketErrno &socketErrno) {
    if (!m_ipv4) {
        socketErrno = Socket::ERROR_AFNOSUPPORT;
        return nullptr;
    }
    NS_ASSERT_MSG(m_ipv4->GetNInterfaces() > 0, "No interfaces");
    auto destination = header.GetDestination();
    auto currentNode = m_ipv4->GetNetDevice(0)->GetNode()->GetId();
    auto sourceNode = currentNode;
    auto destinationNode = address_map->at(destination);
    if ((subnet->contains(sourceNode) == subnet->contains(destinationNode)) || !routes->contains(destinationNode)) {
        socketErrno = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    auto coreSwitch = routes->at(destinationNode);
    auto route = calculateRoute({sourceNode, destinationNode}, *topology, coreSwitch);
    auto currentIterator = std::find(route.cbegin(), route.cend(), currentNode);
    if (currentIterator == route.cend()) {
        socketErrno = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    auto nextHopIterator = std::next(currentIterator);
    if (nextHopIterator == route.cend()) {
        socketErrno = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    auto nextHop = *nextHopIterator;
    auto [outputAddress, nextHopAddress] = links->at(currentNode).at(nextHop);
    auto outputInterface = m_ipv4->GetInterfaceForAddress(outputAddress);
    auto ipv4Route = Create<Ipv4Route>();
    ipv4Route->SetDestination(destination);
    ipv4Route->SetGateway(Ipv4Address::GetZero());
    ipv4Route->SetSource(m_ipv4->SourceAddressSelection(outputInterface, ipv4Route->GetDestination()));
    ipv4Route->SetOutputDevice(m_ipv4->GetNetDevice(outputInterface));
    if (oif != nullptr && oif != ipv4Route->GetOutputDevice()) {
        socketErrno = Socket::ERROR_NOROUTETOHOST;
        return nullptr;
    }
    socketErrno = Socket::ERROR_NOTERROR;
    return ipv4Route;
}

bool ns3::HederaRouting::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, const UnicastForwardCallback &ucb, const MulticastForwardCallback &mcb, const LocalDeliverCallback &lcb, const ErrorCallback &ecb) {
    auto source = header.GetSource();
    auto destination = header.GetDestination();
    auto interface = m_ipv4->GetInterfaceForDevice(idev);
    if (m_ipv4->IsDestinationAddress(destination, interface)) {
        if (!lcb.IsNull()) {
            lcb(p, header, interface);
        }
        return true;
    }
    if (!m_ipv4->IsForwarding(interface)) {
        if (!ecb.IsNull()) {
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        }
        return true;
    }
    NS_ASSERT_MSG(address_map->contains(source), "source address not found");
    NS_ASSERT_MSG(address_map->contains(destination), "destination address not found");
    auto sourceNode = address_map->at(source);
    auto destinationNode = address_map->at(destination);
    auto currentNode = idev->GetNode()->GetId();
    if (!topology->coreSwitches.contains(currentNode)) {
        if ((subnet->contains(sourceNode) == subnet->contains(destinationNode)) || !routes->contains(destinationNode)) {
            return false;
        }
    }
    auto coreSwitch = routes->at(destinationNode);
    auto route = calculateRoute({sourceNode, destinationNode}, *topology, coreSwitch);
    auto currentIterator = std::find(route.cbegin(), route.cend(), currentNode);
    if (currentIterator == route.cend()) {
        return false;
    }
    auto nextHopIterator = std::next(currentIterator);
    if (nextHopIterator == route.cend()) {
        return false;
    }
    auto nextHop = *nextHopIterator;
    auto [outputAddress, nextHopAddress] = links->at(currentNode).at(nextHop);
    auto outputInterface = m_ipv4->GetInterfaceForAddress(outputAddress);
    auto ipv4Route = Create<Ipv4Route>();
    ipv4Route->SetDestination(destination);
    ipv4Route->SetGateway(Ipv4Address::GetZero());
    ipv4Route->SetSource(m_ipv4->SourceAddressSelection(outputInterface, ipv4Route->GetDestination()));
    ipv4Route->SetOutputDevice(m_ipv4->GetNetDevice(outputInterface));
    ucb(ipv4Route, p, header);
    return true;
}

void ns3::HederaRouting::SetIpv4(Ptr <Ipv4> ipv4) {
    m_ipv4 = ipv4;
    if (linkChangeCallback) {
        linkChangeCallback();
    }
}

void ns3::HederaRouting::NotifyInterfaceUp(uint32_t interface) {
    if (linkChangeCallback) {
        linkChangeCallback();
    }
}

void ns3::HederaRouting::NotifyInterfaceDown(uint32_t interface) {
    if (linkChangeCallback) {
        linkChangeCallback();
    }
}

void ns3::HederaRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    if (linkChangeCallback) {
        linkChangeCallback();
    }
}

void ns3::HederaRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    if (linkChangeCallback) {
        linkChangeCallback();
    }
}

void ns3::HederaRouting::PrintRoutingTable(Ptr <OutputStreamWrapper> stream, Time::Unit unit) const {
    auto &s = *stream->GetStream();
    s << "Routing table\n";
    for (const auto &route: *routes) {
        s << "Destination: " << route.first << " Core switch: " << route.second << "\n";
    }
}

ns3::HederaRouting::Route ns3::HederaRouting::calculateRoute(const Flow &flow, const FatTreeTopology &topology, const NS3NodeID coreSwitch) {
    Route route{};
    auto [source, destination] = flow;
    route.push_back(source);
    auto sourceEdgeSwitch = topology.hostToEdge.at(source);
    route.push_back(sourceEdgeSwitch);
    bool found = false;
    for (auto aggregationSwitch: topology.edgeToAggregation.at(sourceEdgeSwitch)) {
        if (topology.aggregationToCore.at(aggregationSwitch).contains(coreSwitch)) {
            route.push_back(aggregationSwitch);
            found = true;
            break;
        }
    }
    NS_ASSERT_MSG(found, "source aggregation switch not found");
    route.push_back(coreSwitch);
    auto destinationEdgeSwitch = topology.hostToEdge.at(destination);
    found = false;
    for (auto aggregationSwitch: topology.coreToAggregation.at(coreSwitch)) {
        if (topology.aggregationToEdge.at(aggregationSwitch).contains(destinationEdgeSwitch)) {
            route.push_back(aggregationSwitch);
            found = true;
            break;
        }
    }
    NS_ASSERT_MSG(found, "destination aggregation switch not found");
    route.push_back(destinationEdgeSwitch);
    route.push_back(destination);
    return route;
}
