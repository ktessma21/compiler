#include "lb.h"
#include "ast_leaves.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <stdexcept>
#include <cassert>
#include <cstdlib>
#include <stack>
#include <map>

namespace LB {

    struct LoopLabels {
        std::string condLabel;
        std::string bodyLabel;
        std::string exitLabel;
    };

    std::vector<std::unique_ptr<Instruction>> translate_while(
            std::unique_ptr<WhileInstruction> whileIns,
            Scope& scope,
            const std::string& condLabelName) {
        std::vector<std::unique_ptr<Instruction>> result;

        auto lbl = std::make_unique<LabelInstruction>();
        lbl->setLabel(Label(condLabelName));
        result.push_back(std::move(lbl));

        Variable var = Variable(freshName("cond"));
        scope.declaredTypes[var.name] = Type(VarType::Int64);
        scope.nameMap[var.name] = var.name;

        auto decl = std::make_unique<DeclInstruction>();
        decl->setType(Type(VarType::Int64));
        decl->setVar(var);
        result.push_back(std::move(decl));

        auto cmp = std::make_unique<OpInstruction>();
        cmp->setDst(var);
        cmp->setLhs(whileIns->getLhs().value());
        cmp->setOp(whileIns->getCmp().value());
        cmp->setRhs(whileIns->getRhs().value());
        result.push_back(std::move(cmp));

        std::string s = "\tbr " + var.name + " " +
                        whileIns->getBodyTarget().value().to_string() + " " +
                        whileIns->getExitTarget().value().to_string();
        result.push_back(std::make_unique<RawInstruction>(s));

        return result;
    }

    std::vector<std::unique_ptr<Instruction>> translate_if(
            std::unique_ptr<IfInstruction> ifIns,
            Scope& scope) {
        std::vector<std::unique_ptr<Instruction>> result;

        Variable var = Variable(freshName("cond"));
        scope.declaredTypes[var.name] = Type(VarType::Int64);
        scope.nameMap[var.name] = var.name;

        auto decl = std::make_unique<DeclInstruction>();
        decl->setType(Type(VarType::Int64));
        decl->setVar(var);
        result.push_back(std::move(decl));

        auto cmp = std::make_unique<OpInstruction>();
        cmp->setDst(var);
        cmp->setLhs(ifIns->getLhs().value());
        cmp->setOp(ifIns->getCmp().value());
        cmp->setRhs(ifIns->getRhs().value());
        result.push_back(std::move(cmp));

        std::string s = "\tbr " + var.name + " " +
                        ifIns->getTrueTarget().value().to_string() + " " +
                        ifIns->getFalseTarget().value().to_string();
        result.push_back(std::make_unique<RawInstruction>(s));

        return result;
    }

    std::vector<std::unique_ptr<Instruction>> translate_goto(
            std::unique_ptr<GotoInstruction> goIns) {
        std::vector<std::unique_ptr<Instruction>> result;

        std::string s = "\tbr " + goIns->getTarget().value().to_string();
        result.push_back(std::make_unique<RawInstruction>(s));

        return result;
    }

    std::vector<std::unique_ptr<Instruction>> translate_break(
            std::unique_ptr<BreakInstruction> brk,
            std::map<Instruction*, LoopLabels>& loopOf) {
        std::vector<std::unique_ptr<Instruction>> result;

        auto it = loopOf.find(brk.get());
        if (it == loopOf.end())
            throw std::runtime_error("translate_break: break not inside a loop");

        std::string s = "\tbr :" + it->second.exitLabel;
        result.push_back(std::make_unique<RawInstruction>(s));

        return result;
    }

    std::vector<std::unique_ptr<Instruction>> translate_continue(
            std::unique_ptr<ContinueInstruction> cont,
            std::map<Instruction*, LoopLabels>& loopOf) {
        std::vector<std::unique_ptr<Instruction>> result;

        auto it = loopOf.find(cont.get());
        if (it == loopOf.end())
            throw std::runtime_error("translate_continue: continue not inside a loop");

        std::string s = "\tbr :" + it->second.condLabel;
        result.push_back(std::make_unique<RawInstruction>(s));

        return result;
    }

