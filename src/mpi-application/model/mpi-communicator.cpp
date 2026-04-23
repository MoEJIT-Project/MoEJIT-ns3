//
// Created by Ricardo Evans on 2023/6/26.
//

#include <unordered_map>
#include <vector>

#include <ns3/core-module.h>
#include <ns3/network-module.h>

#include "mpi-communicator.h"

NS_LOG_COMPONENT_DEFINE("MPICommunicator");

using NS3Packet = ns3::Ptr<ns3::Packet>;
using NS3Error = ns3::Socket::SocketErrno;

// this is just used to test all the templates can be deduced properly
ns3::CoroutineOperation<void> ns3::MPICommunicator::templateTest() {
    co_await Send(RawPacket, 0, Create<Packet>(0));
    co_await Send(FakePacket, 0, 1024);
    co_await Send(0, 0);
    co_await Send<int>(FakePacket, 0);
    co_await Send<std::vector<int>>(FakePacket, 0, 16);
    co_await Recv(RawPacket, 0, 1024);
    co_await Recv(FakePacket, 0, 1024);
    co_await Recv<int>(0);
    co_await Recv<std::vector<int>>(FakePacket, 0, 16);
    co_await SendRecv(0, 1, 2);
    co_await SendRecv<int, int>(FakePacket, 0, 1, {}, {});
    co_await SendRecv<std::vector<int>, std::vector<int>>(FakePacket, 0, 1, std::tuple{1}, std::tuple{2});
    co_await SendRecv<std::vector<int>, std::vector<int>>(FakePacket, 0, 1, std::make_tuple(1), std::make_tuple(2));
    co_await Gather(0, std::vector<int>{1, 2, 3, 4, 5});
    co_await Gather<std::vector<int>>(FakePacket, 0, 16);
    co_await Gather<std::vector<int>>(FakePacket, 0,
                                      std::unordered_map<MPIRankIDType, std::tuple<int>>{
                                              {1, 1},
                                              {2, 3},
                                      });
    co_await AllGather(1);
    co_await AllGather<int>(FakePacket);
    co_await AllGather<std::vector<int>>(FakePacket,
                                         std::unordered_map<MPIRankIDType, std::tuple<int>>{
                                                 {1, 1},
                                                 {2, 3},
                                         });
    co_await Scatter(0, std::unordered_map<MPIRankIDType, int>{
            {1, 5},
            {2, 6},
    });
    co_await Scatter<std::vector<int>>(FakePacket, 0, 5);
    co_await Scatter<std::vector<int>>(FakePacket, 0, std::unordered_map<MPIRankIDType, std::tuple<int>>{
            {1, 1},
            {2, 3},
    });
    co_await Broadcast(0, std::optional<int>{1});
    co_await Broadcast<std::vector<int>>(FakePacket, 0, 16);
    co_await Reduce<MPIOperator::MAX>(0, 1);
    co_await Reduce<MPIOperator::SUM>(0, 1);
    co_await Reduce<std::vector<int>>(FakePacket, 0, 16);
    co_await ReduceScatter<MPIOperator::SUM>(std::unordered_map<MPIRankIDType, int>{
            {1, 5},
            {2, 6},
    });
    co_await ReduceScatter<std::vector<int>>(FakePacket, std::unordered_map<MPIRankIDType, std::tuple<int>>{
            {1, 1},
            {2, 3},
    });
    co_await Barrier();
    co_await AllReduce<MPIOperator::MAX>(1);
    co_await AllReduce<MPIOperator::SUM>(1);
    co_await AllReduce<std::vector<int>>(FakePacket, 16);
    co_await RingAllReduce<uint8_t>(FakePacket, 1024);
    co_await AllToAll(std::unordered_map<MPIRankIDType, int>{
            {1, 5},
            {2, 6},
    });
    co_await AllToAll<int, short>(std::unordered_map<MPIRankIDType, int>{
            {1, 5},
            {2, 6},
    });
    co_await AllToAll<std::vector<int>>(FakePacket, std::unordered_map<MPIRankIDType, std::tuple<int>>{
            {1, 1},
            {2, 3},
    }, std::unordered_map<MPIRankIDType, std::tuple<int>>{
            {1, 1},
            {2, 3},
    });
    co_await AllToAll<std::vector<int>, std::vector<short>>(FakePacket, std::unordered_map<MPIRankIDType, std::tuple<int>>{
            {1, 1},
            {2, 3},
    }, std::unordered_map<MPIRankIDType, std::tuple<int>>{
            {1, 1},
            {2, 3},
    });
}

