// #include <l2.h>
// #include <stdio.h>
// #include <vector>
// #include <string>
// #include <cstddef>
// #include <interference.h>
// #include <coloring.h>
// #include <map>
// #include <set>
// #include <cassert>
// #include <spiller.h>
// #include <iostream>

// namespace L2 {

//     static constexpr bool DBG = false;

//     static void spillallvars(Function& f) {
//         int idx = 0;
//         std::set<std::string> all_vars;
//         for (const auto& instr : f.instructions) {
//             for (const auto& v : instr->reads())  all_vars.insert(v.name);
//             for (const auto& v : instr->writes()) all_vars.insert(v.name);
//         }

//         if (DBG) {
//             std::cerr << "\n[spillall] function " << f.getLabel()
//                       << " has " << all_vars.size() << " distinct variables to spill:\n";
//             for (const auto& n : all_vars) std::cerr << "  " << n << "\n";
//         }

//         if (all_vars.empty()) return;

//         std::string prefix = "%S";
//         bool collision = true;
//         while (collision) {
//             collision = false;
//             for (const auto& name : all_vars) {
//                 if (name.rfind(prefix, 0) == 0) { collision = true; break; }
//             }
//             if (collision) prefix += "S";
//         }

//         if (DBG) std::cerr << "[spillall] using prefix \"" << prefix << "\"\n";

//         for (const auto& name : all_vars) {
//             std::string this_prefix = prefix + std::to_string(idx) + "_";
//             if (DBG) std::cerr << "[spillall]   spilling " << name
//                                << "  (prefix=" << this_prefix << ", slot=" << idx << ")\n";
//             (void)L2::Spill(f, name, this_prefix, idx);
//             idx++;
//         }

//         if (DBG) {
//             std::cerr << "[spillall] post-spill instruction dump for " << f.getLabel() << ":\n";
//             for (const auto& instr : f.instructions) {
//                 std::cerr << "    " << instr->to_string();
//             }
//         }
//     }

//     static void RegisterAllocator(L2::Graph& g, Function& f) {
//     constexpr int K = 15;

//     // Pre-color physical registers, keyed by name to dodge any VALUEComparator weirdness.
//     std::map<std::string, int> name_to_color;
//     for (auto reg : Graph::allRegs) {
//         if (reg == Register::rsp) continue;
//         name_to_color[registerToString(reg)] = static_cast<int>(reg);
//     }

//     std::set<Variable> all_vars;
//     for (const auto& instr : f.instructions) {
//         for (const auto& v : instr->reads())  all_vars.insert(v);
//         for (const auto& v : instr->writes()) all_vars.insert(v);
//     }

//     if (DBG) {
//         std::cerr << "\n[ralloc] function " << f.getLabel()
//                   << "  vars-to-color=" << all_vars.size()
//                   << "  graph-nodes=" << g.graph.size() << "\n";
//     }

//     for (const auto& var : all_vars) {
//         if (name_to_color.count(var.name)) {
//             if (DBG) std::cerr << "[ralloc]   skip (already colored): " << var.name << "\n";
//             continue;
//         }

//         // Build the used-color set from neighbors in the graph.
//         std::set<int> used;
//         auto it = g.graph.find(VALUE(var));
//         if (it != g.graph.end()) {
//             for (const auto& nb : it->second) {
//                 std::string nb_name;
//                 if (std::holds_alternative<Variable>(nb)) {
//                     nb_name = std::get<Variable>(nb).name;
//                 } else if (std::holds_alternative<Register>(nb)) {
//                     nb_name = registerToString(std::get<Register>(nb));
//                 } else {
//                     continue;  // mem/label/number can't be neighbors of interest
//                 }
//                 auto cit = name_to_color.find(nb_name);
//                 if (cit != name_to_color.end()) used.insert(cit->second);
//             }
//         }

//         int chosen = -1;
//         for (int c = 0; c < K; ++c) {
//             if (!used.count(c)) { chosen = c; break; }
//         }

//         if (DBG) {
//             std::cerr << "[ralloc]   color " << var.name << " -> " << chosen
//                       << " (" << valueToString(VALUE(static_cast<L2::Register>(chosen)))
//                       << ")  used={";
//             bool first = true;
//             for (int c : used) { if (!first) std::cerr << ","; std::cerr << c; first = false; }
//             std::cerr << "}\n";
//         }

