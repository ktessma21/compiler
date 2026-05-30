#include "lb.h"
#include "ast_leaves.h"
#include "generate.h"
#include <fstream>
#include <iostream>

namespace LB {



    void generate_code(Program& p) {

        std::ofstream outputFile;
        outputFile.open("prog.LA");

        std::set<std::string> funcNames;
        for (auto& f : p.functions)
            funcNames.insert(f.getName());
        CodeGenerator::functionNames = funcNames;

        for (auto& f : p.functions) {
            CodeGenerator::currentFunction = &f;

            const auto& params     = f.getParams();
            const auto& paramTypes = f.getParamTypes();

            // function header
            outputFile << f.getReturnType().to_string()
                    << " " << f.getName() << "(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i) outputFile << ", ";
                outputFile << paramTypes[i].to_string() << " " << params[i].name;
            }
            outputFile << ")\n";

            // opening brace
            outputFile << "{\n";

            // flat instructions
            if (f.rootScope) {
                for (auto& item : f.rootScope->items) {
                    if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                        auto& ins = std::get<std::unique_ptr<Instruction>>(item);
                        outputFile << CodeGenerator::generate(*ins);
                    }
                }
            }

            // closing brace
            outputFile << "}\n\n";
        }

        outputFile.close();
    }
}