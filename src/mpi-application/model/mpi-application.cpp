//
// Created by Ricardo Evans on 2023/6/29.
//

#include "mpi-application.h"

#include "mpi-util.h"

#include <ns3/internet-module.h>

#include <format>
#include <unordered_map>
#include <unordered_set>
#include <vector>



ns3::MPIApplication::MPIApplication(
    ns3::MPIRankIDType rankID,
    std::map<ns3::MPIRankIDType, Address>&& addresses,
    std::map<Address, ns3::MPIRankIDType>&& ranks,
    std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>>&& functions) noexcept
    : MPIApplication(rankID,
                     std::move(addresses),
                     std::move(ranks),
                     std::move(functions),
                     std::mt19937::default_seed ^ rankID)
{
}

ns3::MPIApplication::MPIApplication(ns3::MPIRankIDType rankID,
                                    std::map<ns3::MPIRankIDType, Address>&& addresses,
                                    std::map<Address, ns3::MPIRankIDType>&& ranks,
                                    std::queue<MPIFunction>&& functions,
                                    std::mt19937::result_type seed) noexcept
    : rankID(rankID),
      addresses(std::move(addresses)),
      ranks(std::move(ranks)),
      functions(std::move(functions)),
      randomEngine(std::make_shared<std::mt19937>(seed))
{
}

ns3::MPIApplication::MPIApplication(
    ns3::MPIRankIDType rankID,
    const std::map<ns3::MPIRankIDType, Address>& addresses,
    const std::map<Address, ns3::MPIRankIDType>& ranks,
    std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>>&& functions) noexcept
    : MPIApplication(rankID,
                     addresses,
                     ranks,
                     std::move(functions),
                     std::mt19937::default_seed ^ rankID)
{
}

ns3::MPIApplication::MPIApplication(
    ns3::MPIRankIDType rankID,
    const std::map<ns3::MPIRankIDType, Address>& addresses,
    const std::map<Address, ns3::MPIRankIDType>& ranks,
    std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>>&& functions,
    std::mt19937::result_type seed) noexcept
    : rankID(rankID),
      addresses(addresses),
      ranks(ranks),
      functions(std::move(functions)),
      randomEngine(std::make_shared<std::mt19937>(seed))
{
}

void
ns3::MPIApplication::StartApplication()
{
    running = true;
    run();
}

void
ns3::MPIApplication::StopApplication()
{
    running = false;
}

namespace ns3
{
std::vector<std::unordered_set<void*>> AppAddrSet(2);
}

ns3::CoroutineOperation<void>
ns3::MPIApplication::run()
{
    //todo:: no varys情况没有初始化
    int Appid = AppAddrSet[0].count(this) ? 1 : 3;
    // 等待dml中改变running为false
    std::cout << "At App" << Appid << ":mpi application of rank " << rankID
              << " total functions: " << functions.size() << std::endl;
    auto start = ns3::Now();
    // test是否能正确判断所在集合

    while (running)
    {
        if (functions.empty())
        {
            // 等待 100 ns 再检查一次
            // todo:: 减少误差用corouting
            co_await this->Compute(std::chrono::duration<int64_t, std::nano>(1000000));
            //            std::cout << "skip a check..."
            //                      << ", now time: " << ns3::Now() << std::endl;
            continue;
        }
        co_await functions.front()(*this);
        functions.pop();
        std::cout << "At App" << Appid << ":mpi application of rank " << rankID
                  << " remaining functions: " << functions.size() << " now time: " << ns3::Now()
                  << std::endl;
    }
    auto end = ns3::Now();
    std::cout << "At App" << Appid << ":mpi application of rank " << rankID
              << " start time: " << start << ", end time: " << end << std::endl;
    running = false;
}

// 20251014存档原来run方式
// ns3::CoroutineOperation<void> ns3::MPIApplication::run() {
//    std::cout << "mpi application of rank " << rankID << " total functions: " << functions.size()
//    << std::endl; auto start = ns3::Now(); while (!functions.empty()) {
//        if (!running) {
//            break;
//        }
//        co_await functions.front()(*this);
//        functions.pop();
//        std::cout << "mpi application of rank " << rankID << " remaining functions: " <<
//        functions.size() << " now time: " << ns3::Now() << std::endl;
//    }
//    auto end = ns3::Now();
//    std::cout << "mpi application of rank " << rankID << " start time: " << start << ", end time:"
//    << end << std::endl; running = false;
//}

ns3::CoroutineOperation<
    std::unordered_map<ns3::MPIRankIDType, std::shared_ptr<ns3::CoroutineSocket>>>
