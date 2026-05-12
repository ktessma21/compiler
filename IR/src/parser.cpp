#include <fstream>
#include <sstream>
#include "IR.h"
#include "ast_leaves.h"
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


namespace IR {


  /* ===== shared lexical rules ===== */
  struct name:
    pegtl::seq<
      pegtl::plus<
        pegtl::sor<
          pegtl::alpha,
          pegtl::one< '_' >
        >
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
            pegtl::seq<
                pegtl::one<'%'>,
                name
            > {};


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




    /* ===== IR grammar ===== */

    // l ::= @name
    struct l :
        pegtl::seq<pegtl::one<'@'>, name> {};

    // label ::= :name
    struct label :
        pegtl::seq<pegtl::one<':'>, name> {};

    // N ::= number
    struct N : number {};

    // t ::= var | N
    struct t :
        pegtl::sor<var, N> {};

    // u ::= var | l
    struct u :
        pegtl::sor<var, l> {};

    // s ::= t | l
    struct s :
        pegtl::sor<l, t> {};

    // op ::= + | - | * | & | << | >> | < | <= | = | >= | >
    struct op :
        pegtl::sor<
            pstring("<<"),
            pstring(">>"),
            pstring("<="),
            pstring(">="),
            pegtl::one<'+'>,
            pegtl::one<'-'>,
            pegtl::one<'*'>,
            pegtl::one<'&'>,
            pegtl::one<'<'>,
            pegtl::one<'>'>,
            pegtl::one<'='>
        > {};

    // type ::= int64([])* | tuple | code
    struct type_int64 :
        pegtl::seq<pstring("int64"), pegtl::star<pegtl::seq<spaces, pegtl::one<'['>, spaces, pegtl::one<']'>>>> {};

    struct type :
        pegtl::sor<
            type_int64,
            pstring("tuple"),
            pstring("code")
        > {};

    // T ::= type | void
    struct T_ret :
        pegtl::sor<
            pstring("void"),
            type
        > {};

    // callee ::= u | print | input | tuple-error | tensor-error
    struct callee :
        pegtl::sor<
            pstring("print"),
            pstring("input"),
            pstring("tuple-error"),
            pstring("tensor-error"),
            u
        > {};

    /* ----- pars list: empty | type var | type var (, type var)* ----- */
    struct typed_var :
        pegtl::seq<type, spaces, var> {};

    struct pars_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, typed_var>
        > {};

    struct pars :
        pegtl::opt<pegtl::seq<typed_var, pars_tail>> {};


    /* ----- args list: empty | t (, t)* ----- */
    struct args_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, t>
        > {};

    struct args :
        pegtl::opt<pegtl::seq<t, args_tail>> {};


    /* ----- index list: ([t])+ ----- */
    struct one_index :
        pegtl::seq<pegtl::one<'['>, spaces, t, spaces, pegtl::one<']'>> {};

    struct indices :
        pegtl::plus<one_index> {};


    /* ===== instructions ===== */

    // type var
    struct insTypeDecl :
        pegtl::seq<type, spaces, var> {};

    // var <- t op t
    struct insVarOp :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   t, spaces, op, spaces, t> {};

    // var <- var([t])+
    struct insIndexLoad :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   var, spaces, indices> {};

    // var([t])+ <- s
    struct insIndexStore :
        pegtl::seq<var, spaces, indices, spaces,
                   pstring("<-"), spaces, s> {};

    // var <- length var t
    struct insLengthDim :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("length"), spaces, var, spaces, t> {};

    // var <- length var
    struct insLength :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("length"), spaces, var> {};

    // var <- call callee ( args )
    struct insVarCall :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("call"), spaces, callee, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // call callee ( args )
    struct insCall :
        pegtl::seq<pstring("call"), spaces, callee, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // var <- new Array(args)
    struct insNewArray :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("new"), spaces, pstring("Array"), spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // var <- new Tuple(t)
    struct insNewTuple :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("new"), spaces, pstring("Tuple"), spaces,
                   pegtl::one<'('>, spaces, t, spaces, pegtl::one<')'>> {};

