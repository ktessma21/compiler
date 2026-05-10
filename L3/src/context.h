#pragma once
#include <vector>
#include <memory>
#include <set>


namespace L3 {

    class Context {
        public:
            std::vector<std::unique_ptr<Instruction>> instructions;
            std::vector<std::unique_ptr<TreeNode>> trees;
            std::vector<LivenessInfo> liveAnalysisReport;
            
            Context() = default;

            void add(std::unique_ptr<Instruction> instr);
            void add(std::vector<LivenessInfo>::iterator begin, 
                     std::vector<LivenessInfo>::iterator end);

            const std::vector<std::unique_ptr<Instruction>>& get() const;

            bool empty() const;
            size_t size() const;
            bool is_terminated() const;

            void print_trees(bool debug = false) const;
            void build_tree();
            void merge_tree();
            void aggregate_tree();

           
    };

}