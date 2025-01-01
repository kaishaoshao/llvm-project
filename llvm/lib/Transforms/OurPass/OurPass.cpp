#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  struct OurPass : public PassInfoMixin<OurPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
      errs() << "OurPass: Processing function '" << F.getName() << "'\n";
      return PreservedAnalyses::all();
    }
  };
}

extern "C" PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "OurPass", "v0.1", [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "ourpass") {
            FPM.addPass(OurPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}