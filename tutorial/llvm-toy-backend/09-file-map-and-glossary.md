# 07 File Map And Glossary

## 学习目标

- 快速定位 `llvm-toy` 各模块文件
- 建立常见术语和职责的对应关系
- 在当前仓库和旧教程之间做路径对照

## 这个文件怎么用

前面几节更像“教学正文”, 这一节更像“查表页”。  
最适合的用法不是从头读到尾, 而是:

- 你先知道自己现在的目标是什么
- 再来这里查“对应应该看哪些文件”

## 按目标查文件

### 想实现 `llc --version` 里出现 target

优先看:

- `TargetInfo/ToyTargetInfo.cpp`
- `llvm/CMakeLists.txt`
- `Triple.h`
- `Triple.cpp`

### 想实现 `llc -mtriple=...` 不再报 unknown target

优先看:

- `Triple.h`
- `Triple.cpp`
- `TargetInfo/ToyTargetInfo.cpp`

### 想实现 `TargetMachine` 创建

优先看:

- `ToyTargetMachine.h`
- `ToyTargetMachine.cpp`

### 想实现 MC 基础对象创建

优先看:

- `TargetDesc/ToyTargetDesc.cpp`
- `Toy.td`
- `ToyRegisterInfo.td`
- `ToyInstrInfo.td`

### 想实现最小 isel 骨架

优先看:

- `ToyISelLowering.h/.cpp`
- `ToyDAGToDAGISel.h/.cpp`
- `ToyInstrInfo.td`

### 想实现栈帧和寄存器保存

优先看:

- `ToyFrameLowering.h/.cpp`
- `ToyRegisterInfo.h/.cpp`
- `ToyInstrInfo.h/.cpp`

### 想实现函数调用和返回

优先看:

- `ToyISelLowering.h/.cpp`
- `ToyCallingConv.td`
- `ToyFrameLowering.cpp`
- `ToyRegisterInfo.cpp`

### 想实现 `MachineInstr -> MCInst -> asm`

优先看:

- `ToyAsmPrinter.h/.cpp`
- `ToyMCInstLower.h/.cpp`
- `TargetDesc/ToyInstPrinter.h/.cpp`

## `llvm-toy` 文件地图

### 顶层 target 目录

- [Toy/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/CMakeLists.txt)
- [Toy/README.md](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/README.md)
- [Toy/TODO.org](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TODO.org)

### target 注册与顶层骨架

- [TargetInfo/ToyTargetInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetInfo/ToyTargetInfo.cpp)
- [ToyTargetMachine.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.h)
- [ToyTargetMachine.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.cpp)
- [ToySubtarget.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.h)
- [ToySubtarget.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.cpp)

### TableGen 描述

- [Toy.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/Toy.td)
- [ToySubtarget.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.td)
- [ToyRegisterInfo.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.td)
- [ToyInstrFormats.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrFormats.td)
- [ToyInstrInfo.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.td)
- [ToyCallingConv.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyCallingConv.td)
- [ToySchedule.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySchedule.td)

### isel / lowering

- [ToyISelLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.h)
- [ToyISelLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.cpp)
- [ToyDAGToDAGISel.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.h)
- [ToyDAGToDAGISel.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.cpp)

### 栈帧 / 寄存器 / 指令行为

- [ToyFrameLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.h)
- [ToyFrameLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.cpp)
- [ToyRegisterInfo.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.h)
- [ToyRegisterInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.cpp)
- [ToyInstrInfo.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.h)
- [ToyInstrInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.cpp)

### asm / MC 路径

- [ToyAsmPrinter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.h)
- [ToyAsmPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.cpp)
- [ToyMCInstLower.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyMCInstLower.h)
- [ToyMCInstLower.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyMCInstLower.cpp)
- [TargetDesc/ToyInstPrinter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.h)
- [TargetDesc/ToyInstPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.cpp)
- [TargetDesc/ToyMCCodeEmitter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyMCCodeEmitter.h)
- [TargetDesc/ToyMCCodeEmitter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyMCCodeEmitter.cpp)
- [TargetDesc/ToyAsmBackend.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyAsmBackend.h)
- [TargetDesc/ToyAsmBackend.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyAsmBackend.cpp)
- [TargetDesc/ToyELFObjectWriter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyELFObjectWriter.cpp)
- [TargetDesc/ToyTargetStreamer.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyTargetStreamer.cpp)

## 术语表

### Target

LLVM 中被注册的目标架构入口对象, 负责挂接各种工厂函数。

### TargetMachine

整个 target 的 codegen 顶层入口。

### Subtarget

具体 CPU / feature 组合下的能力集合。

### SelectionDAG

LLVM 在指令选择前使用的一种图表示。

### MachineInstr

目标相关的机器指令中间表示。

### MCInst

更接近汇编和机器码层的指令表示。

### PEI

Prologue/Epilogue Insertion。负责栈帧最终落地。

### FrameIndex

抽象的栈对象引用, 后续会被消解成真实寄存器加偏移。

### glue

在 DAG 中用于表达必须保持顺序或隐式寄存器依赖的连接关系。

## 旧教程与当前仓库的路径差异

### Triple

旧教程常见路径:

- `llvm/include/llvm/ADT/Triple.h`
- `llvm/lib/Support/Triple.cpp`

当前工作仓库常见路径:

- `llvm/include/llvm/TargetParser/Triple.h`
- `llvm/lib/TargetParser/Triple.cpp`

### MC 目录

`llvm-toy` 使用:

- `TargetDesc`

上游 LLVM 常见命名:

- `MCTargetDesc`

## 使用建议

- 学职责时优先看本文件和前面 6 节教程
- 查实现时按 “文件地图” 直接跳转
- 遇到接口差异时, 先回到职责层面, 再对照你当前 LLVM 版本 API
