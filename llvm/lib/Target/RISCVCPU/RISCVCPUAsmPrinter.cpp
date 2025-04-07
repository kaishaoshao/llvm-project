#include "RISCVCPUAsmPrinter.h"
#include <llvm/MC/TargetRegistry.h>

using namespace llvm;
extern Target TheRISCVCPUTarget;

/**
 * @brief 初始化RISC-V CPU的汇编打印器。
 * 
 * 该函数用于注册RISC-V CPU的汇编打印器（RISCVCPUAsmPrinter），
 * 以便在LLVM编译过程中生成RISC-V架构的汇编代码。
 * 
 * 该函数通过调用`RegisterAsmPrinter`模板函数，将`RISCVCPUAsmPrinter`类
 * 与目标架构`TheRISCVCPUTarget`关联起来，从而在LLVM中注册该汇编打印器。
 * 
 * @note 该函数使用`extern "C"`声明，以确保在C++代码中能够被C语言调用。
 */
extern "C" void LLVMInitializeRISCVCPUAsmPrinter() {
  // 注册RISCVCPUAsmPrinter，将其与RISC-V CPU目标架构关联
  RegisterAsmPrinter<RISCVCPUAsmPrinter> X(TheRISCVCPUTarget);
}