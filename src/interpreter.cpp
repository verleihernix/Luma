#include "interpreter.h"
#include <format>
#include <cmath>

namespace luma {

    LumaValue Env::get(const std::string& name) const {
        if (auto it = vars.find(name); it != vars.end()) return it->second;
        if (parent) return parent->get(name);
        throw RuntimeError(std::format("Undefined variable '{}'", name));
    }

    void Env::set(const std::string& name, LumaValue val) { vars[name] = std::move(val); }

    bool Env::assign(const std::string& name, LumaValue val) {
        if (auto it = vars.find(name); it != vars.end()) { it->second = std::move(val); return true; }
        return parent && parent->assign(name, val);
    }

    Interpreter::Interpreter(std::shared_ptr<Env> global) : global_(std::move(global)) {}

    LumaValue Interpreter::exec_block(const Block& stmts, std::shared_ptr<Env> env) {
        for (auto& s : stmts) exec_stmt(s, env);
        return {};
    }

    // package a script function as a LumaNativeFn closure
    LumaNativeFn Interpreter::make_script_fn(
        const std::vector<std::string>& params,
        const Block* raw_body,
        const Expr* raw_body_expr,
        std::shared_ptr<Env> closure)
    {
        auto interp = this;
        return [interp, params, closure, raw_body, raw_body_expr]
        (LumaVM*, std::span<LumaValue> args) -> LumaValue
            {
                auto call_env = std::make_shared<Env>(closure);
                for (size_t i = 0; i < params.size(); ++i)
                    call_env->set(params[i], i < args.size() ? args[i] : LumaValue{});
                try {
                    if (raw_body) {
                        interp->exec_block(*raw_body, call_env);
                        return {};
                    }
                    if (raw_body_expr) return interp->eval(*raw_body_expr, call_env);
                    return {};
                }
                catch (ReturnSignal& r) { return std::move(r.value); }
            };
    }

