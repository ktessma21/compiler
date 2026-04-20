#include "l2.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include "spiller.h"
#include <stdexcept>
// #include <map>

namespace L2 {


    static void helperSpill(Function& f, std::string to_be_allocated, std::string replacer, bool& spilled){

        for (auto& instr : f.instructions){
            const auto& all_variables = instr -> reads();
            if (all_variables.contains(Variable(to_be_allocated))){
                spilled = true;
            }
        }
        spilled = false;
    };

    void Spill(Function& f, std::string to_be_allocated, std::string replacer){

        if (!f.verify()) throw std::runtime_error("File verification failed!");


        int number_to_use = 0;
        bool splilled = false;

        helperSpill(f, to_be_allocated, replacer, splilled);

        std::cout << '(' << f.getLabel() << '\n';

        if (!splilled)
            std::cout << std::to_string(f.getNumArgs()) << '0' << '\n';
        else
            std::cout << std::to_string(f.getNumArgs()) << '1' << '\n';
        
        return;
    }
    
    


};