#include <fstream>
#include "la.h"
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

namespace LA {

    // static lineNumber = 0;
 

  /* ===== shared lexical rules (unchanged from your L3) ===== */
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




        /* ===== LA grammar ===== */

    // label ::= :name
    struct label :
        pegtl::seq<pegtl::one<':'>, name> {};

    // N ::= number (already defined as `number` above)
    struct N : number {};

    // t ::= name | N
    //   ordered: N first so a leading sign/digit isn't swallowed by name
    struct t :
        pegtl::sor<N, var> {};

    // type ::= int64([])* | tuple | code
    //   the ([])* tail is the array-dimension suffix on int64
    struct type :
        pegtl::sor<
            pegtl::seq<
                pstring("int64"),
                pegtl::star<pegtl::seq<pegtl::one<'['>, pegtl::one<']'>>>
            >,
            pstring("tuple"),
            pstring("code")
        > {};

    // T ::= type | void   (grammar rule renamed Ty: 'T' is the value typedef in ast_leaves.h)
    struct Ty :
        pegtl::sor<
            type,
            pstring("void")
        > {};

    // op ::= + | - | * | & | << | >> | < | <= | = | >= | >
    //   IMPORTANT: longer matches first ('<<' before '<', '>=' before '>', etc.)
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

    /* ----- pars list (function parameters): empty | type var | type var (, type var)* ----- */

    struct pars_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, type, spaces, var>
        > {};

    struct pars :
        pegtl::opt<pegtl::seq<type, spaces, var, pars_tail>> {};


    /* ----- args list (call arguments): empty | t (, t)* ----- */
    struct args_tail :
        pegtl::star<
            pegtl::seq<spaces, pegtl::one<','>, spaces, t>
        > {};

    struct args :
        pegtl::opt<pegtl::seq<t, args_tail>> {};

    /* ----- indices: ([t])+  (one or more bracketed subscripts) ----- */
    struct indices :
        pegtl::plus<
            pegtl::seq<spaces, pegtl::one<'['>, spaces, t, spaces, pegtl::one<']'>>
        > {};

    /* ===== instructions =====
     * i ::= type name
     *     | name <- t
     *     | name <- t op t
     *     | name <- name([t])+
     *     | name([t])+ <- t
     *     | name <- length name t?
     *     | name <- new Array( args )
     *     | name <- new Tuple( t )
     *     | name <- name ( args? )
     *     | name ( args? )
     *     | return
     *     | return t
     *     | label
     *     | br label
     *     | br t label
     */

    // type name
    struct insDecl :
        pegtl::seq<Ty, spaces, var> {};

    // name <- t op t
    struct insVarOp :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   t, spaces, op, spaces, t> {};

    // name <- new Array ( args )
    struct insNewArray :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("new"), spaces, pstring("Array"), spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // name <- new Tuple ( t )
    struct insNewTuple :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("new"), spaces, pstring("Tuple"), spaces,
                   pegtl::one<'('>, spaces, t, spaces, pegtl::one<')'>> {};

    // name <- length name t?
    struct insLength :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   pstring("length"), spaces, var,
                   pegtl::opt<pegtl::seq<spaces, t>>> {};

    // name([t])+ <- t        (array store)
    struct insArrayStore :
        pegtl::seq<var, indices, spaces, pstring("<-"), spaces, t> {};

    // name <- name([t])+     (array load)
    struct insArrayLoad :
        pegtl::seq<var, spaces, pstring("<-"), spaces, var, indices> {};

    // name <- name ( args )  (call with return)
    struct insVarCall :
        pegtl::seq<var, spaces, pstring("<-"), spaces,
                   var, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // name ( args )          (call, no return)
    struct insCall :
        pegtl::seq<var, spaces,
                   pegtl::one<'('>, spaces, args, spaces, pegtl::one<')'>> {};

    // name <- t    (this is the generic fallback for assignment — must come last)
    struct insVarAssign :
        pegtl::seq<var, spaces, pstring("<-"), spaces, t> {};

    // Keywords must not be followed by a name character.
    struct kw_return :
        pegtl::seq< pstring("return"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};

    struct kw_br :
        pegtl::seq< pstring("br"),
                    pegtl::not_at< pegtl::sor< pegtl::alpha, pegtl::one<'_'>, pegtl::digit > > > {};

    // return t
    struct insReturnT :
        pegtl::seq<kw_return, spaces, t> {};

    struct insReturn :
        kw_return {};

    struct insBrT :
        pegtl::seq<kw_br, spaces, t, spaces, label, spaces, label> {};

    struct insBr :
        pegtl::seq<kw_br, spaces, label> {};

    // label (as standalone instruction)
    struct insLabel : label {};

