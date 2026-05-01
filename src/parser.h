#pragma once
#include "lexer.h"
#include "ast.h"
#include <vector>
#include <string>
#include <stdexcept>

namespace luma {

    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
    };

    class Parser {
    public:
        explicit Parser(std::vector<Token> tokens);
        Block parse();

    private:
        Stmt parse_stmt();
        Stmt parse_let();
        Stmt parse_fn_decl();
        Stmt parse_return();
        Stmt parse_if();
        Stmt parse_while();
        Stmt parse_for();
        Block parse_block();

        Expr parse_expr();
        Expr parse_assign();
        Expr parse_or();
        Expr parse_and();
        Expr parse_equality();
        Expr parse_comparison();
        Expr parse_additive();
        Expr parse_multiplicative();
        Expr parse_unary();
        Expr parse_postfix(Expr base);
        Expr parse_primary();
        Expr parse_fn_expr();
        Expr parse_list();
        Expr parse_map();

        std::vector<Expr> parse_arg_list();
        std::vector<std::string> parse_param_list();

        const Token& peek(int offset = 0) const;
        const Token& advance();
        bool check(TokenKind k) const;
        bool match(TokenKind k);
        const Token& expect(TokenKind k, const char* msg);
        bool at_end() const;

        ParseError error(const Token& t, const std::string& msg) const;

        std::vector<Token> tokens_;
        size_t pos_ = 0;
    };

} // namespace luma