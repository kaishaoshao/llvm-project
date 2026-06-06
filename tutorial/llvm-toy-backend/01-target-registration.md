# 01 Target Registration

## 学习目标

- 理解 `toy-1` 到 `toy-3` 的意义
- 理解 `TheToyTarget` 是什么
- 理解 `LLVMInitializeToyTargetInfo` / `LLVMInitializeToyTarget` / `LLVMInitializeToyTargetMC` 三者的分工

## 这一节最重要的新用法

这一节你可以不要按“文件顺序”读, 而是按“目标顺序”读。  
也就是:

- 先看你现在想让 `llc` 做到什么
- 再反推必须实现哪些函数

下面这张表就是给第一次动手的人用的。

## 目标 -> 需要实现什么函数

### 目标 1: `llc --version` 里出现你的 target

你至少要保证这些东西存在:

- `Triple::ArchType` 里有你的架构枚举
- `Triple::getArchTypeForLLVMName`
- `Target TheToyTarget;`
- `LLVMInitializeToyTargetInfo()`
- `RegisterTarget<...>`

这一目标的本质是:

- 让 LLVM 认识 “有这么一个 target”

如果这一步没做完, 最常见现象是:

- `llc --version` 里没有你的 target

### 目标 2: `llc -march=toy` 或 `llc -mtriple=toy-unknown-elf` 不再报 unknown target

你至少要保证这些函数或位置已经接好:

- `Triple::ArchType`
- `Triple::getArchTypeForLLVMName`
- `Triple::getArchTypeName`
- `LLVMInitializeToyTargetInfo()`

这一目标的本质是:

- 让字符串名字能解析成内部架构枚举
- 再把这个架构枚举和 target 注册对象接起来

如果你只改了 `Triple.h`, 没改 `Triple.cpp`, 往往就会卡在这一目标。

### 目标 3: `llc` 已经认识 target, 并开始尝试创建后端

你至少要保证这些东西存在:

- `class ToyTargetMachine : public LLVMTargetMachine`
- `LLVMInitializeToyTarget()`
- `RegisterTargetMachine<ToyTargetMachine>`

这一目标的本质是:

- 从 “LLVM 知道有这个 target”
- 走到 “LLVM 知道应该创建哪个 `TargetMachine` 类”

如果这一步没做完, 常见现象是:

- `Could not allocate target machine`

### 目标 4: `TargetMachine` 不再一创建就因为 MC 依赖失败

你至少要保证这些函数存在:

- `createToyMCRegisterInfo`
- `createToyMCInstrInfo`
- `createToyMCSubtargetInfo`
- `createToyMCAsmInfo`
- `LLVMInitializeToyTargetMC()`

这一目标的本质是:

- 让 `TargetMachine` 初始化时需要的 MC 层对象有地方可创建

如果这一步没做完, 常见现象是:

- `Unable to create reg info`
- `Unable to create asm info`

### 目标 5: LLVM 能继续进入更后面的 codegen 阶段

这时通常说明你已经完成了第一节, 后面才轮到:

- `Subtarget`
- `InstrInfo`
- `RegisterInfo`
- `FrameLowering`
- `ISelLowering`

也就是说, 第一节不是让你“后端能出码”, 而是让你走到:

- target 已识别
- target machine 可创建
- MC 基础对象可创建

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

## 先给你一张最小改动清单

如果你的目标只是 “让 `llc` 开始识别一个新后端”, 第一节最常要碰的是下面这些文件和函数。

### 1. 顶层构建系统

- `llvm/CMakeLists.txt`
  你通常要改 target 列表相关位置

作用:

- 让 CMake 知道 `Toy` 这个 target 要参与构建

如果不改:

- 你的后端目录即使存在, 也可能根本不会被编译进 LLVM

### 2. target 自己的 CMake

- `llvm/lib/Target/Toy/CMakeLists.txt`
- `llvm/lib/Target/Toy/TargetInfo/CMakeLists.txt`
- `llvm/lib/Target/Toy/TargetDesc/CMakeLists.txt`

你通常要添加:

- `add_llvm_component_group(Toy)`
- `add_llvm_target(ToyCodeGen ...)`
- `add_llvm_library(LLVMToyInfo ...)`
- `add_llvm_library(LLVMToyDesc ...)`
- `add_subdirectory(TargetInfo)`
- `add_subdirectory(TargetDesc)`

作用:

- 把你的 target 拆成 LLVM 认识的几个组件库
- 让 `LLVMInitializeToyTargetInfo` / `LLVMInitializeToyTarget` / `LLVMInitializeToyTargetMC` 分别落在正确的库里

### CMake 里这些名字能不能随便取

第一眼看到下面这些名字时, 很容易以为它们只是 CMake 变量风格问题:

- `add_llvm_component_group(Toy)`
- `add_llvm_target(ToyCodeGen ...)`
- `add_llvm_library(LLVMToyInfo ...)`
- `add_llvm_library(LLVMToyDesc ...)`

其实它们不是完全随便取的, 只是“强约束程度”不一样。

#### `add_llvm_component_group(Toy)`

这里的 `Toy` 最好和下面这些名字保持一致:

- target 目录名 `Toy`
- 顶层 target 名 `Toy`
- 你心里给这个后端起的正式名字

它的作用更偏 “把这一组组件归类成同一个 target”。  
所以这里虽然不是 C++ 类型名, 但和 target 身份关联很强。

#### `add_llvm_target(ToyCodeGen ...)`

这里的 `ToyCodeGen` 是这个 target 的 codegen 主库名字。

