#ifndef LLVM_LIB_TARGET_CORALNPU_MCTARGETDESC_CORALNPUTARGETDESC_H
#define LLVM_LIB_TARGET_CORALNPU_MCTARGETDESC_CORALNPUTARGETDESC_H

#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/DataTypes.h"

#define GET_REGINFO_ENUM
#include "CoralNPUGenRegisterInfo.inc"

// #define GET_INSTRINFO_ENUM
// #include "CoralNPUGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "CoralNPUGenSubtargetInfo.inc"

#endif
