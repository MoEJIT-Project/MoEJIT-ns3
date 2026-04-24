//
// Created by Ricardo Evans on 2024/4/27.
//

#include <fstream>

#include "utility.h"

std::vector<std::vector<std::string>> ns3::read_DML_trace(std::filesystem::path trace_path) {
    // std::cout << trace_path << std::endl;
    std::fstream fp;
    fp.open(trace_path, std::ios::in);
    if (!fp.good()) {
        throw std::runtime_error("Could not open trace file");
    }

    std::string line;
    std::vector<std::vector<std::string>> all_lines;
    while (getline(fp, line)) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;

        while (iss >> token) {
            tokens.push_back(token);
        }

        all_lines.push_back(tokens);
    }

//    for (const auto& line_vector : all_lines) {
//        for (const std::string& word : line_vector) {
//            std::cout << word << " ";
//        }
//        std::cout << std::endl;
//    }
//    std::cout << "******" << std::endl;

    fp.close();
    return all_lines;
}

std::vector<std::vector<std::vector<std::string>>> ns3::read_DML_traces(std::filesystem::path trace_dir) {
    std::vector<std::filesystem::path> trace_file_names;
    for (auto &p: std::filesystem::directory_iterator(trace_dir)) {
        trace_file_names.push_back(p.path());
    }
    std::sort(trace_file_names.begin(), trace_file_names.end());

    std::vector<std::vector<std::vector<std::string>>> all_workers_trace;
    all_workers_trace.reserve(trace_file_names.size());
    for (const auto &path: trace_file_names) {
        all_workers_trace.push_back(ns3::read_DML_trace(path));
    }

    return all_workers_trace;
}

std::queue<ns3::MPIFunction> ns3::parse_DML_traces(std::filesystem::path trace_dir) {
    std::queue<MPIFunction> mpi_functions;
    std::vector<std::vector<std::vector<std::string>>> all_workers_trace = read_DML_traces(trace_dir);

    std::cout << "iteration\t comm_start_time(s)\t comm_end_time(s)\t comm_size(byte)" << std::endl;
    double last_comm_end_time = 0;
    for (size_t i = 0; i < all_workers_trace[0].size(); i++) {
        double min_comm_start_time = 0;
        double max_comm_end_time = 0;
        int64_t max_comm_size = 0;
        std::istringstream iss(all_workers_trace[0][i][2] + " " + all_workers_trace[0][i][3] + " " + all_workers_trace[0][i][4]);
        iss >> min_comm_start_time >> max_comm_end_time >> max_comm_size;

        for (size_t j = 1; j < all_workers_trace.size(); j++) {
            double cur_comm_start_time = 0;
            double cur_comm_end_time = 0;
            int64_t cur_comm_size = 0;
            std::istringstream cur_iss(all_workers_trace[j][i][2] + " " + all_workers_trace[j][i][3] + " " + all_workers_trace[j][i][4]);
            cur_iss >> cur_comm_start_time >> cur_comm_end_time >> cur_comm_size;
            if (cur_comm_start_time < min_comm_start_time) {
                min_comm_start_time = cur_comm_start_time;
            }
            if (cur_comm_end_time > max_comm_end_time) {
                max_comm_end_time = cur_comm_end_time;
            }
            if (cur_comm_size > max_comm_size) {
                max_comm_size = cur_comm_size;
            }
        }

        std::cout << std::fixed << std::setprecision(9);
        std::cout << i + 1 << " " << min_comm_start_time << " " << max_comm_end_time << " " << max_comm_size << std::endl;

        int64_t count = max_comm_size;
        dumpi_datatype datatype = DUMPI_BYTE;
        dumpi_comm comm = 2;    // MPI_COMM_WORLD

        if (i >= 1) {
            int64_t interval = (min_comm_start_time - last_comm_end_time) * 1e+9;
            std::cout << "iteration " << i << "-" << i + 1 << " computing interval: " << interval << std::endl;
            mpi_functions.emplace([interval](ns3::MPIApplication &application) -> ns3::CoroutineOperation<void> {
                co_await application.Compute(std::chrono::nanoseconds{interval});
            });
        }

        mpi_functions.emplace([comm, datatype, count](ns3::MPIApplication &application) -> ns3::CoroutineOperation<void> {
            auto &c = application.communicator(comm);
            co_await type_mapping(datatype, [&c, count]<typename T>() {
                return c.template AllReduce<std::vector<T>>(ns3::FakePacket, count);
            });
        });

        last_comm_end_time = max_comm_end_time;
    }

    std::cout << "size of mpi_functions: " << mpi_functions.size() << std::endl;
    return mpi_functions;
}

