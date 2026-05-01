#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace luma {
    struct NumberLit;
    struct StringLit;
    struct BoolLit;
    struct NullLit;
    struct ListLit;
    struct MapLit;
    struct Identifier;
    struct BinaryExpr;
    struct UnaryExpr;
    struct AssignExpr;
    struct CallExpr;
    struct IndexExpr;
    struct FieldExpr;
    struct FnExpr;

    struct LetStmt;
    struct ReturnStmt;
    struct ExprStmt;
    struct BlockStmt;
    struct IfStmt;
    struct WhileStmt;
    struct ForStmt;
    struct FnDecl;

    using ExprNode = std::variant<
        std::unique_ptr<NumberLit>,
        std::unique_ptr<StringLit>,
        std::unique_ptr<BoolLit>,
        std::unique_ptr<NullLit>,
        std::unique_ptr<ListLit>,
        std::unique_ptr<MapLit>,
        std::unique_ptr<Identifier>,
        std::unique_ptr<BinaryExpr>,
        std::unique_ptr<UnaryExpr>,
        std::unique_ptr<AssignExpr>,
        std::unique_ptr<CallExpr>,
        std::unique_ptr<IndexExpr>,
        std::unique_ptr<FieldExpr>,
        std::unique_ptr<FnExpr>
    >;

    using StmtNode = std::variant<
        std::unique_ptr<LetStmt>,
        std::unique_ptr<ReturnStmt>,
        std::unique_ptr<ExprStmt>,
        std::unique_ptr<BlockStmt>,
        std::unique_ptr<IfStmt>,
        std::unique_ptr<WhileStmt>,
        std::unique_ptr<ForStmt>,
        std::unique_ptr<FnDecl>
    >;

    using Expr = ExprNode;
    using Stmt = StmtNode;
    using Block = std::vector<Stmt>;

    struct NumberLit { double value; int line; };
    struct StringLit { std::string value; int line; };
    struct BoolLit { bool value; int line; };
    struct NullLit { int line; };

    struct ListLit {
        std::vector<Expr> elements;
        int line;
    };

    struct MapLit {
        std::vector<std::pair<std::string, Expr>> entries;
        int line;
    };

    struct Identifier {
        std::string name;
        int line;
    };

    struct BinaryExpr {
        std::string op;
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        int line;
    };

    struct UnaryExpr {
        std::string op;
        std::unique_ptr<Expr> operand;
        int line;
    };

    struct AssignExpr {
        std::unique_ptr<Expr> target;
        std::unique_ptr<Expr> value;
        int line;
    };

    struct CallExpr {
        std::unique_ptr<Expr> callee;
        std::vector<Expr> args;
        int line;
    };

    struct IndexExpr {
        std::unique_ptr<Expr> object;
        std::unique_ptr<Expr> index;
        int line;
    };

    struct FieldExpr {
        std::unique_ptr<Expr> object;
        std::string field;
        int line;
    };

    struct FnExpr {
        std::vector<std::string> params;
        std::unique_ptr<Expr> bodyExpr;
        std::unique_ptr<Block> body;
        int line;
    };

    struct LetStmt {
        std::string name;
        std::unique_ptr<Expr> init;
        int line;
    };

    struct ReturnStmt {
        std::unique_ptr<Expr> value;
        int line;
    };

    struct ExprStmt {
        std::unique_ptr<Expr> expr;
        int line;
    };

    struct BlockStmt {
        Block stmts;
        int line;
    };

    struct IfStmt {
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Block> thenBlock;
        std::unique_ptr<Block> elseBlock;
        int line;
    };

    struct WhileStmt {
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Block> body;
        int line;
    };

    struct ForStmt {
        std::string var;
        std::unique_ptr<Expr> iterable;
        std::unique_ptr<Block> body;
        int line;
    };

    struct FnDecl {
        std::string name;
        std::vector<std::string> params;
        std::unique_ptr<Expr> bodyExpr;
        std::unique_ptr<Block> body;
        int line;
    };

} // namespace luma