#include "llvm/Support/Compiler.h"

//  初始化目标架构的​​机器码（Machine Code,
//  MC）层组件​​，包括汇编器（AsmPrinter）、
// 反汇编器（Disassembler）和指令编码器（MCCodeEmitter）。
// 这些组件负责将LLVM IR转换为目标机器码或反汇编二进制文件。
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeNanoTargetMC() {
  // TODO: Add initialize target MC
}
