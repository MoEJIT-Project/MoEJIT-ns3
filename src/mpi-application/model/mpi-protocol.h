//
// Created by Ricardo Evans on 2023/6/20.
//

#ifndef NS3_MPI_APPLICATION_PROTOCOL_H
#define NS3_MPI_APPLICATION_PROTOCOL_H

#include <algorithm>
#include <concepts>
#include <numeric>
#include <ranges>

#include <ns3/network-module.h>

#include "mpi-protocol-trait.h"

namespace ns3 {
    using NS3Packet = Ptr<Packet>;
    using NS3Error = Socket::SocketErrno;

    constinit const MPIRawPacket RawPacket{};

    constinit const MPIFakePacket FakePacket{};

    template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, uint128_t> || std::is_same_v<T, int128_t>
    struct MPIObjectReader<T> {
        CoroutineOperation<T> operator()(CoroutineSocket &socket) const {
            NS3Packet result = co_await read(socket, type_size);
            co_return operator()(std::move(result));
        }

        T operator()(NS3Packet packet) const {
            T data;
            packet->CopyData(reinterpret_cast<uint8_t *>(&data), type_size);
            packet->RemoveAtStart(type_size);
            return std::move(data);
        }

        CoroutineOperation<void> operator()(CoroutineSocket &socket, MPIFakePacket) const {
            co_await read(socket, type_size);
        }

        constexpr static std::size_t size() {
            return type_size;
        }

        constexpr static std::size_t size(MPIFakePacket) {
            return type_size;
        }

    private:
        static constinit const std::size_t type_size = sizeof(T);

        CoroutineOperation<NS3Packet> read(CoroutineSocket &socket, std::size_t s) const {
            auto [packet, error] = co_await socket.receive(s);
            if (error != NS3Error::ERROR_NOTERROR) {
                throw CoroutineSocketException{"Parse " + std::string{typeid(T).name()} + " failed, reason: " + format(error)};
            }
            co_return packet;
        }
    };

    template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, uint128_t> || std::is_same_v<T, int128_t>
    struct MPIObjectWriter<T> {
        CoroutineOperation<void> operator()(CoroutineSocket &socket, const T &t) const {
            auto packet = Create<Packet>();
            operator()(packet, t);
            co_await write(socket, std::move(packet));
        }

        void operator()(NS3Packet packet, const T &t) const {
            packet->AddAtEnd(Create<Packet>(reinterpret_cast<const uint8_t *>(&t), type_size));
        }

        CoroutineOperation<void> operator()(CoroutineSocket &socket, MPIFakePacket) const {
            co_await write(socket, Create<Packet>(type_size));
        }

        constexpr static std::size_t size(T) {
            return type_size;
        }

        constexpr static std::size_t size(MPIFakePacket) {
            return type_size;
        }

    private:
        static constexpr const std::size_t type_size = sizeof(T);

        CoroutineOperation<void> write(CoroutineSocket &socket, NS3Packet packet) const {
            auto [sent, error] = co_await socket.send(packet);
            if (error != NS3Error::ERROR_NOTERROR) {
                throw CoroutineSocketException{"Deparse " + std::string{typeid(T).name()} + " failed, reason: " + format(error)};
            }
        }
    };

    struct FakeDataPacket {
    };

    template<>
    struct MPIObjectReader<FakeDataPacket> {
        constexpr static std::size_t size(MPIFakePacket, std::size_t packet_size) {
            return packet_size;
        }

        CoroutineOperation<void> operator()(CoroutineSocket &socket, MPIFakePacket, std::size_t packet_size) const {
            auto [packet, error] = co_await socket.receive(packet_size);
            if (error != NS3Error::ERROR_NOTERROR) {
                throw CoroutineSocketException{std::string{"Read fake data packet failed, reason: "} + format(error)};
            }
        }
    };

    template<>
    struct MPIObjectWriter<FakeDataPacket> {
        constexpr static std::size_t size(MPIFakePacket, std::size_t packet_size) {
            return packet_size;
        }

        CoroutineOperation<void> operator()(CoroutineSocket &socket, MPIFakePacket, std::size_t packet_size) const {
            auto [sent, error] = co_await socket.send(Create<Packet>(packet_size));
            if (error != NS3Error::ERROR_NOTERROR) {
                throw CoroutineSocketException{std::string{"Write fake data packet failed, reason: "} + format(error)};
            }
        }
    };

    template<typename T>
    struct MPIObjectReader<std::vector<T>> {
        CoroutineOperation<std::vector<T>> operator()(CoroutineSocket &socket) const requires MPIReadable<T> {
            std::size_t count = co_await MPIObjectReader<std::size_t>{}(socket);
            std::vector<T> result;
            MPIObjectReader<T> reader;
            if constexpr (MPIBatchReadable<T>) {
                auto size = reader.size() * count;
                auto [packet, error] = co_await socket.receive(size);
                if (error != NS3Error::ERROR_NOTERROR) {
                    throw CoroutineSocketException{"Batch read vector failed, reason: " + format(error)};
                }
                for (std::size_t i = 0; i < count; ++i) {
                    result.push_back(reader(packet));
                }
            } else {
                for (std::size_t i = 0; i < count; ++i) {
                    result.push_back(std::move(co_await reader(socket)));
                }
            }
            co_return result;
        }

