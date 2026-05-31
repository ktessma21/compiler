#include <l2.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>
#include <interference.h>
#include <coloring.h>
#include <map>
#include <set>
#include <cassert>
#include <spiller.h>
#include <iostream>


#include <stack>

namespace L2 {

    void GraphColoring(L2::Graph& g, Function& f){

        std::map<VALUE, int> value_to_color;

        int num = 0;
        for (auto reg : g.allRegs){
            value_to_color[VALUE(reg)] = num++;
        }

        assert(value_to_color.size() == 15);

        
        L2::Graph copy(g);

        /* Select Node */

        std::stack<VALUE> nodes;
        std::vector<VALUE> to_remove;

        // select all the nodes with less than 15 colors. 
        for (auto& n : g.graph) {
            const VALUE& v = n.first;
            if (value_to_color.find(v) != value_to_color.end()) {
                to_remove.push_back(v);
            } else {
                if (n.second.size() < 15){
                    nodes.push(v);
                    to_remove.push_back(v);
                }
            }
        }

        // sort by degree
        std::vector<std::pair<VALUE, size_t>> by_degree;
        for (auto& [v, neighbors] : g.graph) {
            by_degree.push_back({v, neighbors.size()});
        }
       
        std::sort(by_degree.begin(), by_degree.end(), [](auto& a, auto& b){
            return a.second < b.second; // descending by degree
        });

        // then select in descending order from the biggest to the smallest. 
        for (auto& n : by_degree) {
            const VALUE& v = n.first;
            nodes.push(v);
            to_remove.push_back(v);
        }
       

        for (const auto& v : to_remove) {
            g.removeNode(v);
        }


        /* Everything is removed so now it is the time */
        /* Rebuild the graph and color it properly */
        assert(g.graph.empty());

        L2::LiveSet needs_spilling;

        while (!nodes.empty()){
            VALUE node = nodes.top();
            nodes.pop();

            // collect colors already used by neighbors
            std::set<int> used;
            for (auto& neighbor : copy.graph[node]){
                auto it = value_to_color.find(neighbor);
                if (it != value_to_color.end()){
                    used.insert(it->second);
                }
            }

            // find first color not used
            int chosen = -1;
            for (int i = 0; i < 15; i++){
                if (used.find(i) == used.end()){
                    chosen = i;
                    break;
                }
            }

            if (chosen == -1){
                needs_spilling.insert(node);
            } else {
                value_to_color[node] = chosen;
            }
        }


        return;
    }

}