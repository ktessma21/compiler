#pragma once
#include "tree.h"
#include "l3.h"
#include <vector>
#include <string>

namespace L3 {


    struct Tile {
        virtual ~Tile() = default;
        virtual int cost() const = 0;
        virtual bool match(const TreeNode& node) const = 0;
        // transforms the matched subtree into an L2 instruction
        virtual std::unique_ptr<Instruction> emit(const TreeNode& node) const = 0;
    };


    struct StoreTile : Tile {
        int cost() override {
            return 1;
        }
    }

    



}