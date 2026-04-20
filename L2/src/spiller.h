#pragma once

#include <l2.h>

namespace L2 {

    // class Converter {
	// 	public:
	// 		static std::string toString(Register r) {
	// 			return "%" + registerToString(r);
	// 		}

	// 		static std::string toString(const memoryAccess& mem) {
	// 			std::string baseStr = std::visit([](const auto& b) -> std::string {
	// 				using T = std::decay_t<decltype(b)>;
	// 				if constexpr (std::is_same_v<T, Register>) return "%" + registerToString(b);
	// 				else return b;  // std::string variable name, e.g. "%var1" — already has %
	// 			}, mem.base);
	// 			return std::to_string(mem.size) + "(" + baseStr + ")";
	// 		}

	// 		static std::string toString(const Label& l) {
	// 			// Variables start with '%', real labels don't.
	// 			if (!l.empty() && l[0] == '%') return l;        // variable: emit as-is
	// 			return "_" + l;                                  
	// 		}

	// 		static std::string toString(const Number& n) {
	// 			return "$" + std::to_string(n.getValue());
	// 		}

	// 		static std::string toString(const VALUE& v) {
	// 			return std::visit([](const auto& val) -> std::string {
	// 				return Converter::toString(val);
	// 			}, v);
	// 		}
	// };

	
	void spliller(Function& f, std::string to_be_allocated, std::string replacer);
}
