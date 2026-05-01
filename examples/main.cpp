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

static bool use_color = true;

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

    luma_register_function(vm, "print", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) std::cout << ' ';
            std::cout << args[i].to_string();
        }
        std::cout << '\n';
        return {};
        });

    luma_register_function(vm, "println", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) std::cout << ' ';
            std::cout << args[i].to_string();
        }
        std::cout << '\n';
        return {};
        });

    luma_register_function(vm, "eprint", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) std::cerr << ' ';
            std::cerr << args[i].to_string();
        }
        std::cerr << '\n';
        return {};
        });

    luma_register_function(vm, "tostring", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        return LumaValue(args.empty() ? std::string("null") : args[0].to_string());
        });

    luma_register_function(vm, "tonumber", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty()) return {};
        if (args[0].is_number()) return args[0];
        if (args[0].is_string()) {
            try { return LumaValue(std::stod(args[0].as_string())); }
            catch (...) {}
        }
        return {};
        });

    luma_register_function(vm, "typeof", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty()) return LumaValue(std::string("null"));
        auto& v = args[0];
        if (v.is_null())     return LumaValue(std::string("null"));
        if (v.is_bool())     return LumaValue(std::string("bool"));
        if (v.is_number())   return LumaValue(std::string("number"));
        if (v.is_string())   return LumaValue(std::string("string"));
        if (v.is_list())     return LumaValue(std::string("list"));
        if (v.is_map())      return LumaValue(std::string("map"));
        if (v.is_function()) return LumaValue(std::string("function"));
        return LumaValue(std::string("unknown"));
        });

    luma_register_function(vm, "len", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty()) return LumaValue(0.0);
        if (args[0].is_list())   return LumaValue(static_cast<double>(args[0].as_list()->size()));
        if (args[0].is_string()) return LumaValue(static_cast<double>(args[0].as_string().size()));
        if (args[0].is_map())    return LumaValue(static_cast<double>(args[0].as_map()->size()));
        return LumaValue(0.0);
        });

    luma_register_function(vm, "push", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.size() < 2 || !args[0].is_list())
            throw std::runtime_error("push() expects (list, value)");
        args[0].as_list()->push_back(args[1]);
        return args[0];
        });

    luma_register_function(vm, "pop", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty() || !args[0].is_list())
            throw std::runtime_error("pop() expects (list)");
        auto& lst = *args[0].as_list();
        if (lst.empty()) return {};
        auto v = lst.back();
        lst.pop_back();
        return v;
        });

    luma_register_function(vm, "keys", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty() || !args[0].is_map())
            throw std::runtime_error("keys() expects (map)");
        auto result = std::make_shared<LumaList>();
        for (auto& [k, _] : *args[0].as_map())
            result->push_back(LumaValue(k));
        return LumaValue(result);
        });

    luma_register_function(vm, "has", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.size() < 2 || !args[0].is_map())
            throw std::runtime_error("has() expects (map, key)");
        auto& m = *args[0].as_map();
        return LumaValue(m.contains(args[1].to_string()));
        });

    luma_register_function(vm, "assert", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty() || !args[0].truthy()) {
            std::string msg = (args.size() > 1) ? args[1].to_string() : "Assertion failed";
            throw std::runtime_error(msg);
        }
        return {};
        });

    luma_register_function(vm, "error", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        throw std::runtime_error(args.empty() ? "error()" : args[0].to_string());
        return {};
        });

    luma_register_function(vm, "input", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (!args.empty()) std::cout << args[0].to_string();
        std::string line;
        if (!std::getline(std::cin, line)) return {};
        return LumaValue(line);
        });

    luma_register_function(vm, "math_floor", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::floor(a[0].as_number()));
        });
    luma_register_function(vm, "math_ceil", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::ceil(a[0].as_number()));
        });
    luma_register_function(vm, "math_round", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::round(a[0].as_number()));
        });
    luma_register_function(vm, "math_abs", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::abs(a[0].as_number()));
        });
    luma_register_function(vm, "math_sqrt", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::sqrt(a[0].as_number()));
        });
    luma_register_function(vm, "math_pow", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::pow(a[0].as_number(), a[1].as_number()));
        });
    luma_register_function(vm, "math_sin", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::sin(a[0].as_number()));
        });
    luma_register_function(vm, "math_cos", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::cos(a[0].as_number()));
        });
    luma_register_function(vm, "math_min", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::min(a[0].as_number(), a[1].as_number()));
        });
    luma_register_function(vm, "math_max", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(std::max(a[0].as_number(), a[1].as_number()));
        });
    luma_register_function(vm, "math_rand", [](LumaVM*, std::span<LumaValue>) -> LumaValue {
        return LumaValue(static_cast<double>(std::rand()) / RAND_MAX);
        });

    luma_register_function(vm, "str_upper", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        auto s = a[0].as_string();
        for (auto& c : s) c = static_cast<char>(std::toupper(c));
        return LumaValue(s);
        });

    luma_register_function(vm, "str_lower", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        auto s = a[0].as_string();
        for (auto& c : s) c = static_cast<char>(std::tolower(c));
        return LumaValue(s);
        });

    luma_register_function(vm, "str_sub", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        if (a.size() < 2) return {};
        auto& s = a[0].as_string();
        size_t start = static_cast<size_t>(a[1].as_number());
        size_t len = a.size() >= 3 ? static_cast<size_t>(a[2].as_number()) : std::string::npos;
        return LumaValue(s.substr(start, len));
        });

    luma_register_function(vm, "str_contains", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(a[0].as_string().find(a[1].as_string()) != std::string::npos);
        });

    luma_register_function(vm, "str_starts", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(a[0].as_string().starts_with(a[1].as_string()));
        });

    luma_register_function(vm, "str_ends", [](LumaVM*, std::span<LumaValue> a) -> LumaValue {
        return LumaValue(a[0].as_string().ends_with(a[1].as_string()));
        });

    luma_register_function(vm, "wait", [](LumaVM*, std::span<LumaValue> args) -> LumaValue {
        if (args.empty() || !args[0].is_number())
            throw std::runtime_error("wait(ms) expects a number");

        auto ms = static_cast<int>(args[0].as_number());
        if (ms < 0)
            ms = 0;

        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return {};
        });
}