它更多关联的是:

- 构建目标名
- 链接时的组件名
- LLVM 按 target 组织 codegen 库时的命名习惯

理论上你可以不用这个名字, 但工程上非常不建议。  
LLVM 里几乎都会沿用:

- `XXXCodeGen`

这种形式。

#### `add_llvm_library(LLVMToyInfo ...)`

这里的 `LLVMToyInfo` 通常对应:

- `TargetInfo` 这一层的库
- `ToyTargetInfo.cpp`
- `LLVMInitializeToyTargetInfo`

它不是随便起个库名那么简单, 而是 LLVM target 组织里很稳定的一种约定:

- `LLVM${TargetName}Info`

#### `add_llvm_library(LLVMToyDesc ...)`

这里的 `LLVMToyDesc` 通常对应:

- `TargetDesc` / `MCTargetDesc` 这一层的库
- `ToyTargetDesc.cpp`
- `InstPrinter`
- `MCCodeEmitter`
- `AsmBackend`

它也遵循 LLVM 很稳定的习惯:

- `LLVM${TargetName}Desc`

#### 最重要的不是“某个名字神奇”, 而是整套名字前后一致

这些名字本身不是魔法关键字, 真正重要的是它们最好形成一整套一致命名:

- target 目录: `Toy`
- component group: `Toy`
- codegen 库: `ToyCodeGen`
- info 库: `LLVMToyInfo`
- desc 库: `LLVMToyDesc`
- 入口类: `ToyTargetMachine`
- 初始化函数:
  - `LLVMInitializeToyTargetInfo`
  - `LLVMInitializeToyTarget`
  - `LLVMInitializeToyTargetMC`

这样你一眼就能看出它们属于同一个 target。

如果你把 target 叫 `Toy`, 却把库起成:

- `MyBackendCore`
- `BackendInfo`
- `BackendDesc`

虽然某些情况下未必立刻编译失败, 但你后面会很难对照 LLVM 其他后端理解结构, 也更容易把库职责和初始化入口搞混。

### 3. Triple 架构登记

- [llvm/include/llvm/TargetParser/Triple.h](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/llvm/include/llvm/TargetParser/Triple.h)
- [llvm/lib/TargetParser/Triple.cpp](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/llvm/lib/TargetParser/Triple.cpp)

你最常改的是:

- `Triple::ArchType` 枚举
- `Triple::getArchTypeName`
- `Triple::getArchTypeForLLVMName`
- `Triple::getArchTypePrefix`
- `Triple::getArchPointerBitWidth`

如果你做的是 32/64 双变体, 往往还要补:

- `Triple::get64BitArchVariant`
- `Triple::getBigEndianArchVariant`
- `Triple::getLittleEndianArchVariant`
- `Triple::isLittleEndian`

作用:

- 让 LLVM 能把字符串 target 名字解析成内部架构枚举
- 让 LLVM 能把内部架构枚举再转回名字
- 让 LLVM 知道这个架构是 32 位还是 64 位
- 让 LLVM 知道架构变体之间怎么互相切换

### 4. TargetInfo 层

- `llvm/lib/Target/Toy/TargetInfo/ToyTargetInfo.cpp`

你最常创建 / 修改的是:

- 全局对象 `Target TheToyTarget;`
- 函数 `LLVMInitializeToyTargetInfo()`

作用:

- 把命令行名字和内部 target 对象绑定起来

### 5. TargetMachine 层

- `llvm/lib/Target/Toy/ToyTargetMachine.h`
- `llvm/lib/Target/Toy/ToyTargetMachine.cpp`

你最常创建 / 修改的是:

- 类 `ToyTargetMachine : public LLVMTargetMachine`
- 函数 `LLVMInitializeToyTarget()`

作用:

- 告诉 LLVM: 真要用这个 target 时, 应该创建哪个 `TargetMachine`

### 6. TargetMC 层

- `llvm/lib/Target/Toy/TargetDesc/ToyTargetDesc.cpp`

你最常创建 / 修改的是:

- 函数 `LLVMInitializeToyTargetMC()`
- 一组 `createToyMC...` 工厂函数

作用:

- 给 `TargetMachine` 初始化时要用到的 MC 层对象提供创建入口

## 如果你只关心“为了某个现象该改什么”, 可以先看这里

### 想实现 `llc --version`

优先检查:

- `llvm/CMakeLists.txt`
- `Triple::ArchType`
- `Triple::getArchTypeForLLVMName`
- `Target TheToyTarget`
- `LLVMInitializeToyTargetInfo()`

### 想实现 `llc -march=toy` 不再 unknown

优先检查:

- `Triple::getArchTypeForLLVMName`
- `Triple::getArchTypeName`
- `LLVMInitializeToyTargetInfo()`

### 想实现 `llc -mtriple=toy-unknown-elf` 能走到创建后端

优先检查:

- `LLVMInitializeToyTarget()`
- `RegisterTargetMachine<ToyTargetMachine>`
- `ToyTargetMachine` 类

### 想实现 `llc` 不再报 `Unable to create reg info`

优先检查:

- `createToyMCRegisterInfo`
- `LLVMInitializeToyTargetMC()`

### 想实现 `llc` 不再报 `Unable to create asm info`

优先检查:

- `createToyMCAsmInfo`
- `LLVMInitializeToyTargetMC()`

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

## 先认识这几个文件分别是什么

第一次接触这一节时, 很多人会觉得:

