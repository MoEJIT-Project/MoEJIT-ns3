//
// Created by Ricardo Evans on 2023/6/1.
//

#ifndef NS3_MPI_APPLICATION_COMMUNICATOR_H
#define NS3_MPI_APPLICATION_COMMUNICATOR_H

#include <coroutine>
#include <memory>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>

#include <ns3/core-module.h>
#include <ns3/coroutine-module.h>
#include <ns3/network-module.h>

#include "mpi-exception.h"
#include "mpi-exception.h"
#include "mpi-protocol.h"
#include "mpi-util.h"
#include "mpi-protocol.h"
#include "mpi-util.h"

namespace ns3 {
    using MPICommunicatorIDType = std::uint64_t;
    static const constinit
    MPICommunicatorIDType ERROR_COMMUNICATOR = 0;
    static const constinit
    MPICommunicatorIDType NULL_COMMUNICATOR = 1;
    static const constinit
    MPICommunicatorIDType WORLD_COMMUNICATOR = 2;
    static const constinit
    MPICommunicatorIDType SELF_COMMUNICATOR = 3;

    /**
     * @brief MPICommunicator is the model of the communicator concept in MPI operations, you should do most of communications via this.
     */
    class MPICommunicator {
    public:
        // Global callback for flow completion time tracking
        static inline std::function<void(int, uint32_t, uint32_t, double, double)> m_fctCallback = nullptr;

    private:
        using NS3Packet = Ptr<Packet>;
        using NS3Error = Socket::SocketErrno;

        constexpr static std::string logName = "MPICommunicator";

        MPIRankIDType rankID;
        std::shared_ptr<std::mt19937> randomEngine;
        std::vector<MPIRankIDType> ranks;
        std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>> sockets;
        std::uniform_int_distribution<MPIRankIDType> voteGenerator{0, std::numeric_limits<MPIRankIDType>::max()};

        CoroutineOperation<void> templateTest();

    public:
        MPICommunicator() noexcept = default;

        MPICommunicator(MPIRankIDType rankID, const std::shared_ptr<std::mt19937> &randomEngine, std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>> &&sockets) noexcept;

        MPICommunicator(MPICommunicator &&) noexcept = default;

        MPICommunicator(const MPICommunicator &) noexcept = default;

        MPICommunicator &operator=(MPICommunicator &&) noexcept = default;

        MPICommunicator &operator=(const MPICommunicator &) noexcept = default;

        CoroutineOperation<void> Send(MPIRawPacket, MPIRankIDType rank, NS3Packet packet);

        CoroutineOperation<void> Send(MPIFakePacket, MPIRankIDType rank, std::size_t size);

        template<MPIWritable T>
        CoroutineOperation<void> Send(MPIRankIDType rank, T data) {
            logDebug(logName, std::format("{} send data of type {} to rank {}", rankID, getTypename<T>(), rank));
            co_await MPIObjectWriter<T>{}(*sockets[rank], std::move(data));
        }

        template<typename T, typename ...U>
        requires MPIFakeWritable<T, U...>
        CoroutineOperation<void> Send(MPIFakePacket p, MPIRankIDType rank, U ...u) {
            logDebug(logName, std::format("{} send fake data of type {} to rank {}, fake parameters: {}", rankID, getTypename<T>(), rank, to_string(u...)));
            co_await MPIObjectWriter<T>{}(*sockets[rank], p, std::move(u)...);
        }

        CoroutineOperation<NS3Packet> Recv(MPIRawPacket, MPIRankIDType rank, std::size_t size);

        CoroutineOperation<void> Recv(MPIFakePacket, MPIRankIDType rank, std::size_t size);

        template<MPIReadable T>
        CoroutineOperation<T> Recv(MPIRankIDType rank) {
            logDebug(logName, std::format("{} recv data of type {} from rank {}", rankID, getTypename<T>(), rank));
            co_return std::move(co_await MPIObjectReader<T>{}(*sockets[rank]));
        }

        template<typename T, typename ...U>
        requires MPIFakeReadable<T, U...>
        CoroutineOperation<void> Recv(MPIFakePacket p, MPIRankIDType rank, U ...u) {
            logDebug(logName, std::format("{} recv fake data of type {} from rank {}, fake parameters: {}", rankID, getTypename<T>(), rank, to_string(u...)));
            co_await MPIObjectReader<T>{}(*sockets[rank], p, std::move(u)...);
        }

