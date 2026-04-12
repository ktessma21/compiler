
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace L1 {
    class ASTNode {
        public:
            virtual ~ASTNode() = default;
            virtual std::string to_string() = 0;
    };

    
    // this could be a variant or I mean a UNION
    class Number : public ASTNode {
        int64_t value;
        public:
            Number() : value(0) {}
            Number(int64_t _value) : value(_value) { }
            std::string to_string() override {
                return std::to_string(value);
            }
    };

    class Pointer : public ASTNode {
        uint64_t value;
    public:
        Pointer() : value(0) {}
        Pointer(uint64_t _value) : value(_value) {}
        std::string to_string() override {
            return std::to_string(value);
        }
    };
    
    class Register : public ASTNode {
        std::string value;
        public:
            Register() : value("") {}
            Register(std::string _value) : value(_value) {}
            std::string to_string() override {
                    return value;  // or "@" + value, depending on what you want
                }
    };



    class Label : public ASTNode {
        std::string value;
        public:          // <-- must be public
            Label() : value("") {}
            Label(std::string _value) : value(std::move(_value)) {}
            Label(const char* begin) : value(begin) {} 
            std::string to_string() override {
                    return value;  // or "@" + value, depending on what you want
                }
    };

    class Instruction : public ASTNode {
        public:
            virtual ~Instruction() = default;
    };

    class AssignInstruction : public Instruction {};
    class PrintInstruction  : public Instruction {};
    class ReturnInstruction : public Instruction {};
    class InputInstruction  : public Instruction {};

     // create a type name called NumPoi : could be Number or Pointer
    using NumPoi  = std::variant<Number, Pointer>;  
    
    class Function : public ASTNode {
        public:
            std::string label = "";
            int num_args = 0;
            int num_locals = 0;
            std::vector<std::unique_ptr<Instruction>> instructions;

            Function() = default;
        
    };

    class Program : public ASTNode {
        public:
            Label label;
            std::vector<Function> functions;  // no polymorphism for now. 
            Program() = default;
            std::string to_string() override {
                std::string result;
                result += '(';
                result += label.to_string() + "\n";
                for (auto& function : functions) {
                    result += function.to_string() + "\n";
                }
                result += ')';
                return result;
            }
    };

}