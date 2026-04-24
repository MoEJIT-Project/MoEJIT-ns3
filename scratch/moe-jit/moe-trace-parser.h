#ifndef MOE_TRACE_PARSER_H
#define MOE_TRACE_PARSER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>
#include <filesystem>
#include "json.hpp" // Expecting stored locally

using json = nlohmann::json;

namespace ns3 {

/**
 * @brief Represents a single communication event from one rank's perspective
 */
struct MoeRawEvent {
    int rank_id;
    int batch_id;
    std::string op;             // DISPATCH / COMBINE
    std::string phase;
    std::string moe_block_id;
    double start_time_s;
    double end_time_s;
    std::map<int, size_t> send_bytes; // target -> bytes
    std::map<int, size_t> recv_bytes; // source -> bytes (expected)
};

/**
 * @brief Represents a global logical event (aggregated across all Ranks)
 * Conceptually equivalent to one "Step" in the simulation
 */
struct MoeGlobalEvent {
    uint32_t global_id; // Unique sequential ID
    int batch_id;
    std::string op;
    double start_time_s; // Use Rank 0's time as reference
    double end_time_s;   // Use Rank 0's time as reference
    double computation_delay; // Gap from previous end_time (derived)

    // N x N Matrix
    // matrix[src][dst] = bytes
    std::vector<std::vector<size_t>> traffic_matrix;

    // Helper to check consistency: Does Send[i][j] == Recv[j, i]?
    // In the raw trace, Rank i says "I send X to j" and Rank j says "I expect X from i"
    bool is_consistent;
    std::string consistency_error;
};

class MoeTraceParser {
public:
    /**
     * @brief Load and parse 3 trace files (Rank 0, 1, 2)
     * Assumes specific directory structure or passed vector of filenames
     */
    static std::vector<MoeGlobalEvent> LoadTraces(const std::string& directoryPath) {
        std::vector<std::string> filenames;
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.path().extension() == ".jsonl") {
                filenames.push_back(entry.path().string());
            }
        }

        // Sort filenames to ensure Rank 0 is at index 0, Rank 1 at index 1, etc.
        std::sort(filenames.begin(), filenames.end());

        int num_nodes = filenames.size();
        if (num_nodes == 0) {
            std::cerr << "Error: No .jsonl files found in " << directoryPath << std::endl;
            return {};
        }
        std::vector<std::vector<MoeRawEvent>> all_rank_events(num_nodes);

        // 1. Read Raw Events
        for (int r = 0; r < num_nodes; ++r) {
            std::ifstream infile(filenames[r]);
            if (!infile.is_open()) {
                std::cerr << "Error: Cannot open " << filenames[r] << std::endl;
                return {};
            }
            std::string line;
            while (std::getline(infile, line)) {
                if (line.empty()) continue;
                try {
                    auto j = json::parse(line);
                    int batch_id = j.value("batch_id", -1);
                    if (j.contains("ep_events")) {
                        for (const auto& ev_json : j["ep_events"]) {
                            MoeRawEvent ev;
                            ev.rank_id = r;
                            ev.batch_id = batch_id;
                            ev.op = ev_json.value("op", "");
                            // Ignore GATE_END events as they only contain single timestamp and no communication
                            if (ev.op == "GATE_END") continue;
                            ev.start_time_s = ev_json.value("start_time_s", 0.0);
                            ev.end_time_s = ev_json.value("end_time_s", 0.0);
                            
                            // Send Flows
                            if (ev_json.contains("send_bytes_per_rank")) {
                                for (auto& el : ev_json["send_bytes_per_rank"].items()) {
                                    ev.send_bytes[std::stoi(el.key())] = el.value().get<size_t>();
                                }
                            }
                            // Recv Flows
                            if (ev_json.contains("recv_bytes_per_rank")) {
                                for (auto& el : ev_json["recv_bytes_per_rank"].items()) {
                                    ev.recv_bytes[std::stoi(el.key())] = el.value().get<size_t>();
                                }
                            }
                            all_rank_events[r].push_back(ev);
                        }
                    }
                } catch (...) {
                    continue;
                }
            }
        }

        // 2. Align and Merge into Global Events
        // Assumption: All ranks have same number of events in same order
        std::vector<MoeGlobalEvent> global_events;
        size_t event_count = all_rank_events[0].size();
        
