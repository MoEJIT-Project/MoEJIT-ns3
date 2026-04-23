#ifndef NS3_HEDERA_ROUTING_H
#define NS3_HEDERA_ROUTING_H

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <ns3/internet-module.h>

#include "fat-tree-topology.h"
#include "ipv4-routing-utility.h"

namespace ns3 {
    class HederaRouting : public Ipv4RoutingProtocol {
    public:
        using Flow = std::pair<NS3NodeID, NS3NodeID>;
        using Link = std::pair<NS3NodeID, NS3NodeID>;
        using Route = std::vector<NS3NodeID>;
        using Subnet = std::unordered_set<NS3NodeID>;
        using AddressMap = std::unordered_map<Ipv4Address, NS3NodeID>;
        using RouteMap = std::unordered_map<NS3NodeID, NS3NodeID>;
        using LinkMap = std::unordered_map<NS3NodeID, std::unordered_map<NS3NodeID, std::pair<Ipv4Address, Ipv4Address>>>;
        using LinkChangeCallbak = std::function<void()>;

    private:
        Ptr<Ipv4> m_ipv4;

        std::shared_ptr<FatTreeTopology> topology;
        std::shared_ptr<RouteMap> routes; // destination -> core switch
        std::shared_ptr<AddressMap> address_map;
        std::shared_ptr<Subnet> subnet;
        std::shared_ptr<LinkMap> links; // reversed link map
        LinkChangeCallbak linkChangeCallback;

    public:
        HederaRouting(const std::shared_ptr<FatTreeTopology> &topology, const std::shared_ptr<RouteMap> &routes, const std::shared_ptr<AddressMap> &addressMap, const std::shared_ptr<Subnet> &subnet, const std::shared_ptr<LinkMap> &links, LinkChangeCallbak linkChangeCallback);

        Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &socketErrno) override;

        bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, const UnicastForwardCallback &ucb, const MulticastForwardCallback &mcb, const LocalDeliverCallback &lcb, const ErrorCallback &ecb) override;

        void SetIpv4(Ptr<Ipv4> ipv4) override;

        void NotifyInterfaceUp(uint32_t interface) override;

        void NotifyInterfaceDown(uint32_t interface) override;

        void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;

        void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;

        void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override;

        ~HederaRouting() override = default;

        static Route calculateRoute(const Flow &flow, const FatTreeTopology &topology, const NS3NodeID coreSwitch);
    };
}

template<typename T> requires std::is_convertible_v<T, ns3::HederaRouting::Flow> || std::is_convertible_v<T, ns3::HederaRouting::Link>
struct std::hash<T> {
    std::size_t operator()(const T &pair) const noexcept {
        auto [src, dst] = pair;
        return std::hash<ns3::NS3NodeID>{}(src) ^ std::hash<ns3::NS3NodeID>{}(dst);
    }
};

#endif //NS3_HEDERA_ROUTING_H
