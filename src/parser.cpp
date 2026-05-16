#include <fstream>
#include "l3.h"
#include <parser.h>
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


// handle the M case especially using code generation step. for now assume M is number

namespace L3 {


 

  /* ===== shared lexical rules (unchanged from your L2) ===== */
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




        /* ===== L3 grammar ===== */

    // l ::= @name
    struct l :
        pegtl::seq<pegtl::one<'@'>, name> {};

    // label ::= :name
    struct label :
        pegtl::seq<pegtl::one<':'>, name> {};

    // N ::= number (already defined as `number` above)
    struct N : number {};

    // t ::= var | N
    struct t :
        pegtl::sor<var, N> {};

    // u ::= var | l
    struct u :
        pegtl::sor<var, l> {};

    // s ::= t | label | l
    //   ordered: label and l have unique prefixes (':' / '@'), put them first
    struct s :
        pegtl::sor<label, l, t> {};

    // op ::= + | - | * | & | << | >>
    //   IMPORTANT: longer matches first ('<<' before '<', '>>' before '>')
    struct op :
        pegtl::sor<
            pstring("<<"),
            pstring(">>"),
            pegtl::one<'+'>,
            pegtl::one<'-'>,
            pegtl::one<'*'>,
            pegtl::one<'&'>
        > {};

    // cmp ::= < | <= | = | >= | >
    //   IMPORTANT: '<=' before '<', '>=' before '>'
    struct cmp :
        pegtl::sor<
            pstring("<="),
            pstring(">="),
            pegtl::one<'<'>,
            pegtl::one<'>'>,
            pegtl::one<'='>
        > {};

    // callee ::= u | print | allocate | input | tuple-error | tensor-error
    //   put specific strings before u (since u is generic and could match a var named "print")
    struct callee :
        pegtl::sor<
            pstring("print"),
            pstring("allocate"),
            pstring("input"),
            pstring("tuple-error"),
            pstring("tensor-error"),
            u
        > {};

    /* ----- vars list (function parameters): empty | var | var (, var)* ----- */
  
    struct vars_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, var>
        > {};

    struct vars :
        pegtl::opt<pegtl::seq<var, vars_tail>> {};


    /* ----- args list (call arguments): empty | t (, t)* ----- */
    struct args_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, t>
        > {};

    struct args :
        pegtl::opt<pegtl::seq<t, args_tail>> {};

    /* ===== instructions =====
     * i ::= var <- s
     *     | var <- t op t
     *     | var <- t cmp t
     *     | var <- load var
     *     | store var <- s
     *     | return
     *     | return t
     *     | label
     *     | br label
     *     | br t label
     *     | call callee ( args )
     *     | var <- call callee ( args )
     */

    // var <- t op t
    struct insVarOp :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   t, spaces, op, spaces, t> {};

    // var <- t cmp t
    struct insVarCmp :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   t, spaces, cmp, spaces, t> {};

    // var <- load var
    struct insLoad :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("load"), spaces, var> {};

    // store var <- s
    struct insStore :
        pegtl::seq<pstring("store"), spaces, var, spaces,
                   pstring("<-"), spaces, s> {};

    // var <- call callee ( args )
    struct insVarCall :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("call"), spaces, callee, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // call callee ( args )
    struct insCall :
        pegtl::seq<pstring("call"), spaces, callee, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // var <- s    (this is the generic fallback for assignment — must come last)
    struct insVarAssign :
        pegtl::seq<var, spaces, pstring("<-"), spaces, s> {};

    // return t
    struct insReturnT :
        pegtl::seq<pstring("return"), spaces, t> {};

    // return
    struct insReturn :
        pstring("return") {};

    // br t label
    struct insBrT :
        pegtl::seq<pstring("br"), spaces, t, spaces, label> {};

    // br label
    struct insBr :
        pegtl::seq<pstring("br"), spaces, label> {};

    // label (as standalone instruction)
    struct insLabel : label {};

    /* All var-prefixed forms share the prefix `var <- ...`, so order them
     * specific-first. Same for return/br pairs. */
    struct Instruction_block :
        pegtl::sor<
            insLoad,        // var <- load var       (specific: 'load')
            insVarCall,     // var <- call ...       (specific: 'call')
            insVarOp,       // var <- t op t
            insVarCmp,      // var <- t cmp t
            insVarAssign,   // var <- s              (generic fallback)
            insStore,       // store var <- s
            insReturnT,     // return t              (before bare return)
            insReturn,      // return
            insBrT,         // br t label            (before br label — t prefix differs)
            insBr,          // br label
            insCall,        // call callee ( args )
            insLabel        // :name (standalone)
        > {};

    /* ===== function & program ===== */
      
    struct InstructionFormat :
        pegtl::seq<
            spaces,
            Instruction_block,
            seps_with_comments>
        {};
    
