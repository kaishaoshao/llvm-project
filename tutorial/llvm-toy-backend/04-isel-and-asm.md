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

这里一定要理解作者为什么故意只挑这几条:

- `addi`
  能让你看到立即数和简单整数运算
- `load/store`
  能让你第一次碰到栈槽和地址计算
- `ret`
  能让你看到函数结束路径

它们合起来已经足够覆盖:

- 指令定义
- 寄存器类
- pattern 匹配
- `MachineInstr -> MCInst -> asm`

所以这不是“功能很少”, 而是“学习密度很高”。

## `.td` 中的 pattern 在哪里起作用

### `ToyInstrInfo.td`

这里定义的 pattern 会在 TableGen 生成的 matcher 中使用。

也就是说:

- LLVM 不是手写一堆 if/else 去做基本指令匹配
- 它会用 `.td` 里的 pattern 生成匹配表

`toy-11` 里最值得观察的就是:

- 常量如何被匹配到 `ADDI`
- `FrameIndex` 相关访问如何被匹配到 `STORE` / `LOAD`

第一次读 `.td` 里的 pattern, 建议你先带着这个问题看:

- 左边描述的是什么 DAG 形状?
- 右边生成的是什么目标指令?

也就是说, 你不要把它当成普通宏定义, 而要把它当成:

- “告诉 TableGen: 遇到这种 DAG, 可以选成这条机器指令”

## `emitInstruction` 为什么关键

在 `toy-14` 之后, 教程报错说:

- `EmitInstruction not implemented`

这说明 codegen 其实已经走到了 `AsmPrinter` 阶段, 但 target 还没有把 `MachineInstr` 转成 `MCInst`。

所以:

- `DAGToDAGISel` 不是最后一步
- 后端真正输出汇编前, 还要经过 `AsmPrinter`

很多初学者会在这里第一次意识到:

- “我以为指令选择完就差不多了, 其实还差一整层”

这是因为 `MachineInstr` 仍然是 LLVM 内部的机器指令表示, 它不等于:

- 汇编字符串
- 机器码字节

`emitInstruction` 的存在, 正是在告诉你后端还有一段 “从 codegen 到 MC” 的过渡。

## `printInst` 又在补哪一层

`emitInstruction` 解决的是:

- `MachineInstr -> MCInst`

而 `printInst` 解决的是:

- `MCInst -> asm text`

这两层一定不要混。

如果你写后端时发现:

- `emitInstruction` 已经被调用了
- 但输出的 asm 还是不对

那通常就该看 `InstPrinter` 了。
如果连 `emitInstruction` 都没走到, 问题往往还在更前面的 codegen 或 `AsmPrinter`。

## 这一阶段最重要的知识点

### `MachineInstr`

- 已经是目标相关的机器指令表示
- 但仍然不是最终文本汇编

你可以把它理解成:

- “编译器内部拿来继续分析、调度、插入栈帧代码的机器指令对象”

它的操作数里还可能出现:

- 物理寄存器
- 虚拟寄存器
- `FrameIndex`
- immediate
- basic block 引用

所以它仍然是编译器内部对象, 不是给最终用户看的文本。

### `MCInst`

- 更接近汇编/编码层
- 后续可以打印成 asm, 也可以编码成 binary

`MCInst` 更像 “汇编器能看懂的统一指令容器”。它不关心前面那些 codegen 期的复杂语义, 更关心:

- opcode 是什么
- 操作数是什么
- 最终怎么打印或编码

### `MCOperand`

- `MachineOperand` 不能直接拿去打印
- 通常需要 lowering 成 `MCOperand`

这一点也很值得建立直觉:

-
- 面向 codegen, 信息更丰富
- `MCOperand`
  面向 MC 层, 结构更简单

所以像 [ToyMCInstLower.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyMCInstLower.cpp) 这样的文件才会存在。
它的职责就是把复杂一点的 codegen 操作数转换成 MC 能接受的形式。

## 注意事项

- 如果 `InstPrinter` 写对了, 但 `AsmPrinter::emitInstruction` 没实现, 你还是出不了 asm
- 如果 `.td` pattern 不足, 很多 DAG 节点会在 isel 时直接崩
- 如果寄存器类没定义好, `gen-dag-isel` 和调度阶段都会继续出问题

## 自查问题

1. `emitInstruction` 和 `printInst` 分别处理哪两层数据结构?
2. 为什么 `ToyInstrInfo.td` 里的 pattern 能自动参与指令选择?
3. 为什么最小闭环只支持 `addi/load/store/ret` 也有很高学习价值?