- 为什么同样都叫 “初始化”, 要分成三个函数?
- 为什么 `TargetInfo`、`TargetMachine`、`TargetDesc` 分散在不同目录?

其实它们是在补三层不同的能力:

- [ToyTargetInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetInfo/ToyTargetInfo.cpp)
  负责 “报名字”。让 LLVM 的 target 列表里出现 `toy`
- [ToyTargetMachine.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.cpp)
  负责 “建总入口对象”。让 LLVM 知道如何 new 一个 `ToyTargetMachine`
- [ToyTargetDesc.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyTargetDesc.cpp)
  负责 “挂低层工厂”。让 LLVM 知道如何创建寄存器元信息、指令元信息、汇编信息等 MC 组件

你可以把这一节理解成 “先把 target 接到 LLVM 主板上”, 还没进入真正出码。

## 现在把“文件”进一步细化到“函数”

这一节最容易卡住的地方就是:

- 我知道要改 `Triple.cpp`
- 但我不知道到底改哪个函数
- 更不知道改它是为了让哪一步成立

下面这张对照表就是专门解决这个问题的。

### 文件和函数对照表

- `Triple.h`
  - 改 `enum ArchType`
  - 作用: 给新架构增加一个内部枚举值, 这是后面所有注册的前提

- `Triple.cpp`
  - 改 `Triple::getArchTypeName`
  - 作用: 把内部枚举转回标准架构名, 比如 `Triple::toy -> "toy"`
  - 不改会怎样: 某些地方打印 triple 名字或重新拼 triple 字符串时会不完整

- `Triple.cpp`
  - 改 `Triple::getArchTypeForLLVMName`
  - 作用: 把 LLVM 使用的字符串名解析成内部架构枚举
  - 不改会怎样: `llc -march=toy` 仍然可能不认识你的架构

- `Triple.cpp`
  - 改 `Triple::getArchTypePrefix`
  - 作用: 告诉 LLVM 这个架构归属于哪一类前缀, 比如 `riscv32/riscv64 -> riscv`
  - 不改会怎样: 某些按架构族分类的逻辑、命名空间前缀或特性判断可能不一致

- `Triple.cpp`
  - 改 `Triple::getArchPointerBitWidth`
  - 作用: 告诉 LLVM 这是 16/32/64 位架构
  - 不改会怎样: 指针宽度相关逻辑会错误, 例如 `isArch32Bit()` / `isArch64Bit()` 判断失真

- `Triple.cpp`
  - 改 `Triple::get64BitArchVariant`
  - 作用: 如果你有 `foo32/foo64`, 这里告诉 LLVM 怎么从 32 位切到 64 位
  - 不改会怎样: 需要切换架构位宽的逻辑可能得不到正确结果

- `Triple.cpp`
  - 改 `Triple::isLittleEndian`
  - 作用: 告诉 LLVM 你的架构默认大端还是小端
  - 不改会怎样: 某些端序判断会错

- `ToyTargetInfo.cpp`
  - 创建全局 `Target TheToyTarget`
  - 作用: 这是整个 target 的核心对象, 其他注册都要围绕它

- `ToyTargetInfo.cpp`
  - 实现 `LLVMInitializeToyTargetInfo()`
  - 作用: 用 `RegisterTarget` 把字符串名字、架构枚举、全局 target 对象绑在一起
  - 不改会怎样: `llc --version` 里看不到你的 target

- `ToyTargetMachine.cpp`
  - 实现 `LLVMInitializeToyTarget()`
  - 作用: 用 `RegisterTargetMachine<ToyTargetMachine>` 告诉 LLVM 用哪个类实例化这个后端
  - 不改会怎样: `llc` 认得 target 名字, 但不会创建 `TargetMachine`

- `ToyTargetMachine.h/.cpp`
  - 创建 `ToyTargetMachine` 类
  - 作用: codegen 阶段后端总入口
  - 不改会怎样: 即使注册函数存在, 也没有可构造的后端类

- `ToyTargetDesc.cpp`
  - 实现 `LLVMInitializeToyTargetMC()`
  - 作用: 注册 `MCRegisterInfo` / `MCInstrInfo` / `MCAsmInfo` 等工厂
  - 不改会怎样: `ToyTargetMachine` 创建后会很快因缺少 MC 依赖而失败

## toy-1 在解决什么问题

### 目标

让 `llc --version` 的 `Registered Targets` 里出现:

- `toy - Toy RISC-V backend`

### 关键知识点

- `Triple::ArchType`
- `RegisterTarget`
- 顶层 `LLVM_ALL_TARGETS`

这里最值得先弄懂的是 `Triple::ArchType`。

`Triple` 是 LLVM 用来描述目标平台的一套字符串 / 枚举体系, 例如:

- 架构: `riscv32`, `x86_64`, `arm`
- vendor
- os
- environment

当你在命令行里写 `-march=toy` 或 `-mtriple=toy-unknown-elf` 时, LLVM 最终都要把字符串解析成 `Triple` 里的枚举和字段。  
所以如果没有在 `Triple` 里加 `toy`, 后面所有注册都接不上。

### 在 `Triple.h` 里到底改什么

最直接的改动就是在 `Triple::ArchType` 里加一个新枚举值, 例如:

- `toy`
- 或者像 RISC-V 风格 target 那样加 `toyriscv32` / `toyriscv64`

这一步的意义不是“只是加个名字”, 而是:

- 给 LLVM 一个稳定的内部架构编号

后面所有地方都不会直接长期拿字符串 `"toy"` 做核心判断, 而是会尽量转成:

