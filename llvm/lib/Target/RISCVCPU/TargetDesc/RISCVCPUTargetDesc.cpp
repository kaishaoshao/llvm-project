#include <llvm/MC/TargetRegistry.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/ADT/None.h>

#define GET_SUBTARGETINFO_MC_DESC
#include "RISCVCPUGenSubtargetInfo.inc"

using namespace llvm;
extern Target TheRISCVCPUTarget;

static MCAsmInfo *createRISCVCPUMCAsmInfo(MCRegisterInfo const &MRI,
                Triple const &TT, MCTargetOptions const &Options) {
    MCAsmInfo *x = new MCAsmInfo();
    return x;
}

static MCSubtargetInfo *createRISCVCPUMCSubtargetInfo(Triple const &TT, 
                StringRef CPU, StringRef FS) {
    // NOTE： 这是个函数是通过 td 生成的
    return createRISCVCPUMCSubtargetInfoImpl(TT, CPU, CPU,FS);
}

static MCInstrInfo *createRISCVMCInstrInfo() {
    MCInstrInfo *x = new MCInstrInfo();
    return x;
}

static MCRegisterInfo *createRISCVMCRegisterInfo(Triple const &TT) {
    MCRegisterInfo *x = new MCRegisterInfo();
    return x;
}


///  定义了一个外部c语言函数，用于初始化RISCV CPU目标机器的代码生成器
extern "C" void LLVMInitializeRISCVCPUTargetMC() {
    TargetRegistry::RegisterMCRegInfo(TheRISCVCPUTarget, createRISCVMCRegisterInfo);

    TargetRegistry::RegisterMCInstrInfo(TheRISCVCPUTarget, createRISCVMCInstrInfo);

    TargetRegistry::RegisterMCSubtargetInfo(TheRISCVCPUTarget,createRISCVCPUMCSubtargetInfo);

    TargetRegistry::RegisterMCAsmInfo(TheRISCVCPUTarget, createRISCVCPUMCAsmInfo);
}
