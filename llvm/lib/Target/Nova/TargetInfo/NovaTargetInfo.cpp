#include "NovaTargetInfo.h"

#include "llvm/MC/TargetRegistry.h"

Target &llvm::Target &getTheNovaTarget() {
  static Target TheNovaTarget;
  return TheNovaTarget;
}

//
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeNovaTargetInfo() {
  llvm::RegisterTarget<llvm::Triple::mipsnova> X(::getTheNovaTarget(),
  "mipsnova", "MipsNova (32-bit big endian)", "Nova");
}
