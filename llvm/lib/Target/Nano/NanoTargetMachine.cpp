#include "llvm/Support/Compiler.h"
// 注册​​目标机器（TargetMachine）​​，这是后端代码生成的核心入口。
// 它定义了如何将LLVM IR转换为目标机器码，并关联了目标特定的代码
// 生成策略（如指令选择、寄存器分配）
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeNanoTarget() {}




