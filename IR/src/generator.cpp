#include "IR.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include "generator.h"

namespace IR {


    void  generate_code(Program& p){

        if (!p.verify()) {
            exit(1);
        }

        


        std::ofstream outputFile;
        outputFile.open("prog.L3");

        for (auto& f : p.functions){
            if (f.traces.empty()){
                std::cerr << "Functions without traces: " << f.getName() << std::endl;
                exit(1);
            }
            CodeGenerator::currentFunction = &f;

            // cause a segmentation fault
            // for (size_t i = 0; i < f.getNumParams(); ++i) {
            //     f.varTypes[f.getParams()[i].name] = f.getParamTypes()[i];
            // }

            outputFile << "define " + f.getReturnType().to_string() + " " + f.getName() + "(";
            

            for (const auto& tr : f.traces) {
                if (tr.empty()) continue;

                // Convert list to vector for indexed access, or use an iterator-based peek.
                std::vector<const BasicBlock*> blocks(tr.begin(), tr.end());

                for (size_t i = 0; i < blocks.size(); ++i) {
                    const BasicBlock* bb = blocks[i];
                    const BasicBlock* next = (i + 1 < blocks.size()) ? blocks[i + 1] : nullptr;

                    if (bb->label) {
                        outputFile <<  CodeGenerator::generate(*bb->label);
                    }

                    for (const auto& instr : bb->instructions) {
                        outputFile << CodeGenerator::generate(*instr);
                    }

                    if (bb->terminator) {
                        outputFile << CodeGenerator::generate(*bb->terminator, next);
                    }
                }
            }
        }
        
        outputFile.close();
        return;
    }
    



    


};