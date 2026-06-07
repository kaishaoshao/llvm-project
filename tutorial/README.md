# Tutorial Index

这个目录现在分成两套资料:

- [llvm-toy-backend/README.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/README.md)
  - 这套偏 LLVM target backend 学习路线
  - 当前版本默认把你的练习目标理解成 `CoralNPU`
  - 重点是 `TargetInfo`、`TargetMachine`、`MCTargetDesc`、`Subtarget`、`ISel`、`AsmPrinter`

- [coralnpu/README.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/README.md)
  - 这套偏 CoralNPU 资料整理
  - 重点是 CoralNPU 的架构、微架构、编程方式、仿真和系统集成
  - 如果你想先看单文件总览, 直接读 [coralnpu/CoralNPU-Combined.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/coralnpu/CoralNPU-Combined.md)

如果你当前目标是:

- 学 LLVM 后端怎么一步步搭起来
  - 先看 `llvm-toy-backend`

- 学 CoralNPU 本身是什么、怎么运行程序、怎么放到 SoC 里
  - 先看 `coralnpu`
