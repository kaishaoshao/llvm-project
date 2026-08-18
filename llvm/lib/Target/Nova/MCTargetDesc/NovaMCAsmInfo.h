#ifndef LLVM_LIB_TARGET_NOVA_MCTARGETDESC_NOVAMCASMINFO_H
#define LLVM_LIB_TARGET_NOVA_MCTARGETDESC_NOVAMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;
class NovaMCAsmInfo final : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit NovaMCAsmInfo(const Triple &TheTriple);
};
} // end of namespace llvm

#endif // LLVM_LIB_TARGET_NOVA_MCTARGETDESC_NOVAMCASMINFO_H
