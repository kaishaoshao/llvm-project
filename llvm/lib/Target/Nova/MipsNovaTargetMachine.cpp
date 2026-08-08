#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

static llvm::Target &getTheNovaTarget() {
  static Target TheNovaTarget;
  return TheNovaTarget;
}

//
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeNovaTargetInfo() {
  llvm::RegisterTarget<llvm::Triple::mipsnova> X(::getTheNovaTarget(),
  "mipsnova", "MipsNova (32-bit big endian)", "Nova");
}


extern "C" void LLVMInitializeNovaTargetMC() {
  // TODO: Add initialize target MC
}


extern "C" void LLVMInitializeNovaTarget() {
  // TODO: Add initialize target
}



