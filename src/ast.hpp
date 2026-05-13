#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mc {

struct SourceLocation {
    int line = 1;
    int column = 1;
};

enum class TypeKind {
    Int,
    Bool
};

const char *typeName(TypeKind type);

struct Expr {
    SourceLocation loc;
    TypeKind inferredType = TypeKind::Int;
    virtual ~Expr() = default;
};

struct IntLiteral final : Expr {
    explicit IntLiteral(std::int64_t value) : value(value) {}
    std::int64_t value;
};

struct VarExpr final : Expr {
    explicit VarExpr(std::string name) : name(std::move(name)) {}
    std::string name;
};

enum class UnaryOp {
    Neg,
    Not
};

struct UnaryExpr final : Expr {
    UnaryExpr(UnaryOp op, std::unique_ptr<Expr> operand)
        : op(op), operand(std::move(operand)) {}

    UnaryOp op;
    std::unique_ptr<Expr> operand;
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or
};

struct BinaryExpr final : Expr {
    BinaryExpr(BinaryOp op, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    BinaryOp op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
};

struct AssignExpr final : Expr {
    AssignExpr(std::string name, std::unique_ptr<Expr> value)
        : name(std::move(name)), value(std::move(value)) {}

    std::string name;
    std::unique_ptr<Expr> value;
};

struct TernaryExpr final : Expr {
    TernaryExpr(std::unique_ptr<Expr> condition,
                std::unique_ptr<Expr> thenExpr,
                std::unique_ptr<Expr> elseExpr)
        : condition(std::move(condition)),
          thenExpr(std::move(thenExpr)),
          elseExpr(std::move(elseExpr)) {}

    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> thenExpr;
    std::unique_ptr<Expr> elseExpr;
};

struct CallExpr final : Expr {
    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), args(std::move(args)) {}

    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct Stmt {
    SourceLocation loc;
    virtual ~Stmt() = default;
};

struct BlockStmt final : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct VarDeclStmt final : Stmt {
    VarDeclStmt(TypeKind type, std::string name, std::unique_ptr<Expr> init)
        : type(type), name(std::move(name)), init(std::move(init)) {}

    TypeKind type;
    std::string name;
    std::unique_ptr<Expr> init;
};

struct ExprStmt final : Stmt {
    explicit ExprStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {}
    std::unique_ptr<Expr> expr;
};

struct ReturnStmt final : Stmt {
    explicit ReturnStmt(std::unique_ptr<Expr> value) : value(std::move(value)) {}
    std::unique_ptr<Expr> value;
};

struct IfStmt final : Stmt {
    IfStmt(std::unique_ptr<Expr> condition,
           std::unique_ptr<BlockStmt> thenBlock,
           std::unique_ptr<BlockStmt> elseBlock)
        : condition(std::move(condition)),
          thenBlock(std::move(thenBlock)),
          elseBlock(std::move(elseBlock)) {}

    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock;
};

struct ForStmt final : Stmt {
    ForStmt(std::unique_ptr<Stmt> init,
            std::unique_ptr<Expr> condition,
            std::unique_ptr<Expr> step,
            std::unique_ptr<BlockStmt> body)
        : init(std::move(init)),
          condition(std::move(condition)),
          step(std::move(step)),
          body(std::move(body)) {}

    std::unique_ptr<Stmt> init;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> step;
    std::unique_ptr<BlockStmt> body;
};

struct Param {
    TypeKind type;
    std::string name;
};

struct FunctionDecl {
    FunctionDecl(TypeKind returnType,
                 std::string name,
                 std::vector<Param> params,
                 std::unique_ptr<BlockStmt> body)
        : returnType(returnType),
          name(std::move(name)),
          params(std::move(params)),
          body(std::move(body)) {}

    TypeKind returnType;
    SourceLocation loc;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
};

struct Program {
    std::vector<std::unique_ptr<FunctionDecl>> functions;
};

using ExprList = std::vector<std::unique_ptr<Expr>>;
using ParamList = std::vector<Param>;
using StmtList = std::vector<std::unique_ptr<Stmt>>;
using FunctionList = std::vector<std::unique_ptr<FunctionDecl>>;

} // namespace mc