    struct function_header :
        pegtl::seq<
            spaces, pstring("define"),
            spaces, l,
            spaces, pegtl::one<'('>, spaces, vars, spaces, pegtl::one<')'>
        > {};

    // f ::= define l ( vars ) { i+ }
   struct function_def :
        pegtl::seq<
            seps_with_comments,
            function_header,
            seps_with_comments,
            spaces, pegtl::one<'{'>,
            seps_with_comments,
            pegtl::plus<InstructionFormat>,
            spaces, pegtl::one<'}'>,
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

        // var ::= %name
        inline Variable makeVar(const std::string& tok) {
            if (tok.empty() || tok[0] != '%')
                throw std::runtime_error("makeVar: expected %name, got '" + tok + "'");
            return Variable(tok.substr(1));   // strip the '%'
        }

        // N ::= (+|-)? digit+
        inline Number makeNumber(const std::string& tok) {
            return Number(std::stoll(tok));
        }

        // label ::= :name
        inline Label makeLabel(const std::string& tok) {
            if (tok.empty() || tok[0] != ':')
                throw std::runtime_error("makeLabel: expected :name, got '" + tok + "'");
            return Label(tok.substr(1));      // strip the ':'
        }

       


        /* ---------- composite constructors (dispatch by first char) ---------- */

        // l ::= @name   (FunctionName is an alias for std::string, with '@' included)
        inline FunctionName makeFunctionName(const std::string& tok) {
            if (tok.empty() || tok[0] != '@')
                throw std::runtime_error("makeFunctionName: expected @name, got '" + tok + "'");
            return FunctionName(tok.substr(1));
        }

        // t ::= var | N
        inline T makeT(const std::string& tok) {
            if (tok.empty())
                throw std::runtime_error("makeT: empty token");
            if (tok[0] == '%') return T(makeVar(tok));
            return T(makeNumber(tok));
        }

        // s ::= t | label | l   (i.e. var | N | :label | @function)
        inline S makeS(const std::string& tok) {
            if (tok.empty())
                throw std::runtime_error("makeS: empty token");
            if (tok[0] == ':') return S(makeLabel(tok));
            if (tok[0] == '@') return S(makeFunctionName(tok));
            if (tok[0] == '%') return S(makeVar(tok));
            return S(makeNumber(tok));
        }

        // u ::= var | l
        inline U makeU(const std::string& tok) {
            if (tok.empty())
                throw std::runtime_error("makeU: empty token");
            if (tok[0] == '%') return U(makeVar(tok));
            if (tok[0] == '@') return U(makeFunctionName(tok));
            throw std::runtime_error("makeU: expected %var or @function, got '" + tok + "'");
        }

        // callee ::= u | print | allocate | input | tuple-error | tensor-error
        inline Callee makeCallee(const std::string& tok) {
            if (tok == "print")        return Callee(BuiltinCallee::Print);
            if (tok == "allocate")     return Callee(BuiltinCallee::Allocate);
            if (tok == "input")        return Callee(BuiltinCallee::Input);
            if (tok == "tuple-error")  return Callee(BuiltinCallee::TupleError);
            if (tok == "tensor-error") return Callee(BuiltinCallee::TensorError);
            if (tok.empty())
                throw std::runtime_error("makeCallee: empty token");
            if (tok[0] == '%') return Callee(makeVar(tok));
            if (tok[0] == '@') return Callee(makeFunctionName(tok));
            throw std::runtime_error("makeCallee: unknown callee '" + tok + "'");
        }


        /* ---------- operator/comparison enums ---------- */

        // op ::= + | - | * | & | << | >>
        inline Op stringToOp(const std::string& tok) {
            if (tok == "+")  return Op::Add;
            if (tok == "-")  return Op::Sub;
            if (tok == "*")  return Op::Mul;
            if (tok == "&")  return Op::And;
            if (tok == "<<") return Op::Shl;
            if (tok == ">>") return Op::Shr;
            throw std::runtime_error("stringToOp: unknown operator '" + tok + "'");
        }

        // cmp ::= < | <= | = | >= | >
        inline Cmp stringToCmp(const std::string& tok) {
            if (tok == "<")  return Cmp::Lt;
            if (tok == "<=") return Cmp::Le;
            if (tok == "=")  return Cmp::Eq;
            if (tok == ">=") return Cmp::Ge;
            if (tok == ">")  return Cmp::Gt;
            throw std::runtime_error("stringToCmp: unknown comparison '" + tok + "'");
        }


   /* ============================================================
    * L3 instruction actions
    * Each action runs after PEGTL matches the corresponding rule.
    * The matched text is re-tokenized and consumed in order.
 * ============================================================ */

        
    template<typename Rule>
    struct action : pegtl::normal<Rule> {};

