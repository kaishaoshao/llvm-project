#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  struct SimpleModulePass : public PassInfoMixin<SimpleModulePass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
      outs() << "SimpleModulePass: Processing module '" << M.getName() << "'\n";
      return PreservedAnalyses::all();
    }
  };
}

extern "C" PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "SimpleModulePass", "v0.1", [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "simple-module-pass") {
            MPM.addPass(SimpleModulePass());
            return true;
          }
          return false;
        }
      );
    }
  };
}