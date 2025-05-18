#include "llvm/MC/TargetRegistry.h" // for RegisterTargetMachine
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

static llvm::Target &getTheMipsNanoTarget() {
  static Target TheMipsNanoTarget;
  return TheMipsNanoTarget;
}

// 注册目标架构的​​基础描述信息​​，包括目标名称（如
// mipsnano）、支持的指令集特征、数据布局（DataLayout）等。
// 这些信息用于编译器前端（如Clang）识别目标架构并生成对应的LLVM IR
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMipsNanoTarget() {
  llvm::RegisterTarget<llvm::Triple::mipsnano>
      X(::getTheMipsNanoTarget(), 
      "mipsnano", "MIPSNano (32-bit big endian)",
      "MIPSNano");
}

//  初始化目标架构的​​机器码（Machine Code,
//  MC）层组件​​，包括汇编器（AsmPrinter）、
// 反汇编器（Disassembler）和指令编码器（MCCodeEmitter）。
// 这些组件负责将LLVM IR转换为目标机器码或反汇编二进制文件。
extern "C" void LLVMInitializeMipsNanoTargetMC() {
    // TODO: Add initialize target MC
}

// 注册​​目标机器（TargetMachine）​​，这是后端代码生成的核心入口。
// 它定义了如何将LLVM IR转换为目标机器码，并关联了目标特定的代码
// 生成策略（如指令选择、寄存器分配）
extern "C" void LLVMInitializeMipsNanoTargetInfo() {
    // TODO: Add initialize target info
}

