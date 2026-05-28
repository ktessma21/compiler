#include "lb.h"
#include "ast_leaves.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <stdexcept>
#include <cassert>
#include <cstdlib>

namespace LB {



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

    // handled during parsing. 
    // std::vector<std::unique_ptr<Instruction>> multi_decl_into_singles(std::unique_ptr<DeclInstruction>)

    void organize_functions(Program& p){
        return;
        
    }
};