- `Triple::toy`
- `Triple::toyriscv32`
- `Triple::toyriscv64`

这种枚举值。

所以 `ArchType` 更像是 “架构的内部身份证枚举”。

### 在 `Triple.cpp` 里到底改哪些函数

如果你是第一次加一个架构, 最常见的是下面几类函数。

#### `Triple::getArchTypeName`

作用:

- 内部枚举 -> 标准字符串名字

例如你加了:

- `Triple::toyriscv32`

这里就要能返回:

- `"toyriscv32"`

为什么需要它:

- LLVM 有很多地方需要把 triple 或 arch 再转回字符串
- 比如重新拼 triple、打印信息、某些辅助逻辑里取默认名

#### `Triple::getArchTypeForLLVMName`

作用:

- 字符串名字 -> 内部枚举

这通常是第一节里最关键的一个解析函数。  
你在命令行里写:

- `-march=toy`
- `-march=toyriscv32`

LLVM 最终得在这里把它变成:

- `Triple::toy`
- `Triple::toyriscv32`

如果这一项没加, 很多时候就是你最直观看到的:

- unknown target
- 或 `-march` 名字不识别

#### `Triple::getArchTypePrefix`

作用:

- 告诉 LLVM “这个架构属于哪一个族前缀”

例如:

- `riscv32/riscv64` 共享前缀 `riscv`
- `toyriscv32/toyriscv64` 可以共享前缀 `toyriscv`

第一次做后端时, 你可以把它理解成:

- “给这个架构系列定义一个家族名”

这不是最早会爆炸的点, 但属于应该补齐的基础信息。

#### `Triple::getArchPointerBitWidth`

作用:

- 告诉 LLVM 这个架构的默认指针宽度

例如:

- `toyriscv32 -> 32`
- `toyriscv64 -> 64`

为什么它很重要:

- LLVM 后面会用它判断 `isArch32Bit()` / `isArch64Bit()`
- DataLayout、ABI、指针大小相关逻辑都会间接受影响

所以它不是“可选装饰”, 而是架构语义的一部分。

#### `Triple::get64BitArchVariant`

作用:

- 告诉 LLVM 如何从 32 位架构切换到 64 位对应体

例如:

- `toyriscv32 -> toyriscv64`

什么时候需要:

- 你有明显的 32/64 双架构变体时

如果你的 target 只有一个单独架构名, 这个部分可以相对简单。  
如果你做的是 RISC-V 风格 target, 这一项通常就值得补上。

#### `Triple::isLittleEndian`

作用:

- 告诉 LLVM 这个架构默认是否小端

对 RISC-V 风格 toy target 来说, 往往会是:

- `true`

为什么要补:

- LLVM 里有不少地方会通过 triple 直接问端序
- 这会影响后续一些底层判断

### 关键动作

1. 把 `Toy` 加进 [llvm/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/CMakeLists.txt)
2. 在 triple 枚举里加入 `toy`
3. 在 `ToyTargetInfo.cpp` 中通过 `RegisterTarget` 绑定:
   - 架构枚举
   - 命令行名字 `toy`
   - `TheToyTarget`

`RegisterTarget` 这一步可以先直观理解成:

- 把 “命令行上的 `toy`”
- 和 “内部的 `Triple::toy`”
- 以及 “运行时那一个全局 target 对象 `TheToyTarget`”

绑成同一个 target 身份。

如果少了这一步, LLVM 即使源码里存在你的目录, 也不知道命令行上的 `toy` 应该对应谁。

### `ToyTargetInfo.cpp` 里到底要看什么

这个文件通常非常短, 但作用非常核心。

#### 全局对象 `Target TheToyTarget;`

作用:

- 创建一个可被 LLVM 注册系统引用的 target 对象实例

为什么必须是全局的:

- 后面的 `LLVMInitializeToyTargetInfo`
- `LLVMInitializeToyTarget`
- `LLVMInitializeToyTargetMC`

都要引用同一个对象。  
如果它不是同一个全局对象, 这些注册就会分裂成几段互相连不上的状态。

#### `LLVMInitializeToyTargetInfo()`

这里最重要的一句通常是:

- `RegisterTarget<...> X(TheToyTarget, "toy", "...", "...");`

这句的意义可以拆成三层:

1. 把内部架构枚举接进注册系统
2. 把命令行名字 `"toy"` 接进注册系统
3. 把它们都绑定到 `TheToyTarget`

也就是说, 这一步是在建立:

- “名字”
- “架构枚举”
- “运行时 target 对象”

三者之间的一致身份。

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

这里第一次会出现一个很重要的类: `ToyTargetMachine`。

第一次读时, 你先不要把它理解成“一个很复杂的大类”, 先把它当成:

- 整个后端在 codegen 阶段的总控制器

很多别的对象都会通过它间接拿到, 例如:

- `Subtarget`
- `TargetLowering`
- `FrameLowering`
- `TargetPassConfig`
- `TargetObjectFile`

它自己不一定亲自做所有细节, 但它决定 LLVM 应该去哪里拿这些能力。

### `ToyTargetMachine.h/.cpp` 里到底要看什么

第一次读这两个文件, 建议只盯下面几处。

#### 类定义 `class ToyTargetMachine : public LLVMTargetMachine`

作用:

- 声明 “这个后端的 codegen 顶层入口类”

你可以把它先理解成:

- LLVM 进入这个 target 后, 之后大多数能力查询都会从它身上开始

#### `LLVMInitializeToyTarget()`

