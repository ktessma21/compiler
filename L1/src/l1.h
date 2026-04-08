
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace L1 {
    class ASTNode {
        // public:
        //     virtual 
    };


    
    // this could be a variant or I mean a UNION
    class Number : public ASTNode {
        uint64_t value;
        public:
            Number(uint64_t _value) : value(_value) { };


    };

    class Pointer : public ASTNode {
        uint64_t value;
        public:
            Pointer(uint64_t _value) : value(_value) { };
            
    };
    
    class Register : public ASTNode {
        std::string value;
        public:
            Register(std::string _value) : value(_value) {};
    };


    
   

    class Label : public ASTNode {
        std::string value;
        public:
            Label(std::string _value) : value(_value) {};
    }; 

    class Instruction : public ASTNode {

    };

    class AssignInstruction : public Instruction {

    };

    class PrintInstruction : public Instruction {

    };

    class ReturnInstruction : public Instruction {

    };
    
    class InputInstruction : public Instruction {

    };

     // create a type name called NumPoi : could be Number or Pointer
    using NumPoi  = std::variant<Number, Pointer>;  
    
    class Function : public ASTNode {
        std::vector<NumPoi> input_vals;
        int num_args;
        int num_locals;
        std::vector<std::unique_ptr<Instruction>> instructions;
        
        public:
            Function();
    };

    class Program : public ASTNode {
        Label label;
        std::map<Label, Function> functions;  // no polymorphism for now. 
        
        
    };

}