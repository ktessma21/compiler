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

   

    void Liveness(Function& f, std::vector<LiveSet>& InSet, std::vector<LiveSet>& OutSet) {
        L2::SC sc;
        sc.build(f);

        InSet.assign(f.instructions.size(), LiveSet{});
        OutSet.assign(f.instructions.size(), LiveSet{});

        bool keep_going = true;
        while (keep_going) {
            keep_going = false;

            for (int i = (int)f.instructions.size() - 1; i >= 0; i--) {
                LiveSet liveOut;
                for (size_t s : sc.successors[i]) {
                    liveOut.insert(InSet[s].begin(), InSet[s].end());
                }

                LiveSet liveIn = liveOut;
                for (const auto& w : f.instructions[i]->writesLive()) liveIn.erase(w);
                for (const auto& r : f.instructions[i]->readsLive())  liveIn.insert(r);

                if (liveIn != InSet[i] || liveOut != OutSet[i]) {
                    InSet[i]  = std::move(liveIn);
                    OutSet[i] = std::move(liveOut);
                    keep_going = true;
                }
            }
        }
    }

    void LivenessPrint(Function& f) {

        
        std::vector<LiveSet> in;
        std::vector<LiveSet> out;

        Liveness(f, in, out);


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