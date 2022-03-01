#ifndef LLVM_TRANSFORMS_UTILS_DBGDECLAREVALUE_H
#define LLVM_TRANSFORMS_UTILS_DBGDECLAREVALUE_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {
void traverseDbg(Function &F);
} // namespace llvm

struct DbgDeclareValue : public llvm::PassInfoMixin<DbgDeclareValue> {
  llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
};

#endif // LLVM_TRANSFORMS_UTILS_DBGDECLAREVALUE_H
