#include "llvm/Transforms/Utils/DbgDeclareValue.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

bool llvm::traverseDbg(Function &F) {
  bool Ret = false;

  for (BasicBlock &BB : F)
    for (Instruction &I : make_early_inc_range(BB))
      if (isa<DbgDeclareInst>(I) || isa<DbgValueInst>(I)) {
        I.eraseFromParent();
        Ret = true;
      }

  return Ret;
}

// NewPassManager

PreservedAnalyses DbgDeclareValue::run(Function &F,
                                       FunctionAnalysisManager &AM) {
  if (!traverseDbg(F))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

// LegacyPassManager

namespace {

struct DbgDeclareValuePass : public FunctionPass {
  static char ID;
  DbgDeclareValuePass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override { return traverseDbg(F); }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }
};

} // namespace

char DbgDeclareValuePass::ID = 0;

static RegisterPass<DbgDeclareValuePass>
    X("dbg-declare-value", "DBG Declare and Value Pass", false, false);