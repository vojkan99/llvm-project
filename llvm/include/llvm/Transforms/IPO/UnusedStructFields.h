#ifndef LLVM_TRANSFORMS_IPO_UNUSEDSTRUCTFIELDS_H
#define LLVM_TRANSFORMS_IPO_UNUSEDSTRUCTFIELDS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

/// Eliminate unused struct fields from structures.
class UnusedStructureFieldsEliminationPass
    : public PassInfoMixin<UnusedStructureFieldsEliminationPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_IPO_UNUSEDSTRUCTFIELDS_H