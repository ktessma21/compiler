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


            const auto& params = f.getParams();
            const auto& paramTypes = f.getParamTypes();
            for (size_t i = 0; i < params.size(); ++i) {
                f.declTypes[params[i].name] = paramTypes[i].base;
            }
            f.declTypes.insert(p.declTypes.begin(), p.declTypes.end());

            std::set<std::string> funcNames;
            for (auto& f : p.functions)
                funcNames.insert(f.getName());
            CodeGenerator::functionNames = funcNames;


            CodeGenerator::currentFunction = &f;

            outputFile << "define " << f.getReturnType().to_string()
                       << " @" << f.getName() << "(";
            for (size_t i = 0; i < params.size(); ++i) {
                outputFile << paramTypes[i].to_string() << " %" << params[i].name;
                
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