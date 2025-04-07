//===------ RISCVCPUMCTargetDesc.cpp - RISC-V MC 目标描述实现 ------===//
// 此文件为RISC-V目标机注册MC（Machine Code）相关组件，包括：
// - 汇编器/反汇编器规则
// - 指令/寄存器/子目标信息
//===----------------------------------------------------------------------===//

#include "RISCVCPUInstPrinter.h"          // RISC-V指令打印器（用于汇编/反汇编）
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/TargetParser/Triple.h"

#include <llvm/MC/TargetRegistry.h>        // 目标机注册表（管理目标架构注册）
#include <llvm/MC/MCAsmInfo.h>             // 汇编信息(定义汇编语法规则，如注释符号、对齐方式)
#include <llvm/MC/MCInstrInfo.h>           // 指令元数据（操作码、指令格式、操作数约束）
#include <llvm/MC/MCRegisterInfo.h>        // 寄存器描述（寄存器编号、别名、类别）
#include <llvm/MC/MCSubtargetInfo.h>       // 子目标特性（CPU型号、支持的指令扩展）
#include <llvm/ADT/None.h>                 // LLVM空类型工具（用于可选值占位）

// 启用通过TableGen生成的子目标信息描述（自动生成createRISCVCPUMCSubtargetInfoImpl）
#define GET_SUBTARGETINFO_MC_DESC
#include "RISCVCPUGenSubtargetInfo.inc"    // 包含TableGen生成的子目标信息实现

using namespace llvm;
extern Target TheRISCVCPUTarget;           // 声明全局RISC-V目标实例（在RISCVCPUTarget.cpp中定义）

//====---------------------------------------------------------------===//
// 创建 RISC-V 汇编信息(MCAsmInfo)
// 职责：定义汇编器的基本行为规则
// - 注释起始符号（如 "//" 或 "#"）
// - 对齐方式（如 .align 指令的行为）
// - 数据指令（如 .byte/.word 的语法）
// 参数：
//   MRI - 寄存器信息（当前未使用，保留用于未来扩展）
//   TT  - 目标三元组（例如 riscv64-unknown-linux-gnu）
//   Options - 目标选项（如 relocation model）
//====---------------------------------------------------------------===//
static MCAsmInfo *createRISCVCPUMCAsmInfo(const MCRegisterInfo &MRI,
                                          const Triple &TT,
                                          const MCTargetOptions &Options) {
  // TODO: 应替换为RISCV专用派生类（例如RISCVMCAsmInfo）
  // 当前为占位实现，需根据RISC-V汇编语法定制以下行为：
  // 1. 设置注释符号为 "#"（例如：li a0, 42 # 加载立即数）
  // 2. 定义代码/数据对齐规则（如 .align 2 表示4字节对齐）
  // 3. 配置汇编方言（如是否支持指令缩写）
  MCAsmInfo *AsmInfo = new MCAsmInfo();
  return AsmInfo;
}

