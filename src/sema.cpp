#include "sema.hpp"

#include <sstream>

namespace mc {

bool SemanticAnalyzer::analyze(Program &program) {
    errors_.clear();
    functions_.clear();
    scopes_.clear();

    for (const auto &fn : program.functions) {
        if (functions_.count(fn->name)) {
            errorAt(fn->loc, "function '" + fn->name + "' is already declared");
            continue;
        }

        FunctionSignature sig{fn->returnType, {}};
        for (const auto &param : fn->params) {
            sig.params.push_back(param.type);
        }
        functions_[fn->name] = std::move(sig);
    }

    auto compiled = functions_.find("compiled_fn");
    if (compiled == functions_.end()) {
        error("required function 'compiled_fn' is missing");
    } else {
        if (compiled->second.returnType != TypeKind::Int || compiled->second.params.size() != 1 ||
            compiled->second.params[0] != TypeKind::Int) {
            error("compiled_fn must have signature: int compiled_fn(int arg)");
        }
    }

    for (const auto &fn : program.functions) {
        pushScope();
        currentReturnType_ = fn->returnType;

        for (const auto &param : fn->params) {
            declare(param.name, param.type);
        }

        analyzeBlock(*fn->body, false);
        if (!blockReturns(*fn->body)) {
            errorAt(fn->loc, "function '" + fn->name + "' may finish without return");
        }

        popScope();
    }

    return errors_.empty();
}

TypeKind SemanticAnalyzer::analyzeExpr(Expr &expr) {
    if (auto *literal = dynamic_cast<IntLiteral *>(&expr)) {
        (void)literal;
        expr.inferredType = TypeKind::Int;
        return expr.inferredType;
    }

    if (auto *var = dynamic_cast<VarExpr *>(&expr)) {
        const TypeKind *type = lookup(var->name);
        if (!type) {
            errorAt(var->loc, "use of undeclared variable '" + var->name + "'");
            expr.inferredType = TypeKind::Int;
            return expr.inferredType;
        }
        expr.inferredType = *type;
        return expr.inferredType;
    }

    if (auto *unary = dynamic_cast<UnaryExpr *>(&expr)) {
        TypeKind operand = analyzeExpr(*unary->operand);
        if (unary->op == UnaryOp::Neg) {
            requireAt(operand, TypeKind::Int, unary->loc, "unary '-'");
            expr.inferredType = TypeKind::Int;
        } else {
            requireAt(operand, TypeKind::Bool, unary->loc, "unary '!'");
            expr.inferredType = TypeKind::Bool;
        }
        return expr.inferredType;
    }

    if (auto *binary = dynamic_cast<BinaryExpr *>(&expr)) {
        TypeKind lhs = analyzeExpr(*binary->lhs);
        TypeKind rhs = analyzeExpr(*binary->rhs);

        switch (binary->op) {
        case BinaryOp::Add:
        case BinaryOp::Sub:
        case BinaryOp::Mul:
        case BinaryOp::Div:
        case BinaryOp::Mod:
            requireAt(lhs, TypeKind::Int, binary->lhs->loc, "left operand of arithmetic operator");
            requireAt(rhs, TypeKind::Int, binary->rhs->loc, "right operand of arithmetic operator");
            expr.inferredType = TypeKind::Int;
            break;
        case BinaryOp::Eq:
        case BinaryOp::Ne:
            if (lhs != rhs) {
                errorAt(binary->loc, "operands of equality operator must have the same type");
            }
            expr.inferredType = TypeKind::Bool;
            break;
        case BinaryOp::Lt:
        case BinaryOp::Le:
        case BinaryOp::Gt:
        case BinaryOp::Ge:
            requireAt(lhs, TypeKind::Int, binary->lhs->loc, "left operand of comparison operator");
            requireAt(rhs, TypeKind::Int, binary->rhs->loc, "right operand of comparison operator");
            expr.inferredType = TypeKind::Bool;
            break;
        case BinaryOp::And:
        case BinaryOp::Or:
            requireAt(lhs, TypeKind::Bool, binary->lhs->loc, "left operand of logical operator");
            requireAt(rhs, TypeKind::Bool, binary->rhs->loc, "right operand of logical operator");
            expr.inferredType = TypeKind::Bool;
            break;
        }
        return expr.inferredType;
    }

    if (auto *assign = dynamic_cast<AssignExpr *>(&expr)) {
        const TypeKind *target = lookup(assign->name);
        if (!target) {
            errorAt(assign->loc, "assignment to undeclared variable '" + assign->name + "'");
            expr.inferredType = TypeKind::Int;
            return expr.inferredType;
        }

        TypeKind value = analyzeExpr(*assign->value);
        if (*target != value) {
            errorAt(assign->loc, "cannot assign " + std::string(typeName(value)) + " to variable '" +
                                     assign->name + "' of type " + typeName(*target));
        }
        expr.inferredType = *target;
        return expr.inferredType;
    }

    if (auto *ternary = dynamic_cast<TernaryExpr *>(&expr)) {
        TypeKind condition = analyzeExpr(*ternary->condition);
        requireAt(condition, TypeKind::Bool, ternary->condition->loc, "ternary condition");

        TypeKind thenType = analyzeExpr(*ternary->thenExpr);
        TypeKind elseType = analyzeExpr(*ternary->elseExpr);
        if (thenType != elseType) {
            errorAt(ternary->loc, "ternary branches must have the same type");
        }
        expr.inferredType = thenType;
        return expr.inferredType;
    }

    if (auto *call = dynamic_cast<CallExpr *>(&expr)) {
        auto found = functions_.find(call->callee);
        if (found == functions_.end()) {
            errorAt(call->loc, "call to undeclared function '" + call->callee + "'");
            expr.inferredType = TypeKind::Int;
            return expr.inferredType;
        }

        const auto &sig = found->second;
        if (sig.params.size() != call->args.size()) {
            std::ostringstream os;
            os << "function '" << call->callee << "' expects " << sig.params.size()
               << " argument(s), got " << call->args.size();
            errorAt(call->loc, os.str());
        }

        const std::size_t count = std::min(sig.params.size(), call->args.size());
        for (std::size_t i = 0; i < count; ++i) {
            TypeKind actual = analyzeExpr(*call->args[i]);
            if (actual != sig.params[i]) {
                errorAt(call->args[i]->loc, "argument " + std::to_string(i + 1) + " of '" + call->callee +
                                               "' has type " + typeName(actual) + ", expected " +
                                               typeName(sig.params[i]));
            }
        }
        for (std::size_t i = count; i < call->args.size(); ++i) {
            analyzeExpr(*call->args[i]);
        }

        expr.inferredType = sig.returnType;
        return expr.inferredType;
    }

    error("internal semantic error: unknown expression node");
    expr.inferredType = TypeKind::Int;
    return expr.inferredType;
}

bool SemanticAnalyzer::analyzeStmt(Stmt &stmt) {
    if (auto *block = dynamic_cast<BlockStmt *>(&stmt)) {
        analyzeBlock(*block, true);
        return true;
    }

    if (auto *decl = dynamic_cast<VarDeclStmt *>(&stmt)) {
        if (!declare(decl->name, decl->type)) {
            errorAt(decl->loc, "variable '" + decl->name + "' is already declared in this scope");
        }
        if (decl->init) {
            TypeKind initType = analyzeExpr(*decl->init);
            if (initType != decl->type) {
                errorAt(decl->loc, "initializer for variable '" + decl->name + "' has type " +
                                       typeName(initType) + ", expected " + typeName(decl->type));
            }
        }
        return true;
    }

    if (auto *exprStmt = dynamic_cast<ExprStmt *>(&stmt)) {
        analyzeExpr(*exprStmt->expr);
        return true;
    }

    if (auto *ret = dynamic_cast<ReturnStmt *>(&stmt)) {
        TypeKind value = analyzeExpr(*ret->value);
        if (value != currentReturnType_) {
            errorAt(ret->loc, "return expression has type " + std::string(typeName(value)) +
                                  ", expected " + typeName(currentReturnType_));
        }
        return true;
    }

    if (auto *ifStmt = dynamic_cast<IfStmt *>(&stmt)) {
        TypeKind condition = analyzeExpr(*ifStmt->condition);
        requireAt(condition, TypeKind::Bool, ifStmt->condition->loc, "if condition");
        analyzeBlock(*ifStmt->thenBlock, true);
        if (ifStmt->elseBlock) {
            analyzeBlock(*ifStmt->elseBlock, true);
        }
        return true;
    }

    if (auto *forStmt = dynamic_cast<ForStmt *>(&stmt)) {
        pushScope();
        if (forStmt->init) {
            analyzeStmt(*forStmt->init);
        }
        if (forStmt->condition) {
            TypeKind condition = analyzeExpr(*forStmt->condition);
            requireAt(condition, TypeKind::Bool, forStmt->condition->loc, "for condition");
        }
        if (forStmt->step) {
            analyzeExpr(*forStmt->step);
        }
        analyzeBlock(*forStmt->body, true);
        popScope();
        return true;
    }

    error("internal semantic error: unknown statement node");
    return false;
}

bool SemanticAnalyzer::analyzeBlock(BlockStmt &block, bool createScope) {
    if (createScope) {
        pushScope();
    }
    for (auto &stmt : block.statements) {
        analyzeStmt(*stmt);
    }
    if (createScope) {
        popScope();
    }
    return true;
}

bool SemanticAnalyzer::statementReturns(const Stmt &stmt) const {
    if (dynamic_cast<const ReturnStmt *>(&stmt)) {
        return true;
    }
    if (const auto *block = dynamic_cast<const BlockStmt *>(&stmt)) {
        return blockReturns(*block);
    }
    if (const auto *ifStmt = dynamic_cast<const IfStmt *>(&stmt)) {
        return ifStmt->elseBlock && blockReturns(*ifStmt->thenBlock) && blockReturns(*ifStmt->elseBlock);
    }
    return false;
}

bool SemanticAnalyzer::blockReturns(const BlockStmt &block) const {
    for (const auto &stmt : block.statements) {
        if (statementReturns(*stmt)) {
            return true;
        }
    }
    return false;
}

void SemanticAnalyzer::pushScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::popScope() {
    scopes_.pop_back();
}

bool SemanticAnalyzer::declare(const std::string &name, TypeKind type) {
    auto &scope = scopes_.back();
    if (scope.count(name)) {
        return false;
    }
    scope[name] = type;
    return true;
}

const TypeKind *SemanticAnalyzer::lookup(const std::string &name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

void SemanticAnalyzer::error(const std::string &message) {
    errors_.push_back(message);
}

void SemanticAnalyzer::errorAt(SourceLocation loc, const std::string &message) {
    errors_.push_back("line " + std::to_string(loc.line) + ", column " + std::to_string(loc.column) +
                      ": " + message);
}

bool SemanticAnalyzer::require(TypeKind actual, TypeKind expected, const std::string &where) {
    if (actual == expected) {
        return true;
    }
    error(where + " must have type " + typeName(expected) + ", got " + typeName(actual));
    return false;
}

bool SemanticAnalyzer::requireAt(TypeKind actual, TypeKind expected, SourceLocation loc, const std::string &where) {
    if (actual == expected) {
        return true;
    }
    errorAt(loc, where + " must have type " + typeName(expected) + ", got " + typeName(actual));
    return false;
}

} // namespace mc
