# 00 CoralNPU Overview

## 学习目标

- 先建立一个正确的 CoralNPU 心智模型
- 分清它和“普通 RISC-V CPU”以及“独立专有 NPU”的区别
- 理解为什么它和 LLVM / MLIR 的关系会比传统 CPU 后端更紧

## 核心结论

CoralNPU 不能简单理解成:

- 只是一个普通 RISC-V CPU

也不能简单理解成:

- 一个完全脱离 CPU 语义、只能吃专有命令缓冲区的黑盒 NPU

更准确地说, 它是:

- 基于 RISC-V 的计算核心
- 但在架构和微架构上明显为了机器学习工作负载做了强化
- 同时强调开放工具链和开放编程模型

## 从原始文档里可以抓住的 3 个事实

### 1. 它明确是基于 RISC-V ISA 的

从 [README.md](/Volumes/wsk/code/googlenpu/coralnpu/README.md) 可以直接看到:

- Coral NPU is based on the 32-bit RISC-V Instruction Set Architecture

而且它给出的 ISA 轮廓不是抽象说法, 而是更具体的:

- `rv32imf_zve32x_zicsr_zifencei_zbb`

这说明 CoralNPU 不是“完全另起炉灶的指令体系”, 而是在 RISC-V 基础上向 ML/向量方向扩展。

### 2. 它不是只靠一个标量核干活

从 [README.md](/Volumes/wsk/code/googlenpu/coralnpu/README.md) 和 [doc/overview.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/overview.md) 可以看到, CoralNPU 由三个部分协同:

- scalar
- vector / SIMD
- matrix / ML

所以你以后设计后端时, 不能只按 “普通整数寄存器 + 普通 ALU 指令” 那一套想它。

### 3. 它强调开放软件栈

你前面贴的描述和仓库文档本身都指向同一个方向:

- CoralNPU 想减少 CPU / NPU 编程模型割裂
- 想减少专有编译器命令缓冲区那种黑盒路径
- 想让 C/C++、LLVM、MLIR、ML 框架之间的关系更直接

这意味着从编译器角度看:

- CoralNPU 的价值不只在一个 `llvm/lib/Target/...` 后端
- 它更适合被看成 “ISA + 张量/向量能力 + 开放编译基础设施” 的整体

## 和你当前学习路径的关系

如果你当前是在学 LLVM target backend, CoralNPU 对你的启发是:

### 先从 LLVM target 层入手是合理的

你仍然需要:

- `Triple`
- `TargetInfo`
- `TargetMachine`
- `MCTargetDesc`
- `Subtarget`
- `InstrInfo`

因为 CoralNPU 毕竟有自己的 ISA / ISA 变体。

### 但不能把目标只设成“抄一个 RISCV backend”

因为 CoralNPU 的架构重点明显在:

- 向量能力
- 张量/矩阵能力
- 更贴近 ML 的数据通路

所以它比一个最小 RISC-V 教学后端更像:

- RISC-V 基础 target
- 再叠加 ML 工作负载优化和扩展

## 你现在最应该怎么理解 CoralNPU

如果只用一句话概括:

- CoralNPU 是一个以 RISC-V 为基础、但明显朝机器学习计算模型倾斜的开放计算架构

如果用编译器实现角度概括:

- 它不是“普通 CPU backend”
- 也不是“只能靠专有驱动喂命令的黑盒 NPU”
- 它更像是“RISC-V 扩展 target + 更适合 MLIR/LLVM 的开放软件栈目标”