    // var <- s
    struct insVarAssign :
        pegtl::seq<var, spaces, pstring("<-"), spaces, s> {};

    // return t
    struct insReturnT :
        pegtl::seq<pstring("return"), spaces, t> {};

    // return
    struct insReturn :
        pstring("return") {};

    // br t label label
    struct insBrT :
        pegtl::seq<pstring("br"), spaces, t, spaces, label, spaces, label> {};

    // br label
    struct insBr :
        pegtl::seq<pstring("br"), spaces, label> {};

    // label as a standalone instruction
    struct insLabel : label {};


    /* Specific-first ordering for shared prefixes. */
    struct Instruction_block :
        pegtl::sor<
            insIndexStore,    // var([t])+ <- s
            insNewArray,      // var <- new Array(...)
            insNewTuple,      // var <- new Tuple(...)
            insVarCall,       // var <- call ...
            insLengthDim,     // var <- length var t
            insLength,        // var <- length var
            insIndexLoad,     // var <- var([t])+
            insVarOp,         // var <- t op t
            insVarAssign,     // var <- s
            insReturnT,       // return t       (before bare return)
            insReturn,        // return
            insBrT,           // br t label label  (before br label)
            insBr,            // br label
            insCall,          // call callee ( args )
            insLabel,         // :name
            insTypeDecl       // type var
        > {};


    /* ===== function & program ===== */

    struct InstructionFormat :
        pegtl::seq<
            spaces,
            Instruction_block,
            seps_with_comments>
        {};


    // Synthetic marker: fires before function_header so we can push a fresh Function.
    // (Matches the empty string, then function_def proceeds with the real header.)
    struct function_start : pegtl::success {};
    struct function_end : pegtl::success {};

    struct function_header :
        pegtl::seq<
            spaces, pstring("define"),
            function_start,
            spaces, T_ret,
            spaces, l,
            spaces, pegtl::one<'('>, spaces, pars, spaces, pegtl::one<')'>
        > {};

 

