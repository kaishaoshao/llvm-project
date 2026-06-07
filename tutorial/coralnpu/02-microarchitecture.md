# 02 Microarchitecture

## 学习目标

- 看懂 CoralNPU 最核心的微架构轮廓
- 明白哪些点会直接影响编译器和后端设计
- 建立“ISA 不是全部”的意识

## 先抓住总体轮廓

从 [doc/microarch/microarch.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/microarch/microarch.md) 和 [doc/overview.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/overview.md) 可以抓住几个关键特征:

- 标量前端是 RISC-V 风格
- 后端有独立的向量 / SIMD 数据通路
- 还有面向 ML 的矩阵 / MAC 能力
- 前端和后端是解耦的

这意味着 CoralNPU 的“前端像 CPU, 后端更像 ML accelerator”。

## 标量前端

文档里强调:

- scalar frontend drives command queues of the ML+SIMD backend

这很重要。  
它说明标量前端不只是自己算普通整数逻辑, 它还承担:

- 控制流
- 地址生成
- 向后端发命令

从后端视角看, 这意味着:

- 标量指令不只是普通辅助逻辑
- 很多高层循环和调度会由标量侧驱动

## 向量 / SIMD 部分

文档里给出的信息很关键:

- vector registers: `v0..v63`
- width: 256 bits
- data widths: 8 / 16 / 32 bits

这和“普通 RV32 标量后端”已经很不一样了。  
对编译器来说, 这意味着至少会碰到:

- 不同于 GPR 的寄存器类
- 向量类型
- 与 strip-mining 或 tile 相关的 codegen 机会

## 累加器 / MAC

文档把 MAC 描述为核心组件, 说明 CoralNPU 的真正性能重点不在:

- 标量整数 ALU

而在:

- 量化 outer-product multiply-accumulate

这件事对编译器的启发是:

- 你未来的目标不该只是“让它像 RISCV 一样能跑通整数程序”
- 你最终真正想利用的能力, 在向量和矩阵路径上

## Stripmining

文档里专门强调 stripmining。  
它不是一个可有可无的小优化, 而是 CoralNPU 指令/执行模型的一部分。

这意味着:

- 单个前端派发事件, 可能映射成后端多个 issue 事件
- 指令编码和并行硬件之间不是一一对应的直观关系

从编译器视角看, 这往往意味着:

- 不能只按“每条 IR -> 每条普通机器指令”去想
- 某些 CoralNPU 指令更像带内建分块语义的操作

## 为什么这些对后端实现重要

如果你后面做 CoralNPU backend, 微架构里最该记住的是:

### 1. 标量前端不是全部

你当然可以先从标量侧搭后端骨架, 但 CoralNPU 的真正特点不在那里。

### 2. 向量寄存器类几乎是必需的

如果以后真的要做出“像 CoralNPU”的东西, 而不只是“换名字的最小 RISC-V backend”, 迟早要设计:

- 向量寄存器类
- 向量指令格式
- 向量/张量 lowering

### 3. 指令选择和调度可能更复杂

因为前端派发和后端 issue 之间不是最朴素的一对一关系。

## 一个很实用的结论

如果你现在还在学习阶段, 最合理的路线是:

1. 先把 CoralNPU 当成一个 RISC-V 风格 target 搭起基础骨架
2. 再逐步把它和普通 RISC-V 拉开
3. 差异的主要落点就在:
   - 向量寄存器
   - SIMD 指令
   - ML / MAC 能力
   - stripmining / 调度特性
