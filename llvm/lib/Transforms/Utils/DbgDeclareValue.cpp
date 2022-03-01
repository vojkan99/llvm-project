#include "llvm/Transforms/Utils/DbgDeclareValue.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void llvm::traverseDbg(Function &F) {
    int declareCount = 0, valueCount = 0;

    for (BasicBlock &BB : F)
        for (Instruction &I : BB) {
            if (isa<DbgDeclareInst>(I))
                declareCount++;
            else if (isa<DbgValueInst>(I))
                valueCount++;
        }

    // F.dump();

    outs() << "Function ";
    outs().write_escaped(F.getName()) << " has " << declareCount << " llvm.dbg.declare calls and " << valueCount << " llvm.dbg.value calls!\n";
}

// NewPassManager

PreservedAnalyses DbgDeclareValue::run(Function &F,
                                       FunctionAnalysisManager &AM) {
    traverseDbg(F);
    return PreservedAnalyses::all();
}

// LegacyPassManager

namespace {

    struct DbgDeclareValuePass : public FunctionPass {
        
        static char ID;
        DbgDeclareValuePass() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override {
            traverseDbg(F);
            return false;
        }

    };

}

char DbgDeclareValuePass::ID = 0;

static RegisterPass<DbgDeclareValuePass> X("dbg-declare-value", "DBG Declare and Value Pass", false, false);