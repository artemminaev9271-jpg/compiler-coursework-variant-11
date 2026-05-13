#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace mc {

class CodeGenerator {
public:
    CodeGenerator();

    bool generate(Program &program);
    bool emitIR(const std::string &path);
    bool emitObject(const std::string &path);

    const std::string &error() const { return error_; }
    llvm::Module &module() { return *module_; }

private:
    llvm::Type *llvmType(TypeKind type);
    llvm::Function *declareFunction(const FunctionDecl &fn);
    bool codegenFunction(FunctionDecl &fn);
    bool codegenBlock(BlockStmt &block, bool createScope);
    bool codegenStmt(Stmt &stmt);
    llvm::Value *codegenExpr(Expr &expr);
    llvm::Value *codegenLogical(BinaryExpr &expr);
    llvm::Value *codegenTernary(TernaryExpr &expr);
    llvm::AllocaInst *createEntryAlloca(llvm::Function *fn, const std::string &name, TypeKind type);
    llvm::AllocaInst *lookupVariable(const std::string &name);

    void pushScope();
    void popScope();
    void setError(const std::string &message);

    llvm::LLVMContext context_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;
    std::vector<std::unordered_map<std::string, llvm::AllocaInst *>> scopes_;
    std::unordered_map<std::string, llvm::Function *> functions_;
    std::string targetTriple_ = "x86_64-pc-linux-gnu";
    std::string error_;
};

} // namespace mc
