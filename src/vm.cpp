#include "../include/luma.h"
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include <fstream>
#include <sstream>

struct LumaVM {
    std::shared_ptr<luma::Env>         global;
    std::unique_ptr<luma::Interpreter> interp;
    std::string                        last_error;
    // keep the AST alive so raw pointers in closures remain valid
    std::vector<luma::Block>           ast_roots;
};

extern "C" {

    LumaVM* luma_create() {
        auto vm = new LumaVM;
        vm->global = std::make_shared<luma::Env>();
        vm->interp = std::make_unique<luma::Interpreter>(vm->global);
        return vm;
    }

    void luma_destroy(LumaVM* vm) {
        delete vm;
    }

    void luma_register_function(LumaVM* vm, const char* name, LumaNativeFn fn) {
        vm->global->set(name, LumaValue(std::move(fn)));
    }

    bool luma_run(LumaVM* vm, const char* source) {
        vm->last_error.clear();
        try {
            luma::Lexer  lexer(source);
            auto tokens = lexer.tokenize();

            // check for lex errors
            for (auto& t : tokens) {
                if (t.kind == luma::TokenKind::Error)
                    throw luma::ParseError(std::string("Lex error: ") + t.value);
            }

            luma::Parser parser(std::move(tokens));
            auto block = parser.parse();

            // keep AST alive
            vm->ast_roots.push_back(std::move(block));
            auto& root = vm->ast_roots.back();

            vm->interp->exec_block(root, vm->global);
            return true;
        }
        catch (const luma::ParseError& e) {
            vm->last_error = std::string("ParseError: ") + e.what();
            return false;
        }
        catch (const luma::RuntimeError& e) {
            vm->last_error = std::string("RuntimeError: ") + e.what();
            return false;
        }
        catch (const std::exception& e) {
            vm->last_error = std::string("Error: ") + e.what();
            return false;
        }
    }

    bool luma_run_file(LumaVM* vm, const char* path) {
        std::ifstream f(path);
        if (!f) {
            vm->last_error = std::string("Cannot open file: ") + path;
            return false;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        return luma_run(vm, ss.str().c_str());
    }

    const char* luma_last_error(LumaVM* vm) {
        return vm->last_error.c_str();
    }

    LumaValue luma_call(LumaVM* vm, const char* name, std::span<LumaValue> args) {
        try {
            auto fn = vm->global->get(name);
            std::vector<LumaValue> argv(args.begin(), args.end());
            if (std::holds_alternative<LumaNativeFn>(fn.data)) {
                return std::get<LumaNativeFn>(fn.data)(vm, std::span<LumaValue>(argv));
            }
            vm->last_error = std::string("'") + name + "' is not callable";
            return LumaValue{};
        }
        catch (const luma::RuntimeError& e) {
            vm->last_error = e.what();
            return LumaValue{};
        }
    }

    LumaValue luma_get_global(LumaVM* vm, const char* name) {
        try { return vm->global->get(name); }
        catch (...) { return LumaValue{}; }
    }

    void luma_set_global(LumaVM* vm, const char* name, LumaValue value) {
        vm->global->set(name, std::move(value));
    }

} // extern "C"