这里最关键的一句通常是:

- `RegisterTargetMachine<ToyTargetMachine> X(TheToyTarget);`

它在补什么语义:

- 前面 `TargetInfo` 只让 LLVM 认得这个 target 的名字
- 现在这句是在告诉 LLVM: “真要实例化时, 请用 `ToyTargetMachine` 这个类”

所以它补的是:

- target 名字 -> target machine 构造器

这层连接。

#### `ToyTargetMachine` 构造函数

第一次读构造函数时不要试图全懂, 先看它在初始化哪些大块:

- data layout
- `TargetObjectFile`
- `Subtarget`
- `initAsmInfo()`

你可以先粗略理解为:

- 这里在把“这个 target 后面运行要依赖的大对象”挂起来

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

这也是第一次读 LLVM 后端时一个很关键的经验:

- “能创建 `TargetMachine`” 不等于 “target 已经完整了”

`TargetMachine` 本身还依赖一整套 MC 层工厂。也就是说, 你只是从“门口进来了”, 还没有把屋子里的家具摆齐。

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

这些 `RegisterMC...` 的函数第一次看很容易眼花, 但其实可以先分成四类:

- 寄存器和指令元信息
  - `MCRegInfo`
  - `MCInstrInfo`
  - `MCSubtargetInfo`
- 汇编文本相关
  - `MCAsmInfo`
  - `MCInstPrinter`
- 机器码编码相关
  - `MCCodeEmitter`
  - `MCAsmBackend`
- 目标扩展输出
  - `TargetStreamer`

第一遍学习时, 你最应该抓住前两组。因为它们最早就会阻塞 `llc` 往下走。

### `ToyTargetDesc.cpp` 里到底要看什么

这个文件第一次看最容易被一堆 `createToy...` 和 `RegisterMC...` 吓到。  
你可以先把它拆成两半:

#### 前半部分: 一组 `createToyMC...` 工厂函数

例如:

- `createToyMCRegisterInfo`
- `createToyMCInstrInfo`
- `createToyMCSubtargetInfo`
- `createToyMCAsmInfo`

作用:

- 告诉 LLVM “如果你要这个 MC 对象, 请这样创建它”

这不是立刻创建全局单例, 而是在提供“创建方法”。

#### 后半部分: `LLVMInitializeToyTargetMC()`

这里最关键的是一串注册:

- `TargetRegistry::RegisterMCRegInfo(...)`
- `TargetRegistry::RegisterMCInstrInfo(...)`
- `TargetRegistry::RegisterMCSubtargetInfo(...)`
- `TargetRegistry::RegisterMCAsmInfo(...)`

作用:

- 把前面那些工厂函数挂到 `TheToyTarget` 上

所以这一节真正的关系是:

- `createToyMC...`
  是“工厂函数定义”
- `RegisterMC...`
  是“把工厂函数注册给 target”

## `TargetInfo` / `TargetMachine` / `TargetDesc` 应该如何实现

这一节到这里, 你已经知道它们分别负责什么了。  
下面再进一步回答你最关心的事:

- 这三个文件最小应该写成什么样
- 每一部分代码到底是在补哪一层能力

### 一. `TargetInfo` 的最小实现

通常文件是:

- [ToyTargetInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetInfo/ToyTargetInfo.cpp)

最小骨架一般只有两样东西:

1. 一个全局 `Target` 对象
2. 一个 `LLVMInitializeToyTargetInfo()` 函数

可以先按下面这个形状理解:

```cpp
#include "llvm/MC/TargetRegistry.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

Target TheToyTarget;

extern "C" void LLVMInitializeToyTargetInfo() {
  RegisterTarget<Triple::toy, true> X(TheToyTarget, "toy",
                                      "Toy backend", "Toy");
}
```

这里每一部分在干什么:

- `Target TheToyTarget;`
  - 创建一个全局 target 对象
  - 这是后面所有注册的共同核心对象

- `LLVMInitializeToyTargetInfo()`
  - 这是 LLVM 初始化 target info 层时调用的入口函数
  - 它不负责创建 `TargetMachine`
  - 它只负责注册 “这个 target 叫什么、对应哪个架构枚举、绑定到哪个 target 对象”

- `RegisterTarget<Triple::toy, true>`
  - 把 `Triple::toy`、字符串 `"toy"`、对象 `TheToyTarget` 绑在一起

如果这一层没实现, 最直接的现象通常是:

- `llc --version` 里看不到你的 target
- `-march=toy` 无法映射到你的 target

### 二. `TargetMachine` 的最小实现

通常文件是:

- [ToyTargetMachine.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.h)
- [ToyTargetMachine.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.cpp)

第一次实现时, 你要把它理解成:

- codegen 阶段这个 target 的总入口类

#### 最小类骨架

```cpp
class ToyTargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  std::unique_ptr<ToySubtarget> Subtarget;

public:
  ToyTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                   StringRef FS, const TargetOptions &Options,
                   std::optional<Reloc::Model> RM,
                   std::optional<CodeModel::Model> CM,
                   CodeGenOptLevel OL, bool JIT);

  const ToySubtarget *getSubtargetImpl(const Function &) const override;
  TargetLoweringObjectFile *getObjFileLowering() const override;
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
};
```

这个类第一次看不用全懂, 先抓三件事:

- 它继承 `LLVMTargetMachine`
- 它会持有 `Subtarget`
- 它会持有 `TargetObjectFile`

也就是说, 它不是做某一个具体动作, 而是在把后端运行需要的大块对象组织起来。

