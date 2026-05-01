#pragma once
#include "../include/luma.h"
#include "ast.h"
#include <unordered_map>
#include <memory>

namespace luma {

    struct Env : std::enable_shared_from_this<Env> {
        std::unordered_map<std::string, LumaValue> vars;
        std::shared_ptr<Env> parent;
        explicit Env(std::shared_ptr<Env> p = nullptr) : parent(std::move(p)) {}

        LumaValue get(const std::string& name) const;
        void      set(const std::string& name, LumaValue val);
        bool      assign(const std::string& name, LumaValue val);
    };

    struct ReturnSignal { LumaValue value; };

    struct RuntimeError : std::runtime_error {
        explicit RuntimeError(const std::string& msg) : std::runtime_error(msg) {}
    };

    class Interpreter {
    public:
        explicit Interpreter(std::shared_ptr<Env> global);
        LumaValue exec_block(const Block& stmts, std::shared_ptr<Env> env);
        LumaValue eval(const Expr& expr, std::shared_ptr<Env> env);

    private:
        void exec_stmt(const Stmt& stmt, std::shared_ptr<Env> env);
        void exec_let(const LetStmt&, std::shared_ptr<Env>);
        void exec_fn_decl(const FnDecl&, std::shared_ptr<Env>);
        void exec_if(const IfStmt&, std::shared_ptr<Env>);
        void exec_while(const WhileStmt&, std::shared_ptr<Env>);
        void exec_for(const ForStmt&, std::shared_ptr<Env>);
        void exec_return(const ReturnStmt&, std::shared_ptr<Env>);

        LumaValue eval_binary(const BinaryExpr&, std::shared_ptr<Env>);
        LumaValue eval_unary(const UnaryExpr&, std::shared_ptr<Env>);
        LumaValue eval_assign(const AssignExpr&, std::shared_ptr<Env>);
        LumaValue eval_call(const CallExpr&, std::shared_ptr<Env>);
        LumaValue eval_index(const IndexExpr&, std::shared_ptr<Env>);
        LumaValue eval_field(const FieldExpr&, std::shared_ptr<Env>);

        LumaNativeFn make_script_fn(
            const std::vector<std::string>& params,
            const Block* raw_body,
            const Expr* raw_body_expr,
            std::shared_ptr<Env> closure);

        LumaValue call_value(const LumaValue& callee, std::vector<LumaValue> args, int line);

        std::shared_ptr<Env> global_;
    };

} // namespace luma