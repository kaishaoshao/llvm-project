// 文件职责：
// 建 MC 层对象
// 把工厂函数注册进 TargetRegistry

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/TargetParser/Triple.h"

#include "CoralNPUTargetDesc.h"

// 生成InitCoralNPUMCInsterInfo
// #define GET_INSTRINFO_MC_DESC
// #include "CoralNPUGenInstrInfo.inc"

// 生成InitCoralNPUMCRegisterInfo
#define GET_SUBTARGETINFO_MC_DESC
#include "CoralNPUGenSubtargetInfo.inc"

// 生成createCoralNPUMCSubtargetInfoImpl
#define GET_REGINFO_MC_DESC
#include "CoralNPUGenRegisterInfo.inc"

using namespace llvm;

// “这个对象在别的翻译单元里已经定义过了”
// “我这里只是要引用它来注册 MC 工厂”
extern Target TheCoralNPUTarget;

// 调 TableGen 生成的初始化函数把寄存器描述填进去
// 告诉 LLVM 默认返回地址寄存器是谁
static MCRegisterInfo *createCoralNPUMCRegisterInfo(const Triple &TT) {
  auto *X = new MCRegisterInfo();

  // 第二个参数是默认返回地址寄存器
  // 这里假设你后面会在RegisterInfo.td 里定义RA
  // TODO：RA是什么？
  // InitCoralNPUMCRegisterInfo(X, CoralNPU::RA);
  return X;
}


static MCInstrInfo *createCoralNPUMCInstrInfo() {
  auto *X = new MCInstrInfo();
  // InitCoralNPUMCInsterInfo(X);
  return X;
}

// 把 Triple - CPU - Feature string 组合成一个 MCSubtargetInf
static MCSubtargetInfo *
createCoralNPUMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  // 因为很多时候用户没有显式传 -mcpu=...。这时你得给一个默认 CPU 名字，否则 subtarget 初始化没法落地。
  if (CPU.empty())
    CPU = "generic";
  // NOTE: 通过td生成的
  return createCoralNPUMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

// 创建汇编格式相关的基础对象
// 后面它会决定一些汇编层细节，比如：
// 注释风格
// 某些 section / directive 行为
// 汇编器层默认格式
static MCAsmInfo *createCoralNPUMCAsmInfo(const MCRegisterInfo &MRI,
                                          const Triple &TT,
                                          const MCTargetOptions &Options) {
  // 先给最小对象，后面换成CoralNPUMCAsmInfo之类
  auto *MAI = new MCAsmInfo();
  return MAI;
}

// LLVMInitializeCoralNPUTargetMC() 做的就是：
// 把这些工厂函数挂到 TheCoralNPUTarget 上
extern "C" void LLVMInitializeCoralNPUTargetMC() {
  TargetRegistry::RegisterMCRegInfo(TheCoralNPUTarget,
                                    createCoralNPUMCRegisterInfo);
  TargetRegistry::RegisterMCInstrInfo(TheCoralNPUTarget,
                                      createCoralNPUMCInstrInfo);
  // TargetRegistry::RegisterMCSubtargetInfo(TheCoralNPUTarget,
  //                                         createCoralNPUMCAsmInfo);
  TargetRegistry::RegisterMCAsmInfo(TheCoralNPUTarget,
                                  createCoralNPUMCAsmInfo);
}