#### 最小注册入口

```cpp
extern Target TheToyTarget;

extern "C" void LLVMInitializeToyTarget() {
  RegisterTargetMachine<ToyTargetMachine> X(TheToyTarget);
}
```

这一步的意义非常明确:

- `TargetInfo` 只解决 “LLVM 知道有这个 target”
- `LLVMInitializeToyTarget()` 解决 “LLVM 知道要用哪个类实例化这个 target”

也就是说, 它补的是:

- `TheToyTarget -> ToyTargetMachine`

这条连接。

#### 最小构造函数骨架

```cpp
ToyTargetMachine::ToyTargetMachine(const Target &T, const Triple &TT,
                                   StringRef CPU, StringRef FS,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM,
                                   std::optional<CodeModel::Model> CM,
                                   CodeGenOptLevel OL, bool JIT)
    : LLVMTargetMachine(T, "your-datalayout", TT, CPU, FS, Options,
                        RM.value_or(Reloc::Static),
                        CM.value_or(CodeModel::Small), OL),
      TLOF(std::make_unique<ToyTargetObjectFile>()),
      Subtarget(std::make_unique<ToySubtarget>(TT, CPU, FS, *this)) {
  initAsmInfo();
}
```

第一次读构造函数时, 先只理解它在初始化哪几大块:

- `LLVMTargetMachine(...)`
  - 把 DataLayout、triple、CPU/feature、重定位模型等基础信息交给基类

- `TLOF(...)`
  - 创建目标文件布局对象

- `Subtarget(...)`
  - 创建这个 target 的具体 CPU/feature 能力对象

- `initAsmInfo()`
  - 初始化汇编相关信息
  - 这里会很早依赖 `TargetDesc` 注册的 MC 工厂

如果这层没实现或没注册好, 常见现象是:

- `Could not allocate target machine`

### 三. `TargetDesc` 的最小实现

通常文件是:

- [ToyTargetDesc.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyTargetDesc.cpp)

第一次实现它时, 你要把它理解成:

- MC 层对象工厂的注册中心

它不负责做高层 codegen, 它负责的是:

- 寄存器元信息怎么建
- 指令元信息怎么建
- subtarget 元信息怎么建
- 汇编信息怎么建

#### 最小工厂函数骨架

```cpp
static MCInstrInfo *createToyMCInstrInfo() {
  auto *X = new MCInstrInfo();
  InitToyMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createToyMCRegisterInfo(const Triple &TT) {
  auto *X = new MCRegisterInfo();
  InitToyMCRegisterInfo(X, Toy::RA);
  return X;
}

static MCSubtargetInfo *createToyMCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU, StringRef FS) {
  return createToyMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

static MCAsmInfo *createToyMCAsmInfo(const MCRegisterInfo &MRI,
                                     const Triple &TT,
                                     const MCTargetOptions &Options) {
  return new MCAsmInfo();
}
```

这几类函数分别在回答:

- `createToyMCInstrInfo`
  - “如果 LLVM 需要本 target 的指令元信息, 应该怎么 new 出来?”

- `createToyMCRegisterInfo`
  - “如果 LLVM 需要本 target 的寄存器元信息, 应该怎么 new 出来?”

- `createToyMCSubtargetInfo`
  - “如果 LLVM 需要本 target 的 subtarget 元信息, 应该怎么建?”

- `createToyMCAsmInfo`
  - “如果 LLVM 需要汇编层基本信息, 应该怎么建?”

这些函数本身不是在“立刻创建全局对象”, 而是在提供创建方法。

#### 最小注册入口

```cpp
extern Target TheToyTarget;

extern "C" void LLVMInitializeToyTargetMC() {
  TargetRegistry::RegisterMCRegInfo(TheToyTarget, createToyMCRegisterInfo);
  TargetRegistry::RegisterMCInstrInfo(TheToyTarget, createToyMCInstrInfo);
  TargetRegistry::RegisterMCSubtargetInfo(TheToyTarget,
                                          createToyMCSubtargetInfo);
  TargetRegistry::RegisterMCAsmInfo(TheToyTarget, createToyMCAsmInfo);
}
```

这一段最核心的作用是:

- 把前面那些 `createToyMC...` 工厂函数挂到 `TheToyTarget`

所以你可以把 `TargetDesc` 文件拆成两半来记:

- 前半段:
  - 定义工厂函数
- 后半段:
  - 把工厂注册给 target

如果这层没实现, 常见现象是:

- `Unable to create reg info`
- `MCInstrInfo` / `MCAsmInfo` 缺失
- `TargetMachine` 刚初始化就失败

## 这三层是怎么串起来的

把这三层连成一条线, 你会更不容易混淆:

1. `Triple`
   - 把命令行字符串解析成内部架构枚举

2. `TargetInfo`
   - 把架构枚举、target 名字、`TheToyTarget` 绑定起来

3. `TargetMachine`
   - 告诉 LLVM: 这个 `TheToyTarget` 应该实例化成 `ToyTargetMachine`

4. `TargetDesc`
   - 告诉 LLVM: `ToyTargetMachine` 初始化所需的 MC 对象该怎么创建

也就是说, 第一节最核心的链条是:

- 名字
- 架构枚举
- target 对象
- target machine
- MC 工厂

## 第一阶段你应该做到什么程度

第一阶段不要追求功能完整, 只要做到下面这些就已经是成功:

- `ToyTargetInfo.cpp`
  - 有 `TheToyTarget`
  - 有 `LLVMInitializeToyTargetInfo`

