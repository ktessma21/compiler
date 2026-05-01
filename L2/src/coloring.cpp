
#include <l2.h>

#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>
#include <interference.h>
#include <coloring.h>
#include <map>

namespace L2 { 
    void GraphColoring(const L2::Graph& g, Function& f){
        std::map<VALUE, int> coloring;
        
        int num = 0;
        bool fail_to_color = false;
        
        for (const auto& node : g.graph){
            if (coloring.size() > 15) {
                    fail_to_color = true;
                    break;
                }
               

                // only coloring required if we have a variable. if it is only registers we can't optimize it here. 
            if (std::holds_alternative<Variable>(node.first)){
                if (coloring.find(node.first) == coloring.end()){
                    coloring[node.first] = num++;
                }
                
                for (const auto& neighbor : node.second){
                    if (coloring.find(neighbor) == coloring.end()) continue;
                    coloring[neighbor] = num++;
                }
            }
               
        }

        if (fail_to_color){
            // for now spill all the remaining nodes here -- no heuristic for now 
            throw std::runtime_error("can't color well");
        }

        for (auto l : coloring){
            std::cout << "[ " << valueToString(l.first) << "] = " << l.second << std::endl;
        }
        
        
        return;


    }

}