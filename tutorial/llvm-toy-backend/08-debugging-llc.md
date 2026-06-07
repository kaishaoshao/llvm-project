# 08 Debugging llc

## 学习目标

- 知道调试 `llc` 时为什么优先用 `Debug` 构建
- 知道遇到崩溃时应该先下哪些断点
- 知道如何把问题分层定位到 `Triple`、`TargetMachine`、`MC` 或更后面的 codegen

## 这节解决什么问题

当你已经做到:

- `llc --version` 里看得到 `coralnpu32`

但一运行:

```bash
./build/bin/llc -mtriple=coralnpu32 test.ll
```

就出现:

- 崩溃
- `Could not allocate target machine`
- `Unable to create reg info`
- 更靠后的 `Subtarget` / `InstrInfo` / `AsmPrinter` 问题

这时最有效的做法不是盲改代码, 而是先把 `llc` 调起来。

## 最推荐的调试方式

优先采用这套组合:

1. `Debug` 构建
2. 最小 `.ll` 输入
3. `lldb` 启动 `llc`
4. 少量关键断点

这是 LLVM 后端前期最稳的调试方法。

## 第 1 步: 建一个 Debug 版 llc

你平时跑功能可以用优化构建, 但调试时建议单独建一个目录:

```bash
cmake -S llvm -B build-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_TARGETS_TO_BUILD="RISCV;CoralNPU;TOYRISCV"
ninja -C build-debug llc
```

这样做的原因是:

- 有完整调试符号
- 变量值更容易看
- 断言更早暴露问题
- 回溯通常比 `Optimized build` 更可信

## 第 2 步: 永远先准备最小输入

先不要拿复杂测试调。

优先用最小 IR:

```llvm
define i32 @main() {
  ret i32 0
}
```

原因是:

- 复杂 IR 会把问题藏起来
- 你现在调的是 target 接线, 不是指令覆盖率

## 第 3 步: 用 lldb 启动 llc

最常用命令:

```bash
lldb -- ./build-debug/bin/llc -mtriple=coralnpu32 test.ll
```

进入 `lldb` 后, 最常用的是:

- `run`
- `bt`
- `frame select 0`
- `up`
- `down`
- `p 变量名`
- `b 函数名`
- `c`
- `n`
- `s`

如果你只记住两条命令, 先记:

- `run`
- `bt`

## 第 4 步: 先打哪几个断点

### 调 target 注册链时

先打:

```lldb
b LLVMInitializeCoralNPUTargetInfo
b LLVMInitializeCoralNPUTarget
b LLVMInitializeCoralNPUTargetMC
b llvm::TargetRegistry::lookupTarget
b llvm::Target::createTargetMachine
```

这组断点回答的是:

- target 有没有被初始化
- `lookupTarget` 有没有找到你的 target
- LLVM 有没有真的去创建 `TargetMachine`

### 调 `TargetMachine` 时

如果你已经补了 `CoralNPUTargetMachine`, 再加:

```lldb
b llvm::CoralNPUTargetMachine::CoralNPUTargetMachine
```

如果符号名暂时不好断, 就直接按文件行号断:

```lldb
b /Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.cpp:1
```

## 第 5 步: 先看哪几个变量

在 `llc` 前期接线阶段, 最值得先看的变量通常是:

- `TheTriple`
- `TheTarget`
- `Target`

尤其是 `llc.cpp` 里:

- `TargetRegistry::lookupTarget(...)` 之后
- `TheTarget->createTargetMachine(...)` 之后

最想确认的是:

- `TheTarget` 为空还是非空
- `Target` 为空还是非空

如果:

- `TheTarget` 是空

通常说明:

- triple 解析不对
- target 注册没接上

如果:

- `Target` 是空

通常说明:

- `RegisterTargetMachine<CoralNPUTargetMachine>` 没注册
- `CoralNPUTargetMachine` 还只是空壳

## 第 6 步: 按层定位问题

调 `llc` 时, 最好把问题分成这几层:

### 第 1 层: target 是否被识别

先看:

```bash
./build-debug/bin/llc --version
```

如果这里都没有 `coralnpu32`, 优先回去查:

- `Triple.h`
- `Triple.cpp`
- `TargetInfo`
- `llvm/CMakeLists.txt`

### 第 2 层: target machine 是否能创建

如果:

```bash
./build-debug/bin/llc -mtriple=coralnpu32 test.ll
```

一跑就崩, 或断在 `createTargetMachine`, 优先查:

- `LLVMInitializeCoralNPUTarget`
- `RegisterTargetMachine<CoralNPUTargetMachine>`
- `CoralNPUTargetMachine` 构造函数

### 第 3 层: MC 基础对象是否能创建

如果 `TargetMachine` 已经创建了, 但很快报:

- `Unable to create reg info`
- `Unable to create asm info`

优先查:

- `LLVMInitializeCoralNPUTargetMC`
- `createCoralNPUMCRegisterInfo`
- `createCoralNPUMCInstrInfo`
- `createCoralNPUMCSubtargetInfo`
- `createCoralNPUMCAsmInfo`

### 第 4 层: codegen 骨架是否存在

如果已经过了 MC, 但继续报:

- `Subtarget` 缺失
- `InstrInfo` 缺失
- `FrameLowering` 缺失

优先查:

- `CoralNPUSubtarget`
- `CoralNPUTargetLowering`
- `CoralNPUFrameLowering`
- `.td` 是否已经生成基础 `inc`

## 常用辅助参数

### 看调试输出

```bash
./build-debug/bin/llc -debug -mtriple=coralnpu32 test.ll
```

### 看 pass 结构

```bash
./build-debug/bin/llc -debug-pass=Structure -mtriple=coralnpu32 test.ll
```

### 看各 pass 前后 IR/MIR

```bash
./build-debug/bin/llc -print-after-all -mtriple=coralnpu32 test.ll
```

注意:

- `-debug` 在 `Debug` 构建下更有价值
- 如果你的代码里还没加 `LLVM_DEBUG(...)`, 输出可能不会很多

## 一条很实用的调试顺序

每次出问题时, 尽量按这个顺序排:

1. `llc --version`
2. `llc -mtriple=coralnpu32 test.ll`
3. `lldb + bt`
4. 看 `lookupTarget`
5. 看 `createTargetMachine`
6. 看 `TargetMachine` 构造函数
7. 再看 `TargetMC`
8. 最后才看 `Subtarget` / `ISel` / `AsmPrinter`

这个顺序能帮你避免一上来就掉进后面的复杂代码里。

## 你当前阶段最该记住的判断

如果你现在已经看到:

- `llc --version` 里有 `coralnpu32`

但一运行 `llc -mtriple=coralnpu32 test.ll` 就崩,  
最先怀疑的通常不是 IR, 而是:

- `TargetMachine` 还没真正实现
- 或 `TargetMC` 还是空壳

## 和前两节怎么配合看

- 如果你现在还在做 target 识别, 配合看 [01-target-registration.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/01-target-registration.md)
- 如果你现在已经进入 `TargetMachine` / `Subtarget` 骨架阶段, 配合看 [02-minimal-pipeline.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/02-minimal-pipeline.md)
