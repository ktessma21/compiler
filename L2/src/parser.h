#pragma once

#include <l2.h>

#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>

namespace L2 {


	struct SpillInput {
        Function function;
        std::string target;   
        std::string prefix;   
    };


	L2::Function parse_function_file(const char* fileName);
	SpillInput parse_spill_file(const char* fileName);
    L2::Function parse_l2_function(const std::string& source);
    L2::Program parse_file(const char* fileName);

      
   /* ------------------------------------------------------------------
     *  Tokenizer — splits a string on whitespace and hands out tokens
     *  one at a time. Replaces all the manual pos++ / find / substr
     *  scaffolding the actions used to do by hand.
     * ------------------------------------------------------------------ */
    class Tokenizer {
        std::vector<std::string> tokens;
        size_t pos = 0;

        public:
            explicit Tokenizer(const std::string& s) {
                std::string cur;
                for (char c : s) {
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                        if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
                    } else {
                        cur += c;
                    }
                }
                if (!cur.empty()) tokens.push_back(cur);
            }

            // get next token and advance
            std::string next() {
                if (pos >= tokens.size())
                    throw std::runtime_error("tokenizer: no more tokens");
                return tokens[pos++];
            }

            // look without advancing
            const std::string& peek(size_t offset = 0) const {
                if (pos + offset >= tokens.size())
                    throw std::runtime_error("tokenizer: peek past end");
                return tokens[pos + offset];
            }

            bool done() const { return pos >= tokens.size(); }
            size_t remaining() const { return tokens.size() - pos; }

            // consume and verify (useful for "mem", "<-", etc.)
            void expect(const std::string& s) {
                std::string t = next();
                if (t != s)
                    throw std::runtime_error("tokenizer: expected '" + s + "' got '" + t + "'");
            }
        };

}
