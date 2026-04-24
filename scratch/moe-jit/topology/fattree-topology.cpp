#include "fattree-topology.h"

#include <iostream>
#include <vector>

FattreeTopology::FattreeTopology(std::size_t k,
                                 std::string bandwidth,
                                 std::string delay,
                                 float stop_time,
                                 bool ecmp,
                                 std::string app_bd,
                                 std::string bandwidth_opt,
                                 double scale_factor,
                                 int64_t optimization_delay,
                                 std::optional<ns3::HederaRoutingHelper> hederaRoutingHelper)
{
    this->k = k;
    this->bandwidth = bandwidth;
    this->delay = delay;
    this->stop_time = stop_time;
    this->ecmp = ecmp;
    this->app_bd = app_bd;
    this->bandwidth_opt = bandwidth_opt;
    this->scale_factor = scale_factor;
    this->optimization_delay = std::chrono::duration<int64_t, std::nano>(optimization_delay);
    this->scale_factor = scale_factor;

    this->number_of_pod = k;
    this->number_of_aggre_switches_per_pod = k / 2;
    this->number_of_edge_switches_per_pod = k / 2;
    this->number_of_servers_per_edge_switch = (k / 2);
    this->number_of_servers_per_pod =
        number_of_servers_per_edge_switch * number_of_edge_switches_per_pod;
    this->number_of_core_switches = (k / 2) * (k / 2);
    this->number_of_aggre_switches = number_of_aggre_switches_per_pod * number_of_pod;
    this->number_of_edge_switches = number_of_edge_switches_per_pod * number_of_pod;
    this->number_of_servers = number_of_servers_per_pod * number_of_pod;

    this->hederaRoutingHelper = hederaRoutingHelper;
}

