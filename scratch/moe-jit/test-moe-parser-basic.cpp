
#include "moe-trace-parser.h"
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char** argv) {
    std::string traceDir = "/home/dichox/ns-3/scratch/net-jit/moe-traces/GPT-ep-trace-8-nodes";

    std::cout << "Loading traces from " << traceDir << "..." << std::endl;
    auto events = MoeTraceParser::LoadTraces(traceDir);

    std::cout << "Loaded " << events.size() << " global synchronized events." << std::endl;

    if (events.empty()) {
        std::cerr << "FAILED: No events loaded." << std::endl;
        return 1;
    }

    // Verify logic
    int consistency_failures = 0;
    size_t total_bytes = 0;

    for (const auto& ev : events) {
        if (!ev.is_consistent) {
            consistency_failures++;
            // Print first few errors
            if (consistency_failures < 5) {
                std::cout << "  Consistency Error: " << ev.consistency_error << std::endl;
            }
        }
        
        for (const auto& row : ev.traffic_matrix) {
            for (auto bytes : row) {
                total_bytes += bytes;
            }
        }
    }

    std::cout << "Consistency Check: " 
              << (consistency_failures == 0 ? "PASS" : "FAIL") 
              << " (" << consistency_failures << " failures)" << std::endl;
    std::cout << "Total Traffic Volume: " << total_bytes / (1024.0 * 1024.0) << " MB" << std::endl;

    if (consistency_failures == 0 && events.size() > 0) {
        return 0;
    } else {
        return 1;
    }
}
