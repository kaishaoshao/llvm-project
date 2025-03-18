
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>

// 该变量用于表示 RISC-V CPU 目标的注册信息，通常在 LLVM 中用于将目标架构
// 与编译器工具链关联起来。通过此变量，LLVM 可以识别并处理 RISCV CPU 架构的
// 代码生成任务。
using namespace llvm;
Target TheRISCVCPUTarget;

extern "C" void LLVMInitializeRISCVCPUTargetInfo() {
    RegisterTarget<Triple::riscvcpu, /*HasJIT=*/true> X(TheRISCVCPUTarget, "riscvcpu", "The RISCVCPU backend", "RISCVCPU");
}