void
FattreeTopology::prediction_callback_with_hedera_multiapp(ns3::MPIRankIDType noderankID,
                                                          ns3::BatchID batchID,
                                                          TimeType comm_begin_time,
                                                          ns3::SizeType comm_size,
                                                          int app_rank)
{
    std::cout << "FattreeTopology::prediction_callback_with_hedera_multiapp: app rank=" << app_rank
              << std::endl;

    // AP/PM: 预计优化后的带宽，计算通信结束的时间
    auto bandwidth = 1000;
    auto comm_begin_time_nano = comm_begin_time.count();
    auto comm_end_time_nano =
        comm_begin_time_nano + 2 * (this->number_of_servers) * comm_size * 8 / bandwidth;

    // 根据app_rank选择不同的app flows
    std::vector<Flow> new_flows;
    if (app_rank == 1)
        new_flows = this->flows1;
    else if (app_rank == 2)
        new_flows = this->flows2;

    // 已经进入新的一轮，则进行新的重构，直至无需调整边
    if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
        batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
        batchCommsizeMap[batchID][comm_size][0].count() > comm_begin_time_nano)
    {
        std::cout << "FattreeTopology::prediction_callback_with_hedera" << noderankID << " "
                  << batchID << " " << comm_size << " " << comm_begin_time.count() << std::endl;
        if (comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds() >= 0)
        {
            ns3::Simulator::Schedule(
                NanoSeconds(comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::add_flows,
                this,
                new_flows,
                batchID,
                comm_size);
            ns3::Simulator::Schedule(
                NanoSeconds(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::clear_flows,
                this,
                new_flows,
                comm_size,
                batchID);
        }
        if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
            batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
            batchCommsizeMap[batchID][comm_size].size() == 0)
        {
            batchCommsizeMap[batchID][comm_size] = {comm_begin_time, TimeType{comm_end_time_nano}};
        }
        else
        {
            batchCommsizeMap[batchID][comm_size][0] = comm_begin_time;
        }
    }
    if (batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
        batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
        batchCommsizeMap[batchID][comm_size][1].count() < (int)comm_end_time_nano)
    {
        if (comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds() > 0)
        {
            batchCommsizeMap[batchID][comm_size][1] = TimeType{comm_end_time_nano};
            ns3::Simulator::Schedule(
                NanoSeconds(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::clear_flows,
                this,
                new_flows,
                comm_size,
                batchID);
        }
    }
}

void
FattreeTopology::prediction_callback_with_hedera(ns3::MPIRankIDType noderankID,
                                                 ns3::BatchID batchID,
                                                 TimeType comm_begin_time,
                                                 ns3::SizeType comm_size)
{
    // AP/PM: 预计优化后的带宽，计算通信结束的时间
    std::cout << "FattreeTopology::prediction_callback_with_hedera" << std::endl;
    auto bandwidth = 1000;
    auto comm_begin_time_nano = comm_begin_time.count();
    auto comm_end_time_nano =
        comm_begin_time_nano + 2 * (this->number_of_servers) * comm_size * 8 / bandwidth;

    std::vector<Flow> new_flows;
    for (int rankID = 0; rankID < (int)this->number_of_servers; ++rankID)
    {
        int next_rankID = (rankID + 1) % this->number_of_servers;
        auto flow = Flow(this->servers.Get(this->rankID2nodeIndex[rankID])->GetId(),
                         this->servers.Get(this->rankID2nodeIndex[next_rankID])->GetId());
        new_flows.push_back(flow);
    }

    // 已经进入新的一轮，则进行新的重构，直至无需调整边
    if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
        batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
        batchCommsizeMap[batchID][comm_size][0].count() > comm_begin_time_nano)
    {
        std::cout << "FattreeTopology::prediction_callback_with_hedera" << noderankID << " "
                  << batchID << " " << comm_size << " " << comm_begin_time.count() << std::endl;
        if (comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds() > 0)
        {
            ns3::Simulator::Schedule(
                NanoSeconds(comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::add_flows,
                this,
                new_flows,
                batchID,
                comm_size);
            ns3::Simulator::Schedule(
                NanoSeconds(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::clear_flows,
                this,
                new_flows,
                comm_size,
                batchID);
        }
        if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
            batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
            batchCommsizeMap[batchID][comm_size].size() == 0)
        {
            batchCommsizeMap[batchID][comm_size] = {comm_begin_time, TimeType{comm_end_time_nano}};
        }
        else
        {
            batchCommsizeMap[batchID][comm_size][0] = comm_begin_time;
        }
    }
    if (batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
        batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
        batchCommsizeMap[batchID][comm_size][1].count() < (int)comm_end_time_nano)
    {
        if (comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds() > 0)
        {
            batchCommsizeMap[batchID][comm_size][1] = TimeType{comm_end_time_nano};
            ns3::Simulator::Schedule(
                NanoSeconds(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::clear_flows,
                this,
                new_flows,
                comm_size,
                batchID);
        }
    }
}

void
FattreeTopology::optimize_network_with_hedera(std::vector<Flow> new_flows,
                                              ns3::SizeType comm_size,
                                              BatchID batchID,
                                              TimeType comm_begin_time)
{
    // ideal/net-jit: 触发优化
    std::cout << "FattreeTopology::optimize_network_with_hedera" << std::endl;

    auto comm_begin_time_nano = comm_begin_time.count();

    // 已经进入新的一轮，则进行新的重构，直至无需调整边
    if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
        batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
        batchCommsizeMap[batchID][comm_size][0].count() > comm_begin_time_nano)
    {
        std::cout << "FattreeTopology::optimize_network_with_hedera" << " " << batchID << " "
                  << comm_size << " " << comm_begin_time.count() << std::endl;
        if (comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds() >= 0)
        {
            ns3::Simulator::Schedule(
                NanoSeconds(comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &FattreeTopology::add_flows,
                this,
                new_flows,
                batchID,
                comm_size);
        }
        if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
            batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
            batchCommsizeMap[batchID][comm_size].size() == 0)
        {
            batchCommsizeMap[batchID][comm_size] = {comm_begin_time, TimeType{8}}; // !8次结束的调用
        }
        else
        {
            batchCommsizeMap[batchID][comm_size][0] = comm_begin_time;
        }
    }
}

void
FattreeTopology::restore_network_with_hedera(std::vector<Flow> new_flows,
                                             ns3::SizeType comm_size,
                                             BatchID batchID,
                                             TimeType comm_end_time)
{
    // ideal/net-jit: 结束优化
    std::cout << "FattreeTopology::restore_network_with_hedera" << std::endl;

    auto comm_end_time_nano = comm_end_time.count();

    // 已经进入新的一轮，则进行新的重构，直至无需调整边
    if (batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
        batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
        batchCommsizeMap[batchID][comm_size][1].count() > 0)
    {
        batchCommsizeMap[batchID][comm_size][1] =
            TimeType{batchCommsizeMap[batchID][comm_size][1].count() - 1};
        if (batchCommsizeMap[batchID][comm_size][1].count() == 0)
        {
            std::cout << "FattreeTopology::restore_network_with_hedera" << " " << batchID << " "
                      << comm_size << " " << comm_end_time_nano << std::endl;
            clear_flows(new_flows, comm_size, batchID);
        }
    }
}

void
FattreeTopology::add_flows(std::vector<Flow> new_flows, BatchID batchID, ns3::SizeType comm_size)
{
    auto curr_time_nano = ns3::Simulator::Now().GetNanoSeconds();
    if ((batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
         batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
         batchCommsizeMap[batchID][comm_size][0].count() == curr_time_nano) ||
        batchCommsizeMap.size() == 0) // 没有更早的优化开始时间 或者是 ideal
    {
        record_into_file("optimization.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(batchID) + " " + std::to_string(comm_size));
        if (batchCommsizeMap.size() == 0)
        {
            for (const auto& new_flow : this->flows1)
            {
                this->flows.push_back(new_flow);
            }
        }
        else
        {
            for (auto flow : new_flows)
            {
                this->flows.push_back(flow);
            }
        }
        // 输出this->flows, flow is pair
        for (const auto& flow : this->flows)
        {
            std::cout << flow.first << " " << flow.second << std::endl;
        }
        if (this->hederaRoutingHelper)
            this->hederaRoutingHelper->updateRoutes(this->flows);
        else
            cout << "Warning: HederaRoutingHelper optimized routes requested but not active." << endl;

        cout << "add_flows " << batchID << " " << comm_size << " "
             << ns3::Simulator::Now().GetNanoSeconds() << endl;
    }
}

void
FattreeTopology::clear_flows(std::vector<Flow> new_flows, ns3::SizeType comm_size, BatchID batchID)
{
    auto curr_time_nano = ns3::Simulator::Now().GetNanoSeconds();
    if (batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
        batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
        batchCommsizeMap[batchID][comm_size][1].count() > curr_time_nano)
    { // 有更晚的优化结束时间
        return;
    }
    bool changed = false;
    if (batchCommsizeMap.size() == 0)
    {
        for (const auto& new_flow : this->flows1)
        {
            auto it = std::find(this->flows.begin(), this->flows.end(), new_flow);
            if (it == this->flows.end())
            {
                this->flows.push_back(new_flow);
                changed = true;
            }
        }
    }
    else
    {
        for (Flow new_flow : new_flows)
        {
            auto it = std::find(this->flows.begin(), this->flows.end(), new_flow);
            if (it != this->flows.end())
            {
                this->flows.erase(it);
                changed = true;
            }
        }
    }
    if (changed == false)
        return;
    else
    {
        record_into_file("restore.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(batchID) + " " + std::to_string(comm_size));

        if (this->flows.size() == 0)
        {
            if (this->hederaRoutingHelper)
                this->hederaRoutingHelper->resetRoutes();
        }
        else
        {
            if (this->hederaRoutingHelper)
                this->hederaRoutingHelper->updateRoutes(this->flows);
        }
    }
}

void
FattreeTopology::prediction_callback(ns3::MPIRankIDType noderankID,
                                     ns3::BatchID batchID,
                                     TimeType comm_begin_time,
                                     ns3::SizeType comm_size)
{
    std::cout << "now: " << Simulator::Now() << ", noderankID: " << noderankID
              << ", batchID: " << batchID << ", comm_begin_time: " << comm_begin_time.count()
              << ", comm_size: " << comm_size << std::endl;
    // 优化带宽的数字表示, ！！不需要因为缩放而修改，因为comm_size没有缩放
    auto bandwidth_opt_number = 100;
    if (this->bandwidth_opt.find("4") != std::string::npos)
        bandwidth_opt_number = 400;
    else
        bandwidth_opt_number = 100;
    auto time_now_nano = ns3::Simulator::Now().GetNanoSeconds();
    // 预测通信开始/结束实践
    auto comm_begin_time_nano = comm_begin_time.count();
    // ns的1e9和Bbps的G相抵消
    auto comm_end_time_nano =
        comm_begin_time_nano + 2 * (this->number_of_servers) * comm_size * 8 / bandwidth_opt_number;
    // 优化延时
    auto time_limit_nano = time_now_nano + this->optimization_delay.count();
    // 完全超过优化延时
    if (comm_end_time_nano < (long unsigned int)time_limit_nano)
        return;
    else if (comm_begin_time_nano >= time_limit_nano)
    { // 完全未超过优化延时
        if (batchTimeMap.find(batchID) == batchTimeMap.end())
        {
            batchTimeMap[batchID].push_back(TimeType{comm_begin_time_nano});
            batchTimeMap[batchID].push_back(TimeType{comm_end_time_nano});
            ns3::Simulator::Schedule(NanoSeconds(comm_begin_time_nano - time_now_nano),
                                     &FattreeTopology::optimize_network,
                                     this,
                                     batchID);
            ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                     &FattreeTopology::restore_network,
                                     this,
                                     batchID);
        }
        else
        {
            auto ealiest_time_nano = batchTimeMap[batchID][0].count();
            auto latest_time_nano = batchTimeMap[batchID][1].count();
            if (ealiest_time_nano > comm_begin_time_nano)
            {
                batchTimeMap[batchID][0] = TimeType{comm_begin_time_nano};
                ns3::Simulator::Schedule(NanoSeconds(comm_begin_time_nano - time_now_nano),
                                         &FattreeTopology::optimize_network,
                                         this,
                                         batchID);
            }
            if ((long unsigned int)latest_time_nano < comm_end_time_nano)
            {
                batchTimeMap[batchID][1] = TimeType{comm_end_time_nano};
                ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                         &FattreeTopology::restore_network,
                                         this,
                                         batchID);
            }
        }
    }
    else
    { // 部分超过优化延时
        if (batchTimeMap.find(batchID) == batchTimeMap.end())
        {
            batchTimeMap[batchID].push_back(TimeType{time_limit_nano});
            batchTimeMap[batchID].push_back(TimeType{comm_end_time_nano});
            ns3::Simulator::Schedule(NanoSeconds(time_limit_nano - time_now_nano),
                                     &FattreeTopology::optimize_network,
                                     this,
                                     batchID);
            ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                     &FattreeTopology::restore_network,
                                     this,
                                     batchID);
        }
        else
        {
            auto ealiest_time_nano = batchTimeMap[batchID][0].count();
            auto latest_time_nano = batchTimeMap[batchID][1].count();
            if (ealiest_time_nano > time_limit_nano)
            {
                batchTimeMap[batchID][0] = TimeType{time_limit_nano};
                ns3::Simulator::Schedule(NanoSeconds(time_limit_nano - time_now_nano),
                                         &FattreeTopology::optimize_network,
                                         this,
                                         batchID);
            }
            if ((long unsigned int)latest_time_nano < comm_end_time_nano)
            {
                batchTimeMap[batchID][1] = TimeType{comm_end_time_nano};
                ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                         &FattreeTopology::restore_network,
                                         this,
                                         batchID);
            }
        }
    }
}

