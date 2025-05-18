#include "llvm/MC/TargetRegistry.h" // for RegisterTargetMachine
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

static llvm::Target &getTheMipsNanoTarget() {
  static Target TheNanoTarget;
  return TheNanoTarget;
}

// 注册目标架构的​​基础描述信息​​，包括目标名称（如
// mipsnano）、支持的指令集特征、数据布局（DataLayout）等。
// 这些信息用于编译器前端（如Clang）识别目标架构并生成对应的LLVM IR
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeNanoTargetInfo() {
  // TODO: Add initialize target info
  llvm::RegisterTarget<llvm::Triple::mipsnano> X(
      ::getTheMipsNanoTarget(), "mipsnano", "MIPSNano (32-bit big endian)",
      "MIPSNano");
}