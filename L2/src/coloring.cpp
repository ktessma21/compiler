
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
        std::map<VALUE, int> value_to_color;

        int num = 0;
        for (auto reg : g.allRegs){
            value_to_color[VALUE(reg)] = num++;
        }
        
        bool fail_to_color = false;
        
        for (const auto& node : g.graph){
               
                // only coloring required if we have a variable. if it is only registers we can't optimize it here. 
            if (std::holds_alternative<Variable>(node.first)){
                // if i didn't color it yet
                if (value_to_color.find(node.first) == value_to_color.end()){
                    // find color for it. 
                    std::set color_option = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

                    for (const auto& neighbor : node.second){
                        if (value_to_color.find(neighbor) == value_to_color.end()) continue;  // if not colored continue
                        color_option.erase(value_to_color[neighbor]);
                    }
                    
                    if (color_option.size() <= 0){
                        fail_to_color = true; 
                        break;
                    }
                    // otherwise just color it with the first number you find. 
                    value_to_color[node.first] = *color_option.begin();
                }
                
            }
        }

        if (fail_to_color){
            // for now spill all the remaining nodes here -- no heuristic for now 
            throw std::runtime_error("can't color well");
        }

        for (auto l : value_to_color){
            std::cout << "[" << valueToString(l.first) << "] = " << l.second << std::endl;
        }
        
        
        return;


    }

}