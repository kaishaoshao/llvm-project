# 05 Calls And ABI

## 学习目标

- 理解调用约定在 LLVM 后端中的位置
- 看懂 `LowerFormalArguments`、`LowerCall`、`LowerReturn`
- 理解 `glue` 为何是调用和返回相关代码里非常关键的一部分

## 对应 toy 章节

- `toy-24: LowerReturn`
- `toy-26` 到 `toy-31: LowerCall / LowerFormalArguments / LowerReturn`
- `toy-37: LowerCall Pt. 5`
- `toy-38: glue`
- `toy-40: return struct`

## 必看文件

- [ToyISelLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.h)
- [ToyISelLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.cpp)
- [ToyCallingConv.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyCallingConv.td)
- [ToyInstrInfo.td](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyInstrInfo.td)
- [ToyFrameLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.cpp)
- [ToyRegisterInfo.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyRegisterInfo.cpp)

## 调用约定在后端里分成三块

### `LowerFormalArguments`

负责:

- callee 进入函数时, 从参数寄存器或参数栈槽中取参数

### `LowerCall`

负责:

- caller 发起调用前, 把参数放进约定好的寄存器或栈位置
- 生成调用指令
- 调用结束后取回返回值

### `LowerReturn`

负责:

- 按约定把返回值放进指定寄存器或内存
- 生成返回指令

## `ToyCallingConv.td` 的作用

它不是直接执行参数传递, 而是声明规则。

例如:

- 哪种类型优先放 `A0/A1`
- 多出来的参数怎么落栈

这些规则会被 TableGen 生成辅助逻辑, 然后在 `LowerFormalArguments` / `LowerCall` 中调用。

## 为什么 call 很难

函数调用不是 “生成一条跳转” 就结束了, 它还隐含:

- 参数寄存器占用
- 返回值寄存器占用
- `ra` 使用
- caller/callee 保存规则
- stack argument 布局

这就是为什么 toy 教程把调用约定拆成很多步, 而不是一次讲完。

## `glue` 为什么重要

调用和返回会隐式依赖某些物理寄存器, 比如:

- `a0`, `a1`
- `ra`

如果你只在逻辑上 “知道会用这些寄存器”, 但没有把这个依赖显式绑定在 DAG 上, scheduler 或寄存器分配可能会把相关指令排坏。

`glue` 的作用就是:

- 把相关节点绑在一起
- 防止错误重排

这是 LLVM 后端里非常容易忽略, 但又极其关键的一点。

## 学这节时最值得盯住的三个例子

### 无参数无返回值调用

最适合看清调用指令本身

### 两个 `i32` 参数

最适合看寄存器传参

### 超出寄存器数量的参数

最适合看 stack argument 布局

## 注意事项

- `CALL` 指令要正确标记属性, 否则 LLVM 不会意识到函数有调用
- 如果要保存 `ra`, 往往还要配合 CSR 逻辑
- `loadRegFromStackSlot` 往往会在恢复返回地址时变得必要
- 返回结构体时, clang 生成的 IR 形式会受 target ABI 影响

## 自查问题

1. `LowerFormalArguments` 和 `LowerCall` 为什么是镜像关系?
2. 为什么只记录 “call 用了 a0” 还不够, 还需要 `glue`?
3. 为什么返回结构体会把 ABI 复杂度显著提高?

