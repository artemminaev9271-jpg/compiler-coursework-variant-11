#include "codegen.hpp"

#include <system_error>

#include <llvm/ADT/Optional.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/LegacyPassManager.h>

namespace mc {

CodeGenerator::CodeGenerator()
    : module_(std::make_unique<llvm::Module>("mini_module", context_)), builder_(context_) {
    module_->setTargetTriple(targetTriple_);
}

bool CodeGenerator::generate(Program &program) {
    error_.clear();
    functions_.clear();
    scopes_.clear();

    module_ = std::make_unique<llvm::Module>("mini_module", context_);
    module_->setTargetTriple(targetTriple_);

    for (const auto &fn : program.functions) {
        if (!declareFunction(*fn)) {
            return false;
        }
    }

    for (auto &fn : program.functions) {
        if (!codegenFunction(*fn)) {
            return false;
        }
    }

    std::string verifyErrors;
    llvm::raw_string_ostream os(verifyErrors);
    if (llvm::verifyModule(*module_, &os)) {
        os.flush();
        setError("LLVM module verification failed:\n" + verifyErrors);
        return false;
    }

    return true;
}

bool CodeGenerator::emitIR(const std::string &path) {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        setError("cannot open IR output file '" + path + "': " + ec.message());
        return false;
    }
    module_->print(out, nullptr);
    return true;
}

bool CodeGenerator::emitObject(const std::string &path) {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();

    std::string targetError;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(targetTriple_, targetError);
    if (!target) {
        setError("target lookup failed: " + targetError);
        return false;
    }

    llvm::TargetOptions opt;
    auto relocModel = llvm::Optional<llvm::Reloc::Model>();
    std::unique_ptr<llvm::TargetMachine> targetMachine(
        target->createTargetMachine(targetTriple_, "generic", "", opt, relocModel));
    if (!targetMachine) {
        setError("failed to create target machine for " + targetTriple_);
        return false;
    }

    module_->setDataLayout(targetMachine->createDataLayout());
    module_->setTargetTriple(targetTriple_);

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        setError("cannot open object output file '" + path + "': " + ec.message());
        return false;
    }

    llvm::legacy::PassManager pass;
    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CGFT_ObjectFile)) {
        setError("target machine cannot emit object file");
        return false;
    }

    pass.run(*module_);
    dest.flush();
    return true;
}

llvm::Type *CodeGenerator::llvmType(TypeKind type) {
    switch (type) {
    case TypeKind::Int:
        return llvm::Type::getInt64Ty(context_);
    case TypeKind::Bool:
        return llvm::Type::getInt1Ty(context_);
    }
    return llvm::Type::getInt64Ty(context_);
}

llvm::Function *CodeGenerator::declareFunction(const FunctionDecl &fn) {
    std::vector<llvm::Type *> params;
    params.reserve(fn.params.size());
    for (const auto &param : fn.params) {
        params.push_back(llvmType(param.type));
    }

    auto *functionType = llvm::FunctionType::get(llvmType(fn.returnType), params, false);
    auto *function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, fn.name, module_.get());

    std::size_t index = 0;
    for (auto &arg : function->args()) {
        arg.setName(fn.params[index++].name);
    }

    functions_[fn.name] = function;
    return function;
}

bool CodeGenerator::codegenFunction(FunctionDecl &fn) {
    llvm::Function *function = functions_[fn.name];
    auto *entry = llvm::BasicBlock::Create(context_, "entry", function);
    builder_.SetInsertPoint(entry);

    pushScope();
    std::size_t index = 0;
    for (auto &arg : function->args()) {
        const auto &param = fn.params[index++];
        llvm::AllocaInst *alloca = createEntryAlloca(function, param.name, param.type);
        builder_.CreateStore(&arg, alloca);
        scopes_.back()[param.name] = alloca;
    }

    if (!codegenBlock(*fn.body, false)) {
        popScope();
        return false;
    }

    if (!builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateRet(llvm::ConstantInt::get(llvmType(fn.returnType), 0, true));
    }

    std::string verifyErrors;
    llvm::raw_string_ostream os(verifyErrors);
    if (llvm::verifyFunction(*function, &os)) {
        os.flush();
        setError("LLVM function verification failed for '" + fn.name + "':\n" + verifyErrors);
        popScope();
        return false;
    }

    popScope();
    return true;
}