std::vector<ns3::DMLBatchTrace<int64_t, std::nano>> ns3::parse_DML_batch_traces(std::filesystem::path trace_dir) {
    std::vector<ns3::DMLBatchTrace<int64_t, std::nano>> DML_batch_traces;
    std::vector<std::vector<std::vector<std::string>>> all_workers_trace = read_DML_traces(trace_dir);

    double last_batch_start_time = 0;
    double last_comm_start_time = 0;
    double last_comm_end_time = 0;
    int64_t last_comm_size = 0;
    for (size_t i = 0; i < all_workers_trace[0].size(); i++) {
        double batch_start_time = 0;
        double min_comm_start_time = 0;
        double max_comm_end_time = 0;
        int64_t max_comm_size = 0;
        std::istringstream iss(all_workers_trace[0][i][1] + " " + all_workers_trace[0][i][2] + " " + all_workers_trace[0][i][3] + " " + all_workers_trace[0][i][4]);
        iss >> batch_start_time >> min_comm_start_time >> max_comm_end_time >> max_comm_size;

        for (size_t j = 1; j < all_workers_trace.size(); j++) {
            double cur_comm_start_time = 0;
            double cur_comm_end_time = 0;
            int64_t cur_comm_size = 0;
            std::istringstream cur_iss(all_workers_trace[j][i][2] + " " + all_workers_trace[j][i][3] + " " + all_workers_trace[j][i][4]);
            cur_iss >> cur_comm_start_time >> cur_comm_end_time >> cur_comm_size;
            if (cur_comm_start_time < min_comm_start_time) {
                min_comm_start_time = cur_comm_start_time;
            }
            if (cur_comm_end_time > max_comm_end_time) {
                max_comm_end_time = cur_comm_end_time;
            }
            if (cur_comm_size > max_comm_size) {
                max_comm_size = cur_comm_size;
            }
        }

//        std::cout << std::fixed << std::setprecision(9);
//        std::cout << i + 1 << " " << min_comm_start_time << " " << max_comm_end_time << " " << max_comm_size << std::endl;

        if(i >= 1){
            ns3::DMLBatchTrace<int64_t, std::nano> pre_batch_trace;
            pre_batch_trace.batch = i;
            pre_batch_trace.batch_start_time = std::chrono::nanoseconds{static_cast<int64_t>(last_batch_start_time * 1e+9)};
            pre_batch_trace.batch_end_time = std::chrono::nanoseconds{static_cast<int64_t>(batch_start_time * 1e+9)};
            pre_batch_trace.communication_start_time = std::chrono::nanoseconds{static_cast<int64_t>(last_comm_start_time * 1e+9)};
            pre_batch_trace.communication_end_time = std::chrono::nanoseconds{static_cast<int64_t>(last_comm_end_time * 1e+9)};
            pre_batch_trace.communication_size = last_comm_size;

            DML_batch_traces.push_back(pre_batch_trace);
        }

        if(i == all_workers_trace[0].size() - 1){
            ns3::DMLBatchTrace<int64_t, std::nano> last_batch_trace;
            last_batch_trace.batch = all_workers_trace[0].size();
            last_batch_trace.batch_start_time = std::chrono::nanoseconds{static_cast<int64_t>(batch_start_time * 1e+9)};
            last_batch_trace.batch_end_time = std::chrono::nanoseconds{static_cast<int64_t>((max_comm_end_time + 0.1) * 1e+9)};
            last_batch_trace.communication_start_time = std::chrono::nanoseconds{static_cast<int64_t>(min_comm_start_time * 1e+9)};
            last_batch_trace.communication_end_time = std::chrono::nanoseconds{static_cast<int64_t>(max_comm_end_time * 1e+9)};
            last_batch_trace.communication_size = max_comm_size;

            DML_batch_traces.push_back(last_batch_trace);
        }

        last_batch_start_time = batch_start_time;
        last_comm_start_time = min_comm_start_time;
        last_comm_end_time = max_comm_end_time;
        last_comm_size = max_comm_size;
    }

    std::cout << "size of batch traces: " << DML_batch_traces.size() << std::endl;
    std::cout << "iteration\t\t batch_start_time\t\t batch_end_time\t\t comm_start_time\t\t comm_end_time\t\t comm_size(byte)" << std::endl;
//    for (const auto& batch_trace : DML_batch_traces) {
//        std::cout << batch_trace.batch << "\t\t\t" << batch_trace.batch_start_time << "\t\t\t" << batch_trace.batch_end_time <<
//            "\t\t\t" << batch_trace.communication_start_time << "\t\t\t" << batch_trace.communication_end_time << "\t\t\t" <<
//            batch_trace.communication_size << std::endl;
//    }

    return DML_batch_traces;
}