    void assignCondLabels(Scope* scope,
                          std::map<WhileInstruction*, std::string>& whileCondLabels) {
        for (auto& item : scope->items) {
            if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                auto& ins = std::get<std::unique_ptr<Instruction>>(item);
                if (auto* w = dynamic_cast<WhileInstruction*>(ins.get())) {
                    whileCondLabels[w] = freshName("while_cond");
                }
            } else {
                auto& child = std::get<std::unique_ptr<Scope>>(item);
                assignCondLabels(child.get(), whileCondLabels);
            }
        }
    }

    // Pass 1: collect ALL while labels from entire scope tree first
    void collectWhileLabels(Scope* scope,
                            std::map<std::string, LoopLabels>& beginWhile,
                            std::map<std::string, LoopLabels>& endWhile,
                            std::map<WhileInstruction*, std::string>& whileCondLabels) {
        for (auto& item : scope->items) {
            if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                auto& ins = std::get<std::unique_ptr<Instruction>>(item);
                if (auto* w = dynamic_cast<WhileInstruction*>(ins.get())) {
                    std::string condLabel = whileCondLabels[w];
                    std::string bodyLabel = w->getBodyTarget().value().name;
                    std::string exitLabel = w->getExitTarget().value().name;
                    LoopLabels labels{condLabel, bodyLabel, exitLabel};
                    beginWhile[bodyLabel] = labels;
                    endWhile[exitLabel]   = labels;
                }
            } else {
                auto& child = std::get<std::unique_ptr<Scope>>(item);
                collectWhileLabels(child.get(), beginWhile, endWhile, whileCondLabels);
            }
        }
    }

    // Pass 2: walk instructions in order, using stack to assign enclosing loop
    void assignLoopMap(Scope* scope,
                       std::map<std::string, LoopLabels>& beginWhile,
                       std::map<std::string, LoopLabels>& endWhile,
                       std::map<Instruction*, LoopLabels>& loopOf,
                       std::stack<LoopLabels>& loopStack) {
        for (auto& item : scope->items) {
            if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                auto& ins = std::get<std::unique_ptr<Instruction>>(item);

                // assign current enclosing loop to this instruction
                if (!loopStack.empty()) {
                    loopOf[ins.get()] = loopStack.top();
                }

                if (auto* lbl = dynamic_cast<LabelInstruction*>(ins.get())) {
                    std::string name = lbl->getLabel().value().name;

                    if (beginWhile.count(name)) {
                        loopStack.push(beginWhile[name]);
                    } else if (endWhile.count(name)) {
                        if (!loopStack.empty() && loopStack.top().exitLabel == name) {
                            loopStack.pop();
                        }
                    }
                }

            } else {
                auto& child = std::get<std::unique_ptr<Scope>>(item);
                assignLoopMap(child.get(), beginWhile, endWhile, loopOf, loopStack);
            }
        }
    }

    void buildLoopMap(Scope* scope,
                      std::map<std::string, LoopLabels>& beginWhile,
                      std::map<std::string, LoopLabels>& endWhile,
                      std::map<Instruction*, LoopLabels>& loopOf,
                      std::stack<LoopLabels>& loopStack,
                      std::map<WhileInstruction*, std::string>& whileCondLabels) {
        // pass 1: collect all while labels before any stack logic
        collectWhileLabels(scope, beginWhile, endWhile, whileCondLabels);
        // pass 2: assign enclosing loop to each instruction
        assignLoopMap(scope, beginWhile, endWhile, loopOf, loopStack);
    }

    void translateScope(Scope* scope,
                        std::vector<std::unique_ptr<Instruction>>& out,
                        std::map<Instruction*, LoopLabels>& loopOf,
                        std::map<WhileInstruction*, std::string>& whileCondLabels) {

        for (auto& item : scope->items) {
            if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                auto& ins = std::get<std::unique_ptr<Instruction>>(item);

                if (auto* w = dynamic_cast<WhileInstruction*>(ins.get())) {
                    std::string condLabel = whileCondLabels[w];
                    auto translated = translate_while(
                        std::unique_ptr<WhileInstruction>(static_cast<WhileInstruction*>(ins.release())),
                        *scope,
                        condLabel);
                    for (auto& i : translated) out.push_back(std::move(i));

                } else if (auto* i = dynamic_cast<IfInstruction*>(ins.get())) {
                    auto translated = translate_if(
                        std::unique_ptr<IfInstruction>(static_cast<IfInstruction*>(ins.release())),
                        *scope);
                    for (auto& i : translated) out.push_back(std::move(i));

                } else if (auto* g = dynamic_cast<GotoInstruction*>(ins.get())) {
                    auto translated = translate_goto(
                        std::unique_ptr<GotoInstruction>(static_cast<GotoInstruction*>(ins.release())));
                    for (auto& i : translated) out.push_back(std::move(i));

                } else if (auto* b = dynamic_cast<BreakInstruction*>(ins.get())) {
                    auto translated = translate_break(
                        std::unique_ptr<BreakInstruction>(static_cast<BreakInstruction*>(ins.release())),
                        loopOf);
                    for (auto& i : translated) out.push_back(std::move(i));

                } else if (auto* c = dynamic_cast<ContinueInstruction*>(ins.get())) {
                    auto translated = translate_continue(
                        std::unique_ptr<ContinueInstruction>(static_cast<ContinueInstruction*>(ins.release())),
                        loopOf);
                    for (auto& i : translated) out.push_back(std::move(i));

                } else {
                    out.push_back(std::move(ins));
                }

            } else {
                auto& child = std::get<std::unique_ptr<Scope>>(item);
                translateScope(child.get(), out, loopOf, whileCondLabels);
            }
        }
    }

    void translate_statements(Program& p) {
        for (auto& f : p.functions) {
            if (!f.rootScope) continue;

            std::map<WhileInstruction*, std::string> whileCondLabels;
            assignCondLabels(f.rootScope.get(), whileCondLabels);

            std::map<std::string, LoopLabels> beginWhile;
            std::map<std::string, LoopLabels> endWhile;
            std::map<Instruction*, LoopLabels> loopOf;
            std::stack<LoopLabels> loopStack;
            buildLoopMap(f.rootScope.get(), beginWhile, endWhile, loopOf, loopStack, whileCondLabels);

            std::vector<std::unique_ptr<Instruction>> flat;
            translateScope(f.rootScope.get(), flat, loopOf, whileCondLabels);

            f.rootScope->items.clear();
            for (auto& ins : flat) {
                f.rootScope->add(std::move(ins));
            }
        }
    }
}