static void print_error(std::string_view err) {
    std::cerr << red("✖ ") << bold(err) << '\n';
}

static bool is_incomplete(std::string_view src) {
    int braces = 0, parens = 0;
    bool in_str = false;

    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (c == '"' && (i == 0 || src[i - 1] != '\\')) in_str = !in_str;
        if (in_str) continue;
        if (c == '{') ++braces; else if (c == '}') --braces;
        if (c == '(') ++parens; else if (c == ')') --parens;
    }
    return braces > 0 || parens > 0;
}

static void run_repl(LumaVM* vm) {
    std::cout << bold(cyan("Luma")) << dim(" v0.1 - :help for help, :quit to exit\n");

    std::string buffer;
    bool multiline = false;

    while (true) {
        if (multiline)
            std::cout << dim("... ") << std::flush;
        else
            std::cout << cyan(">>> ") << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }

        if (!multiline) {
            if (line == ":quit" || line == ":q" || line == ":exit") break;

            if (line == ":help" || line == ":h") {
                std::cout
                    << bold("\nLuma REPL commands:\n")
                    << "  " << cyan(":help") << " / " << cyan(":h") << "    show help\n"
                    << "  " << cyan(":quit") << " / " << cyan(":q") << "    exit\n"
                    << "  " << cyan(":clear") << "            reset global state\n"
                    << "  " << cyan(":load <file>") << "     load and execute file\n"
                    << "  " << cyan(":fns") << "              list registered functions\n"
                    << bold("\nMultiline input:\n")
                    << "  If a line ends with { or brackets are open,\n"
                    << "  multiline mode is automatically enabled.\n"
                    << "  Empty line closes the block.\n\n";
                continue;
            }

            if (line == ":clear") {
                luma_destroy(vm);
                std::cout << yellow("⚠ :clear requires restart - restart the process.\n");
                continue;
            }

            if (line.starts_with(":load ")) {
                auto path = std::string(line.substr(6));
                while (!path.empty() && path.front() == ' ') path = path.substr(1);

                if (!luma_run_file(vm, path.c_str())) {
                    print_error(luma_last_error(vm));
                }
                else {
                    std::cout << green("✔ ") << path << " loaded.\n";
                }
                continue;
            }

            if (line == ":fns") {
                std::cout << bold("Available functions (host-registered):\n");

                std::vector<std::string> fns = {
                    "print","println","eprint","tostring","tonumber","typeof",
                    "len","push","pop","keys","has","assert","error","input",
                    "math_floor","math_ceil","math_round","math_abs","math_sqrt",
                    "math_pow","math_sin","math_cos","math_min","math_max","math_rand",
                    "str_upper","str_lower","str_sub","str_contains","str_starts","str_ends", "wait"
                };

                for (size_t i = 0; i < fns.size(); ++i) {
                    std::cout << "  " << cyan(fns[i]);
                    if ((i + 1) % 4 == 0) std::cout << '\n';
                    else std::cout << '\t';
                }
                std::cout << "\n\n";
                continue;
            }

            if (line.empty()) continue;
        }

        buffer += line + '\n';
        multiline = is_incomplete(buffer);

        if (multiline) {
            if (line.empty()) {
                multiline = false;
            }
            else {
                continue;
            }
        }

        if (buffer.empty() || buffer == "\n") {
            buffer.clear();
            continue;
        }

        std::string wrapped = "let __repl_result__ = (" + buffer.substr(0, buffer.size() - 1) + ")";
        bool is_expr = luma_run(vm, wrapped.c_str());

        if (is_expr) {
            auto result = luma_get_global(vm, "__repl_result__");
            if (!result.is_null()) {
                std::cout << dim("= ") << green(result.to_string()) << '\n';
            }
        }
        else {
            if (!luma_run(vm, buffer.c_str())) {
                print_error(luma_last_error(vm));
            }
        }

        buffer.clear();
        multiline = false;
    }

    std::cout << dim("\nGoodbye!\n");
}

static void print_help(std::string_view progname) {
    std::cout
        << bold("\nLuma - an embeddable scripting language\n\n")
        << bold("Usage:\n")
        << "  " << progname << "                  start REPL\n"
        << "  " << progname << " " << cyan("<file.luma>") << "   run script file\n"
        << "  " << progname << " " << cyan("-e <code>") << "     execute code snippet\n"
        << "  " << progname << " " << cyan("--help") << "         show help\n\n";
}

int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (std::getenv("NO_COLOR")) use_color = false;

    LumaVM* vm = luma_create();
    register_stdlib(vm);

    std::vector<std::string_view> args(argv + 1, argv + argc);

    if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        print_help(argv[0]);
        luma_destroy(vm);
        return 0;
    }

    if (args.size() >= 2 && args[0] == "-e") {
        std::string code(args[1]);
        if (!luma_run(vm, code.c_str())) {
            print_error(luma_last_error(vm));
            luma_destroy(vm);
            return 1;
        }
        luma_destroy(vm);
        return 0;
    }

    if (!args.empty() && args[0] != "-") {
        std::string path(args[0]);
        if (!luma_run_file(vm, path.c_str())) {
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