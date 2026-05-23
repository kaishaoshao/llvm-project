# 03 ISel And ASM Path

## 学习目标

- 看懂一条最简单的 IR 是如何被选成机器指令的
- 理解 `SelectionDAG -> MachineInstr -> MCInst -> asm` 的闭环
- 知道 `.td` 中的 pattern 在什么时候起作用

## 对应 toy 章节

- `toy-11: isel`
- `toy-15: emitInstruction`
- `toy-16: printInst`
- `toy-17: add registers`
- `toy-18: add more insns`
- `toy-19: simplify insn definition`

## 必看文件

- [ToyInstrFormats.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrFormats.td)
- [ToyInstrInfo.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.td)
- [ToyDAGToDAGISel.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.cpp)
- [ToyAsmPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.cpp)
- [ToyMCInstLower.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyMCInstLower.cpp)
- [TargetDesc/ToyInstPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.cpp)
- [ToyRegisterInfo.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.td)

## 最小闭环是什么

教程在这一阶段追求的不是“支持很多指令”, 而是让最简单的事情能通:

- 常量
- `addi`
- `load/store`
- `ret`

只要这几条打通, 你就第一次拿到了完整流水线。

## `.td` 中的 pattern 在哪里起作用

### `ToyInstrInfo.td`

这里定义的 pattern 会在 TableGen 生成的 matcher 中使用。

也就是说:

- LLVM 不是手写一堆 if/else 去做基本指令匹配
- 它会用 `.td` 里的 pattern 生成匹配表

`toy-11` 里最值得观察的就是:

- 常量如何被匹配到 `ADDI`
- `FrameIndex` 相关访问如何被匹配到 `STORE` / `LOAD`

## `emitInstruction` 为什么关键

在 `toy-14` 之后, 教程报错说:

- `EmitInstruction not implemented`

这说明 codegen 其实已经走到了 `AsmPrinter` 阶段, 但 target 还没有把 `MachineInstr` 转成 `MCInst`。

所以:

- `DAGToDAGISel` 不是最后一步
- 后端真正输出汇编前, 还要经过 `AsmPrinter`

## `printInst` 又在补哪一层

`emitInstruction` 解决的是:

- `MachineInstr -> MCInst`

而 `printInst` 解决的是:

- `MCInst -> asm text`

这两层一定不要混。

## 这一阶段最重要的知识点

### `MachineInstr`

- 已经是目标相关的机器指令表示
- 但仍然不是最终文本汇编

### `MCInst`

- 更接近汇编/编码层
- 后续可以打印成 asm, 也可以编码成 binary

### `MCOperand`

- `MachineOperand` 不能直接拿去打印
- 通常需要 lowering 成 `MCOperand`

## 注意事项

- 如果 `InstPrinter` 写对了, 但 `AsmPrinter::emitInstruction` 没实现, 你还是出不了 asm
- 如果 `.td` pattern 不足, 很多 DAG 节点会在 isel 时直接崩
- 如果寄存器类没定义好, `gen-dag-isel` 和调度阶段都会继续出问题

## 自查问题

1. `emitInstruction` 和 `printInst` 分别处理哪两层数据结构?
2. 为什么 `ToyInstrInfo.td` 里的 pattern 能自动参与指令选择?
3. 为什么最小闭环只支持 `addi/load/store/ret` 也有很高学习价值?

