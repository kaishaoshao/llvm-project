#ifndef __RISCVCPU_SUBTARGET_H__
#define __RISCVCPU_SUBTARGET_H__


#include <llvm/ADT/None.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/MC/MCInst.h>

#define GET_SUBTARGETINFO_HEADER
#include "RISCVCPUGenSubtargetInfo.inc"

namespace llvm {
class RISCVCPUTargetMachine;
class RISCVCPUSubtarget : public RISCVCPUGenSubtargetInfo {
public:
  RISCVCPUSubtarget(Triple const &TT, StringRef &CPU, StringRef &TuneCPU,
                    StringRef &FS, RISCVCPUTargetMachine const &TTM);

  ~RISCVCPUSubtarget() override {}

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

};
} // namespace llvm

#endif // __RISCVCPU_SUBTARGET_H__