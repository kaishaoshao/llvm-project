# 05 Calls And ABI

## 学习目标

- 理解调用约定在 LLVM 后端中的位置
- 看懂 `LowerFormalArguments`、`LowerCall`、`LowerReturn`
- 理解 `glue` 为何是调用和返回相关代码里非常关键的一部分

## 这一节按什么目标来学

这一节最容易失焦, 因为 call/return 会同时碰到:

- 参数传递
- 返回值
- 隐式寄存器
- 栈参数
- 调度顺序

更好的学法是按目标拆开。

## 目标 -> 需要实现什么函数

### 目标 1: 被调用函数能接住参数

优先实现:

- `LowerFormalArguments`

你在实现的是:

- “参数到达 callee 时, 怎么从参数寄存器或栈槽变成函数体里可用的值”

### 目标 2: 调用者能把参数送出去

优先实现:

- `LowerCall`

你在实现的是:

- “调用发生前, 参数怎么放进约定好的寄存器或栈位置”

### 目标 3: 被调用函数能按约定返回结果

优先实现:

- `LowerReturn`

你在实现的是:

- “返回值怎么放到约定的位置, 以及最后怎么发出 return”

### 目标 4: 参数分配规则不再全手写

优先补齐:

- `ToyCallingConv.td` 或你自己的 `CallingConv.td`

你在实现的是:

- “参数/返回值放寄存器还是落栈的规则手册”

### 目标 5: 调用相关指令不会被错误重排

优先理解和实现:

- `glue`
- `Chain` / `Glue` 相关节点连接

你在实现的是:

- “让隐式使用物理寄存器的调用序列保持正确顺序”

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

## 如果你只关心“某个现象没通, 应该先看哪里”

### 现象: 函数进来以后参数全是错的

优先看:

- `LowerFormalArguments`
- `CallingConv.td`

### 现象: 调用出去时参数没放对位置

优先看:

- `LowerCall`
- `CallingConv.td`

### 现象: 返回值不对或 return 路径炸掉

优先看:

- `LowerReturn`

### 现象: 调用前后某些寄存器值神秘坏掉

优先看:

- `glue`
- 调用序列里隐式寄存器依赖
- `getCallPreservedMask`

## 先把 ABI 和调用约定说清楚

很多人第一次学这里会把 ABI 和 calling convention 混成一件事。  
先给一个够用的入门理解:

- ABI
  是更大的约定集合, 包括参数传递、返回值、寄存器保存规则、栈对齐、对象文件接口等
- calling convention
  更聚焦在 “函数怎么收参数、怎么传参数、怎么返回”

所以这一节虽然标题里会写 ABI, 但你真正先接触到的核心是 calling convention。

再补三个最基本的角色:

- caller
  发起调用的函数
- callee
  被调用的函数
- call site
  代码里那条实际发生调用的位置

如果这三个角色不先分清, 后面 `LowerCall` 和 `LowerFormalArguments` 很容易看反。

## 调用约定在后端里分成三块

### `LowerFormalArguments`

负责:

- callee 进入函数时, 从参数寄存器或参数栈槽中取参数

你可以把它理解成:

- “站在被调用者的门口, 看调用者是怎么把参数送进来的”

它处理的是函数一开始的入参落地问题。

### `LowerCall`

负责:

- caller 发起调用前, 把参数放进约定好的寄存器或栈位置
- 生成调用指令
- 调用结束后取回返回值

它处理的是 “站在调用者视角, 如何准备这次调用”。

所以第一次学时, 一个很好的记忆法是:

- `LowerFormalArguments`
  站在 callee 视角
- `LowerCall`
  站在 caller 视角

### `LowerReturn`

负责:

- 按约定把返回值放进指定寄存器或内存
- 生成返回指令

它处理的是 “被调用者要离开函数时, 怎么把结果交还出去”。

于是这三者刚好围成了一个完整闭环:

- 入口收参数
- 中间发调用
- 出口交返回值

## `ToyCallingConv.td` 的作用

它不是直接执行参数传递, 而是声明规则。

例如:

