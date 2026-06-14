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

## 先把整条路线看成任务列表

如果你现在更需要一张“我接下来到底做什么”的清单, 可以先把整个教程压缩成下面 7 个任务:

1. 让 `llc --version` 看到 `coralnpu32`
2. 让 `llc -mtriple=coralnpu32 test.ll` 不再报 unknown target
3. 让 `CoralNPUTargetMachine` 真正开始被创建
4. 让 `TargetMC` 不再报 `Unable to create reg info`
5. 让 `Subtarget` 和 `TargetLowering/FrameLowering/RegisterInfo` 链接起来
6. 让 `DAGToDAGISel` 和 `AsmPrinter` 开始接进 pipeline
7. 让最小 IR 用例能真正往目标汇编方向推进

你后面读每一节时, 最好都先问:

- 这一节是在帮我完成上面哪一个任务?
- 完成后我应该看到什么新现象?

## 为什么一定要按这个顺序做

很多初学者最容易困惑的是:

- 为什么先做 `TargetMachine`
- 为什么不是先做 `Subtarget`
- 为什么不是先做 `InstrInfo` 或 `ISel`

最关键的原因不是“哪一块更重要”, 而是:

- LLVM 运行时就是按这个顺序来要对象的

也就是说, 顺序不是我们主观规定的, 而是 `llc` 真的会按下面这条链一路往下问:

1. 你是谁
2. 你的后端总入口是谁
3. 你的低层 MC 对象怎么创建
4. 你的 `Subtarget` 在哪里
5. 你的 `InstrInfo/RegisterInfo/FrameLowering/TargetLowering` 在哪里
6. 你的 isel pass 怎么接进 pipeline

所以教程里的顺序本质上是在跟着 LLVM 的真实调用顺序走。

## 一条最实用的创建顺序图

如果把一个新 target 被 `llc` 使用时的对象关系压缩成一条线, 最适合记成:

`名字`
-> `Target`
-> `TargetMachine`
-> `TargetMC`
-> `Subtarget`
-> `InstrInfo/RegisterInfo/FrameLowering/TargetLowering`
-> `ISel/AsmPrinter`

这条顺序背后的含义是:

- `名字`
  先靠 `Triple` 和 `TargetInfo` 识别出来
- `Target`
  是 LLVM 注册表里挂着的 target 对象
- `TargetMachine`
  是真正进入后端 codegen 的总入口
- `TargetMC`
  是 `TargetMachine` 初始化后很快就会依赖的底层工厂
- `Subtarget`
  通常由 `TargetMachine` 持有或创建
- `InstrInfo/RegisterInfo/FrameLowering/TargetLowering`
  通常再由 `Subtarget` 提供

所以:

- 没有 `TargetMachine`, LLVM 就没法继续要 `Subtarget`
- 没有 `Subtarget`, LLVM 也没法继续要 `InstrInfo`、`FrameLowering` 这些能力对象

这也是为什么学习顺序不能随意打乱。

## LLVM 后端的主要层次

### 从高到低

- LLVM IR
- SelectionDAG
- MachineInstr
- MCInst
- asm / object file

第一次学 LLVM 后端, 最容易犯的错是把这些层混在一起。你可以先用一句话抓住它们:

- LLVM IR
  还是目标无关的程序表示, 关心语义, 不关心具体寄存器和指令编码
- SelectionDAG
  是 codegen 前半段用来做指令选择和合法化的一种图结构
- MachineInstr
  已经是 “某个 target 的机器指令”, 但还不是最终文本汇编
- MCInst
  是更靠近汇编器和编码器的低层指令表示
- asm / object file
  才是最终用户能看到或链接器能处理的产物

你以后看到某个类时, 第一反应都应该先问:

- 它处理的是哪一层的数据结构?

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

如果把这些模块按“谁最先接触源码、谁最后接近机器码”排一下, 大致就是:

`TargetLowering` -> `DAGToDAGISel` -> `InstrInfo/RegisterInfo/FrameLowering` -> `AsmPrinter` -> `MCInstPrinter/MCCodeEmitter`

这条顺序不是绝对的调用栈, 但非常适合刚入门时建立方向感。

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

你可以先把 `.td` 和 `C++` 简化理解成:

- `.td` 回答 “这个 target 有什么”
- `C++` 回答 “这些东西在运行时怎么工作”

例如:

- 寄存器名字、寄存器类、指令格式, 更适合写在 `.td`
- “某个局部变量最后离 `sp` 多远”, 更适合写在 `C++`
- “函数参数先用 `a0/a1` 还是直接落栈”, 规则可写在 `.td`, 真正搬运参数的逻辑则在 `C++`

这条分工理解清楚后, 你看到一个新文件时就更容易判断它为什么存在。

## 先认识目录长什么样

在一个 target 目录下, 你通常会看到几类文件:

- `TargetInfo/`
  做 target 名字注册, 让 LLVM “知道有这个后端”
- `TargetDesc/` 或 `MCTargetDesc/`
  做 MC 层描述和工厂注册
- `XXXTargetMachine.*`
  整个后端顶层入口
- `XXXSubtarget.*`
  某个 CPU / feature 组合的能力集合
- `XXXISelLowering.*`
  LLVM IR / DAG 到 target 语义的 lowering
- `XXXDAGToDAGISel.*`
  DAG 到机器指令选择
- `XXXInstrInfo.*`
  目标指令层面的行为
- `XXXRegisterInfo.*`
  寄存器、保留寄存器、frame register、FrameIndex 消解
- `XXXFrameLowering.*`
  栈帧布局与 prologue/epilogue
- `XXXAsmPrinter.*`
  `MachineInstr -> MCInst`
- `XXXMCInstLower.*`
  把单个机器操作数 lower 成 MC 层操作数
- `*.td`
  声明寄存器、指令、pattern、feature、calling convention 等

第一次看到这些目录时不用急着全懂, 先知道 “这不是随便分的”, 而是在对应 LLVM codegen 的不同层。

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

## 每推进一步, 你应该怎么验收

教程里每一步最好都用 “一个命令 + 一个现象” 验收。

例如:

- `llc --version`
  - 看 target 是否已注册
- `llc -mtriple=coralnpu32 test.ll`
  - 看 `TargetMachine` 是否开始创建
- `lldb -- ./build-debug/bin/llc -mtriple=coralnpu32 test.ll`
  - 看具体崩在 `lookupTarget`、`createTargetMachine`、`initAsmInfo` 还是更后面

如果没有这种“命令 -> 现象”的验收方式, 很容易出现:

- 写了很多代码
- 但不知道到底多走了哪一步

## 建议的学习策略

- 第一遍只看模块职责和报错因果
- 第二遍顺着代码追调用链
- 第三遍才尝试自己在当前仓库做一个独立 target

更具体一点, 第一遍你甚至可以只做三件事:

1. 看每节的 “必看文件”
2. 弄清每个文件大概在流水线哪一层
3. 记住这一节解决的报错是什么

这样你第二遍再读实现时, 大脑里已经有“地图”了, 不会只是被函数名推着走。

## 注意事项

- 不要上来先看 object file
- 不要试图一次性理解所有 `.td`
- 不要只看代码不看报错
- 不要把旧版 LLVM 的具体接口当成永远不变的事实

## 自查问题

1. LLVM 后端中 `TargetLowering` 和 `AsmPrinter` 分别工作在哪一层?
2. 为什么 `MCInstPrinter` 不应该直接处理 `MachineInstr`?
3. 为什么要把学习顺序拆成多步, 而不是一次性实现所有模块?
