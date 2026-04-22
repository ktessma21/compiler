#include "l2.h"
#include <fstream>
#include <iostream>
#include <utils.h>
#include "spiller.h"
#include <stdexcept>
#include <liveness.h>
#include <set>
#include <vector>
// #include <map>

namespace L2 { 
    // std::vector<std::vector<size_t>> successors;

   struct LiveCompare {
    bool operator()(const VALUE& a, const VALUE& b) const {
            auto key = [](const VALUE& v) -> std::string {
                std::string s;
                if (std::holds_alternative<Variable>(v)) {
                    // Drop the leading '%' so "%val_to_test" compares after "r15"
                    s = std::get<Variable>(v).name.substr(1);
                }else{
                    s = valueToString(v);
                }
                // Lowercase for case-insensitive comparison because of test 1
                for (char& c : s) c = std::tolower((unsigned char)c);
                return s;
            };
            return key(a) < key(b);
        }
    };

    using LiveSet = std::set<VALUE, LiveCompare>;

    void Liveness(Function& f) {
        L2::SC sc;
        sc.build(f);

        std::vector<LiveSet> in(f.instructions.size());
        std::vector<LiveSet> out(f.instructions.size());

        bool keep_going = true;
        while (keep_going) {
            keep_going = false;

            for (int i = (int)f.instructions.size() - 1; i >= 0; i--) {
                LiveSet liveOut;
                for (size_t s : sc.successors[i]) {
                    liveOut.insert(in[s].begin(), in[s].end());
                }

                LiveSet liveIn = liveOut;
                for (const auto& w : f.instructions[i]->writesLive()) liveIn.erase(w);
                for (const auto& r : f.instructions[i]->readsLive())  liveIn.insert(r);

                if (liveIn != in[i] || liveOut != out[i]) {
                    in[i]  = std::move(liveIn);
                    out[i] = std::move(liveOut);
                    keep_going = true;
                }
            }
            }
        

        std::cout << '(' << '\n';
        std::cout << "(in" << '\n';
        for (auto& ele : in) {
            std::cout << '(';
            bool first = true;
            for (auto& val : ele) {
                if (!first) std::cout << ' ';
                std::cout << valueToString(val);
                first = false;
            }
            std::cout << ")\n";
        }
        std::cout << ")\n\n";

        std::cout << "(out" << '\n';

        for (auto& ele : out){
            std::cout << '(';
            bool first = true;
            for (auto& val : ele) {
                if (!first) std::cout << ' ';
                std::cout << valueToString(val);
                first = false;
            }
            std::cout << ")\n";
          
        }
        std::cout << ")\n\n";
        std::cout << ')' << '\n';

    }

    


};