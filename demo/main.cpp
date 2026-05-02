#include "../include/luma.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <format>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cmath>
#include <ctime>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>

static bool use_color = true;

static std::vector<std::string> registered_functions;

static std::runtime_error luma_error(std::string_view fn, std::string_view msg) {
    return std::runtime_error(std::format("{}: {}", fn, msg));
}

static std::runtime_error luma_error(
    std::string_view fn,
    std::string_view expected,
    std::string_view got
) {
    return std::runtime_error(
        std::format("{}: expected {}, got {}", fn, expected, got)
    );
}

static void expect_args(std::span<LumaValue> a, size_t n, std::string_view fn) {
    if (a.size() < n) {
        throw std::runtime_error(
            std::format("{}: expected {} arguments, got {}", fn, n, a.size())
        );
    }
}

static double expect_number(const LumaValue& v, std::string_view fn) {
    if (!v.is_number()) {
        throw luma_error(fn, "number", v.to_string());
    }
    return v.as_number();
}

static const std::string& expect_string(const LumaValue& v, std::string_view fn) {
    if (!v.is_string()) {
        throw luma_error(fn, "string", v.to_string());
    }
    return v.as_string();
}

static const bool expect_bool(const LumaValue& v, std::string_view fn) {
    if (!v.is_bool())
        throw luma_error(fn, "bool", v.to_string());

    return v.as_bool();
}

static LumaList& expect_list(LumaValue& v, std::string_view fn) {
    if (!v.is_list()) {
        throw luma_error(fn, "list", v.to_string());
    }
    return *v.as_list();
}

static LumaMap& expect_map(LumaValue& v, std::string_view fn) {
    if (!v.is_map()) {
        throw luma_error(fn, "map", v.to_string());
    }
    return *v.as_map();
}

static std::string col(std::string_view code, std::string_view text) {
    if (!use_color) return std::string(text);
    return std::format("\033[{}m{}\033[0m", code, text);
}

static auto red(std::string_view s) { return col("31", s); }
static auto yellow(std::string_view s) { return col("33", s); }
static auto green(std::string_view s) { return col("32", s); }
static auto cyan(std::string_view s) { return col("36", s); }
static auto bold(std::string_view s) { return col("1", s); }
static auto dim(std::string_view s) { return col("2", s); }

