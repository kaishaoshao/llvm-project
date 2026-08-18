#include "Nova.h"
#include "NovaTatgetInfo.h"

#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

namespace {
class NovaPassConfig : public TargetPassConfig {
public:
  NovaPassConfig(NovaTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  NovaTargetMachine &getNovaTargetMachine() const {
    return getTM<NovaTargetMachine>();
  }

  bool addInstSelector() override {
    addPass(createNovaISelDagLegacy(getNovaTargetMachine(), getOptLevel()));
    return false;
  }

  void addPreEmitPass() override {}
};
} // namespace

TargetPassConfig *NovaTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new NovaPassConfig(*this, PM);
}

extern "C" void LLVMInitializeNovaTarget() {
  RegisterTargetMachine<NovaTargetMachine> X(getTheNovaTarget());

  initializeNovaDAGToDAGISelLegacyPass(*PassRegistry::getPassRegistry());
}