        template<MPIWritable S, MPIReadable R=S>
        CoroutineOperation<R> SendRecv(MPIRankIDType destination, S data, MPIRankIDType source) {
            logDebug(logName, std::format("{} send data of type {} to rank {} and recv data of type {} from rank {}", rankID, getTypename<S>(), destination, getTypename<R>(), source));
            auto oS = Send(destination, std::move(data));
            auto oR = Recv<R>(source);
            co_await oS;
            co_return co_await oR;
        }

        template<typename S, typename R, typename ...US, typename ...UR>
        requires MPIFakeWritable<S, US...> and MPIFakeReadable<R, UR...>
        CoroutineOperation<void> SendRecv(MPIFakePacket p, MPIRankIDType destination, MPIRankIDType source, const std::tuple<US...> &uS, const std::tuple<UR...> &uR) {
            logDebug(logName, std::format("{} send fake data of type {} to rank {} and recv fake data of type {} from rank {}, fake parameters S: {}, fake parameters R: {}", rankID, getTypename<S>(), destination, getTypename<R>(), source, to_string(uS), to_string(uR)));
            auto oS = std::apply([this, &p, &destination](auto &&...args) { return this->Send<S>(p, destination, std::forward<decltype(args)>(args)...); }, uS);
            auto oR = std::apply([this, &p, &source](auto &&...args) { return this->Recv<R>(p, source, std::forward<decltype(args)>(args)...); }, uR);
            co_await oS;
            co_await oR;
        }

