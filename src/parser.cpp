#include "parser.h"
#include <format>
#include <cassert>

namespace luma {

    Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    const Token& Parser::peek(int offset) const {
        size_t i = pos_ + offset;
        if (i >= tokens_.size()) return tokens_.back(); // Eof
        return tokens_[i];
    }

    const Token& Parser::advance() {
        if (!at_end()) ++pos_;
        return tokens_[pos_ - 1];
    }

    bool Parser::check(TokenKind k) const { return peek().kind == k; }

    bool Parser::match(TokenKind k) {
        if (check(k)) { advance(); return true; }
        return false;
    }

    const Token& Parser::expect(TokenKind k, const char* msg) {
        if (!check(k)) throw error(peek(), msg);
        return advance();
    }

    bool Parser::at_end() const { return peek().kind == TokenKind::Eof; }

    ParseError Parser::error(const Token& t, const std::string& msg) const {
        return ParseError(std::format("[line {}] Parse error at '{}': {}", t.line, t.value, msg));
    }

    Block Parser::parse() {
        Block stmts;
        while (!at_end()) {
            stmts.push_back(parse_stmt());
        }
        return stmts;
    }

    Stmt Parser::parse_stmt() {
        if (check(TokenKind::Let))    return parse_let();
        if (check(TokenKind::Fn))     return parse_fn_decl();
        if (check(TokenKind::Return)) return parse_return();
        if (check(TokenKind::If))     return parse_if();
        if (check(TokenKind::While))  return parse_while();
        if (check(TokenKind::For))    return parse_for();
        if (check(TokenKind::LBrace)) {
            int ln = peek().line;
            auto blk = std::make_unique<BlockStmt>();
            blk->line = ln;
            blk->stmts = parse_block();
            return blk;
        }

        // expression statement
        int ln = peek().line;
        auto expr = parse_expr();
        match(TokenKind::Semicolon);
        auto stmt = std::make_unique<ExprStmt>();
        stmt->line = ln;
        stmt->expr = std::make_unique<Expr>(std::move(expr));
        return stmt;
    }

    Stmt Parser::parse_let() {
        int ln = peek().line;
        advance(); // 'let'
        auto name = expect(TokenKind::Ident, "expected variable name").value;
        expect(TokenKind::Eq, "expected '=' after variable name");
        auto init = parse_expr();
        match(TokenKind::Semicolon);

        auto node = std::make_unique<LetStmt>();
        node->name = name;
        node->init = std::make_unique<Expr>(std::move(init));
        node->line = ln;
        return node;
    }

    Stmt Parser::parse_fn_decl() {
        int ln = peek().line;
        advance(); // 'fn'
        auto name = expect(TokenKind::Ident, "expected function name").value;
        auto params = parse_param_list();

        auto node = std::make_unique<FnDecl>();
        node->name = name;
        node->params = std::move(params);
        node->line = ln;

        if (match(TokenKind::Arrow)) {
            auto expr = parse_expr();
            match(TokenKind::Semicolon);
            node->bodyExpr = std::make_unique<Expr>(std::move(expr));
        }
        else {
            node->body = std::make_unique<Block>(parse_block());
        }
        return node;
    }

    Stmt Parser::parse_return() {
        int ln = peek().line;
        advance(); // 'return'
        auto node = std::make_unique<ReturnStmt>();
        node->line = ln;

        if (!check(TokenKind::RBrace) && !check(TokenKind::Semicolon) && !at_end()) {
            auto expr = parse_expr();
            node->value = std::make_unique<Expr>(std::move(expr));
        }
        match(TokenKind::Semicolon);
        return node;
    }

    Stmt Parser::parse_if() {
        int ln = peek().line;
        advance(); // 'if'
        auto cond = parse_expr();
        auto then_block = std::make_unique<Block>(parse_block());

        std::unique_ptr<Block> else_block;
        if (match(TokenKind::Else)) {
            if (check(TokenKind::If)) {
                // else if → wrap in block
                auto inner = parse_stmt();
                else_block = std::make_unique<Block>();
                else_block->push_back(std::move(inner));
            }
            else {
                else_block = std::make_unique<Block>(parse_block());
            }
        }

        auto node = std::make_unique<IfStmt>();
        node->condition = std::make_unique<Expr>(std::move(cond));
        node->thenBlock = std::move(then_block);
        node->elseBlock = std::move(else_block);
        node->line = ln;
        return node;
    }

    Stmt Parser::parse_while() {
        int ln = peek().line;
        advance(); // 'while'
        auto cond = parse_expr();
        auto body = std::make_unique<Block>(parse_block());

        auto node = std::make_unique<WhileStmt>();
        node->condition = std::make_unique<Expr>(std::move(cond));
        node->body = std::move(body);
        node->line = ln;
        return node;
    }