bool CodeGenerator::codegenBlock(BlockStmt &block, bool createScope) {
    if (createScope) {
        pushScope();
    }

    for (auto &stmt : block.statements) {
        if (!codegenStmt(*stmt)) {
            if (createScope) {
                popScope();
            }
            return false;
        }
        if (builder_.GetInsertBlock()->getTerminator()) {
            break;
        }
    }

    if (createScope) {
        popScope();
    }
    return true;
}

bool CodeGenerator::codegenStmt(Stmt &stmt) {
    if (auto *block = dynamic_cast<BlockStmt *>(&stmt)) {
        return codegenBlock(*block, true);
    }

    llvm::Function *function = builder_.GetInsertBlock()->getParent();

    if (auto *decl = dynamic_cast<VarDeclStmt *>(&stmt)) {
        llvm::AllocaInst *alloca = createEntryAlloca(function, decl->name, decl->type);
        scopes_.back()[decl->name] = alloca;

        llvm::Value *initial = nullptr;
        if (decl->init) {
            initial = codegenExpr(*decl->init);
            if (!initial) {
                return false;
            }
        } else {
            initial = llvm::ConstantInt::get(llvmType(decl->type), 0, true);
        }

        builder_.CreateStore(initial, alloca);
        return true;
    }

    if (auto *exprStmt = dynamic_cast<ExprStmt *>(&stmt)) {
        return codegenExpr(*exprStmt->expr) != nullptr;
    }

    if (auto *ret = dynamic_cast<ReturnStmt *>(&stmt)) {
        llvm::Value *value = codegenExpr(*ret->value);
        if (!value) {
            return false;
        }
        builder_.CreateRet(value);
        return true;
    }

    if (auto *ifStmt = dynamic_cast<IfStmt *>(&stmt)) {
        llvm::Value *condition = codegenExpr(*ifStmt->condition);
        if (!condition) {
            return false;
        }

        llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context_, "if.then", function);
        llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(context_, "if.else");
        llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context_, "if.end");

        if (ifStmt->elseBlock) {
            builder_.CreateCondBr(condition, thenBB, elseBB);
        } else {
            builder_.CreateCondBr(condition, thenBB, mergeBB);
        }

        builder_.SetInsertPoint(thenBB);
        if (!codegenBlock(*ifStmt->thenBlock, true)) {
            return false;
        }
        if (!builder_.GetInsertBlock()->getTerminator()) {
            builder_.CreateBr(mergeBB);
        }

        if (ifStmt->elseBlock) {
            function->getBasicBlockList().push_back(elseBB);
            builder_.SetInsertPoint(elseBB);
            if (!codegenBlock(*ifStmt->elseBlock, true)) {
                return false;
            }
            if (!builder_.GetInsertBlock()->getTerminator()) {
                builder_.CreateBr(mergeBB);
            }
        }

        function->getBasicBlockList().push_back(mergeBB);
        builder_.SetInsertPoint(mergeBB);
        return true;
    }

    if (auto *forStmt = dynamic_cast<ForStmt *>(&stmt)) {
        pushScope();
        if (forStmt->init && !codegenStmt(*forStmt->init)) {
            popScope();
            return false;
        }

        llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context_, "for.cond", function);
        llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context_, "for.body");
        llvm::BasicBlock *stepBB = llvm::BasicBlock::Create(context_, "for.step");
        llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(context_, "for.end");

        builder_.CreateBr(condBB);
        builder_.SetInsertPoint(condBB);

        llvm::Value *condition = nullptr;
        if (forStmt->condition) {
            condition = codegenExpr(*forStmt->condition);
            if (!condition) {
                popScope();
                return false;
            }
        } else {
            condition = llvm::ConstantInt::getTrue(context_);
        }
        builder_.CreateCondBr(condition, bodyBB, afterBB);

        function->getBasicBlockList().push_back(bodyBB);
        builder_.SetInsertPoint(bodyBB);
        if (!codegenBlock(*forStmt->body, true)) {
            popScope();
            return false;
        }
        if (!builder_.GetInsertBlock()->getTerminator()) {
            builder_.CreateBr(stepBB);
        }

        function->getBasicBlockList().push_back(stepBB);
        builder_.SetInsertPoint(stepBB);
        if (forStmt->step && !codegenExpr(*forStmt->step)) {
            popScope();
            return false;
        }
        if (!builder_.GetInsertBlock()->getTerminator()) {
            builder_.CreateBr(condBB);
        }

        function->getBasicBlockList().push_back(afterBB);
        builder_.SetInsertPoint(afterBB);
        popScope();
        return true;
    }

    setError("internal codegen error: unknown statement node");
    return false;
}

