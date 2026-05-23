# 00 Overview

## 学习目标

- 理解 LLVM 后端的基本流水线
- 知道 `llvm-toy` 教程为什么按现在这个顺序展开
- 建立 `.td` 和 `C++` 两条线并行推进的意识

## 对应 toy 章节

- `toy-1` 到 `toy-49` 的总览

## 核心观点

这套 toy 教程最重要的地方, 不是它实现了一个 Toy RISC-V, 而是它展示了 LLVM 后端最适合学习的推进方式:

1. 先让 `llc` 认识 target
2. 再让 `TargetMachine` 能创建
3. 再补 MC 元信息
4. 再补最小 isel 和 asm 输出
5. 再补栈帧和寄存器
6. 再补调用约定
7. 最后补浮点和 object file

这是一条 “每次只让编译器多走一步” 的路线。

## LLVM 后端的主要层次

### 从高到低

- LLVM IR
- SelectionDAG
- MachineInstr
- MCInst
- asm / object file

### 关键模块对应关系

- `TargetLowering`
  负责把 IR 相关语义降到 SelectionDAG 的 target-specific 节点
- `DAGToDAGISel`
  负责把 DAG 节点选成机器指令
- `InstrInfo`
  负责 target 指令层面的行为
- `RegisterInfo`
  负责寄存器、callee-saved、frame register 等
- `FrameLowering`
  负责 prologue / epilogue / 栈帧布局
- `AsmPrinter`
  负责 `MachineInstr -> MCInst`
- `MCInstPrinter`
  负责 `MCInst -> asm`
- `MCCodeEmitter`
  负责 `MCInst -> binary`

## `.td` 和 `C++` 的分工

### `.td` 更偏声明

- target
- feature
- 寄存器
- 寄存器类
- 指令格式
- 指令 pattern
- calling convention

### `C++` 更偏行为

- 初始化注册
- lowering
- 栈帧处理
- MachineInstr 构造
- MC 层转换

## 为什么教程顺序重要

如果一开始就试图把整个后端写完整, 你会同时卡在:

- CMake 接线
- Triple 注册
- TargetMachine 初始化
- TableGen
- isel
- 栈帧
- 调用约定
- asm/object emission

toy 教程的顺序恰好是把这些问题串成一条因果链。前一步的报错, 就是下一步的学习入口。

## 建议的学习策略

- 第一遍只看模块职责和报错因果
- 第二遍顺着代码追调用链
- 第三遍才尝试自己在当前仓库做一个独立 target

## 注意事项

- 不要上来先看 object file
- 不要试图一次性理解所有 `.td`
- 不要只看代码不看报错
- 不要把旧版 LLVM 的具体接口当成永远不变的事实

## 自查问题

1. LLVM 后端中 `TargetLowering` 和 `AsmPrinter` 分别工作在哪一层?
2. 为什么 `MCInstPrinter` 不应该直接处理 `MachineInstr`?
3. 为什么要把学习顺序拆成多步, 而不是一次性实现所有模块?

