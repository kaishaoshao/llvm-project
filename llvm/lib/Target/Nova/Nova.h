/// This is for global functions in the Nova target.
#ifndef LLVM_LIB_TARGET_NOVA_NOVA_H
#define LLVM_LIB_TARGET_NOVA_NOVA_H

#include "MCTargetDesc/NovaMCTargetDesc.h"

#include "llvm/Pass.h"
#include "llvm/Support/CodeGen.h"

namespace llvm {
class FunctionPass;
class NovaTargetMachine;

FunctionPass *createNovaISelDagLegacy(NovaTargetMachine &TM,
                                      CodeGenOptLevel OptLevel);

void initializeNovaDAGToDAGISelLegacyPass(PassRegistry &);

} // end of namspace llvm

#endif