- 哪种类型优先放 `A0/A1`
- 多出来的参数怎么落栈

这些规则会被 TableGen 生成辅助逻辑, 然后在 `LowerFormalArguments` / `LowerCall` 中调用。

这里最重要的认知是:

- `ToyCallingConv.td` 不直接搬运参数
- 它提供的是 “分配规则”

也就是说, 它更像一份规则手册:

- 第 1 个 `i32` 参数先试哪个寄存器
- 寄存器用完后参数往哪里落
- 返回值优先放哪里

真正把参数节点、寄存器复制、栈对象拼起来的, 还是 `ToyISelLowering.cpp` 里的 C++ 逻辑。

## 为什么 call 很难

函数调用不是 “生成一条跳转” 就结束了, 它还隐含:

- 参数寄存器占用
- 返回值寄存器占用
- `ra` 使用
- caller/callee 保存规则
- stack argument 布局

你可以把函数调用想成一串必须严格配合的动作:

1. 按规则给参数分配位置
2. 必要时保存某些会被 clobber 的状态
3. 发出调用本身
4. 知道哪些寄存器会在调用后失效
5. 从指定位置取回返回值

任何一步没接上, 最后都可能不是“立刻崩”, 而是出现非常隐蔽的错码。

这就是为什么 toy 教程把调用约定拆成很多步, 而不是一次讲完。

## `glue` 为什么重要

调用和返回会隐式依赖某些物理寄存器, 比如:

- `a0`, `a1`
- `ra`

如果你只在逻辑上 “知道会用这些寄存器”, 但没有把这个依赖显式绑定在 DAG 上, scheduler 或寄存器分配可能会把相关指令排坏。

`glue` 的作用就是:

- 把相关节点绑在一起
- 防止错误重排

第一次学 `glue` 时, 你不必追它所有实现细节, 先把它理解成:

- “给 DAG 上的几个节点加一条必须连着走的粘合关系”

因为函数调用会隐式读写某些物理寄存器。  
如果没有这种粘合关系, 编译器只从普通数据依赖看, 可能会觉得某些节点可以重排, 但实际上重排后就违反了调用约定。

这是 LLVM 后端里非常容易忽略, 但又极其关键的一点。

## 学这节时最值得盯住的三个例子

### 无参数无返回值调用

最适合看清调用指令本身

### 两个 `i32` 参数

最适合看寄存器传参

### 超出寄存器数量的参数

最适合看 stack argument 布局

第一次读这节代码时, 我很建议你真的拿这三种例子分别在脑子里走一遍:

- 没参数没返回值
  最容易只看“调用这件事本身”
- 两个 `i32`
  最容易看懂寄存器分配
- 参数很多
  最容易看懂寄存器和栈是怎么混合使用的

这样你再去看 `LowerCall` / `LowerFormalArguments`, 每段代码都更有落点。

## 这一节完成后的阶段目标

这一节完成不等于:

- ABI 已经完整无缺

更合理的完成标准是:

- 简单整数参数可以进出函数
- 最小函数调用可以正确传参和取回返回值
- 调用序列不会因为隐式寄存器依赖而被错误打乱

## 注意事项

- `CALL` 指令要正确标记属性, 否则 LLVM 不会意识到函数有调用
- 如果要保存 `ra`, 往往还要配合 CSR 逻辑
- `loadRegFromStackSlot` 往往会在恢复返回地址时变得必要
- 返回结构体时, clang 生成的 IR 形式会受 target ABI 影响

如果你第一次读这里觉得特别抽象, 可以先不要上来就读“返回结构体”。  
最好的顺序通常是:

1. 无参数无返回值
2. 简单整数参数和返回值
3. 超寄存器数量参数
4. 聚合类型 / 结构体返回

因为复杂度确实是一级一级增加的。

## 自查问题

1. `LowerFormalArguments` 和 `LowerCall` 为什么是镜像关系?
2. 为什么只记录 “call 用了 a0” 还不够, 还需要 `glue`?
3. 为什么返回结构体会把 ABI 复杂度显著提高?
