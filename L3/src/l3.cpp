#include "l3.h"
#include "context.h"

namespace L3 {

    std::string Function::to_string() const {
        std::string result;
        result += "define " + this->name.name + "(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) result += ", ";
            result += "%" + params[i].name;
        }
        result += ") {\n";
        // std::cerr << "joined" ;
        if (_is_context) {
            for (const auto& ctx : contexts) {
                for (const auto& instr : ctx.get()) {
                    result += instr->to_string();
                }
            }
        } else {
            for (const auto& instr : instructions) {
                result += instr->to_string();
            }
        }
        result += "}\n";
        return result;
    }

    void Function::build_blocks() {
        std::vector<LivenessInfo> result = compute_liveness(*this); 
        auto begin = result.begin();
        auto upto  = result.begin();

        Context current;
        for (auto& instr : instructions) {
            if ((instr->type == InstructionType::Label || 
                instr->type == InstructionType::Call) && !current.empty()) {
                current.add(begin, upto);  // assign liveness before push
                contexts.push_back(std::move(current));
                current = Context{};
                begin = upto;             // ← advance begin to start of new context
            }

            current.add(std::move(instr));
            upto++;

            if (current.is_terminated()) {
                current.add(begin, upto); // assign liveness before push
                contexts.push_back(std::move(current));
                current = Context{};
                begin = upto;             // ← advance begin
            }
        }

        if (!current.empty()) {
            current.add(begin, upto);     // don't forget the last context
            contexts.push_back(std::move(current));
        }

        instructions.clear();
        this->_is_context = true;
    }

} // namespace L3