#pragma once

#include <lb.h>

#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>
#include <stdexcept>

namespace LB {

    /* ------------------------------------------------------------------
     *  Tokenizer — same shape as LA's, with LB-specific keywords/symbols.
     *  Splits a string on whitespace and hands out tokens one at a time.
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
                    return c == '(' || c == ')' || c == '{' || c == '}' || c == ','
                        || c == '[' || c == ']';
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

                    // single-char operators that always stand alone
                    if (c == '<' || c == '>' || c == '=' || c == '+' || c == '*' || c == '&') {
                        flush();
                        tokens.push_back(std::string(1, c));
                        continue;
                    }

                    // '-' handling
                    if (c == '-') {
                        // tuple-error / tensor-error compound names
                        if (cur == "tuple" || cur == "tensor") { cur += c; continue; }

                        // Decide: binary subtract vs unary minus / negative literal
                        bool prev_is_value = false;
                        if (!cur.empty()) {
                            prev_is_value = true;
                        } else if (!tokens.empty()) {
                            char pc = tokens.back()[0];
                            prev_is_value = (pc == '%' || pc == '@' || pc == ':' || pc == ')'
                                            || pc == ']'
                                            || (pc >= '0' && pc <= '9'));
                        }

                        flush();
                        if (prev_is_value) {
                            tokens.push_back("-");
                        } else if (n1 >= '0' && n1 <= '9') {
                            cur += c;
                        } else {
                            tokens.push_back("-");
                        }
                        continue;
                    }
                    cur += c;
                }
                flush();
            }

            std::string next() {
                if (pos >= tokens.size())
                    throw std::runtime_error("tokenizer: no more tokens");
                return tokens[pos++];
            }

            const std::string& peek(size_t offset = 0) const {
                if (pos + offset >= tokens.size())
                    throw std::runtime_error("tokenizer: peek past end");
                return tokens[pos + offset];
            }

            bool done() const { return pos >= tokens.size(); }
            size_t remaining() const { return tokens.size() - pos; }

            void expect(const std::string& s) {
                std::string t = next();
                if (t != s)
                    throw std::runtime_error("tokenizer: expected '" + s + "' got '" + t + "'");
            }
        };


    LB::Program parse_file(const char* fileName);
    void organize_functions(Program& p);
    void translate_statements(Program& p);
    void generate_code(Program& p);
}