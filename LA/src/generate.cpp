#include "la.h"
#include "ast_leaves.h"
#include "generate.h"
#include <fstream>
#include <iostream>

namespace LA {

    void generate_code(Program& p) {

        if (!p.verify()) {
            exit(1);
        }

        std::ofstream outputFile;
        outputFile.open("prog.IR");

        for (auto& f : p.functions) {

            CodeGenerator::currentFunction = &f;

            // ----- function start -----
            const auto& params = f.getParams();

            outputFile << "define " << f.getReturnType().to_string()
                       << " @" << f.getName() << "(";
            for (size_t i = 0; i < params.size(); ++i) {
                outputFile << "%" << params[i].name;
                if (i + 1 < params.size()) {
                    outputFile << ", ";
                }
            }
            outputFile << ") {\n";

            
            size_t i = 0;
            const size_t n = f.instructions.size();
        
            while (i < n) {
                const auto& instr = f.instructions[i];
                outputFile << CodeGenerator::generate(*instr);
                ++i;
            }

            outputFile << "}\n";
        }

        outputFile.close();
        return;
    }

}