    /* All var-prefixed forms share the prefix `name <- ...` (or `name(`), so order them
     * specific-first. Same for return/br pairs. The `type name` decl and the
     * return/br keyword forms are placed so their keyword prefixes win first. */
    struct Instruction_block :
        pegtl::sor<
            insReturnT,     // return t              (before bare return)
            insReturn,      // return
            insBrT,         // br t label            (before br label — t prefix differs)
            insBr,          // br label
            insLabel,       // :name (standalone)
            insNewArray,    // name <- new Array(...)
            insNewTuple,    // name <- new Tuple(...)
            insLength,      // name <- length name t?
            insVarOp,       // name <- t op t
            insArrayStore,  // name([t])+ <- t       (store: brackets before '<-')
            insArrayLoad,   // name <- name([t])+    (load:  brackets after  '<-')
            insVarCall,     // name <- name ( args )
            insVarAssign,   // name <- t             (generic fallback)
            insCall,        // name ( args )
            insDecl         // type name             (only thing starting with a type kw)
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
            spaces, Ty,
            spaces, var,
            spaces, pegtl::one<'('>, spaces, pars, spaces, pegtl::one<')'>
        > {};

    // f ::= T name ( pars ) { i* }
   struct function_def :
        pegtl::seq<
            seps_with_comments,
            function_header,
            seps_with_comments,
            spaces, pegtl::one<'{'>,
            seps_with_comments,
            pegtl::star<InstructionFormat>,
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

        // var ::= name
        inline Variable makeVar(const std::string& tok) {
            if (tok.empty())
                throw std::runtime_error("makeVar: expected name, got '" + tok + "'");
            return Variable(tok);
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

        // name ::= function name  (FunctionName is an alias for std::string)
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

        // type ::= int64([])* | tuple | code ; T ::= type | void
        inline Type makeType(const std::string& tok) {
            if (tok == "tuple") return Type(VarType::Tuple);
            if (tok == "code")  return Type(VarType::Code);
            if (tok == "void")  return Type(VarType::Void);
            if (tok.rfind("int64", 0) == 0) {
                // count "[]" pairs to get array dimensionality
                int64_t dims = 0;
                for (size_t i = 5; i + 1 < tok.size(); i += 2)
                    if (tok[i] == '[' && tok[i + 1] == ']') ++dims;
                return Type(VarType::Int64, dims);
            }
            throw std::runtime_error("makeType: unknown type '" + tok + "'");
        }

        // Read a full type from the tokenizer. The tokenizer splits "int64[]"
        // into separate tokens (int64, [, ]), so reassemble the base token plus
        // any trailing '[' ']' pairs into the string makeType expects.
        inline Type readType(Tokenizer& tk) {
            std::string base = tk.next();          // int64 | tuple | code | void
            if (base == "int64") {
                while (!tk.done() && tk.peek() == "[") {
                    tk.expect("[");
                    tk.expect("]");
                    base += "[]";
                }
            }
            return makeType(base);
        }


        /* ---------- operator enum ---------- */

        // op ::= + | - | * | & | << | >> | < | <= | = | >= | >
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
    * LA instruction actions
    * Each action runs after PEGTL matches the corresponding rule.
    * The matched text is re-tokenized and consumed in order.
 * ============================================================ */

        
    template<typename Rule>
    struct action : pegtl::normal<Rule> {};

    template<> struct action<function_header> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            Type ret_type = readType(tk);       // T (may span int64 [ ] tokens)
            std::string name_str = tk.next();   // name
            tk.expect("(");

            Function f;
            f.setReturnType(ret_type);
            f.setName(makeFunctionName(name_str));

            if (tk.peek() != ")") {
                // type var (, type var)*
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

            for (size_t i = 0; i < f.getParams().size(); ++i) {
                f.declTypes[f.getParams()[i].name] = f.getParamTypes()[i].base;
            }

            p.functions.push_back(std::move(f));
        }
    };

    // type name          (variable declaration)
    template<> struct action<insDecl> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            Type var_type = readType(tk);       // T (may span int64 [ ] tokens)
            std::string name_str = tk.next();   // name

