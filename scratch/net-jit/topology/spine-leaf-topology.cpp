#include "spine-leaf-topology.h"

namespace ns3 {

SpineLeafTopology::SpineLeafTopology(uint32_t leafCount, uint32_t serversPerLeaf, uint32_t spineCount, std::string bandwidth, std::string delay)
    : m_leafCount(leafCount), m_serversPerLeaf(serversPerLeaf), m_spineCount(spineCount), m_bandwidth(bandwidth), m_delay(delay)
{
    Build();
    InstallStack();
    AssignIpv4Addresses();
}

void SpineLeafTopology::Build()
{
    spineSwitches.Create(m_spineCount);
    leafSwitches.Create(m_leafCount);
    servers.Create(m_leafCount * m_serversPerLeaf);

    allNodes.Add(spineSwitches);
    allNodes.Add(leafSwitches);
    allNodes.Add(servers);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(m_bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(m_delay));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100000p"));

    // Connect Spine to Leaf
    for (uint32_t l = 0; l < m_leafCount; ++l) {
        for (uint32_t s = 0; s < m_spineCount; ++s) {
            NetDeviceContainer devs = p2p.Install(spineSwitches.Get(s), leafSwitches.Get(l));
            spineLeafDevices.Add(devs);
        }
    }

    // Connect Leaf to Servers
    for (uint32_t l = 0; l < m_leafCount; ++l) {
        for (uint32_t i = 0; i < m_serversPerLeaf; ++i) {
            uint32_t serverIdx = l * m_serversPerLeaf + i;
            NetDeviceContainer devs = p2p.Install(leafSwitches.Get(l), servers.Get(serverIdx));
            leafServerDevices.Add(devs);
        }
    }
}

void SpineLeafTopology::InstallStack()
{
    InternetStackHelper stack;
    stack.Install(allNodes);
}

void SpineLeafTopology::AssignIpv4Addresses()
{
    Ipv4AddressHelper ipv4;
    
    // Assign addresses for Spine-Leaf links
    // Network: 10.1.x.0 / 24
    for (uint32_t i = 0; i < spineLeafDevices.GetN(); i += 2) {
        std::ostringstream subnet;
        subnet << "10.1." << (i / 2 + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        spineLeafInterfaces.Add(ipv4.Assign(spineLeafDevices.Get(i)));
        spineLeafInterfaces.Add(ipv4.Assign(spineLeafDevices.Get(i + 1)));
    }

    // Assign addresses for Leaf-Server links
    // Network: 10.2.x.0 / 24
    for (uint32_t i = 0; i < leafServerDevices.GetN(); i += 2) {
        std::ostringstream subnet;
        subnet << "10.2." << (i / 2 + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        leafServerInterfaces.Add(ipv4.Assign(leafServerDevices.Get(i)));
        leafServerInterfaces.Add(ipv4.Assign(leafServerDevices.Get(i + 1)));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

} // namespace ns3
