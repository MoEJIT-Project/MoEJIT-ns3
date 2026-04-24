
#include "moe-trace-parser.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

int main(int argc, char** argv) {
    std::string traceDir = "/home/dichox/ns-3/scratch/net-jit/moe-traces/GPT-ep-trace-8-nodes";
    auto templates = MoeTraceParser::LoadTraces(traceDir);
    
    if (templates.empty()) {
        std::cerr << "Templates failed to load." << std::endl;
        return 1;
    }

    // 2. Scale to 16 Nodes
    int N = 16;
    double alpha = 1.2;
    std::cout << "Scaling to " << N << " nodes with Zipf alpha=" << alpha << "..." << std::endl;
    
    MoeTraceParser::TraceConfig config;
    config.target_n = N;
    config.zipf_alpha = alpha;
    config.start_time_offset = 0.0;
    config.volume_scale = 1.0; 
    config.max_events = -1;

    auto scaled_events = MoeTraceParser::GenerateScaledEvents(templates, config);
    
    std::cout << "Generated " << scaled_events.size() << " scaled events." << std::endl;

    // 3. Verify Traffic Distribution (Zipf Skew)
    // We expect some columns (Rank IDs) to receive much more traffic than others across the simulation.
    std::vector<size_t> total_recv_per_rank(N, 0);
    size_t grand_total = 0;

    for (const auto& ev : scaled_events) {
        // Semantic Check: Matrix Dimensions
        if (ev.traffic_matrix.size() != (size_t)N || ev.traffic_matrix[0].size() != (size_t)N) {
            std::cerr << "FAIL: Matrix dimension mismatch." << std::endl;
            return 1;
        }

        // Semantic Check: Consistency
        // With Generated events, Send[i][j] IS Recv[j][i] by definition of a shared matrix.
        
        // Accumulate Stats
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                total_recv_per_rank[j] += ev.traffic_matrix[i][j];
                grand_total += ev.traffic_matrix[i][j];
            }
        }
    }

    // Output Distribution
    std::cout << "\nReceiver Load Distribution (Top 5 Hot Experts?):" << std::endl;
    // Create pair vec to sort
    std::vector<std::pair<int, size_t>> sorted_ranks;
    for (int i = 0; i < N; ++i) sorted_ranks.push_back({i, total_recv_per_rank[i]});
    
    std::sort(sorted_ranks.begin(), sorted_ranks.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // Descending
    });

    std::cout << std::left << std::setw(10) << "Rank" << std::setw(20) << "Total Recv (MB)" << std::setw(10) << "% of Total" << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
    for (int i = 0; i < N; ++i) {
        double mb = sorted_ranks[i].second / (1024.0 * 1024.0);
        double pct = 100.0 * sorted_ranks[i].second / grand_total;
        std::cout << std::left << std::setw(10) << sorted_ranks[i].first 
                  << std::setw(20) << mb 
                  << std::setw(10) << pct << "%" << std::endl;
    }

    // Verify Skew
    // If Zipf works, Top 1 should have roughly P(1) = 1/sum(1/k^alpha) share
    // For N=16, alpha=1.2, P(1) is significant.
    // However, in my current implementation, I strictly mapped popularity rank 0 to node 0, rank 1 to node 1...
    // Wait, the code says:
    // int target = popularity_rank[rank_idx];
    // And popularity_rank was iota (0,1,2...). 
    // And I commented out shuffle.
    // So Rank 0 should be the hottest (Index 0).
    
    if (sorted_ranks[0].first == 0 && sorted_ranks[0].second > sorted_ranks[N-1].second * 2) {
        std::cout << "\nPASS: Traffic shows significant skew towards Rank 0 (Zipf behavior verified)." << std::endl;
        return 0;
    } else {
        std::cout << "\nWARNING: Traffic might not be skewed as expected. Check Alpha or Shuffle logic." << std::endl;
        return 1;
    }
}
