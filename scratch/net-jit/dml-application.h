//
// Created by Ricardo Evans on 2024/4/27.
//

#ifndef NS3_DML_APPLICATION_H
#define NS3_DML_APPLICATION_H

#include "net-jit-application.h"

#include <ns3/applications-module.h>

#include <chrono>
#include <memory>

namespace ns3
{

using BatchID = int64_t;
using SizeType = int64_t;
using AppId = int64_t;

struct coInfo
{
    AppId appId;
    BatchID batchId;
    size_t comSize;
    uint64_t eTime = 0;
};

struct CompareByComSize
{
    bool operator()(const coInfo& a, const coInfo& b) const
    {
        // 返回 true 表示 a 的优先级低于 b（即 a 排在 b 后面）
        if (a.comSize != b.comSize)
        {
            return a.comSize > b.comSize;
        }
        return a.eTime > b.eTime;
    }

    //        {
    //            // 添加元素
    //            coInfo data = {...};
    //            pq.push(data);
    //
    //            // 访问最小元素（队首）
    //            coInfo smallest = pq.top();
    //
    //            // 删除最小元素
    //            pq.pop();
    //        }
};

extern std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
extern std::vector<int> batchFlag;
extern std::vector<std::vector<int>> batchFlags;
extern std::vector<std::unordered_set<void*>> AppAddrSet;

template <typename R, typename P>
struct DMLBatchTrace
{
    using TimeType = std::chrono::duration<R, P>;
    BatchID batch;
    TimeType batch_start_time;
    TimeType batch_end_time;
    TimeType communication_start_time;
    TimeType communication_end_time;
    SizeType communication_size;
};

template <typename R, typename P>
struct DMLWithNetJITTrace
{
    using TimeType = std::chrono::duration<R, P>;
    SizeType tensor_size; // the size of the tensor been all reduced (in bytes)
    TimeType offset;      // time elapsed before the first batch
    std::vector<DMLBatchTrace<R, P>> batches;
    std::vector<NetJITPrediction<R, P>> predictions;
};

template <typename R, typename P>
class DMLApplicationHelper
{
  public:
    using TimeType = std::chrono::duration<R, P>;
    using RandomDisturbanceGenerator = std::function<TimeType(MPIRankIDType, BatchID)>;
    using PredictionCallback = std::function<void(MPIRankIDType, BatchID, TimeType, SizeType)>;
    using BatchCallback = std::function<void(MPIRankIDType, BatchID)>;
    using ApplicationCallback = std::function<void(MPIRankIDType)>;
    using VarysScheduleCallback =
        std::function</*start_delay*/ TimeType(MPIRankIDType, BatchID, unsigned long, TimeType)>;
    using NodeAppId2Mpi = std::vector<std::vector<Ptr<MPIApplication>>>; //[nodeid][appid]

    struct Callbacks
    {
        PredictionCallback prediction_callback;
        BatchCallback batch_start_callback;
        BatchCallback batch_end_callback;
        BatchCallback batch_communication_start_callback;
        BatchCallback batch_communication_end_callback;
        ApplicationCallback application_start_callback;
        ApplicationCallback application_end_callback;
        VarysScheduleCallback varys_schedule_callback;
    };

  private:
    using NS3Node = Ptr<Node>;
    DMLWithNetJITTrace<R, P> trace;
    std::unordered_map<MPIRankIDType, NS3Node> nodes;
    std::map<MPIRankIDType, Address> addresses;
    std::map<Address, MPIRankIDType> ranks;
    DMLApplicationHelper<R, P>* dml1;
    DMLApplicationHelper<R, P>* dml2;
    NodeAppId2Mpi nodeAppId2Mpi;
    std::vector<int> batchCount = {0, 0};

  public:
    void GlobalInitiate()
    {
        nodeAppId2Mpi.resize(8, std::vector<Ptr<MPIApplication>>(2));
        auto nodeCnt = 0;
        // 0号应用是netjit, 1号是mpiApp
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp1 = node->GetApplication(1);
            Ptr<MPIApplication> mpiApp1 = DynamicCast<MPIApplication>(retrievedApp1);
            nodeAppId2Mpi[nodeCnt][0] = mpiApp1;
            Ptr<Application> retrievedApp2 = node->GetApplication(3);
            Ptr<MPIApplication> mpiApp2 = DynamicCast<MPIApplication>(retrievedApp2);
            nodeAppId2Mpi[nodeCnt][1] = mpiApp2;
            nodeCnt++;
        }
        appAddrInit();
    }

