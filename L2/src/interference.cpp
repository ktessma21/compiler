#include "l2.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include <spiller.h>
#include <stdexcept>
#include <liveness.h>
#include <interference.h>
#include <set>
#include <vector>
// #include <map>

namespace L2 { 
    L2::Graph Interference(Function& f) {
        
        std::vector<LiveSet> in;
        std::vector<LiveSet> out;

        Liveness(f, in, out);

        Graph g;
        for (auto& i : in) g.add(i);
      
        for (int i = (int)f.instructions.size() - 1; i >= 0; i--){
                    // handle special case of sx
            const auto& instr = f.instructions[i];

            if (instr->type == InstructionType::WsopSx) {
                auto* shift = dynamic_cast<ShiftInstruction*>(instr.get());
                if (shift && shift->src.has_value() &&
                    !std::holds_alternative<Register>(shift->src.value())) {
                    g.connect_with_everything_except_sx(shift->src.value());
                }
            }
           
            auto writes = instr->writesLive();
            out[i].insert(writes.begin(), writes.end()); // merge i.e connect kill[i] and out[i]
            
        }

        
        for (auto& i : out) g.add(i);

        // after we connect all the in and out. 
        
        // g.printItems();

        return g;

    }


};