        template<typename ...U>
        CoroutineOperation<void> operator()(CoroutineSocket &socket, MPIFakePacket p, std::size_t count, U &&...u) const requires MPIFakeReadable<T, U...> {
            co_await MPIObjectReader<std::size_t>{}(socket, p);
            MPIObjectReader<T> reader;
            if constexpr (MPIFakeBatchReadable<T, U...>) {
                auto size = count * reader.size(p, std::forward<U>(u)...);
                co_await MPIObjectReader<FakeDataPacket>{}(socket, p, size);
            } else {
                while (count-- > 0) {
                    co_await reader(socket, p, std::forward<U>(u)...);
                }
            }
        }
    };

    template<typename T>
    struct MPIObjectWriter<std::vector<T>> {
        CoroutineOperation<void> operator()(CoroutineSocket &socket, const std::vector<T> &vector) const requires MPIWritable<T> {
            co_await MPIObjectWriter<std::size_t>{}(socket, vector.size());
            MPIObjectWriter<T> writer;
            if constexpr (MPIBatchWritable<T>) {
                auto packet = Create<Packet>();
                for (auto &t: vector) {
                    writer(packet, t);
                }
                auto [size, error] = co_await socket.send(packet);
                if (error != NS3Error::ERROR_NOTERROR) {
                    throw CoroutineSocketException{"Batch write vector failed, reason: " + format(error)};
                }
            } else {
                for (auto &t: vector) {
                    co_await writer(socket, t);
                }
            }
        }

        template<typename ...U>
        CoroutineOperation<void> operator()(CoroutineSocket &socket, MPIFakePacket p, std::size_t count, U &&...u) const requires MPIFakeWritable<T, U...> {
            co_await MPIObjectWriter<std::size_t>{}(socket, count);
            MPIObjectWriter<T> writer;
            if constexpr (MPIFakeBatchWritable<T, U...>) {
                auto size = count * writer.size(p, std::forward<U>(u)...);
                co_await MPIObjectWriter<FakeDataPacket>{}(socket, p, size);
            } else {
                while (count-- > 0) {
                    co_await writer(socket, p, std::forward<U>(u)...);
                }
            }
        }
    };

    template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T>
    struct MPIOperatorImplementation<MPIOperator::SUM, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T{0}, std::plus{});
        }
    };

    template<MPIAddable T>
    struct MPIOperatorImplementation<MPIOperator::SUM, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T::AdditionUnit, std::plus{});
        }
    };

    template<typename T>
    struct MPIOperatorImplementation<MPIOperator::SUM, T> {
        template<std::ranges::range R, typename O, typename U=T>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r, U &&unit, O &&o) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), std::forward<U>(unit), std::forward<O>(o));
        }
    };

    template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T>
    struct MPIOperatorImplementation<MPIOperator::PRODUCT, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T{1}, std::multiplies{});
        }
    };

    template<MPIMultiplicative T>
    struct MPIOperatorImplementation<MPIOperator::PRODUCT, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T::MultiplicationUnit, std::multiplies{});
        }
    };

    template<typename T>
    struct MPIOperatorImplementation<MPIOperator::PRODUCT, T> {
        template<std::ranges::range R, typename O, typename U=T>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r, U &&unit, O &&o) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), std::forward<U>(unit), std::forward<O>(o));
        }
    };

    template<typename T>
    struct MPIOperatorImplementation<MPIOperator::MAX, T> {
        template<std::ranges::range R, typename C=std::ranges::less, typename P=std::identity>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r, C &&comparator = {}, P &&projectile = {}) const {
            return *std::ranges::max_element(std::forward<R>(r), std::forward<C>(comparator), std::forward<P>(projectile));
        }
    };

    template<typename T>
    struct MPIOperatorImplementation<MPIOperator::MIN, T> {
        template<std::ranges::range R, typename C=std::ranges::less, typename P=std::identity>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r, C &&comparator = {}, P &&projectile = {}) const {
            return *std::ranges::min_element(std::forward<R>(r), std::forward<C>(comparator), std::forward<P>(projectile));
        }
    };

    template<typename T> requires std::is_integral_v<T>
    struct MPIOperatorImplementation<MPIOperator::BITWISE_AND, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), ~T{0}, std::bit_and{});
        }
    };

    template<typename T> requires std::is_integral_v<T>
    struct MPIOperatorImplementation<MPIOperator::BITWISE_OR, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T{0}, std::bit_or{});
        }
    };

    template<typename T> requires std::is_integral_v<T>
    struct MPIOperatorImplementation<MPIOperator::BITWISE_XOR, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T{0}, std::bit_xor{});
        }
    };

    template<typename T> requires std::is_integral_v<T>
    struct MPIOperatorImplementation<MPIOperator::LOGICAL_AND, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), ~T{0}, std::logical_and{});
        }
    };

    template<typename T> requires std::is_integral_v<T>
    struct MPIOperatorImplementation<MPIOperator::LOGICAL_OR, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T{0}, std::logical_or{});
        }
    };

    template<typename T> requires std::is_integral_v<T>
    struct MPIOperatorImplementation<MPIOperator::LOGICAL_XOR, T> {
        template<std::ranges::range R>
        requires std::is_same_v<std::ranges::range_value_t<R>, T>
        T operator()(R &&r) const {
            return std::accumulate(std::ranges::begin(r), std::ranges::end(r), T{0}, std::bit_xor{});
        }
    };
}

#endif //NS3_MPI_APPLICATION_PROTOCOL_H
