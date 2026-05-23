# 06 Advanced Topics

## 学习目标

- 知道 toy 教程后半段在补什么
- 能区分 “后端核心闭环” 和 “能力扩展”
- 为后续深入软浮点、硬浮点、intrinsic、object file 做导航

## 对应 toy 章节

- `toy-20` 到 `toy-23`
- `toy-34` 到 `toy-49`

## 主题划分

### 1. 地址类扩展

- global address
- global address with constant offset
- frameindex with constant offset

这些问题的共同点是:

- operand 不再只是简单的寄存器和立即数
- AsmPrinter / MC lowering / encoder 都要开始真正处理更复杂的寻址形式

### 2. 控制流和比较

- `setcc`
- `br_cc`

这里的重点不是单条指令, 而是:

- LLVM 的 target-independent 条件语义
- 如何映射到一个较弱的真实 ISA

### 3. 类型提升

- `type promotion`

后端未必直接支持所有类型宽度, LLVM 会通过 legalize 和 promote 进行拆分或提升。

这一节很适合理解:

- 为什么 8/16 位算术并不一定需要后端单独实现所有算术指令
- 为什么 load/store pattern 仍然很重要

### 4. 浮点

- soft float
- hard float
- `truncstoref32`

重点在于:

- legalize 阶段如何决定用软浮点还是硬浮点
- 浮点寄存器类如何接入
- 浮点常量为什么经常走 constant pool

### 5. builtin / intrinsic

- `fma`
- `fmaxf`
- custom intrinsic

你要理解:

- clang 和 IR 里 builtin / intrinsic 的入口是什么
- target 是通过 operation legalization, pattern 还是 target hook 来接它

### 6. object file

- `MCCodeEmitter`
- `AsmBackend`
- `fixup`
- object writer

这是 MC 层真正落地到二进制的部分。适合在前面所有 asm 路都通了以后再看。

## 学习建议

### 如果你当前目标是做一个最小后端

可以先跳过:

- 硬浮点
- builtin / intrinsic
- object file

### 如果你当前目标是做一个能跑 C 函数的教学后端

优先补:

- global address
- `LowerCall` / `LowerFormalArguments`
- `setcc` / `br_cc`
- stack arguments

### 如果你当前目标是做一个真正可用的后端

最终都要补, 但顺序依然建议:

1. asm
2. function calls
3. control flow
4. floating point
5. object file

## 注意事项

- object file 是 MC 层知识最密集的部分, 不要过早进入
- 浮点往往会暴露 constant pool 和 legalize 的理解缺口
- intrinsic 不只是加一条指令, 常常还要理解 clang 和 IR 前端的生成方式

## 自查问题

1. 为什么 hard float 不只是 “加几个 FPR 和浮点指令”?
2. 为什么 object file 相关内容适合最后学?
3. `setcc` / `br_cc` 为什么经常体现 ISA 与 LLVM 中间语义的差异?