// === Lightweight Packet Delay Monitor Implementation ===

ns3::TypeId ns3::MoeTimestampTag::GetTypeId(void) {
    static ns3::TypeId tid = ns3::TypeId("ns3::MoeTimestampTag")
        .SetParent<Tag>()
        .AddConstructor<MoeTimestampTag>();
    return tid;
}

ns3::TypeId ns3::MoeTimestampTag::GetInstanceTypeId(void) const {
    return GetTypeId();
}

uint32_t ns3::MoeTimestampTag::GetSerializedSize(void) const {
    return sizeof(Time) + sizeof(uint32_t) + sizeof(int64_t);
}

void ns3::MoeTimestampTag::Serialize(ns3::TagBuffer i) const {
    i.Write((const uint8_t *)&m_timestamp, sizeof(Time));
    i.Write((const uint8_t *)&m_flowId, sizeof(uint32_t));
    i.Write((const uint8_t *)&m_flowStartTimeNs, sizeof(int64_t));
}

void ns3::MoeTimestampTag::Deserialize(ns3::TagBuffer i) {
    i.Read((uint8_t *)&m_timestamp, sizeof(Time));
    i.Read((uint8_t *)&m_flowId, sizeof(uint32_t));
    i.Read((uint8_t *)&m_flowStartTimeNs, sizeof(int64_t));
}

void ns3::MoeTimestampTag::Print(std::ostream &os) const {
    os << "t=" << m_timestamp << " id=" << m_flowId << " start=" << m_flowStartTimeNs;
}

// Global counters
double ns3::g_totalDelaySum = 0.0;
uint64_t ns3::g_totalRxPackets = 0;

// Map to track in-flight packet start times (UID -> Time)
// Use unordered_map for O(1) access
static std::unordered_map<uint64_t, ns3::Time> g_packetTxTimes;
// Mutex not needed as NS-3 is single-threaded event loop

static void PacketTxCallback(ns3::Ptr<const ns3::Packet> packet, ns3::Ptr<ns3::Ipv4> ipv4, uint32_t interface) {
    // Store Tx Time
    g_packetTxTimes[packet->GetUid()] = ns3::Simulator::Now();
}

static void PacketRxCallback(ns3::Ptr<const ns3::Packet> packet, ns3::Ptr<ns3::Ipv4> ipv4, uint32_t interface) {
    auto it = g_packetTxTimes.find(packet->GetUid());
    if (it != g_packetTxTimes.end()) {
        ns3::Time now = ns3::Simulator::Now();
        ns3::Time delay = now - it->second;
        ns3::g_totalDelaySum += delay.GetSeconds();
        ns3::g_totalRxPackets++;
        
        // Remove from map to keep memory usage low (only store in-flight)
        // Note: For multicast, this might be tricky, but here we assume unicast for MoE AllToAll
        g_packetTxTimes.erase(it);
    }
}

void ns3::SetupLightweightPacketMonitor() {
    ns3::Config::ConnectWithoutContext("/NodeList/*/$ns3::Ipv4L3Protocol/Tx", ns3::MakeCallback(&PacketTxCallback));
    ns3::Config::ConnectWithoutContext("/NodeList/*/$ns3::Ipv4L3Protocol/Rx", ns3::MakeCallback(&PacketRxCallback));
}

double ns3::GetAveragePacketDelayMs() {
    if (g_totalRxPackets == 0) return 0.0;
    return (g_totalDelaySum / g_totalRxPackets) * 1000.0;
}
