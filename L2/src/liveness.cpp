#include "l2.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include "spiller.h"
#include <stdexcept>
#include <liveness.h>
#include <set>
#include <vector>
// #include <map>

namespace L2 { 
    // std::vector<std::vector<size_t>> successors;

   struct LiveCompare {
        bool operator()(const VALUE& a, const VALUE& b) const {     // ← const
            if (a.index() != b.index()) return a.index() < b.index();
            if (std::holds_alternative<Register>(a))
                return std::get<Register>(a) < std::get<Register>(b);
            if (std::holds_alternative<Variable>(a))
                return std::get<Variable>(a) < std::get<Variable>(b);
            return false;
        }
    };

    using LiveSet = std::set<VALUE, LiveCompare>;

    void Liveness(Function& f) {
        L2::SC sc;
        sc.build(f);

        std::vector<LiveSet> in(f.instructions.size());
        std::vector<LiveSet> out(f.instructions.size());

        bool keep_going = true;
        while (keep_going) {
            keep_going = false;

            for (int i = (int)f.instructions.size() - 1; i >= 0; i--) {

                
                // out[i] = union of in[successor] for each successor
                LiveSet liveOut;
                for (size_t s : sc.successors[i]) {
                    liveOut.insert(in[s].begin(), in[s].end());
                }

                // in[i] = (out[i] - writes) ∪ reads
                LiveSet liveIn = liveOut;
                for (const auto& w : f.instructions[i]->writes()) liveIn.erase(VALUE(w));
                for (const auto& r : f.instructions[i]->reads())  liveIn.insert(VALUE(r));


                // special case handle
                if (f.instructions[i]->type == InstructionType::Return) {
                    liveIn.insert(VALUE(Register::rax));
                    liveIn.insert(VALUE(Register::rbx));
                    liveIn.insert(VALUE(Register::rbp));
                    liveIn.insert(VALUE(Register::r12));
                    liveIn.insert(VALUE(Register::r13));
                    liveIn.insert(VALUE(Register::r14));
                    liveIn.insert(VALUE(Register::r15));
                }

                if (f.instructions[i]->type == InstructionType::CallUN ||
                    f.instructions[i]->type == InstructionType::CallPrint ||
                    f.instructions[i]->type == InstructionType::CallInput ||
                    f.instructions[i]->type == InstructionType::CallAllocate ||
                    f.instructions[i]->type == InstructionType::CallTupleError ||
                    f.instructions[i]->type == InstructionType::CallTensorError) {

                    for (Register r : {Register::rax, Register::rcx, Register::rdx, Register::rdi,
                       Register::rsi, Register::r8,  Register::r9,
                       Register::r10, Register::r11}) {
                        liveIn.erase(VALUE(r));
                    }

                    auto* call = dynamic_cast<CallInstruction*>(f.instructions[i].get());
                    int n = call->arg.value_or(0);
                    std::vector<Register> argRegs = {Register::rdi, Register::rsi, Register::rdx,
                                                    Register::rcx, Register::r8,  Register::r9};
                    for (int k = 0; k < std::min(n, (int)argRegs.size()); k++) {
                        liveIn.insert(VALUE(argRegs[k]));
                    }
                    if (call->type == InstructionType::CallUN && call->callee) {
                        if (std::holds_alternative<Variable>(*call->callee) ||
                            std::holds_alternative<Register>(*call->callee)) {
                            liveIn.insert(*call->callee);
                        }
                    }
                }
              
                if (liveIn != in[i] || liveOut != out[i]) {
                    in[i]  = std::move(liveIn);
                    out[i] = std::move(liveOut);
                    keep_going = true;
                }
            }
        }

        std::cout << '(' << '\n';
        std::cout << '(in' << '\n';


    }

    


};