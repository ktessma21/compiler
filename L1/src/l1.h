
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
// #include <vector>

namespace L1 {
    class ASTNode {
        // public:
        //     virtual 
    };

    class Program : public ASTNode {
        Label label;
        std::map<Label, Function> functions;  // high likely for this to be unique_ptr
        
        
    };

    class Function : public ASTNode {
        
    };

    class Label : public ASTNode {
        std::string value;
        public:
            Label(std::string _value) : value(_value) {};
    }; 

    




};
