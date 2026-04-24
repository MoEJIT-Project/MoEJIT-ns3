//
// Created by Zoeyzyy on 2024/4/29.
//

#include "dml-application.h"
#include "main.h"
#include "net-jit-application.h"
#include "topology/fattree-topology.h"
#include "topology/utility.h"

#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor.h"
#include <ns3/applications-module.h>
#include <ns3/core-module.h>
#include <ns3/internet-module.h>
#include <ns3/ipv4-l3-protocol.h>
#include <ns3/mpi-application-module.h>
#include <ns3/mpi-application.h>
#include <ns3/network-module.h>
#include <ns3/point-to-point-module.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using TimeType = std::chrono::duration<int64_t, std::nano>;
using PredictionCallback =
    std::function<void(ns3::MPIRankIDType, ns3::BatchID, TimeType, ns3::SizeType)>;
using BatchCallback = std::function<void(MPIRankIDType, BatchID)>;
using ApplicationCallback = std::function<void(MPIRankIDType)>;

void
record_into_file(std::filesystem::path filename, std::string line)
{
    // 打开文件，如果文件不存在则创建新文件
    std::fstream file;
    file.open(filename, std::ios::app); // std::ios::app 表示在文件末尾追加内容
    // 检查文件是否成功打开
    if (!file.good())
    {
        throw std::runtime_error("Could not open to-write file");
    }
    // 在文件中写入字符串
    file << line << std::endl;
    // 关闭文件
    file.close();
}

