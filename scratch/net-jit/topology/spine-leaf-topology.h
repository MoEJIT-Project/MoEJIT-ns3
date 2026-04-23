#ifndef SPINE_LEAF_TOPOLOGY_H
#define SPINE_LEAF_TOPOLOGY_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <string>
#include <vector>

namespace ns3 {

class SpineLeafTopology {
public:
    uint32_t m_leafCount;        // Number of leaf switches
    uint32_t m_serversPerLeaf;   // Number of servers connected to each leaf
    uint32_t m_spineCount;       // Number of core/spine switches

    std::string m_bandwidth;
    std::string m_delay;

    NodeContainer spineSwitches;
    NodeContainer leafSwitches;
    NodeContainer servers;
    NodeContainer allNodes;

    NetDeviceContainer spineLeafDevices;
    NetDeviceContainer leafServerDevices;

    Ipv4InterfaceContainer spineLeafInterfaces;
    Ipv4InterfaceContainer leafServerInterfaces;

    SpineLeafTopology(uint32_t leafCount, uint32_t serversPerLeaf, uint32_t spineCount, std::string bandwidth = "10Gbps", std::string delay = "1us");

    void Build();
    void InstallStack();
    void AssignIpv4Addresses();

    uint32_t GetTotalServers() const { return m_leafCount * m_serversPerLeaf; }
};

} // namespace ns3

#endif // SPINE_LEAF_TOPOLOGY_H
