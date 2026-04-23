#include "l2.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include <spiller.h>
#include <stdexcept>
#include <liveness.h>
#include <inference.h>
#include <set>
#include <vector>
// #include <map>

namespace L2 { 
    void Inference(Function& f) {
        
        std::vector<LiveSet> in;
        std::vector<LiveSet> out;

        Liveness(f, in, out);

        Graph g;
        // for (auto& s : in) g.add(s);
       
        // for (auto& instr : f.instructions){
        //     auto& writes = instr->writesLive();
        //     s.insert(writes.begin(), writes.end());
        // }
        for (auto& i : in) g.add(i);
        for (auto& i : out) g.add(i);

        for (const auto& instr : f.instructions){
            if (instr->type == InstructionType::WsopSx) {
                auto* shift = dynamic_cast<ShiftInstruction*>(instr.get());
                if (shift && shift->src.has_value() &&
                    !std::holds_alternative<Register>(shift->src.value())) {
                    g.connect_with_everything_except_sx(shift->src.value());
                }
            }
            L2::LiveSet s;
            auto writes = instr->writesLive();
            auto reads = instr->readsLive();
            s.insert(reads.begin(), reads.end()); // merge them
            s.insert(writes.begin(), writes.end());
            g.add(s); // connect everything. 
        }

        // after we connect all the in and out. 
        
        g.printItems();

    }


};