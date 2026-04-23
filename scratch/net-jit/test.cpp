//
// Created by Ricardo Evans on 2024/4/27.
//

#include <queue>
#include <string>

#include <ns3/mpi-application-module.h>

#include "utility.h"

int main() {
    std::string trace_path = "../scratch/net-jit/traces/gpt2";
    std::string app = "gpt2/batch_info_to.4.txt";
    // ns3::MPIRankIDType rank_id = 0;
    std::vector<ns3::DMLBatchTrace<int64_t, std::nano>> v = ns3::parse_DML_batch_traces(trace_path);
    ns3::DMLWithNetJITTrace<int64_t, std::nano> trace = ns3::parse_net_jit_traces("../scratch/net-jit/traces/bert/bert-0.1.txt", std::move(v), std::numeric_limits<ns3::BatchID>::min(), std::numeric_limits<ns3::BatchID>::max());
    return 0;
}