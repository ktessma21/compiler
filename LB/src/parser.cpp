#include <fstream>
#include "lb.h"
#include "parser.h"
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <cassert>


namespace pegtl = TAO_PEGTL_NAMESPACE;

static bool TRACE = false;

#define pstring TAO_PEGTL_STRING

using namespace pegtl;


namespace LB {

    // struct scope; // forward declaration

  /* ===== shared lexical rules ===== */
  struct name:
    pegtl::seq<
      pegtl::sor<
        pegtl::alpha,
        pegtl::one< '_' >
      >,
      pegtl::star<
        pegtl::sor<
          pegtl::alpha,
          pegtl::one< '_' >,
          pegtl::digit
        >
      >
    > {};

    struct var :
            name {};

  struct number:
    pegtl::seq<
      pegtl::opt<
        pegtl::sor<
          pegtl::one< '-' >,
          pegtl::one< '+' >
        >
      >,
      pegtl::plus<
        pegtl::digit
      >
    >{};


    struct comment:
        pegtl::disable<
        pstring( "//" ),
        pegtl::until< pegtl::eolf >
         >{};

    struct spaces :
        pegtl::star<
            pegtl::sor<
                pegtl::one< ' ' >,
                pegtl::one< '\t'>
            >
        > { };

    struct seps :
        pegtl::star<
            pegtl::seq<
                spaces,
                pegtl::eol
            >
        > { };

    struct seps_with_comments :
        pegtl::star<
            pegtl::seq<
                spaces,
                pegtl::sor<
                    pegtl::eol,
                    comment
                >
            >
        > { };


    /* ===== LB grammar ===== */

    // label ::= :name
    struct label :
        pegtl::seq<pegtl::one<':'>, name> {};

    struct N : number {};

    // t ::= name | N
    struct t :
        pegtl::sor<N, var> {};

    // type ::= int64([])* | tuple | code
    struct type :
        pegtl::sor<
            pegtl::seq<
                pstring("int64"),
                pegtl::star<pegtl::seq<pegtl::one<'['>, pegtl::one<']'>>>
            >,
            pstring("tuple"),
            pstring("code")
        > {};

    // T ::= type | void
    struct Ty :
        pegtl::sor<
            type,
            pstring("void")
        > {};

    // op = arithmetic only (cmp is a separate non-terminal in LB).
    // Keyword-safe ordering: longer first.
    struct arith_op :
        pegtl::sor<
            pstring("<<"),
            pstring(">>"),
            pegtl::one<'+'>,
            pegtl::one<'-'>,
            pegtl::one<'*'>,
            pegtl::one<'&'>
        > {};

    // cmp ::= < | <= | = | >= | >
    struct cmp_op :
        pegtl::sor<
            pstring("<="),
            pstring(">="),
            pegtl::one<'<'>,
            pegtl::one<'>'>,
            pegtl::one<'='>
        > {};

    // op ::= arith | cmp   (used in name <- t op t)
    struct op :
        pegtl::sor<arith_op, cmp_op> {};