    template<> struct action<function_header> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("define");
            std::string name_str = tk.next();   // @name
            tk.expect("(");

            Function f;
            f.setName(makeFunctionName(name_str));

            if (tk.peek() != ")") {
                f.addParam(makeVar(tk.next()));
                while (tk.peek() == ",") {
                    tk.expect(",");
                    f.addParam(makeVar(tk.next()));
                }
            }
            tk.expect(")");

            p.functions.push_back(std::move(f));
        }
    };

    // var <- s          (generic assignment fallback)
    template<> struct action<insVarAssign> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // var
            tk.expect("<-");
            std::string src_str = tk.next();   // s = t | label | l

            auto instr = std::make_unique<AssignInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeS(src_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // var <- t op t
    template<> struct action<insVarOp> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // var
            tk.expect("<-");
            std::string lhs_str = tk.next();   // t
            std::string op_str  = tk.next();   // op: + - * & << >>
            std::string rhs_str = tk.next();   // t

            auto instr = std::make_unique<OpInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setLhs(makeT(lhs_str));
            instr->setOp(stringToOp(op_str));
            instr->setRhs(makeT(rhs_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // var <- t cmp t
    template<> struct action<insVarCmp> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // var
            tk.expect("<-");
            std::string lhs_str = tk.next();   // t
            std::string cmp_str = tk.next();   // cmp: < <= = >= >
            std::string rhs_str = tk.next();   // t

            auto instr = std::make_unique<CmpInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setLhs(makeT(lhs_str));
            instr->setCmp(stringToCmp(cmp_str));
            instr->setRhs(makeT(rhs_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // var <- load var
    template<> struct action<insLoad> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // var
            tk.expect("<-");
            tk.expect("load");
            std::string src_str = tk.next();   // var

            auto instr = std::make_unique<LoadInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeVar(src_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // store var <- s
    template<> struct action<insStore> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("store");
            std::string dst_str = tk.next();   // var
            tk.expect("<-");
            std::string src_str = tk.next();   // s

            auto instr = std::make_unique<StoreInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeS(src_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // var <- call callee ( args )
    template<> struct action<insVarCall> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // var
            tk.expect("<-");
            tk.expect("call");
            std::string callee_str = tk.next();   // callee
            tk.expect("(");

            std::vector<std::string> arg_strs;
            if (tk.peek() != ")") {
                arg_strs.push_back(tk.next());        // first t
                while (tk.peek() == ",") {
                    tk.expect(",");
                    arg_strs.push_back(tk.next());    // subsequent t
                }
            }
            tk.expect(")");

            auto instr = std::make_unique<VarCallInstruction>();
            instr->setDst(makeVar(dst_str));
            instr->setCallee(makeCallee(callee_str));
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            p.functions.back().instructions.push_back(std::move(instr));
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
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // return t
    template<> struct action<insReturnT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("return");
            std::string val_str = tk.next();   // t

            auto instr = std::make_unique<ReturnTInstruction>();
            instr->setValue(makeT(val_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // return
    template<> struct action<insReturn> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            // no operands
            auto instr = std::make_unique<ReturnInstruction>();
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // br t label
    template<> struct action<insBrT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("br");
            std::string cond_str  = tk.next();   // t
            std::string label_str = tk.next();   // :name

            auto instr = std::make_unique<BrTInstruction>();
            instr->setCond(makeT(cond_str));
            instr->setTarget(makeLabel(label_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // br label
    template<> struct action<insBr> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            tk.expect("br");
            std::string label_str = tk.next();   // :name

            auto instr = std::make_unique<BrInstruction>();
            instr->setTarget(makeLabel(label_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // label   (standalone :name as instruction)
    template<> struct action<insLabel> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string label_str = tk.next();   // :name

            auto instr = std::make_unique<LabelInstruction>();
            instr->setLabel(makeLabel(label_str));
            p.functions.back().instructions.push_back(std::move(instr));
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




    static L3::Function parsefunction(const std::string& source) {
        std::string contents = source;
        size_t close = contents.rfind(')');
        if (close == std::string::npos)
            throw std::runtime_error("parsefunction: no closing ')'");
        std::string prog_part = contents.substr(0, close + 1);

        L3::Program tmp;                                  // was: L3::Function result
        pegtl::memory_input<> in(prog_part, "l3_function");
        pegtl::parse<grammar_function, action, my_tracer>(in, tmp);

        if (tmp.functions.size() != 1)
            throw std::runtime_error("parsefunction: expected exactly one function");
        return std::move(tmp.functions.front());
    }

    L3::Program parse_file(const char* fileName) {
        std::ifstream f(fileName);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string contents = ss.str();

        L3::Program result;
        pegtl::memory_input<> in(contents, fileName);
        pegtl::parse<grammar_program, action, my_tracer>(in, result);
        return result;
    }


}

