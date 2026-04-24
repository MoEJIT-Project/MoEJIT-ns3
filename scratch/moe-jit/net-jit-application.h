//
// Created by Ricardo Evans on 2024/4/25.
//

#ifndef NS3_NET_JIT_APPLICATION_H
#define NS3_NET_JIT_APPLICATION_H

#include <chrono>
#include <functional>
#include <queue>
#include <unordered_set>

#include <ns3/applications-module.h>
#include <ns3/core-module.h>
#include <ns3/mpi-application-module.h>

namespace ns3 {
    using BatchID = int64_t;
    using SizeType = int64_t;

    template<typename R, typename P>
    struct NetJITPrediction {
        using TimeType = std::chrono::duration<R, P>;
        BatchID batch;
        BatchID advanced_batch;
        SizeType size;
        TimeType waited_time;
        TimeType reporting_time;
        TimeType waiting_time;
        TimeType expected_time;
    };

    template<typename R, typename P>
    class NetJITApplication : public Application {
    private:
        using TimeType = std::chrono::duration<R, P>;
        using PredictionCallback = std::function<void(MPIRankIDType, BatchID, TimeType, SizeType)>;
        bool running;
        MPIRankIDType rank;
        std::queue<NetJITPrediction<R, P>> predictions;
        std::unordered_map<BatchID, std::pair<EventId, NetJITPrediction<R, P>>> pending_predictions{};
        std::shared_ptr<int64_t> current_batch;
        std::shared_ptr<TimeType> waited_time;
        std::shared_ptr<TimeType> time_offset;
        PredictionCallback callback;
    public:
        NetJITApplication(MPIRankIDType rank, const std::queue<NetJITPrediction<R, P>> &predictions, const std::shared_ptr<int64_t> &current_batch, const std::shared_ptr<TimeType> &waited_time, const std::shared_ptr<TimeType> &time_offset, const PredictionCallback &callback)
                : rank(rank), predictions(predictions), current_batch(current_batch), waited_time(waited_time), time_offset(time_offset), callback(callback) {}

        NetJITApplication(MPIRankIDType rank, std::queue<NetJITPrediction<R, P>> &&predictions, std::shared_ptr<int64_t> &&current_batch, std::shared_ptr<TimeType> &&waited_time, std::shared_ptr<TimeType> &&time_offset, PredictionCallback &&callback)
                : rank(rank), predictions(std::move(predictions)), current_batch(std::move(current_batch)), waited_time(std::move(waited_time)), time_offset(std::move(time_offset)), callback(std::move(callback)) {}

        void StartApplication() override {
            running = true;
        }

        void StopApplication() override {
            running = false;
        }

        void on_batch() {
            for (auto iterator = pending_predictions.begin(); iterator != pending_predictions.end(); ++iterator) {
                auto &[event_id, prediction] = iterator->second;
                event_id.Cancel();
                auto delay = calculate_delay(prediction);
                if (delay.IsNegative()) {
                    continue;
                }
                auto event = Simulator::Schedule(delay, &NetJITApplication::predict, this, std::move(prediction));
                iterator->second.first = event;
            }
            while (running && !predictions.empty() && predictions.front().batch <= *current_batch) {
                NetJITPrediction<R, P> prediction = std::move(predictions.front());
                predictions.pop();
                auto delay = calculate_delay(prediction);
                if (delay.IsNegative()) {
                    continue;
                }
                auto event = Simulator::Schedule(delay, &NetJITApplication::predict, this, prediction);
                pending_predictions[prediction.batch + prediction.advanced_batch] = {event, prediction};
            }
        }

        Time calculate_delay(const NetJITPrediction<R, P> &prediction) const {
            TimeType correction = prediction.waited_time;
            if (*current_batch > 1) {
                correction = (*waited_time) * (prediction.batch - 1) / (*current_batch - 1);
            }
            return convert(prediction.reporting_time - prediction.waited_time + correction + *time_offset) - Simulator::Now();
        }

        void predict(NetJITPrediction<R, P> prediction) {
            pending_predictions.erase(prediction.batch + prediction.advanced_batch);
            if (*current_batch <= 1) {
                return;
            }
            TimeType correction = (prediction.advanced_batch + prediction.batch - 1) * (*waited_time) / (*current_batch - 1);
            auto corrected = prediction.expected_time - prediction.waited_time - prediction.waiting_time + correction + *time_offset;
            callback(rank, prediction.batch + prediction.advanced_batch, corrected, prediction.size);
        }

        ~NetJITApplication() override = default;
    };

}

#endif //NS3_NET_JIT_APPLICATION_H