llvm::Value *CodeGenerator::codegenExpr(Expr &expr) {
    if (auto *literal = dynamic_cast<IntLiteral *>(&expr)) {
        return llvm::ConstantInt::get(llvmType(TypeKind::Int), literal->value, true);
    }

    if (auto *var = dynamic_cast<VarExpr *>(&expr)) {
        llvm::AllocaInst *alloca = lookupVariable(var->name);
        if (!alloca) {
            setError("internal codegen error: missing variable '" + var->name + "'");
            return nullptr;
        }
        return builder_.CreateLoad(llvmType(expr.inferredType), alloca, var->name);
    }

    if (auto *unary = dynamic_cast<UnaryExpr *>(&expr)) {
        llvm::Value *value = codegenExpr(*unary->operand);
        if (!value) {
            return nullptr;
        }
        if (unary->op == UnaryOp::Neg) {
            return builder_.CreateNeg(value, "negtmp");
        }
        return builder_.CreateNot(value, "nottmp");
    }

    if (auto *binary = dynamic_cast<BinaryExpr *>(&expr)) {
        if (binary->op == BinaryOp::And || binary->op == BinaryOp::Or) {
            return codegenLogical(*binary);
        }

        llvm::Value *lhs = codegenExpr(*binary->lhs);
        llvm::Value *rhs = codegenExpr(*binary->rhs);
        if (!lhs || !rhs) {
            return nullptr;
        }

        switch (binary->op) {
        case BinaryOp::Add:
            return builder_.CreateAdd(lhs, rhs, "addtmp");
        case BinaryOp::Sub:
            return builder_.CreateSub(lhs, rhs, "subtmp");
        case BinaryOp::Mul:
            return builder_.CreateMul(lhs, rhs, "multmp");
        case BinaryOp::Div:
            return builder_.CreateSDiv(lhs, rhs, "divtmp");
        case BinaryOp::Mod:
            return builder_.CreateSRem(lhs, rhs, "modtmp");
        case BinaryOp::Eq:
            return builder_.CreateICmpEQ(lhs, rhs, "eqtmp");
        case BinaryOp::Ne:
            return builder_.CreateICmpNE(lhs, rhs, "netmp");
        case BinaryOp::Lt:
            return builder_.CreateICmpSLT(lhs, rhs, "lttmp");
        case BinaryOp::Le:
            return builder_.CreateICmpSLE(lhs, rhs, "letmp");
        case BinaryOp::Gt:
            return builder_.CreateICmpSGT(lhs, rhs, "gttmp");
        case BinaryOp::Ge:
            return builder_.CreateICmpSGE(lhs, rhs, "getmp");
        case BinaryOp::And:
        case BinaryOp::Or:
            break;
        }
    }

    if (auto *assign = dynamic_cast<AssignExpr *>(&expr)) {
        llvm::AllocaInst *alloca = lookupVariable(assign->name);
        if (!alloca) {
            setError("internal codegen error: missing variable '" + assign->name + "'");
            return nullptr;
        }

        llvm::Value *value = codegenExpr(*assign->value);
        if (!value) {
            return nullptr;
        }
        builder_.CreateStore(value, alloca);
        return value;
    }

    if (auto *ternary = dynamic_cast<TernaryExpr *>(&expr)) {
        return codegenTernary(*ternary);
    }

    if (auto *call = dynamic_cast<CallExpr *>(&expr)) {
        auto found = functions_.find(call->callee);
        if (found == functions_.end()) {
            setError("internal codegen error: missing function '" + call->callee + "'");
            return nullptr;
        }

        std::vector<llvm::Value *> args;
        args.reserve(call->args.size());
        for (auto &arg : call->args) {
            llvm::Value *value = codegenExpr(*arg);
            if (!value) {
                return nullptr;
            }
            args.push_back(value);
        }
        return builder_.CreateCall(found->second, args, call->callee + ".call");
    }

    setError("internal codegen error: unknown expression node");
    return nullptr;
}