        template<MPIObject T>
        CoroutineOperation<std::unordered_map<MPIRankIDType, T>> Gather(MPIRankIDType root, T data) {
            logDebug(logName, std::format("{} gather data of type {} to rank {}", rankID, getTypename<T>(), root));
            std::unordered_map<MPIRankIDType, T> result;
            auto o = Send(root, std::move(data));
            if (rankID == root) {
                std::unordered_map<MPIRankIDType, CoroutineOperation<T>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations[rank] = Recv<T>(rank);
                }
                for (auto &[rank, operation]: operations) {
                    if constexpr (std::is_move_assignable_v<T>) {
                        result[rank] = std::move(co_await operation);
                    } else {
                        result[rank] = co_await operation;
                    }
                }
            }
            co_await o;
            co_return result;
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> Gather(MPIFakePacket p, MPIRankIDType root, U ...u) {
            logDebug(logName, std::format("{} gather fake data of type {} to rank {}, fake parameters: ", rankID, getTypename<T>(), root, to_string(u...)));
            std::unordered_map<MPIRankIDType, T> result;
            auto o = Send<T>(p, root, u...);
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(Recv<T>(p, rank, u...));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_await o;
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> Gather(MPIFakePacket p, MPIRankIDType root, const std::unordered_map<MPIRankIDType, std::tuple<U...>> &u) {
            logDebug(logName, std::format("{} gather fake data of type {} to rank {}", rankID, getTypename<T>(), root));
            std::unordered_map<MPIRankIDType, T> result;
            auto o = std::apply([this, &p, &root](auto &&...args) { return this->Send<T>(p, root, std::forward<decltype(args)>(args)...); }, u.at(rankID));
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(std::apply([this, &p, &rank](auto &&...args) { return this->Recv<T>(p, rank, std::forward<decltype(args)>(args)...); }, u.at(rank)));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_await o;
        }

        template<MPIObject T>
        CoroutineOperation<std::unordered_map<MPIRankIDType, T>> AllGather(T data) {
            logDebug(logName, std::format("{} all gather data of type {}", rankID, getTypename<T>()));
            std::unordered_map<MPIRankIDType, CoroutineOperation<std::unordered_map<MPIRankIDType, T>>> operations;
            for (auto rank: sockets | std::ranges::views::keys) {
                operations[rank] = Gather(rank, data);
            }
            for (auto &operation: operations | std::ranges::views::values) {
                co_await operation;
            }
            co_return co_await operations[rankID];
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> AllGather(MPIFakePacket p, U ...u) {
            logDebug(logName, std::format("{} all gather fake data of type {}, fake parameters: {}", rankID, getTypename<T>(), to_string(u...)));
            std::vector<CoroutineOperation<void>> operations;
            for (auto rank: sockets | std::ranges::views::keys) {
                operations.push_back(Gather<T>(p, rank, u...));
            }
            for (auto &operation: operations) {
                co_await operation;
            }
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> AllGather(MPIFakePacket p, const std::unordered_map<MPIRankIDType, std::tuple<U...>> &u) {
            logDebug(logName, std::format("{} all gather fake data of type {}, fake parameters omitted", rankID, getTypename<T>()));
            std::vector<CoroutineOperation<void>> operations;
            for (auto rank: sockets | std::ranges::views::keys) {
                operations.push_back(Gather<T>(p, rank, u));
            }
            for (auto &operation: operations) {
                co_await operation;
            }
        }

        template<MPIObject T>
        CoroutineOperation<T> Scatter(MPIRankIDType root, const std::unordered_map<MPIRankIDType, T> &data) {
            logDebug(logName, std::format("{} scatter data of type {} from rank {}", rankID, getTypename<T>(), root));
            auto o = Recv<T>(root);
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(Send(rank, data.at(rank)));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_return co_await o;
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> Scatter(MPIFakePacket p, MPIRankIDType root, U ...u) {
            logDebug(logName, std::format("{} scatter fake data of type {} from rank {}, fake parameters: {}", rankID, getTypename<T>(), root, to_string(u...)));
            auto o = Recv<T>(p, root, u...);
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(Send<T>(p, rank, u...));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_await o;
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> Scatter(MPIFakePacket p, MPIRankIDType root, const std::unordered_map<MPIRankIDType, std::tuple<U...>> &u) {
            logDebug(logName, std::format("{} scatter fake data of type {} from rank {}, fake parameters omitted", rankID, getTypename<T>(), root));
            auto o = std::apply([this, &p, &root](auto &&...args) { return this->Recv<T>(p, root, std::forward<decltype(args)>(args)...); }, u.at(rankID));
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(std::apply([this, &p, &rank](auto &&...args) { return this->Send<T>(p, rank, std::forward<decltype(args)>(args)...); }, u.at(rank)));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_await o;
        }

        template<MPIObject T>
        CoroutineOperation<T> Broadcast(MPIRankIDType root, const std::optional<T> &data) {
            logDebug(logName, std::format("{} broadcast data of type {} from rank {}", rankID, getTypename<T>(), root));
            auto o = Recv<T>(root);
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(Send(rank, data.value()));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_return co_await o;
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> Broadcast(MPIFakePacket p, MPIRankIDType root, U ...u) {
            logDebug(logName, std::format("{} broadcast fake data of type {} from rank {}, fake parameters: {}", rankID, getTypename<T>(), root, to_string(u...)));
            auto o = Recv<T>(p, root, u...);
            if (rankID == root) {
                std::vector<CoroutineOperation<void>> operations;
                for (auto rank: sockets | std::ranges::views::keys) {
                    operations.push_back(Send<T>(p, rank, u...));
                }
                for (auto &operation: operations) {
                    co_await operation;
                }
            }
            co_await o;
        }

        CoroutineOperation<void> Barrier();

        template<MPIOperator O, MPIObject T, typename ...U>
        requires MPIOperatorApplicable<O, T, U...>
        CoroutineOperation<std::optional<std::decay_t<T>>> Reduce(MPIRankIDType root, T data, U ...u) {
            logDebug(logName, std::format("{} reduce data of type {} at rank {}, parameters: {}", rankID, getTypename<T>(), root, to_string(u...)));
            std::unordered_map<MPIRankIDType, T> result = co_await Gather(root, std::move(data));
            if (rankID == root) {
                co_return MPIOperatorImplementation<O, T>{}(result | std::views::values, std::move(u)...);
            }
            co_return std::nullopt;
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> Reduce(MPIFakePacket p, MPIRankIDType root, U ...u) {
            logDebug(logName, std::format("{} reduce fake data of type {} at rank {}, fake parameters: {}", rankID, getTypename<T>(), root, to_string(u...)));
            co_await Gather<T>(p, root, std::move(u)...);
        }

        template<MPIOperator O, MPIObject T, typename ...P>
        requires MPIOperatorApplicable<O, T, P...>
        CoroutineOperation<T> ReduceScatter(const std::unordered_map<MPIRankIDType, T> &data, P ...p) {
            logDebug(logName, std::format("{} reduce scatter data of type {}", rankID, getTypename<T>()));
            std::unordered_map<MPIRankIDType, CoroutineOperation<T>> operations;
            for (auto &[rank, d]: data) {
                operations[rank] = Reduce<O>(rank, d, p...).then([](auto &&o) { return o.value(); });
            }
            for (auto &o: operations | std::ranges::views::values) {
                co_await o;
            }
            co_return co_await operations[rankID];
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> ReduceScatter(MPIFakePacket p, const std::unordered_map<MPIRankIDType, std::tuple<U...>> &u) {
            logDebug(logName, std::format("{} reduce scatter fake data of type {}, fake parameters omitted", rankID, getTypename<T>()));
            std::vector<CoroutineOperation<void>> operations;
            for (auto &[rank, u_]: u) {
                operations.push_back(std::apply([this, &p, &rank](auto &&...args) { return this->Reduce<T>(p, rank, std::forward<decltype(args)>(args)...); }, u_));
            }
            for (auto &o: operations) {
                co_await o;
            }
        }

        template<MPIObject T>
        CoroutineOperation<MPIRankIDType> Elect(T votes) {
            logDebug(logName, std::format("{} is electing", rankID));
            std::unordered_map<MPIRankIDType, CoroutineOperation<std::unordered_map<MPIRankIDType, T>>> operations;
            for (auto rank: sockets | std::ranges::views::keys) {
                operations[rank] = Gather(rank, std::move(votes));
            }
            for (auto &[_, operation]: operations) {
                co_await operation;
            }
            co_return std::ranges::max_element(operations[rankID].result(), [](auto &p1, auto &p2) { return p1.second == p2.second ? p1.first < p2.first : p1.second < p2.second; })->first;
        }

        template<MPIOperator O, MPIObject T, typename ...U>
        requires MPIOperatorApplicable<O, T, U...>
        CoroutineOperation<std::decay_t<T>> AllReduce(T data, U ...u) {
            logDebug(logName, std::format("{} all reduce data of type {}", rankID, getTypename<T>()));
            MPIRankIDType root = co_await Elect(voteGenerator(*randomEngine));
            auto result = co_await Reduce<O, T>(root, std::move(data), std::move(u)...);
            co_return std::move(co_await Broadcast(root, std::move(result)));
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> AllReduce(MPIFakePacket p, U ...u) {
            logDebug(logName, std::format("{} all reduce fake data of type {}, fake parameters: {}", rankID, getTypename<T>(), to_string(u...)));
            MPIRankIDType root = co_await Elect(voteGenerator(*randomEngine));
            co_await Reduce<T>(p, root, u...);
            co_await Broadcast<T>(p, root, u...);
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<std::vector<T>, std::size_t, U...>
        CoroutineOperation<void>  RingAllReduce(MPIFakePacket p, std::size_t size, U ...u) {
            logDebug(logName, std::format("{} ring all reduce fake data of type {}, size: {}, fake parameters: {}", rankID, getTypename<T>(), size, to_string(u...)));
            if (size < GroupSize()) {
                throw std::runtime_error("ring all reduce size must be no less than group size");
            }
            auto partition = std::ceil(1.0 * size / GroupSize());
            auto ranks_sorted = ranks;
            std::sort(ranks_sorted.begin(), ranks_sorted.end());
            auto index = std::count_if(ranks.begin(), ranks.end(), [this](const auto &item) { return item < rankID; });
            auto send_target = ranks_sorted[(index + GroupSize() - 1) % GroupSize()];
            auto receive_target = ranks_sorted[(index + 1) % GroupSize()];
            // ring scatter reduce
            for (std::size_t i = 0; i < GroupSize() - 1; ++i) {
                auto send_partition_index = (index + i) % GroupSize();
                auto receive_partition_index = (index + i + 1) % GroupSize();
                auto send_offset = partition * send_partition_index;
                auto send_partition_size = std::min(partition, size - send_offset);
                auto receive_offset = partition * receive_partition_index;
                auto receive_partition_size = std::min(partition, size - receive_offset);
                auto send = Send<std::vector<T>>(p, send_target, send_partition_size, u...);
                auto receive = Recv<std::vector<T>>(p, receive_target, receive_partition_size, u...);
                co_await send;
                co_await receive;
            }
            // all gather
            std::unordered_map<MPIRankIDType, std::tuple<std::size_t, U...>> parameter_map{};
            for (std::size_t i = 0; i < GroupSize() - 1; ++i) {
                auto send_partition_index = (index + i + GroupSize() - 1) % GroupSize();
                auto receive_partition_index = (index + i) % GroupSize();
                auto send_offset = partition * send_partition_index;
                auto send_partition_size = std::min(partition, size - send_offset);
                auto receive_offset = partition * receive_partition_index;
                auto receive_partition_size = std::min(partition, size - receive_offset);
                auto send = Send<std::vector<T>>(p, send_target, send_partition_size, u...);
                auto receive = Recv<std::vector<T>>(p, receive_target, receive_partition_size, u...);
                co_await send;
                co_await receive;
            }
        }

        template<MPIWritable S, MPIReadable R>
        CoroutineOperation<std::unordered_map<MPIRankIDType, R>> AllToAll(const std::unordered_map<MPIRankIDType, S> &data) {
            logDebug(logName, std::format("{} all to all data of type S: {} and type R: {}", rankID, getTypename<S>(), getTypename<R>()));
            std::vector<CoroutineOperation<void>> sendOperations;
            std::unordered_map<MPIRankIDType, CoroutineOperation<R>> recvOperations;
            for (auto &[rank, s]: data) {
                sendOperations.push_back(Send(rank, s));
                recvOperations[rank] = Recv<R>(rank);
            }
            std::unordered_map<MPIRankIDType, R> result;
            for (auto &o: sendOperations) {
                co_await o;
            }
            for (auto &[rank, o]: recvOperations) {
                result[rank] = std::move(co_await o);
            }
            co_return result;
        }

        template<MPIObject T>
        CoroutineOperation<std::unordered_map<MPIRankIDType, T>> AllToAll(const std::unordered_map<MPIRankIDType, T> &data) {
            logDebug(logName, std::format("{} all to all data of type {}", rankID, getTypename<T>()));
            return AllToAll<T, T>(data);
        }

        template<typename S, typename R, typename ...US, typename ...UR>
        requires MPIFakeWritable<S, US...> and MPIFakeReadable<R, UR...>
        CoroutineOperation<void> AllToAll(MPIFakePacket p, const std::unordered_map<MPIRankIDType, std::tuple<US...>> &uS, const std::unordered_map<MPIRankIDType, std::tuple<UR...>> &uR, int taskId = -1) {
            logDebug(logName, std::format("{} all to all fake data of type S: {} and type R: {}, fake parameters omitted", rankID, getTypename<S>(), getTypename<R>()));
            
            std::vector<CoroutineOperation<void>> operations;

            // Sends (No FCT measurement for sends)
            for (auto &[rank, u]: uS) {
                operations.push_back(std::apply([this, &p, &rank](auto &&...args) { return this->Send<S>(p, rank, std::forward<decltype(args)>(args)...); }, u));
            }

            // Recvs (Measure FCT here)
            for (auto &[rank, u]: uR) {
                // Define wrapper for measurement
                auto measuredRecv = [this, &p, &rank, taskId](auto&&... args) -> CoroutineOperation<void> {
                    auto start = Simulator::Now();
                    co_await this->Recv<R>(p, rank, std::forward<decltype(args)>(args)...);
                    auto end = Simulator::Now();
                    
                    if (taskId >= 0 && m_fctCallback) {
                        m_fctCallback(taskId, (uint32_t)rank, (uint32_t)this->rankID, start.GetSeconds(), end.GetSeconds());
                    }
                };

                operations.push_back(std::apply(measuredRecv, u));
            }
            
            /*
            // Log Sends (Flow Start)
            if (taskId >= 0) {
                 for (auto &[rank, u] : uS) {
                     // Verify size > 0? The tuple contains size.
                     // std::tuple<size_t> for FakeDataPacket.
                     // Accessing tuple element 0 is safe for FakeDataPacket logic in AllToAllV
                     // std::cout << "[FlowStart] TaskId=" << taskId << " Src=" << rankID << " Dst=" << rank << " Time=" << Simulator::Now().GetSeconds() << std::endl;
                     // Better: Log to stderr or a slightly parseable format
                     // Format: [FlowTx] <TaskId> <Src> <Dst> <Time>
                     // std::cout << "[FlowTx] " << taskId << " " << rankID << " " << rank << " " << Simulator::Now().GetSeconds() << std::endl;
                 }
            }
            */
            
            for (auto &o: operations) {
                co_await o;
            }
        }

        template<typename S, typename R, typename ...US, typename ...UR>
        requires MPIFakeWritable<S, US...> and MPIFakeReadable<R, UR...>
        CoroutineOperation<void> AllToAll(MPIFakePacket p, const std::tuple<US...> &uS, const std::tuple<UR...> &uR) {
            logDebug(logName, std::format("{} all to all fake data of type S: {} and type R: {}, fake parameters S: {}, fake parameters R: {}", rankID, getTypename<S>(), getTypename<R>(), to_string(uS), to_string(uR)));
            std::unordered_map<MPIRankIDType, std::tuple<US...>> uSMap;
            std::unordered_map<MPIRankIDType, std::tuple<UR...>> uRMap;
            for (auto rank: sockets | std::ranges::views::keys) {
                uSMap[rank] = uS;
                uRMap[rank] = uR;
            }
            return AllToAll<S, R>(p, uSMap, uRMap, -1);
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> AllToAll(MPIFakePacket p, const std::unordered_map<MPIRankIDType, std::tuple<U...>> &uS, const std::unordered_map<MPIRankIDType, std::tuple<U...>> &uR, int taskId = -1) {
            logDebug(logName, std::format("{} all to all fake data of type {}", rankID, getTypename<T>()));
            return AllToAll<T, T>(p, uS, uR, taskId);
        }

        template<typename T, typename ...U>
        requires MPIFakeObject<T, U...>
        CoroutineOperation<void> AllToAll(MPIFakePacket p, const std::tuple<U...> &uS, const std::tuple<U...> &uR) {
            logDebug(logName, std::format("{} all to all fake data of type {}, fake parameters S: {}, fake parameters R: {}", rankID, getTypename<T>(), to_string(uS), to_string(uR)));
            std::unordered_map<MPIRankIDType, std::tuple<U...>> uSMap;
            std::unordered_map<MPIRankIDType, std::tuple<U...>> uRMap;
            for (auto rank: sockets | std::ranges::views::keys) {
                uSMap[rank] = uS;
                uRMap[rank] = uR;
            }
            return AllToAll<T, T>(p, uSMap, uRMap, -1);
        }

        /**
         * @brief 简化的 AllToAllV 实现，专门用于模拟数据传输。
         * 只需要提供每个 Rank 对应的发送和接收数据量（字节数或数量）。
         * 
         * @tparam T 传输的对象类型，默认为 FakeDataPacket (以字节为单位)
         * @param p 模拟包标识符
         * @param sendSizes Map: { 目标Rank -> 发送数量 }
         * @param recvSizes Map: { 来源Rank -> 接收数量 }
         */
        template<typename T = FakeDataPacket>
        CoroutineOperation<void> AllToAllV(MPIFakePacket p, 
                                          const std::unordered_map<MPIRankIDType, std::size_t> &sendSizes, 
                                          const std::unordered_map<MPIRankIDType, std::size_t> &recvSizes,
                                          int taskId = -1) {
            std::unordered_map<MPIRankIDType, std::tuple<std::size_t>> uSMap, uRMap;
            
            // 自动将普通的 size 包装成 AllToAll 需要的 tuple 格式
            for (const auto& [rank, size] : sendSizes) {
                uSMap[rank] = std::make_tuple(size);
            }
            for (const auto& [rank, size] : recvSizes) {
                uRMap[rank] = std::make_tuple(size);
            }
            
            // 调用现有的 AllToAll 实现
            return AllToAll<T, T>(p, uSMap, uRMap, taskId);
        }


        void Block() noexcept;

        void Unblock() noexcept;

        std::size_t TxBytes() const noexcept;

        std::size_t RxBytes() const noexcept;

        void Close();

        MPIRankIDType RankID() const noexcept;

        std::set<MPIRankIDType> GroupMembers() const noexcept;

        inline std::size_t GroupSize() const noexcept {
            return sockets.size();
        }

        virtual ~MPICommunicator() = default;
    };
}

#endif // NS3_MPI_APPLICATION_COMMUNICATOR_H
