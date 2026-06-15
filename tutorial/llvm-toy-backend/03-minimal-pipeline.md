# 03 Minimal Pipeline

## 学习目标

- 知道当前 `CoralNPUTargetMachine` 已经做了什么
- 知道当前代码为什么还不能进入完整的 codegen pipeline
- 分清 `TargetMachine`、`TargetMC`、`Subtarget` 这三层的先后关系
- 知道你现在下一步最合理的最小目标是什么

## 这一章怎么读

第 1 章解决的是:

- LLVM 能不能认出 `coralnpu32`
- LLVM 能不能开始创建 `CoralNPUTargetMachine`

第 2 章解决的是:

- `.td` 这一层最小骨架有没有接上

这一章解决的是:

- `CoralNPUTargetMachine` 创建之后, LLVM 还会立刻要求什么
- 你当前代码已经到哪里
- 下一步到底该补 `TargetMC`、`Subtarget` 还是更后面的对象

所以这章不是泛讲“LLVM pipeline 很复杂”, 而是直接贴着你当前的 `CoralNPU` 代码讲。

## 当前代码已经做到哪一步

当前这两个文件:

- [CoralNPUTargetMachine.h](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.h)
- [CoralNPUTargetMachine.cpp](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.cpp)

已经做到了两件事:

1. 定义了 `class CoralNPUTargetMachine : public LLVMTargetMachine`
2. 在 `LLVMInitializeCoralNPUTarget()` 里注册了:
   - `RegisterTargetMachine<CoralNPUTargetMachine>`

当前构造函数也已经真的调用了:

```cpp
initAsmInfo();
```

这意味着:

- 你已经不再停留在 “只认得 target 名字”
- LLVM 已经真的开始创建 `TargetMachine`

这比第 1 章前半段已经往前走了一步。

## 当前代码还缺什么

虽然 `CoralNPUTargetMachine` 已经存在, 但它现在仍然是一个很薄的骨架。

当前头文件里只有:

- 构造函数
- 析构函数

还没有这些常见的最小接口:

- `getSubtargetImpl`
- `getObjFileLowering`
- `createPassConfig`

这说明当前代码阶段还没有走到:

- `Subtarget`
- `TargetLowering`
- `FrameLowering`
- `DAGToDAGISel`

这些对象真正接进来的时候。

## 你当前最重要的运行时顺序

现在一定要先把这条真实顺序记住:

`TargetInfo`
-> `TargetMachine`
-> `initAsmInfo()`
-> `MCRegisterInfo`
-> `MCInstrInfo`
-> `MCSubtargetInfo`
-> `MCAsmInfo`

这条顺序是你当前调试里已经实际看到的。

也就是说, 当 `CoralNPUTargetMachine` 构造函数里调用:

```cpp
initAsmInfo();
```

LLVM 的下一个问题不是:

- “你的 `Subtarget` 在哪?”
- “你的 `FrameLowering` 在哪?”

而是先问:

- “你的 `MCRegisterInfo` 能不能创建?”

所以如果你现在断在:

- `Unable to create reg info`

那你当前还没有到该实现 `Subtarget` 的阶段。

## 为什么现在还不该先做 `Subtarget`

这是这章最重要的判断。

很多人看到 `TargetMachine` 已经有了, 就会自然往下写:

- `CoralNPUSubtarget`
- `CoralNPUTargetLowering`
- `CoralNPUFrameLowering`

但对你当前这个仓库来说, 这一步其实还太早。

原因很简单:

- 当前 `CoralNPUTargetMachine` 构造函数刚进来
- 就在 `initAsmInfo()` 里卡住了

也就是说, 运行时连 `TargetMC` 都还没接好, 根本还没有走到:

- `Subtarget`
- `InstrInfo`
- `FrameLowering`

这些更后面的层。

所以你现在最该做的不是继续往“完整 pipeline”展开, 而是先把:

- `TargetMachine -> TargetMC`

这条链走通。

## 当前阶段最小目标是什么

对你现在的代码来说, 这一章最现实的目标不是:

- “把最小 pipeline 全补齐”

而是:

- **让 `CoralNPUTargetMachine::initAsmInfo()` 不再因为 `MC` 对象缺失而立刻失败**

也就是先让下面这些能工作:

1. `createCoralNPUMCRegisterInfo`
2. `createCoralNPUMCInstrInfo`
3. `createCoralNPUMCSubtargetInfo`
4. `createCoralNPUMCAsmInfo`
5. `LLVMInitializeCoralNPUTargetMC()`

换句话说:

- 第 3 章现在对你最重要的部分, 实际上还是 `TargetMC`
- 而不是 `Subtarget`

