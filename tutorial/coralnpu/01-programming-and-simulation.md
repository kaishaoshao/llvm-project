# 01 Programming And Simulation

## 学习目标

- 看懂 CoralNPU 程序最小长什么样
- 知道 ELF 是怎么跑进仿真器里的
- 建立“程序 -> 编译 -> 仿真”的最小闭环

## 先抓住最小闭环

从 [doc/tutorials/writing_coralnpu_programs.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/tutorials/writing_coralnpu_programs.md) 和 [doc/tutorials/simulator_usage_zh.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/tutorials/simulator_usage_zh.md) 可以把最小使用链路压缩成:

1. 写一个 CoralNPU 端 C/C++ 程序
2. 编译成 ELF
3. 用仿真器加载 ELF
4. 运行到 core halt
5. 从 DTCM 或符号地址读取结果

## CoralNPU 程序最小结构

一个最简单的程序通常有三部分:

1. 输入缓冲区
2. 输出缓冲区
3. `main()` 里的实际计算

教程示例会把数据放在 `.data` 段里, 例如:

```c++
uint32_t input1_buffer[8] __attribute__((section(".data")));
uint32_t input2_buffer[8] __attribute__((section(".data")));
uint32_t output_buffer[8] __attribute__((section(".data")));
```

这样做的意义是:

- 这些符号会进入 ELF
- 后续测试平台或仿真脚本可以通过符号名找到它们
- 链接脚本会把它们放进 CoralNPU 可访问的数据内存区域

## 运行模型是什么

从教程文字能看出来, CoralNPU 的最小使用模型很像:

- host 先把程序和输入准备好
- CoralNPU 从入口地址开始执行
- `main()` 返回后核心停止

这和完整操作系统、多进程、多中断的通用 CPU 模型不是一回事。  
它更像一个:

- run-to-completion executor

这点对你理解后端也很重要, 因为它会影响:

- ABI 复杂度
- runtime 模型
- 调试方式

## 仿真器最常见的角色

在 [doc/tutorials/simulator_usage_zh.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/tutorials/simulator_usage_zh.md) 里, 三个概念很关键:

- `C/C++ 程序`
- `ELF`
- `core_mini_axi_sim`

它们的关系是:

- 程序是你写的逻辑
- ELF 是编译产物
- 仿真器负责加载 ELF 并执行

## 你现在最应该记住的几件事

### 1. CoralNPU 不是只讲 ISA, 它有很强的“程序装载”语境

它的最小开发闭环不是:

- 写 `.s`
- 直接 `llc`

而是:

- 写 C/C++
- 编译成 ELF
- 装入 ITCM / DTCM
- 通过仿真器或系统接口运行

### 2. 符号和内存布局很重要

因为:

- 输入输出缓冲区位置会被脚本或宿主侧查询
- 你不是只关心指令文本, 还关心数据如何落在 CoralNPU 可访问的内存里

### 3. 后端不是唯一重点

如果你后面要把一个最小 RISC-V backend 演化成 `CoralNPU`, 你不能只想:

- 寄存器
- 指令

还要想:

- 程序运行模型
- ELF 装载路径
- memory map

这些在 CoralNPU 的真实使用里都很重要。
