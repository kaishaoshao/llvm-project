#ifndef LLVM_LIB_TARGET_NOVA_NOVAISELDAGTODAG_H
#define LLVM_LIB_TARGET_NOVA_NOVAISELDAGTODAG_H

#include "NovaSubtarget.h"
#include "NovaTargetMachine.h"

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

namespace llvm {
class NovaDAGToDAGISel final : public SelectionDAGISel {
  const NovaSubtarget *Subtarget;

public:
  explicit NovaDAGToDAGISel(NovaTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
#include "NovaGenDAGSel.inc"

  void Select(SDNode *Node) override;
};
} // end of  namespace llvm

#endif // LLVM_LIB_TARGET_NOVA_NOVAISELDAGTODAG_H