## 当前 `TargetMachine` 代码应该怎么理解

当前 [CoralNPUTargetMachine.cpp](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.cpp) 的核心逻辑是:

```cpp
extern "C" void LLVMInitializeCoralNPUTarget() {
  RegisterTargetMachine<CoralNPUTargetMachine> X(TheCoralNPUTarget);
}
```

这段代码解决的是:

- “当 LLVM 已经认出 target 是 `CoralNPU` 后, 应该 new 哪个 `TargetMachine` 类”

而构造函数:

```cpp
: LLVMTargetMachine(...data layout..., TT, CPU, FS, Options, ...)
{
  initAsmInfo();
}
```

解决的是:

- 把 `CoralNPUTargetMachine` 接到 LLVM 通用 `LLVMTargetMachine` 基类上
- 然后立刻要求 target 提供最小的 MC 层对象

### 这里的 data layout 暂时应该怎么理解

当前代码里 data layout 是手写字符串:

```cpp
"e-m:m-p:32:32-i8:8:32-i16:16:32-i64:64-n32-S64"
```

对当前阶段来说, 你先把它理解成:

- “CoralNPU 当前按 32 位目标来组织基本数据布局”

现在不用急着在教程里把每一段都吃透。  
在这个阶段更重要的是:

- `TargetMachine` 已经真的被创建
- 后面卡住的第一个点是 `initAsmInfo()`

## 这一章真正要分清的对象边界

### `TargetMachine`

它是:

- LLVM 进入当前后端后的总入口

当前它至少承担了两件事:

1. 通过 `RegisterTargetMachine<...>` 被注册
2. 在构造时调用 `initAsmInfo()`

### `TargetMC`

它是:

- `TargetMachine` 一创建后就会立刻用到的一组底层工厂

当前最直接的职责就是提供:

- `MCRegisterInfo`
- `MCInstrInfo`
- `MCSubtargetInfo`
- `MCAsmInfo`

### `Subtarget`

它是:

- codegen 更后面阶段的能力聚合对象

但它不是你当前第一个该补的东西, 因为现在运行时还没走到那里。

## 什么时候才算进入下一阶段

当你把 `TargetMC` 这层补到最小可用之后, 你看到的现象一般会变成:

- 不再断在 `Unable to create reg info`
- 报错开始往后移动
- 或者开始真正暴露 `Subtarget` / `InstrInfo` / `FrameLowering` 的缺失

只有到了那时, 你才真正进入:

- `TargetMachine -> Subtarget -> codegen capability objects`

这一阶段。

## 当前仓库下, 第 3 章最推荐的实际推进顺序

结合你现在的 `CoralNPU` 代码状态, 最推荐的顺序是:

1. 先把 `CoralNPUTargetDesc.h` / `.cpp` 的 `TargetMC` 骨架接完整
2. 让 `initAsmInfo()` 至少能拿到最小:
   - `MCRegisterInfo`
   - `MCInstrInfo`
   - `MCSubtargetInfo`
   - `MCAsmInfo`
3. 再回头补 `CoralNPUTargetMachine.h` 里缺的最小接口:
   - `getSubtargetImpl`
   - `getObjFileLowering`
   - `createPassConfig`
4. 然后才开始真正引入:
   - `CoralNPUSubtarget`
   - `CoralNPUTargetLowering`
   - `CoralNPUFrameLowering`

所以当前阶段不要把重点放在:

- `DAGToDAGISel`
- `AsmPrinter`
- `InstPrinter`

这些更后面的对象上。

## 第 3 章现在最该配合看的代码

当前最值得和本章一起对照看的文件是:

- [CoralNPUTargetMachine.h](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.h)
- [CoralNPUTargetMachine.cpp](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUTargetMachine.cpp)
- [MCTargetDesc/CoralNPUTargetDesc.h](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/MCTargetDesc/CoralNPUTargetDesc.h)
- [MCTargetDesc/CoralNPUTargetDesc.cpp](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/MCTargetDesc/CoralNPUTargetDesc.cpp)
- [CoralNPU/CMakeLists.txt](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CMakeLists.txt)

## 当前阶段的验收标准

这一章做完, 当前最现实的验收标准不是:

- `CoralNPU` 已经有完整 codegen pipeline

而是:

1. 你能分清 `TargetMachine`、`TargetMC`、`Subtarget` 这三层谁先谁后
2. 你知道当前代码为什么会先断在 `initAsmInfo()`
3. 你知道当前最小目标其实是先补 `TargetMC`
4. 你不会在这个阶段过早跳去写 `Subtarget`、`FrameLowering`、`DAGToDAGISel`

做到这里, 第 3 章才算真正服务于你当前的 `CoralNPU` 代码状态。