- `ToyTargetMachine.h/.cpp`
  - 有 `ToyTargetMachine` 类
  - 有 `LLVMInitializeToyTarget`
  - 构造函数能正常走到 `initAsmInfo()`

- `ToyTargetDesc.cpp`
  - 有最小的 `MCInstrInfo` / `MCRegisterInfo` / `MCSubtargetInfo` / `MCAsmInfo`
  - 有 `LLVMInitializeToyTargetMC`

这时你的目标不是 “已经能编 C 程序”, 而是:

- `llc --version` 能看到 target
- `llc -march=toy` 不再是 unknown target
- `llc` 开始报更靠后的错误

这其实说明你已经把第一节该搭的骨架搭起来了。

## 第一节真正动手时的顺序

到这里, 你已经知道“要改哪些文件”和“这些文件大概在干什么”。  
下面这部分更偏操作手册:

- 先改哪个文件
- 再改哪个函数
- 每改完一步, 应该期待什么现象

如果你第一次自己搭后端, 强烈建议按这个顺序来, 不要跳着改。

### 第 1 步: 先把 target 接进构建系统

先看:

- `llvm/CMakeLists.txt`
- `llvm/lib/Target/Toy/CMakeLists.txt`
- `llvm/lib/Target/Toy/TargetInfo/CMakeLists.txt`
- `llvm/lib/Target/Toy/TargetDesc/CMakeLists.txt`

你这一步通常要做的事是:

1. 在顶层 target 列表里把 `Toy` 加进去
2. 新建 `Toy` target 目录自己的 `CMakeLists.txt`
3. 让 `Toy` 目录包含 `TargetInfo` 和 `TargetDesc` 子目录
4. 给 `TargetInfo` 和 `TargetDesc` 各建一个最小库

这一步的目标不是运行 `llc`, 而是:

- 确保你的 target 目录会被编译系统看见

如果这一步没做, 后面所有 C++ 文件即使写了也可能根本没参与构建。

### 第 2 步: 在 `Triple.h` 里加架构枚举

先看:

- [llvm/include/llvm/TargetParser/Triple.h](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/llvm/include/llvm/TargetParser/Triple.h)

你通常要改的是:

- `enum ArchType`

例如加:

- `toy`
- 或者 `toyriscv32` / `toyriscv64`

这一步的作用是:

- 给 LLVM 增加一个新的“内部架构编号”

此时你还没有让字符串名生效, 只是先把内部枚举准备好。

### 第 3 步: 在 `Triple.cpp` 里把名字和枚举接起来

先看:

- [llvm/lib/TargetParser/Triple.cpp](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/llvm/lib/TargetParser/Triple.cpp)

这一阶段建议至少按下面顺序改:

1. `Triple::getArchTypeName`
2. `Triple::getArchTypeForLLVMName`
3. `Triple::getArchTypePrefix`
4. `Triple::getArchPointerBitWidth`

如果是双变体架构, 再继续改:

5. `Triple::get64BitArchVariant`
6. `Triple::isLittleEndian`

这一步每个函数的操作目标可以再压缩成一句话:

- `getArchTypeName`
  - 让内部枚举能重新变回字符串名字
- `getArchTypeForLLVMName`
  - 让字符串名字能解析成内部枚举
- `getArchTypePrefix`
  - 让 LLVM 知道这个架构属于哪个家族前缀
- `getArchPointerBitWidth`
  - 让 LLVM 知道它是 32 位还是 64 位
- `get64BitArchVariant`
  - 让 LLVM 知道 32 位变体如何切到 64 位
- `isLittleEndian`
  - 让 LLVM 知道默认端序

这一阶段完成后, 你等于把 “字符串 target 名字” 和 “内部架构语义” 接起来了。

### 第 4 步: 实现 `ToyTargetInfo.cpp`

先看:

- [ToyTargetInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetInfo/ToyTargetInfo.cpp)

你要创建的最小内容是:

1. `Target TheToyTarget;`
2. `LLVMInitializeToyTargetInfo()`
3. 里面的 `RegisterTarget<...>`

这一阶段的作用是:

- 把 target 名字真正注册到 LLVM target 列表里

如果你前面的 `Triple` 已经接好了, 而这一步也接好了, 你应该期待的第一个明显变化是:

- `llc --version` 的 `Registered Targets` 里开始出现你的 target

这是第一节里最关键的第一个“看得见的成功信号”。

### 第 5 步: 实现 `ToyTargetMachine.h/.cpp`

先看:

- [ToyTargetMachine.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.h)
- [ToyTargetMachine.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetMachine.cpp)

第一轮只做最小骨架:

1. 声明 `class ToyTargetMachine : public LLVMTargetMachine`
2. 实现 `LLVMInitializeToyTarget()`
3. 写最小构造函数
4. 提供最基本的 `getSubtargetImpl` / `getObjFileLowering` / `createPassConfig`

这一阶段的作用是:

- 告诉 LLVM: 如果用户真的选了这个 target, 应该创建哪个 codegen 总入口对象

做到这一步之后, 你最常见的现象是:

- `llc` 已经不再停留在 “unknown target”
- 而是进入 “尝试创建 target machine”

如果 MC 层还没接, 常见下一阶段报错就是:

- `Could not allocate target machine`
- 或初始化过程中很快因为缺少 reg info / asm info 失败

### 第 6 步: 实现 `ToyTargetDesc.cpp`

先看:

- [ToyTargetDesc.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyTargetDesc.cpp)

