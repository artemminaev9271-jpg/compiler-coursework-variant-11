#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

namespace mc {

struct FunctionSignature {
    TypeKind returnType;
    std::vector<TypeKind> params;
};

class SemanticAnalyzer {
public:
    bool analyze(Program &program);
    const std::vector<std::string> &errors() const { return errors_; }

private:
    TypeKind analyzeExpr(Expr &expr);
    bool analyzeStmt(Stmt &stmt);
    bool analyzeBlock(BlockStmt &block, bool createScope);
    bool statementReturns(const Stmt &stmt) const;
    bool blockReturns(const BlockStmt &block) const;

    void pushScope();
    void popScope();
    bool declare(const std::string &name, TypeKind type);
    const TypeKind *lookup(const std::string &name) const;
    void error(const std::string &message);
    void errorAt(SourceLocation loc, const std::string &message);
    bool require(TypeKind actual, TypeKind expected, const std::string &where);
    bool requireAt(TypeKind actual, TypeKind expected, SourceLocation loc, const std::string &where);

    std::vector<std::unordered_map<std::string, TypeKind>> scopes_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    TypeKind currentReturnType_ = TypeKind::Int;
    std::vector<std::string> errors_;
};

} // namespace mc
