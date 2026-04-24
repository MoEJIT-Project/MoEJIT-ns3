#include "dragonfly-topology.h"

#include <exception>
#include <iostream>
#include <type_traits>
#include <vector>

DragonflyTopology::DragonflyTopology(std::size_t p,
                                     std::size_t a,
                                     std::size_t h,
                                     std::size_t g,
                                     std::string bandwidth,
                                     std::string delay,
                                     float stop_time,
                                     bool ecmp,
                                     std::string app_bd,
                                     std::string bandwidth_opt,
                                     double scale_factor,
                                     int64_t optimization_delay)
{
    this->p = p;
    this->a = a;
    this->h = h;
    this->g = g;
    this->bandwidth = bandwidth;
    this->delay = delay;
    this->stop_time = stop_time;
    this->ecmp = ecmp;
    this->app_bd = app_bd;
    this->bandwidth_opt = bandwidth_opt;
    this->scale_factor = scale_factor;
    // this->optimization_delay = std::chrono::duration<int64_t, std::nano>(optimization_delay);

    this->switchCount = g * a;
    this->serverCount = switchCount * p;
    this->backgroundLinkCount =
        switchCount * (a + h - 1) +
        serverCount *
            2; // server-tor links + links in every group + links between groups (simplex links)
}

void
DragonflyTopology::prediction_callback_for_MUSE(ns3::MPIRankIDType noderankID,
                                                ns3::BatchID batchID,
                                                TimeType comm_begin_time,
                                                ns3::SizeType comm_size)
{
    std::cout << "DragonflyTopology::prediction_callback_for_MUSE" << std::endl;
    auto bandwidth = 300;
    auto comm_begin_time_nano = comm_begin_time.count();
    auto comm_end_time_nano =
        comm_begin_time_nano + 2 * (this->serverCount) * comm_size * 8 / bandwidth;

    std::vector<Flow> new_flows;
    for (int rankID = 0; rankID < (int)this->serverCount; ++rankID)
    {
        int next_rankID = (rankID + 1) % this->serverCount;
        auto flow = Flow(this->servers.Get(this->rankID2nodeIndex[rankID])->GetId(),
                         this->servers.Get(this->rankID2nodeIndex[next_rankID])->GetId());
        new_flows.push_back(flow);
    }

    // 已经进入新的一轮，则进行新的重构，直至无需调整边
    if (batchCommsizeMap.find(batchID) == batchCommsizeMap.end() ||
        batchCommsizeMap[batchID].find(comm_size) == batchCommsizeMap[batchID].end() ||
        batchCommsizeMap[batchID][comm_size][0].count() > comm_begin_time_nano)
    {
        std::cout << "DragonflyTopology::prediction_callback_for_MUSE" << noderankID << " "
                  << batchID << " " << comm_size << " " << comm_begin_time.count() << std::endl;
        if ((int)(comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds()) > 0)
        {
            ns3::Simulator::Schedule(
                NanoSeconds(comm_begin_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &DragonflyTopology::multiple_incremental_changes,
                this,
                new_flows,
                batchID,
                comm_size);
            ns3::Simulator::Schedule(
                NanoSeconds(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &DragonflyTopology::clear_flows,
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
        batchCommsizeMap[batchID][comm_size][1] = TimeType{comm_end_time_nano};
        if ((int)(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()) > 0)
            ns3::Simulator::Schedule(
                NanoSeconds(comm_end_time_nano - ns3::Simulator::Now().GetNanoSeconds()),
                &DragonflyTopology::clear_flows,
                this,
                new_flows,
                comm_size,
                batchID);
    }
}

void
DragonflyTopology::multiple_incremental_changes(std::vector<Flow> flows,
                                                ns3::BatchID batchID,
                                                ns3::SizeType comm_size)
{
    // new_flows just to clarify clear flow
    std::vector<Flow> new_flows;
    auto flow = Flow(this->servers.Get(0)->GetId(), this->servers.Get(0)->GetId());
    new_flows.push_back(flow);

    if (flows != new_flows)
        record_into_file("optimization.txt",
                         std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                             std::to_string(batchID) + " " + std::to_string(comm_size));

    auto curr_time_nano = ns3::Simulator::Now().GetNanoSeconds();
    if ((batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
         batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
         batchCommsizeMap[batchID][comm_size][0].count() == curr_time_nano) ||
        batchCommsizeMap.size() == 0) // 没有更早的优化开始时间 或者是 ideal
    {
        if (batchCommsizeMap.size() == 0)
        { // 更新this->flows
            for (const auto& new_flow : this->flows1)
            {
                this->flows.push_back(new_flow);
            }
        }
        else
        { // 更新this->flows
            for (const auto& new_flow : new_flows)
            {
                this->flows.push_back(new_flow);
            }
        }
    }

    // 重构直至没有优化
    float pre_targetreturn = 10000;
    int count = 0;
    float current_targetreturn = 0;
    while (true)
    {
        std::cout << "multiple_incremental_changes " << ++count << " " << pre_targetreturn << " "
                  << current_targetreturn << " flows count:" << this->flows.size() << std::endl;
        current_targetreturn = incremental_change_for_MUSE();
        if (current_targetreturn == 0.0 || (count >= 20 && current_targetreturn > pre_targetreturn))
        {
            incremental_change_for_MUSE();
            return;
        }
        pre_targetreturn = current_targetreturn;
    }
}

void
DragonflyTopology::clear_flows(std::vector<Flow> new_flows,
                               ns3::SizeType comm_size,
                               ns3::BatchID batchID)
{
    auto curr_time_nano = ns3::Simulator::Now().GetNanoSeconds();
    if (batchCommsizeMap.find(batchID) != batchCommsizeMap.end() &&
        batchCommsizeMap[batchID].find(comm_size) != batchCommsizeMap[batchID].end() &&
        batchCommsizeMap[batchID][comm_size][1].count() > curr_time_nano)
    { // 有更晚的优化结束时间
        return;
    }
    record_into_file("restore.txt",
                     std::to_string(ns3::Simulator::Now().GetNanoSeconds()) + " " +
                         std::to_string(batchID) + " " + std::to_string(comm_size));
    bool changed = false;
    if (batchCommsizeMap.size() == 0)
    {
        for (Flow new_flow : this->flows1)
        {
            auto it = std::find(this->flows.begin(), this->flows.end(), new_flow);
            if (it != this->flows.end())
            {
                this->flows.erase(it);
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
        // new_flows just to call multiple_incremental_changes;
        std::vector<Flow> new_flows;
        auto flow = Flow(this->servers.Get(0)->GetId(), this->servers.Get(0)->GetId());
        new_flows.push_back(flow);

        this->multiple_incremental_changes(new_flows, batchID, comm_size);
    }
}

void
DragonflyTopology::prediction_callback(ns3::MPIRankIDType noderankID,
                                       ns3::BatchID batchID,
                                       TimeType comm_begin_time,
                                       ns3::SizeType comm_size)
{
    std::cout << "noderankID: " << noderankID << ", batchID: " << batchID
              << ", comm_begin_time: " << comm_begin_time.count() << ", comm_size: " << comm_size
              << std::endl;
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
        comm_begin_time_nano + 2 * (this->serverCount) * comm_size * 8 / bandwidth_opt_number;
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
                                     &DragonflyTopology::optimize_network,
                                     this,
                                     batchID);
            ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                     &DragonflyTopology::restore_network,
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
                                         &DragonflyTopology::optimize_network,
                                         this,
                                         batchID);
            }
            if ((long unsigned int)latest_time_nano < comm_end_time_nano)
            {
                batchTimeMap[batchID][1] = TimeType{comm_end_time_nano};
                ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                         &DragonflyTopology::restore_network,
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
                                     &DragonflyTopology::optimize_network,
                                     this,
                                     batchID);
            ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                     &DragonflyTopology::restore_network,
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
                                         &DragonflyTopology::optimize_network,
                                         this,
                                         batchID);
            }
            if ((long unsigned int)latest_time_nano < comm_end_time_nano)
            {
                batchTimeMap[batchID][1] = TimeType{comm_end_time_nano};
                ns3::Simulator::Schedule(NanoSeconds(comm_end_time_nano - time_now_nano),
                                         &DragonflyTopology::restore_network,
                                         this,
                                         batchID);
            }
        }
    }
}