第一轮你最少补这几类内容:

1. `createToyMCRegisterInfo`
2. `createToyMCInstrInfo`
3. `createToyMCSubtargetInfo`
4. `createToyMCAsmInfo`
5. `LLVMInitializeToyTargetMC()`

这一阶段的作用是:

- 给 `TargetMachine` 初始化要用到的 MC 层依赖提供工厂

做到这一步后, 你应该期待:

- `TargetMachine` 不再一创建就立刻因为缺 MC 对象而失败
- `llc` 能往更后面的 codegen 流程继续走

这时候即使后面还会报:

- 缺少 `Subtarget`
- 缺少 `ISelLowering`
- 缺少 `FrameLowering`

也不要紧, 这说明你已经成功走出第一节了。

## 每一步改完后, 你应该检查什么

第一次搭后端时, 最好不要一口气改完所有文件再看结果。  
更好的方法是每完成一小步就看一次现象。

### 检查点 1: 构建系统接好了没有

你可以检查:

- CMake / Ninja 是否开始看到 `Toy` 相关目标
- target 目录下的源码是否参与编译

如果这里没通, 后面代码层再对也没有意义。

### 检查点 2: `Triple` 接好了没有

你可以关注:

- 编译 `Triple.cpp` 时是否出现你新增枚举相关错误
- `-march=toy` 是否还被当成完全未知字符串

### 检查点 3: `TargetInfo` 接好了没有

最直接检查:

- `llc --version` 里有没有你的 target 名字

如果还没有, 重点回看:

- `Target TheToyTarget`
- `LLVMInitializeToyTargetInfo`
- `RegisterTarget<...>`

### 检查点 4: `TargetMachine` 接好了没有

如果 target 名字已经出现, 但 `llc -march=toy` 继续报:

- `Could not allocate target machine`

那就重点回看:

- `LLVMInitializeToyTarget`
- `RegisterTargetMachine<ToyTargetMachine>`
- `ToyTargetMachine` 类本身是否真的存在并可构造

### 检查点 5: `TargetDesc` 接好了没有

如果 `TargetMachine` 已经开始创建, 但很快出现:

- `Unable to create reg info`
- `Unable to create asm info`
- `MCInstrInfo` 相关缺失

那就重点回看:

- `LLVMInitializeToyTargetMC`
- 那几类 `createToyMC...` 工厂函数

## 第一节最常见的三个误区

### 误区 1: 一上来就去写 `Subtarget`

这会让你很容易在还没把 target 注册链接通时, 就提前掉进后面的实现细节里。

第一节的目标不是:

- 做完整 codegen

而是:

- 让 LLVM 认出并接入这个 target

### 误区 2: 只改 `Triple.h`, 不改 `Triple.cpp`

这会导致一种很典型的半通状态:

- 代码里有了枚举
- 但字符串解析和回写逻辑没接上

结果就是你以为“架构已经加了”, 实际上命令行和 triple 逻辑仍然接不上。

### 误区 3: target 名字前后不一致

例如:

- triple 用 `toy`
- target 对象叫别的
- CMake 组件名又是另一套

这种问题不一定第一时间报特别清晰的错, 但会让你很难沿着 LLVM 现有 target 的结构去对照理解。

### 必须先有的 TableGen 产物

在 [Toy/CMakeLists.txt](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/CMakeLists.txt) 里会生成:

- `ToyGenSubtargetInfo.inc`
- `ToyGenInstrInfo.inc`
- `ToyGenRegisterInfo.inc`

`ToyTargetDesc.cpp` 会直接 include 它们。

### 关键理解

`LLVMInitializeToyTargetMC()` 不是在直接创建这些对象, 而是在注册 “createXXX 工厂函数”。

后续真正创建对象的是 `TheToyTarget` 在运行时通过这些工厂完成的。

“工厂函数” 这个词你一定要适应。LLVM 这里不是提前 new 好所有对象挂全局, 而是:

1. 先把 “怎么创建它” 的函数注册进去
2. 等真正需要时再调用工厂创建对象

这样做的好处是:

- 各个 target 的实现解耦
- 初始化顺序更清晰
- 不必在启动时把所有对象都强行建出来

## 这一步最容易混淆的点

### `TargetInfo` 不等于 `TargetMC`

- `TargetInfo` 负责让 target 能被枚举到
- `TargetMC` 负责让 target 的低层元信息能被构造出来

### `TargetMachine` 不等于 `Subtarget`

- `TargetMachine` 是后端顶层入口
- `Subtarget` 是特定 CPU / feature 组合

这一点初学者特别容易混。最直观的区分方式是:

- `TargetMachine`
  更像整个后端的“总配置”和“总入口”
- `Subtarget`
  更像 “这一台具体 CPU 机器长什么样”

例如将来一个 target 可能同时支持多个 CPU 型号:

- 它们共享同一个 `TargetMachine` 类
- 但会对应不同的 `Subtarget` 配置

### `MCRegisterInfo` 不等于 `RegisterInfo`

- `MCRegisterInfo`
  偏汇编/机器码层面的寄存器元信息
- `RegisterInfo`
  偏 codegen 阶段对寄存器的策略和行为

你可以把它们想成:

- `MCRegisterInfo`
  在回答 “这个架构有哪些寄存器、编号是什么、名字是什么”
- `RegisterInfo`
  在回答 “编译器在生成代码时, 哪些寄存器保留、哪些 callee-saved、FrameIndex 最后怎么落地”

前者更像静态字典, 后者更像 codegen 策略对象。

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
