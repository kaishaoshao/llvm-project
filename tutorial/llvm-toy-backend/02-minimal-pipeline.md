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

### `ToyDAGToDAGISel`

把 SelectionDAG 节点选成 target 的机器指令。

当前阶段它先是一个入口占位, 让 pass pipeline 能继续往下走。

### `ToyInstPrinter`

负责 `MCInst -> asm`。

注意:

- 它不处理 `MachineInstr`
- 它不做寄存器分配
- 它不决定调用约定

### `ToyAsmPrinter`

负责 `MachineInstr -> MCInst`。

它和 `ToyInstPrinter` 的分工是这一节最重要的理解点。

### `ToyTargetObjectFile`

负责 object file 相关的 section 和数据放置规则。

即使你现在还不真正输出 `.o`, `AsmPrinter` 也会依赖它。

### `ToySubtarget`

是后端能力的聚合点。通常它会持有:

- `InstrInfo`
- `RegisterInfo`
- `FrameLowering`
- `TargetLowering`

LLVM 的很多 pass 都是先拿 `Subtarget`, 再从里面取这些对象。

### `ToyTargetLowering`

负责 target-specific lowering。

哪怕你暂时只支持很小的程序, 它也得存在, 因为函数返回和调用这类事情天然依赖 target。

### `ToyFrameLowering`

负责 prologue / epilogue 和栈帧约定。

这里先有最小骨架, 后续再逐步补行为。

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

## 注意事项

- `InstPrinter` 处理的是 `MCInst`, 不是 `MachineInstr`
- `AsmPrinter` 必须实现的核心函数是 `emitInstruction`
- `TargetObjectFile` 虽然看起来“像以后再说”, 实际上很早就会被依赖
- `Subtarget` 不是可有可无的封装, 它是 pass 查询 target 能力的主要入口

## 自查问题

1. `ToyAsmPrinter` 和 `ToyInstPrinter` 的边界在哪里?
2. 为什么 `TargetLowering` 即使在后端很小的时候也必须存在?
3. 为什么 `Subtarget` 会成为 `getRegisterInfo()` / `getInstrInfo()` 的统一入口?