void
DragonflyTopology::record_into_file(std::filesystem::path filename, std::string line)
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
DragonflyTopology::optimize_network(ns3::BatchID batchID)
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
DragonflyTopology::restore_network(ns3::BatchID batchID)
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
DragonflyTopology::change_all_link_delay(string delay)
{
    Config::Set("/ChannelList/*/$ns3::PointToPointChannel/Delay", StringValue(delay));
}

void
DragonflyTopology::change_all_link_bandwidth(string bandwidth)
{
    Config::Set("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/DataRate",
                StringValue(bandwidth));
}

void
DragonflyTopology::inter_group_hop_trigger(Ptr<const NetDevice> device1,
                                           Ptr<const NetDevice> device2)
{
    auto nodeId1 = device1->GetNode()->GetId();
    auto nodeId2 = device2->GetNode()->GetId();
    if (check_nodes_between_group(nodeId1, nodeId2))
    {
        totalHops++;
    }
}

bool
DragonflyTopology::check_nodes_between_group(NS3NodeID nodeid1, NS3NodeID nodeid2)
{
    // end id should be excluded
    if (switchID2Index.find(nodeid1) == switchID2Index.end() ||
        switchID2Index.find(nodeid2) == switchID2Index.end())
    {
        return false;
    }

    auto nodeIndex1 = switchID2Index[nodeid1];
    auto nodeIndex2 = switchID2Index[nodeid2];
    if ((nodeIndex1 / a) != (nodeIndex2 / a))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool
DragonflyTopology::check_link_status(NS3NetDevice device1, NS3NetDevice device2)
{
    return linkDeviceState[device1][device2];
}

void
DragonflyTopology::change_linkdevice_state(NS3NetDevice device1, NS3NetDevice device2, bool state)
{
    linkDeviceState[device1][device2] = state;
    linkDeviceState[device2][device1] = state;
    auto nodeId1 = device1->GetNode()->GetId();
    auto nodeId2 = device2->GetNode()->GetId();
    auto nodeIndex1 = switchID2Index[nodeId1];
    auto nodeIndex2 = switchID2Index[nodeId2];
    auto groupIndex1 = nodeIndex1 / a;
    auto groupIndex2 = nodeIndex2 / a;
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
DragonflyTopology::link_reconfiguration_for_MUSE(std::vector<std::size_t>& to_choose_pair)
{
    choosed_pair = to_choose_pair;

    std::vector<GroupID> groupLinkDown;
    std::vector<GroupID> groupLinkUp;
    // find add link
    auto tor1Ipv4 = backgroundLinkMap[choosed_pair[0]][choosed_pair[1]].ipv41;
    auto device1 = backgroundLinkMap[choosed_pair[0]][choosed_pair[1]].device1;
    auto tor2Ipv4 = backgroundLinkMap[choosed_pair[0]][choosed_pair[1]].ipv42;
    auto device2 = backgroundLinkMap[choosed_pair[0]][choosed_pair[1]].device2;
    tor1Ipv4->SetUp(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetUp(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, true);

    tor1Ipv4 = backgroundLinkMap[choosed_pair[2]][choosed_pair[3]].ipv41;
    device1 = backgroundLinkMap[choosed_pair[2]][choosed_pair[3]].device1;
    tor2Ipv4 = backgroundLinkMap[choosed_pair[2]][choosed_pair[3]].ipv42;
    device2 = backgroundLinkMap[choosed_pair[2]][choosed_pair[3]].device2;
    tor1Ipv4->SetUp(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetUp(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, true);

    groupLinkUp.push_back(choosed_pair[0]);
    groupLinkUp.push_back(choosed_pair[1]);
    groupLinkUp.push_back(choosed_pair[2]);
    groupLinkUp.push_back(choosed_pair[3]);

    // find delete link
    auto min_g0_g2 = std::min(choosed_pair[0], choosed_pair[2]);
    auto max_g0_g2 = std::max(choosed_pair[0], choosed_pair[2]);
    tor1Ipv4 = backgroundLinkMap[min_g0_g2][max_g0_g2].ipv41;
    device1 = backgroundLinkMap[min_g0_g2][max_g0_g2].device1;
    tor2Ipv4 = backgroundLinkMap[min_g0_g2][max_g0_g2].ipv42;
    device2 = backgroundLinkMap[min_g0_g2][max_g0_g2].device2;
    tor1Ipv4->SetDown(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetDown(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, false);

    auto min_g1_g3 = std::min(choosed_pair[1], choosed_pair[3]);
    auto max_g1_g3 = std::max(choosed_pair[1], choosed_pair[3]);
    tor1Ipv4 = backgroundLinkMap[min_g1_g3][max_g1_g3].ipv41;
    device1 = backgroundLinkMap[min_g1_g3][max_g1_g3].device1;
    tor2Ipv4 = backgroundLinkMap[min_g1_g3][max_g1_g3].ipv42;
    device2 = backgroundLinkMap[min_g1_g3][max_g1_g3].device2;
    tor1Ipv4->SetDown(tor1Ipv4->GetInterfaceForDevice(device1));
    tor2Ipv4->SetDown(tor2Ipv4->GetInterfaceForDevice(device2));
    change_linkdevice_state(device1, device2, false);

    groupLinkDown.push_back(choosed_pair[0]);
    groupLinkDown.push_back(choosed_pair[2]);
    groupLinkDown.push_back(choosed_pair[1]);
    groupLinkDown.push_back(choosed_pair[3]);

    std::cout << Simulator::Now() << " groupLinkUp: " << groupLinkUp[0] << " " << groupLinkUp[1]
              << " " << groupLinkUp[2] << " " << groupLinkUp[3] << std::endl;
    std::cout << Simulator::Now() << " groupLinkDown: " << groupLinkDown[0] << " "
              << groupLinkDown[1] << " " << groupLinkDown[2] << " " << groupLinkDown[3]
              << std::endl;

    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    // if (ugal && ecmp)
    // {
    //     ugalRoutingHelperForECMPUgal->NotifyLinkChanges();
    // }
    // else if (ugal)
    // {
    //     ugalRoutingHelper->NotifyLinkChanges();
    // }
}

// incremental_change_for_MUSE
float
DragonflyTopology::incremental_change_for_MUSE()
{
    std::cout << Simulator::Now() << " incremental_change_for_MUSE" << endl;

    traffic_matrix = std::vector<std::vector<float>>(g, std::vector<float>(g, 0.0));
    topology_matrix = std::vector<std::vector<int>>(g, std::vector<int>(g, 0));
    AVG = std::vector<std::vector<float>>(g, std::vector<float>(g, 0.0));
    float target = 0;
    hottest_pair.clear();
    change_pair.clear();

    if (this->flows.size() == 0)
    {
        return false;
    }

    // get traffic matirx : from setting
    for (Flow& flow : this->flows)
    {
        auto nodeIndex1 = this->rankID2nodeIndex[flow.first];
        auto nodeIndex2 = this->rankID2nodeIndex[flow.second];
        NS_ASSERT_MSG(nodeIndex1 / (a * p) < g && nodeIndex2 / (a * p) < g,
                      "nodeIndex out of range");
        traffic_matrix[nodeIndex1 / (a * p)][nodeIndex2 / (a * p)] += 1.0;
        traffic_matrix[nodeIndex2 / (a * p)][nodeIndex1 / (a * p)] += 1.0;
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
                // std::cout << "no link?" << std::endl;
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
    // std::cout << "hottest size: " << hottest_pair.size() << " " << hottest_pair[0][0] << " " <<
    // hottest_pair[0][1] << endl;

    // find targetreturn
    target = 0;
    float best_target = 0;
    for (uint32_t i = 0; i < hottest_pair.size(); i++)
    {
        for (GroupID k = 0; k < g; k++)
        {
            for (GroupID l = k + 1; l < g; l++)
            {
                auto g_index1 = std::min(hottest_pair[i][0], hottest_pair[i][1]);
                auto g_index2 = std::max(hottest_pair[i][0], hottest_pair[i][1]);
                auto g_index3 = std::min(k, l);
                auto g_index4 = std::max(k, l);
                // std::cout << g_index1 << " " << g_index2 << " " << g_index3 << " " <<
                // g_index4 << endl;
                if ((g_index1 == g_index3 || g_index2 == g_index4) ||
                    (g_index1 == g_index4 || g_index2 == g_index3))
                    continue;
                if (hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                        (AVG[g_index1][g_index3] + AVG[g_index3][g_index1]) -
                        (AVG[g_index2][g_index4] + AVG[g_index4][g_index2]) >
                    0)
                {
                    target = hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                             (AVG[g_index1][g_index3] + AVG[g_index3][g_index1]) -
                             (AVG[g_index2][g_index4] + AVG[g_index4][g_index2]);
                    best_target = max(best_target, target);
                    change_pair.push_back({g_index1, g_index2, g_index3, g_index4});
                }
                // std::cout << hottest_pair[i][0] << " " <<  hottest_pair[i][1] << " " << k <<
                // " " << l << " " << hottest + AVG[k][l] + AVG[l][k] -
                // (AVG[hottest_pair[i][0]][k] + AVG[k][hottest_pair[i][0]]) -
                // (AVG[hottest_pair[i][1]][l] + AVG[l][hottest_pair[i][1]]) << endl;
                if (hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                        (AVG[g_index1][g_index4] + AVG[g_index4][g_index1]) -
                        (AVG[g_index2][g_index3] + AVG[g_index3][g_index2]) >
                    0)
                {
                    target = hottest + AVG[g_index3][g_index4] + AVG[g_index4][g_index3] -
                             (AVG[g_index1][g_index4] + AVG[g_index4][g_index1]) -
                             (AVG[g_index2][g_index3] + AVG[g_index3][g_index2]);
                    best_target = max(best_target, target);
                    change_pair.push_back({g_index1, g_index2, g_index4, g_index3});
                }
                // std::cout << hottest_pair[i][0] << " " <<  hottest_pair[i][1] << " " << k <<
                // " " << l << " " << hottest + AVG[k][l] + AVG[l][k] -
                // (AVG[hottest_pair[i][1]][k] + AVG[k][hottest_pair[i][1]]) -
                // (AVG[hottest_pair[i][0]][l] + AVG[l][hottest_pair[i][0]]) << endl;
            }
        }
    }
    std::cout << "best target: " << best_target << std::endl;

    // sort(change_pair.begin(),
    //      change_pair.end(),
    //      [this](const std::vector<std::size_t>& a, const std::vector<std::size_t>& b) {
    //          return cmp_change_pair(a, b);
    //      });

    // sort change pair with AVG value ( if target is same, random choose order )
    for (uint32_t i = 0; i < change_pair.size(); i++)
    {
        for (uint32_t j = i + 1; j < change_pair.size(); j++)
        {
            auto a = change_pair[i];
            auto b = change_pair[j];
            auto target1 = AVG[a[2]][a[3]] + AVG[a[3]][a[2]] + AVG[a[0]][a[1]] + AVG[a[1]][a[0]] -
                           (AVG[a[0]][a[2]] + AVG[a[2]][a[0]]) -
                           (AVG[a[1]][a[3]] + AVG[a[3]][a[1]]);
            auto target2 = AVG[b[2]][b[3]] + AVG[b[3]][b[2]] + AVG[b[0]][b[1]] + AVG[b[1]][b[0]] -
                           (AVG[b[0]][b[2]] + AVG[b[2]][b[0]]) -
                           (AVG[b[1]][b[3]] + AVG[b[3]][b[1]]);
            if (target1 < target2 || (target1 == target2 && rand() % 2))
            {
                swap(change_pair[i], change_pair[j]);
            }
        }
    }

    int num = change_pair.size();
    bool choose_success = false;
    for (int choose_i = 0; choose_i < num; choose_i++)
    {
        std::vector<std::vector<SwitchID>> candidate_switch_change_pair;
        for (auto switchIndex1 = change_pair[choose_i][0] * a;
             switchIndex1 < change_pair[choose_i][0] * a + a;
             switchIndex1++)
        {
            for (auto switchIndex2 = change_pair[choose_i][1] * a;
                 switchIndex2 < change_pair[choose_i][1] * a + a;
                 switchIndex2++)
            {
                for (auto switchIndex3 = change_pair[choose_i][2] * a;
                     switchIndex3 < change_pair[choose_i][2] * a + a;
                     switchIndex3++)
                {
                    for (auto switchIndex4 = change_pair[choose_i][3] * a;
                         switchIndex4 < change_pair[choose_i][3] * a + a;
                         switchIndex4++)
                    {
                        candidate_switch_change_pair.push_back(
                            {switchIndex1, switchIndex2, switchIndex3, switchIndex4});
                    }
                }
            }
        }

        while (!choose_success && !candidate_switch_change_pair.empty())
        {
            auto randomIndex = std::rand() % candidate_switch_change_pair.size();
            auto& switch_change_pair = candidate_switch_change_pair[randomIndex];

            auto switchId1 = switches.Get(switch_change_pair[0])->GetId();
            auto switchId2 = switches.Get(switch_change_pair[1])->GetId();
            auto switchId3 = switches.Get(switch_change_pair[2])->GetId();
            auto switchId4 = switches.Get(switch_change_pair[3])->GetId();
            auto ipv411 = backgroundLinkMap[switchId1][switchId2].ipv41;
            auto ipv412 = backgroundLinkMap[switchId1][switchId2].ipv42;
            auto ipv421 = backgroundLinkMap[switchId3][switchId4].ipv41;
            auto ipv422 = backgroundLinkMap[switchId3][switchId4].ipv42;
            auto ipv431 = backgroundLinkMap[switchId1][switchId3].ipv41;
            auto ipv432 = backgroundLinkMap[switchId1][switchId3].ipv42;
            auto ipv441 = backgroundLinkMap[switchId2][switchId4].ipv41;
            auto ipv442 = backgroundLinkMap[switchId2][switchId4].ipv42;
            auto device11 = backgroundLinkMap[switchId1][switchId2].device1;
            auto device12 = backgroundLinkMap[switchId1][switchId2].device2;
            auto device21 = backgroundLinkMap[switchId3][switchId4].device1;
            auto device22 = backgroundLinkMap[switchId3][switchId4].device2;
            auto device31 = backgroundLinkMap[switchId1][switchId3].device1;
            auto device32 = backgroundLinkMap[switchId1][switchId3].device2;
            auto device41 = backgroundLinkMap[switchId2][switchId4].device1;
            auto device42 = backgroundLinkMap[switchId2][switchId4].device2;
            auto ipv4ifIndex11 = ipv411->GetInterfaceForDevice(device11);
            auto ipv4ifIndex12 = ipv412->GetInterfaceForDevice(device12);
            auto ipv4ifIndex21 = ipv421->GetInterfaceForDevice(device21);
            auto ipv4ifIndex22 = ipv422->GetInterfaceForDevice(device22);
            auto ipv4ifIndex31 = ipv431->GetInterfaceForDevice(device31);
            auto ipv4ifIndex32 = ipv432->GetInterfaceForDevice(device32);
            auto ipv4ifIndex41 = ipv441->GetInterfaceForDevice(device41);
            auto ipv4ifIndex42 = ipv442->GetInterfaceForDevice(device42);

            if (!ipv411->IsUp(ipv4ifIndex11) && !ipv412->IsUp(ipv4ifIndex12) &&
                !ipv421->IsUp(ipv4ifIndex21) && !ipv422->IsUp(ipv4ifIndex22) &&
                ipv431->IsUp(ipv4ifIndex31) && ipv432->IsUp(ipv4ifIndex32) &&
                ipv441->IsUp(ipv4ifIndex41) && ipv442->IsUp(ipv4ifIndex42))
            {
                std::vector<std::size_t> to_choose_pair = {switchId1,
                                                           switchId2,
                                                           switchId3,
                                                           switchId4};

                NS_ASSERT_MSG(switchID2Index[switchId1] / a != switchID2Index[switchId2] / a &&
                                  switchID2Index[switchId3] / a != switchID2Index[switchId4] / a &&
                                  switchID2Index[switchId1] / a != switchID2Index[switchId3] / a &&
                                  switchID2Index[switchId2] / a != switchID2Index[switchId4] / a,
                              "choose within one group");

                if (ugal && enable_reconfiguration && !ecmp)
                {
                    // Direct_change_for_MUSE(to_choose_pair);
                    link_reconfiguration_for_MUSE(to_choose_pair);
                }
                else
                {
                    // Local_draining_for_MUSE(to_choose_pair);
                    link_reconfiguration_for_MUSE(to_choose_pair);
                    // Direct_change_for_MUSE(to_choose_pair);
                }
                choose_success = true;
                break;
            }

            candidate_switch_change_pair.erase(candidate_switch_change_pair.begin() + randomIndex);
        }

        if (choose_success)
        {
            break;
        }
    }
    std::cout << "choose_success = " << choose_success << std::endl;
    return best_target;
}