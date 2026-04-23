//
// Created by Ricardo Evans on 2024/4/27.
//

#ifndef NS3_NET_JIT_UTILITY_H
#define NS3_NET_JIT_UTILITY_H

#include <queue>

#include <ns3/mpi-application-module.h>
#include <ns3/internet-module.h> // Required for Ipv4
#include "dml-application.h"

namespace ns3 {
    std::vector<std::vector<std::string>> read_DML_trace(std::filesystem::path trace_path);

    std::vector<std::vector<std::vector<std::string>>> read_DML_traces(std::filesystem::path trace_dir);

    std::queue<MPIFunction> parse_DML_traces(std::filesystem::path trace_dir);

    std::vector<DMLBatchTrace<int64_t, std::nano>> parse_DML_batch_traces(std::filesystem::path trace_dir);

    template<typename R, typename P>
    DMLWithNetJITTrace<R, P> parse_net_jit_traces(std::filesystem::path trace_path, std::vector<DMLBatchTrace<R, P>> batches, BatchID start_batch, BatchID end_batch) {
        using TimeType = std::chrono::duration<R, P>;
        using TraceTimeTypeRepresentation = double;
        using TraceTimeType = std::chrono::duration<TraceTimeTypeRepresentation>;
        using ConfidenceType = double;
        std::unordered_map<BatchID, TimeType> waited_time{{(BatchID) 0, TimeType{0}}};
        std::unordered_map<BatchID, TimeType> compute_time{{(BatchID) 0, TimeType{0}}};
        TimeType total_waited_time{0};
        TimeType total_compute_time{0};
        TimeType offset = TimeType{std::numeric_limits<R>::max()};
        std::vector<DMLBatchTrace<R, P>> truncated_batches{};
        for (auto batch: batches) {
            total_waited_time += batch.communication_end_time - batch.communication_start_time;
            total_compute_time += batch.communication_start_time - batch.batch_start_time + batch.batch_end_time - batch.communication_end_time;
            waited_time[batch.batch] = total_waited_time;
            compute_time[batch.batch] = total_compute_time;
            if (batch.batch < start_batch || batch.batch >= end_batch) {
                continue;
            }
            offset = std::min(offset, batch.batch_start_time);
            batch.batch -= (start_batch - 1);
            truncated_batches.push_back(std::move(batch));
        }
        std::fstream trace_file{trace_path, std::ios::in};
        if (!trace_file) {
            throw std::ios_base::failure("cannot open NetJIT trace file");
        }
        std::vector<NetJITPrediction<R, P>> predictions{};
        BatchID batch;
        TraceTimeTypeRepresentation reporting_time;
        TraceTimeTypeRepresentation duration;
        TraceTimeTypeRepresentation expected_time;
        ConfidenceType confidence;
        SizeType size;
        BatchID advanced_batch;
        auto compute_time_offset = compute_time[start_batch - 1];
        auto waited_time_offset = waited_time[start_batch - 1];
        while (trace_file >> batch >> reporting_time >> duration >> expected_time >> confidence >> size >> advanced_batch) {
            BatchID actual_batch = std::min(batch - advanced_batch, (BatchID) batches.size());
            if (actual_batch < start_batch || actual_batch >= end_batch) {
                continue;
            }
            BatchID predicting_batch = std::min(batch, (BatchID) batches.size());
            predictions.emplace_back(
                    actual_batch - start_batch + 1, advanced_batch, size,
                    waited_time[actual_batch - 1] - waited_time_offset,
                    std::chrono::duration_cast<TimeType>(TraceTimeType{reporting_time}),
                    waited_time[predicting_batch - 1] - waited_time[actual_batch - 1],
                    std::chrono::duration_cast<TimeType>(TraceTimeType{expected_time})
            );
        }
        NS_ASSERT_MSG(!predictions.empty(), "the NetJIT trace is empty");
        return ns3::DMLWithNetJITTrace<R, P>{size, offset, std::move(truncated_batches), std::move(predictions)};
    }
    // === Lightweight Packet Delay Monitor Setup ===
    
    // Tag for tracking packet creation time
    class MoeTimestampTag : public Tag {
    public:
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

        void SetTimestamp(Time time) { m_timestamp = time; }
        Time GetTimestamp(void) const { return m_timestamp; }

        void SetFlowId(uint32_t id) { m_flowId = id; }
        uint32_t GetFlowId(void) const { return m_flowId; }

        void SetFlowStartTime(int64_t timeNs) { m_flowStartTimeNs = timeNs; }
        int64_t GetFlowStartTime(void) const { return m_flowStartTimeNs; }

    private:
        Time m_timestamp;         // Packet Creation Time (for Pkt Delay)
        uint32_t m_flowId;        // Helper: TaskId
        int64_t m_flowStartTimeNs; // Helper: Flow Start Time (ns)
    };

    // Global counters for delay measurement
    extern double g_totalDelaySum;
    extern uint64_t g_totalRxPackets;

    // Helper functions to hook into the simulation
    void SetupLightweightPacketMonitor();
    double GetAveragePacketDelayMs();

}

#endif //NS3_NET_JIT_UTILITY_H
