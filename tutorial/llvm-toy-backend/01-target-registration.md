# 01 Target Registration

## 学习目标

- 理解 `toy-1` 到 `toy-3` 的意义
- 理解 `TheToyTarget` 是什么
- 理解 `LLVMInitializeToyTargetInfo` / `LLVMInitializeToyTarget` / `LLVMInitializeToyTargetMC` 三者的分工

## 对应 toy 章节

- `toy-1: llc 识别 target`
- `toy-2: LLVMInitializeToyTarget`
- `toy-3: LLVMInitializeToyTargetMC`

## 必看文件

- [ToyTargetInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetInfo/ToyTargetInfo.cpp)
- [ToyTargetMachine.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.cpp)
- [ToyTargetMachine.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.h)
- [ToyTargetDesc.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyTargetDesc.cpp)
- [Toy/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/CMakeLists.txt)
- [TargetInfo/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetInfo/CMakeLists.txt)
- [TargetDesc/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/CMakeLists.txt)
- [llvm/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/CMakeLists.txt)

## 这一节的核心结论

### 三个初始化函数各自负责一层

- `LLVMInitializeToyTargetInfo`
  让 LLVM 认识 `toy` 这个 target 名字
- `LLVMInitializeToyTarget`
  让 LLVM 知道如何创建 `ToyTargetMachine`
- `LLVMInitializeToyTargetMC`
  让 LLVM 知道如何创建寄存器信息、指令信息、subtarget 信息、汇编信息等 MC 组件

### `TheToyTarget` 是核心插座

几乎所有初始化都围绕一个全局对象:

- `Target TheToyTarget;`

它不是某个 pass, 也不是某个 `TargetMachine`, 而是整个 target 的注册入口。

## toy-1 在解决什么问题

### 目标

让 `llc --version` 的 `Registered Targets` 里出现:

- `toy - Toy RISC-V backend`

### 关键知识点

- `Triple::ArchType`
- `RegisterTarget`
- 顶层 `LLVM_ALL_TARGETS`

### 关键动作

1. 把 `Toy` 加进 [llvm/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/CMakeLists.txt)
2. 在 triple 枚举里加入 `toy`
3. 在 `ToyTargetInfo.cpp` 中通过 `RegisterTarget` 绑定:
   - 架构枚举
   - 命令行名字 `toy`
   - `TheToyTarget`

### 完成后会遇到的下一类错误

虽然 `llc` 已经认识 `-march=toy`, 但还不能创建 `TargetMachine`, 所以会报:

- `Could not allocate target machine!`

这说明已经从 “不认识 target” 进入 “认识 target, 但不会实例化 target machine” 的阶段。

## toy-2 在解决什么问题

### 目标

让 `TheToyTarget->createTargetMachine(...)` 能真正返回一个 `ToyTargetMachine`

### 关键知识点

- `RegisterTargetMachine<T>`
- `LLVMTargetMachine`
- `ToyTargetMachine` 构造函数

### 关键动作

在 [ToyTargetMachine.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.cpp) 中:

- 实现 `LLVMInitializeToyTarget`
- 调用 `RegisterTargetMachine<ToyTargetMachine>(TheToyTarget)`

### 为什么做完还会崩

`ToyTargetMachine` 构造函数里会调用:

- `initAsmInfo()`

它需要 MC 层工厂已经注册完成, 否则拿不到:

- `MCRegisterInfo`
- `MCInstrInfo`
- `MCSubtargetInfo`
- `MCAsmInfo`

所以教程里的下一步自然是 `toy-3`。

## toy-3 在解决什么问题

### 目标

把 `ToyTargetMachine` 初始化时依赖的 MC 工厂全部挂到 `TheToyTarget` 上

### 关键知识点

- `TargetRegistry::RegisterMCRegInfo`
- `TargetRegistry::RegisterMCInstrInfo`
- `TargetRegistry::RegisterMCSubtargetInfo`
- `TargetRegistry::RegisterMCAsmInfo`
- `TargetRegistry::RegisterMCInstPrinter`
- `TargetRegistry::RegisterMCCodeEmitter`
- `TargetRegistry::RegisterMCAsmBackend`

### 必须先有的 TableGen 产物

在 [Toy/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/CMakeLists.txt) 里会生成:

- `ToyGenSubtargetInfo.inc`
- `ToyGenInstrInfo.inc`
- `ToyGenRegisterInfo.inc`

`ToyTargetDesc.cpp` 会直接 include 它们。

### 关键理解

`LLVMInitializeToyTargetMC()` 不是在直接创建这些对象, 而是在注册 “createXXX 工厂函数”。

后续真正创建对象的是 `TheToyTarget` 在运行时通过这些工厂完成的。

## 这一步最容易混淆的点

### `TargetInfo` 不等于 `TargetMC`

- `TargetInfo` 负责让 target 能被枚举到
- `TargetMC` 负责让 target 的低层元信息能被构造出来

### `TargetMachine` 不等于 `Subtarget`

- `TargetMachine` 是后端顶层入口
- `Subtarget` 是特定 CPU / feature 组合

### `MCRegisterInfo` 不等于 `RegisterInfo`

- `MCRegisterInfo`
  偏汇编/机器码层面的寄存器元信息
- `RegisterInfo`
  偏 codegen 阶段对寄存器的策略和行为

## 在你当前仓库上的迁移提醒

当前工作仓库不是 `llvm-toy`, 而是 `llvm-project_mips`, 所以:

- 顶层 target 列表要改当前仓库自己的 `llvm/CMakeLists.txt`
- triple 文件路径要看当前仓库版本
- 类签名可能与 `llvm-toy` 略有不同

但这一节的核心逻辑不会变。

## 自查问题

1. 为什么 `toy-1` 之后 `llc` 能认出 target, 却还不能创建 `TargetMachine`?
2. `RegisterTargetMachine<ToyTargetMachine>` 到底给 `TheToyTarget` 填了什么能力?
3. 为什么 `ToyTargetMachine` 构造函数里调用 `initAsmInfo()` 会强迫你继续实现 `toy-3`?