static void register_stdlib(LumaVM* vm) {
    // macro to easily register functions
#define REG_FN(name, impl) \
        luma_register_function(vm, name, impl); \
        registered_functions.push_back(name)

    REG_FN("println",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "println");
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) std::cout << ' ';
                std::cout << args[i].to_string();
            }
            std::cout << '\n';
            return {};
        });

    REG_FN("eprint",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "eprint");
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) std::cerr << ' ';
                std::cerr << args[i].to_string();
            }
            std::cerr << '\n';
            return {};
        });

    REG_FN("input",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            if (!args.empty())
                std::cout << args[0].to_string();
            std::string line;
            if (!std::getline(std::cin, line))
                throw std::runtime_error("input: EOF reached");
            return LumaValue(line);
        });

    REG_FN("tostring",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "tostring");
            return LumaValue(args[0].to_string());
        });

    REG_FN("tonumber",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "tonumber");
            if (args[0].is_number())
                return args[0];
            if (args[0].is_string()) {
                try {
                    return LumaValue(std::stod(args[0].as_string()));
                }
                catch (...) {
                    throw luma_error("tonumber", "valid number string", args[0].as_string());
                }
            }
            throw luma_error("tonumber", "number or numeric string", args[0].to_string());
        });

    REG_FN("typeof", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "typeof");
            auto& v = args[0];
            std::string type;
            if (v.is_null())     type = "null";
            else if (v.is_bool())   type = "bool";
            else if (v.is_number()) type = "number";
            else if (v.is_string()) type = "string";
            else if (v.is_list())   type = "list";
            else if (v.is_map())    type = "map";
            else if (v.is_function()) type = "function";
            else type = "unknown";
            return LumaValue(std::string(type));
        });

    REG_FN("len",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "len");
            auto& v = args[0];
            if (v.is_list())   return LumaValue((double)v.as_list()->size());
            if (v.is_string()) return LumaValue((double)v.as_string().size());
            if (v.is_map())    return LumaValue((double)v.as_map()->size());
            throw luma_error("len", "list|string|map", v.to_string());
        });

    REG_FN("push",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 2, "push");
            LumaList& list = expect_list(args[0], "push");
            list.push_back(args[1]);
            return args[0];
        });

    REG_FN("pop",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "pop");
            LumaList& list = expect_list(args[0], "pop");
            if (list.empty()) return {};
            auto v = list.back();
            list.pop_back();
            return v;
        });

    REG_FN("keys",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "keys");
            LumaMap& m = expect_map(args[0], "keys");
            auto result = std::make_shared<LumaList>();
            for (auto& [k, _] : m)
                result->push_back(LumaValue(k));
            return LumaValue(result);
        });

    REG_FN("has",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 2, "has");
            LumaMap& m = expect_map(args[0], "has");
            return LumaValue(m.contains(args[1].to_string()));
        });

    REG_FN("math_floor",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_floor");
            return LumaValue(std::floor(expect_number(a[0], "math_floor")));
        });

    REG_FN("math_ceil",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_ceil");
            return LumaValue(std::ceil(expect_number(a[0], "math_ceil")));
        });

    REG_FN("math_round",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_round");
            return LumaValue(std::round(expect_number(a[0], "math_round")));
        });

    REG_FN("math_abs",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_abs");
            return LumaValue(std::abs(expect_number(a[0], "math_abs")));
        });

    REG_FN("math_sqrt",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_sqrt");
            return LumaValue(std::sqrt(expect_number(a[0], "math_sqrt")));
        });

    REG_FN("math_pow",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "math_pow");
            return LumaValue(std::pow(
                expect_number(a[0], "math_pow"),
                expect_number(a[1], "math_pow")
            ));
        });

    REG_FN("math_sin",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_sin");
            return LumaValue(std::sin(expect_number(a[0], "math_sin")));
        });

    REG_FN("math_cos",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "math_cos");
            return LumaValue(std::cos(expect_number(a[0], "math_cos")));
        });

    REG_FN("math_min",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "math_min");
            return LumaValue(std::min(
                expect_number(a[0], "math_min"),
                expect_number(a[1], "math_min")
            ));
        });

    REG_FN("math_max",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "math_max");
            return LumaValue(std::max(
                expect_number(a[0], "math_max"),
                expect_number(a[1], "math_max")
            ));
        });

    REG_FN("math_rand",
        [](LumaVM*, std::span<LumaValue>) -> LumaValue {
            return LumaValue((double)std::rand() / RAND_MAX);
        });

    REG_FN("str_upper",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "str_upper");
            auto s = expect_string(a[0], "str_upper");
            for (auto& c : s) c = std::toupper(c);
            return LumaValue(s);
        });

    REG_FN("str_lower",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 1, "str_lower");
            auto s = expect_string(a[0], "str_lower");
            for (auto& c : s) c = std::tolower(c);
            return LumaValue(s);
        });

    REG_FN("str_sub",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "str_sub");
            auto& s = expect_string(a[0], "str_sub");
            size_t start = (size_t)expect_number(a[1], "str_sub");
            size_t len = std::string::npos;
            if (a.size() >= 3)
                len = (size_t)expect_number(a[2], "str_sub");
            return LumaValue(s.substr(start, len));
        });

    REG_FN("str_contains",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "str_contains");
            return LumaValue(
                a[0].as_string().find(a[1].as_string()) != std::string::npos
            );
        });

    REG_FN("str_starts",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "str_starts");
            return LumaValue(a[0].as_string().starts_with(a[1].as_string()));
        });

    REG_FN("str_ends",
        [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
            expect_args(a, 2, "str_ends");
            return LumaValue(a[0].as_string().ends_with(a[1].as_string()));
        });

    REG_FN("wait",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "wait");
            int ms = (int)expect_number(args[0], "wait");
            if (ms < 0) ms = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return {};
        });

    REG_FN("writefile",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 2, "writefile");
            const std::string& path = expect_string(args[0], "writefile");
            const std::string& content = expect_string(args[1], "writefile");

            std::ofstream file(path, std::ios::out | std::ios::trunc);

            if (!file.is_open())
                throw std::runtime_error("Cannot open file: " + path);

            file << content;
            return {};
        });

    REG_FN("readfile",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "readfile");

            const std::string& path = expect_string(args[0], "readfile");

            std::ifstream file(path, std::ios::in);

            if (!file.is_open())
                throw std::runtime_error("Cannot open file: " + path);

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            return LumaValue(std::move(content));
        });

    REG_FN("assert",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 1, "assert");

            const bool is_true = expect_bool(args[0], "assert");
            if (!is_true)
                throw std::runtime_error("assertion failed");

            return {};
        });

    REG_FN("range",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 2, "range");

            const int start = (int)expect_number(args[0], "range");
            const int end = (int)expect_number(args[1], "range");

            auto list = std::make_shared<LumaList>();

            if (start <= end) {
                for (int i = start; i <= end; i++)
                    list->push_back(LumaValue((double)i));
            }
            else {
                for (int i = start; i >= end; --i) {
                    list->push_back(LumaValue((double)i));
                }
            }

            return LumaValue(list);
        });

    REG_FN("remove",
        [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
            expect_args(args, 2, "remove");

            LumaList& list = expect_list(args[0], "remove");
            const int index = (int)expect_number(args[1], "remove");

            if (index < 0 || static_cast<size_t>(index) >= list.size())
                throw std::runtime_error("index out of range");

            list.erase(list.begin() + index);

            return {};
        });
