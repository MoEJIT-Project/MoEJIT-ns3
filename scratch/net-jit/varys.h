//
// Created by Dichox on 2025/8/4.
//

#ifndef NS3_VARYS_H
#define NS3_VARYS_H

#include "dml-application.h"
#include "main.h"
#include "net-jit-application.h"
#include "topology/fattree-topology.h"
#include "topology/utility.h"

#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor.h"
#include <ns3/applications-module.h>
#include <ns3/core-module.h>
#include <ns3/internet-module.h>
#include <ns3/ipv4-l3-protocol.h>
#include <ns3/mpi-application-module.h>
#include <ns3/mpi-application.h>
#include <ns3/network-module.h>
#include <ns3/point-to-point-module.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using TimeType = std::chrono::duration<int64_t, std::nano>;


struct _coflowInfo{
    MPIRankIDType rank;
    BatchID batch_id;
    unsigned long data_size;
    TimeType current_time;


    // 构造函数
    _coflowInfo(MPIRankIDType r, BatchID b, unsigned long d, TimeType t)
        : rank(r), batch_id(b), data_size(d), current_time(t) {}
};

using coflowInfo = _coflowInfo*;

#endif // NS3_VARYS_H
