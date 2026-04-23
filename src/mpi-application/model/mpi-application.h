//
// Created by Ricardo Evans on 2023/6/1.
//

#ifndef NS3_MPI_APPLICATION_H
#define NS3_MPI_APPLICATION_H

#include <chrono>
#include <coroutine>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <unordered_map>

#include <ns3/core-module.h>
#include <ns3/coroutine-module.h>
#include <ns3/network-module.h>

#include "mpi-communicator.h"

namespace ns3 {
    using MPIRequestIDType = uint64_t;

    static const constinit
    MPIRequestIDType NULL_REQUEST = 1;

    class MPIApplication;

    using MPIFunction = std::function<CoroutineOperation<void>(MPIApplication &)>;


    class MPIApplication : public Application {
    public:
        enum class Status {
            INITIAL,
            WORKING,
            FINALIZED,
        };
    private:
        using NS3Node = Ptr<Node>;

        bool running = false;
        Status status = Status::INITIAL;
        MPIRankIDType rankID;
        std::map<MPIRankIDType, Address> addresses;
        std::map<Address, MPIRankIDType> ranks;
        std::queue<MPIFunction> functions;
        std::shared_ptr<std::mt19937> randomEngine;
        std::unordered_map<MPICommunicatorIDType, MPICommunicator> communicators;

        static CoroutineOperation<std::unordered_map<MPIRankIDType, std::shared_ptr<CoroutineSocket>>> connect(size_t cache_limit, MPIRankIDType rankID, NS3Node node, const std::map<MPIRankIDType, Address> &addresses, const std::map<Address, MPIRankIDType> &ranks);

    public:
        std::unordered_map<MPIRequestIDType, CoroutineOperation<void>> requests;

        MPIApplication(
                MPIRankIDType rankID,
                std::map<MPIRankIDType, Address> &&addresses,
                std::map<Address, MPIRankIDType> &&ranks,
                std::queue<std::function<CoroutineOperation<void>(MPIApplication &)>> &&functions) noexcept;

        MPIApplication(
                MPIRankIDType rankID,
                std::map<MPIRankIDType, Address> &&addresses,
                std::map<Address, MPIRankIDType> &&ranks,
                std::queue<std::function<CoroutineOperation<void>(MPIApplication &)>> &&functions,
                std::mt19937::result_type seed) noexcept;

        MPIApplication(
                MPIRankIDType rankID,
                const std::map<MPIRankIDType, Address> &addresses,
                const std::map<Address, MPIRankIDType> &ranks,
                std::queue<std::function<CoroutineOperation<void>(MPIApplication &)>> &&functions) noexcept;

        MPIApplication(
                MPIRankIDType rankID,
                const std::map<MPIRankIDType, Address> &addresses,
                const std::map<Address, MPIRankIDType> &ranks,
                std::queue<std::function<CoroutineOperation<void>(MPIApplication &)>> &&functions,
                std::mt19937::result_type seed) noexcept;

        MPIApplication(const MPIApplication &application) = delete;

        MPIApplication(MPIApplication &&application) noexcept = default;

        MPIApplication &operator=(const MPIApplication &application) = delete;

        MPIApplication &operator=(MPIApplication &&application) noexcept = default;

        void StartApplication() override;

        void StopApplication() override;

        CoroutineOperation<void> run();

        CoroutineOperation<void> Initialize(size_t mtu_size = 1492);

        void Finalize();

        void Block() noexcept;

        void Unblock() noexcept;

        template<typename R, typename P>
        CoroutineOperation<void> Compute(const std::chrono::duration<R, P> &duration) {
            auto o = makeCoroutineOperation<void>();
            Simulator::Schedule(convert(duration), [o]() { o.terminate(); });
            co_await o;
        }

        MPICommunicator &communicator(const MPICommunicatorIDType id);

        MPICommunicator &duplicate_communicator(const MPICommunicatorIDType oldID, const MPICommunicatorIDType newID);

        void free_communicator(const MPICommunicatorIDType id);

        bool Initialized() const noexcept;

        bool Finalized() const noexcept;

        void AddFunction(MPIFunction new_function){
            functions.push(new_function);
        }

        int GetFunctionSize(){
            return functions.size();
        }

        void ChangeRunning(bool state){
            running = state;
        }

        ~MPIApplication() override = default;
    };
}

#endif // NS3_MPI_APPLICATION_H