#undef REG_FN
}

static void print_error(std::string_view err) {
    std::cerr << red("✖ ") << bold(err) << '\n';
}

static bool is_incomplete(std::string_view src) {
    int braces = 0, parens = 0;
    bool in_str = false;

    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];

        if (c == '"' && (i == 0 || src[i - 1] != '\\'))
            in_str = !in_str;

        if (in_str) continue;

        if (c == '{') ++braces;
        else if (c == '}') --braces;

        if (c == '(') ++parens;
        else if (c == ')') --parens;
    }

    return braces > 0 || parens > 0;
}

static void run_repl(LumaVM* vm) {
    std::cout << bold(cyan("Luma"))
        << dim(":help for help, :quit to exit\n");

    std::string buffer;
    bool multiline = false;

    while (true) {
        std::cout << (multiline ? dim("... ") : cyan(">>> ")) << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }

        if (!multiline) {
            if (line == ":quit" || line == ":q" || line == ":exit")
                break;

            if (line == ":help" || line == ":h") {
                std::cout << bold("\nLuma REPL commands:\n")
                    << cyan(":help") << " / " << cyan(":h") << " show help\n"
                    << cyan(":quit") << " / " << cyan(":q") << " exit\n"
                    << cyan(":clear") << " reset global state\n"
                    << cyan(":load <file>") << " load script\n"
                    << cyan(":fns") << " list functions\n\n";
                continue;
            }

            if (line == ":fns") {
                std::cout << bold("Registered functions:\n");
                if (registered_functions.empty()) {
                    std::cout << dim("  (none)\n");
                }
                else {
                    auto names = registered_functions;
                    std::sort(names.begin(), names.end());
                    for (const auto& name : names) {
                        std::cout << "  " << cyan(name) << '\n';
                    }
                }
                std::cout << '\n';
                continue;
            }

            if (line == ":clear") {
                luma_destroy(vm);
                vm = luma_create();
                register_stdlib(vm);
                std::cout << green("State cleared.\n");
                continue;
            }
            if (line.rfind(":load ", 0) == 0) { // starts with ":load "
                std::string filename = line.substr(6);
                // trim whitespace
                filename.erase(0, filename.find_first_not_of(" \t"));
                filename.erase(filename.find_last_not_of(" \t") + 1);
                if (filename.empty()) {
                    std::cerr << red("Error: no filename provided for :load\n");
                }
                else {
                    if (!luma_run_file(vm, filename.c_str())) {
                        print_error(luma_last_error(vm));
                    }
                }
                continue;
            }

            if (line.empty()) continue;
        }

        buffer += line + '\n';
        multiline = is_incomplete(buffer);

        if (multiline) continue;

        if (!luma_run(vm, buffer.c_str())) {
            print_error(luma_last_error(vm));
        }

        buffer.clear();
    }

    std::cout << dim("\nGoodbye!\n");
}

static void print_help(std::string_view progname) {
    std::cout << bold("\nLuma - scripting language\n\n")
        << "Usage:\n"
        << progname << "               start REPL\n"
        << progname << " <file>        run script\n"
        << progname << " -e <code>     execute code\n"
        << progname << " --help        show help\n"
        << progname << " --t           test if Luma is working correclty\n\n";
}

int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (std::getenv("NO_COLOR"))
        use_color = false;

    LumaVM* vm = luma_create();
    register_stdlib(vm);

    std::vector<std::string_view> args(argv + 1, argv + argc);

    if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        print_help(argv[0]);
        luma_destroy(vm);
        return 0;
    }

    if (args.size() >= 2 && args[0] == "-e") {
        if (!luma_run(vm, std::string(args[1]).c_str())) {
            print_error(luma_last_error(vm));
            luma_destroy(vm);
            return 1;
        }
        luma_destroy(vm);
        return 0;
    }

    if (!args.empty() && args[0] == "-t") {
        std::cout << "Hello from LUMA!" << std::endl;
        luma_destroy(vm);
        return 0;
    }

    if (!args.empty() && args[0] != "-") {
        if (!luma_run_file(vm, std::string(args[0]).c_str())) {
            print_error(luma_last_error(vm));
            luma_destroy(vm);
            return 1;
        }
        luma_destroy(vm);
        return 0;
    }

    run_repl(vm);
    luma_destroy(vm);
    return 0;
}