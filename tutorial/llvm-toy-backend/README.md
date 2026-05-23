# LLVM Toy Backend Tutorial

这套教程基于两份材料整理:

- `llvm-toy` 代码: `/Volumes/wsk/code/llvm-mlir/llvm-toy`
- 教程文章: `/Volumes/wsk/code/llvm-mlir/pages/toolchain/llvm_toy_riscv_backend.html`

目标不是直接照抄代码, 而是建立一条清晰的学习路线:

1. 先理解 LLVM 后端的分层
2. 再理解一个最小 target 是怎么被接进 LLVM 的
3. 再逐步理解 isel, MachineInstr, MCInst, 栈帧, 调用约定
4. 最后再看浮点, object file, intrinsic 等扩展

## 建议阅读顺序

1. [00-overview.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/00-overview.md)
2. [01-target-registration.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/01-target-registration.md)
3. [02-minimal-pipeline.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/02-minimal-pipeline.md)
4. [03-isel-and-asm.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/03-isel-and-asm.md)
5. [04-frame-and-registers.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/04-frame-and-registers.md)
6. [05-calls-and-abi.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/05-calls-and-abi.md)
7. [06-advanced-topics.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/06-advanced-topics.md)
8. [07-file-map-and-glossary.md](/Volumes/wsk/code/llvm-mlir/llvm-project_mips/tutorial/llvm-toy-backend/07-file-map-and-glossary.md)

## 教程组织方式

每一节都包含这些部分:

- 学习目标
- 对应的 toy 章节
- 关键知识点
- 必看文件
- 常见报错与原因
- 注意事项
- 自查问题

## 重要提醒

- `llvm-toy` 使用的是较老一代 LLVM API, 教程里的模块职责和推进顺序仍然非常有价值, 但具体接口签名不一定和你当前仓库一致。
- 你当前仓库里与 `Triple` 相关的路径是 `llvm/include/llvm/TargetParser/Triple.h` 和 `llvm/lib/TargetParser/Triple.cpp`, 而不是更老的 `ADT/Triple.h` / `Support/Triple.cpp`。
- 学习时优先理解 “为什么要补这一块”, 不要机械照抄代码。