void
FattreeTopology::record_into_file(std::filesystem::path filename, std::string line)
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
FattreeTopology::optimize_network(ns3::BatchID batchID)
{
    auto time_now_nano = ns3::Simulator::Now().GetNanoSeconds();
    if (batchTimeMap.find(batchID) != batchTimeMap.end())
    { // batch有通信预测记录
        auto ealiest_time_nano = batchTimeMap[batchID][0].count();
        if (ealiest_time_nano < time_now_nano)
        {
            return;
        }
        else
        { // 只在batch的最早通信的时候优化
            if (is_bandwidth_opt)
            { // 优化带宽
                change_all_link_bandwidth(this->bandwidth_opt);
                record_into_file("optimize_timestamp.txt",
                                 std::to_string(batchID) + " " + std::to_string(time_now_nano));
            }
            else
            { // 优化延时
                change_all_link_delay(this->delay_opt);
            }
        }
    }
}

void
FattreeTopology::restore_network(ns3::BatchID batchID)
{
    auto time_now_nano = ns3::Simulator::Now().GetNanoSeconds();
    if (batchTimeMap.find(batchID) != batchTimeMap.end())
    { // batch有通信预测记录
        auto latest_time_nano = batchTimeMap[batchID][1].count();
        if (latest_time_nano > time_now_nano)
        {
            return;
        }
        else
        { // 只在batch的最晚通信的时候恢复
            // 判断是否下i个batch已经开始优化，从而判断是否需要恢复
            for (auto it = batchTimeMap.begin(); it != batchTimeMap.end(); ++it)
            {
                if (it->first != batchID)
                {
                    if (it->second[0].count() < time_now_nano &&
                        it->second[1].count() > time_now_nano)
                    {
                        return;
                    }
                }
            }
            if (is_bandwidth_opt)
            { // 恢复带宽
                change_all_link_bandwidth(this->bandwidth);
                record_into_file("restore_timestamp.txt",
                                 std::to_string(batchID) + " " + std::to_string(time_now_nano));
            }
            else
            { // 恢复延时
                change_all_link_delay(this->delay);
            }
        }
    }
}

