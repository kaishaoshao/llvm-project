//===-- NovaSubtarget.h - Define Subtarget for the Nova --------*- C++ -*-===//
#ifndef LLVM_LIB_TARGET_NOVA_NOVASUBTARGET_H
#define LLVM_LIB_TARGET_NOVA_NOVASUBTARGET_H

#include "NovaFrameLowering.h"
#include "NovaISelLowering.h"
#include "NovaInstrInfo.h"
#include "NovaRegisterInfo.h"

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"

#define GET_SUBTARGETINFO_HEADER
#include "NovaGenSubtargetInfo.inc"

namespace llvm {
class NovaSubtarget : public NovaGenSubtargetInfo {
protected:
  // 信息字段
  SelectionDAGTargetInfo TSInfo;
  NovaInstrInfo InstrInfo;
  NovaFrameLowering FrameLowering;
  NovaTargetLowering TLInfo;
  NovaRegisterInfo RegInfo;

public:
  NovaSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                const TargetMachine &TM)
      : NovaGenSubtargetInfo(TT, CPU, CPU, FS), InstrInfo(*this),
        FrameLowering(*this, Align(8)), TLInfo(TM, *this) {}

  const NovaRegisterInfo *getRegisterInfo() const override { return &RegInfo; }

  const NovaInstrInfo *getInstrInfo() const override { return &InstrInfo; }

  const NovaFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }

  const NovaTargetLowering *getTargetLowering() const override {
    return &TSInfo;
  }

  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }

  /// 解析子目标特性 - 解析指定的特性字符串设置
  /// 子目标选项。函数的定义是由 tblgen 自动生成的。
  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

};
} // end of  namespcae llvm

#endif // LLVM_LIB_TARGET_NOVA_NOVASUBTARGET_H
