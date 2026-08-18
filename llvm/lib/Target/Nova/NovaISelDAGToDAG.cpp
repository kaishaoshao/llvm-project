#include "NovaISelDAGToDAG.h"
#include "NovaSubtarget.h"

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Pass.h"
#include "llvm/Support/CodeGen.h"

using namespace llvm;

#define DEBUG_TYPE "nova-isel"

namespace {
class NovaDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  NovaDAGToDAGISelLegacy(NovaTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(
        ID, std::make_unique<NovaDAGToDAGISel>(TM, OptLevel)
      ) {}
};
} // end of namespace

char NovaDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(NovaDAGToDAGISelLegacy, DEBUG_TYPE, "nova-isel", false, false);

FunctionPass *llvm::createNovaISelDagLegacy(NovaTargetMachine &TM,
                                            CodeGenOptLevel OptLevel) {
  return new NovaDAGToDAGISelLegacy(TM, OptLevel);
}

bool NovaDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget =
      &static_cast<const NovaSubtarget &>(MF.getSubtarget<NovaSubtarget>());
  return SelectionDAGISel::runOnMachineFunction(MF);
}

void NovaDAGToDAGISel::Select(SDNode *Node) {
  // Implement the selection logic here.
  // This is where you would match the SelectionDAG nodes to the target
  // instructions. For example, you might want to match a specific
  // node type and
  // then create a corresponding machine instruction.

  // Example: if (Node->getOpcode() == ISD::ADD) { ... }
  // This is just a placeholder for the actual implementation.
  SelectCode(Node);
}
