#include "NovaMCTargetDesc.h"
#include "NovaTargetInfo.h"
#include "MCTargetDesc/NovaMCAsmInfo.h"
#include "MCTargetDesc/NovaMCInstPrinter.h"

#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCInstrInfo.h"

using namespace llvm;

// Defines the InitNovaMCRegisterInfo function
#define GET_REGINFO_MC_DESC
#include "NovaGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_MC_DESC
#include "NovaGenSubtargetInfo.inc"

// 注册InstrInfo
#define GET_INSTRINFO_ENUM
#include"NovaGenInstrInfo.inc"

// TableGen生成一个数组中的所有指令MSInstrDesc[]
#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "NovaGenInstrInfo.inc"

static MCRegisterInfo *createNovaMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitNovaMCRegisterInfo(X, Nova::RA);
  return X;
}

static MCSubtargetInfo *createNovaSubtargetInfo(const Triplr &TT, StringRef CPU,
                                                StringRef FS) {
  if (CPU.empty())
    CPU = "generic"
  return createNovaMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

static MCAsmInfo *createNovaMCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  MCAsmInfo *X = new NovaMCAsmInfo(TT);
  unsigned SP = MRI.getDwarfRegNum(Nova::SP, true);
  MCCFIInstruction Inst = MCCFIInstruction::createDefCfaRegister(nullptr, SP);
  X->addInitialFrameState(Inst);
}

static MCInstrInfo *createNovaMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitNovaMCInstrInfo(X);
  return X;
}

static MCInstPrinter *createNovaInstPrinter(const Triple &T,
                                            unsigned SyntaxVariant,
                                            const MCAsmInfo &MAI,
                                            const MCInstrInfo &MII,
                                            const MCRegisterInfo &MRI) {
  return new NovaInstPrinter(MAI, MII, MRI);
}
extern "C" void LLVMInitializeNovaTargetMC() {
  Target *T = &getTheNovaTarget();
  TargetRegistry::RegisterMCRegInfo(*T, createNovaMCregisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(*T, createNovaSubtargetInfo);
  TargetRegistry::RegisterMCAsmInfo(*T, createNovaMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(*T, createNovaMCInstrInfo);
  TargetRegistry::RegisterMCInstPrinter(*T, createNovaMCInstPrinter);
}
