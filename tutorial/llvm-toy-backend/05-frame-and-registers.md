# 04 Frame And Registers

## 学习目标

- 理解 `FrameLowering`、`RegisterInfo`、`InstrInfo` 在栈帧处理中的配合
- 理解 `storeRegToStackSlot` 和 `eliminateFrameIndex` 分别解决什么问题
- 理解 PEI 在后端流程里的位置

## 这一节按什么目标来学

这一节不要只看成“栈帧实现细节”。  
更适合的读法是:

- 我现在想让 LLVM 的栈访问和寄存器保存往前走一步
- 为了做到这件事, 我要补哪些函数

## 目标 -> 需要实现什么函数

### 目标 1: LLVM 能知道哪些寄存器不能乱分配

优先实现:

- `getReservedRegs`

你在实现的是:

- “寄存器分配器有哪些寄存器不能碰”

常见对象:

- `sp`
- `fp`
- `zero`

### 目标 2: LLVM 能知道函数用了哪些 callee-saved registers 要保存

优先实现:

- `getCalleeSavedRegs`
- `getCallPreservedMask`

你在实现的是:

- “哪些寄存器是被调用者负责保留的”

这一步不通时, 后面保存/恢复寄存器的逻辑会很混乱。

### 目标 3: LLVM 能决定栈访问默认相对哪个寄存器

优先实现:

- `getFrameRegister`

你在实现的是:

- “栈槽地址最后是相对 `sp` 还是 `fp` 来算”

### 目标 4: LLVM 能把寄存器保存到栈上

优先实现:

- `storeRegToStackSlot`

你在实现的是:

- “如果要 spill 一个寄存器, 这个 target 到底该发哪条 store 指令”

如果这一步没做, 常见现象就是:

- PEI 想保存寄存器时直接失败

### 目标 5: LLVM 能把寄存器从栈上恢复回来

优先实现:

- `loadRegFromStackSlot`

你在实现的是:

- “如果要 reload 一个寄存器, 这个 target 到底该发哪条 load 指令”

### 目标 6: LLVM 能把抽象 `FrameIndex` 变成真实地址

优先实现:

- `eliminateFrameIndex`

你在实现的是:

- “把编译器中间阶段的抽象栈槽编号, 改写成真实寄存器加偏移”

如果这一步没做, 后面就算前面 pass 都过了, 到真正输出机器指令时也会卡住。

### 目标 7: LLVM 能在函数入口和出口真正调整栈

优先实现:

- `emitPrologue`
- `emitEpilogue`

你在实现的是:

- “函数一进来怎么改 `sp`”
- “函数返回前怎么把 `sp` 恢复”

这一步本质上是在让栈帧真正落地。

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

## 如果你只关心“某个现象还没通, 应该先看哪里”

### 现象: 栈相关 pass 一走到保存寄存器就失败

优先看:

- `storeRegToStackSlot`
- `getCalleeSavedRegs`

### 现象: 栈相关指令里还残留 `FrameIndex`

优先看:

- `eliminateFrameIndex`
- `getFrameRegister`

### 现象: 函数没有正确分配/回收栈空间

优先看:

- `emitPrologue`
- `emitEpilogue`

### 现象: 某些寄存器明明不该被分配却被拿去用了

优先看:

- `getReservedRegs`

## 先认识这三个类为什么会同时出现

初学者看到这一节最常见的困惑是:

- 栈帧不是应该由 `FrameLowering` 管吗?
- 为什么 `RegisterInfo` 也在管?
- 为什么 `InstrInfo` 也跑进来了?

答案是: 栈帧处理不是一个类单独做完的, 而是三个人分工合作。

- `FrameLowering`
  决定函数入口和出口怎么调整栈, 也就是 “栈长什么样”
- `RegisterInfo`
  决定用哪个寄存器作为 frame 基准, 哪些寄存器保留, `FrameIndex` 最后怎么变成真实寻址
- `InstrInfo`
  决定真要保存 / 恢复寄存器时, 具体发哪条 load/store 指令

如果用更生活化的话说:

- `FrameLowering` 负责定方案
- `RegisterInfo` 负责算地址和寄存器规则
- `InstrInfo` 负责真正下指令

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

这里的 PEI 通常指 Prologue/Epilogue Insertion。  
第一次学时你不用记住 pass 名字, 但要记住它的时机:

- 前面大部分 codegen 先决定 “程序要做什么”
- 到了比较后面, LLVM 才知道这个函数到底用了多少栈空间、哪些寄存器需要保存
- 所以 prologue/epilogue 不可能太早生成

这也是为什么很多和栈帧有关的 bug 都是“前面看起来没事, 后面突然炸”。

## `RegisterInfo` 负责什么

这一节里最重要的接口有:

- `getCalleeSavedRegs`
- `getCallPreservedMask`
- `getReservedRegs`
- `getFrameRegister`
- `eliminateFrameIndex`

第一次读 [ToyRegisterInfo.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.h) 和 [ToyRegisterInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.cpp) 时, 建议按下面这个顺序看:

1. `getReservedRegs`
2. `getFrameRegister`
3. `getCalleeSavedRegs`
4. `eliminateFrameIndex`

这是因为前 3 个先帮你建立 “寄存器规则”, 最后一个才是 “把抽象栈对象落地”。

