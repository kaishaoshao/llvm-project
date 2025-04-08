#include "RISCVCPUTargetObjectFile.h"

using namespace llvm;

/**
 * @brief 初始化RISCV CPU目标对象文件。
 * 
 * 该函数用于初始化与RISCV CPU相关的目标对象文件。它首先调用基类的初始化函数，
 * 然后初始化ELF（Executable and Linkable Format）相关的设置。
 * 
 * @param Ctx MCContext对象，用于管理目标相关的上下文信息。
 * @param TM TargetMachine对象，表示目标机器的配置和状态。
 * 
 * @return 无返回值。
 */
void RISCVCPUTargetObjectFile::Initialize(MCContext &Ctx, TargetMachine
                                          const &TM) {
  // 调用基类的初始化函数，确保基类的相关设置被正确初始化。
  TargetLoweringObjectFile::Initialize(Ctx, TM);

  // 初始化ELF格式相关的设置，false表示不启用某些特定的ELF特性。
  InitializeELF(false);
}