void
FattreeTopology::change_all_link_delay(std::string delay)
{
    ns3::Config::Set("/ChannelList/*/$ns3::PointToPointChannel/Delay", ns3::StringValue(delay));
}

void
FattreeTopology::change_all_link_bandwidth(std::string bandwidth)
{
    ns3::Config::Set("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/DataRate",
                     ns3::StringValue(bandwidth));
}

/*
void
FattreeTopology::change_linkdevice_state(NS3NetDevice device1, NS3NetDevice device2, bool state)
{
    linkDeviceState[device1][device2] = state;
    linkDeviceState[device2][device1] = state;
    auto nodeId1 = device1->GetNode()->GetId();
    auto nodeId2 = device2->GetNode()->GetId();
    if (aggreSwitchID2Index.find(nodeId1) != aggreSwitchID2Index.end() &&
        edgeSwitchID2Index.find(nodeId2) != edgeSwitchID2Index.end())
    {
        auto nodeIndex1 = aggreSwitchID2Index[nodeId1];
        auto nodeIndex2 = edgeSwitchID2Index[nodeId2];
    }
    else
    {
        swap(nodeId1, nodeId2);
        auto nodeIndex1 = aggreSwitchID2Index[nodeId1];
        auto nodeIndex2 = edgeSwitchID2Index[nodeId2];
    }

    auto groupIndex1 = nodeIndex1 / self.number_of_aggre_switches_per_pod;
    auto groupIndex2 = nodeIndex2 / self.number_of_edge_switches_per_pod;
    if (state == true)
    {
        groupLinkNumberMap[groupIndex1][groupIndex2]++;
        groupLinkNumberMap[groupIndex2][groupIndex1]++;
    }
    else
    {
        groupLinkNumberMap[groupIndex1][groupIndex2]--;
        groupLinkNumberMap[groupIndex2][groupIndex1]--;
    }
}


void
FattreeTopology::activate_one_level(OCSLayerID layer)
{
    // std::cout << "activate_one_level" << endl;
    GroupID choose_g_aggre = 0;
    GroupID choose_g_edge = 0;

    std::vector<GroupID> count_choose_g_aggre;
    std::vector<GroupID> count_choose_g_edge;

    for (GroupID i = 0; i < self.number_of_pod; i++)
    {
        count_choose_g_aggre.push_back(i);
        count_choose_g_edge.push_back(i);
    }

    while (count_choose_g_aggre.size() >= 1 && count_choose_g_edge.size() >= 1)
    {
        GroupID index1 = rand() % count_choose_g_aggre.size();
        choose_g_1 = count_choose[index1];
        GroupID index2 = rand() % count_choose_g_edge.size();
        choose_g_2 = count_choose[index2];

        auto ipv41 = reconfigurableLinkMap[layer][choose_g_1][choose_g_2].ipv41;
        auto device1 = reconfigurableLinkMap[layer][choose_g_1][choose_g_2].device1;
        auto ipv41ifIndex = ipv41->GetInterfaceForDevice(device1);
        ipv41->SetUp(ipv41ifIndex);

        auto ipv42 = reconfigurableLinkMap[layer][choose_g_1][choose_g_2].ipv42;
        auto device2 = reconfigurableLinkMap[layer][choose_g_1][choose_g_2].device2;
        auto ipv42ifIndex = ipv42->GetInterfaceForDevice(device2);
        ipv42->SetUp(ipv42ifIndex);

        change_linkdevice_state(device1, device2, true);

        count_choose_g_aggre.erase(count_choose_g_aggre.begin() + index1);
        count_choose_g_edge.erase(count_choose_g_edge.begin() + index2);
    }
}

void
FattreeTopology::link_reconfiguration(std::vector<std::size_t>& to_choose_pair)
{
    choosed_pair = to_choose_pair;

    std::vector<GroupID> groupLinkDown;
    std::vector<GroupID> groupLinkUp;
    // find add link
    auto tor1Ipv4 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].ipv41; auto device1 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].device1; auto tor2Ipv4
= reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].ipv42; auto device2 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].device2;
    tor1Ipv4->SetUp(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetUp(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, true);

    tor1Ipv4 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].ipv41;
    device1 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].device1;
    tor2Ipv4 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].ipv42;
    device2 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].device2;
    tor1Ipv4->SetUp(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetUp(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, true);

    groupLinkUp.push_back(choosed_pair[0]);
    groupLinkUp.push_back(choosed_pair[1]);
    groupLinkUp.push_back(choosed_pair[2]);
    groupLinkUp.push_back(choosed_pair[3]);

    // find delete link
    auto min_g0_g2 = choosed_pair[0];
    auto max_g0_g2 = choosed_pair[3];
    tor1Ipv4 = reconfigurableLinkMap[choosed_pair[4]][min_g0_g2][max_g0_g2].ipv41;
    device1 = reconfigurableLinkMap[choosed_pair[4]][min_g0_g2][max_g0_g2].device1;
    tor2Ipv4 = reconfigurableLinkMap[choosed_pair[4]][min_g0_g2][max_g0_g2].ipv42;
    device2 = reconfigurableLinkMap[choosed_pair[4]][min_g0_g2][max_g0_g2].device2;
    tor1Ipv4->SetDown(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetDown(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, false);

    auto min_g1_g3 = choosed_pair[2];
    auto max_g1_g3 = choosed_pair[1];
    tor1Ipv4 = reconfigurableLinkMap[choosed_pair[4]][min_g1_g3][max_g1_g3].ipv41;
    device1 = reconfigurableLinkMap[choosed_pair[4]][min_g1_g3][max_g1_g3].device1;
    tor2Ipv4 = reconfigurableLinkMap[choosed_pair[4]][min_g1_g3][max_g1_g3].ipv42;
    device2 = reconfigurableLinkMap[choosed_pair[4]][min_g1_g3][max_g1_g3].device2;
    tor1Ipv4->SetDown(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetDown(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, false);

    groupLinkDown.push_back(choosed_pair[0]);
    groupLinkDown.push_back(choosed_pair[3]);
    groupLinkDown.push_back(choosed_pair[2]);
    groupLinkDown.push_back(choosed_pair[1]);

    std::cout << Simulator::Now() << " groupLinkUp: " << groupLinkUp[0] << " " << groupLinkUp[1]
              << " " << groupLinkUp[2] << " " << groupLinkUp[3] << std::endl;
    std::cout << Simulator::Now() << " groupLinkDown: " << groupLinkDown[0] << " "
              << groupLinkDown[1] << " " << groupLinkDown[2] << " " << groupLinkDown[3]
              << std::endl;

    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    if (ugal && ecmp)
    {
        ugalRoutingHelperForECMPUgal->NotifyLinkChanges();
    }
    else if (ugal)
    {
        ugalRoutingHelper->NotifyLinkChanges();
    }
}

void
FattreeTopology::set_direct_change(Ptr<Ipv4> ipv4, bool value)
{
    ipv4->local_drain(value);
}

void
FattreeTopology::Direct_change(std::vector<std::size_t>& to_choose_pair)
{
    choosed_pair = to_choose_pair;
    std::cout << "Direct_change " << choosed_pair[0] << " " << choosed_pair[1] << " "
              << choosed_pair[2] << " " << choosed_pair[3] << std::endl;

    // delete link[0][3]
    auto Ipv41 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].ipv41;
    auto device1 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].device1; auto
ipv4ifIndex1 = Ipv41->GetInterfaceForDevice(device1); set_direct_change(Ipv41, true);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &DragonflyTopology::set_direct_change,
                        this,
                        Ipv41,
                        true);
    Ipv41->SetDown(ipv4ifIndex1);

    auto Ipv42 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].ipv42;
    auto device2 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].device2; auto
ipv4ifIndex2 = Ipv42->GetInterfaceForDevice(device2); set_direct_change(Ipv42, true);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &DragonflyTopology::set_direct_change,
                        this,
                        Ipv42,
                        true);
    Ipv42->SetDown(ipv4ifIndex2);

    change_linkdevice_state(device1, device2, false);

    // delete link[2][1]
    Ipv41 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].ipv41;
    device1 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].device1;
    ipv4ifIndex1 = Ipv41->GetInterfaceForDevice(device1);
    set_direct_change(Ipv41, true);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &DragonflyTopology::set_direct_change,
                        this,
                        Ipv41,
                        false);
    Ipv41->SetDown(ipv4ifIndex1);

    Ipv42 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].ipv42;
    device2 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].device2;
    ipv4ifIndex2 = Ipv42->GetInterfaceForDevice(device2);
    set_direct_change(Ipv42, true);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &DragonflyTopology::set_direct_change,
                        this,
                        Ipv42,
                        false);
    Ipv42->SetDown(ipv4ifIndex2);

    change_linkdevice_state(device1, device2, false);

    // add link[0][1]
    Ipv41 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].ipv41;
    device1 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].device1;
    ipv4ifIndex1 = Ipv41->GetInterfaceForDevice(device1);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(), &Ipv4::SetUp, Ipv41,
ipv4ifIndex1);

    Ipv42 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].ipv42;
    device2 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[1]].device2;
    ipv4ifIndex2 = Ipv42->GetInterfaceForDevice(device2);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(), &Ipv4::SetUp, Ipv42,
ipv4ifIndex2);

    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &DragonflyTopology::change_linkdevice_state,
                        this,
                        device1,
                        device2,
                        true);

    // add link[2][3]
    Ipv41 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].ipv41;
    device1 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].device1;
    ipv4ifIndex1 = Ipv41->GetInterfaceForDevice(device1);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(), &Ipv4::SetUp, Ipv41,
ipv4ifIndex1);

    Ipv42 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].ipv42;
    device2 = reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[3]].device2;
    ipv4ifIndex2 = Ipv42->GetInterfaceForDevice(device2);
    Simulator::Schedule(Seconds(threshold) + Simulator::Now(), &Ipv4::SetUp, Ipv42,
ipv4ifIndex2);

    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &DragonflyTopology::change_linkdevice_state,
                        this,
                        device1,
                        device2,
                        true);

    Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                        &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    if (ugal && ecmp)
    {
        ugalRoutingHelperForECMPUgal->NotifyLinkChanges();
        Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                            &ns3::Ipv4UGALRoutingHelper<IntergroupCost>::NotifyLinkChanges,
                            &(*ugalRoutingHelperForECMPUgal));
    }
    else if (ugal)
    {
        ugalRoutingHelper->NotifyLinkChanges();
        Simulator::Schedule(Seconds(threshold) + Simulator::Now(),
                            &ns3::Ipv4UGALRoutingHelper<>::NotifyLinkChanges,
                            &(*ugalRoutingHelper));
    }
}

void
FattreeTopology::unblock_flows()
{
    // find src-dst pair to unblock
    for (auto& srcDstPair : blockedPairState)
    {
        auto srcID = srcDstPair.first;
        auto srcAddress = serverMap[srcID].address;

        auto sendApp = address2Onoffapplication[srcAddress];
        auto socket = sendApp->GetSocket();
        socket->unblock();
    }
}

int
FattreeTopology::block_flows(Ptr<Node> node, uint32_t ipv4ifIndex, Ptr<Node> peer)
{
    // get all src-dst pair from interface
    auto ipv4 = node->GetObject<Ipv4>();
    auto ipv4l3 = ipv4->GetObject<Ipv4L3Protocol>();
    auto interface = ipv4l3->GetInterface(ipv4ifIndex);
    auto routingTable = ipv4l3->GetRoutingProtocol()->GetObject<Ipv4GlobalRouting>();
    if (routingTable == nullptr)
    { // list routing exists
        auto ListRoutingTable = ipv4l3->GetRoutingProtocol()->GetObject<Ipv4ListRouting>();
        int16_t priority = 0;
        uint32_t index = 1;
        routingTable =
            ListRoutingTable->GetRoutingProtocol(index,
priority)->GetObject<Ipv4GlobalRouting>();
    }

    // list routing exists && ecmp + ugal
    if (routingTable == nullptr)
    {
        auto ListRoutingTable = ipv4l3->GetRoutingProtocol()->GetObject<Ipv4ListRouting>();
        int16_t priority = 0;
        uint32_t index = 1;
        auto ugalRouting = ListRoutingTable->GetRoutingProtocol(index, priority)
                               ->GetObject<Ipv4UGALRouting<double>>();

        NS_ASSERT_MSG(ecmp == true, "ecmp is false");

        int nflow = 0;
        for (auto& trafficPair : traffic_pattern_end_pair)
        {
            auto dstNode = trafficPair[0];
            auto srcNode = trafficPair[1];
            auto dstID = servers.Get(dstNode)->GetId();
            auto srcID = servers.Get(srcNode)->GetId();
            // auto dstAddress = serverMap[dstID].address;
            auto srcAddress = serverMap[srcID].address;

            auto minCongestionRoutes = ugalRouting->minCongestionECMPRoute(node->GetId(),
dstID); auto minHopsRoutes = ugalRouting->minHopsECMPRoute(node->GetId(), dstID);

            for (auto route : minCongestionRoutes)
            {
                if (route.nextHop == peer->GetId())
                {
                    NS_ASSERT_MSG(route.current == node->GetId(), "route.current !=
node->GetId()"); blockedPairState[srcID][dstID] = false;

                    auto sendApp = address2Onoffapplication[srcAddress];
                    auto socket = sendApp->GetSocket();
                    std::function<void(ns3::NS3NodeID, ns3::Address)> callback =
                        [this](ns3::NS3NodeID srcID, ns3::Address dstadd) {
                            alarm_local_drain(srcID, dstadd);
                        };
                    socket->block(callback);

                    nflow++;
                }
            }
            // for(auto route : minHopsRoutes)
            // {
            //     if (route.nextHop == peer->GetId())
            //     {
            //         NS_ASSERT_MSG(route.current == node->GetId(), "route.current !=
            //         node->GetId()"); nflow++; blockedPairState[srcID][dstID] = false;
            //     }
            // }
        }
        std::cout << "nflow: " << nflow << std::endl;
        return nflow;
    }

    int nflow = 0;
    for (auto& trafficPair : traffic_pattern_end_pair)
    {
        auto dstNode = trafficPair[0];
        auto srcNode = trafficPair[1];
        auto dstID = servers.Get(dstNode)->GetId();
        auto srcID = servers.Get(srcNode)->GetId();
        auto dstAddress = serverMap[dstID].address;
        auto srcAddress = serverMap[srcID].address;

        NS_ASSERT_MSG((routingTable) != nullptr, "routingTable is null");
        int nRoutes = routingTable->GetNRoutes();
        for (auto routeIndex = 0; routeIndex < nRoutes; routeIndex++)
        {
            auto route = routingTable->GetRoute(routeIndex);
            if (route->GetInterface() == ipv4ifIndex && route->GetDest() == dstAddress)
            {
                // std::cout << "srcID: " << srcID << " dstID: " << dstID << std::endl;
                blockedPairState[srcID][dstID] = false;
                auto sendApp = address2Onoffapplication[srcAddress];
                auto socket = sendApp->GetSocket();
                std::function<void(ns3::NS3NodeID, ns3::Address)> callback =
                    [this](ns3::NS3NodeID srcID, ns3::Address dstadd) {
                        alarm_local_drain(srcID, dstadd);
                    };
                socket->block(callback);

                nflow++;
            }
            else
            {
                continue;
            }
        }
    }
    std::cout << "nflow: " << nflow << std::endl;
    return nflow;
}

void
FattreeTopology::alarm_local_drain(NS3NodeID srcID, Address dstadd)
{
    // check if all pairs local drained
    int count = 0;
    bool allLocalDrained = true;
    for (auto& outerPair : blockedPairState)
    {
        if (outerPair.first == srcID)
        {
            // traffic pattern : one src to one dst
            for (auto& innerPair : outerPair.second)
            {
                innerPair.second = true;
            }
            continue;
        }
        for (auto& innerPair : outerPair.second)
        {
            if (innerPair.second == false)
            {
                allLocalDrained = false;
                break;
            }
            else
            {
                count++;
            }
        }
    }

    if (allLocalDrained == true)
    {
        link_reconfiguration(choosed_pair);
        unblock_flows();
        blockedPairState.clear();
    }
}

void
FattreeTopology::Local_draining(std::vector<std::size_t>& to_choose_pair)
{
    // debug info
    std::cout << "local draining " << to_choose_pair[0] << " " << to_choose_pair[1] << " "
              << to_choose_pair[2] << " " << to_choose_pair[3] << std::endl;

    // ensure previous local draining is done
    for (auto& kv1 : blockedPairState)
    {
        for (auto& kv2 : kv1.second)
        {
            if (kv2.second == false)
            {
                std::cout << "ERROR: Previous local draining is not done." << std::endl;
                return;
            }
        }
    }

    // clear  blockedPairState
    blockedPairState.clear();
    choosed_pair = to_choose_pair;

    // build new blockedPairState
    // // to-delete link1 [0][3]
    auto Ipv411 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].ipv41; auto device11 =
        reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].device1;
    auto ipv4ifIndex11 = Ipv411->GetInterfaceForDevice(device11);

    auto Ipv412 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].ipv42; auto device12 =
        reconfigurableLinkMap[choosed_pair[4]][choosed_pair[0]][choosed_pair[3]].device2;
    auto ipv4ifIndex12 = Ipv412->GetInterfaceForDevice(device12);

    auto nflow_11 = block_flows(device11->GetNode(), ipv4ifIndex11, device12->GetNode());
    auto nflow_12 = block_flows(device12->GetNode(), ipv4ifIndex12, device11->GetNode());

    // // to-delete link2 [2][1]
    auto Ipv421 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].ipv41; auto device21 =
        reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].device1;
    auto ipv4ifIndex21 = Ipv421->GetInterfaceForDevice(device21);

    auto Ipv422 =
reconfigurableLinkMap[choosed_pair[4]][choosed_pair[2]][choosed_pair[1]].ipv42; auto device22 =
        reconfigurableLinkMap[choosed_pair[4]][choosed_pair[1]][choosed_pair[3]].device2;
    auto ipv4ifIndex22 = Ipv422->GetInterfaceForDevice(device22);
    auto nflow_21 = block_flows(device21->GetNode(), ipv4ifIndex21, device22->GetNode());
    auto nflow_22 = block_flows(device22->GetNode(), ipv4ifIndex22, device21->GetNode());

    if (nflow_11 == 0 && nflow_12 == 0 && nflow_21 == 0 && nflow_22 == 0)
    {
        link_reconfiguration(choosed_pair);
    }

    std::cout << "blockedPairState size: " << blockedPairState.size() << std::endl;
}

void
FattreeTopology::incremental_change()
{
    std::cout << Simulator::Now() << " incremental_change" << endl;

    traffic_matrix = std::vector<std::vector<float>>(g, std::vector<float>(g, 0.0));
    topology_matrix = std::vector<std::vector<int>>(g, std::vector<int>(g, 0));
    AVG = std::vector<std::vector<float>>(g, std::vector<float>(g, 0.0));
    float target = 0;
    hottest_pair.clear(); // store group id
    change_pair.clear();
    // get traffic matirx : from ip interface
    //  for(int i = 0; i < number_of_end; i++){
    //      Ptr<Node> n = nodes.Get(i / p);
    //      uint32_t ipv4ifIndex = i % p + 1; //?
    //      Ptr<Ipv4L3Protocol> ipv4l3 = n->GetObject<Ipv4L3Protocol> ();
    //      std::cout << "index " << i / p << " "<< i % p + 1 << endl;
    //      Ptr<Ipv4Interface> interface = ipv4l3->GetInterface(ipv4ifIndex);
    //      std::cout << i << " " << ipv4ifIndex <<" " << interface->GetAddress(ipv4ifIndex) <<
    //      endl; for(uint32_t i = 0; i < interface->packet_src_record.size(); i ++){
    //          uint32_t index = interface->packet_src_record[i].first.Get();
    //          index = (index & 0b1111111111111111) >> 8;
    //          if(index >= 0 && index < uint32_t(number_of_end))
    //              traffic_matrix[index][i] += 1.0;
    //      }
    //  }

    // get traffic matirx : from setting
    for (std::vector<uint32_t>& tp : traffic_pattern_end_pair)
    {
        traffic_matrix[tp[0] / (a * p)][tp[1] / (a * p)] += 1.0;
        traffic_matrix[tp[1] / (a * p)][tp[0] / (a * p)] += 1.0;
    }

    // get topology matrix
    for (GroupID i = 0; i < g; i++)
    {
        for (GroupID j = 0; j < g; j++)
        {
            topology_matrix[i][j] = groupLinkNumberMap[i][j];
            topology_matrix[j][i] = groupLinkNumberMap[j][i];
        }
    }

    // get AVG
    for (GroupID i = 0; i < g; i++)
    {
        for (GroupID j = 0; j < g; j++)
        {
            if (i == j)
            {
                continue;
            }
            if (topology_matrix[i][j] != 0)
            {
                AVG[i][j] = (float(traffic_matrix[i][j])) / (float(topology_matrix[i][j]));
            }
            else
            {
                std::cout << "no link?" << std::endl;
                AVG[i][j] = (float(traffic_matrix[i][j]));
            }
        }
    }

    // find hottest and target
    for (GroupID i = 0; i < g; i++)
    {
        for (GroupID j = 0; j < g; j++)
        {
            if (i == j)
                continue;
            if (AVG[i][j] + AVG[j][i] > target)
            {
                hottest_pair.clear();
                hottest_pair.push_back({i, j});
                target = AVG[i][j] + AVG[j][i];
            }
            else if (AVG[i][j] + AVG[j][i] == target)
            {
                hottest_pair.push_back({i, j});
            }
        }
    }
    int hottest = target;
    // std::cout << "hottest size: " << hottest_pair.size() << " " << hottest_pair[0][0] << " "
<<
    // hottest_pair[0][1] << endl;

    // find targetreturn
    target = 0;
    for (uint32_t i = 0; i < hottest_pair.size(); i++)
    {
        for (OCSLayerID layer = 0; layer < ocs; layer++)
        {
            for (GroupID k = 0; k < g; k++)
            {
                for (GroupID l = k + 1; l < g; l++)
                {
                    auto g_index1 = hottest_pair[i][0];
                    auto g_index2 = hottest_pair[i][1];
                    auto g_index3 = k;
                    auto g_index4 = l;
                    if ((g_index1 == g_index3 || g_index2 == g_index4) ||
                        (g_index1 == g_index4 || g_index2 == g_index3))
                        continue;
                    if (hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                            (AVG[g_index1][g_index3] + AVG[g_index3][g_index1]) -
                            (AVG[g_index2][g_index4] + AVG[g_index4][g_index2]) >
                        0)
                    {
                        auto ipv41 = reconfigurableLinkMap[layer][g_index1][g_index2].ipv41;
                        auto device1 = reconfigurableLinkMap[layer][g_index1][g_index2].device1;
                        auto ipv41ifIndex = ipv41->GetInterfaceForDevice(device1);
                        auto ipv42 = reconfigurableLinkMap[layer][g_index3][g_index4].ipv41;
                        auto device2 = reconfigurableLinkMap[layer][g_index3][g_index4].device1;
                        auto ipv42ifIndex = ipv42->GetInterfaceForDevice(device2);
                        auto ipv43 = reconfigurableLinkMap[layer][std::min(g_index1, g_index3)]
                                                          [std::max(g_index1, g_index3)]
                                                              .ipv41;
                        auto device3 = reconfigurableLinkMap[layer][std::min(g_index1,
g_index3)] [std::max(g_index1, g_index3)] .device1; auto ipv43ifIndex =
ipv43->GetInterfaceForDevice(device3); auto ipv44 =
reconfigurableLinkMap[layer][std::min(g_index2, g_index4)] [std::max(g_index2, g_index4)]
                                                              .ipv41;
                        auto device4 = reconfigurableLinkMap[layer][std::min(g_index2,
g_index4)] [std::max(g_index2, g_index4)] .device1; auto ipv44ifIndex =
ipv44->GetInterfaceForDevice(device4);

                        if (!ipv41->IsUp(ipv41ifIndex) && !ipv42->IsUp(ipv42ifIndex) &&
                            ipv43->IsUp(ipv43ifIndex) && ipv44->IsUp(ipv44ifIndex))
                        {
                            change_pair.push_back({g_index1, g_index2, g_index3, g_index4,
layer}); target = hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                                     (AVG[g_index1][g_index3] + AVG[g_index3][g_index1]) -
                                     (AVG[g_index2][g_index4] + AVG[g_index4][g_index2]);
                        }
                    }
                    if (hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                            (AVG[g_index1][g_index4] + AVG[g_index4][g_index1]) -
                            (AVG[g_index2][g_index3] + AVG[g_index3][g_index2]) >
                        0)
                    {
                        auto ipv41 = reconfigurableLinkMap[layer][g_index1][g_index2].ipv41;
                        auto device1 = reconfigurableLinkMap[layer][g_index1][g_index2].device1;
                        auto ipv41ifIndex = ipv41->GetInterfaceForDevice(device1);
                        auto ipv42 = reconfigurableLinkMap[layer][g_index3][g_index4].ipv41;
                        auto device2 = reconfigurableLinkMap[layer][g_index3][g_index4].device1;
                        auto ipv42ifIndex = ipv42->GetInterfaceForDevice(device2);
                        auto ipv43 = reconfigurableLinkMap[layer][std::min(g_index1, g_index4)]
                                                          [std::max(g_index1, g_index4)]
                                                              .ipv41;
                        auto device3 = reconfigurableLinkMap[layer][std::min(g_index1,
g_index4)] [std::max(g_index1, g_index4)] .device1; auto ipv43ifIndex =
ipv43->GetInterfaceForDevice(device3); auto ipv44 =
reconfigurableLinkMap[layer][std::min(g_index2, g_index3)] [std::max(g_index2, g_index3)]
                                                              .ipv41;
                        auto device4 = reconfigurableLinkMap[layer][std::min(g_index2,
g_index3)] [std::max(g_index2, g_index3)] .device1; auto ipv44ifIndex =
ipv44->GetInterfaceForDevice(device4);

                        if (!ipv41->IsUp(ipv41ifIndex) && !ipv42->IsUp(ipv42ifIndex) &&
                            ipv43->IsUp(ipv43ifIndex) && ipv44->IsUp(ipv44ifIndex))
                        {
                            target = hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3]
- (AVG[g_index1][g_index4] + AVG[g_index4][g_index1]) - (AVG[g_index2][g_index3] +
AVG[g_index3][g_index2]); change_pair.push_back({g_index1, g_index2, g_index4, g_index3,
layer});
                        }
                    }
                }
            }
        }
    }

    sort(change_pair.begin(),
         change_pair.end(),
         [this](const std::vector<std::size_t>& a, const std::vector<std::size_t>& b) {
             return cmp_change_pair(a, b);
         });

    int num = change_pair.size();
    for (int choose_i = 0; choose_i < num; choose_i++)
    {
        // Direct_change(change_pair[choose_i]);
        // link_reconfiguration(change_pair[choose_i]);
        Local_draining(change_pair[choose_i]);
        break;
    }

    // std::cout << "end increment" << endl;
}

*/

