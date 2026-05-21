#include "l3.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include "generator.h"

namespace L3 {


    void  generate_code(const Program& p){

        if (!p.verify()) {
            exit(1);
        }


        std::ofstream outputFile;
        outputFile.open("prog.L2");


        outputFile << "(@main\n" ;  // by default each program entry point is main. 




        for (const auto& f : p.functions){
            outputFile << "\t(@" << f.getName() << '\n' ;
            outputFile << "\t " << std::to_string(f.getNumParams()) << '\n' ;
            CodeGenerator::currentFnPrefix = f.getName(); // add _ func name for all of them

            const auto& params = f.getParams();
            for (size_t i = 0; i < params.size(); ++i) {
                if (i < CodeGenerator::ARG_REGS.size()) {
                    outputFile << "\t" << params[i].to_string()
                            << " <- "
                            << CodeGenerator::registerToString(CodeGenerator::ARG_REGS[i])
                            << '\n';
                } else {
                    // 7th param onwards lives on the stack
                    int64_t offset = 8 * static_cast<int64_t>(i - CodeGenerator::ARG_REGS.size());
                    outputFile << "\t" << params[i].to_string()
                            << " <- stack-arg " << offset << '\n';
                }
            }




            for (const auto& instr : f.instructions){
                outputFile << CodeGenerator::generate(*instr);
            }
            outputFile << "\t)" << '\n' ;
        }
   
        outputFile << ')' << '\n' ;

        

        outputFile << "\n";
        outputFile.close();
        return;
    }
    
    


};