    Stmt Parser::parse_for() {
        int ln = peek().line;
        advance(); // 'for'
        auto var = expect(TokenKind::Ident, "expected loop variable").value;
        expect(TokenKind::In, "expected 'in' after loop variable");
        auto iter = parse_expr();
        auto body = std::make_unique<Block>(parse_block());

        auto node = std::make_unique<ForStmt>();
        node->var = var;
        node->iterable = std::make_unique<Expr>(std::move(iter));
        node->body = std::move(body);
        node->line = ln;
        return node;
    }

    Block Parser::parse_block() {
        expect(TokenKind::LBrace, "expected '{'");
        Block stmts;
        while (!check(TokenKind::RBrace) && !at_end()) {
            stmts.push_back(parse_stmt());
        }
        expect(TokenKind::RBrace, "expected '}'");
        return stmts;
    }

    Expr Parser::parse_expr() { return parse_assign(); }

    Expr Parser::parse_assign() {
        auto expr = parse_or();
        if (check(TokenKind::Eq)) {
            int ln = peek().line;
            advance();
            auto val = parse_assign();
            auto node = std::make_unique<AssignExpr>();
            node->target = std::make_unique<Expr>(std::move(expr));
            node->value = std::make_unique<Expr>(std::move(val));
            node->line = ln;
            return node;
        }
        return expr;
    }

    Expr Parser::parse_or() {
        auto left = parse_and();
        while (check(TokenKind::Or)) {
            int ln = peek().line;
            advance();
            auto right = parse_and();
            auto node = std::make_unique<BinaryExpr>();
            node->op = "||";
            node->left = std::make_unique<Expr>(std::move(left));
            node->right = std::make_unique<Expr>(std::move(right));
            node->line = ln;
            left = std::move(node);
        }
        return left;
    }

    Expr Parser::parse_and() {
        auto left = parse_equality();
        while (check(TokenKind::And)) {
            int ln = peek().line;
            advance();
            auto right = parse_equality();
            auto node = std::make_unique<BinaryExpr>();
            node->op = "&&";
            node->left = std::make_unique<Expr>(std::move(left));
            node->right = std::make_unique<Expr>(std::move(right));
            node->line = ln;
            left = std::move(node);
        }
        return left;
    }

    Expr Parser::parse_equality() {
        auto left = parse_comparison();
        while (check(TokenKind::EqEq) || check(TokenKind::BangEq)) {
            int ln = peek().line;
            std::string op = peek().kind == TokenKind::EqEq ? "==" : "!=";
            advance();
            auto right = parse_comparison();
            auto node = std::make_unique<BinaryExpr>();
            node->op = op;
            node->left = std::make_unique<Expr>(std::move(left));
            node->right = std::make_unique<Expr>(std::move(right));
            node->line = ln;
            left = std::move(node);
        }
        return left;
    }

    Expr Parser::parse_comparison() {
        auto left = parse_additive();
        while (check(TokenKind::Lt) || check(TokenKind::LtEq) ||
            check(TokenKind::Gt) || check(TokenKind::GtEq)) {
            int ln = peek().line;
            std::string op;
            switch (peek().kind) {
            case TokenKind::Lt:   op = "<";  break;
            case TokenKind::LtEq: op = "<="; break;
            case TokenKind::Gt:   op = ">";  break;
            case TokenKind::GtEq: op = ">="; break;
            default: break;
            }
            advance();
            auto right = parse_additive();
            auto node = std::make_unique<BinaryExpr>();
            node->op = op;
            node->left = std::make_unique<Expr>(std::move(left));
            node->right = std::make_unique<Expr>(std::move(right));
            node->line = ln;
            left = std::move(node);
        }
        return left;
    }

