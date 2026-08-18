#ifndef LLVM_LIB_TARGET_NOVA_NOVATARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_NOVA_NOVATARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {
class TargetMachine;
class MCContext;
class NovaTargetObjectFile final : public TargetLoweringObjFileELF {
public:
  void Initialize(MCContext &Ctx, const TargetMachine &TM) override;
};
} namespace llvm

#endif // LLVM_LIB_TARGET_NOVA_NOVATARGETOBJECTFILE_H
