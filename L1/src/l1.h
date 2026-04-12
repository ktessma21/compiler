
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


    // label is just another string. 
    using Label = std::string;

    class Instruction : public ASTNode {
        public:
            virtual ~Instruction() = default;
            std::string to_string() override { return ""; }
    };

    class AssignInstruction : public Instruction {};
    class PrintInstruction  : public Instruction {};
    class ReturnInstruction : public Instruction {};
    class InputInstruction  : public Instruction {};

     // create a type name called NumPoi : could be Number or Pointer
    using NumPoi  = std::variant<Number, Pointer>;  
    
    class Function : public ASTNode {
        Label label = "";
        
        public:
            
            std::vector<std::unique_ptr<Instruction>> instructions;
            int num_args = 0;
            int num_locals = 0;
            bool args_set = false;
            bool local_set = false;

            Function() = default;
            std::string to_string() override {
                std::string result;
                result += '(' + label + '\n';
                result += std::to_string(num_args) + ' ' + std::to_string(num_locals) + '\n';
                for (auto& instruction : instructions) {
                    result += instruction->to_string();  // -> and semicolon
                }
                result += ')';
                return result;  // semicolon
            }

            // getters
            std::string getLabel()   const { return label; }
            int getNumArgs()         const { return num_args; }
            int getNumLocals()       const { return num_locals; }

            // setters
            void setLabel(std::string l)  { label = l; }
            void setNumArgs(int n)        { args_set = true; num_args = n; }
            void setNumLocals(int n)      { local_set = true; num_locals = n; }


        
    };

    // Note label[0] is always the head. 

    class Program : public ASTNode {
        public:
            Label label;
            std::vector<Function> functions;

            Program() = default;
            std::string to_string() override {
                std::string result;
                result += '(';
                result += label + "\n";
                for (auto& function: functions){
                    result += function.to_string();
                }
                result += ')';
                return result;
            }
    };

}