bool
FattreeTopology::check_link_status(NS3NetDevice device1, NS3NetDevice device2)
{
    return linkDeviceState[device1][device2];
}

bool
FattreeTopology::check_nodes_between_group(NS3NodeID nodeid1, NS3NodeID nodeid2)
{
    // end id should be excluded
    // nodeid1 should be aggre or edge, &&, nodeid2 should be aggre or edge
    if (!((aggreSwitchID2Index.find(nodeid1) != aggreSwitchID2Index.end() ||
           edgeSwitchID2Index.find(nodeid1) != edgeSwitchID2Index.end()) &&
          (aggreSwitchID2Index.find(nodeid1) != aggreSwitchID2Index.end() ||
           edgeSwitchID2Index.find(nodeid1) != edgeSwitchID2Index.end())))
    {
        return false;
    }

    auto nodeIndex1 = switchID2Index[nodeid1];
    auto nodeIndex2 = switchID2Index[nodeid2];
    // self.number_of_aggre_switches_per_pod == self.number_of_edge_switches_per_pod
    if ((nodeIndex1 / number_of_aggre_switches_per_pod) !=
        (nodeIndex2 / number_of_aggre_switches_per_pod))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void
FattreeTopology::inter_group_hop_trigger(ns3::Ptr<const ns3::NetDevice> device1,
                                         ns3::Ptr<const ns3::NetDevice> device2)
{
    auto nodeId1 = device1->GetNode()->GetId();
    auto nodeId2 = device2->GetNode()->GetId();
    if (check_nodes_between_group(nodeId1, nodeId2))
    {
        totalHops++;
    }
}