llvm::Value *CodeGenerator::codegenLogical(BinaryExpr &expr) {
    llvm::Function *function = builder_.GetInsertBlock()->getParent();
    llvm::Value *lhs = codegenExpr(*expr.lhs);
    if (!lhs) {
        return nullptr;
    }

    llvm::BasicBlock *lhsBB = builder_.GetInsertBlock();
    llvm::BasicBlock *rhsBB = llvm::BasicBlock::Create(context_, "logic.rhs", function);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context_, "logic.end");

    if (expr.op == BinaryOp::And) {
        builder_.CreateCondBr(lhs, rhsBB, mergeBB);
    } else {
        builder_.CreateCondBr(lhs, mergeBB, rhsBB);
    }

    builder_.SetInsertPoint(rhsBB);
    llvm::Value *rhs = codegenExpr(*expr.rhs);
    if (!rhs) {
        return nullptr;
    }
    llvm::BasicBlock *rhsEndBB = builder_.GetInsertBlock();
    builder_.CreateBr(mergeBB);

    function->getBasicBlockList().push_back(mergeBB);
    builder_.SetInsertPoint(mergeBB);
    llvm::PHINode *phi = builder_.CreatePHI(llvmType(TypeKind::Bool), 2, "logictmp");
    if (expr.op == BinaryOp::And) {
        phi->addIncoming(llvm::ConstantInt::getFalse(context_), lhsBB);
    } else {
        phi->addIncoming(llvm::ConstantInt::getTrue(context_), lhsBB);
    }
    phi->addIncoming(rhs, rhsEndBB);
    return phi;
}

llvm::Value *CodeGenerator::codegenTernary(TernaryExpr &expr) {
    llvm::Function *function = builder_.GetInsertBlock()->getParent();
    llvm::Value *condition = codegenExpr(*expr.condition);
    if (!condition) {
        return nullptr;
    }

    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context_, "ternary.then", function);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(context_, "ternary.else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context_, "ternary.end");

    builder_.CreateCondBr(condition, thenBB, elseBB);

    builder_.SetInsertPoint(thenBB);
    llvm::Value *thenValue = codegenExpr(*expr.thenExpr);
    if (!thenValue) {
        return nullptr;
    }
    llvm::BasicBlock *thenEndBB = builder_.GetInsertBlock();
    builder_.CreateBr(mergeBB);

    function->getBasicBlockList().push_back(elseBB);
    builder_.SetInsertPoint(elseBB);
    llvm::Value *elseValue = codegenExpr(*expr.elseExpr);
    if (!elseValue) {
        return nullptr;
    }
    llvm::BasicBlock *elseEndBB = builder_.GetInsertBlock();
    builder_.CreateBr(mergeBB);

    function->getBasicBlockList().push_back(mergeBB);
    builder_.SetInsertPoint(mergeBB);
    llvm::PHINode *phi = builder_.CreatePHI(llvmType(expr.inferredType), 2, "ternarytmp");
    phi->addIncoming(thenValue, thenEndBB);
    phi->addIncoming(elseValue, elseEndBB);
    return phi;
}

llvm::AllocaInst *CodeGenerator::createEntryAlloca(llvm::Function *fn, const std::string &name, TypeKind type) {
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmp.CreateAlloca(llvmType(type), nullptr, name);
}

llvm::AllocaInst *CodeGenerator::lookupVariable(const std::string &name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return nullptr;
}

void CodeGenerator::pushScope() {
    scopes_.emplace_back();
}

void CodeGenerator::popScope() {
    scopes_.pop_back();
}

void CodeGenerator::setError(const std::string &message) {
    error_ = message;
}

} // namespace mc
