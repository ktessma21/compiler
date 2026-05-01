#include <l2.h>

#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>
#include <interference.h>
#include <coloring.h>
#include <map>
#include <vector>
#include <tuple> 
#include <liveness.h>
#include <set>
#include <cassert>

namespace L2 { 

    enum class Tag { COLOR, SPILL };

    void GraphColoring(L2::Graph& g, Function& f){

        Graph gprime = g;
        L2::LiveSet spill;
        // tuple: (node, its neighbors at push time, tag)
        std::vector<std::tuple<VALUE, LiveSet, int>> stack;
        int color_number = 0;

        constexpr int K = 15;

        // ---------- Step 2: simplify ----------
        // Repeat: pick a node with degree < K and push as COLOR; otherwise pick
        // a spill candidate (highest-degree variable) and push as SPILL.
        while (true) {
            auto nd = g.node_with_less_than_15_neighbors();

            if (nd.has_value()) {
                assert(std::holds_alternative<Variable>(*nd));
                stack.emplace_back(*nd, g.graph[*nd], static_cast<int>(Tag::COLOR));
                g.removeNode(*nd);
                continue;
            }

            // No low-degree variable — pick a spill candidate.
            std::optional<VALUE> spill_candidate;
            size_t max_deg = 0;
            for (const auto& [v, neighbors] : g.graph) {
                if (!std::holds_alternative<Variable>(v)) continue;
                if (neighbors.size() >= max_deg) {
                    max_deg = neighbors.size();
                    spill_candidate = v;
                }
            }

            if (!spill_candidate.has_value()) break;  // only registers left

            stack.emplace_back(*spill_candidate,
                               g.graph[*spill_candidate],
                               static_cast<int>(Tag::SPILL));
            g.removeNode(*spill_candidate);
        }

        // ---------- Step 3: select / assign colors ----------
        std::map<VALUE, int, VALUEComparator> value_to_color;

        // Pre-color physical registers with their canonical indices.
        for (size_t i = 0; i < Graph::allRegs.size(); ++i) {
            value_to_color[VALUE(Graph::allRegs[i])] = static_cast<int>(i);
        }

        while (!stack.empty()) {
            auto [node, edges, tag] = stack.back();
            stack.pop_back();

            // Colors used by already-colored neighbors.
            std::set<int> used;
            for (const auto& nb : edges) {
                auto it = value_to_color.find(nb);
                if (it != value_to_color.end()) used.insert(it->second);
            }

            if (tag == static_cast<int>(Tag::COLOR)) {
                // Definitely colorable — pick the smallest free color.
                for (int c = 0; c < K; ++c) {
                    if (!used.count(c)) {
                        value_to_color[node] = c;
                        break;
                    }
                }
            } else {
                // Optimistic coloring: a SPILL-tagged node may still be colorable
                // if its neighbors didn't actually consume all K colors.
                if (used.size() < static_cast<size_t>(K)) {
                    for (int c = 0; c < K; ++c) {
                        if (!used.count(c)) {
                            value_to_color[node] = c;
                            break;
                        }
                    }
                } else {
                    // Real spill.
                    assert(std::holds_alternative<Variable>(node));
                    spill.insert(node);
                }
            }
        }

        // Debug print.
        for (const auto& l : value_to_color) {
            std::cout << "[" << valueToString(l.first) << "] = " << l.second << std::endl;
        }

        // TODO: if spill is non-empty, rewrite f to spill those variables to the
        // stack and call GraphColoring again on the rebuilt interference graph.
        // For now, throw so the caller knows we couldn't fully color.
        if (!spill.empty()) {
            throw std::runtime_error("spilling required — not yet implemented");
        }
    }

    // void GraphColoring(const L2::Graph& g, Function& f){
    //     std::map<VALUE, int> value_to_color;

    //     int num = 0;
    //     for (auto reg : g.allRegs){
    //         value_to_color[VALUE(reg)] = num++;
    //     } // use the enum for the mapping 
        
    //     bool fail_to_color = false;
        
    //     for (const auto& node : g.graph){
               
    //             // only coloring required if we have a variable. if it is only registers we can't optimize it here. 
    //         if (std::holds_alternative<Variable>(node.first)){
    //             // if i didn't color it yet
    //             if (value_to_color.find(node.first) == value_to_color.end()){
    //                 // find color for it. 
    //                 std::set color_option = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

    //                 for (const auto& neighbor : node.second){
    //                     if (value_to_color.find(neighbor) == value_to_color.end()) continue;  // if not colored continue
    //                     color_option.erase(value_to_color[neighbor]);
    //                 }
                    
    //                 if (color_option.size() <= 0){
    //                     fail_to_color = true; 
    //                     break;
    //                 }
    //                 // otherwise just color it with the first number you find. 
    //                 value_to_color[node.first] = *color_option.begin();
    //             }
                
    //         }
    //     }

    //     if (fail_to_color){
    //         // for now spill all the remaining nodes here -- no heuristic for now 
    //         throw std::runtime_error("can't color well");
    //     }

    //     for (auto l : value_to_color){
    //         std::cout << "[" << valueToString(l.first) << "] = " << l.second << std::endl;
    //     }
        
        
    //     return;


    // }

}