    /* ---- keyword guards: keywords must not be followed by a name char ---- */
    struct kw_return :
        pegtl::seq< pstring("return"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_if :
        pegtl::seq< pstring("if"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_goto :
        pegtl::seq< pstring("goto"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_while :
        pegtl::seq< pstring("while"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_continue :
        pegtl::seq< pstring("continue"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_break :
        pegtl::seq< pstring("break"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_length :
        pegtl::seq< pstring("length"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};
    struct kw_new :
        pegtl::seq< pstring("new"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};


    /* ---- pars list: empty | type var (, type var)* ---- */
    struct pars_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, type, spaces, var>
        > {};

    struct pars :
        pegtl::opt<pegtl::seq<type, spaces, var, pars_tail>> {};


    /* ---- args list: empty | t (, t)* ---- */
    struct args_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, t>
        > {};

    struct args :
        pegtl::opt<pegtl::seq<t, args_tail>> {};

    /* ---- names list: name (, name)*  (for "type names" multi-declaration) ---- */
    struct names_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, var>
        > {};

    struct names :
        pegtl::seq<var, names_tail> {};

    /* ---- indices: ([t])+ ---- */
    struct indices :
        pegtl::plus<
            pegtl::seq<spaces, pegtl::one<'['>, spaces, t, spaces, pegtl::one<']'>>
        > {};


    /* ---- cond ::= t cmp t ---- */
    struct cond :
        pegtl::seq<t, spaces, cmp_op, spaces, t> {};


    /* ---- scope ::= { i* } ---- */
    struct scope_open  : pegtl::one<'{'> {};
    struct scope_close : pegtl::one<'}'> {};

    struct InstructionFormat; // forward declaration
    struct scope :
        pegtl::seq<
            spaces, scope_open,
            seps_with_comments,
            pegtl::star<InstructionFormat>,
            spaces, scope_close
        > {};


    /* ===== instructions ===== */

    // type names           (multi-declaration)
    struct insDecl :
        pegtl::seq<Ty, spaces, names> {};

    // name <- t op t       (must come before insVarAssign)
    struct insVarOp :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   t, spaces, op, spaces, t> {};

    // name <- new Array ( args )
    struct insNewArray :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   kw_new, spaces, pstring("Array"), spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // name <- new Tuple ( t )
    struct insNewTuple :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   kw_new, spaces, pstring("Tuple"), spaces,
                   pegtl::one<'('>, spaces, t, spaces, pegtl::one<')'>> {};

    // name <- length name t?
    struct insLength :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   kw_length, spaces, var,
                   pegtl::opt<pegtl::seq<spaces, t>>> {};

    // name([t])+ <- t      (array store)
    struct insArrayStore :
        pegtl::seq<var, indices, spaces, pstring("<-"), spaces, t> {};

    // name <- name([t])+   (array load)
    struct insArrayLoad :
        pegtl::seq<var, spaces, pstring("<-"), spaces, var, indices> {};

    // name <- name ( args )  (call with return)
    struct insVarCall :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   var, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // name ( args )        (call, no return)
    struct insCall :
        pegtl::seq<var, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // name <- t   (generic assignment fallback)
    struct insVarAssign :
        pegtl::seq<var, spaces, pstring("<-"), spaces, t> {};

    // return t
    struct insReturnT :
        pegtl::seq<kw_return, spaces, t> {};

    // return
    struct insReturn :
        kw_return {};

    // if ( cond ) label label
    struct insIf :
        pegtl::seq<kw_if, spaces, pegtl::one<'('>, spaces,
                   cond, spaces, pegtl::one<')'>, spaces,
                   label, spaces, label> {};

    // goto label
    struct insGoto :
        pegtl::seq<kw_goto, spaces, label> {};

    // while ( cond ) label label
    struct insWhile :
        pegtl::seq<kw_while, spaces, pegtl::one<'('>, spaces,
                   cond, spaces, pegtl::one<')'>, spaces,
                   label, spaces, label> {};

    // continue
    struct insContinue : kw_continue {};

    // break
    struct insBreak : kw_break {};

    // label (standalone)
    struct insLabel : label {};

    // scope as an instruction
    struct insScope : scope {};


    /* ---- instruction dispatch (specific first) ---- */
    struct Instruction_block :
        pegtl::sor<
            insReturnT,    // return t          (before bare return)
            insReturn,     // return
            insIf,         // if (cond) label label
            insWhile,      // while (cond) label label
            insGoto,       // goto label
            insContinue,
            insBreak,
            insLabel,      // :name
            insNewArray,
            insNewTuple,
            insLength,
            insVarOp,      // name <- t op t
            insArrayStore, // name([t])+ <- t
            insArrayLoad,  // name <- name([t])+
            insVarCall,    // name <- name(args)
            insVarAssign,  // name <- t            (fallback for assignment)
            insCall,       // name(args)
            insScope,      // { ... }
            insDecl        // type names
        > {};

    struct InstructionFormat :
        pegtl::seq<
            spaces,
            Instruction_block,
            seps_with_comments>
        {};

    


    /* ===== function & program ===== */

    struct function_header :
        pegtl::seq<
            spaces, Ty,
            spaces, var,
            spaces, pegtl::one<'('>, spaces, pars, spaces, pegtl::one<')'>
        > {};

    // f ::= T name ( pars ) scope
    struct function_def :
        pegtl::seq<
            seps_with_comments,
            function_header,
            seps_with_comments,
            scope,
            seps_with_comments
        > {};

    // p ::= f+
    struct entry_point_rule :
        pegtl::seq<
            seps_with_comments,
            pegtl::plus<function_def>,
            seps_with_comments
        > {};

    struct grammar_function :
        pegtl::must<function_def> {};

    struct grammar_program :
        pegtl::must<entry_point_rule> {};


    /* ---------- atomic constructors ---------- */

    inline Variable makeVar(const std::string& tok) {
        if (tok.empty())
            throw std::runtime_error("makeVar: expected name, got '" + tok + "'");
        return Variable(tok);
    }

    inline Number makeNumber(const std::string& tok) {
        return Number(std::stoll(tok));
    }

    inline Label makeLabel(const std::string& tok) {
        if (tok.empty() || tok[0] != ':')
            throw std::runtime_error("makeLabel: expected :name, got '" + tok + "'");
        return Label(tok.substr(1));
    }

    inline FunctionName makeFunctionName(const std::string& tok) {
        if (tok.empty())
            throw std::runtime_error("makeFunctionName: expected name, got '" + tok + "'");
        return FunctionName(tok);
    }

    // t ::= name | N
    inline T makeT(const std::string& tok) {
        if (tok.empty())
            throw std::runtime_error("makeT: empty token");
        char c = tok[0];
        if (c == '+' || c == '-' || (c >= '0' && c <= '9'))
            return T(makeNumber(tok));
        return T(makeVar(tok));
    }

    inline Type makeType(const std::string& tok) {
        if (tok == "tuple") return Type(VarType::Tuple);
        if (tok == "code")  return Type(VarType::Code);
        if (tok == "void")  return Type(VarType::Void);
        if (tok.rfind("int64", 0) == 0) {
            int64_t dims = 0;
            for (size_t i = 5; i + 1 < tok.size(); i += 2)
                if (tok[i] == '[' && tok[i + 1] == ']') ++dims;
            return Type(VarType::Int64, dims);
        }
        throw std::runtime_error("makeType: unknown type '" + tok + "'");
    }

    inline Type readType(Tokenizer& tk) {
        std::string base = tk.next();
        if (base == "int64") {
            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                tk.expect("]");
                base += "[]";
            }
        }
        return makeType(base);
    }

    inline Op stringToOp(const std::string& tok) {
        if (tok == "+")  return Op::Add;
        if (tok == "-")  return Op::Sub;
        if (tok == "*")  return Op::Mul;
        if (tok == "&")  return Op::And;
        if (tok == "<<") return Op::Shl;
        if (tok == ">>") return Op::Shr;
        if (tok == "<")  return Op::Lt;
        if (tok == "<=") return Op::Le;
        if (tok == "=")  return Op::Eq;
        if (tok == ">=") return Op::Ge;
        if (tok == ">")  return Op::Gt;
        throw std::runtime_error("stringToOp: unknown operator '" + tok + "'");
    }


    /* ============================================================
     * Actions
     * ============================================================ */

    template<typename Rule>
    struct action : pegtl::normal<Rule> {};


    /* ---- function header ---- */
    template<> struct action<function_header> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            Type ret_type = readType(tk);
            std::string name_str = tk.next();
            tk.expect("(");

            Function f;
            f.setReturnType(ret_type);
            f.setName(makeFunctionName(name_str));

            if (tk.peek() != ")") {
                {
                    Type ptype = readType(tk);
                    std::string pname = tk.next();
                    f.addParam(ptype, makeVar(pname));
                    
                }
                while (tk.peek() == ",") {
                    tk.expect(",");
                    Type ptype = readType(tk);
                    std::string pname = tk.next();
                    f.addParam(ptype, makeVar(pname));
                    
                }
            }
            tk.expect(")");

            p.functions.push_back(std::move(f));
        }
    };


    /* ---- type names      (multi-declaration) ---- */
    template<> struct action<insDecl> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            Type var_type = readType(tk);

            auto& f = p.functions.back();

            // first name
            std::string first = tk.next();

            if (!f.declareVariable(first, var_type)) {
                throw std::runtime_error("redeclaration of " + first);
            }

            {
                auto instr = std::make_unique<DeclInstruction>();
                instr->setType(var_type);
                instr->setVar(makeVar(first));

                f.currentScope->add(std::move(instr));
            }

            // additional ", name"
            while (!tk.done() && tk.peek() == ",") {
                tk.expect(",");
                std::string nm = tk.next();

                if (!f.declareVariable(nm, var_type)) {
                    throw std::runtime_error("redeclaration of " + nm);
                }

                auto instr = std::make_unique<DeclInstruction>();
                instr->setType(var_type);
                instr->setVar(makeVar(nm));

                f.currentScope->add(std::move(instr));
            }
        }
    };


    /* ---- name <- t   (generic assignment fallback) ---- */
    template<> struct action<insVarAssign> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string src_str = tk.next();

            auto& f = p.functions.back();
            auto instr = std::make_unique<AssignInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeT(src_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name <- t op t ---- */
    template<> struct action<insVarOp> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string lhs_str = tk.next();
            std::string op_str  = tk.next();

            // Handle tokenizer collapsing "- 1" / "-1" into a signed number token.
            std::string rhs_str;
            if ((op_str.size() > 1) &&
                (op_str[0] == '-' || op_str[0] == '+') &&
                std::isdigit(static_cast<unsigned char>(op_str[1]))) {
                rhs_str = op_str.substr(1);
                op_str  = op_str.substr(0, 1);
            } else {
                rhs_str = tk.next();
            }

            auto& f = p.functions.back();
            auto instr = std::make_unique<OpInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setLhs(makeT(lhs_str));
            instr->setOp(stringToOp(op_str));
            instr->setRhs(makeT(rhs_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name <- name([t])+ ---- */
    template<> struct action<insArrayLoad> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string src_str = tk.next();

            std::vector<std::string> idx_strs;
            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                idx_strs.push_back(tk.next());
                tk.expect("]");
            }

            auto& f = p.functions.back();
            auto instr = std::make_unique<ArrayLoadInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeVar(src_str));
            for (auto& ix : idx_strs) instr->addIndex(makeT(ix));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name([t])+ <- t ---- */
    template<> struct action<insArrayStore> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();

            std::vector<std::string> idx_strs;
            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                idx_strs.push_back(tk.next());
                tk.expect("]");
            }
            tk.expect("<-");
            std::string src_str = tk.next();

            auto instr = std::make_unique<ArrayStoreInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            for (auto& ix : idx_strs) instr->addIndex(makeT(ix));

            auto& f = p.functions.back();
            auto it = p.declTypes.find(src_str);
            if (it != p.declTypes.end() && it->second == VarType::Code) {
                instr->setSrcCallee(makeFunctionName(src_str));
            } else {
                instr->setSrc(makeT(src_str));
            }
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name <- length name t? ---- */
    template<> struct action<insLength> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            tk.expect("length");
            std::string arr_str = tk.next();

            auto& f = p.functions.back();
            auto instr = std::make_unique<LengthInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setArray(makeVar(arr_str));
            if (!tk.done())
                instr->setDim(makeT(tk.next()));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name <- new Array(args) ---- */
    template<> struct action<insNewArray> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            tk.expect("new");
            tk.expect("Array");
            tk.expect("(");

            std::vector<std::string> arg_strs;
            if (tk.peek() != ")") {
                arg_strs.push_back(tk.next());
                while (tk.peek() == ",") {
                    tk.expect(",");
                    arg_strs.push_back(tk.next());
                }
            }
            tk.expect(")");

            auto& f = p.functions.back();
            auto instr = std::make_unique<NewArrayInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name <- new Tuple(t) ---- */
    template<> struct action<insNewTuple> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            tk.expect("new");
            tk.expect("Tuple");
            tk.expect("(");
            std::string size_str = tk.next();
            tk.expect(")");

            auto& f = p.functions.back();
            auto instr = std::make_unique<NewTupleInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setSize(makeT(size_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name <- name(args) ---- */
    template<> struct action<insVarCall> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string callee_str = tk.next();
            tk.expect("(");

            std::vector<std::string> arg_strs;
            if (tk.peek() != ")") {
                arg_strs.push_back(tk.next());
                while (tk.peek() == ",") {
                    tk.expect(",");
                    arg_strs.push_back(tk.next());
                }
            }
            tk.expect(")");

            auto instr = std::make_unique<VarCallInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));

            auto& f = p.functions.back();

            auto it = f.lookupVariable(callee_str);
            if (it != std::nullopt && it.value().base == VarType::Code) {
                instr->setCallee(makeVar(callee_str));
            } else {
                instr->setCallee(makeFunctionName(callee_str));
            }
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- name(args) ---- */
    template<> struct action<insCall> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string callee_str = tk.next();
            tk.expect("(");

            std::vector<std::string> arg_strs;
            if (tk.peek() != ")") {
                arg_strs.push_back(tk.next());
                while (tk.peek() == ",") {
                    tk.expect(",");
                    arg_strs.push_back(tk.next());
                }
            }
            tk.expect(")");

            auto instr = std::make_unique<CallInstruction>(in.position().line);

            auto& f = p.functions.back();
            auto it = f.lookupVariable(callee_str);
            if (it != std::nullopt && it.value().base == VarType::Code) {
                instr->setCallee(makeVar(callee_str));
            } else {
                instr->setCallee(makeFunctionName(callee_str));
            }
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- return t ---- */
    template<> struct action<insReturnT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("return");
            std::string val_str = tk.next();

            auto& f = p.functions.back();
            auto instr = std::make_unique<ReturnTInstruction>(in.position().line);
            instr->setValue(makeT(val_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- return ---- */
    template<> struct action<insReturn> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            auto& f = p.functions.back();
            auto instr = std::make_unique<ReturnInstruction>(in.position().line);
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- if (cond) label label
     *      cond ::= t cmp t
     * ---- */
    template<> struct action<insIf> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("if");
            tk.expect("(");
            std::string lhs_str = tk.next();
            std::string cmp_str = tk.next();
            std::string rhs_str = tk.next();
            tk.expect(")");
            std::string true_label_str  = tk.next();
            std::string false_label_str = tk.next();

            auto& f = p.functions.back();
            auto instr = std::make_unique<IfInstruction>(in.position().line);
            instr->setLhs(makeT(lhs_str));
            instr->setCmp(stringToOp(cmp_str));
            instr->setRhs(makeT(rhs_str));
            instr->setTrueTarget(makeLabel(true_label_str));
            instr->setFalseTarget(makeLabel(false_label_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- goto label ---- */
    template<> struct action<insGoto> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("goto");
            std::string label_str = tk.next();

            auto& f = p.functions.back();
            auto instr = std::make_unique<GotoInstruction>(in.position().line);
            instr->setTarget(makeLabel(label_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- while (cond) label label ---- */
    template<> struct action<insWhile> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("while");
            tk.expect("(");
            std::string lhs_str = tk.next();
            std::string cmp_str = tk.next();
            std::string rhs_str = tk.next();
            tk.expect(")");
            std::string body_label_str = tk.next();
            std::string exit_label_str = tk.next();

            auto& f = p.functions.back();
            auto instr = std::make_unique<WhileInstruction>(in.position().line);
            instr->setLhs(makeT(lhs_str));
            instr->setCmp(stringToOp(cmp_str));
            instr->setRhs(makeT(rhs_str));
            instr->setBodyTarget(makeLabel(body_label_str));
            instr->setExitTarget(makeLabel(exit_label_str));
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- continue ---- */
    template<> struct action<insContinue> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            auto instr = std::make_unique<ContinueInstruction>(in.position().line);
            auto& f = p.functions.back();
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- break ---- */
    template<> struct action<insBreak> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            auto instr = std::make_unique<BreakInstruction>(in.position().line);
            auto& f = p.functions.back();
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- label   (standalone) ---- */
    template<> struct action<insLabel> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string label_str = tk.next();

            auto instr = std::make_unique<LabelInstruction>();
            instr->setLabel(makeLabel(label_str));
            auto& f = p.functions.back();
            f.currentScope->add(std::move(instr));
        }
    };


    /* ---- scope ::= { i* }
     *
     * Scopes nest, so we emit explicit scope-open and scope-close markers
     * around the inner instructions. The inner InstructionFormat rules fire
     * their own actions for everything inside, in order, between these
     * markers.
     * ---- */
    template<> struct action<scope_open> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {

            auto& f = p.functions.back();

            bool main_scope = f.currentScope == nullptr;
 
            f.enterScope();

            if (main_scope) {
                auto& params = f.getParams();
                auto& paramTypes = f.getParamTypes();

                for (size_t i = 0; i < params.size(); i++) {
                    f.declareVariable(params[i].name, paramTypes[i]);
                }
            }
        }
    };

    template<> struct action<scope_close> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            auto& f = p.functions.back();
            
            if (f.currentScope->parent) {
                f.currentScope->declaredTypes.insert(
                    f.currentScope->parent->declaredTypes.begin(),
                    f.currentScope->parent->declaredTypes.end()
                );
            }
            
            f.exitScope();
        }
    };


    /* ---- pegtl tracing ---- */
    template<typename Rule>
    struct my_tracer : pegtl::normal<Rule> {
        template<typename Input, typename... States>
        static void start(const Input& in, States&&...) {
            if (!TRACE) return;
            std::cerr << "try   " << pegtl::demangle<Rule>()
                      << " at line " << in.position().line
                      << " col " << in.position().column << "\n";
        }
        template<typename Input, typename... States>
        static void success(const Input& in, States&&...) {
            if (!TRACE) return;
            std::cerr << "ok    " << pegtl::demangle<Rule>() << "\n";
        }
        template<typename Input, typename... States>
        static void failure(const Input& in, States&&...) {
            if (!TRACE) return;
            std::cerr << "FAIL  " << pegtl::demangle<Rule>() << "\n";
        }
    };


    LB::Program parse_file(const char* fileName) {
        std::ifstream f(fileName);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string contents = ss.str();

        LB::Program result;

        pegtl::memory_input<> in(contents, fileName);
        pegtl::parse<grammar_program, action, my_tracer>(in, result);

        // record every function name as a Code-typed global, so calls to
        // user functions are recognized just like LA.
        for (auto& f : result.functions) {
            result.declTypes[f.getName()] = VarType::Code;
        }
        return result;
    }

}