    Expr Parser::parse_additive() {
        auto left = parse_multiplicative();
        while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
            int ln = peek().line;
            std::string op = peek().kind == TokenKind::Plus ? "+" : "-";
            advance();
            auto right = parse_multiplicative();
            auto node = std::make_unique<BinaryExpr>();
            node->op = op;
            node->left = std::make_unique<Expr>(std::move(left));
            node->right = std::make_unique<Expr>(std::move(right));
            node->line = ln;
            left = std::move(node);
        }
        return left;
    }

    Expr Parser::parse_multiplicative() {
        auto left = parse_unary();
        while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
            int ln = peek().line;
            std::string op;
            switch (peek().kind) {
            case TokenKind::Star:    op = "*"; break;
            case TokenKind::Slash:   op = "/"; break;
            case TokenKind::Percent: op = "%"; break;
            default: break;
            }
            advance();
            auto right = parse_unary();
            auto node = std::make_unique<BinaryExpr>();
            node->op = op;
            node->left = std::make_unique<Expr>(std::move(left));
            node->right = std::make_unique<Expr>(std::move(right));
            node->line = ln;
            left = std::move(node);
        }
        return left;
    }

    Expr Parser::parse_unary() {
        if (check(TokenKind::Bang) || check(TokenKind::Minus)) {
            int ln = peek().line;
            std::string op = peek().kind == TokenKind::Bang ? "!" : "-";
            advance();
            auto operand = parse_unary();
            auto node = std::make_unique<UnaryExpr>();
            node->op = op;
            node->operand = std::make_unique<Expr>(std::move(operand));
            node->line = ln;
            return node;
        }
        auto primary = parse_primary();
        return parse_postfix(std::move(primary));
    }

    Expr Parser::parse_postfix(Expr base) {
        while (true) {
            if (check(TokenKind::LParen)) {
                int ln = peek().line;
                auto args = parse_arg_list();
                auto node = std::make_unique<CallExpr>();
                node->callee = std::make_unique<Expr>(std::move(base));
                node->args = std::move(args);
                node->line = ln;
                base = std::move(node);
            }
            else if (check(TokenKind::LBracket)) {
                int ln = peek().line;
                advance();
                auto idx = parse_expr();
                expect(TokenKind::RBracket, "expected ']'");
                auto node = std::make_unique<IndexExpr>();
                node->object = std::make_unique<Expr>(std::move(base));
                node->index = std::make_unique<Expr>(std::move(idx));
                node->line = ln;
                base = std::move(node);
            }
            else if (check(TokenKind::Dot)) {
                int ln = peek().line;
                advance();
                auto field = expect(TokenKind::Ident, "expected field name").value;
                auto node = std::make_unique<FieldExpr>();
                node->object = std::make_unique<Expr>(std::move(base));
                node->field = field;
                node->line = ln;
                base = std::move(node);
            }
            else break;
        }
        return base;
    }

    Expr Parser::parse_primary() {
        const auto& t = peek();
        int ln = t.line;

        if (match(TokenKind::Number)) {
            auto node = std::make_unique<NumberLit>();
            node->value = std::stod(tokens_[pos_ - 1].value);
            node->line = ln;
            return node;
        }
        if (match(TokenKind::String)) {
            auto node = std::make_unique<StringLit>();
            node->value = tokens_[pos_ - 1].value;
            node->line = ln;
            return node;
        }
        if (match(TokenKind::True)) {
            auto node = std::make_unique<BoolLit>();
            node->value = true; node->line = ln;
            return node;
        }
        if (match(TokenKind::False)) {
            auto node = std::make_unique<BoolLit>();
            node->value = false; node->line = ln;
            return node;
        }
        if (match(TokenKind::Null)) {
            auto node = std::make_unique<NullLit>();
            node->line = ln;
            return node;
        }
        if (check(TokenKind::Ident)) {
            auto name = advance().value;
            auto node = std::make_unique<Identifier>();
            node->name = name; node->line = ln;
            return node;
        }
        if (check(TokenKind::Fn)) return parse_fn_expr();
        if (check(TokenKind::LBracket)) return parse_list();
        if (check(TokenKind::LBrace))   return parse_map();
        if (match(TokenKind::LParen)) {
            auto expr = parse_expr();
            expect(TokenKind::RParen, "expected ')'");
            return expr;
        }
        throw error(peek(), "unexpected token in expression");
    }

    Expr Parser::parse_fn_expr() {
        int ln = peek().line;
        advance(); // 'fn'
        auto params = parse_param_list();
        auto node = std::make_unique<FnExpr>();
        node->params = std::move(params);
        node->line = ln;
        if (match(TokenKind::Arrow)) {
            auto expr = parse_expr();
            node->bodyExpr = std::make_unique<Expr>(std::move(expr));
        }
        else {
            node->body = std::make_unique<Block>(parse_block());
        }
        return node;
    }

    Expr Parser::parse_list() {
        int ln = peek().line;
        advance(); // '['
        auto node = std::make_unique<ListLit>();
        node->line = ln;
        while (!check(TokenKind::RBracket) && !at_end()) {
            node->elements.push_back(parse_expr());
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBracket, "expected ']'");
        return node;
    }

    Expr Parser::parse_map() {
        int ln = peek().line;
        advance(); // '{'
        auto node = std::make_unique<MapLit>();
        node->line = ln;
        while (!check(TokenKind::RBrace) && !at_end()) {
            auto key = expect(TokenKind::Ident, "expected map key").value;
            expect(TokenKind::Colon, "expected ':' after map key");
            auto val = parse_expr();
            node->entries.emplace_back(key, std::move(val));
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBrace, "expected '}'");
        return node;
    }

    std::vector<Expr> Parser::parse_arg_list() {
        advance(); // '('
        std::vector<Expr> args;
        while (!check(TokenKind::RParen) && !at_end()) {
            args.push_back(parse_expr());
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "expected ')'");
        return args;
    }

    std::vector<std::string> Parser::parse_param_list() {
        expect(TokenKind::LParen, "expected '('");
        std::vector<std::string> params;
        while (!check(TokenKind::RParen) && !at_end()) {
            params.push_back(expect(TokenKind::Ident, "expected parameter name").value);
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "expected ')'");
        return params;
    }

} // namespace luma