
#include "CoralNPUTargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"

using namespace llvm;

extern Target TheCoralNPUTarget;

extern "C" void LLVMInitializeCoralNPUTarget() {
  RegisterTargetMachine<CoralNPUTargetMachine> X(TheCoralNPUTarget);
}

CoralNPUTargetMachine::CoralNPUTargetMachine(const Target &T, const Triple &TT,
                                             StringRef CPU, StringRef FS,
                                             const TargetOptions &Options,
                                             std::optional<Reloc::Model> RM,
                                             std::optional<CodeModel::Model> CM,
                                             CodeGenOptLevel OL, bool JIT)
    // NOTE: 这里的参数主要是 llc 传递过来的命令行参数 ，目前暂时没有针对 CoralNPU 的特殊处理，所以直接传递默认值
    : LLVMTargetMachine(T, "e-m:m-p:32:32-i8:8:32-i16:16:32-i64:64-n32-S64",
      TT, CPU, FS, Options, Reloc::Static, CodeModel::Small, OL) {
  initAsmInfo();
}

