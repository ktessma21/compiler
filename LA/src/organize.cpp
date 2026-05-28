#include "la.h"
#include "ast_leaves.h"
#include "generate.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <stdexcept>
#include <cassert>
#include <cstdlib>

namespace LA {



    static bool debug(){
        if (std::getenv("LA_DEBUG") != nullptr) {
            return true;
        }
        return false;
    }

    static int64_t freshLabelCounter = 0;
 
    static Label freshLabel() {
        return Label("label_never_used_please_" + std::to_string(freshLabelCounter++));
    }


    void organize_functions(Program& p){

        for (auto& f : p.functions){

            // if (debug()){
            //     std::cerr << "Organizing function: " << f.getName() << std::endl;
            // }

            bool startBB = true;
            std::vector<std::unique_ptr<Instruction>> newInstructions;

            // ----- function entry needed first ----
            auto Inst = f.instructions.begin();

            while (Inst != f.instructions.end()){
                if (startBB){
                    if ((*Inst)->type != InstructionType::Label){
                        std::unique_ptr<LabelInstruction> label = std::make_unique<LabelInstruction>();
                        label->setLabel(freshLabel());
                        newInstructions.push_back(std::move(label));
                    }
                    startBB = false;
                }
                else if ((*Inst)->type == InstructionType::Label){
                    std::unique_ptr<BrInstruction> br = std::make_unique<BrInstruction>();
                    auto lbl = dynamic_cast<LabelInstruction*>((*Inst).get());
                    Label target = lbl->getLabel().value();
                    br->setTarget(target);
                    newInstructions.push_back(std::move(br));

                    // if (debug()){
                    //     std::cerr << "Inserted branch to " << target.to_string() << " before label " << target.to_string() << std::endl;
                    // }

                }


                InstructionType ty = (*Inst)->type;

                newInstructions.push_back(std::move(*Inst));

                if (ty == InstructionType::Br ||
                    ty == InstructionType::BrT ||
                    ty == InstructionType::Return ||
                    ty == InstructionType::ReturnT) {
                    startBB = true;
                }

                ++Inst;

                // if (debug()){
                //     std::cerr << "Current instruction type: " << static_cast<int>(ty) << std::endl;
                // }


            }
            if (!startBB || f.instructions.empty()){
                if (f.instructions.empty()){
                    std::unique_ptr<LabelInstruction> label = std::make_unique<LabelInstruction>();
                    label->setLabel(Label("entry"));
                    newInstructions.push_back(std::move(label));

                }
                if (f.getReturnType() == Type(VarType::Void)){
                            std::unique_ptr<ReturnInstruction> retVoid = std::make_unique<ReturnInstruction>();
                            newInstructions.push_back(std::move(retVoid));
                        }
                else {
                    std::unique_ptr<ReturnTInstruction> retT = std::make_unique<ReturnTInstruction>();
                    retT->setValue(Number(0));
                    newInstructions.push_back(std::move(retT));
                }
            }
            

                
            


            f.instructions = std::move(newInstructions);

        }
    }
};