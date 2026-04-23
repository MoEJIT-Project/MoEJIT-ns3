//
// Created by Ricardo Evans on 2023/6/18.
//

#ifndef NS3_MPI_APPLICATION_PROTOCOL_TRAIT_H
#define NS3_MPI_APPLICATION_PROTOCOL_TRAIT_H

#include <cstdint>
#include <concepts>
#include <vector>

#include <ns3/coroutine-module.h>
#include <ns3/network-module.h>

#include "mpi-util.h"

namespace ns3 {
    using MPIRankIDType = uint64_t;
    using NS3Packet = Ptr<Packet>;

    enum class MPIOperator {
        SUM,
        PRODUCT,
        MAX,
        MIN,
        LOGICAL_AND,
        BITWISE_AND,
        LOGICAL_OR,
        BITWISE_OR,
        LOGICAL_XOR,
        BITWISE_XOR,
    };

    struct MPIRawPacket {
    };

    struct MPIFakePacket {
    };

    template<typename T>
    struct MPIObjectReader;

    template<typename T>
    struct MPIObjectWriter;

    template<MPIOperator O, typename T>
    struct MPIOperatorImplementation;

    template<typename T>
    concept MPIReadable=requires(MPIObjectReader<T> reader){
        { reader(std::declval<CoroutineSocket &>()) } -> std::same_as<CoroutineOperation<T>>;
    };

    template<typename T>
    concept MPIBatchReadable=MPIReadable<T> and requires(MPIObjectReader<T> reader){
        { reader.size() } -> std::convertible_to<std::size_t>;
        { reader(std::declval<NS3Packet>()) } -> std::convertible_to<T>;
    };

    template<typename T>
    concept MPIWritable=requires(MPIObjectWriter<T> writer){
        { writer(std::declval<CoroutineSocket &>(), std::declval<T>()) } -> std::same_as<CoroutineOperation<void>>;
    };

    template<typename T>
    concept MPIBatchWritable=MPIWritable<T> and requires(MPIObjectWriter<T> writer){
        { writer.size(std::declval<T>()) } -> std::convertible_to<std::size_t>;
        { writer(std::declval<NS3Packet>(), std::declval<T>()) };
    };

    template<typename T>
    concept MPIObject = MPIReadable<T> and MPIWritable<T>;

    template<typename T>
    concept MPIBatchableObject = MPIBatchReadable<T> and MPIBatchWritable<T>;

    template<typename T, typename ...U>
    concept MPIFakeReadable=requires(MPIObjectReader<T> reader){
        { reader(std::declval<CoroutineSocket &>(), std::declval<MPIFakePacket>(), std::declval<U>()...) } -> std::same_as<CoroutineOperation<void>>;
    };

    template<typename T, typename ...U>
    concept MPIFakeBatchReadable=MPIFakeReadable<T, U...> and requires(MPIObjectReader<T> reader){
        { reader.size(std::declval<MPIFakePacket>(), std::declval<U>()...) } -> std::convertible_to<std::size_t>;
    };

    template<typename T, typename ...U>
    concept MPIFakeWritable=requires(MPIObjectWriter<T> writer){
        { writer(std::declval<CoroutineSocket &>(), std::declval<MPIFakePacket>(), std::declval<U>()...) } -> std::same_as<CoroutineOperation<void>>;
    };

    template<typename T, typename ...U>
    concept MPIFakeBatchWritable=MPIFakeWritable<T, U...> and requires(MPIObjectWriter<T> writer){
        { writer.size(std::declval<MPIFakePacket>(), std::declval<U>()...) } -> std::convertible_to<std::size_t>;
    };

    template<typename T, typename ...U>
    concept MPIFakeObject = MPIFakeReadable<T, U...> and MPIFakeWritable<T, U...>;

    template<typename T, typename ...U>
    concept MPIFakeBatchableObject = MPIFakeBatchReadable<T, U...> and MPIFakeBatchWritable<T, U...>;

    template<typename T>
    concept MPIAddable=requires(T t1, T t2){
        { t1 + t2 } -> std::same_as<T>;
        { T::AdditionUnit } -> std::same_as<T>;
    };

    template<typename T>
    concept MPIMultiplicative=requires(T t1, T t2){
        { t1 * t2 } -> std::same_as<T>;
        { T::MultiplicationUnit } -> std::same_as<T>;
    };

    template<MPIOperator O, typename T, typename ...P>
    concept MPIOperatorApplicable=requires(MPIOperatorImplementation<O, T> o){
        { o(std::declval<std::vector<T>>(), std::declval<P>()...) } -> std::convertible_to<T>;
    };
}
#endif //NS3_MPI_APPLICATION_PROTOCOL_TRAIT_H
