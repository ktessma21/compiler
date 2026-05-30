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

namespace LB {



    static bool debug(){
        if (std::getenv("LA_DEBUG") != nullptr) {
            return true;
        }
        return false;
    }

    int freshCounter = 0;  

    std::string freshName(const std::string& original) {
        return original + "_" + std::to_string(freshCounter++);
    }

    void collectMappings(Scope* scope, Function& f) {
        for (auto& item : scope->items) {
            if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                auto& ins = std::get<std::unique_ptr<Instruction>>(item);

                if (auto* decl = dynamic_cast<DeclInstruction*>(ins.get())) {
                    std::string oldName = decl->getVar()->name;
                    std::string newName = freshName(oldName);
                    scope->nameMap[oldName] = newName;
                }

            } else {
                auto& child = std::get<std::unique_ptr<Scope>>(item);
                collectMappings(child.get(), f);
            }
        }
    }

    void collectMappings(Function& f) {
        if (!f.rootScope) return;

        // parameters live in root scope
        for (auto& param : f.getParams()) {
            std::string oldName = param.name;
            std::string newName = freshName(oldName);
            f.rootScope->nameMap[oldName] = newName;
        }

        collectMappings(f.rootScope.get(), f);
    }

    std::string lookupRename(Scope* scope, const std::string& name) {
        Scope* s = scope;
        while (s) {
            auto it = s->nameMap.find(name);
            if (it != s->nameMap.end()) return it->second;
            s = s->parent;
        }
        return name; // not in any scope's map = function name or number, leave as is
    }

    void renameUsesInInstruction(Instruction* ins, Scope* scope) {
        auto renameT = [&](T& t) {
            if (auto* v = std::get_if<Variable>(&t)) {
                v->name = lookupRename(scope, v->name);
            }
        };

        if (auto* i = dynamic_cast<AssignInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            if (i->getSrc()) { T s = *i->getSrc(); renameT(s); i->setSrc(s); }

        } else if (auto* i = dynamic_cast<OpInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            T l = *i->getLhs(); renameT(l); i->setLhs(l);
            T r = *i->getRhs(); renameT(r); i->setRhs(r);

        } else if (auto* i = dynamic_cast<ArrayLoadInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            Variable src = *i->getSrc();
            src.name = lookupRename(scope, src.name);
            i->setSrc(src);
            for (auto& ix : i->getIndicesMut()) renameT(ix);  
        } else if (auto* i = dynamic_cast<ArrayStoreInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            
            for (auto& ix : i->getIndicesMut()) renameT(ix);

            if (i->getSrc()) { 
                T s = *i->getSrc(); 
                renameT(s); 
                i->setSrc(s); 
            }
            if (i->getSrcCallee()) {
                if (auto* cv = std::get_if<Variable>(&(*i->getSrcCallee()))) {
                    Variable newCallee = *cv;
                    newCallee.name = lookupRename(scope, newCallee.name);
                    i->setSrcCallee(newCallee);
                }
            }
        } else if (auto* i = dynamic_cast<LengthInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            Variable arr = *i->getArray();
            arr.name = lookupRename(scope, arr.name);
            i->setArray(arr);
            if (i->getDim()) { T d = *i->getDim(); renameT(d); i->setDim(d); }

        } else if (auto* i = dynamic_cast<NewArrayInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            for (auto& arg : i->getArgs()) renameT(arg);

        } else if (auto* i = dynamic_cast<NewTupleInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);
            if (i->getSize()) { T s = *i->getSize(); renameT(s); i->setSize(s); }

        } else if (auto* i = dynamic_cast<VarCallInstruction*>(ins)) {
            Variable dst = *i->getDst();
            dst.name = lookupRename(scope, dst.name);
            i->setDst(dst);

            // rename callee if it's a Variable (code type)
            if (auto* cv = std::get_if<Variable>(&(*i->getCallee()))) {
                Variable newCallee = *cv;
                newCallee.name = lookupRename(scope, newCallee.name);
                i->setCallee(newCallee);
            }

            for (auto& arg : i->getArgs()) renameT(arg);
        } else if (auto* i = dynamic_cast<CallInstruction*>(ins)) {
            // rename callee if it's a Variable (code type)
            if (auto* cv = std::get_if<Variable>(&(*i->getCallee()))) {
                Variable newCallee = *cv;
                newCallee.name = lookupRename(scope, newCallee.name);
                i->setCallee(newCallee);
            }

            for (auto& arg : i->getArgs()) renameT(arg);
        }else if (auto* i = dynamic_cast<ReturnTInstruction*>(ins)) {
            T val = *i->getValue(); renameT(val); i->setValue(val);

        } else if (auto* i = dynamic_cast<IfInstruction*>(ins)) {
            T l = *i->getLhs(); renameT(l); i->setLhs(l);
            T r = *i->getRhs(); renameT(r); i->setRhs(r);

        } else if (auto* i = dynamic_cast<WhileInstruction*>(ins)) {
            T l = *i->getLhs(); renameT(l); i->setLhs(l);
            T r = *i->getRhs(); renameT(r); i->setRhs(r);

        } else if (auto* i = dynamic_cast<DeclInstruction*>(ins)) {
            // rename the decl itself using its own scope's nameMap
            std::string oldName = i->getVar()->name;
            auto it = scope->nameMap.find(oldName);
            if (it != scope->nameMap.end()) {
                Variable v = *i->getVar();
                v.name = it->second;
                i->setVar(v);
            }
        }
        // Return, Goto, Label, Break, Continue — no variables
    }

    void applyRenames(Scope* scope) {
        for (auto& item : scope->items) {
            if (std::holds_alternative<std::unique_ptr<Instruction>>(item)) {
                auto& ins = std::get<std::unique_ptr<Instruction>>(item);
                renameUsesInInstruction(ins.get(), scope);
            } else {
                auto& child = std::get<std::unique_ptr<Scope>>(item);
                applyRenames(child.get());
            }
        }
    }

    void applyRenames(Function& f) {
        if (!f.rootScope) return;

        // rename parameter names in the function signature
        for (auto& param : f.getParams()) {  // now non-const ref
            auto it = f.rootScope->nameMap.find(param.name);
            if (it != f.rootScope->nameMap.end()) {
                param.name = it->second;
            }
        }

        applyRenames(f.rootScope.get());
    }

    void organize_functions(Program& p) {
        for (auto& f : p.functions) {
            collectMappings(f);
            applyRenames(f); 
        }
    }
};