ns3::MPIApplication::connect(size_t cache_limit,
                             MPIRankIDType rankID,
                             NS3Node node,
                             const std::map<MPIRankIDType, Address>& addresses,
                             const std::map<Address, MPIRankIDType>& ranks)
{
    auto& self = addresses.at(rankID);
    CoroutineSocket listener{node, TcpSocketFactory::GetTypeId(), cache_limit};
    listener.bind(self);
    std::vector<CoroutineOperation<void>> operations;
    std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>> sockets;
    for (auto& [rank, address] : addresses)
    {
        if (rank < rankID)
        {
            operations.push_back(listener.accept().then([&sockets, &ranks](auto d) {
                auto& [s, a, e] = d;
                if (e != NS3Error::ERROR_NOTERROR)
                {
                    throw std::runtime_error("error when accepting connection from other ranks");
                }
                auto ip = retrieveIPAddress(a);
                NS_ASSERT_MSG(ranks.contains(ip), "rank not found");
                sockets[ranks.at(ip)] = std::make_shared<CoroutineSocket>(std::move(s));
            }));
        }
        if (rank > rankID)
        {
            sockets[rank] =
                std::make_shared<CoroutineSocket>(node, TcpSocketFactory::GetTypeId(), cache_limit);
            operations.push_back(sockets[rank]->connect(address).then([](auto e) {
                if (e != NS3Error::ERROR_NOTERROR)
                {
                    throw std::runtime_error("error when connecting to other ranks");
                }
            }));
        }
    }
    for (auto& o : operations)
    {
        co_await o;
    }
    listener.close();
    co_return std::move(sockets);
}

ns3::CoroutineOperation<void>
ns3::MPIApplication::Initialize(size_t mtu_size)
{
    if (status != Status::INITIAL)
    {
        throw std::runtime_error("MPIApplication::Init() should only be called once");
    }
    auto cache_limit = mtu_size * 100;
    std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>> selfSockets;
    std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>> worldSockets =
        std::move(co_await connect(cache_limit, rankID, GetNode(), addresses, ranks));
    worldSockets[rankID] = std::make_shared<CoroutineSocket>(cache_limit); // loopback
    selfSockets[rankID] = std::make_shared<CoroutineSocket>(cache_limit);  // loopback
    NS_ASSERT_MSG(selfSockets.size() == 1, "self sockets size is not correct");
    NS_ASSERT_MSG(worldSockets.size() == addresses.size(), "world sockets size is not correct");
    communicators.emplace(std::piecewise_construct,
                          std::forward_as_tuple(NULL_COMMUNICATOR),
                          std::forward_as_tuple());
    communicators.emplace(std::piecewise_construct,
                          std::forward_as_tuple(WORLD_COMMUNICATOR),
                          std::forward_as_tuple(rankID, randomEngine, std::move(worldSockets)));
    communicators.emplace(std::piecewise_construct,
                          std::forward_as_tuple(SELF_COMMUNICATOR),
                          std::forward_as_tuple(rankID, randomEngine, std::move(selfSockets)));
    status = Status::WORKING;
}

void
ns3::MPIApplication::Finalize()
{
    if (status != Status::WORKING)
    {
        throw std::runtime_error(
            "MPIApplication::Finalize() should only be called after MPIApplication::Init()");
    }
    for (auto& [_, communicator] : communicators)
    {
        communicator.Close();
    }
    status = Status::FINALIZED;
}

void
ns3::MPIApplication::Block() noexcept
{
    std::ranges::for_each(communicators | std::ranges::views::values, &MPICommunicator::Block);
}

void
ns3::MPIApplication::Unblock() noexcept
{
    std::ranges::for_each(communicators | std::ranges::views::values, &MPICommunicator::Unblock);
}

ns3::MPICommunicator&
ns3::MPIApplication::communicator(const MPICommunicatorIDType id)
{
    if (!Initialized())
    {
        throw std::domain_error(
            "MPIApplication::communicator can only be called after initialized");
    }
    return communicators.at(id);
}

ns3::MPICommunicator&
ns3::MPIApplication::duplicate_communicator(const MPICommunicatorIDType oldID,
                                            const MPICommunicatorIDType newID)
{
    if (!Initialized())
    {
        throw std::domain_error(
            "MPIApplication::duplicate_communicator can only be called after initialized");
    }
    if (!communicators.contains(oldID))
    {
        throw std::domain_error(
            "MPIApplication::duplicate_communicator old communicator id not found");
    }
    if (communicators.contains(newID))
    {
        throw std::domain_error(
            "MPIApplication::duplicate_communicator new communicator id already exists");
    }
    auto& communicator = communicators.at(oldID);
    auto [iterator, inserted] = communicators.emplace(std::piecewise_construct,
                                                      std::forward_as_tuple(newID),
                                                      std::forward_as_tuple(communicator));
    if (!inserted)
    {
        throw std::domain_error(
            "MPIApplication::duplicate_communicator insert communicator failed");
    }
    return iterator->second;
}

void
ns3::MPIApplication::free_communicator(const ns3::MPICommunicatorIDType id)
{
    communicators.at(id).Close();
    communicators.erase(id);
}

bool
ns3::MPIApplication::Initialized() const noexcept
{
    return status == Status::WORKING;
}

bool
ns3::MPIApplication::Finalized() const noexcept
{
    return status == Status::FINALIZED;
}
