# CoralNPU Tutorial

这套资料是根据 `/Volumes/wsk/code/googlenpu/coralnpu` 里的公开文档整理出来的学习版笔记。  
目标不是逐字翻译原文, 而是把它整理成更适合你当前 LLVM/后端学习语境的一套入口。

## 原始资料来源

- [README.md](/Volumes/wsk/code/googlenpu/coralnpu/README.md)
- [doc/overview.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/overview.md)
- [doc/tutorials/writing_coralnpu_programs.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/tutorials/writing_coralnpu_programs.md)
- [doc/tutorials/simulator_usage_zh.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/tutorials/simulator_usage_zh.md)
- [doc/microarch/microarch.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/microarch/microarch.md)
- [doc/integration_guide.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/integration_guide.md)

## 推荐先读

如果你现在想先拿到一份单文档总览, 先读:

- [CoralNPU-Combined.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/CoralNPU-Combined.md)

## 分章节阅读顺序

1. [00-overview.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/00-overview.md)
2. [01-programming-and-simulation.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/01-programming-and-simulation.md)
3. [02-microarchitecture.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/02-microarchitecture.md)
4. [03-integration-and-memory-map.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/03-integration-and-memory-map.md)
5. [04-backend-implications.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/04-backend-implications.md)

## 这套资料回答什么问题

- CoralNPU 到底是不是一个 RISC-V CPU
- 它和普通 RISC-V 风格最小 backend 最大的差异是什么
- CoralNPU 程序怎么写、怎么编译、怎么在仿真器里运行
- CoralNPU 的微架构里, 哪些特性会直接影响后端设计
- 如果你想把一个最小 RISC-V backend 演化成 `CoralNPU`, 编译器层面应该怎么重新理解目标
