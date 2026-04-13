#include "l1.h"
#include <fstream>
#include <iostream>
#include <utils.h>

namespace L1 {


    void generate_code(const Program& p){

        if (!p.verify()) {
            exit(1);
        }

        if (Utils::verbose){
            std::cout << "generating" << std::endl;
        }

        std::ofstream outputFile;
        outputFile.open("prog.S");
   
    /* 
     * Generate target code
     */ 
    //TODO
        outputFile << "\t.text\n";
        outputFile << "\t.globl go\n\n";
        outputFile << "go:\n";
        outputFile << "\t# save callee-saved registers\n";
        outputFile << "\tpushq %rbx\n";
        outputFile << "\tpushq %rbp\n";
        outputFile << "\tpushq %r12\n";
        outputFile << "\tpushq %r13\n";
        outputFile << "\tpushq %r14\n";
        outputFile << "\tpushq %r15\n\n";
        outputFile << "\tcall _"<< p.label << "\n\n";
        outputFile << "\t# restore callee-saved registers and return\n";
        outputFile << "\tpopq %r15\n";
        outputFile << "\tpopq %r14\n";
        outputFile << "\tpopq %r13\n";
        outputFile << "\tpopq %r12\n";
        outputFile << "\tpopq %rbp\n";
        outputFile << "\tpopq %rbx\n";
        outputFile << "\tretq\n\n";

        outputFile << p.generate_code();

        outputFile << "\n";
        outputFile.close();
        return;
    }

    


};