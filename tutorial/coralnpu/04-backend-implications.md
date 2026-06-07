# 04 Backend Implications

## 学习目标

- 把前面 CoralNPU 文档里的架构信息翻译成“后端实现影响”
- 明确它和你之前的最小 RISC-V backend 学习路径到底差在哪
- 给你一个更现实的 CoralNPU backend 学习路线

## 先回答一个核心问题

如果你把之前的最小 RISC-V backend 改成 `CoralNPU`, 你到底是在改什么?

最容易犯的错是:

- 只是把名字从旧的教学 target 改成 `CoralNPU`

这样当然可以让 `llc --version` 里出现一个新 target, 但这还不是真正的 CoralNPU backend。

## 真正的差异会落在哪

### 1. Triple / TargetInfo / TargetMachine 仍然要做

也就是说, 你前面学过的:

- `Triple`
- `TargetInfo`
- `TargetMachine`
- `MCTargetDesc`

对 CoralNPU 仍然成立。

这部分你完全可以沿用 “先让 LLVM 识别 target” 的套路。

### 2. 但寄存器体系不该停留在普通 RV32

从 CoralNPU 文档看, 真正像它的地方至少包括:

- 标量寄存器
- 向量寄存器
- 可能的 accumulator 语义

所以后端迟早要面对:

- 多寄存器类
- 向量类型
- 向量/矩阵指令格式

### 3. 指令集重点不该只放在普通整数指令

如果只是补:

- `add`
- `sub`
- `lw`
- `sw`

你学到的是 RISC-V 风格最小后端, 还不是 CoralNPU 的重点。

CoralNPU 更有代表性的方向会是:

- SIMD 指令
- 向量 load/store
- MAC / dot-product 类能力

### 4. 最终很可能需要 MLIR 视角

因为 CoralNPU 的价值非常偏 ML 工作负载。  
所以真正像样的 CoralNPU 编译链很可能不是只有 LLVM MC / CodeGen, 而是:

- 上层 MLIR / 高层算子 lowering
- 下层 LLVM target backend

## 对你当前最实用的建议

如果你现在的目标是“先学会怎么搭 CoralNPU backend”, 最现实的路线是:

### 第一阶段

先把一个最小 RISC-V backend 演化成一个:

- `CoralNPU` 名字正确
- triple 正确
- `TargetMachine` 正确
- `MCTargetDesc` 正确

的最小 target

这阶段重点是:

- 搭好 target 骨架

### 第二阶段

再把它和普通 RISC-V 拉开:

- 引入向量寄存器类
- 引入 CoralNPU 风格的向量指令或原语
- 让 `Subtarget` 和 `InstrInfo` 真的体现 Coral 特性

### 第三阶段

最后才考虑:

- 更高层的 tensor / ML lowering

## 一个非常实用的判断标准

如果某个改动只是:

- 改名字
- 改 triple
- 改 `TargetInfo`

那它在做的是:

- “把一个最小 RISC-V backend 迁移成一个叫 CoralNPU 的 target”

如果某个改动开始涉及:

- 向量寄存器类
- SIMD 指令
- MAC / stripmining 语义

那它才是在做:

- “让这个 target 开始像 CoralNPU”
