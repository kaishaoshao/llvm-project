#include "llvm/MC/TargetRegistry.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

Target TheCoralNPUTarget;

extern "C" void LLVMInitializeCoralNPUTargetInfo() {
  RegisterTarget<Triple::coralnpu32, true>
      X(TheCoralNPUTarget, "coralnpu32", "CoralNPU 32-bit", "CoralNPU");
}