    // f ::= define T l ( pars ) { i+ }
    //   (the i+ here is the flat instruction stream;
    //    labels open new bbs and terminators close them via actions)
    struct function_def :
        pegtl::seq<
            seps_with_comments,
            function_header,
            seps_with_comments,
            spaces, pegtl::one<'{'>,
            seps_with_comments,
            pegtl::plus<InstructionFormat>,
            spaces, pegtl::seq<pegtl::one<'}'>, function_end>,
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
        if (tok.empty() || tok[0] != '%')
            throw std::runtime_error("makeVar: expected %name, got '" + tok + "'");
        return Variable(tok.substr(1));
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
        if (tok.empty() || tok[0] != '@')
            throw std::runtime_error("makeFunctionName: expected @name, got '" + tok + "'");
        return FunctionName(tok.substr(1));
    }

    inline T makeT(const std::string& tok) {
        if (tok.empty())
            throw std::runtime_error("makeT: empty token");
        if (tok[0] == '%') return T(makeVar(tok));
        return T(makeNumber(tok));
    }

    inline S makeS(const std::string& tok) {
        if (tok.empty())
            throw std::runtime_error("makeS: empty token");
        if (tok[0] == '@') return S(makeFunctionName(tok));
        if (tok[0] == '%') return S(makeVar(tok));
        return S(makeNumber(tok));
    }

    inline Callee makeCallee(const std::string& tok) {
        if (tok == "print")        return Callee(BuiltinCallee::Print);
        if (tok == "input")        return Callee(BuiltinCallee::Input);
        if (tok == "tuple-error")  return Callee(BuiltinCallee::TupleError);
        if (tok == "tensor-error") return Callee(BuiltinCallee::TensorError);
        if (tok.empty())
            throw std::runtime_error("makeCallee: empty token");
        if (tok[0] == '%') return Callee(makeVar(tok));
        if (tok[0] == '@') return Callee(makeFunctionName(tok));
        throw std::runtime_error("makeCallee: unknown callee '" + tok + "'");
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

    inline Type parseType(Tokenizer& tk) {
        std::string first = tk.next();
        if (first == "void")  return Type(TypeKind::Void);
        if (first == "tuple") return Type(TypeKind::Tuple);
        if (first == "code")  return Type(TypeKind::Code);
        if (first == "int64") {
            int dims = 0;
            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                tk.expect("]");
                ++dims;
            }
            return Type(TypeKind::Int64, dims);
        }
        throw std::runtime_error("parseType: unknown type '" + first + "'");
    }


    /* ============================================================
     * Routing helpers
     *
     * Every body instruction (not a label, not a terminator) goes into
     * the current basic block:
     *     p.functions.back().blocks.back()->instructions
     *
     * A label starts a NEW basic block.
     * A terminator FINISHES the current basic block by setting its `terminator`.
     * ============================================================ */

    inline BasicBlock& current_bb(Program& p) {
        return *p.functions.back().blocks.back();
    }

    inline void push_body(Program& p, std::unique_ptr<Instruction> instr) {
        current_bb(p).instructions.push_back(std::move(instr));
    }

    inline void set_terminator(Program& p, std::unique_ptr<Instruction> instr) {
        current_bb(p).terminator = std::move(instr);
    }


   /* ============================================================
    * IR instruction actions
    * ============================================================ */

    template<typename Rule>
    struct action : pegtl::normal<Rule> {};


    // function_start: push a fresh Function onto the program before we parse its header.
    template<> struct action<function_start> {
        template<typename Input>
        static void apply(const Input&, Program& p) {
            p.functions.emplace_back();
        }
    };

    // function_end: triggers to start the process of the graph building. 
    template<> struct action<function_end> {
        template<typename Input>
        static void apply(const Input&, Program& p) {
            p.functions.back().build_successor_graph();
        }
    };


    // function_header: fill in the (already-pushed) current function's name/type/params.
    template<> struct action<function_header> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("define");
            Type rtype = parseType(tk);
            std::string name_str = tk.next();    // @name
            tk.expect("(");

            Function& f = p.functions.back();
            f.setReturnType(rtype);
            f.setName(makeFunctionName(name_str));

            if (tk.peek() != ")") {
                Type pt = parseType(tk);
                f.addParam(pt, makeVar(tk.next()));
                while (tk.peek() == ",") {
                    tk.expect(",");
                    Type pt2 = parseType(tk);
                    f.addParam(pt2, makeVar(tk.next()));
                }
            }
            tk.expect(")");
        }
    };


    // label   :name   --> opens a NEW basic block
    template<> struct action<insLabel> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string label_str = tk.next();

            auto lbl = std::make_unique<LabelInstruction>();
            lbl->setLabel(makeLabel(label_str));

            auto bb = std::make_unique<BasicBlock>();
            bb->label = std::move(lbl);
            p.functions.back().blocks.push_back(std::move(bb));
        }
    };


    // type var
    template<> struct action<insTypeDecl> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            Type t = parseType(tk);
            std::string var_str = tk.next();

            auto instr = std::make_unique<TypeDeclInstruction>();
            instr->setType(t);
            instr->setVar(makeVar(var_str));
            push_body(p, std::move(instr));
        }
    };


    // var <- s
    template<> struct action<insVarAssign> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string src_str = tk.next();

            auto instr = std::make_unique<AssignInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeS(src_str));
            push_body(p, std::move(instr));
        }
    };


    // var <- t op t
    template<> struct action<insVarOp> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string lhs_str = tk.next();
            std::string op_str  = tk.next();
            std::string rhs_str = tk.next();

            auto instr = std::make_unique<OpInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setLhs(makeT(lhs_str));
            instr->setOp(stringToOp(op_str));
            instr->setRhs(makeT(rhs_str));
            push_body(p, std::move(instr));
        }
    };


    // var <- var([t])+
    template<> struct action<insIndexLoad> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str  = tk.next();
            tk.expect("<-");
            std::string base_str = tk.next();

            auto instr = std::make_unique<IndexLoadInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setBase(makeVar(base_str));

            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                std::string idx_str = tk.next();
                tk.expect("]");
                instr->addIndex(makeT(idx_str));
            }
            push_body(p, std::move(instr));
        }
    };


    // var([t])+ <- s
    template<> struct action<insIndexStore> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string base_str = tk.next();

            auto instr = std::make_unique<IndexStoreInstruction>();
            instr->setBase(makeVar(base_str));

            while (tk.peek() == "[") {
                tk.expect("[");
                std::string idx_str = tk.next();
                tk.expect("]");
                instr->addIndex(makeT(idx_str));
            }
            tk.expect("<-");
            std::string src_str = tk.next();
            instr->setSrc(makeS(src_str));
            push_body(p, std::move(instr));
        }
    };


    // var <- length var t
    template<> struct action<insLengthDim> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str  = tk.next();
            tk.expect("<-");
            tk.expect("length");
            std::string base_str = tk.next();
            std::string dim_str  = tk.next();

            auto instr = std::make_unique<LengthInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setBase(makeVar(base_str));
            instr->setDim(makeT(dim_str));
            push_body(p, std::move(instr));
        }
    };


    // var <- length var
    template<> struct action<insLength> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str  = tk.next();
            tk.expect("<-");
            tk.expect("length");
            std::string base_str = tk.next();

            auto instr = std::make_unique<LengthInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setBase(makeVar(base_str));
            push_body(p, std::move(instr));
        }
    };


    // var <- call callee ( args )
    template<> struct action<insVarCall> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            tk.expect("call");
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

            auto instr = std::make_unique<VarCallInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setCallee(makeCallee(callee_str));
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            push_body(p, std::move(instr));
        }
    };


    // call callee ( args )
    template<> struct action<insCall> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("call");
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

            auto instr = std::make_unique<CallInstruction>();
            instr->setCallee(makeCallee(callee_str));
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            push_body(p, std::move(instr));
        }
    };


    // var <- new Array(args)
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

            auto instr = std::make_unique<NewArrayInstruction>();
            instr->setDst(makeVar(dst_str));
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            push_body(p, std::move(instr));
        }
    };


    // var <- new Tuple(t)
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

            auto instr = std::make_unique<NewTupleInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setSize(makeT(size_str));
            push_body(p, std::move(instr));
        }
    };


    // return t  --> TERMINATOR, closes current bb
    template<> struct action<insReturnT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("return");
            std::string val_str = tk.next();

            auto instr = std::make_unique<ReturnTInstruction>();
            instr->setValue(makeT(val_str));
            set_terminator(p, std::move(instr));
        }
    };


    // return  --> TERMINATOR
    template<> struct action<insReturn> {
        template<typename Input>
        static void apply(const Input&, Program& p) {
            auto instr = std::make_unique<ReturnInstruction>();
            set_terminator(p, std::move(instr));
        }
    };


    // br t label label  --> TERMINATOR
    template<> struct action<insBrT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("br");
            std::string cond_str    = tk.next();
            std::string true_label  = tk.next();
            std::string false_label = tk.next();

            auto instr = std::make_unique<BrTInstruction>();
            instr->setCond(makeT(cond_str));
            instr->setTrueTarget(makeLabel(true_label));
            instr->setFalseTarget(makeLabel(false_label));
            set_terminator(p, std::move(instr));
        }
    };


    // br label  --> TERMINATOR
    template<> struct action<insBr> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("br");
            std::string label_str = tk.next();

            auto instr = std::make_unique<BrInstruction>();
            instr->setTarget(makeLabel(label_str));
            set_terminator(p, std::move(instr));
        }
    };


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
        static void success(const Input&, States&&...) {
            if (!TRACE) return;
            std::cerr << "ok    " << pegtl::demangle<Rule>() << "\n";
        }
        template<typename Input, typename... States>
        static void failure(const Input&, States&&...) {
            if (!TRACE) return;
            std::cerr << "FAIL  " << pegtl::demangle<Rule>() << "\n";
        }
    };


    IR::Program parse_file(const char* fileName) {
        std::ifstream f(fileName);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string contents = ss.str();

        IR::Program result;
        pegtl::memory_input<> in(contents, fileName);
        pegtl::parse<grammar_program, action, my_tracer>(in, result);
        return result;
    }


}