    void Interpreter::exec_stmt(const Stmt& stmt, std::shared_ptr<Env> env) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if      constexpr (std::is_same_v<T, std::unique_ptr<LetStmt>>)    exec_let(*s, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<FnDecl>>)     exec_fn_decl(*s, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ReturnStmt>>) exec_return(*s, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<IfStmt>>)     exec_if(*s, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<WhileStmt>>)  exec_while(*s, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ForStmt>>)    exec_for(*s, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<BlockStmt>>) {
                exec_block(s->stmts, std::make_shared<Env>(env));
            }
            else if constexpr (std::is_same_v<T, std::unique_ptr<ExprStmt>>) {
                eval(*s->expr, env);
            }
            }, stmt);
    }

    void Interpreter::exec_let(const LetStmt& s, std::shared_ptr<Env> env) {
        env->set(s.name, eval(*s.init, env));
    }

    void Interpreter::exec_fn_decl(const FnDecl& d, std::shared_ptr<Env> env) {
        auto fn = make_script_fn(
            d.params,
            d.body ? d.body.get() : nullptr,
            d.bodyExpr ? d.bodyExpr.get() : nullptr,
            env);
        env->set(d.name, LumaValue(std::move(fn)));
    }

    void Interpreter::exec_if(const IfStmt& s, std::shared_ptr<Env> env) {
        if (eval(*s.condition, env).truthy())
            exec_block(*s.thenBlock, std::make_shared<Env>(env));
        else if (s.elseBlock)
            exec_block(*s.elseBlock, std::make_shared<Env>(env));
    }

    void Interpreter::exec_while(const WhileStmt& s, std::shared_ptr<Env> env) {
        while (eval(*s.condition, env).truthy())
            exec_block(*s.body, std::make_shared<Env>(env));
    }

    void Interpreter::exec_for(const ForStmt& s, std::shared_ptr<Env> env) {
        auto iter = eval(*s.iterable, env);
        if (!iter.is_list()) throw RuntimeError("for-in requires a list");
        for (auto& elem : *iter.as_list()) {
            auto inner = std::make_shared<Env>(env);
            inner->set(s.var, elem);
            exec_block(*s.body, inner);
        }
    }

    void Interpreter::exec_return(const ReturnStmt& s, std::shared_ptr<Env> env) {
        LumaValue v;
        if (s.value) v = eval(*s.value, env);
        throw ReturnSignal{ std::move(v) };
    }

    LumaValue Interpreter::eval(const Expr& expr, std::shared_ptr<Env> env) {
        return std::visit([&](const auto& e) -> LumaValue {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<NumberLit>>)
                return LumaValue(e->value);
            else if constexpr (std::is_same_v<T, std::unique_ptr<StringLit>>)
                return LumaValue(e->value);
            else if constexpr (std::is_same_v<T, std::unique_ptr<BoolLit>>)
                return LumaValue(e->value);
            else if constexpr (std::is_same_v<T, std::unique_ptr<NullLit>>)
                return LumaValue{};
            else if constexpr (std::is_same_v<T, std::unique_ptr<Identifier>>)
                return env->get(e->name);
            else if constexpr (std::is_same_v<T, std::unique_ptr<ListLit>>) {
                auto list = std::make_shared<LumaList>();
                for (auto& elem : e->elements) list->push_back(eval(elem, env));
                return LumaValue(list);
            }
            else if constexpr (std::is_same_v<T, std::unique_ptr<MapLit>>) {
                auto map = std::make_shared<LumaMap>();
                for (auto& [k, v] : e->entries) (*map)[k] = eval(v, env);
                return LumaValue(map);
            }
            else if constexpr (std::is_same_v<T, std::unique_ptr<BinaryExpr>>)
                return eval_binary(*e, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<UnaryExpr>>)
                return eval_unary(*e, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<AssignExpr>>)
                return eval_assign(*e, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpr>>)
                return eval_call(*e, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpr>>)
                return eval_index(*e, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpr>>)
                return eval_field(*e, env);
            else if constexpr (std::is_same_v<T, std::unique_ptr<FnExpr>>) {
                auto fn = make_script_fn(
                    e->params,
                    e->body ? e->body.get() : nullptr,
                    e->bodyExpr ? e->bodyExpr.get() : nullptr,
                    env);
                return LumaValue(std::move(fn));
            }
            return LumaValue{};
            }, expr);
    }

    LumaValue Interpreter::eval_binary(const BinaryExpr& e, std::shared_ptr<Env> env) {
        if (e.op == "&&") { auto l = eval(*e.left, env); return l.truthy() ? eval(*e.right, env) : l; }
        if (e.op == "||") { auto l = eval(*e.left, env); return l.truthy() ? l : eval(*e.right, env); }

        auto left = eval(*e.left, env);
        auto right = eval(*e.right, env);

        // string concat
        if (e.op == "+" && (left.is_string() || right.is_string()))
            return LumaValue(left.to_string() + right.to_string());

        // arithmetic
        if (e.op == "+" || e.op == "-" || e.op == "*" || e.op == "/" || e.op == "%") {
            if (!left.is_number() || !right.is_number())
                throw RuntimeError(std::format("Operator '{}' requires numbers", e.op));
            double l = left.as_number(), r = right.as_number();
            if (e.op == "+") return LumaValue(l + r);
            if (e.op == "-") return LumaValue(l - r);
            if (e.op == "*") return LumaValue(l * r);
            if (e.op == "/") { if (r == 0) throw RuntimeError("Division by zero"); return LumaValue(l / r); }
            if (e.op == "%") return LumaValue(std::fmod(l, r));
        }

        if (e.op == "==") return LumaValue(left == right);
        if (e.op == "!=") return LumaValue(!(left == right));

        if (!left.is_number() || !right.is_number())
            throw RuntimeError(std::format("Operator '{}' requires numbers", e.op));
        double l = left.as_number(), r = right.as_number();
        if (e.op == "<") return LumaValue(l < r);
        if (e.op == "<=") return LumaValue(l <= r);
        if (e.op == ">") return LumaValue(l > r);
        if (e.op == ">=") return LumaValue(l >= r);
        throw RuntimeError(std::format("Unknown operator '{}'", e.op));
    }

    LumaValue Interpreter::eval_unary(const UnaryExpr& e, std::shared_ptr<Env> env) {
        auto val = eval(*e.operand, env);
        if (e.op == "!") return LumaValue(!val.truthy());
        if (e.op == "-") {
            if (!val.is_number()) throw RuntimeError("Unary '-' requires a number");
            return LumaValue(-val.as_number());
        }
        throw RuntimeError(std::format("Unknown unary op '{}'", e.op));
    }

    LumaValue Interpreter::eval_assign(const AssignExpr& e, std::shared_ptr<Env> env) {
        auto val = eval(*e.value, env);
        if (auto* id = std::get_if<std::unique_ptr<Identifier>>(e.target.get())) {
            if (!env->assign((*id)->name, val))
                throw RuntimeError(std::format("Undefined variable '{}'", (*id)->name));
            return val;
        }
        if (auto* ip = std::get_if<std::unique_ptr<IndexExpr>>(e.target.get())) {
            auto obj = eval(*(*ip)->object, env);
            auto index = eval(*(*ip)->index, env);
            if (obj.is_list()) {
                if (!index.is_number()) throw RuntimeError("List index must be a number");
                auto& lst = *obj.as_list();
                size_t i = static_cast<size_t>(index.as_number());
                if (i >= lst.size()) throw RuntimeError("List index out of bounds");
                lst[i] = val; return val;
            }
            if (obj.is_map()) { (*obj.as_map())[index.to_string()] = val; return val; }
            throw RuntimeError("Index assignment on non-list/map");
        }
        if (auto* fp = std::get_if<std::unique_ptr<FieldExpr>>(e.target.get())) {
            auto obj = eval(*(*fp)->object, env);
            if (!obj.is_map()) throw RuntimeError("Field assignment on non-map");
            (*obj.as_map())[(*fp)->field] = val; return val;
        }
        throw RuntimeError("Invalid assignment target");
    }

    LumaValue Interpreter::eval_call(const CallExpr& e, std::shared_ptr<Env> env) {
        auto callee = eval(*e.callee, env);
        std::vector<LumaValue> args;
        args.reserve(e.args.size());
        for (auto& a : e.args) args.push_back(eval(a, env));
        return call_value(callee, std::move(args), e.line);
    }

    LumaValue Interpreter::call_value(const LumaValue& callee, std::vector<LumaValue> args, int line) {
        if (callee.is_function())
            return callee.as_fn()(nullptr, std::span<LumaValue>(args));
        throw RuntimeError(std::format("[line {}] Value is not callable", line));
    }

    LumaValue Interpreter::eval_index(const IndexExpr& e, std::shared_ptr<Env> env) {
        auto obj = eval(*e.object, env);
        auto index = eval(*e.index, env);
        if (obj.is_list()) {
            if (!index.is_number()) throw RuntimeError("List index must be a number");
            auto& lst = *obj.as_list();
            size_t i = static_cast<size_t>(index.as_number());
            if (i >= lst.size()) throw RuntimeError("List index out of bounds");
            return lst[i];
        }
        if (obj.is_map()) {
            auto& m = *obj.as_map();
            if (auto it = m.find(index.to_string()); it != m.end()) return it->second;
            return {};
        }
        throw RuntimeError("Index on non-list/map");
    }

    LumaValue Interpreter::eval_field(const FieldExpr& e, std::shared_ptr<Env> env) {
        auto obj = eval(*e.object, env);
        if (obj.is_map()) {
            auto& m = *obj.as_map();
            if (auto it = m.find(e.field); it != m.end()) return it->second;
            return {};
        }
        if (obj.is_list() && e.field == "length") return LumaValue(static_cast<double>(obj.as_list()->size()));
        if (obj.is_string() && e.field == "length") return LumaValue(static_cast<double>(obj.as_string().size()));
        throw RuntimeError(std::format("Field '{}' not found", e.field));
    }

} // namespace luma