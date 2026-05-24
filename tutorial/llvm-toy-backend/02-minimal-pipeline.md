# 02 Minimal Pipeline Skeleton

## 学习目标

- 理解后端最小骨架为什么需要 `Subtarget`、`TargetLowering`、`FrameLowering`
- 理解教程中 `toy-4` 到 `toy-10` 的推进顺序
- 分清 “先占位” 和 “开始真正出码” 的边界

## 对应 toy 章节

- `toy-4: ToyDAGToDAGISel`
- `toy-5: ToyInstPrinter`
- `toy-6: ToyAsmPrinter`
- `toy-7: ToyTargetObjectFile`
- `toy-8: ToySubtarget`
- `toy-9: ToyTargetLowering`
- `toy-10: ToyFrameLowering`

## 必看文件

- [ToyDAGToDAGISel.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.h)
- [ToyDAGToDAGISel.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.cpp)
- [ToyAsmPrinter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.h)
- [ToyAsmPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.cpp)
- [ToyTargetObjectFile.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetObjectFile.h)
- [ToyTargetObjectFile.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetObjectFile.cpp)
- [ToySubtarget.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.h)
- [ToySubtarget.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.cpp)
- [ToyISelLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.h)
- [ToyISelLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.cpp)
- [ToyFrameLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.h)
- [ToyFrameLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.cpp)
- [TargetDesc/ToyInstPrinter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.h)
- [TargetDesc/ToyInstPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.cpp)

## 这几个模块各自负责什么

在读这些类之前, 你可以先记一个总原则:

- 这一阶段出现的很多类, 不是因为它们已经“功能完整”
- 而是因为 LLVM 的 codegen pipeline 需要它们先占住位置

也就是说, 这一节的重点是 “让骨架站起来”, 不是 “每个模块都做完”

### `ToyDAGToDAGISel`

把 SelectionDAG 节点选成 target 的机器指令。

当前阶段它先是一个入口占位, 让 pass pipeline 能继续往下走。

第一次读这个类时, 先只抓两个点:

- 它属于 SelectionDAG 阶段
- 它的输出已经是 `MachineInstr` 方向的目标相关表示

你现在还不必把 matcher 细节看透, 但一定要知道:

- `TargetLowering` 更像 “先把问题翻译成 target 能懂的话”
- `DAGToDAGISel` 更像 “再把这些 DAG 节点选成具体指令”

### `ToyInstPrinter`

负责 `MCInst -> asm`。

注意:

- 它不处理 `MachineInstr`
- 它不做寄存器分配
- 它不决定调用约定

这是一个非常容易被误解的类。它名字里有 `Printer`, 很多人第一次会以为:

- “那它是不是整个后端最后打印汇编的总入口?”

其实不是。它只是专门负责:

- 已经 lower 成 `MCInst` 之后
- 把 opcode、寄存器、立即数格式化成汇编文本

所以它更像 “汇编文本格式化器”, 而不是 codegen 主流程控制器。

### `ToyAsmPrinter`

负责 `MachineInstr -> MCInst`。

它和 `ToyInstPrinter` 的分工是这一节最重要的理解点。

如果一定要给它一句最直白的描述:

- `ToyAsmPrinter` 站在 codegen 和 MC 的交界处

前面 LLVM 还在操作 `MachineInstr`, 到了它这里开始准备进入:

- `MCInst`
- 汇编文本
- 机器码编码

所以这个类在整体流水线中的位置非常关键。

### `ToyTargetObjectFile`

负责 object file 相关的 section 和数据放置规则。

即使你现在还不真正输出 `.o`, `AsmPrinter` 也会依赖它。

第一次学时你可以先把它理解成:

- “目标文件格式相关的后台规则对象”

它平时不一定很显眼, 但一旦涉及:

- 全局变量放哪个 section
- 常量池放哪里
- 某些符号怎么布局

它就会参与进来。

### `ToySubtarget`

是后端能力的聚合点。通常它会持有:

- `InstrInfo`
- `RegisterInfo`
- `FrameLowering`
- `TargetLowering`

LLVM 的很多 pass 都是先拿 `Subtarget`, 再从里面取这些对象。

`Subtarget` 是第一阶段之后最值得你建立直觉的类之一。它不是单独干某个具体活, 而是:

- 把 “这台具体 CPU 的能力” 聚合成一个对象

第一次阅读它时, 你可以重点观察:

- 它持有哪些成员对象
- 构造函数里初始化了哪些能力
- `TargetMachine` 是怎么把它暴露给别的 pass 的

### `ToyTargetLowering`

负责 target-specific lowering。

哪怕你暂时只支持很小的程序, 它也得存在, 因为函数返回和调用这类事情天然依赖 target。

这是初学者最容易低估的类。很多人会想:

- “我不是还没做复杂指令吗, 为什么这么早就要 `TargetLowering`?”

因为 LLVM IR 里有很多语义不是 target-neutral 地直接变成机器指令的, 比如:

- 函数参数怎么进来
- 返回值怎么出去
- 某些整数操作要不要扩展
- 某个操作能不能直接用本机指令表示

这些都需要 target 来给出答案。

### `ToyFrameLowering`

负责 prologue / epilogue 和栈帧约定。

这里先有最小骨架, 后续再逐步补行为。

第一次看它时不要被“栈帧”三个字吓到。此时你只需要先记住:

- 函数一进入时, 谁来决定 `sp` 怎么减?
- 函数要返回时, 谁来决定 `sp` 怎么恢复?

答案通常就是 `FrameLowering`。

## 为什么教程按这个顺序推进

教程不是凭感觉写的, 而是由下一条断言决定顺序:

- 没 `TargetMachine`
  就先补 `toy-2`
- 没 `MCRegisterInfo`
  就先补 `toy-3`
- 没 `TargetLowering`
  就先补 `toy-9`
- 没 `FrameLowering`
  就先补 `toy-10`

你以后自己做后端时也应该沿用这条策略。

## 这一阶段最该抓住的调用关系

### `TargetMachine -> Subtarget`

`TargetMachine` 要能通过 `getSubtargetImpl` 返回一个 `Subtarget`

### `Subtarget -> Lowering/Frame/Reg/Instr`

`Subtarget` 要能提供:

- `getTargetLowering`
- `getFrameLowering`
- `getRegisterInfo`
- `getInstrInfo`

### `AsmPrinter -> MCInstLower -> InstPrinter`

当前最重要的下游链条是:

- `MachineInstr`
- `AsmPrinter`
- `MCInst`
- `InstPrinter`
- asm

这条链你最好反复记。因为从这一节开始, 很多“我明明已经有指令了, 为什么还没出汇编”的困惑, 都是在这里解开的。

更直观地说:

- `DAGToDAGISel` 负责“选出机器指令”
- `AsmPrinter` 负责“把机器指令变成 MC 层指令”
- `InstPrinter` 负责“把 MC 层指令变成字符串”

每一层都少不了。

## 注意事项

- `InstPrinter` 处理的是 `MCInst`, 不是 `MachineInstr`
- `AsmPrinter` 必须实现的核心函数是 `emitInstruction`
- `TargetObjectFile` 虽然看起来“像以后再说”, 实际上很早就会被依赖
- `Subtarget` 不是可有可无的封装, 它是 pass 查询 target 能力的主要入口

## 自查问题

1. `ToyAsmPrinter` 和 `ToyInstPrinter` 的边界在哪里?
2. 为什么 `TargetLowering` 即使在后端很小的时候也必须存在?
3. 为什么 `Subtarget` 会成为 `getRegisterInfo()` / `getInstrInfo()` 的统一入口?