void
batch_start_callback(MPIRankIDType id, BatchID batch_id)
{
    record_into_file("batch_start.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(id) + " " + std::to_string(batch_id));
}

void
batch_end_callback(MPIRankIDType id, BatchID batch_id)
{
    record_into_file("batch_end.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(id) + " " + std::to_string(batch_id));
}

void
application_start_callback(MPIRankIDType id)
{
    record_into_file("application_start.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(id));
}

void
application_end_callback(MPIRankIDType id)
{
    record_into_file("application_end.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(id));
}

void
record_throughput_curve(ns3::ApplicationContainer& applications)
{
    for (int i = 1; i < (int)applications.GetN(); i += 2)
    {
        auto application = DynamicCast<MPIApplication>(applications.Get(i));
        if (application->Initialized() == false)
        {
            record_into_file(
                "./throughput_curve_tx/throughput_curve_tx_" + std::to_string(i / 2) + ".txt",
                std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " + std::to_string(0));
            record_into_file(
                "./throughput_curve_rx/throughput_curve_rx_" + std::to_string(i / 2) + ".txt",
                std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " + std::to_string(0));
            if (i == 1)
                ns3::Simulator::Schedule(Seconds(0.1), record_throughput_curve, applications);
            continue;
        }
        auto& communicator = application->communicator(ns3::WORLD_COMMUNICATOR);
        auto TxBytes = communicator.TxBytes();
        auto RxBytes = communicator.RxBytes();
        record_into_file(
            "./throughput_curve_tx/throughput_curve_tx_" + std::to_string(i / 2) + ".txt",
            std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " + std::to_string(TxBytes));
        record_into_file(
            "./throughput_curve_rx/throughput_curve_rx_" + std::to_string(i / 2) + ".txt",
            std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " + std::to_string(RxBytes));
        if (i == 1)
            ns3::Simulator::Schedule(Seconds(0.1), record_throughput_curve, applications);
    }
}

int
main(int argc, char** argv)
{
    // ns3::LogComponentEnable("Ipv4UGALRouting", ns3::LOG_LEVEL_ALL);
    // ns3::LogComponentEnable("CongestionMonitorApplication", ns3::LOG_LEVEL_ALL);
    // ns3::LogComponentEnable("Ipv4UGALRoutingHelper", ns3::LOG_LEVEL_ALL);
    // ns3::LogComponentEnableAll(ns3::LOG_LEVEL_WARN);
    // ns3::LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // setting default parameters
    std::string tracePath = "scratch/net-jit/traces/resnet152";
    std::string measure_traceFilePath = "scratch/net-jit/net-jit-traces/resnet152/1.txt";
    std::string tracePath2 = "scratch/net-jit/traces/gpt2";
    std::string measure_traceFilePath2 = "scratch/net-jit/net-jit-traces/gpt2/1.txt";

    float start_time = 0.0;
    float stop_time = 5000;

    std::size_t k = 4;//8
    std::string bandwidth = "100Gbps";
    std::string delay = "1us";
    std::size_t ocs = 0;
    bool ugal = false;
    bool flowRouting = false;
    double congestionMonitorPeriod = 0.0001;
    bool enable_reconfiguration = false;
    float reconfiguration_timestep = 0.0002;
    bool is_adversial = false;
    bool ecmp = true;
    std::string app_bd = "10Gbps";
    double bias = 0.0;
    int reconfiguration_count = 100;
    bool only_reconfiguration = false;
    uint16_t maxBytes = 9000;
    std::string bandwidth_opt = "400Gbps";
    double scale_factor = 0.0001;//0.00001
    int64_t optimization_delay = 100000000; // ns
    std::string measure_method =
        "slide_window"; // "net-jit" or "static_window" or "slide_window" or "none"
    int64_t static_window_advance_time = 100000000; // ns
    std::size_t application_end_callback_count = 0; // for set stop simulation

    // double threshold = 0.00000001;

    // set default ttl
    ns3::Config::SetDefault("ns3::Ipv4L3Protocol::DefaultTtl", ns3::StringValue("128"));

    // set dctcp (to reset red attribute)
    ns3::Config::SetDefault("ns3::TcpSocketBase::UseEcn", ns3::StringValue("On"));
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                            ns3::TypeIdValue(ns3::TcpDctcp::GetTypeId()));
    ns3::Config::SetDefault("ns3::RedQueueDisc::UseEcn", ns3::BooleanValue(true));
    ns3::Config::SetDefault("ns3::RedQueueDisc::QW", ns3::DoubleValue(1.0));
    ns3::Config::SetDefault("ns3::RedQueueDisc::MinTh", ns3::DoubleValue(16));
    ns3::Config::SetDefault("ns3::RedQueueDisc::MaxTh", ns3::DoubleValue(16));

    // parse command line

    ns3::CommandLine cmd;
    cmd.Usage("simulations of HPC applications in reconfigurable network");
    cmd.AddValue("static_window_advance_time",
                 "the static window advance time",
                 static_window_advance_time);
    cmd.AddValue("measure_traceFilePath", "the path measurement traces", measure_traceFilePath);
    cmd.AddValue(
        "measure_method",
        "the measure method : \"net-jit\" or \"static_window\" or \"slide_window\" or \"none\"",
        measure_method);
    cmd.AddValue("optimization_delay", "the optimization delay in ns", optimization_delay);
    cmd.AddValue("scale_factor", "the scale factor of the network", scale_factor);
    cmd.AddValue("bandwidth_opt",
                 "the optimized bandwidth of the links in the topology",
                 bandwidth_opt);
    cmd.AddNonOption("trace-path", "the path containing binary dumpi traces", tracePath);
    cmd.AddValue("k",
                 "the fattree parameter k, represents the number of the ports per switch in "
                 "the topology",
                 k);
    cmd.AddValue("bandwidth", "the bandwidth of the links in the topology", bandwidth);
    cmd.AddValue("delay", "the delay of the links in the topology", delay);
    cmd.AddValue("ocs",
                 "the ocs count used in the reconfigurable topology, each ocs is connected to a "
                 "TOR in every group",
                 ocs);
    cmd.AddValue("ugal", "whether to use ugal routing", ugal);
    cmd.AddValue("flow-routing", "whether to use flow routing", flowRouting);
    cmd.AddValue("congestion-monitor-period",
                 "the period of congestion monitor in seconds",
                 congestionMonitorPeriod);
    cmd.AddValue("enable_reconfig", "whether to enable reconfiguration", enable_reconfiguration);
    cmd.AddValue("reconfig_time",
                 "the period of reconfiguration in seconds",
                 reconfiguration_timestep);
    cmd.AddValue("stop_time", "the time flow stop to generate", stop_time);
    cmd.AddValue("is_ad", "traffic pattern is adversial", is_adversial);
    cmd.AddValue("ecmp", "enable ecmp", ecmp);
    cmd.AddValue("app_bd", "app input speed", app_bd);
    cmd.AddValue("bias", "bias for ugal", bias);
    cmd.AddValue("reconfiguration_count", "the count of reconfiguration", reconfiguration_count);
    cmd.AddValue("only_reconfig", "whether no background links", only_reconfiguration);
    cmd.AddValue("max_bytes", "the max bytes of flow", maxBytes);

    cmd.Parse(argc, argv);

    if (ecmp)
    {
        ns3::Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting",
                                ns3::BooleanValue(true)); // 启用多路径路由
    }

    // the rate and packet size of sending
    ns3::Config::SetDefault("ns3::OnOffApplication::PacketSize", ns3::UintegerValue(1024));
    ns3::Config::SetDefault("ns3::OnOffApplication::DataRate", ns3::StringValue(app_bd));

    ns3::Time::SetResolution(ns3::Time::NS);

    // change bandwidth
    bandwidth = std::to_string(scale_factor * 5.0) + "Gbps";
    if (bandwidth_opt.find("4") != std::string::npos)
        bandwidth_opt = std::to_string(scale_factor * 400.0) + "Gbps";
    else
        bandwidth_opt = std::to_string(scale_factor * 100.0) + "Gbps";

    // initialize topology for hedera
    ns3::FatTreeTopology topology_hedera;
    // initialize hedera routing (just for initial fattree!)
    std::mt19937 mt(time(nullptr));
    auto hederaRoutingHelper = HederaRoutingHelper(topology_hedera, mt, 1000, 10000, false);

    FattreeTopology fattree(k,
                            bandwidth,
                            delay,
                            stop_time,
                            ecmp,
                            app_bd,
                            bandwidth_opt,
                            scale_factor,
                            optimization_delay,
                            hederaRoutingHelper);

    // initialize fattree nodes
    fattree.core_switches.Create(fattree.number_of_core_switches);
    fattree.aggre_switches.Create(fattree.number_of_aggre_switches);
    fattree.edge_switches.Create(fattree.number_of_edge_switches);
    fattree.servers.Create(fattree.number_of_servers);

    fattree.switches.Add(fattree.core_switches);
    fattree.switches.Add(fattree.aggre_switches);
    fattree.switches.Add(fattree.edge_switches);
    fattree.allNodes.Add(fattree.switches);
    fattree.allNodes.Add(fattree.servers);

    // install routing
    // hedera(default: min)
    // initialize hedera所需的拓扑信息
    {
        for (int core_switch_index = 0; core_switch_index < (int)fattree.core_switches.GetN();
             ++core_switch_index)
        {
            topology_hedera.coreSwitches.insert(
                fattree.core_switches.Get(core_switch_index)->GetId());
        }
        for (int aggre_switch_index = 0; aggre_switch_index < (int)fattree.aggre_switches.GetN();
             ++aggre_switch_index)
        {
            topology_hedera.aggregationSwitches.insert(
                fattree.aggre_switches.Get(aggre_switch_index)->GetId());
        }
        for (int edge_switch_index = 0; edge_switch_index < (int)fattree.edge_switches.GetN();
             ++edge_switch_index)
        {
            topology_hedera.edgeSwitches.insert(
                fattree.edge_switches.Get(edge_switch_index)->GetId());
        }
        for (int server_index = 0; server_index < (int)fattree.servers.GetN(); ++server_index)
        {
            topology_hedera.hosts.insert(fattree.servers.Get(server_index)->GetId());
        }
        // initialize links
        for (int core_index = 0; core_index < (int)fattree.core_switches.GetN(); ++core_index)
        {
            auto coreID = fattree.core_switches.Get(core_index)->GetId();
            for (int aggre_index = core_index / (k / 2);
                 aggre_index < (int)fattree.aggre_switches.GetN();)
            {
                topology_hedera.coreToAggregation[coreID].insert(
                    fattree.aggre_switches.Get(aggre_index)->GetId());
                aggre_index += k / 2;
            }
            NS_ASSERT_MSG(
                topology_hedera.coreToAggregation[coreID].size() == k,
                "topology hedera is not correct, coreToAggregation are not installed correctly");
        }
        for (int aggre_index = 0; aggre_index < (int)fattree.aggre_switches.GetN(); ++aggre_index)
        {
            auto aggreID = fattree.aggre_switches.Get(aggre_index)->GetId();
            for (int core_index = (int)(aggre_index % (k / 2)) * (k / 2);
                 core_index < (int)((aggre_index % (k / 2) + 1) * (k / 2));)
            {
                topology_hedera.aggregationToCore[aggreID].insert(
                    fattree.core_switches.Get(core_index)->GetId());
                core_index += 1;
            }
            NS_ASSERT_MSG(
                topology_hedera.aggregationToCore[aggreID].size() == k / 2,
                "topology hedera is not correct, aggregationToCore are not installed correctly");
        }
        for (int aggre_index = 0; aggre_index < (int)fattree.aggre_switches.GetN(); ++aggre_index)
        {
            auto aggreID = fattree.aggre_switches.Get(aggre_index)->GetId();
            for (int edge_index = ((int)(aggre_index / (k / 2))) * (k / 2);
                 edge_index < (int)(((int)(aggre_index / (k / 2)) + 1) * (k / 2));)
            {
                topology_hedera.aggregationToEdge[aggreID].insert(
                    fattree.edge_switches.Get(edge_index)->GetId());
                edge_index += 1;
            }
            NS_ASSERT_MSG(
                topology_hedera.aggregationToEdge[aggreID].size() == k / 2,
                "topology hedera is not correct, aggregationToEdge are not installed correctly");
        }
        for (int edge_index = 0; edge_index < (int)fattree.edge_switches.GetN(); ++edge_index)
        {
            auto edgeID = fattree.edge_switches.Get(edge_index)->GetId();
            for (int aggre_index = ((int)(edge_index / (k / 2))) * (k / 2);
                 aggre_index < (int)(((int)(edge_index / (k / 2)) + 1) * (k / 2));)
            {
                topology_hedera.edgeToAggregation[edgeID].insert(
                    fattree.aggre_switches.Get(aggre_index)->GetId());
                aggre_index += 1;
            }
            NS_ASSERT_MSG(
                topology_hedera.edgeToAggregation[edgeID].size() == k / 2,
                "topology hedera is not correct, edgeToAggregation are not installed correctly");
        }
        for (int edge_index = 0; edge_index < (int)fattree.edge_switches.GetN(); ++edge_index)
        {
            auto edgeID = fattree.edge_switches.Get(edge_index)->GetId();
            for (int server_index = (int)edge_index * (k / 2);
                 server_index < (int)((edge_index + 1) * (k / 2));)
            {
                topology_hedera.edgeToHost[edgeID].insert(
                    fattree.servers.Get(server_index)->GetId());
                server_index += 1;
            }
            NS_ASSERT_MSG(topology_hedera.edgeToHost[edgeID].size() == k / 2,
                          "topology hedera is not correct, edgeToHost are not installed correctly");
        }
        for (int server_index = 0; server_index < (int)fattree.servers.GetN(); ++server_index)
        {
            auto serverID = fattree.servers.Get(server_index)->GetId();
            topology_hedera.hostToEdge[serverID] =
                (fattree.edge_switches.Get(server_index / (k / 2))->GetId());
        }
        for (int pod_index = 0; pod_index < (int)k; ++pod_index)
        {
            std::unordered_set<NS3NodeID> pod;
            for (int server_index = pod_index * (k / 2) * (k / 2);
                 server_index < (int)((pod_index + 1) * (k / 2) * (k / 2));
                 ++server_index)
            {
                pod.insert(fattree.servers.Get(server_index)->GetId());
            }
            NS_ASSERT_MSG(pod.size() == (k / 2) * (k / 2),
                          "topology hedera is not correct, pod are not installed correctly");
            topology_hedera.pods.push_back(pod);
        }
    }
    hederaRoutingHelper = HederaRoutingHelper(topology_hedera, mt, 1000, 10000, false);
    fattree.hederaRoutingHelper = hederaRoutingHelper;

    auto listRoutinghelper = Ipv4ListRoutingHelper();
    if (fattree.hederaRoutingHelper)
        listRoutinghelper.Add(*fattree.hederaRoutingHelper, 1);
    listRoutinghelper.Add(fattree.globalRoutingHelper, 0);
    fattree.internetStackHelper.SetRoutingHelper(listRoutinghelper);

    // install internet stack
    fattree.internetStackHelper.Install(fattree.allNodes);

    // set p2p link parameters
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue(bandwidth));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue(delay));

    // set ipv4 address
    ns3::Ipv4AddressHelper ipv4AddressHelper{"10.0.0.0", "255.255.255.0"};

    // background links
    // server-tor links
    for (SwitchID switch_index = 0; switch_index < fattree.number_of_edge_switches; ++switch_index)
    {
        auto tor = fattree.edge_switches.Get(switch_index);
        for (ServerID server_index = 0; server_index < fattree.number_of_servers_per_edge_switch;
             ++server_index)
        {
            auto server = fattree.servers.Get(
                switch_index * fattree.number_of_servers_per_edge_switch + server_index);
            auto torIpv4 = tor->GetObject<ns3::Ipv4>();
            auto serverIpv4 = server->GetObject<ns3::Ipv4>();
            auto torIpv4L3Protocol = tor->GetObject<ns3::Ipv4L3Protocol>();
            auto serverIpv4L3Protocol = server->GetObject<ns3::Ipv4L3Protocol>();
            auto devices = p2pHelper.Install(tor, server);
            auto [torDevice, serverDevice] = ContainerPattern<2>(devices);
            auto addresses = ipv4AddressHelper.Assign(devices);
            auto [torAddress, serverAddress] =
                ContainerPattern<2>(addresses,
                                    [](const ns3::Ipv4InterfaceContainer& c, std::size_t offset) {
                                        return c.GetAddress(offset);
                                    });
            fattree.serverMap[server->GetId()] = {serverDevice, serverAddress};
            fattree.backgroundLinkMap[tor->GetId()][server->GetId()] = {torDevice,
                                                                        serverDevice,
                                                                        torIpv4,
                                                                        serverIpv4,
                                                                        torAddress,
                                                                        serverAddress,
                                                                        torIpv4L3Protocol,
                                                                        serverIpv4L3Protocol};
            fattree.backgroundLinkMap[server->GetId()][tor->GetId()] = {serverDevice,
                                                                        torDevice,
                                                                        serverIpv4,
                                                                        torIpv4,
                                                                        serverAddress,
                                                                        torAddress,
                                                                        serverIpv4L3Protocol,
                                                                        torIpv4L3Protocol};
            fattree.linkDeviceState[torDevice][serverDevice] = true;
            fattree.linkDeviceState[serverDevice][torDevice] = true;
            ipv4AddressHelper.NewNetwork();
        }
    }

    // links in every pod
    for (GroupID group_index = 0; group_index < fattree.number_of_pod; ++group_index)
    {
        for (SwitchID switch_index1 = 0; switch_index1 < fattree.number_of_edge_switches_per_pod;
             ++switch_index1)
        { // tor1 means edge switch
            auto tor1 = fattree.edge_switches.Get(
                group_index * fattree.number_of_edge_switches_per_pod + switch_index1);
            for (SwitchID switch_index2 = 0;
                 switch_index2 < fattree.number_of_aggre_switches_per_pod;
                 ++switch_index2)
            { // tor2 means aggre switch
                auto tor2 = fattree.aggre_switches.Get(
                    group_index * fattree.number_of_aggre_switches_per_pod + switch_index2);
                auto tor1Ipv4 = tor1->GetObject<ns3::Ipv4>();
                auto tor2Ipv4 = tor2->GetObject<ns3::Ipv4>();
                auto tor1Ipv4L3Protocol = tor1->GetObject<ns3::Ipv4L3Protocol>();
                auto tor2Ipv4L3Protocol = tor2->GetObject<ns3::Ipv4L3Protocol>();
                auto devices = p2pHelper.Install(tor1, tor2);
                auto [tor1Device, tor2Device] = ContainerPattern<2>(devices);
                auto addresses = ipv4AddressHelper.Assign(devices);
                auto [tor1Address, tor2Address] =
                    ContainerPattern<2>(addresses,
                                        [](const ns3::Ipv4InterfaceContainer& c,
                                           std::size_t offset) { return c.GetAddress(offset); });
                fattree.backgroundLinkMap[tor1->GetId()][tor2->GetId()] = {tor1Device,
                                                                           tor2Device,
                                                                           tor1Ipv4,
                                                                           tor2Ipv4,
                                                                           tor1Address,
                                                                           tor2Address,
                                                                           tor1Ipv4L3Protocol,
                                                                           tor2Ipv4L3Protocol};
                fattree.backgroundLinkMap[tor2->GetId()][tor1->GetId()] = {tor2Device,
                                                                           tor1Device,
                                                                           tor2Ipv4,
                                                                           tor1Ipv4,
                                                                           tor2Address,
                                                                           tor1Address,
                                                                           tor2Ipv4L3Protocol,
                                                                           tor1Ipv4L3Protocol};
                fattree.linkDeviceState[tor1Device][tor2Device] = true;
                fattree.linkDeviceState[tor2Device][tor1Device] = true;
                ipv4AddressHelper.NewNetwork();
            }
        }
    }

    // links between core and aggre
    for (SwitchID switch_index1 = 0; switch_index1 < fattree.number_of_core_switches;
         ++switch_index1)
    { // tor1 means core switch
        auto tor1 = fattree.core_switches.Get(switch_index1);
        for (GroupID group_index = 0; group_index < fattree.number_of_pod; ++group_index)
        {
            // tor2 means aggre switch
            auto switch_index2 = switch_index1 / fattree.number_of_aggre_switches_per_pod;
            auto tor2 = fattree.aggre_switches.Get(
                group_index * fattree.number_of_aggre_switches_per_pod + switch_index2);
            auto tor1Ipv4 = tor1->GetObject<ns3::Ipv4>();
            auto tor2Ipv4 = tor2->GetObject<ns3::Ipv4>();
            auto tor1Ipv4L3Protocol = tor1->GetObject<ns3::Ipv4L3Protocol>();
            auto tor2Ipv4L3Protocol = tor2->GetObject<ns3::Ipv4L3Protocol>();
            auto devices = p2pHelper.Install(tor1, tor2);
            auto [tor1Device, tor2Device] = ContainerPattern<2>(devices);
            auto addresses = ipv4AddressHelper.Assign(devices);
            auto [tor1Address, tor2Address] =
                ContainerPattern<2>(addresses,
                                    [](const ns3::Ipv4InterfaceContainer& c, std::size_t offset) {
                                        return c.GetAddress(offset);
                                    });
            fattree.backgroundLinkMap[tor1->GetId()][tor2->GetId()] = {tor1Device,
                                                                       tor2Device,
                                                                       tor1Ipv4,
                                                                       tor2Ipv4,
                                                                       tor1Address,
                                                                       tor2Address,
                                                                       tor1Ipv4L3Protocol,
                                                                       tor2Ipv4L3Protocol};
            fattree.backgroundLinkMap[tor2->GetId()][tor1->GetId()] = {tor2Device,
                                                                       tor1Device,
                                                                       tor2Ipv4,
                                                                       tor1Ipv4,
                                                                       tor2Address,
                                                                       tor1Address,
                                                                       tor2Ipv4L3Protocol,
                                                                       tor1Ipv4L3Protocol};
            fattree.linkDeviceState[tor1Device][tor2Device] = true;
            fattree.linkDeviceState[tor2Device][tor1Device] = true;
            ipv4AddressHelper.NewNetwork();
        }
    }

    // 检查fattree拓扑是否正确

    for (auto iterator = fattree.servers.Begin(); iterator < fattree.servers.End(); ++iterator)
    {
        auto node = *iterator;
        NS_ASSERT_MSG(
            node->GetNDevices() == 2,
            "network devices of server are not installed correctly"); // with an additional
                                                                      // loopback device
    }

    for (auto iterator = fattree.core_switches.Begin(); iterator < fattree.core_switches.End();
         ++iterator)
    {
        auto node = *iterator;
        NS_ASSERT_MSG(node->GetNDevices() == k + 1,
                      "network devices of tor are not installed correctly"); // with an additional
                                                                             // loopback device
    }

    for (auto iterator = fattree.aggre_switches.Begin(); iterator < fattree.aggre_switches.End();
         ++iterator)
    {
        auto node = *iterator;
        NS_ASSERT_MSG(node->GetNDevices() == k + 1,
                      "network devices of tor are not installed correctly"); // with an additional
                                                                             // loopback device
    }

    for (auto iterator = fattree.edge_switches.Begin(); iterator < fattree.edge_switches.End();
         ++iterator)
    {
        auto node = *iterator;
        NS_ASSERT_MSG(node->GetNDevices() == k + 1,
                      "network devices of tor are not installed correctly"); // with an additional
                                                                             // loopback device
    }

    auto backgroundLinkCount = (fattree.switches.GetN() * k + fattree.servers.GetN() * 1);

    NS_ASSERT_MSG(fattree.serverMap.size() == fattree.number_of_servers, "conflict server ports");

    auto backgroundLinkCountRange =
        fattree.backgroundLinkMap | std::views::values |
        std::views::transform(&std::unordered_map<NS3NodeID, LinkInfo>::size);

    NS_ASSERT_MSG(std::accumulate(backgroundLinkCountRange.begin(),
                                  backgroundLinkCountRange.end(),
                                  (std::size_t)0) == backgroundLinkCount,
                  "conflict background links");

    // // initialize GroupLinkNumberMap
    // for (GroupID i = 0; i < fattree.number_of_pod; i++)
    // {
    //     for (LinkID j = 0; j < fattree.number_of_pod; j++)
    //     {
    //         if (i != j)
    //         {
    //             fattree.groupLinkNumberMap[i][j] = 1;
    //         }
    //     }
    // }

    // 从tracefile中读取batch trace和net-jit trace
    // 读取 batch trace
    std::vector<ns3::DMLBatchTrace<int64_t, std::nano>> batch_traces =
        ns3::parse_DML_batch_traces(tracePath);
    // 读取 net_jit trace
    ns3::DMLWithNetJITTrace<int64_t, std::nano> traces =
        ns3::parse_net_jit_traces(measure_traceFilePath, batch_traces, 151, 171);
    // 读取 batch trace
    std::vector<ns3::DMLBatchTrace<int64_t, std::nano>> batch_traces2 =
        ns3::parse_DML_batch_traces(tracePath2);
    // 读取 net_jit trace
    ns3::DMLWithNetJITTrace<int64_t, std::nano> traces2 =
        ns3::parse_net_jit_traces(measure_traceFilePath2, batch_traces2, 151, 171);

    // 判断是否有prediction callbacks(没有)
    PredictionCallback prediction_callback;
    if (measure_method == "net-jit")
    {
        prediction_callback = [&fattree](ns3::MPIRankIDType rank,
                                         ns3::BatchID batch,
                                         TimeType time,
                                         ns3::SizeType size) {
            // fattree.prediction_callback_with_hedera(
            //     rank,
            //     batch,
            //     time,
            //     size); // 在 Lambda 中调用 prediction_callback 函数
        };
    }
    else
    {
        prediction_callback = [&fattree](ns3::MPIRankIDType rank,
                                         ns3::BatchID batch,
                                         TimeType time,
                                         ns3::SizeType size) {
            // do nothing
        };
    }

    // initialize batch communication callbacks
    BatchCallback batch_communication_start_callback1 = [&fattree](MPIRankIDType id,
                                                                   BatchID batch_id) {
        record_into_file("batch_communication_start1.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(id) + " " + std::to_string(batch_id));
    };

    BatchCallback batch_communication_end_callback1 = [&fattree](MPIRankIDType id,
                                                                 BatchID batch_id) {
        record_into_file("batch_communication_end1.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(id) + " " + std::to_string(batch_id));
    };

    BatchCallback batch_communication_start_callback2 = [&fattree](MPIRankIDType id,
                                                                   BatchID batch_id) {
        record_into_file("batch_communication_start2.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(id) + " " + std::to_string(batch_id));
    };

    BatchCallback batch_communication_end_callback2 = [&fattree](MPIRankIDType id,
                                                                 BatchID batch_id) {
        record_into_file("batch_communication_end2.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(id) + " " + std::to_string(batch_id));
    };

    // ideal: batch communication callbacks
    if (measure_method == "ideal")
    {
        batch_communication_start_callback1 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_start1.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            fattree.optimize_network_with_hedera(fattree.flows1,
                                                 1,
                                                 batch_id,
                                                 TimeType{ns3::Simulator::Now().GetNanoSeconds()});
        };
        batch_communication_end_callback1 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_end1.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            fattree.restore_network_with_hedera(fattree.flows1,
                                                1,
                                                batch_id,
                                                TimeType{ns3::Simulator::Now().GetNanoSeconds()});
        };
        batch_communication_start_callback2 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_start2.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            fattree.optimize_network_with_hedera(fattree.flows2,
                                                 2,
                                                 batch_id,
                                                 TimeType{ns3::Simulator::Now().GetNanoSeconds()});
        };
        batch_communication_end_callback2 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_end2.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            fattree.restore_network_with_hedera(fattree.flows2,
                                                2,
                                                batch_id,
                                                TimeType{ns3::Simulator::Now().GetNanoSeconds()});
        };
    }

    // net-jit: batch communication callbacks
    if (measure_method == "net-jit")
    {
        batch_communication_start_callback1 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_start1.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            std::random_device rd;
            std::mt19937 gen(rd());
            // 正态分布，para=（均值，标准差）
            std::normal_distribution<> distr(0, 16378697);
            int random_delay = abs(distr(gen));
            ns3::Simulator::Schedule(
                NanoSeconds(random_delay),
                &FattreeTopology::optimize_network_with_hedera,
                &fattree,
                fattree.flows1,
                1,
                batch_id,
                TimeType{ns3::Simulator::Now().GetNanoSeconds() + random_delay});
        };
        batch_communication_end_callback1 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_end1.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            std::random_device rd;
            std::mt19937 gen(rd());
            // 正态分布，para=（均值，标准差）
            std::normal_distribution<> distr(0, 16378697);
            int random_delay = abs(distr(gen));
            ns3::Simulator::Schedule(
                NanoSeconds(random_delay),
                &FattreeTopology::restore_network_with_hedera,
                &fattree,
                fattree.flows1,
                1,
                batch_id,
                TimeType{ns3::Simulator::Now().GetNanoSeconds() + random_delay});
        };
        batch_communication_start_callback2 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_start2.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            std::random_device rd;
            std::mt19937 gen(rd());
            // 正态分布，para=（均值，标准差）
            std::normal_distribution<> distr(0, 16378697);
            int random_delay = abs(distr(gen));
            ns3::Simulator::Schedule(
                NanoSeconds(random_delay),
                &FattreeTopology::optimize_network_with_hedera,
                &fattree,
                fattree.flows2,
                2,
                batch_id,
                TimeType{ns3::Simulator::Now().GetNanoSeconds() + random_delay});
        };
        batch_communication_end_callback2 = [&fattree](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_end2.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));
            std::random_device rd;
            std::mt19937 gen(rd());
            // 正态分布，para=（均值，标准差）
            std::normal_distribution<> distr(0, 16378697);
            int random_delay = abs(distr(gen));
            ns3::Simulator::Schedule(
                NanoSeconds(random_delay),
                &FattreeTopology::restore_network_with_hedera,
                &fattree,
                fattree.flows2,
                2,
                batch_id,
                TimeType{ns3::Simulator::Now().GetNanoSeconds() + random_delay});
        };
    }

    // static_window: batch communication callbacks
    if (measure_method == "static_window")
    {
        batch_communication_start_callback1 = [&fattree,
                                               &static_window_advance_time,
                                               &traces](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_start1.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));

            int64_t start_time =
                ceil(1.0 * ns3::Simulator::Now().GetNanoSeconds() / static_window_advance_time) *
                static_window_advance_time;

            ns3::Simulator::Schedule(
                NanoSeconds(start_time - 1.0 * ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::prediction_callback_with_hedera_multiapp,
                &fattree,
                id, // nodeRankID
                batch_id,
                TimeType{start_time},
                traces.predictions[0].size,
                1);
        };
        batch_communication_start_callback2 = [&fattree,
                                               &static_window_advance_time,
                                               &traces2](MPIRankIDType id, BatchID batch_id) {
            record_into_file("batch_communication_start2.txt",
                             std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                 std::to_string(id) + " " + std::to_string(batch_id));

            int64_t start_time =
                ceil(1.0 * ns3::Simulator::Now().GetNanoSeconds() / static_window_advance_time) *
                static_window_advance_time;

            ns3::Simulator::Schedule(
                NanoSeconds(start_time - 1.0 * ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::prediction_callback_with_hedera_multiapp,
                &fattree,
                id, // nodeRankID
                batch_id,
                TimeType{start_time},
                traces2.predictions[0].size,
                2);
        };
    }

    /* 非侵入式测量平均batch时长 */
    vector<int> batches_comm_start_nano1 = {
        4457133,    86834974,   171625782,  255933916,  340688495,  425139798,  509893210,
        594542783,  678614951,  763361545,  848091411,  932157613,  1016564517, 1101244068,
        1183584594, 1420443496, 1506254334, 1591726722, 1677028344, 1762009515};

    auto ten_average_batch_time_length1 =
        (batches_comm_start_nano1[10] - batches_comm_start_nano1[0]) / 10;

    vector<int> batches_comm_start_nano2 = {
        4457133,    86834974,   171625782,  255933916,  340688495,  425139798,  509893210,
        594542783,  678614951,  763361545,  848091411,  932157613,  1016564517, 1101244068,
        1183584594, 1420443496, 1506254334, 1591726722, 1677028344, 1762009515};

    auto ten_average_batch_time_length2 =
        (batches_comm_start_nano2[10] - batches_comm_start_nano2[0]) / 10;

    // slide_window: batch communication callbacks
    if (measure_method == "slide_window")
    {
        batch_communication_start_callback1 =
            [&fattree, &traces, &ten_average_batch_time_length1](ns3::MPIRankIDType rankid,
                                                                 ns3::BatchID batchid) {
                record_into_file("batch_communication_start1.txt",
                                 std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                     std::to_string(rankid) + " " + std::to_string(batchid));

                ns3::Simulator::Schedule(NanoSeconds((int64_t)ten_average_batch_time_length1),
                                         &FattreeTopology::prediction_callback_with_hedera_multiapp,
                                         &fattree,
                                         rankid,
                                         batchid + 1,
                                         TimeType{ns3::Simulator::Now().GetNanoSeconds() +
                                                  (int64_t)ten_average_batch_time_length1},
                                         traces.predictions[0].size,
                                         1);
            };

        batch_communication_start_callback2 =
            [&fattree, &traces2, &ten_average_batch_time_length2](ns3::MPIRankIDType rankid,
                                                                  ns3::BatchID batchid) {
                record_into_file("batch_communication_start2.txt",
                                 std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                                     std::to_string(rankid) + " " + std::to_string(batchid));

                ns3::Simulator::Schedule(NanoSeconds((int64_t)ten_average_batch_time_length2),
                                         &FattreeTopology::prediction_callback_with_hedera_multiapp,
                                         &fattree,
                                         rankid,
                                         batchid + 1,
                                         TimeType{ns3::Simulator::Now().GetNanoSeconds() +
                                                  (int64_t)ten_average_batch_time_length2},
                                         traces2.predictions[0].size,
                                         2);
            };
    }

    // 应用1的调用函数定义
    ns3::DMLApplicationHelper<int64_t, std::nano>::Callbacks callbacks1;
    {
        callbacks1.prediction_callback = prediction_callback;
        callbacks1.batch_start_callback = batch_start_callback;
        callbacks1.batch_end_callback = batch_end_callback;
        callbacks1.batch_communication_start_callback = batch_communication_start_callback1;
        callbacks1.batch_communication_end_callback = batch_communication_end_callback1;
        callbacks1.application_start_callback = application_start_callback;
        callbacks1.application_end_callback = [&application_end_callback_count,
                                               &fattree](ns3::MPIRankIDType rank) -> void {
            application_end_callback_count++;
            application_end_callback(rank);
            // 只有16个节点停止才会停止仿真，注意需要修改！
            if (application_end_callback_count == 16)
            {
                ns3::Simulator::Stop(ns3::Simulator::Now() + Seconds(0.1));
            }
        };
    }
    // 应用2的调用函数定义
    ns3::DMLApplicationHelper<int64_t, std::nano>::Callbacks callbacks2;
    {
        callbacks2.prediction_callback = prediction_callback;
        callbacks2.batch_start_callback = batch_start_callback;
        callbacks2.batch_end_callback = batch_end_callback;
        callbacks2.batch_communication_start_callback = batch_communication_start_callback2;
        callbacks2.batch_communication_end_callback = batch_communication_end_callback2;
        callbacks2.application_start_callback = application_start_callback;
        callbacks2.application_end_callback = [&application_end_callback_count,
                                               &fattree](ns3::MPIRankIDType rank) -> void {
            application_end_callback_count++;
            application_end_callback(rank);
            // 只有16个节点停止才会停止仿真，注意需要修改！
            if (application_end_callback_count == 16)
            {
                ns3::Simulator::Stop(ns3::Simulator::Now() + Seconds(0.1));
            }
        };
    }

    // 安装dmlapplication
    ns3::DMLApplicationHelper<int64_t, std::nano> dmlappHelper1(traces);
    ns3::DMLApplicationHelper<int64_t, std::nano> dmlappHelper2(traces2);
    // 乱序安装节点
    std::vector<NS3NodeID> nodeIndex2rankID(fattree.number_of_servers);
    for (int node_index = 0; node_index < (int)fattree.number_of_servers; ++node_index)
    {
        nodeIndex2rankID[node_index] = node_index;
    }
    std::shuffle(nodeIndex2rankID.begin(), nodeIndex2rankID.end(), std::default_random_engine());
    for (int node_index = 0; node_index < (int)fattree.number_of_servers; ++node_index)
    {
        fattree.nodeIndex2rankID[node_index] = nodeIndex2rankID[node_index];
    }
    for (int node_index = 0; node_index < (int)fattree.number_of_servers; ++node_index)
    {
        fattree.rankID2nodeIndex[nodeIndex2rankID[node_index]] = node_index;
    }
    for (int rankID = 0; rankID < 8; ++rankID)
    {
        int next_rankID = (rankID + 1) % 8; // 一个应用8个rank
        auto flow = make_pair(fattree.servers.Get(fattree.rankID2nodeIndex[rankID])->GetId(),
                              fattree.servers.Get(fattree.rankID2nodeIndex[next_rankID])->GetId());
        fattree.flows1.push_back(flow);
    }
    for (int rankID = 8; rankID < 16; ++rankID)
    {
        int next_rankID = (rankID + 1) % 8 + 8; // 一个应用8个rank
        auto flow = make_pair(fattree.servers.Get(fattree.rankID2nodeIndex[rankID])->GetId(),
                              fattree.servers.Get(fattree.rankID2nodeIndex[next_rankID])->GetId());
        fattree.flows2.push_back(flow);
    }
    for (int rankID = 0; rankID < 8; ++rankID)
    {
        auto node = fattree.servers.Get(fattree.rankID2nodeIndex[rankID]);
        auto ip = fattree.serverMap[node->GetId()].address;
        dmlappHelper1.install_node(node, rankID, InetSocketAddress{ip, 1000});
    }
    for (int rankID = 8; rankID < 16; ++rankID)
    {
        auto node = fattree.servers.Get(fattree.rankID2nodeIndex[rankID]);
        auto ip = fattree.serverMap[node->GetId()].address;
        dmlappHelper2.install_node(node, rankID - 8, InetSocketAddress{ip, 1000});
    }

    // rankID 0 is the master
    // install dmlapplication
    ns3::MPIRankIDType masterRankID = 0;
    // 不同模型，不同delta
    TimeType delta1 = TimeType{0};
    TimeType delta2 = TimeType{0};
    if (tracePath.find("resnet152") != std::string::npos)
        delta1 = TimeType{500000};
    else
        delta1 = TimeType{3000000};
    if (tracePath2.find("resnet152") != std::string::npos)
        delta2 = TimeType{500000};
    else
        delta2 = TimeType{3000000};
    std::default_random_engine random_engine1;
    std::default_random_engine random_engine2;
    auto distribute_generator1 =
        ns3::DMLApplicationHelper<int64_t, std::nano>::normal_distributed_disturbance(
            delta1,
            std::move(random_engine1));
    auto distribute_generator2 =
        ns3::DMLApplicationHelper<int64_t, std::nano>::normal_distributed_disturbance(
            delta2,
            std::move(random_engine2));
    ns3::ApplicationContainer applications1 = dmlappHelper1.install(masterRankID,
                                                                    callbacks1,
                                                                    distribute_generator1,
                                                                    maxBytes,
                                                                    scale_factor);
    ns3::ApplicationContainer applications2 = dmlappHelper2.install(masterRankID,
                                                                    callbacks2,
                                                                    distribute_generator2,
                                                                    maxBytes,
                                                                    scale_factor);

    // set app start time
    applications1.Start(ns3::Seconds(start_time));
    applications1.Start(ns3::Seconds(start_time));

    // 统计吞吐量曲线
    ns3::Simulator::Schedule(ns3::Seconds((double)0.1), record_throughput_curve, applications1);
    ns3::Simulator::Schedule(ns3::Seconds((double)0.1), record_throughput_curve, applications2);

    // set aggre/edge/core SwitchID2Index
    {
        for (auto i = 0; i < (int)fattree.number_of_aggre_switches; ++i)
        {
            fattree.aggreSwitchID2Index[fattree.aggre_switches.Get(i)->GetId()] = i;
        }
        for (auto i = 0; i < (int)fattree.number_of_edge_switches; ++i)
        {
            fattree.edgeSwitchID2Index[fattree.edge_switches.Get(i)->GetId()] = i;
        }
        for (auto i = 0; i < (int)fattree.number_of_core_switches; ++i)
        {
            fattree.coreSwitchID2Index[fattree.core_switches.Get(i)->GetId()] = i;
        }
    }

    ns3::Ptr<ns3::FlowMonitor> flowMonitor;
    ns3::FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();
        if (fattree.hederaRoutingHelper)
            fattree.hederaRoutingHelper->notifyLinkChanges();
    ns3::Simulator::Schedule(ns3::Seconds(start_time),
                             &ns3::Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    std::cout << "Run..." << std::endl;
    ns3::Simulator::Run();
    std::cout << "Run done." << std::endl;
    std::cout << "totalHops: " << fattree.totalHops << std::endl;
    flowMonitor->SerializeToXmlFile("flow.xml", true, true);
    ns3::Simulator::Destroy();
    std::cout << "Done." << std::endl;
    return 0;
}
