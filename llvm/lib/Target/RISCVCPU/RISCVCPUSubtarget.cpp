#include "RISCVCPUSubtarget.h"

#define DEBUG_TYPE "riscv-cpu-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "RISCVCPUGenSubtargetInfo.inc"

using namespace llvm;

RISCVCPUSubtarget::RISCVCPUSubtarget(Triple const &TT, StringRef &CPU,
                                     StringRef &TuneCPU, StringRef &FS,
                                     RISCVCPUTargetMachine const &TTM)
    : RISCVCPUGenSubtargetInfo(TT, CPU, TuneCPU, FS) {}
