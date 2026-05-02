#pragma once

#include <l3.h>

#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>

namespace L3 {


    
      
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
                auto flush = [&]() {
                    if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
                };

                auto isPunct = [](char c) {
                    return c == '(' || c == ')' || c == '{' || c == '}' || c == ',';
                };

                for (size_t i = 0; i < s.size(); ++i) {
                    char c = s[i];
                    char n1 = (i + 1 < s.size()) ? s[i + 1] : '\0';

                    // whitespace
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                        flush();
                        continue;
                    }

                    // single-char punctuation
                    if (isPunct(c)) {
                        flush();
                        tokens.push_back(std::string(1, c));
                        continue;
                    }

                    // two-char operators: <-, <<, >>, <=, >=
                    if (c == '<' && n1 == '-') { flush(); tokens.push_back("<-"); ++i; continue; }
                    if (c == '<' && n1 == '<') { flush(); tokens.push_back("<<"); ++i; continue; }
                    if (c == '>' && n1 == '>') { flush(); tokens.push_back(">>"); ++i; continue; }
                    if (c == '<' && n1 == '=') { flush(); tokens.push_back("<="); ++i; continue; }
                    if (c == '>' && n1 == '=') { flush(); tokens.push_back(">="); ++i; continue; }

                    // single-char operators that always stand alone: <, >, =, +, *, &
                    // ('-' is special — could be op OR start of negative number OR inside 'tuple-error')
                    if (c == '<' || c == '>' || c == '=' || c == '+' || c == '*' || c == '&') {
                        flush();
                        tokens.push_back(std::string(1, c));
                        continue;
                    }

                    // '-' handling:
                    //   - if cur is non-empty (e.g. "tuple"), keep it attached → "tuple-error"
                    //   - if cur is empty and next char is a digit, it's a negative number → attach
                    //   - otherwise, it's the standalone '-' op
                    if (c == '-') {
                        if (!cur.empty()) { cur += c; continue; }            // tuple-error / tensor-error
                        if (n1 >= '0' && n1 <= '9') { cur += c; continue; }  // -42
                        tokens.push_back("-");
                        continue;
                    }

                    cur += c;
                }
                flush();
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





        L3::Program parse_file(const char* fileName);
}
