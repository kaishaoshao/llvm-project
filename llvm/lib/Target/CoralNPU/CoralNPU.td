// 总入口
include "llvm/Target/Target.td"

// 负责 mode / feature
include "CoralNPUSubtarget.td"

// 管寄存器和寄存器类
include "CoralNPURegisterInfo.td"

// 定义：这个 target 的指令系统入口名字是什么
include "CoralNPUInstrInfo.td"


def CoralNPU : Target {
  // 这一步是在告诉 LLVM: 这个 target 叫 CoralNPU
  // 它的指令系统入口是 CoralNPUInstrInfo
  let InstructionSet = CoralNPUInstrInfo;
}
