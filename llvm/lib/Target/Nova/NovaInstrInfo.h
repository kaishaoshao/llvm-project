// 表格生成的指令记录存储在NovaInstrInfo类中。
// 遵循常见的表格生成模式，我们的类继承自 NovaGenInstrInfo该类。

#ifndef LLVM_LIB_TARGET_NOVA_NOVAINSTRINFO_H
#define LLVM_LIB_TARGET_NOVA_NOVAINSTRINFO_H

#include "Nova.h"
#include "NovaRegisterInfo.h"



#define GET_INSTRINFO_HEADER
#include "NovaGenInstrInfo.inc"

namespace llvm {
class NovaSubtarget;

class NovaInstrInfo : public NovaGenInstrInfo {
public:
  explicit NovaInstrInfo(const NovaSubtarget &STI);

protected:
  const NovaSubtarget &Subtarget;
};
}  // end namespace llvm

#endif
