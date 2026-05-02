#include "l2.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include "spiller.h"
#include <stdexcept>
#include <sstream>
// #include <map>

namespace L2 {


    std::string Spill(Function& f, std::string target, std::string replacer){

        if (!f.verify()) throw std::runtime_error("File verification failed!");
        int counter = 0;
        bool spilled = false;

        // helperSpill(f, to_be_allocated, replacer, spilled);
        std::vector<std::unique_ptr<Instruction>> new_instructions;

        std::stringstream ss; 
        ss << '(' << f.getLabel() << '\n';

        std::string result;

        for (auto& instr : f.instructions){

            // std::cout << instr->to_string() << "is: \n";
            auto read = instr->reads();
            auto write = instr->writes();

            bool isRead = read.contains(Variable(target));
            bool isWrite = write.contains(Variable(target));


            if (!isRead && !isWrite){ 
                result += instr -> to_string();
                new_instructions.push_back(std::move(instr));
                continue;
            }

            spilled = true;
            Variable fresh = Variable(replacer + std::to_string(counter++));

            if (isRead){ // we are loading. 
                    // %S1 <- mem rsp 0 - must do 
                auto new_instr = std::make_unique<AssignInstruction>(InstructionType::AssignFromMemory);
                memoryAccess m;
                m.base = Register::rsp;
                m.size = 0;
                new_instr -> setFrom(VALUE(m));
                new_instr -> setTo(fresh);
                result += new_instr -> to_string();
                new_instructions.push_back(std::move(new_instr));
                
            }


            instr->replaceVar(Variable(target), VALUE(fresh));
            result += instr -> to_string();
            new_instructions.push_back(std::move(instr));

            if (isWrite){
                auto new_instr = std::make_unique<AssignInstruction>(InstructionType::AssignMemoryFromS);
                memoryAccess m;
                m.base = Register::rsp;
                m.size = 0;
                new_instr->setTo(VALUE(m));  
                new_instr->setFrom(fresh);
                result += new_instr->to_string();
                new_instructions.push_back(std::move(new_instr));
     
            }
        }

        if (spilled){
            f.setNumLocal(f.getNumLocals() + 1);
        }

        if (!spilled)
            ss << '\t' << std::to_string(f.getNumArgs()) << ' ' << f.getNumLocals() << '\n';
        else
            ss << '\t' << std::to_string(f.getNumArgs()) << ' ' << f.getNumLocals() << '\n';

        f.instructions = std::move(new_instructions);
        
        ss << result;
        ss << ')' << '\n';
        
        return ss.str();
    }
    
    


};