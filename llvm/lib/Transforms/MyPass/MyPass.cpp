// MyPass.cpp

#include "llvm/Transforms/MyPass/MyPass.h"
#include "llvm/IR/Function.h"

using namespace llvm;

PreservedAnalyses MyPass::run(Function &F,
                    FunctionAnalysisManager &AM)
{
    errs() << "MyPass in function: " << F.getName() << "\n";

    return PreservedAnalyses::all();
}


