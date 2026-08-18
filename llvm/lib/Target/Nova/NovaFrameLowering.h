// This file defines the frame lowering for the Nova target
#ifndef LLVM_LIB_TARGET_NOVA_FRAMELOWERING_H
#define LLVM_LIB_TARGET_NOVA_FRAMELOWERING_H

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

namespace llvm {
class NovaFrameLowering : public TargetFrameLowering {
public:
  explicit NovaFrameLowering(const TargetSubtargetInfo &STI, Align Alignment)
      : TargetFrameLowering(StackGrowsDown, Alignment, 0, Alignment) {}

  void emitPrologue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override {}

  void emitEpilogue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override {}

  bool hasFPImpl(const MachineFunction &MF) const override {
    return true;
  }
};

} // end namespace llvm

#endif

