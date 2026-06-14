# LLVM Backend Tutorial For CoralNPU

这套教程基于两份材料整理:

- `llvm-toy` 代码: `/Volumes/wsk/code/llvm-mlir/llvm-toy`
- 教程文章: `/Volumes/wsk/code/llvm-mlir/pages/toolchain/llvm_toy_riscv_backend.html`

目标不是直接照抄代码, 而是建立一条清晰的学习路线:

1. 先理解 LLVM 后端的分层
2. 再理解一个最小 target 是怎么被接进 LLVM 的
3. 再逐步理解 isel, MachineInstr, MCInst, 栈帧, 调用约定
4. 最后再看浮点, object file, intrinsic 等扩展

虽然这套资料仍然以 `llvm-toy` 为参考样本, 但从这一版开始, 文档里的“你的目标 target”默认指向 `CoralNPU`。
也就是说:

- `Toy` 代码仍然是教学参考
- 你当前仓库里的真实练习目标默认按 `CoralNPU` 来组织

## 这套教程适合谁

如果你现在的状态更接近下面几条, 这套教程就是按你来写的:

- 你知道 LLVM 很大, 但并不知道后端到底有哪些文件
- 你看见 `TargetMachine`、`Subtarget`、`InstrInfo` 这些名字, 不知道它们谁管什么
- 你能大概理解 “编译器会把 IR 变成汇编”, 但中间那几层数据结构还很模糊
- 你希望先建立一张地图, 再慢慢看代码, 而不是一上来被几十个 `.cpp` 和 `.td` 文件淹没

这套教程默认你是第一次系统接触 LLVM 后端。它会更强调:

- “这个文件为什么存在”
- “这个类什么时候会被调用”
- “它和前后模块的边界是什么”
- “如果这一层没实现, `llc` 通常会报什么错”

## 学习时要一直记住的两个问题

读每一节时, 你都可以反复问自己:

1. 这个模块在整条 codegen 流水线的哪一层?
2. 如果没有它, LLVM 会在什么地方卡住?

只要这两个问题清楚了, 你读代码时就不容易迷路。

## 建议阅读顺序

1. [00-overview.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/00-overview.md)
2. [01-target-registration.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/01-target-registration.md)
3. [02-td-files-for-coralnpu.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/02-td-files-for-coralnpu.md)
4. [03-minimal-pipeline.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/03-minimal-pipeline.md)
5. [04-isel-and-asm.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/04-isel-and-asm.md)
6. [05-frame-and-registers.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/05-frame-and-registers.md)
7. [06-calls-and-abi.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/06-calls-and-abi.md)
8. [07-advanced-topics.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/07-advanced-topics.md)
9. [08-debugging-llc.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/08-debugging-llc.md)
10. [09-file-map-and-glossary.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/09-file-map-and-glossary.md)

## 最实用的任务地图

如果你不是想“系统读一遍”, 而是想“今天先把后端往前推一步”, 最适合先看这张任务地图。

### 阶段 A: 让 LLVM 识别 target

目标:

- `llc --version` 里出现 `coralnpu32`
- `llc -mtriple=coralnpu32 ...` 不再报 unknown target

主要文件:

- `llvm/include/llvm/TargetParser/Triple.h`
- `llvm/lib/TargetParser/Triple.cpp`
- `llvm/lib/Target/CoralNPU/TargetInfo/CoralNPUTargetInfo.cpp`
- `llvm/CMakeLists.txt`

先看:

- [01-target-registration.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/01-target-registration.md)

### 阶段 B: 让 `TargetMachine` 能创建

目标:

- `llc -mtriple=coralnpu32 test.ll` 开始进入你的 `CoralNPUTargetMachine`

主要文件:

- `llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.h`
- `llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.cpp`

先看:

- [01-target-registration.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/01-target-registration.md)
- [03-minimal-pipeline.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/03-minimal-pipeline.md)

### 阶段 C: 让 `TargetMC` 不再报 `Unable to create reg info`

目标:

- `TargetMachine::initAsmInfo()` 能拿到 `MCRegisterInfo`、`MCInstrInfo`、`MCSubtargetInfo`、`MCAsmInfo`

主要文件:

- `llvm/lib/Target/CoralNPU/MCTargetDesc/CoralNPUTargetDesc.cpp`

先看:

- [01-target-registration.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/01-target-registration.md)
- [08-debugging-llc.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/08-debugging-llc.md)

### 阶段 D: 让 codegen 骨架站起来

