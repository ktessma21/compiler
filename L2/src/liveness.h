#pragma once

#include <l2.h>
#include <unordered_map>


namespace L2 {

		struct VALUEComparator {
			bool operator()(const VALUE& a, const VALUE& b) const {
				auto key = [](const VALUE& v) -> std::string {
					std::string s;
					if (std::holds_alternative<Variable>(v)) {
						// Drop the leading '%' so "%val_to_test" compares after "r15"
						s = std::get<Variable>(v).name.substr(1);
					} else {
						s = valueToString(v);
					}
					// Lowercase for case-insensitive comparison (test 1)
					for (char& c : s) c = std::tolower((unsigned char)c);

					// Tiebreak: Variable sorts BEFORE Register when names match.
					// Without this, set<LiveCompare> treats %r12 and r12 as equivalent
					// and silently deduplicates one of them.
					if (std::holds_alternative<Variable>(v))      s += "a";
					else if (std::holds_alternative<Register>(v)) s += "b";
					return s;
				};
				return key(a) < key(b);
			}
    	};

   	 	using LiveSet = std::set<VALUE, VALUEComparator>;

		struct SC {

			
			std::vector<std::vector<size_t>> successors;

			void build(const Function& f){
				successors.assign(f.instructions.size(), {});  
				
				std::unordered_map<std::string, size_t> labelIndex;
				for (size_t j = 0; j < f.instructions.size(); j++) {
					if (auto* lbl = dynamic_cast<LabelInstruction*>(f.instructions[j].get())) {
						labelIndex[lbl->label.name] = j;
					}
				}

				for (size_t i = 0; i < f.instructions.size(); i++){

					switch (f.instructions[i]->type) {
						// Single successor: fall through to i+1
						case InstructionType::compareAssign:
						case InstructionType::AssignFromMemory:
						case InstructionType::AssignFromS:
						case InstructionType::AssignMemoryFromS:
						case InstructionType::AssignFromStack:
						case InstructionType::WaopT:
						case InstructionType::WsopSx:
						case InstructionType::WsopN:
						case InstructionType::WIncDec:
						case InstructionType::MemoryIncDecT:
						case InstructionType::WIncDecMemory:
						case InstructionType::WAtWWE:
						case InstructionType::CallPrint:
						case InstructionType::CallInput:
						case InstructionType::CallAllocate:
						case InstructionType::CallUN:
						case InstructionType::Label:
							successors[i] = {i + 1};
							break;

						// No successor (terminates the function / never returns)
						case InstructionType::Return:
						case InstructionType::CallTupleError:
						case InstructionType::CallTensorError:
							break;

						// One successor but NOT i+1 — jumps unconditionally
						case InstructionType::Goto :{
							auto* g = dynamic_cast<GotoInstruction*>(f.instructions[i].get());
							successors[i] = { labelIndex.at(g->label.name) };
							break;
						}
						// Two successors: fall through AND jump
						case InstructionType::CJump : {
							auto* c = dynamic_cast<CjumpInstruction*>(f.instructions[i].get());
							successors[i] = { i+ 1, labelIndex.at(c->label.name) };
							break;
						}
						
						case InstructionType::Unknown:
							assert(false && "CFG: unknown instruction type");
					}
				}

			}

			// void run(){
			// 	if (successors.empty()) return;

			// 	for (size_t i = 0; i < f.instructions.size(); i++){

			// 	}
			// }
	
		};
	
	void LivenessPrint(Function& f);
	void Liveness(Function& f, std::vector<LiveSet>& InSet, std::vector<LiveSet>& OutSet);
}
