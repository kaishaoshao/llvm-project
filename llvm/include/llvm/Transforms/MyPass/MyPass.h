#ifndef LLVM_TRANSFORMS_MYPASS_H
#define LLVM_TRANSFORMS_MYPASS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class MyPass :  public PassInfoMixin<MyPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif