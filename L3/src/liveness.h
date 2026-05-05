#pragma once

#include <unordered_map>
#include <set>
#include "ast_leaves.h"
#include "l3.h"

namespace L3 {

    // Per-instruction liveness info.
    struct LivenessInfo {
        std::set<Variable> in;       // variables live coming INTO the instruction
        std::set<Variable> out;      // variables live coming OUT of the instruction
        std::set<Variable> reads;    // GEN
        std::set<Variable> writes;   // KILL
    };

    // Compute liveness for a single basic block (Context).
    std::vector<LivenessInfo> compute_liveness(const Context& ctx);

    // Compute liveness for a single basic block (Function-block).
    std::vector<LivenessInfo> compute_liveness(const Function& func);

    // // Use-count analysis (simpler — for tree merging).
    // std::unordered_map<std::string, int> compute_use_counts(const Context& ctx);

}