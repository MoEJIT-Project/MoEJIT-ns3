//
// Created by Zoeyzyy on 2024/4/29.
//

#include "dml-application.h"
#include "net-jit-application.h"
#include "topology/dragonfly-topology.h"
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
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>

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
batch_communication_start_callback(MPIRankIDType id, BatchID batch_id)
{
    record_into_file("batch_communication_start.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(id) + " " + std::to_string(batch_id));
}

void
batch_communication_end_callback(MPIRankIDType id, BatchID batch_id)
{
    record_into_file("batch_communication_end.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(id) + " " + std::to_string(batch_id));
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
    std::string tracePath = "../scratch/net-jit/traces/resnet152";
    std::string measure_traceFilePath = "../scratch/net-jit/net-jit-traces/resnet152/1.txt";

    float start_time = 0.0;
    float stop_time = 5000;

    std::size_t k = 2;
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
    double scale_factor = 0.00001;
    int64_t optimization_delay = 100000000; // ns
    std::string measure_method =
        "net-jit"; // "net-jit" or "static_window" or "slide_window" or "none"
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
    cmd.AddValue(
        "k",
        "the dragonfly parameter k, represents the number of the ports per switch in the topology",
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

    // create dragonfly topology
    auto p = k;
    auto a = 2 * k;
    auto h = k;
    auto g = a * h + 1;
    NS_ASSERT_MSG((a * h) % (g - 1) == 0, "invalid group count");
    auto switchCount = g * a;
    auto serverCount = switchCount * p;
    auto backgroundLinkCount =
        switchCount * (a + h - 1) +
        serverCount *
            2; // server-tor links + links in every group + links between groups (simplex links)

    // change bandwidth
    bandwidth = std::to_string(scale_factor * 100.0) + "Gbps";
    if (bandwidth_opt.find("4") != std::string::npos)
        bandwidth_opt = std::to_string(scale_factor * 400.0) + "Gbps";
    else
        bandwidth_opt = std::to_string(scale_factor * 100.0) + "Gbps";

    // initialize topology
    DragonflyTopology dragonfly(p,
                                a,
                                h,
                                g,
                                bandwidth,
                                delay,
                                stop_time,
                                ecmp,
                                app_bd,
                                bandwidth_opt,
                                scale_factor,
                                optimization_delay);

    dragonfly.switches.Create(switchCount);
    dragonfly.servers.Create(serverCount);
    dragonfly.allNodes.Add(dragonfly.switches);
    dragonfly.allNodes.Add(dragonfly.servers);

    // install routing
    // default: min
    // install internet stack
    dragonfly.internetStackHelper.Install(dragonfly.allNodes);

    // set p2p link parameters
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue(bandwidth));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue(delay));

    // set ipv4 address
    ns3::Ipv4AddressHelper ipv4AddressHelper{"10.0.0.0", "255.255.255.0"};

    // background links
    // server-tor links
    for (SwitchID switch_index = 0; switch_index < switchCount; ++switch_index)
    {
        auto tor = dragonfly.switches.Get(switch_index);
        for (ServerID server_index = 0; server_index < p; ++server_index)
        {
            auto server = dragonfly.servers.Get(switch_index * p + server_index);
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
            dragonfly.serverMap[server->GetId()] = {serverDevice, serverAddress};
            dragonfly.backgroundLinkMap[tor->GetId()][server->GetId()] = {torDevice,
                                                                          serverDevice,
                                                                          torIpv4,
                                                                          serverIpv4,
                                                                          torAddress,
                                                                          serverAddress,
                                                                          torIpv4L3Protocol,
                                                                          serverIpv4L3Protocol};
            dragonfly.backgroundLinkMap[server->GetId()][tor->GetId()] = {serverDevice,
                                                                          torDevice,
                                                                          serverIpv4,
                                                                          torIpv4,
                                                                          serverAddress,
                                                                          torAddress,
                                                                          serverIpv4L3Protocol,
                                                                          torIpv4L3Protocol};
            dragonfly.linkDeviceState[torDevice][serverDevice] = true;
            dragonfly.linkDeviceState[serverDevice][torDevice] = true;
            ipv4AddressHelper.NewNetwork();
        }
    }
    // links in every group
    for (GroupID group_index = 0; group_index < g; ++group_index)
    {
        for (SwitchID switch_index1 = 0; switch_index1 < a; ++switch_index1)
        {
            auto tor1 = dragonfly.switches.Get(group_index * a + switch_index1);
            for (SwitchID switch_index2 = switch_index1 + 1; switch_index2 < a; ++switch_index2)
            {
                auto tor2 = dragonfly.switches.Get(group_index * a + switch_index2);
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
                dragonfly.backgroundLinkMap[tor1->GetId()][tor2->GetId()] = {tor1Device,
                                                                             tor2Device,
                                                                             tor1Ipv4,
                                                                             tor2Ipv4,
                                                                             tor1Address,
                                                                             tor2Address,
                                                                             tor1Ipv4L3Protocol,
                                                                             tor2Ipv4L3Protocol};
                dragonfly.backgroundLinkMap[tor2->GetId()][tor1->GetId()] = {tor2Device,
                                                                             tor1Device,
                                                                             tor2Ipv4,
                                                                             tor1Ipv4,
                                                                             tor2Address,
                                                                             tor1Address,
                                                                             tor2Ipv4L3Protocol,
                                                                             tor1Ipv4L3Protocol};
                dragonfly.linkDeviceState[tor1Device][tor2Device] = true;
                dragonfly.linkDeviceState[tor2Device][tor1Device] = true;
                ipv4AddressHelper.NewNetwork();
            }
        }
    }
    // links between groups, every group connect to groups whose ID is less than its
    for (GroupID group_index1 = 0; group_index1 < g; ++group_index1)
    {
        for (SwitchID switch_index1 = 0; switch_index1 < a; ++switch_index1)
        {
            auto tor1 = dragonfly.switches.Get(group_index1 * a + switch_index1);
            for (LinkID link_index = 0; link_index < h; ++link_index)
            {
                auto offset = (switch_index1 * h + link_index) % (g - 1) + 1;
                auto group_index2 = (g + group_index1 - offset) % g;
                if (group_index2 >= group_index1)
                {
                    continue;
                }
                auto switch_index2 = a - switch_index1 - 1; // symmetric switch placement
                auto tor2 = dragonfly.switches.Get(group_index2 * a + switch_index2);
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
                dragonfly.backgroundLinkMap[tor1->GetId()][tor2->GetId()] = {tor1Device,
                                                                             tor2Device,
                                                                             tor1Ipv4,
                                                                             tor2Ipv4,
                                                                             tor1Address,
                                                                             tor2Address,
                                                                             tor1Ipv4L3Protocol,
                                                                             tor2Ipv4L3Protocol};
                dragonfly.backgroundLinkMap[tor2->GetId()][tor1->GetId()] = {tor2Device,
                                                                             tor1Device,
                                                                             tor2Ipv4,
                                                                             tor1Ipv4,
                                                                             tor2Address,
                                                                             tor1Address,
                                                                             tor2Ipv4L3Protocol,
                                                                             tor1Ipv4L3Protocol};
                dragonfly.linkDeviceState[tor1Device][tor2Device] = true;
                dragonfly.linkDeviceState[tor2Device][tor1Device] = true;
                ipv4AddressHelper.NewNetwork();
            }
        }
    }
    for (auto iterator = dragonfly.servers.Begin(); iterator < dragonfly.servers.End(); ++iterator)
    {
        auto node = *iterator;
        NS_ASSERT_MSG(
            node->GetNDevices() == 2,
            "network devices of server are not installed correctly"); // with an additional loopback
                                                                      // device
    }

    for (auto iterator = dragonfly.switches.Begin(); iterator < dragonfly.switches.End();
         ++iterator)
    {
        auto node = *iterator;
        NS_ASSERT_MSG(node->GetNDevices() == p + a + h,
                      "network devices of tor are not installed correctly"); // with an additional
                                                                             // loopback device
    }

    NS_ASSERT_MSG(dragonfly.serverMap.size() == serverCount, "conflict server ports");
    auto backgroundLinkCountRange =
        dragonfly.backgroundLinkMap | std::views::values |
        std::views::transform(&std::unordered_map<NS3NodeID, LinkInfo>::size);
    NS_ASSERT_MSG(std::accumulate(backgroundLinkCountRange.begin(),
                                  backgroundLinkCountRange.end(),
                                  (std::size_t)0) == backgroundLinkCount,
                  "conflict background links");

    // initialize GroupLinkNumberMap
    for (GroupID i = 0; i < dragonfly.g; i++)
    {
        for (LinkID j = 0; j < dragonfly.g; j++)
        {
            if (i != j)
            {
                dragonfly.groupLinkNumberMap[i][j] = 1;
            }
        }
    }

    // 读取 batch trace
    std::vector<ns3::DMLBatchTrace<int64_t, std::nano>> batch_traces =
        ns3::parse_DML_batch_traces(tracePath);
    // 读取 net_jit trace
    ns3::DMLWithNetJITTrace<int64_t, std::nano> traces =
        ns3::parse_net_jit_traces(measure_traceFilePath, batch_traces, 151, 171);

    // 判断是否有预测函数
    PredictionCallback prediction_callback;
    if (measure_method == "net-jit")
    {
        prediction_callback = [&dragonfly](ns3::MPIRankIDType rank,
                                           ns3::BatchID batch,
                                           TimeType time,
                                           ns3::SizeType size) {
            dragonfly.prediction_callback(rank,
                                          batch,
                                          time,
                                          size); // 在 Lambda 中调用 prediction_callback 函数
        };
    }
    else
    {
        prediction_callback = [&dragonfly](ns3::MPIRankIDType rank,
                                           ns3::BatchID batch,
                                           TimeType time,
                                           ns3::SizeType size) {
            // do nothing
        };
    }

    ns3::DMLApplicationHelper<int64_t, std::nano>::Callbacks callbacks;
    callbacks.prediction_callback = prediction_callback;
    callbacks.batch_start_callback = batch_start_callback;
    callbacks.batch_end_callback = batch_end_callback;
    callbacks.batch_communication_start_callback = batch_communication_start_callback;
    callbacks.batch_communication_end_callback = batch_communication_end_callback;
    callbacks.application_start_callback = application_start_callback;
    callbacks.application_end_callback = [&application_end_callback_count,
                                          &serverCount](ns3::MPIRankIDType rank) -> void {
        application_end_callback_count++;
        application_end_callback(rank);
        if (application_end_callback_count == serverCount)
        {
            ns3::Simulator::Stop(ns3::Simulator::Now() + Seconds(0.1));
        }
    };

    // 非侵入式测量，设置网络优化
    if (measure_method == "static_window")
    {
        for (auto& batch : traces.batches)
        {
            ns3::Simulator::Schedule(
                NanoSeconds(ceil(1.0 * batch.communication_start_time.count() /
                                 static_window_advance_time) *
                            static_window_advance_time) -
                    convert(traces.offset),
                &DragonflyTopology::prediction_callback,
                &dragonfly,
                0, // nodeRankID
                batch.batch,
                TimeType{batch.communication_start_time.count() + static_window_advance_time} -
                    traces.offset,
                traces.predictions[0].size);
        }
    }

    // 机器学习测量，设置网络优化
    if (measure_method == "slide_window")
    {
        NS_ASSERT_MSG(batch_traces.size() >= 10, "batch_traces.size() < 10");
        auto ten_average_batch_time_length =
            (batch_traces[9].batch_end_time.count() - batch_traces[0].batch_start_time.count()) /
            10;
        for (auto i = 0; i < (int)traces.batches.size(); i++)
        {
            if (i != 0)
            {
                ns3::Simulator::Schedule(
                    NanoSeconds(traces.batches[i - 1].batch_end_time.count()) -
                        convert(traces.offset),
                    &DragonflyTopology::prediction_callback,
                    &dragonfly,
                    0,
                    traces.batches[i].batch,
                    TimeType{traces.batches[i - 1].communication_start_time.count() +
                             (int64_t)(ten_average_batch_time_length)} -
                        traces.offset,
                    traces.predictions[0].size);
            }
        }
    }

    // 安装dmlapplication
    ns3::DMLApplicationHelper<int64_t, std::nano> dmlappHelper(traces);

    for (ns3::MPIRankIDType rankID = 0; rankID < serverCount; ++rankID)
    {
        auto node = dragonfly.servers.Get(rankID);
        auto ip = dragonfly.serverMap[node->GetId()].address;
        dmlappHelper.install_node(node, rankID, InetSocketAddress{ip, 1000});
    }

    // rankID 0 is the master
    // install dmlapplication
    ns3::MPIRankIDType masterRankID = 0;
    // 不同模型，不同delta
    TimeType delta;
    if (tracePath.find("resnet152") != std::string::npos)
        delta = TimeType{500000};
    else
        delta = TimeType{3000000};
    std::default_random_engine random_engine;
    auto distribute_generator =
        ns3::DMLApplicationHelper<int64_t, std::nano>::normal_distributed_disturbance(
            delta,
            std::move(random_engine));
    ns3::ApplicationContainer applications =
        dmlappHelper.install(masterRankID, callbacks, distribute_generator, maxBytes, scale_factor);

    // set app time
    applications.Start(ns3::Seconds(start_time));
    // applications.Stop(ns3::Seconds(stop_time));

    // 统计吞吐量曲线
    ns3::Simulator::Schedule(ns3::Seconds((double)0.1), record_throughput_curve, applications);

    // set SwitchID2Index
    for (auto i = 0; i < (int)switchCount; ++i)
    {
        dragonfly.switchID2Index[dragonfly.switches.Get(i)->GetId()] = i;
    }

    ns3::Ptr<ns3::FlowMonitor> flowMonitor;
    ns3::FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    ns3::Simulator::Schedule(ns3::Seconds(start_time),
                             &ns3::Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    // applications.Start(ns3::Seconds(0));
    std::cout << "Run..." << std::endl;
    ns3::Simulator::Run();
    std::cout << "Run done." << std::endl;
    std::cout << "totalHops: " << dragonfly.totalHops << std::endl;
    flowMonitor->SerializeToXmlFile("flow.xml", true, true);
    ns3::Simulator::Destroy();
    std::cout << "Done." << std::endl;
    return 0;
}
