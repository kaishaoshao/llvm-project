# 04 Frame And Registers

## 学习目标

- 理解 `FrameLowering`、`RegisterInfo`、`InstrInfo` 在栈帧处理中的配合
- 理解 `storeRegToStackSlot` 和 `eliminateFrameIndex` 分别解决什么问题
- 理解 PEI 在后端流程里的位置

## 对应 toy 章节

- `toy-10: ToyFrameLowering`
- `toy-12: ToyRegisterInfo`
- `toy-13: storeRegToStackSlot`
- `toy-14: eliminateFrameIndex`
- `toy-25: emitPrologue and emitEpilogue`
- `toy-32: frameindex with constant offset`

## 必看文件

- [ToyFrameLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.h)
- [ToyFrameLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.cpp)
- [ToyRegisterInfo.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.h)
- [ToyRegisterInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.cpp)
- [ToyInstrInfo.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.h)
- [ToyInstrInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.cpp)
- [ToyCallingConv.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyCallingConv.td)

## 先建立正确的时序感

### `emitPrologue` / `emitEpilogue` 在什么时候执行

它们发生在:

- 调度之后
- 寄存器分配之后

因此它们操作的是:

- `MachineInstr`
- 物理寄存器
- 已经确定的 stack size

这就是为什么教程反复强调 PEI 的时机。

## `RegisterInfo` 负责什么

这一节里最重要的接口有:

- `getCalleeSavedRegs`
- `getCallPreservedMask`
- `getReservedRegs`
- `getFrameRegister`
- `eliminateFrameIndex`

### 直觉理解

- 哪些寄存器需要保存?
- 哪些寄存器不能随便用?
- 栈访问最终以哪个寄存器为基准?
- `FrameIndex` 最终怎么变成真实地址?

## `InstrInfo` 为什么也参与栈帧

因为保存和恢复寄存器最终是通过机器指令完成的。

所以需要:

- `storeRegToStackSlot`
- `loadRegFromStackSlot`

它们本质上是:

- “帮 PEI 生成保存寄存器的机器指令”
- “帮 PEI 生成恢复寄存器的机器指令”

## `FrameIndex` 的本质

编译器前半段并不急着知道 “局部变量距离 `sp` 几个字节”。

它先用抽象的:

- `FrameIndex`

表示栈对象。

后面才在 `eliminateFrameIndex` 阶段把它改成:

- 真实基寄存器, 如 `sp`
- 真实偏移量

### 所以 `eliminateFrameIndex` 在做什么

把类似:

- `STORE ra, frameindex, 0`

改成:

- `STORE ra, sp, offset`

## 这一节最容易出的问题

### `storeRegToStackSlot` 没实现

PEI 想 spill CSR 时会直接失败。

### `eliminateFrameIndex` 空实现

看似前面都通了, 最后会在栈访问阶段卡死或者反复产生错误指令。

### 忽略 constant offset

只处理裸 `FrameIndex` 不够, 数组元素和结构体字段会带额外 offset。

## 注意事项

- `FrameLowering` 管 “什么时候调整栈”
- `RegisterInfo` 管 “用哪个寄存器和哪些偏移”
- `InstrInfo` 管 “具体生成哪条机器指令”

三者分工不同, 不要混着理解。

## 自查问题

1. 为什么 `emitPrologue` 一定发生在寄存器分配之后?
2. `storeRegToStackSlot` 和 `eliminateFrameIndex` 的职责为什么不能合并?
3. `FrameIndex` 为什么要先保留成抽象形式, 而不是一开始就直接变成 `sp + offset`?