//====---------------------------------------------------------------===//
// 创建 RISC-V 子目标信息(MCSubtargetInfo)
// 职责：描述目标CPU的硬件特性
// - CPU型号（如 SiFive U74）
// - 支持的指令扩展（如 M/A/C 扩展）
// - 流水线特性（如乱序执行、分支预测）
// 参数：
//   TT  - 目标三元组
//   CPU - 指定的CPU型号（如 "generic-rv64"）
//   FS  - 特性字符串（如 "+m,+a,+c"）
//====---------------------------------------------------------------===//
static MCSubtargetInfo *createRISCVCPUMCSubtargetInfo(const Triple &TT,
                                                      StringRef CPU,
                                                      StringRef FS) {
  // 通过TableGen生成的函数实现特性解析和CPU检测
  // 实现细节见RISCVCPUGenSubtargetInfo.inc
  return createRISCVCPUMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

//====---------------------------------------------------------------===//
// 创建 RISC-V 指令信息(MCInstrInfo)
// 职责：提供指令集元数据
// - 操作码（如 ADD 对应二进制编码）
// - 操作数类型（如寄存器、立即数）
// - 指令格式（如 R-type/I-type）
//====---------------------------------------------------------------===//
static MCInstrInfo *createRISCVMCInstrInfo() {
  // TODO: 应替换为TableGen生成的指令信息（RISCVGenInstrInfo）
  // 当前为占位实现，需初始化以下内容：
  // 1. 指令名称表（如 "add" -> 操作码）
  // 2. 操作数约束（如 add rd, rs1, rs2 的寄存器约束）
  // 3. 指令编码规则（如 RISC-V的32位指令格式）
  MCInstrInfo *InstrInfo = new MCInstrInfo();
  return InstrInfo;
}

//====---------------------------------------------------------------===//
// 创建 RISC-V 寄存器信息(MCRegisterInfo)
// 职责：描述寄存器架构
// - 寄存器名称（如 x0-x31, f0-f31）
// - 寄存器编号（如 x0=0, x1=1...）
// - 寄存器类别（如通用寄存器、浮点寄存器）
// 参数：
//   TT - 目标三元组（用于区分32/64位寄存器布局）
//====---------------------------------------------------------------===//
static MCRegisterInfo *createRISCVMCRegisterInfo(const Triple &TT) {
  // TODO: 调用TableGen生成的初始化函数
  // 例如：InitRISCVMCRegisterInfo(Registry, RA, PC, ...)
  // 当前为占位实现，需定义以下内容：
  // 1. 寄存器名称映射（如 x0 别名为 zero）
  // 2. 寄存器类别（如 GPR、FPR）
  // 3. 保留寄存器标记（如 x0 为硬编码零寄存器）
  MCRegisterInfo *RegInfo = new MCRegisterInfo();
  return RegInfo;
}

/**
 * 创建RISC-V CPU指令打印机
 *
 * 该函数用于生成一个RISC-V
 * CPU指令打印机实例，该实例能够根据提供的信息打印出符合特定语法变体的指令
 *
 * @param T 代表目标系统的Triple信息，用于确定指令集架构
 * @param SyntaxVariant 语法变体的标识符，用于选择不同的指令语法风格
 * @param MAI 提供有关汇编语言的信息，如注释规则和指令格式
 * @param MII 提供有关指令集的信息，包括指令的操作码和格式
 * @param MRI 提供有关寄存器的信息，包括寄存器的名称和编号
 *
 * @return 返回一个MCInstPrinter的实例，用于打印指令
 */
static MCInstPrinter *createRISCVCPUInstPrinter(Triple const &T,
                                                unsigned SyntaxVariant,
                                                MCAsmInfo const &MAI,
                                                MCInstrInfo const &MII,
                                                MCRegisterInfo const &MRI) {

    return new RISCVCPUInstPrinter(MAI, MII, MRI);
}


//===----------------------------------------------------------------------===//
// LLVM 目标机MC组件注册入口
// 职责：向LLVM注册RISC-V的MC相关组件创建函数
// 调用时机：LLVM初始化过程中加载RISC-V目标时
//===----------------------------------------------------------------------===//
extern "C" void LLVMInitializeRISCVCPUTargetMC() {
  // 注册寄存器信息创建函数
  // 当需要获取寄存器信息时（如反汇编），调用createRISCVMCRegisterInfo
  TargetRegistry::RegisterMCRegInfo(TheRISCVCPUTarget, createRISCVMCRegisterInfo);

  // 注册指令信息创建函数
  // 用于查询指令元数据（如汇编器/反汇编器使用）
  TargetRegistry::RegisterMCInstrInfo(TheRISCVCPUTarget, createRISCVMCInstrInfo);

  // 注册子目标信息创建函数
  // 根据CPU型号和特性字符串生成硬件特性描述
  TargetRegistry::RegisterMCSubtargetInfo(TheRISCVCPUTarget, createRISCVCPUMCSubtargetInfo);

  // 注册汇编信息创建函数
  // 控制汇编输出格式（如objdump的输出风格）
  TargetRegistry::RegisterMCAsmInfo(TheRISCVCPUTarget, createRISCVCPUMCAsmInfo);

  // 注册指令打印器创建函数
  // 用于将指令转换为汇编字符串（如汇编器/反汇编器使用）
  TargetRegistry::RegisterMCInstPrinter(TheRISCVCPUTarget, createRISCVCPUInstPrinter);
}