//         assert(chosen != -1 && "RegisterAllocator: no free color after full spill");
//         name_to_color[var.name] = chosen;

//         VALUE replacement = VALUE(static_cast<L2::Register>(chosen));
//         int touched = 0;
//         for (const auto& instr : f.instructions) {
//             std::string before = instr->to_string();
//             instr->replaceVar(var, replacement);
//             if (instr->to_string() != before) touched++;
//         }
//         if (DBG) std::cerr << "[ralloc]     rewrote " << touched << " instructions\n";
//     }

//     if (DBG) {
//         std::cerr << "[ralloc] post-rewrite scan for " << f.getLabel() << ":\n";
//         int leaks = 0;
//         for (const auto& instr : f.instructions) {
//             std::string s = instr->to_string();
//             if (s.find('%') != std::string::npos) {
//                 std::cerr << "    LEAK: " << s;
//                 leaks++;
//             }
//         }
//         if (leaks == 0) std::cerr << "    clean — no '%' variables remain.\n";
//         else            std::cerr << "    *** " << leaks << " instructions still contain '%' ***\n";
//     }
// }

//     void GraphColoring(L2::Graph& g, Function& f, int idx) {
//         if (DBG) std::cerr << "\n========== GraphColoring: " << f.getLabel() << " ==========\n";

//         spillallvars(f);
//         g = L2::Interference(f);

//         if (DBG) {
//             std::cerr << "\n[GC] interference graph after spill (" << g.graph.size() << " nodes):\n";
//             g.printItems();
//         }

//         RegisterAllocator(g, f);

//         if (DBG) std::cerr << "========== GraphColoring done: " << f.getLabel() << " ==========\n\n";
//     }

// }

    

//     // enum class Tag { COLOR, SPILL };

//     // static constexpr bool TRACE = false;  // <-- flip to false to silence

//     // void GraphColoring(L2::Graph& g, Function& f, int idx){

//     //     if (TRACE) {
//     //         std::cerr << "\n=== GraphColoring entry: function=" << f.getLabel()
//     //                   << " idx=" << idx << " ===\n";
//     //         std::cerr << "[trace] graph has " << g.graph.size() << " nodes\n";
//     //         int var_count = 0;
//     //         for (const auto& [v, _] : g.graph) {
//     //             if (std::holds_alternative<Variable>(v)) var_count++;
//     //         }
//     //         std::cerr << "[trace] variable nodes: " << var_count << "\n";
//     //     }

//     //     L2::LiveSet spill;
//     //     // tuple: (node, its neighbors at push time, tag)
//     //     std::vector<std::tuple<VALUE, LiveSet, Tag>> stack;
//     //     int color_number = 0;

//     //     constexpr int K = 15;

//     //     // ---------- Step 2: simplify ----------
//     //     // Repeat: pick a node with degree < K and push as COLOR; otherwise pick
//     //     // a spill candidate (highest-degree variable) and push as SPILL.
//     //     while (true) {
//     //         auto nd = g.node_with_less_than_15_neighbors();   // CHECK: will my code becomes better if I pick a node with closer to 15 neighbors? 

//     //         if (nd.has_value()) {
//     //             assert(std::holds_alternative<Variable>(*nd));
//     //             if (TRACE) {
//     //                 std::cerr << "[simplify] push COLOR  " << valueToString(*nd)
//     //                           << " (degree=" << g.graph[*nd].size() << ")\n";
//     //             }
//     //             stack.emplace_back(*nd, g.graph[*nd], Tag::COLOR);
//     //             g.removeNode(*nd);
//     //             continue;
//     //         }
//     //         // No low-degree variable — pick a spill candidate.
//     //         std::optional<VALUE> spill_candidate;
//     //         size_t max_deg = 0;
//     //         for (const auto& [v, neighbors] : g.graph) {
//     //             if (!std::holds_alternative<Variable>(v)) continue;
//     //             if (neighbors.size() >= max_deg) {
//     //                 max_deg = neighbors.size();
//     //                 spill_candidate = v;
//     //             }
//     //         }

