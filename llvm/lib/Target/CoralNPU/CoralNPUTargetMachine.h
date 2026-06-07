#ifndef CORALNPU_TARGET_MACHINE_H
#define CORALNPU_TARGET_MACHINE_H

#include "llvm/Target/TargetMachine.h"

namespace llvm {
class CoralNPUTargetMachine : public LLVMTargetMachine {
public:
  CoralNPUTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                        StringRef FS, const TargetOptions &Options,
                        std::optional<Reloc::Model> RM,
                        std::optional<CodeModel::Model> CM,
                        CodeGenOptLevel OL,
                        bool JIT);
  ~CoralNPUTargetMachine() {};
};

} // namespace llvm


#endif // CORALNPU_TARGET_MACHINE_H
