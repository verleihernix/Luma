#pragma once
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <span>
#include <stdexcept>
#include <cmath>

struct LumaVM;
struct LumaValue;

using LumaList = std::vector<LumaValue>;
using LumaMap = std::unordered_map<std::string, LumaValue>;
using LumaNativeFn = std::function<LumaValue(LumaVM*, std::span<LumaValue>)>;

struct LumaValue {
    using Variant = std::variant<
        std::monostate, bool, double, std::string,
        std::shared_ptr<LumaList>,
        std::shared_ptr<LumaMap>,
        LumaNativeFn
    >;
    Variant data;

    LumaValue() : data(std::monostate{}) {}
    explicit LumaValue(bool v) : data(v) {}
    explicit LumaValue(double v) : data(v) {}
    explicit LumaValue(std::string v) : data(std::move(v)) {}
    explicit LumaValue(std::shared_ptr<LumaList> v) : data(std::move(v)) {}
    explicit LumaValue(std::shared_ptr<LumaMap>  v) : data(std::move(v)) {}
    explicit LumaValue(LumaNativeFn v) : data(std::move(v)) {}

    [[nodiscard]]  inline bool is_null()     const { return std::holds_alternative<std::monostate>(data); }
    [[nodiscard]]  inline bool is_bool()     const { return std::holds_alternative<bool>(data); }
    [[nodiscard]]  inline bool is_number()   const { return std::holds_alternative<double>(data); }
    [[nodiscard]]  inline bool is_string()   const { return std::holds_alternative<std::string>(data); }
    [[nodiscard]]  inline bool is_list()     const { return std::holds_alternative<std::shared_ptr<LumaList>>(data); }
    [[nodiscard]]  inline bool is_map()      const { return std::holds_alternative<std::shared_ptr<LumaMap>>(data); }
    [[nodiscard]]  inline bool is_function() const { return std::holds_alternative<LumaNativeFn>(data); }

    [[nodiscard]] inline bool                      as_bool()   const { return std::get<bool>(data); }
    [[nodiscard]] inline double                    as_number() const { return std::get<double>(data); }
    [[nodiscard]] inline const std::string&        as_string() const { return std::get<std::string>(data); }
    [[nodiscard]] inline std::shared_ptr<LumaList> as_list() const { return std::get<std::shared_ptr<LumaList>>(data); }
    [[nodiscard]] inline std::shared_ptr<LumaMap>  as_map()  const { return std::get<std::shared_ptr<LumaMap>>(data); }
    [[nodiscard]] inline const LumaNativeFn&       as_fn() const { return std::get<LumaNativeFn>(data); }

    [[nodiscard]] bool truthy() const {
        return std::visit([](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) return false;
            else if constexpr (std::is_same_v<T, bool>)   return v;
            else if constexpr (std::is_same_v<T, double>)  return v != 0.0;
            else if constexpr (std::is_same_v<T, std::string>) return !v.empty();
            else return true;
            }, data);
    }

    std::string to_string() const {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) return "null";
            else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
            else if constexpr (std::is_same_v<T, double>) {
                if (v == std::floor(v) && std::abs(v) < 1e15)
                    return std::to_string(static_cast<long long>(v));
                return std::to_string(v);
            }
            else if constexpr (std::is_same_v<T, std::string>) return v;
            else if constexpr (std::is_same_v<T, std::shared_ptr<LumaList>>) {
                std::string s = "[";
                for (size_t i = 0; i < v->size(); ++i) {
                    if (i > 0) s += ", ";
                    s += (*v)[i].to_string();
                }
                return s + "]";
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<LumaMap>>) {
                std::string s = "{";
                bool first = true;
                for (auto& [k, val] : *v) {
                    if (!first) s += ", ";
                    s += k + ": " + val.to_string();
                    first = false;
                }
                return s + "}";
            }
            else return "<fn>";
            }, data);
    }

    bool operator==(const LumaValue& o) const {
        return std::visit([&](const auto& a) -> bool {
            using T = std::decay_t<decltype(a)>;
            if (!std::holds_alternative<T>(o.data)) return false;
            const auto& b = std::get<T>(o.data);
            if constexpr (std::is_same_v<T, LumaNativeFn>) return false;
            else return a == b;
            }, data);
    }
};

extern "C" {
    LumaVM* luma_create();
    void        luma_destroy(LumaVM* vm);
    void        luma_register_function(LumaVM* vm, const char* name, LumaNativeFn fn);
    bool        luma_run(LumaVM* vm, const char* source);
    bool        luma_run_file(LumaVM* vm, const char* path);
    const char* luma_last_error(LumaVM* vm);
    LumaValue   luma_call(LumaVM* vm, const char* name, std::span<LumaValue> args);
    LumaValue   luma_get_global(LumaVM* vm, const char* name);
    void        luma_set_global(LumaVM* vm, const char* name, LumaValue value);
}