//     //         if (!spill_candidate.has_value()) {
//     //             if (TRACE) std::cerr << "[simplify] no more variables — done\n";
//     //             break;  // only registers left
//     //         }

//     //         if (TRACE) {
//     //             std::cerr << "[simplify] push SPILL  " << valueToString(*spill_candidate)
//     //                       << " (degree=" << max_deg << ")\n";
//     //         }
//     //         stack.emplace_back(*spill_candidate,
//     //                            g.graph[*spill_candidate],
//     //                            Tag::SPILL);
//     //         g.removeNode(*spill_candidate);
//     //     }

//     //     // ---------- Step 3: select / assign colors ----------
//     //     std::map<VALUE, int, VALUEComparator> value_to_color;

//     //     // Pre-color physical registers with their canonical indices.
//     //     for (auto reg : Graph::allRegs) {
//     //         if (reg == Register::rsp) continue;
//     //         value_to_color[VALUE(reg)] = static_cast<int>(reg);  // the color number in register is an enum . 
//     //     }

//     //     if (TRACE) std::cerr << "[select] popping stack of size " << stack.size() << "\n";

//     //     while (!stack.empty()) {
//     //         auto [node, edges, tag] = stack.back();
//     //         stack.pop_back();

//     //         // Colors used by already-colored neighbors.
//     //         std::set<int> used;
//     //         for (const auto& nb : edges) {
//     //             auto it = value_to_color.find(nb);
//     //             if (it != value_to_color.end()) used.insert(it->second);
//     //         }

//     //         if (TRACE) {
//     //             std::cerr << "[select] pop  " << valueToString(node)
//     //                       << "  tag=" << (tag == Tag::COLOR ? "COLOR" : "SPILL")
//     //                       << "  used_colors={";
//     //             bool first = true;
//     //             for (int c : used) {
//     //                 if (!first) std::cerr << ",";
//     //                 std::cerr << c;
//     //                 first = false;
//     //             }
//     //             std::cerr << "}\n";
//     //         }

//     //         if (tag == Tag::COLOR) {
//     //             // Definitely colorable — pick the smallest free color.- direction to which register to pick. 
//     //             for (int c = 0; c < K; ++c) {
//     //                 if (!used.count(c)) {
//     //                     value_to_color[node] = c;
//     //                     if (TRACE) {
//     //                         std::cerr << "[select]   -> assigned color " << c
//     //                                   << " ("
//     //                                   << valueToString(VALUE(static_cast<L2::Register>(c)))
//     //                                   << ")\n";
//     //                     }

//     //                     // color them
//     //                     for (const auto& instr : f.instructions) {
//     //                         assert(std::holds_alternative<Variable>(node));
//     //                         instr->replaceVar(std::get<Variable>(node), VALUE(static_cast<L2::Register>(c)));
//     //                     }
//     //                     break;
//     //                 }
//     //             }
//     //         } else {
//     //             // Optimistic coloring: a SPILL-tagged node may still be colorable
//     //             // if its neighbors didn't actually consume all K colors.
//     //             if (used.size() < static_cast<size_t>(K)) {
//     //                 for (int c = 0; c < K; ++c) {
//     //                     if (!used.count(c)) {
//     //                         value_to_color[node] = c;
//     //                         if (TRACE) {
//     //                             std::cerr << "[select]   -> optimistic assigned color " << c
//     //                                       << " ("
//     //                                       << valueToString(VALUE(static_cast<L2::Register>(c)))
//     //                                       << ")  [NOT applied to instructions!]\n";
//     //                         }
//     //                         break;
//     //                     }
//     //                 }
//     //             } else {
//     //                 // Real spill.
//     //                 assert(std::holds_alternative<Variable>(node));
//     //                 if (TRACE) {
//     //                     std::cerr << "[select]   -> SPILL " << valueToString(node) << "\n";
//     //                 }
//     //                 spill.insert(node);
//     //             }
//     //         }
//     //     }

//     //     if (TRACE) {
//     //         std::cerr << "[summary] spill set size = " << spill.size() << "\n";
//     //         for (const auto& v : spill) {
//     //             std::cerr << "[summary]   spilled: " << valueToString(v) << "\n";
//     //         }
//     //     }


