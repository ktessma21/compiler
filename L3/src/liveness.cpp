

#include <unordered_map>
#include <vector>
#include <set>
#include "ast_leaves.h"
#include "l3.h"

namespace L3 {


    void printLiveness(const std::vector<LivenessInfo>& result, 
                   const Function& f, 
                   bool debug = false) {
            if (!debug) return;

            for (int i = 0; i < (int)result.size(); i++) {
                std::cerr << "[" << i << "] " << f.instructions[i]->to_string() << "\n";

                std::cerr << "     in : { ";
                for (const auto& v : result[i].in)  std::cerr << v.to_string() << " ";
                std::cerr << "}\n";

                std::cerr << "     out: { ";
                for (const auto& v : result[i].out) std::cerr << v.to_string() << " ";
                std::cerr << "}\n\n";
            }
        }

   std::vector<LivenessInfo> compute_liveness(const Context& ctx) {
        std::vector<LivenessInfo> result(ctx.trees.size());

        // seed the last instruction from existing report — already correct
        int last = (int)ctx.trees.size() - 1;
        result[last].in  = ctx.liveAnalysisReport[last].in;
        result[last].out = ctx.liveAnalysisReport[last].out;
        {
            std::set<Variable> liveIn = result[last].out;
            if (ctx.trees[last]) {
                for (const auto& w : tree_writes(*ctx.trees[last])) liveIn.erase(w);
                for (const auto& r : tree_reads(*ctx.trees[last]))  liveIn.insert(r);
            } else {
                for (const auto& w : ctx.instructions[last]->writes()) liveIn.erase(w);
                for (const auto& r : ctx.instructions[last]->reads())  liveIn.insert(r);
            }
            result[last].in = std::move(liveIn);
        }

        bool keep_going = true;
        while (keep_going) {
            keep_going = false;

            for (int i = last - 1; i >= 0; i--) {  // skip last, already seeded
                std::set<Variable> liveOut;
                liveOut = result[i+1].in;

                std::set<Variable> liveIn = liveOut;
                if (ctx.trees[i]) {
                    for (const auto& w : tree_writes(*ctx.trees[i])) liveIn.erase(w);
                    for (const auto& r : tree_reads(*ctx.trees[i]))  liveIn.insert(r);
                } else {
                    for (const auto& w : ctx.instructions[i]->writes()) liveIn.erase(w);
                    for (const auto& r : ctx.instructions[i]->reads())  liveIn.insert(r);
                }

                if (liveIn != result[i].in || liveOut != result[i].out) {
                    result[i].in  = std::move(liveIn);
                    result[i].out = std::move(liveOut);
                    keep_going = true;
                }
            }
        }
        return result;
    }
   
    // Run this code before building the context trees. 
    // Compute liveness for a single basic block (Function-block).
    std::vector<LivenessInfo> compute_liveness(const Function& f){

        if (f.instructions.empty()){
            throw std::runtime_error("can't use compute_liveness func once context trees are built.");
        }

        std::vector<std::vector<size_t>> successors(f.instructions.size()); 
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
                    successors[i] = {labelIndex.at(c->getTarget().value().name) };
                    break;
                }

            }
        }


        std::vector<LivenessInfo> result(f.instructions.size()); // initialized 

        bool keep_going = true;
        while (keep_going) {
            keep_going = false;

            for (int i = (int)f.instructions.size() - 1; i >= 0; i--) {
                std::set<Variable> liveOut;
                for (size_t s : successors[i]) {
                    liveOut.insert(result[s].in.begin(), result[s].in.end());
                }

                std::set<Variable> liveIn = liveOut;
                for (const auto& w : f.instructions[i]->writes()) liveIn.erase(w);
                for (const auto& r : f.instructions[i]->reads())  liveIn.insert(r);

                if (liveIn != result[i].in || liveOut != result[i].out) {
                    result[i].in  = std::move(liveIn);
                    result[i].out = std::move(liveOut);
                    keep_going = true;
                }
            }
        }
    //    printLiveness(result, f, true); // print for debugging

        return result;
        
    }





       

    // // Use-count analysis (simpler — for tree merging).
    // std::unordered_map<std::string, int> compute_use_counts(const Context& ctx);

}