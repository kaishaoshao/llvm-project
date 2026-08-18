#include "NovaMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void NovaMCAsmInfo::anchor() {}

NovaMCAsmInfo::NovaMCAsmInfo(const Triple &TT) {
  IsLittleEndian = false;
  AlignmentIsInBytes = true;
}