    void AddNewDML(DMLApplicationHelper<R, P>* newdml1, DMLApplicationHelper<R, P>* newdml2)
    {
        dml1 = newdml1;
        dml2 = newdml2;
    }

    DMLApplicationHelper(DMLWithNetJITTrace<R, P>&& trace)
        : trace(std::move(trace))
    {
    }

    DMLApplicationHelper(const DMLWithNetJITTrace<R, P>& trace)
        : trace(trace)
    {
    }

    void install_node(NS3Node node, MPIRankIDType rank, Address address)
    {
        addresses[rank] = address;
        ranks[retrieveIPAddress(address)] = rank;
        nodes[rank] = node;
    }

    ApplicationContainer install(
        MPIRankIDType master,
        Callbacks callbacks,
        RandomDisturbanceGenerator disturbance_generator = no_disturbance(),
        std::size_t mtu = 1500,
        double scale_factor = 1.0)
    {
        ApplicationContainer applications{};
        for (auto& [rank, node] : nodes)
        {
            auto waited_time = std::make_shared<TimeType>();
            auto current_batch = std::make_shared<BatchID>();
            auto time_offset = std::make_shared<TimeType>(trace.offset);
            auto prediction_queue = std::queue<NetJITPrediction<R, P>>(
                std::deque<NetJITPrediction<R, P>>{trace.predictions.cbegin(),
                                                   trace.predictions.cend()});
            auto net_jit_application =
                Create<NetJITApplication<R, P>>(rank,
                                                std::move(prediction_queue),
                                                current_batch,
                                                waited_time,
                                                time_offset,
                                                callbacks.prediction_callback);
            node->AddApplication(net_jit_application);
            applications.Add(net_jit_application);
            std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>> functions{};
            // 应用初始化
            functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                if (callbacks.application_start_callback)
                {
                    callbacks.application_start_callback(rank);
                }
                co_await application.Initialize(mtu);
                auto now = convert<R, P>(Simulator::Now());
                *time_offset = now - *time_offset;
            });
            // 对每个batch
            for (auto& batch : trace.batches)
            {
                // batch通信前开始时间
                functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    if (callbacks.batch_start_callback)
                    {
                        callbacks.batch_start_callback(rank, batch.batch);
                    }
                    *current_batch = batch.batch;
                    net_jit_application->on_batch();
                    auto duration = batch.communication_start_time - batch.batch_start_time;
                    co_await application.Compute(0 * duration);
                });

                // batch通信时间
                functions.push([=, this, size = (size_t)trace.tensor_size](
                                   MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    auto start = convert<R, P>(Simulator::Now());
                    auto scaled_size = std::max((size_t)(1 * size * scale_factor), (size_t)1);

                    if (callbacks.batch_communication_start_callback)
                    {
                        callbacks.batch_communication_start_callback(rank, batch.batch);
                    }

                    //                    co_await
                    //                    application.Compute(std::chrono::nanoseconds(100));
                    //                    //test::给所有 rank 一个“短小的同步窗口”
                    co_await application.communicator(WORLD_COMMUNICATOR)
                        .template RingAllReduce<uint8_t>(FakePacket, scaled_size);

                    if (callbacks.batch_communication_end_callback)
                    {
                        callbacks.batch_communication_end_callback(rank, batch.batch);
                    }
                    auto end = convert<R, P>(Simulator::Now());
                    *waited_time += (end - start);
                });
                // batch通信后结束时间
                functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    auto disturbance = disturbance_generator(rank, batch.batch);
                    auto duration =
                        batch.batch_end_time - batch.communication_end_time + disturbance;
                    NS_ASSERT(duration.count() >= 0);
                    co_await application.Compute(0 * duration);
                    *waited_time += disturbance;
                    if (callbacks.batch_end_callback)
                    {
                        callbacks.batch_end_callback(rank, batch.batch);
                    }
                });
            }
            // 应用结束
            functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                application.Finalize();
                if (callbacks.application_end_callback)
                {
                    callbacks.application_end_callback(rank);
                }
                co_return;
            });
            // 把上述的若干操作打包安装到application同其中
            auto mpi_application =
                Create<MPIApplication>(rank, addresses, ranks, std::move(functions));
            node->AddApplication(mpi_application);
            applications.Add(mpi_application);
        }
        if (dml2)
        {
            appAddrInit();
        }
        return applications;
    }

    ApplicationContainer install_alltoallv(
        MPIRankIDType master,
        Callbacks callbacks,
        RandomDisturbanceGenerator disturbance_generator = no_disturbance(),
        std::size_t mtu = 1500,
        double scale_factor = 1.0)
    {
        ApplicationContainer applications{};
        for (auto& [rank, node] : nodes)
        {
            auto waited_time = std::make_shared<TimeType>();
            auto current_batch = std::make_shared<BatchID>();
            auto time_offset = std::make_shared<TimeType>(trace.offset);
            auto prediction_queue = std::queue<NetJITPrediction<R, P>>(
                std::deque<NetJITPrediction<R, P>>{trace.predictions.cbegin(),
                                                   trace.predictions.cend()});
            auto net_jit_application =
                Create<NetJITApplication<R, P>>(rank,
                                                std::move(prediction_queue),
                                                current_batch,
                                                waited_time,
                                                time_offset,
                                                callbacks.prediction_callback);
            node->AddApplication(net_jit_application);
            applications.Add(net_jit_application);
            std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>> functions{};
            // 应用初始化
            functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                if (callbacks.application_start_callback)
                {
                    callbacks.application_start_callback(rank);
                }
                co_await application.Initialize(mtu);
                auto now = convert<R, P>(Simulator::Now());
                *time_offset = now - *time_offset;
            });
            // 对每个batch
            for (auto& batch : trace.batches)
            {
                // batch通信前开始时间
                functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    if (callbacks.batch_start_callback)
                    {
                        callbacks.batch_start_callback(rank, batch.batch);
                    }
                    *current_batch = batch.batch;
                    net_jit_application->on_batch();
                    auto duration = batch.communication_start_time - batch.batch_start_time;
                    co_await application.Compute(0 * duration);
                });

                // batch通信时间 - 使用 AllToAllV 替代 RingAllReduce
                functions.push([=, this](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    auto start = convert<R, P>(Simulator::Now());
                    
                    if (callbacks.batch_communication_start_callback)
                    {
                        callbacks.batch_communication_start_callback(rank, batch.batch);
                    }

                    // 构建简单的小流量模式进行测试
                    // 模式：发送给每个rank (rank+target)*100 字节
                    // 这样 rank A 发送给 B 的数据量为 (A+B)*100
                    // rank B 从 A 接收的数据量也为 (A+B)*100，符合匹配规则
                    auto& comm = application.communicator(WORLD_COMMUNICATOR);
                    auto groupSize = comm.GroupSize();
                    std::unordered_map<MPIRankIDType, std::size_t> sendSizes;
                    std::unordered_map<MPIRankIDType, std::size_t> recvSizes;
                    
                    for (size_t i = 0; i < groupSize; ++i) {
                        sendSizes[i] = (rank + i) * 100;
                        recvSizes[i] = (i + rank) * 100;
                    }

                    co_await comm.AllToAllV(FakePacket, sendSizes, recvSizes);

                    if (callbacks.batch_communication_end_callback)
                    {
                        callbacks.batch_communication_end_callback(rank, batch.batch);
                    }
                    auto end = convert<R, P>(Simulator::Now());
                    *waited_time += (end - start);
                });
                
                // batch通信后结束时间
                functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    auto disturbance = disturbance_generator(rank, batch.batch);
                    auto duration =
                        batch.batch_end_time - batch.communication_end_time + disturbance;
                    NS_ASSERT(duration.count() >= 0);
                    co_await application.Compute(0 * duration);
                    *waited_time += disturbance;
                    if (callbacks.batch_end_callback)
                    {
                        callbacks.batch_end_callback(rank, batch.batch);
                    }
                });
            }
            // 应用结束
            functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                application.Finalize();
                if (callbacks.application_end_callback)
                {
                    callbacks.application_end_callback(rank);
                }
                co_return;
            });
            // 把上述的若干操作打包安装到application同其中
            auto mpi_application =
                Create<MPIApplication>(rank, addresses, ranks, std::move(functions));
            node->AddApplication(mpi_application);
            applications.Add(mpi_application);
        }
        if (dml2)
        {
            appAddrInit();
        }
        return applications;
    }

    static RandomDisturbanceGenerator no_disturbance()
    {
        return [](MPIRankIDType, BatchID) { return TimeType{0}; };
    }

    template <typename RandomEngine>
    static RandomDisturbanceGenerator normal_distributed_disturbance(TimeType delta,
                                                                     RandomEngine&& random_engine)
    {
        std::normal_distribution<double> distribution{0, (double)delta.count()};
        auto limit = delta.count() * 3;
        return [=, random_engine = std::forward<RandomEngine>(random_engine)](MPIRankIDType,
                                                                              BatchID) mutable {
            TimeType disturbance;
            do
            {
                disturbance = TimeType{(R)distribution(random_engine)};
            } while (std::abs(disturbance.count()) > limit);
            return disturbance;
        };
    }

    ApplicationContainer schedule_install(
        MPIRankIDType master,
        Callbacks callbacks,
        RandomDisturbanceGenerator disturbance_generator = no_disturbance(),
        std::size_t mtu = 1500,
        double scale_factor = 1.0)
    {
        ApplicationContainer applications{};
        for (auto& [rank, node] : nodes)
        {
            auto waited_time = std::make_shared<TimeType>();
            auto current_batch = std::make_shared<BatchID>();
            auto time_offset = std::make_shared<TimeType>(trace.offset);
            auto prediction_queue = std::queue<NetJITPrediction<R, P>>(
                std::deque<NetJITPrediction<R, P>>{trace.predictions.cbegin(),
                                                   trace.predictions.cend()});
            auto net_jit_application =
                Create<NetJITApplication<R, P>>(rank,
                                                std::move(prediction_queue),
                                                current_batch,
                                                waited_time,
                                                time_offset,
                                                callbacks.prediction_callback);
            node->AddApplication(net_jit_application);
            applications.Add(net_jit_application);
            std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>> functions{};
            // 应用初始化
            functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                if (callbacks.application_start_callback)
                {
                    callbacks.application_start_callback(rank);
                }
                co_await application.Initialize(mtu);
                auto now = convert<R, P>(Simulator::Now());
                *time_offset = now - *time_offset;
            });

            // 应用结束
            functions.push([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                application.Finalize();
                if (callbacks.application_end_callback)
                {
                    callbacks.application_end_callback(rank);
                }
                co_return;
            });
            // 把上述的若干操作打包安装到application同其中
            auto mpi_application =
                Create<MPIApplication>(rank, addresses, ranks, std::move(functions));
            node->AddApplication(mpi_application);
            applications.Add(mpi_application);
        }

        //        {
        //            // 模块测试区
        //            // 测试通过，注意每个DMLapp安装的时候参数设置要不一样
        //            //                addComputeAheadFunctions(applications, 0, 1,callbacks);
        //            //
        //            addComputeBehindFunctions(applications,0,1,callbacks,disturbance_generator);
        //            //                addInitializeFunctions(applications,1,callbacks,mtu);
        //            //                addFinalizeFunctions(applications, 1, callbacks);
        //            addCommunicateFunctions(0, 1, callbacks, scale_factor);
        //        }

        //            //单个插入mpi函数测试，api通过
        //            auto mpiApp = DynamicCast<MPIApplication>(applications.Get(1));
        //            mpiApp->AddFunction([=](MPIApplication &application) ->
        //            ns3::CoroutineOperation<void> {
        //                co_await application.Compute( std::chrono::nanoseconds (100000));
        //            });

        return applications;
    }

    ApplicationContainer empty_install(
        MPIRankIDType master,
        Callbacks callbacks,
        RandomDisturbanceGenerator disturbance_generator = no_disturbance(),
        std::size_t mtu = 1500,
        double scale_factor = 1.0)
    {
        ApplicationContainer applications{};
        for (auto& [rank, node] : nodes)
        {
            auto waited_time = std::make_shared<TimeType>();
            auto current_batch = std::make_shared<BatchID>();
            auto time_offset = std::make_shared<TimeType>(trace.offset);
            auto prediction_queue = std::queue<NetJITPrediction<R, P>>(
                std::deque<NetJITPrediction<R, P>>{trace.predictions.cbegin(),
                                                   trace.predictions.cend()});
            auto net_jit_application =
                Create<NetJITApplication<R, P>>(rank,
                                                std::move(prediction_queue),
                                                current_batch,
                                                waited_time,
                                                time_offset,
                                                callbacks.prediction_callback);
            node->AddApplication(net_jit_application);
            applications.Add(net_jit_application);
            std::queue<std::function<CoroutineOperation<void>(MPIApplication&)>> functions{};

            // 把上述的若干操作打包安装到application同其中
            auto mpi_application =
                Create<MPIApplication>(rank, addresses, ranks, std::move(functions));
            node->AddApplication(mpi_application);
            applications.Add(mpi_application);
        }

        return applications;
    }

    // 给某个应用的每个结点上插入ComputeAheadFunction
    void addComputeAheadFunctions(BatchID batchId, AppId appId, Callbacks callbacks)
    {
        //        std::cout << "App" << appId << " cd addComputeAhead...,cur batchid == " << batchId
        //                  << std::endl;

        if (!dml1)
        {
            std::cout << "dml1 is NULL in addComputeAhead!!!!!";
        }

        // 0号应用是netjit, 1号是mpiApp
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(appId);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            auto batch = trace.batches[batchId];
            mpiApp->AddFunction(
                [=, this](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                    std::cout << "App" << appId << " begin ComputeAhead function..." << std::endl;
                    co_await application.Compute(
                        0 * (batch.communication_start_time - batch.batch_start_time));
                    if (batchId > batchFlags[appId / 2][0])
                    {
                        batchFlags[appId / 2][0] = batchId;
                        coInfo newCoInfo{appId, batchId, GetCommunicateSize(batchId, 0.00001)};
                        ns3::Jobpq.push(newCoInfo);
                        std::cout << "Jobpq gets a new CoInfo, cur size ==  " << Jobpq.size()
                                  << std::endl;
                        if (Jobpq.size() >= 1)
                        // 只有第一轮可以直接放进去
                        {
                            checkPQ(callbacks);
                        }
                    }
                });
        }
        if (callbacks.batch_start_callback)
        {
            callbacks.batch_start_callback(-1, -1);
        }
    }

    void addComputeBehindFunctions(
        BatchID batchId,
        AppId appId,
        Callbacks callbacks,
        RandomDisturbanceGenerator disturbance_generator = no_disturbance())
    {
        std::cout << "App" << appId << " cd addComputeBehind..." << std::endl;

        // 0号应用是netjit, 1号是mpiApp
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(appId);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            addComputeBehindFunction(mpiApp,
                                     rank,
                                     batchId,
                                     appId,
                                     callbacks,
                                     disturbance_generator);
        }
        // 检测batch是否完毕，最后才能加finalize
    }

    void addComputeBehindFunction(
        Ptr<MPIApplication> mpiApp,
        MPIRankIDType rank,
        BatchID batchId,
        AppId appId,
        Callbacks callbacks,
        RandomDisturbanceGenerator disturbance_generator = no_disturbance())
    {
        std::cout << "App" << appId << " rank:" << rank
                  << " cd addComputeBehind...,cur batchid == " << batchId << std::endl;
        // 0号应用是netjit, 1号是mpiApp
        auto batch = trace.batches[batchId];
        mpiApp->AddFunction(
            [=, this](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                auto disturbance = disturbance_generator(rank, batch.batch);
                auto duration = batch.batch_end_time - batch.communication_end_time + disturbance;
                NS_ASSERT(duration.count() >= 0);
                std::cout << "App" << appId << " begin ComputeBehind function..." << std::endl;

                co_await application.Compute(0 * duration);
                if (callbacks.batch_end_callback)
                {
                    callbacks.batch_end_callback(rank, batch.batch);
                }
                if (batchId > batchFlags[appId / 2][2])
                {
                    auto nextBatchId = batchId + 1;
                    batchFlags[appId / 2][2] = batchId;
                    if (nextBatchId < batchCount[appId / 2]) // todo::该处设置循环轮次
                    {
                        if (appId == 1)
                        {
                            std::cout << "------------------app1----batch" << nextBatchId
                                      << "-------------------------------" << std::endl;
                            dml1->addComputeAheadFunctions(nextBatchId, appId, callbacks);
                        }
                        else
                        {
                            std::cout << "-------------------app3---batch" << nextBatchId
                                      << "-------------------------------" << std::endl;
                            dml2->addComputeAheadFunctions(nextBatchId, appId, callbacks);
                        }
                    }
                    else
                    {
                        if (appId == 1)
                        {
                            std::cout << "----------------app1 put finalize funcs----------------"
                                      << std::endl;
                            dml1->addFinalizeFunctions(1, callbacks);
                            //                            mpiApp->ChangeRunning(false);
                        }
                        else
                        {
                            std::cout << "----------------app3 put finalize funcs----------------"
                                      << std::endl;
                            dml2->addFinalizeFunctions(3, callbacks);
                            //                            mpiApp->ChangeRunning(false);
                        }
                    }
                }
            });
    }

    // 给某个应用的每个结点上插入Communicate
    void addCommunicateFunctions(BatchID batchId,
                                 AppId appId,
                                 Callbacks callbacks,
                                 double scale_factor = 1.0)
    {
        std::cout << "App" << appId << " cd addCom..." << std::endl;
        // 0号应用是netjit, 1号是mpiApp
        auto tmpCntPtr = std::make_shared<int>(0);
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(appId);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            addCommunicateFunction(mpiApp,
                                   rank,
                                   batchId,
                                   appId,
                                   callbacks,
                                   tmpCntPtr,
                                   scale_factor);
        }
    }

    //    void checkPQ(Callbacks callbacks)
    //    {
    //        //        std::cout << "cd checkPQ... remain jobs:" << Jobpq.size() << std::endl;
    //        //        std::cout << "cur Jobpq address: " << &Jobpq << std::endl;
    //        if (Jobpq.empty())
    //        {
    //            return;
    //        }
    //        // todo::判断是否达到最早开始时间,返回最短的最早可开始任务
    //        auto shortestJob = Jobpq.top();
    //        Jobpq.pop();
    //        auto curTime = Simulator::Now().GetNanoSeconds();
    //
    //        if (curTime < shortestJob.eTime)
    //        {
    //            if (Jobpq.size() == 1)
    //            {
    //                Jobpq.push(shortestJob);
    //                return;
    //            }
    //            else
    //            {
    //                auto tmpJob = Jobpq.top();
    //                Jobpq.pop();
    //                Jobpq.push(shortestJob); // 把原来错取出来的放回队列
    //                if (curTime < tmpJob.eTime)
    //                {
    //                    Jobpq.push(tmpJob);
    //                    return;
    //                }
    //                else
    //                {
    //                    std::cout << "get shortest job:[" << tmpJob.comSize << " " <<
    //                    tmpJob.batchId
    //                              << " " << tmpJob.appId << "]" << std::endl;
    //                    addCommunicateFunctions(tmpJob.batchId, tmpJob.appId, callbacks, 0.0001);
    //                }
    //            }
    //        }
    //
    //        std::cout << "get shortest job:[" << shortestJob.comSize << " " << shortestJob.batchId
    //                  << " " << shortestJob.appId << "]" << std::endl;
    //
    //        addCommunicateFunctions(shortestJob.batchId, shortestJob.appId, callbacks, 0.0001);
    //    }

    void checkPQ(Callbacks callbacks)
    {
        if (Jobpq.empty())
        {
            return;
        }

        std::vector<coInfo> tempJobs; // 暂存不满足时间条件的任务
        coInfo targetJob;
        bool found = false;
        uint64_t curTime = Simulator::Now().GetNanoSeconds();

        // 遍历队列，找到第一个满足时间条件的最优任务（comSize 最小）
        // todo::申请下次检测时间schedule()，根据etime 只检查不一定通信！
        while (!Jobpq.empty())
        {
            auto topJob = Jobpq.top();
            Jobpq.pop();

            if (curTime >= topJob.eTime)
            {
                // 找到目标：满足时间条件，且是 comSize 最小的（队列排序保证）
                targetJob = topJob;
                found = true;
                break;
            }
            else
            {
                // 不满足时间条件，暂存起来（后续放回队列）
                tempJobs.push_back(topJob);
            }
        }

        // 把暂存的不满足条件的任务放回队列（恢复队列原始排序）
        for (auto& job : tempJobs)
        {
            Jobpq.push(job);
        }

        // 处理找到的目标任务
        if (found)
        {
            std::cout << "get shortest job:[" << targetJob.comSize << " " << targetJob.batchId
                      << " " << targetJob.appId << "]" << std::endl;
            addCommunicateFunctions(targetJob.batchId, targetJob.appId, callbacks, 0.0001);
        }
        // 未找到满足条件的任务，直接返回
    }

    size_t GetCommunicateSize(BatchID batchId, double scale_factor = 1.0)
    {
        return scale_factor * trace.batches[batchId].communication_size;
    }

    //    TimeType GetEarliestStartTime(BatchID batchId)
    //    {
    //        auto curBatch = trace.batches[batchId];
    //        auto duration = curBatch.communication_start_time - curBatch.batch_start_time;
    //        auto nowTime = Simulator::Now().GetNanoSeconds();
    //        return nowTime + duration;
    //    }

    uint64_t GetEarliestStartTime(BatchID batchId)
    {
        // todo:: 存在越界报错
        auto curBatch = trace.batches[batchId];
        // 1. 计算时间偏移（communication_start - batch_start 的时间间隔）
        auto duration = curBatch.communication_start_time - curBatch.batch_start_time;

        // 2. 将“数值型纳秒”转换为 chrono::duration 类型
        uint64_t nowNano = Simulator::Now().GetNanoSeconds();
        auto nowDuration = std::chrono::nanoseconds(nowNano);

        // 3. 统一类型后相加，直接返回纳秒数值（int64_t）
        auto resultDuration =
            nowDuration + std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
        return resultDuration.count(); // chrono::nanoseconds的count()返回int64_t类型的纳秒数
    }

    void addCommunicateFunction(Ptr<MPIApplication> mpiApp,
                                MPIRankIDType rank,
                                BatchID batchId,
                                AppId appId,
                                Callbacks callbacks,
                                std::shared_ptr<int> tmpCntPtr,
                                double scale_factor = 1.0)
    {
        // 0号应用是netjit, 1号是mpiApp
        std::cout << "app" << appId << " add Com batch:" << batchId << " at rank" << rank
                  << std::endl;
        auto batch = trace.batches[batchId];
        mpiApp->AddFunction([=, this, size = (size_t)batch.communication_size, &tmpCntPtr](
                                MPIApplication& application) -> ns3::CoroutineOperation<void> {
            auto scaled_size = std::max((size_t)(size * scale_factor), (size_t)1);

            if (callbacks.batch_communication_start_callback)
            {
                callbacks.batch_communication_start_callback(rank, batch.batch);
            }
            std::cout << "App" << appId << " begin Communicate function..." << std::endl;

            co_await application.communicator(WORLD_COMMUNICATOR)
                .template RingAllReduce<uint8_t>(FakePacket, scaled_size);

            if (callbacks.batch_communication_end_callback)
            {
                callbacks.batch_communication_end_callback(rank, batch.batch);
            }

            if (batchId > batchFlags[appId / 2][1])
            {
                batchFlags[appId / 2][1] = batchId;
                if (appId == 1)
                {
                    //                    Simulator::ScheduleNow([this, batchId, appId, callbacks]()
                    //                    {
                    //                        dml1->addComputeBehindFunctions(batchId,
                    //                                                        appId,
                    //                                                        callbacks,
                    //                                                        no_disturbance());
                    //                        // 不要直接调用 checkPQ，这里通过事件排队让调度器处理
                    //                    });
                    //                    auto oldSize = mpiApp->GetFunctionSize();
                    dml1->addComputeBehindFunctions(batchId, appId, callbacks, no_disturbance());
                    //                    auto newSize = mpiApp->GetFunctionSize();
                    //                    std::cout << "on app1 funSize " << oldSize << "-->" <<
                    //                    newSize << std::endl; addComputeBehindFunctions(batchId,
                    //                    appId, callbacks, no_disturbance());
                }
                else
                // 暂时不会触发
                {
                    dml2->addComputeBehindFunctions(batchId, appId, callbacks, no_disturbance());
                }

                // todo::当前通信结束后 加入下一轮新的coFlowInfo(插入最早可开始时间 &&
                // 分竞争和不竞争两种) && checkPq()触发下一个通信事件
                //                auto nextBatchId = batchId + 1;
                //                if (nextBatchId > batchFlags[appId / 2][3] && nextBatchId <=
                //                batchCount[0])
                //                {
                //                    std::cout << "1111" << std::endl;
                //                    batchFlags[appId / 2][3] = nextBatchId;
                //                    // 得到下一轮通信(1)最早开始时间；(2)通信量
                //                    auto eTime = GetEarliestStartTime(nextBatchId);
                //                    auto CoInfoSize = GetCommunicateSize(batchId + 1, 0.00001);
                //                    coInfo newCoInfo{appId, batchId, CoInfoSize, eTime};
                //                    ns3::Jobpq.push(newCoInfo);
                //                    // 触发下一个通信事件
                //                    checkPQ(callbacks);
                //                }
            }
        });
    }

    void appAddrInit()
    {
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(1);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            std::cout << "test!!" << mpiApp << "|| " << &mpiApp << "|| " << PeekPointer(mpiApp)
                      << std::endl;
            AppAddrSet[0].insert(PeekPointer(mpiApp));
        }
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(3);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            std::cout << "test!!" << mpiApp << "|| " << &mpiApp << "|| " << PeekPointer(mpiApp)
                      << std::endl;
            AppAddrSet[1].insert(PeekPointer(mpiApp));
        }
    }

    // 给某个应用的每个结点上插入Initialize
    void addInitializeFunctions(AppId appId, Callbacks callbacks, std::size_t mtu = 1500)
    {
        // 0号应用是netjit, 1号是mpiApp
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(appId);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            //            std::cout << "test!!" << mpiApp << "|| " << &mpiApp << "|| " <<
            //            PeekPointer(mpiApp)
            //                      << std::endl;
            //            AppAddrSet[appId / 2].insert(PeekPointer(mpiApp));

            mpiApp->AddFunction([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                if (callbacks.application_start_callback)
                {
                    callbacks.application_start_callback(rank);
                }
                std::cout << "App" << appId << " begin Initialize function..." << std::endl;
                co_await application.Initialize(mtu);
            });
        }
    }

    // 给某个应用的每个结点上插入Finalize
    void addFinalizeFunctions(AppId appId, Callbacks callbacks)
    {
        // 0号应用是netjit, 1号是mpiApp
        for (auto& [rank, node] : nodes)
        {
            Ptr<Application> retrievedApp = node->GetApplication(appId);
            Ptr<MPIApplication> mpiApp = DynamicCast<MPIApplication>(retrievedApp);
            mpiApp->AddFunction([=](MPIApplication& application) -> ns3::CoroutineOperation<void> {
                std::cout << "App" << appId << " begin Finalize function..." << std::endl;
                application.Finalize();
                if (callbacks.application_end_callback)
                {
                    callbacks.application_end_callback(rank);
                }
                mpiApp->ChangeRunning(false);
                co_return;
            });
        }
    }

    void mpiFunctionsSchedule(DMLApplicationHelper& dmlApplicationHelper1,
                              DMLApplicationHelper& dmlApplicationHelper2,
                              MPIRankIDType master,
                              Callbacks callbacks,
                              RandomDisturbanceGenerator disturbance_generator = no_disturbance(),
                              std::size_t mtu = 1500,
                              double scale_factor = 1.0)
    {
        GlobalInitiate();

        std::cout << dml1->trace.tensor_size << std::endl;
        std::cout << dml2->trace.tensor_size << std::endl;

        batchCount[0] = dml2->batchCount[0] = dmlApplicationHelper1.trace.batches.size();
        batchCount[1] = dml2->batchCount[1] = dmlApplicationHelper2.trace.batches.size();
        std::cout << batchCount[0] << ' ' << batchCount[1] << std::endl;

        if (!dml1)
        {
            std::cout << "dml1 is NULL in mpiFunction Schedule!!!!!";
        }

        dml1->addInitializeFunctions(1, callbacks, mtu);
        dml2->addInitializeFunctions(3, callbacks, mtu);

        dml1->addComputeAheadFunctions(0, 1, callbacks);
        dml2->addComputeAheadFunctions(0, 3, callbacks);

        //        dml1->addComputeAheadFunctions(0, 1, callbacks);
        //        dml2->addComputeAheadFunctions(0, 3, callbacks);

        //                dmlApplicationHelper1.addCommunicateFunctions(0,1,callbacks,0.00001);
        //                dmlApplicationHelper2.addCommunicateFunctions(0,3,callbacks);

        //        dmlApplicationHelper1.addComputeBehindFunctions(0, 1, callbacks);
        //        dmlApplicationHelper2.addComputeBehindFunctions(0, 3, callbacks);

        //        std::cout << "put finalize funcs" << std::endl;
        //        dml1->addFinalizeFunctions(1, callbacks);
        //                dml2->addFinalizeFunctions(3, callbacks);
    }

    ~DMLApplicationHelper() = default;
};

} // namespace ns3

#endif // NS3_DML_APPLICATION_H
