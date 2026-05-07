

#include <unordered_map>
#include <vector>
#include <set>
#include "ast_leaves.h"
#include "liveness.h"
#include "l3.h"

namespace L3 {

   
    // Run this code before building the context trees. 
    // Compute liveness for a single basic block (Function-block).
    std::vector<LivenessInfo> compute_liveness(const Function& f){

        if (f.instructions.empty()){
            throw std::runtime_error("can't use compute_liveness func once context trees are built.");
        }

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


        for (size_t i = 0; i < f.instructions.size(); i++){
            
            switch (f.instructions[i] -> type) {
                // Single successor: fall through to i+1
                case InstructionType::AssignFromCall:
                case InstructionType::AssignFromCmp:
                case InstructionType::AssignFromLoad:
                case InstructionType::AssignFromOp:
                case InstructionType::AssignFromS:
                case InstructionType::Label:
                case InstructionType::Store:
                case InstructionType::Call:
                    successors[i] = {i + 1};
					break;

                case InstructionType::Return:
                case InstructionType::ReturnT:
                    break;

                // One successor but NOT i+1 — jumps unconditionally
                case InstructionType::Br:{
                    auto* g = dynamic_cast<BrInstruction*>(f.instructions[i].get());
                    successors[i] = { labelIndex.at(g->getTarget().value().name) };
                    break;
                } 

                case InstructionType::BrT:{
                    auto* c = dynamic_cast<BrTInstruction*>(f.instructions[i].get());
                    successors[i] = { i+ 1, labelIndex.at(c->getTarget().value().name) };
                    break;
                }



                
            }
        }


        std::vector<LivenessInfo> result;

        return result;



        
    }

    // // Use-count analysis (simpler — for tree merging).
    // std::unordered_map<std::string, int> compute_use_counts(const Context& ctx);

}