//     //     // if spill is non-empty then spilling is required. 
//     //     if (!spill.empty()) {
//     //         // Find a prefix that doesn't collide with any existing variable in f.
//     //         std::string prefix = "%S";
//     //         bool collision = true;
//     //         while (collision) {
//     //             collision = false;
//     //             for (const auto& instr : f.instructions) {
//     //                 for (const auto& v : instr->reads()) {
//     //                     const auto& name = v.name;
//     //                     if (name.rfind(prefix, 0) == 0) {  // starts with prefix
//     //                         collision = true;
//     //                         break;
//     //                     }
//     //                 }
//     //                 if (collision) break;
//     //                 // (also check writes() the same way)
//     //             }
//     //             if (collision) prefix += "S";
//     //         }

//     //         if (TRACE) std::cerr << "[spill] using prefix \"" << prefix << "\"\n";

//     //         // Now spill each variable. Use a unique sub-prefix per variable so
//     //         // the counters don't collide across calls.
            
//     //         for (const auto& var : spill) {
//     //             if (!std::holds_alternative<Variable>(var)) continue;
//     //             const auto& name = std::get<Variable>(var).name;
//     //             std::string this_prefix = prefix + std::to_string(idx) + "_";
//     //             if (TRACE) {
//     //                 std::cerr << "[spill] rewriting " << name
//     //                           << " with prefix " << this_prefix
//     //                           << " idx=" << idx << "\n";
//     //             }
//     //             (void)L2::Spill(f, name, this_prefix, idx);
//     //             idx++;
//     //             // In the spill loop, after each Spill call:
//     //                 if (TRACE) {
//     //                     std::cerr << "[spill] post-rewrite, function instructions referencing "
//     //                             << name << " or new prefix:\n";
//     //                     for (const auto& instr : f.instructions) {
//     //                         std::string s = instr->to_string();
//     //                         if (s.find(this_prefix) != std::string::npos) {
//     //                             std::cerr << "  " << s;
//     //                         }
//     //                     }
//     //                 }
//     //         }

//     //         if (TRACE) std::cerr << "[spill] rebuilding interference graph and recursing (idx now=" << idx << ")\n";

//     //         g = L2::Interference(f);
//     //         return L2::GraphColoring(g, f, idx);

//     //     }else{

//     //         if (TRACE) std::cerr << "=== GraphColoring done for " << f.getLabel() << " ===\n";
//     //         // // we are done 
//     //         // for (const auto& l : value_to_color) {
//     //         //     std::cout << "[" << valueToString(l.first) << "] = " << l.second << std::endl;
//     //         // }
//     //         return;
//     //     }
       
//     // }

//     // void GraphColoring(const L2::Graph& g, Function& f){
//     //     std::map<VALUE, int> value_to_color;

//     //     int num = 0;
//     //     for (auto reg : g.allRegs){
//     //         value_to_color[VALUE(reg)] = num++;
//     //     } // use the enum for the mapping 
        
//     //     bool fail_to_color = false;
        
//     //     for (const auto& node : g.graph){
               
//     //             // only coloring required if we have a variable. if it is only registers we can't optimize it here. 
//     //         if (std::holds_alternative<Variable>(node.first)){
//     //             // if i didn't color it yet
//     //             if (value_to_color.find(node.first) == value_to_color.end()){
//     //                 // find color for it. 
//     //                 std::set color_option = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

//     //                 for (const auto& neighbor : node.second){
//     //                     if (value_to_color.find(neighbor) == value_to_color.end()) continue;  // if not colored continue
//     //                     color_option.erase(value_to_color[neighbor]);
//     //                 }
                    
//     //                 if (color_option.size() <= 0){
//     //                     fail_to_color = true; 
//     //                     break;
//     //                 }
//     //                 // otherwise just color it with the first number you find. 
//     //                 value_to_color[node.first] = *color_option.begin();
//     //             }
                
//     //         }
//     //     }

//     //     if (fail_to_color){
//     //         // for now spill all the remaining nodes here -- no heuristic for now 
//     //         throw std::runtime_error("can't color well");
//     //     }

//     //     for (auto l : value_to_color){
//     //         std::cout << "[" << valueToString(l.first) << "] = " << l.second << std::endl;
//     //     }
        
        
//     //     return;


//     // }

