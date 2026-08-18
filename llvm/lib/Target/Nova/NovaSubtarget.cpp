#include "Nova.h"
#include "NovaSubtarget.h"
#include "NovaRegisterInfo.h"
#include "NovaTargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "nova-subtarget"

#define GET_SUBTARGETINFO_CTOR
#define GET_SUBTARGETINFO_TARGET_DESC
#include "NovaGenSubtargetInfo.inc"
