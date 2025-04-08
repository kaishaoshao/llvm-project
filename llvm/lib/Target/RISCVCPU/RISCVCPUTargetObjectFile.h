#ifndef __RISCVCPU_TARGET_OBJECT_FILE_H__
#define __RISCVCPU_TARGET_OBJECT_FILE_H__

#include <llvm/CodeGen/TargetLoweringObjectFileImpl.h>
#include <llvm/MC/MCContext.h>
#include <llvm/Target/TargetMachine.h>


namespace llvm {
class RISCVCPUTargetObjectFile : public TargetLoweringObjectFileELF {
public:
  void Initialize(MCContext &Ctx, TargetMachine const  &TM) override;
};

} // namespace llvm


#endif // __RISCVCPU_TARGET_OBJECT_FILE_H__