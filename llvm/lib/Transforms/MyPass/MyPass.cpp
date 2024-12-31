#include "llvm/Transforms/MyPass/MyPass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

PreservedAnalyses MyPass::run(Function &F, FunctionAnalysisManager &AM) {
  errs() << "MyPass in function: " << F.getName().str() << "\n";
  return PreservedAnalyses::all();
}