### 直觉理解

- 哪些寄存器需要保存?
- 哪些寄存器不能随便用?
- 栈访问最终以哪个寄存器为基准?
- `FrameIndex` 最终怎么变成真实地址?

可以把这几个接口进一步翻成白话:

- `getReservedRegs`
  回答 “有哪些寄存器编译器自己不能拿来随便分配”
- `getFrameRegister`
  回答 “做栈访问时, 默认应该以谁为基准, `sp` 还是 `fp`”
- `getCalleeSavedRegs`
  回答 “如果函数里用到了这些寄存器, 进入函数时要先保存, 返回前要恢复”
- `eliminateFrameIndex`
  回答 “之前记成抽象栈槽的位置, 最后到底改写成哪一个寄存器加多少偏移”

你会发现它们都和 “寄存器策略” 有关, 而不是和 “指令格式” 本身有关。

## `InstrInfo` 为什么也参与栈帧

因为保存和恢复寄存器最终是通过机器指令完成的。

所以需要:

- `storeRegToStackSlot`
- `loadRegFromStackSlot`

第一次看 [ToyInstrInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.cpp) 时, 很容易忽略这两个函数, 觉得它们只是一些工具函数。  
其实它们正是 LLVM 在问你的 target:

- “如果我要把某个寄存器 spill 到栈上, 你希望我发哪条机器指令?”
- “如果我要把它 reload 回来, 又该发哪条机器指令?”

它们本质上是:

- “帮 PEI 生成保存寄存器的机器指令”
- “帮 PEI 生成恢复寄存器的机器指令”

也就是说, `InstrInfo` 在这里并不是参与“栈布局设计”, 而是在参与 “执行设计好的动作”。

## `FrameIndex` 的本质

编译器前半段并不急着知道 “局部变量距离 `sp` 几个字节”。

它先用抽象的:

- `FrameIndex`

表示栈对象。

第一次理解 `FrameIndex` 时, 你可以把它想成 “一个还没决算的栈槽编号”。

编译器前半段之所以不急着立刻把它变成 `sp + 12`、`sp + 24`, 是因为那时很多信息还没最终稳定:

- 栈帧总大小可能还会变
- 还不确定会不会用 frame pointer
- 还不确定 callee-saved registers 会占多少空间
- 局部变量和临时 spill slot 的最终相对位置可能还会调整

所以先用抽象编号表示, 等后面信息收齐了再统一结算。

后面才在 `eliminateFrameIndex` 阶段把它改成:

- 真实基寄存器, 如 `sp`
- 真实偏移量

### 所以 `eliminateFrameIndex` 在做什么

把类似:

- `STORE ra, frameindex, 0`

改成:

- `STORE ra, sp, offset`

更准确一点说, 它做了两件事:

1. 选一个真实基寄存器
2. 计算最终偏移, 把 `FrameIndex` 操作数改写掉

这一步完成后, 那条机器指令才真正具备了可以打印/编码的地址形式。

如果这里没做好, 你后面即使到了 `AsmPrinter`, 也会因为还残留抽象操作数而出问题。

## 这一节最容易出的问题

### `storeRegToStackSlot` 没实现

PEI 想 spill CSR 时会直接失败。

这是因为 LLVM 已经知道 “这个寄存器得保存”, 但它不知道 “你这个架构要用哪条指令保存它”。

### `eliminateFrameIndex` 空实现

看似前面都通了, 最后会在栈访问阶段卡死或者反复产生错误指令。

这是因为中间阶段还能容忍抽象 `FrameIndex`, 但越往后越必须要真实地址。

### 忽略 constant offset

只处理裸 `FrameIndex` 不够, 数组元素和结构体字段会带额外 offset。

这一点非常像真实后端开发里的常见坑:

- 你最开始用最简单例子测试都过了
- 一碰到 `alloca` 后的带偏移访问或者聚合类型字段访问就错了

原因往往不是大逻辑错了, 而是你只处理了 “`FI` 本身”, 没把附带偏移一起算进去。

## 这一节完成后的阶段目标

这一节完成不等于:

- 你的后端已经能正确出所有函数

更合理的完成标准是:

- LLVM 已经知道哪些寄存器要保留
- 能为栈访问选出真实基寄存器
- 能把 `FrameIndex` 改写成真实地址
- 能在函数入口出口插入最小栈调整代码

## 注意事项

- `FrameLowering` 管 “什么时候调整栈”
- `RegisterInfo` 管 “用哪个寄存器和哪些偏移”
- `InstrInfo` 管 “具体生成哪条机器指令”

三者分工不同, 不要混着理解。

第一次读代码时, 建议你专门带着这三个问题去看:

1. 谁决定栈增减指令插在哪里?
2. 谁决定某个栈槽最后相对哪个寄存器寻址?
3. 谁真正构造出那条 `store` / `load` 指令?

你能把答案分别对回 `FrameLowering`、`RegisterInfo`、`InstrInfo`, 这一节就清楚很多了。

## 自查问题

1. 为什么 `emitPrologue` 一定发生在寄存器分配之后?
2. `storeRegToStackSlot` 和 `eliminateFrameIndex` 的职责为什么不能合并?
3. `FrameIndex` 为什么要先保留成抽象形式, 而不是一开始就直接变成 `sp + offset`?
