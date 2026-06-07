# 03 Integration And Memory Map

## 学习目标

- 看懂 CoralNPU 怎么作为系统里的一个外设或协处理核心接入
- 理解 ITCM / DTCM / CSR 这些概念为什么重要
- 理解这类系统结构会怎样反过来影响编译器和程序组织

## CoralNPU 在系统里怎么出现

从 [doc/integration_guide.md](/Volumes/wsk/code/googlenpu/coralnpu/doc/integration_guide.md) 看, CoralNPU 并不是“单独的一颗黑盒芯片抽象”, 而是更像:

- SoC 里的一个 AXI 外设 / 协处理计算单元

它既有:

- AXI slave 接口
  - 用于外部主机写 TCM、触碰 CSR

也有:

- AXI master 接口
  - 用于 CoralNPU 自己访问外部内存或系统资源

这说明 CoralNPU 的运行不是“孤立执行”。

## Memory Map 为什么关键

文档给出了非常明确的三块区域:

- ITCM
- DTCM
- CSR

它们大致对应:

- ITCM: 指令存储
- DTCM: 数据存储
- CSR: 外部控制 / 状态接口

这对编译器和程序组织的影响很直接:

### 1. 指令和数据不是抽象地“在内存里”

而是通常会有更明确的落点:

- 程序装载到 ITCM
- 数据对象落到 DTCM

### 2. 外部主机可以通过内存映射方式控制它

例如:

- 写 start PC
- 释放时钟门控
- 释放 reset
- 读 halted / fault 状态

### 3. 这不是普通主机 CPU 进程模型

它更像:

- host 准备环境
- CoralNPU 运行一个任务
- host 读取结果和状态

## Boot 流程对你意味着什么

文档中的启动顺序非常值得记:

1. 初始化指令内存
2. 写启动 PC
3. 释放时钟门控
4. 释放 reset
5. 监控 halted / fault

这套流程说明:

- CoralNPU 的“运行程序”不仅是编译问题
- 还是装载、控制和系统集成问题

如果你以后把一个最小 RISC-V backend 演化成 `CoralNPU`, 这会影响你怎么理解:

- 链接脚本
- section 布局
- 运行时初始化
- 调试和仿真

## 为什么这会影响后端设计

至少有三点会受影响:

### 1. Data layout 和 section 组织

你最终不只是关心指令能不能生成, 还会关心:

- 代码放哪里
- 全局变量放哪里
- 常量池放哪里

### 2. 程序入口和执行模型

如果执行模型是 run-to-completion, 那么:

- 入口
- halt 语义
- host / device 协作方式

都会更重要。

### 3. 调试方式

CoralNPU 的 debug 信号和 CSR 状态说明, 调试可能大量依赖:

- 仿真 trace
- 状态寄存器
- memory dump

而不只是普通软件断点。
