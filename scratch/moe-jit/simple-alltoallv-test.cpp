#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mpi-application-module.h"

// Include mpi-util.h for retrieveIPAddress
// Need to use path relative to scratch/net-jit or absolute path
#include "../../src/mpi-application/model/mpi-util.h"

#include <iostream>
#include <vector>
#include <map>
#include <string>

using namespace ns3;

// 定义一个最小化的测试场景
// 3个节点，两两互联 (实际上是用一个交换机或是直接全连接，这里为了简单直接用P2P连成三角形/总线)
// 为了最简单，我们使用一个 Csma 信道 或者 PointToPoint 星型拓扑 (Switch)

NS_LOG_COMPONENT_DEFINE ("SimpleAllToAllVTest");

int main (int argc, char *argv[])
{
  // 1. 命令行参数设置
  LogComponentEnable ("SimpleAllToAllVTest", LOG_LEVEL_INFO);
  // LogComponentEnable ("MPICommunicator", LOG_LEVEL_DEBUG); // 如果需要看底层详细通信
  
  uint32_t nNodes = 4; // 节点数量
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nNodes", "Number of nodes", nNodes);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Create " << nNodes << " nodes.");
  NodeContainer nodes;
  nodes.Create (nNodes);

  // 2. 网络拓扑构建: 使用简单的 PointToPoint 连接一个中心 Switch (Star Topology) 
  // 或者最简单的：全共享信道 (CSMA) 方便模拟 Switch
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1us"));

  // 为了简单，我们创建一个“中心节点”充当交换机，其他节点连接到它
  NodeContainer centerNode;
  centerNode.Create(1);
  
  InternetStackHelper stack;
  stack.Install (nodes);
  stack.Install (centerNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  // 存储应用需要的地址映射
  std::map<MPIRankIDType, Address> rankAddresses;
  std::map<Address, MPIRankIDType> addressRanks;

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      NetDeviceContainer devices = pointToPoint.Install (nodes.Get (i), centerNode.Get (0));
      Ipv4InterfaceContainer interfaces = address.Assign (devices);
      // 获取叶子节点的 IP 地址 (interfaces.GetAddress(0))
      Address addr = InetSocketAddress (interfaces.GetAddress (0), 8000);
      
      MPIRankIDType rank = i;
      rankAddresses[rank] = addr;
      addressRanks[retrieveIPAddress(addr)] = rank;
      
      address.NewNetwork ();
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // 3. 安装 MPI 应用
  NS_LOG_INFO ("Installing Applications...");

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      MPIRankIDType rank = i;
      std::queue<std::function<CoroutineOperation<void>(MPIApplication &)>> functions;

      // === 任务定义 ===
      
      // 任务 1: 初始化
      functions.push([rank](MPIApplication &app) -> CoroutineOperation<void> {
          NS_LOG_INFO ("Rank " << rank << ": Initializing...");
          co_await app.Initialize();
          NS_LOG_INFO ("Rank " << rank << ": Initialized.");
      });

      // 任务 2: 执行 AllToAllV 测试
      functions.push([rank, nNodes](MPIApplication &app) -> CoroutineOperation<void> {
          NS_LOG_INFO ("Rank " << rank << ": Starting AllToAllV...");
          
          auto &comm = app.communicator(WORLD_COMMUNICATOR);
          
          // 构建变长发送/接收 数据量
          // 规则: Rank i 发给 Rank j 的数据量 = (i + 1) * (j + 1) * 100 字节
          // 举例: Rank 1 -> Rank 2: 2 * 3 * 100 = 600 bytes
          // 验证: Rank 2 <- Rank 1: 也应该接收 600 bytes
          
          std::unordered_map<MPIRankIDType, std::size_t> sendSizes;
          std::unordered_map<MPIRankIDType, std::size_t> recvSizes;

          // 本地计算预期数据量
          for (uint32_t target = 0; target < nNodes; ++target) {
              sendSizes[target] = (rank + 1) * (target + 1) * 100;
              recvSizes[target] = (target + 1) * (rank + 1) * 100;
              
              if (rank == 0) {
                  // 只打印 Rank 0 的详细计划以减少日志
                   NS_LOG_INFO ("Rank " << rank << " plans to send " << sendSizes[target] << " bytes to Rank " << target);
              }
          }

          auto start = Simulator::Now();
          
          // 记录初始接收字节数
          auto initialRxBytes = comm.RxBytes();
          
          // 调用 AllToAllV
          co_await comm.AllToAllV(FakePacket, sendSizes, recvSizes);
          
          auto end = Simulator::Now();
          
          // 计算接收到的字节数
          auto finalRxBytes = comm.RxBytes();
          auto receivedBytes = finalRxBytes - initialRxBytes;

          NS_LOG_INFO ("Rank " << rank << ": AllToAllV Finished! Duration: " << (end - start).GetNanoSeconds() << " ns. Total Received: " << receivedBytes << " bytes");
          
          // 验证接收量是否符合预期 (所有其他节点发给我的总和)
          // 注意：MPICommunicator::RxBytes() 会特意过滤掉来自自己的 Loopback 流量
          // 所以在计算预期值时，我们也要排除掉 src == rank 的情况
          size_t expectedRxBytes = 0;
          for (uint32_t src = 0; src < nNodes; ++src) {
              if (src != rank) {
                  expectedRxBytes += (src + 1) * (rank + 1) * 100;
              }
          }
          
          if (receivedBytes == expectedRxBytes) {
              NS_LOG_INFO ("Rank " << rank << ": PASS - Received expected amount of data.");
          } else {
              NS_LOG_ERROR ("Rank " << rank << ": FAIL - Expected " << expectedRxBytes << " bytes, but received " << receivedBytes << " bytes.");
          }
      });

      // 任务 3: 结束
      functions.push([rank](MPIApplication &app) -> CoroutineOperation<void> {
          NS_LOG_INFO ("Rank " << rank << ": Finalizing...");
          app.Finalize();
          NS_LOG_INFO ("Rank " << rank << ": Finalized.");
          co_return;
      });

      // 创建并安装应用
      Ptr<MPIApplication> app = CreateObject<MPIApplication> (rank, rankAddresses, addressRanks, std::move(functions));
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0));
      app->SetStopTime (Seconds (10.0));
    }

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}

// 定义缺失的全局变量以满足链接要求 (因为 libmpi-application 可能间接依赖了这些，或者是因为我们链接了包含这些声明的库但没提供定义)
// 注意：如果 libmpi-application 本身不依赖这些，那么可能是其他地方引入的。
// 从报错看 `undefined reference to ns3::AppAddrSet`，说明有代码引用了它但没定义。
// 在本测试中我们不需要这些变量的实际功能，只需提供定义即可。

#include "dml-application.h" // 为了获取类型定义

namespace ns3
{
std::priority_queue<coInfo, std::vector<coInfo>, CompareByComSize> Jobpq;
std::vector<int> batchFlag;
std::vector<std::vector<int>> batchFlags;
std::vector<std::unordered_set<void*>> AppAddrSet(2);
} // namespace ns3
