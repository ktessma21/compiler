#include "l2.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <liveness.h>
#include <set>
#include <unordered_map>
#include <vector>
// #include <map>

namespace L2 { 
   
  
    class Graph {
        inline static const std::vector<Register> allRegs = {
            Register::rax, Register::rbx, Register::rcx, Register::rdx,
            Register::rdi, Register::rsi, Register::rbp,
            Register::r8,  Register::r9,  Register::r10, Register::r11,
            Register::r12, Register::r13, Register::r14, Register::r15
        };
        public:
            std::map<VALUE, L2::LiveSet, VALUEComparator> graph;
            Graph() {
                for (auto r1 : allRegs) {
                    graph[VALUE(r1)];
                    for (auto r2 : allRegs) {
                        if (r1 != r2) graph[VALUE(r1)].insert(VALUE(r2));
                    }
                }
            }
            void add(L2::LiveSet& s){
                    for (auto it1 = s.begin(); it1 != s.end(); it1++){
                        auto it2 = std::next(it1);   // always add the one next to the it1 so no duplicate
                        for (; it2 != s.end(); ++it2) {  
                            graph[*it1].insert(*it2);
                            graph[*it2].insert(*it1);
                        }
                    }
            }

            void connect_with_everything_except_sx(VALUE& v) {
                for (auto& entry : graph) {
                    if (entry.first != v &&
                        std::holds_alternative<Register>(entry.first) &&
                        std::get<Register>(entry.first) != Register::rcx) {
                        graph[entry.first].insert(v);
                        graph[v].insert(entry.first);
                    }
                }
            }
            // void add(VALUE& a, VALUE& b){
            //     graph[a].insert(b);
            //     graph[b].insert(a);
            // }


            void printItems() const {
                for (auto& entry : graph){
                    std::cout << valueToString(entry.first);
                    for (auto& val : entry.second){
                        std::cout << " " << valueToString(val);
                    }
                    std::cout << '\n';
                }
            }
    };

    void Inference(Function& f);

    
};