            auto instr = std::make_unique<DeclInstruction>();
            instr->setType(var_type);
            instr->setVar(makeVar(name_str));
            p.functions.back().instructions.push_back(std::move(instr));
            p.functions.back().declTypes[name_str] = var_type.base;  // record
        }
    };

    // pegtl::eol

    // template<> struct action<pegtl::eol || pegtl::eolf> {
    //     template<typename Input>
    //     static void apply(const Input& in, Program& p){
    //         lineNumber++;
    //     }
    // }

    // name <- t          (generic assignment fallback)
    template<> struct action<insVarAssign> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name
            tk.expect("<-");
            std::string src_str = tk.next();   // t

            auto instr = std::make_unique<AssignInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeT(src_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name <- t op t
    template<> struct action<insVarOp> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();
            tk.expect("<-");
            std::string lhs_str = tk.next();
            std::string op_str  = tk.next();

            // If the tokenizer collapsed "- 1" or "-1" into a signed number token,
            // split it: the leading sign is the op, the rest is the rhs.
            std::string rhs_str;
            if ((op_str.size() > 1) &&
                (op_str[0] == '-' || op_str[0] == '+') &&
                std::isdigit(static_cast<unsigned char>(op_str[1]))) {
                rhs_str = op_str.substr(1);
                op_str  = op_str.substr(0, 1);
            } else {
                rhs_str = tk.next();
            }

            auto instr = std::make_unique<OpInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setLhs(makeT(lhs_str));
            instr->setOp(stringToOp(op_str));
            instr->setRhs(makeT(rhs_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name <- name([t])+     (array load)
    template<> struct action<insArrayLoad> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name
            tk.expect("<-");
            std::string src_str = tk.next();   // name

            std::vector<std::string> idx_strs;
            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                idx_strs.push_back(tk.next());   // t
                tk.expect("]");
            }

            auto instr = std::make_unique<ArrayLoadInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setSrc(makeVar(src_str));
            for (auto& ix : idx_strs) instr->addIndex(makeT(ix));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name([t])+ <- t        (array store)
    template<> struct action<insArrayStore> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name

            std::vector<std::string> idx_strs;
            while (!tk.done() && tk.peek() == "[") {
                tk.expect("[");
                idx_strs.push_back(tk.next());   // t
                tk.expect("]");
            }
            tk.expect("<-");
            std::string src_str = tk.next();   // t

            // std::cerr << in.position().line << "from array store func parser \n";
            

            auto instr = std::make_unique<ArrayStoreInstruction>(in.position().line);

            instr->setDst(makeVar(dst_str));
            for (auto& ix : idx_strs) instr->addIndex(makeT(ix));
            auto& f = p.functions.back();
                auto it = p.declTypes.find(src_str);
                // std::cerr << src_str << '\n';
                if (it != p.declTypes.end() && it->second == VarType::Code) {
                    instr->setSrcCallee(makeFunctionName(src_str));
                    // assert(false);
                } else {
                    instr->setSrc(makeT(src_str));
            }
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name <- length name t?
    template<> struct action<insLength> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name
            tk.expect("<-");
            tk.expect("length");
            std::string arr_str = tk.next();   // name

            auto instr = std::make_unique<LengthInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setArray(makeVar(arr_str));
            if (!tk.done())                    // optional dimension t
                instr->setDim(makeT(tk.next()));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name <- new Array ( args )
    template<> struct action<insNewArray> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name
            tk.expect("<-");
            tk.expect("new");
            tk.expect("Array");
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

            auto instr = std::make_unique<NewArrayInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name <- new Tuple ( t )
    template<> struct action<insNewTuple> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name
            tk.expect("<-");
            tk.expect("new");
            tk.expect("Tuple");
            tk.expect("(");
            std::string size_str = tk.next();   // t
            tk.expect(")");

            auto instr = std::make_unique<NewTupleInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));
            instr->setSize(makeT(size_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name <- name ( args )
    template<> struct action<insVarCall> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            Tokenizer tk(in.string());
            std::string dst_str = tk.next();   // name
            tk.expect("<-");
            std::string callee_str = tk.next();   // name
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

            auto instr = std::make_unique<VarCallInstruction>(in.position().line);
            instr->setDst(makeVar(dst_str));

            auto& f = p.functions.back();
            auto it = f.declTypes.find(callee_str);
            if (it != f.declTypes.end() && it->second == VarType::Code) {
                instr->setCallee(makeVar(callee_str));      
            } else {
                instr->setCallee(makeFunctionName(callee_str));
            }
            for (auto& a : arg_strs) instr->addArg(makeT(a));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // name ( args )
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
            auto it = f.declTypes.find(callee_str);
            if (it != f.declTypes.end() && it->second == VarType::Code) {
                instr->setCallee(makeVar(callee_str));      // indirect call through a code variable
            } else {
                instr->setCallee(makeFunctionName(callee_str));
            }

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

            auto instr = std::make_unique<ReturnTInstruction>(in.position().line);
            instr->setValue(makeT(val_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    // return
    template<> struct action<insReturn> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            // no operands
            auto instr = std::make_unique<ReturnInstruction>(in.position().line);
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
            std::string true_label_str = tk.next();   // :name
            std::string false_label_str = tk.next();   // :name

            auto instr = std::make_unique<BrTInstruction>(in.position().line);
            instr->setCond(makeT(cond_str));
            instr->setTrueTarget(makeLabel(true_label_str));
            instr->setFalseTarget(makeLabel(false_label_str));
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

            auto instr = std::make_unique<BrInstruction>(in.position().line);
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





    LA::Program parse_file(const char* fileName) {
        std::ifstream f(fileName);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string contents = ss.str();

        LA::Program result;

        pegtl::memory_input<> in(contents, fileName);
        pegtl::parse<grammar_program, action, my_tracer>(in, result);

        for (auto& f: result.functions){
            std::string fn = f.getName();
            result.declTypes[fn] = VarType::Code;
        }
        return result;
    }


  


}