        for (size_t i = 0; i < event_count; ++i) {
            MoeGlobalEvent gev;
            gev.global_id = i;
            gev.batch_id = all_rank_events[0][i].batch_id;
            gev.op = all_rank_events[0][i].op;
            gev.start_time_s = all_rank_events[0][i].start_time_s;
            gev.end_time_s = all_rank_events[0][i].end_time_s; // Use Rank 0 end time
            gev.is_consistent = true;

            // Init Matrix N x N
            gev.traffic_matrix.assign(num_nodes, std::vector<size_t>(num_nodes, 0));

            // Populate Matrix from Sender's perspective (Truth)
            for (int r = 0; r < num_nodes; ++r) {
                if (i >= all_rank_events[r].size()) {
                    gev.is_consistent = false;
                    gev.consistency_error = "Rank " + std::to_string(r) + " missing event " + std::to_string(i);
                    continue;
                }
                const auto& rev = all_rank_events[r][i];
                for (auto const& [target, bytes] : rev.send_bytes) {
                    if (target < num_nodes) {
                        gev.traffic_matrix[r][target] = bytes;
                    }
                }
            }

            // Verify against Receiver's perspective
            for (int r = 0; r < num_nodes; ++r) {
                const auto& rev = all_rank_events[r][i];
                for (auto const& [src, expected_bytes] : rev.recv_bytes) {
                    if (src < num_nodes) {
                        size_t sender_says = gev.traffic_matrix[src][r];
                        if (sender_says != expected_bytes) {
                            gev.is_consistent = false;
                            gev.consistency_error += "Mismatch at Event " + std::to_string(i) + 
                                " Src:" + std::to_string(src) + "->Dst:" + std::to_string(r) + 
                                " Send:" + std::to_string(sender_says) + " Recv:" + std::to_string(expected_bytes) + "; ";
                        }
                    }
                }
            }
            global_events.push_back(gev);
        }

        return global_events;
    }

    // Configuration for Scaling
    struct TraceConfig {
        int target_n = 16;
        double zipf_alpha = 1.2;
        double start_time_offset = -1.0; // If < 0, use first event time as offset
        double volume_scale = 1.0;       // Factor to multiply payload sizes
        double computation_scale = 1.0;  // Factor to multiply computation delay
        int max_events = -1;             // If > 0, limit number of events
        int target_batch = -1;           // If >= 0, only load events with this batch_id
    };

    static std::vector<MoeGlobalEvent> GenerateScaledEvents(const std::vector<MoeGlobalEvent>& templates, TraceConfig config) {
        // 1. Filter templates by batch first
        std::vector<MoeGlobalEvent> filtered;
        for (const auto& t : templates) {
            if (config.target_batch >= 0 && t.batch_id != config.target_batch) continue;
            filtered.push_back(t);
        }

        if (filtered.empty()) {
            std::cerr << "Warning: No events found for batch " << config.target_batch << std::endl;
            return {};
        }

        std::vector<MoeGlobalEvent> scaled_events;
        std::mt19937 gen(42); 

        // Precompute Zipf Probabilities
        std::vector<double> zipf_probs(config.target_n);
        double zipf_norm = 0.0;
        for (int i = 0; i < config.target_n; ++i) {
            zipf_probs[i] = 1.0 / std::pow(i + 1, config.zipf_alpha);
            zipf_norm += zipf_probs[i];
        }
        for (double& p : zipf_probs) p /= zipf_norm;

        int count = 0;
        double current_sim_time = (config.start_time_offset >= 0) ? config.start_time_offset : 1.0;

        // 2. Loop until max_events reached
        while (config.max_events > 0 && count < config.max_events) {
            for (const auto& tmpl : filtered) {
                if (count >= config.max_events) break;

                MoeGlobalEvent new_ev;
                new_ev.global_id = count; 
                new_ev.batch_id = tmpl.batch_id;
                new_ev.op = tmpl.op;
                
                // Calculate and scale computation delay
                double raw_gap = (count % filtered.size() == 0) ? 0.05 : (tmpl.start_time_s - (scaled_events.empty() ? tmpl.start_time_s : filtered[(count-1)%filtered.size()].end_time_s));
                if (raw_gap < 0) raw_gap = 0.01; 
                
                new_ev.computation_delay = raw_gap * config.computation_scale;
                new_ev.start_time_s = current_sim_time;
                
                double duration = (tmpl.end_time_s - tmpl.start_time_s);
                current_sim_time += new_ev.computation_delay + duration;

                new_ev.is_consistent = true;
                new_ev.traffic_matrix.assign(config.target_n, std::vector<size_t>(config.target_n, 0));

                size_t total_template_bytes = 0;
                int active_senders = 0;
                for(const auto& row : tmpl.traffic_matrix) {
                    size_t s = 0;
                    for(auto b : row) s += b;
                    if (s > 0) {
                        total_template_bytes += s;
                        active_senders++;
                    }
                }
                
                for (int src = 0; src < config.target_n; ++src) {
                    for (int rank_idx = 0; rank_idx < config.target_n; ++rank_idx) {
                        int target = rank_idx; 
                        if (src == target) continue; 
                        
                        double prob = zipf_probs[rank_idx];
                        double scaled_size_f = (double)total_template_bytes * config.volume_scale * prob / (active_senders > 0 ? active_senders : 1);
                        size_t flow_size = static_cast<size_t>(scaled_size_f);
                        if (flow_size == 0 && scaled_size_f > 0.0) flow_size = 1;
                        
                        new_ev.traffic_matrix[src][target] = flow_size;
                    }
                }
                scaled_events.push_back(new_ev);
                count++;
            }
            current_sim_time += 0.1; 
        }
        return scaled_events;
    }
};

} // namespace ns3

#endif

