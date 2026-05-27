#include "llvm/MC/TargetRegistry.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

Target TheMiniRiscvTarget;

extern "C" void LLVMInitializeMiniRiscvTargetInfo() {
  RegisterTarget<Triple::miniriscv32, true>
      X(TheMiniRiscvTarget, "mini-riscv32", "RISC-V 32-bit", "MINIRISCV");
}