ns3::MPICommunicator::MPICommunicator(ns3::MPIRankIDType rankID, const std::shared_ptr<std::mt19937> &randomEngine, std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>> &&sockets) noexcept:
        rankID(rankID),
        randomEngine(randomEngine),
        sockets(std::move(sockets)) {
    auto rank_id = this->sockets | std::ranges::views::keys;
    ranks = std::vector<MPIRankIDType>{std::ranges::begin(rank_id), std::ranges::end(rank_id)};
    std::sort(ranks.begin(), ranks.end());
}

ns3::CoroutineOperation<void> ns3::MPICommunicator::Send(MPIRawPacket, MPIRankIDType rank, NS3Packet packet) {
    NS_LOG_DEBUG(std::format("{} send raw data of size {} to rank {}", rankID, packet->GetSize(), rank));
    auto &socket = *sockets[rank];
    auto [size, error] = co_await socket.send(packet);
    if (error != NS3Error::ERROR_NOTERROR) {
        throw CoroutineSocketException{"Send to rank " + std::to_string(rank) + " failed, reason: " + format(error)};
    }
}

ns3::CoroutineOperation<void> ns3::MPICommunicator::Send(MPIFakePacket, MPIRankIDType rank, std::size_t size) {
    NS_LOG_DEBUG(std::format("{} send fake data of size {} to rank {}", rankID, size, rank));
    co_await Send(RawPacket, rank, Create<Packet>(size));
}

ns3::CoroutineOperation<NS3Packet> ns3::MPICommunicator::Recv(MPIRawPacket, MPIRankIDType rank, std::size_t size) {
    NS_LOG_DEBUG(std::format("{} receive raw data of size{} from rank {}", rankID, size, rank));
    auto &socket = *sockets[rank];
    auto [packet, error] = co_await socket.receive(size);
    if (error != NS3Error::ERROR_NOTERROR) {
        throw CoroutineSocketException{"Receive from rank " + std::to_string(rank) + " failed, reason: " + format(error)};
    }
    co_return packet;
}

ns3::CoroutineOperation<void> ns3::MPICommunicator::Recv(MPIFakePacket, MPIRankIDType rank, std::size_t size) {
    NS_LOG_DEBUG(std::format("{} receive fake data of size {} from rank {}", rankID, size, rank));
    co_await Recv(RawPacket, rank, size);
}

ns3::CoroutineOperation<void> ns3::MPICommunicator::Barrier() {
    NS_LOG_DEBUG(std::format("{} barrier", rankID));
    std::vector<CoroutineOperation<void>> operations;
    for (auto rank: sockets | std::ranges::views::keys) {
        operations.push_back(Gather(rank, rankID).then(discard<std::unordered_map<MPIRankIDType, MPIRankIDType>>));
    }
    for (auto &operation: operations) {
        co_await operation;
    }
}

void ns3::MPICommunicator::Block() noexcept {
    std::ranges::for_each(sockets | std::ranges::views::values, &CoroutineSocket::block);
}

void ns3::MPICommunicator::Unblock() noexcept {
    std::ranges::for_each(sockets | std::ranges::views::values, &CoroutineSocket::unblock);
}

std::size_t ns3::MPICommunicator::TxBytes() const noexcept {
    auto connections = sockets | std::ranges::views::filter([this](auto &p) { return p.first != rankID; }) | std::ranges::views::values | std::ranges::views::transform(&CoroutineSocket::txBytes);
    return std::accumulate(std::ranges::begin(connections), std::ranges::end(connections), 0, std::plus{});
}

std::size_t ns3::MPICommunicator::RxBytes() const noexcept {
    auto connections = sockets | std::ranges::views::filter([this](auto &p) { return p.first != rankID; }) | std::ranges::views::values | std::ranges::views::transform(&CoroutineSocket::rxBytes);
    return std::accumulate(std::ranges::begin(connections), std::ranges::end(connections), 0, std::plus{});
}

void ns3::MPICommunicator::Close() {
    for (auto &socket: sockets | std::ranges::views::values) {
        auto error = socket->close();
        if (error != NS3Error::ERROR_NOTERROR) {
            throw std::domain_error(std::format("communicator {}::error when closing socket", rankID));
        }
    }
}

ns3::MPIRankIDType ns3::MPICommunicator::RankID() const noexcept {
    return rankID;
}

std::set<ns3::MPIRankIDType> ns3::MPICommunicator::GroupMembers() const noexcept {
    auto members = sockets | std::ranges::views::keys;
    return std::set<MPIRankIDType>{members.begin(), members.end()};
}


