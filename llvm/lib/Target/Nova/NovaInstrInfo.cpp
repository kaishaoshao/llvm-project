#include "NovaInstrInfo.h"
#include "MCTargetDesc/NovaMCTargetDesc.h"
#include "NovaTargetMachine.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "nova-instr-info"

#define GET_INSTRINFO_CTOR_DTOR
#include "NovaGenInstrInfo.inc"

NovaInstrInfo::NovaInstrInfo(const NovaSubtarget &STI)
    : NovaGenInstrInfo(Nova::ADJCALLSTACKDOWN, Nova::ADJCALLSTACKUP),
      Subtarget(STI) { }



