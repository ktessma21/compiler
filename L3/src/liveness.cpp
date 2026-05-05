#pragma once

#include <unordered_map>
#include <vector>
#include <set>
#include "ast_leaves.h"
#include "l3.h"

namespace L3 {

   
    // Run this code before building the context trees. 
    // Compute liveness for a single basic block (Function-block).
    std::vector<LivenessInfo> compute_liveness(const Function& func){

        std::vector<std::vector<size_t>> successors;
        std::unordered_map<std::string, size_t> labelIndex;
        for (size_t j = 0; j < f.instructions.size(); j++) {
            if (auto* lbl = dynamic_cast<LabelInstruction*>(f.instructions[j].get())) {
                
                auto optLabel = lbl->getLabel();
                if (optLabel.has_value()) {
                    labelIndex[optLabel->name] = j;
                } 
            }
        }



        
    }

    // // Use-count analysis (simpler — for tree merging).
    // std::unordered_map<std::string, int> compute_use_counts(const Context& ctx);

}