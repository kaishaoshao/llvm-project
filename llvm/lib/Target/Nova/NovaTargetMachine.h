#ifndef LLVM_LIB_TARGET_NOVA_NOVATARGETMACHINE_H
#define LLVM_LIB_TARGET_NOVA_NOVATARGETMACHINE_H

#include "NovaSubtarget.h"
#include "NovaTargetObjectFile.h"

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

static const char *NovaDataLayoutString =
    "e-m:m-p32:32-i8:8:32-i26:16:32-i64:64-n32-S64";

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  if (!RM)
    return Reloc::Static;
  return *RM;
}

namespace llvm {
class NovaTargetMachine final : public CodeGenTargetMachineImpl {
  NovaSubtarget Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  NovaTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, const TargetOptions &Options,
                    std::optional<Reloc::Model> RM,
                    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                    bool JIT)
      : CodeGenTargetMachineImpl(T, NovaDataLayoutString, TT, CPU, FS, Options,
                                 getEffectiveRelocModel(RM),
                                 getEffectiveCodeModel(CM, CodeModel::Medium),
                                 OL),
        Subtarget(TT, CPU, FS, *this), TLOF(new NovaTargetObjectFile()) {
          initAsmInfo();
}

// ~NovaTargetMachine() override = default;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  const NovaSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
};
} // end of namespace llvm

#endif // LLVM_LIB_TARGET_NOVA_NOVATARGETMACHINE_H