目标:

- `TargetMachine -> Subtarget -> TargetLowering/RegisterInfo/FrameLowering`
  这条链成立

主要文件:

- `CoralNPUSubtarget.*`
- `CoralNPUTargetLowering.*`
- `CoralNPURegisterInfo.*`
- `CoralNPUFrameLowering.*`

先看:

- [03-minimal-pipeline.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/03-minimal-pipeline.md)
- [02-td-files-for-coralnpu.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/02-td-files-for-coralnpu.md)

### 阶段 E: 让最小 IR 真正开始出目标相关结果

目标:

- 最小 `ret i32 0`
- 以及后续最小 `addi/load/store`
  能开始进入 isel / asm 路径

主要文件:

- `CoralNPUDAGToDAGISel.*`
- `CoralNPUAsmPrinter.*`
- `CoralNPUInstPrinter.*`
- `.td` 文件一套

先看:

- [04-isel-and-asm.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/04-isel-and-asm.md)
- [05-frame-and-registers.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/05-frame-and-registers.md)
- [02-td-files-for-coralnpu.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/02-td-files-for-coralnpu.md)

## 每节建议怎么使用

每一节都建议按下面这个顺序使用:

1. 先看“这一节的目标”
2. 再看“要实现哪些函数”
3. 再看“这一步完成后的预期现象”
4. 真跑一次命令
5. 如果失败, 对照“常见报错”和 [08-debugging-llc.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/08-debugging-llc.md)

## 如果你当前目标已经转向 CoralNPU

这套 `llvm-toy-backend` 资料仍然有用, 但它解决的是:

- LLVM target backend 怎么搭起来

如果你现在的目标已经转向 `CoralNPU`, 建议同时配合阅读:

- [tutorial/README.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/README.md)
- [coralnpu/README.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/README.md)

因为 CoralNPU 不只是“换个名字的 RISC-V target”, 它还有:

- 更强的向量 / ML 语义
- 特定的程序装载和仿真方式
- 更明显的系统集成背景

## 教程组织方式

每一节都包含这些部分:

- 学习目标
- 对应的 toy 章节
- 关键知识点
- 必看文件
- 常见报错与原因
- 注意事项
- 自查问题

部分章节还会额外补:

- 文件职责说明
- 类 / 结构体在流水线中的位置
- 第一次阅读这些代码时建议先看哪几个函数
- 初学者最常混淆的边界

## 新的阅读方式: 先看目标, 再看函数

从这一版开始, 教程会更强调一种更适合动手的阅读方式:

- 先问 “我现在想让 LLVM 做到什么”
- 再问 “为了做到这个, 我要实现哪些函数”

也就是说, 文档不会只说:

- “你要改 `Triple.cpp`”
- “你要补 `TargetMachine`”

而会尽量写成:

- “如果你的目标是让 `llc --version` 里出现 target, 你要实现哪些函数”
- “如果你的目标是让 `llc -mtriple=...` 不再报 unknown target, 你要实现哪些函数”
- “如果你的目标是让 LLVM 能创建 `TargetMachine`, 你要实现哪些函数”
- “如果你的目标是让 MC 层不再报 `Unable to create reg info`, 你要实现哪些函数”

这样你就可以把 LLVM 后端学习过程看成一串很具体的小目标, 而不是一大堆同时出现的文件和类名。

## 重要提醒

- `llvm-toy` 使用的是较老一代 LLVM API, 教程里的模块职责和推进顺序仍然非常有价值, 但具体接口签名不一定和你当前仓库一致。
- 你当前仓库里与 `Triple` 相关的路径是 `llvm/include/llvm/TargetParser/Triple.h` 和 `llvm/lib/TargetParser/Triple.cpp`, 而不是更老的 `ADT/Triple.h` / `Support/Triple.cpp`。
- 学习时优先理解 “为什么要补这一块”, 不要机械照抄代码。

## 第一次阅读时的建议

第一次读不要追求 “把所有实现细节都读懂”, 更好的节奏是:

1. 先只读每节开头的学习目标和文件地图
2. 再把 “这一层在流水线里的位置” 看明白
3. 最后才去追具体函数体

如果你在某一节突然觉得函数很多、看不下去, 先退回去问:

- 这个文件属于 `.td` 还是 `C++`?
- 它属于 IR、SelectionDAG、MachineInstr 还是 MC 层?
- 它是在 “描述规则”, 还是在 “执行行为”?

这样通常就能重新找到方向。
