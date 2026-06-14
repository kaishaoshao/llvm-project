# 02 Minimal Pipeline Skeleton

## 学习目标

- 理解后端最小骨架为什么需要 `Subtarget`、`TargetLowering`、`FrameLowering`
- 理解教程中 `toy-4` 到 `toy-10` 的推进顺序
- 分清 “先占位” 和 “开始真正出码” 的边界

## 这一节完成后你应该达到什么状态

做完第二节, 你的目标也不是“已经能正确生成 CoralNPU 汇编”, 而是:

1. `CoralNPUTargetMachine` 不再只是空壳
2. `CoralNPUSubtarget` 能被返回
3. `TargetLowering/RegisterInfo/FrameLowering` 有最小占位对象
4. pass pipeline 开始有地方接入 target-specific isel

如果第二节结束后你看到的是:

- `Subtarget` 相关报错往后移动了
- `TargetLowering` / `FrameLowering` 缺失问题开始暴露

那通常说明你在正确推进。

## 这一节怎么读

第一节解决的是:

- `llc` 认出 target
- LLVM 能开始尝试创建 target machine

第二节解决的是:

- target machine 创建后, LLVM 还缺哪些“骨架对象”才能继续往下走

所以这一节更适合按“我现在想让 `llc` 多走一步”来读, 而不是按文件树顺序硬看。

## 目标 -> 需要实现什么函数

如果你现在已经从 “target 可识别” 走到 “一跑 `llc -mtriple=coralnpu32 ...` 就崩”,  
建议配合看 [08-debugging-llc.md](/Users/kaishaoshao/Desktop/code/llvm-project_mips/tutorial/llvm-toy-backend/08-debugging-llc.md)。

第二节最常见的调试重点是:

- `RegisterTargetMachine<CoralNPUTargetMachine>` 是否真的注册
- `CoralNPUTargetMachine` 构造函数有没有进到
- `Target` 是否在 `createTargetMachine` 后变成空指针

## 第二节开始前要先建立的依赖关系

如果你在这里还会疑惑:

- 为什么第二节才轮到 `Subtarget`
- 为什么 `InstrInfo` / `FrameLowering` 不能比 `TargetMachine` 更早做

可以先记住这条对象依赖:

`TargetMachine`
-> `Subtarget`
-> `InstrInfo/RegisterInfo/FrameLowering/TargetLowering`

这条依赖意味着:

- `Subtarget` 通常是 `TargetMachine` 的成员, 或由 `TargetMachine::getSubtargetImpl()` 返回
- `InstrInfo/RegisterInfo/FrameLowering/TargetLowering` 又通常是 `Subtarget` 持有并提供的

所以你现在在第二节做的事, 本质上是在补:

- `TargetMachine` 后面那一整串能力对象

而不是跳过 `TargetMachine` 直接写后面的类。

这也是为什么第二节的第一个目标仍然是:

- 把 `CoralNPUTargetMachine` 变成真正可用的总入口

然后才轮到:

- `CoralNPUSubtarget`
- `CoralNPUTargetLowering`
- `CoralNPUFrameLowering`
- `CoralNPUDAGToDAGISel`

### 目标 1: `llc -mtriple=...` 不再只停在 target 注册阶段

你至少要保证这些存在:

- `class CoralNPUTargetMachine : public LLVMTargetMachine`
- `LLVMInitializeCoralNPUTarget()`
- `RegisterTargetMachine<CoralNPUTargetMachine>`
- `CoralNPUTargetMachine` 构造函数

这一目标的本质是:

- 让 LLVM 不只是“认得这个 target”
- 而是真的有一个 codegen 总入口类可以创建

推荐验证命令:

```bash
./build/bin/llc -mtriple=coralnpu32 test.ll
```

### 目标 2: `TargetMachine` 能返回一个 `Subtarget`

你至少要保证这些存在:

- `class CoralNPUSubtarget`
- `CoralNPUTargetMachine::getSubtargetImpl`
- `CoralNPUSubtarget` 构造函数

这一目标的本质是:

- 让后续 pass 有地方查询目标能力

如果这一步没做, 后面很多 pass 会不知道该向谁要:

- `InstrInfo`
- `RegisterInfo`
- `FrameLowering`
- `TargetLowering`

推荐验证方式:

- `lldb` 里看 `CoralNPUTargetMachine::getSubtargetImpl` 有没有被走到
- 或在实现里先加最小日志/断点确认

### 目标 3: `Subtarget` 能提供最小 codegen 能力对象

你至少要保证这些存在:

- `CoralNPUSubtarget::getInstrInfo`
- `CoralNPUSubtarget::getRegisterInfo`
- `CoralNPUSubtarget::getFrameLowering`
- `CoralNPUSubtarget::getTargetLowering`

这一目标的本质是:

- 让 LLVM 的 codegen pass 有统一入口去拿目标相关策略对象

推荐验证方式:

- 看崩溃是否从 “拿不到 `Subtarget`” 往后移动到更具体的能力对象

### 目标 4: LLVM 能进入 SelectionDAG 指令选择骨架

你至少要保证这些存在:

- `class CoralNPUDAGToDAGISel`
- `CoralNPUTargetMachine::createPassConfig`
- `TargetPassConfig::addInstSelector` 的覆写

这一目标的本质是:

- 让 pass pipeline 真正把目标专属的 isel pass 插进去

推荐验证方式:

- 看 `createPassConfig`
- 看 `addInstSelector`
- 必要时配合 `-debug-pass=Structure`

### 目标 5: LLVM 有最小 lowering 骨架

你至少要保证这些存在:

- `class CoralNPUTargetLowering`
- `CoralNPUSubtarget` 中持有它
- `CoralNPUSubtarget::getTargetLowering`

这一目标的本质是:

- 让 LLVM IR / DAG 层的 target-specific 语义有承接对象

### 目标 6: LLVM 有最小栈帧骨架

你至少要保证这些存在:

- `class CoralNPUFrameLowering`
- `CoralNPUSubtarget` 中持有它
- `CoralNPUSubtarget::getFrameLowering`

这一目标的本质是:

- 让函数 prologue / epilogue 和栈帧约定有承接对象

### 目标 7: LLVM 后面能开始尝试走到汇编输出链路

你至少要保证这些存在:

- `class CoralNPUAsmPrinter`
- `class CoralNPUInstPrinter`
- `LLVMInitializeCoralNPUTargetMC()` 已经注册对应 MC 能力

这一目标的本质是:

- 让机器指令后面有机会继续走向 `MCInst` 和 asm 文本

## 第二节推荐的实际操作顺序

如果你要自己落地第二节, 最推荐按这个顺序:

1. 先把 `CoralNPUTargetMachine` 补完整到最小可用
2. 再实现 `CoralNPUSubtarget`
3. 再让 `CoralNPUSubtarget` 持有 `TargetLowering/RegisterInfo/FrameLowering`
4. 再补 `createPassConfig`
5. 最后才把 `CoralNPUDAGToDAGISel` 接进来

这一节最重要的任务边界是:

- 先把对象链接上
- 还不是去追求完整指令语义

## 对应 toy 章节

- `toy-4: ToyDAGToDAGISel`
- `toy-5: ToyInstPrinter`
- `toy-6: ToyAsmPrinter`
- `toy-7: ToyTargetObjectFile`
- `toy-8: ToySubtarget`
- `toy-9: ToyTargetLowering`
- `toy-10: ToyFrameLowering`

## 必看文件

- [ToyDAGToDAGISel.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.h)
- [ToyDAGToDAGISel.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyDAGToDAGISel.cpp)
- [ToyAsmPrinter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.h)
- [ToyAsmPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyAsmPrinter.cpp)
- [ToyTargetObjectFile.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetObjectFile.h)
- [ToyTargetObjectFile.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyTargetObjectFile.cpp)
- [ToySubtarget.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.h)
- [ToySubtarget.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToySubtarget.cpp)
- [ToyISelLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.h)
- [ToyISelLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyISelLowering.cpp)
- [ToyFrameLowering.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.h)
- [ToyFrameLowering.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/ToyFrameLowering.cpp)
- [TargetDesc/ToyInstPrinter.h](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.h)
- [TargetDesc/ToyInstPrinter.cpp](/Volumes/wsk/code/llvm-mlir/llvm-toy/llvm/lib/Target/Toy/TargetDesc/ToyInstPrinter.cpp)

## 如果你现在是在做 `CoralNPU`, 第二节最值得优先补什么

结合你当前仓库状态, 第二节最适合优先落地的是下面这个顺序:

1. `CoralNPUTargetMachine`
2. `CoralNPUSubtarget`
3. `CoralNPUTargetLowering`
4. `CoralNPUFrameLowering`
5. `CoralNPUDAGToDAGISel`

先不要同时展开:

- `AsmPrinter`
- `InstPrinter`
- 真正的指令 pattern
- 完整寄存器系统

因为第二节的目标还是:

- 把 codegen 骨架站起来

而不是:

- 立刻能出很多正确指令

## 这几个模块各自负责什么

在读这些类之前, 你可以先记一个总原则:

- 这一阶段出现的很多类, 不是因为它们已经“功能完整”
- 而是因为 LLVM 的 codegen pipeline 需要它们先占住位置

也就是说, 这一节的重点是 “让骨架站起来”, 不是 “每个模块都做完”

## 第二节里你第一次最容易遇到的几个现象

### 现象 1: target 已识别, 但 `TargetMachine` 很空

这通常意味着你还缺:

- 真正的 `CoralNPUTargetMachine` 类内容
- `createPassConfig`
- `getSubtargetImpl`

### 现象 2: 已经有 `TargetMachine`, 但后面的 pass 找不到目标能力对象

这通常意味着你还缺:

- `CoralNPUSubtarget`
- `Subtarget` 里持有的 `InstrInfo/RegisterInfo/FrameLowering/TargetLowering`

### 现象 3: pipeline 里没有你的 isel pass

这通常意味着你还缺:

- `CoralNPUDAGToDAGISel`
- `TargetPassConfig::addInstSelector`

### 现象 4: 你觉得“我还没实现指令, 为什么先要这些类”

这是第二节最核心的心态转换:

- LLVM 先要一套目标能力对象
- 然后才会在这些对象里继续要求更具体的行为

所以这里很多类的第一版都只是“占位骨架”。

### `ToyDAGToDAGISel`

把 SelectionDAG 节点选成 target 的机器指令。

当前阶段它先是一个入口占位, 让 pass pipeline 能继续往下走。

第一次读这个类时, 先只抓两个点:

- 它属于 SelectionDAG 阶段
- 它的输出已经是 `MachineInstr` 方向的目标相关表示

你现在还不必把 matcher 细节看透, 但一定要知道:

- `TargetLowering` 更像 “先把问题翻译成 target 能懂的话”
- `DAGToDAGISel` 更像 “再把这些 DAG 节点选成具体指令”

如果换成你当前自己的 target 名字, 第二节你最终想补到的就是:

- `CoralNPUDAGToDAGISel`

它在最初阶段甚至可以几乎不做复杂逻辑, 但它必须存在, 因为:

- `TargetPassConfig` 需要把它插进 pass pipeline

### `ToyInstPrinter`

负责 `MCInst -> asm`。

注意:

- 它不处理 `MachineInstr`
- 它不做寄存器分配
- 它不决定调用约定

这是一个非常容易被误解的类。它名字里有 `Printer`, 很多人第一次会以为:

- “那它是不是整个后端最后打印汇编的总入口?”

其实不是。它只是专门负责:

- 已经 lower 成 `MCInst` 之后
- 把 opcode、寄存器、立即数格式化成汇编文本

所以它更像 “汇编文本格式化器”, 而不是 codegen 主流程控制器。

### `ToyAsmPrinter`

负责 `MachineInstr -> MCInst`。

它和 `ToyInstPrinter` 的分工是这一节最重要的理解点。

如果一定要给它一句最直白的描述:

- `ToyAsmPrinter` 站在 codegen 和 MC 的交界处

前面 LLVM 还在操作 `MachineInstr`, 到了它这里开始准备进入:

- `MCInst`
- 汇编文本
- 机器码编码

所以这个类在整体流水线中的位置非常关键。

### `ToyTargetObjectFile`

负责 object file 相关的 section 和数据放置规则。

即使你现在还不真正输出 `.o`, `AsmPrinter` 也会依赖它。

第一次学时你可以先把它理解成:

- “目标文件格式相关的后台规则对象”

它平时不一定很显眼, 但一旦涉及:

- 全局变量放哪个 section
- 常量池放哪里
- 某些符号怎么布局

它就会参与进来。

### `ToySubtarget`

是后端能力的聚合点。通常它会持有:

- `InstrInfo`
- `RegisterInfo`
- `FrameLowering`
- `TargetLowering`

LLVM 的很多 pass 都是先拿 `Subtarget`, 再从里面取这些对象。

`Subtarget` 是第一阶段之后最值得你建立直觉的类之一。它不是单独干某个具体活, 而是:

- 把 “这台具体 CPU 的能力” 聚合成一个对象

第一次阅读它时, 你可以重点观察:

- 它持有哪些成员对象
- 构造函数里初始化了哪些能力
- `TargetMachine` 是怎么把它暴露给别的 pass 的

如果你现在要在自己的后端里实现这一层, 最小关注点就是:

- `CoralNPUSubtarget` 里有没有持有:
  - `InstrInfo`
  - `RegisterInfo`
  - `FrameLowering`
  - `TargetLowering`
- `CoralNPUTargetMachine::getSubtargetImpl` 能不能返回它

### `ToyTargetLowering`

负责 target-specific lowering。

哪怕你暂时只支持很小的程序, 它也得存在, 因为函数返回和调用这类事情天然依赖 target。

这是初学者最容易低估的类。很多人会想:

- “我不是还没做复杂指令吗, 为什么这么早就要 `TargetLowering`?”

因为 LLVM IR 里有很多语义不是 target-neutral 地直接变成机器指令的, 比如:

- 函数参数怎么进来
- 返回值怎么出去
- 某些整数操作要不要扩展
- 某个操作能不能直接用本机指令表示

这些都需要 target 来给出答案。

第二节里第一版 `TargetLowering` 不要求你马上把 call/return 都做好。  
但这个类需要先存在, 因为 LLVM 很快就会问:

- 这个 target 的基本 lowering 对象在哪里?

### `ToyFrameLowering`

负责 prologue / epilogue 和栈帧约定。

这里先有最小骨架, 后续再逐步补行为。

第一次看它时不要被“栈帧”三个字吓到。此时你只需要先记住:

- 函数一进入时, 谁来决定 `sp` 怎么减?
- 函数要返回时, 谁来决定 `sp` 怎么恢复?

答案通常就是 `FrameLowering`。

第二节里它最早的价值不一定是“功能完整”, 而是:

- 让 `Subtarget` 能把一个 frame lowering 对象交给 LLVM

## 为什么教程按这个顺序推进

教程不是凭感觉写的, 而是由下一条断言决定顺序:

- 没 `TargetMachine`
  就先补 `toy-2`
- 没 `MCRegisterInfo`
  就先补 `toy-3`
- 没 `TargetLowering`
  就先补 `toy-9`
- 没 `FrameLowering`
  就先补 `toy-10`

你以后自己做后端时也应该沿用这条策略。

## 这一阶段最该抓住的调用关系

### `TargetMachine -> Subtarget`

`TargetMachine` 要能通过 `getSubtargetImpl` 返回一个 `Subtarget`

### `Subtarget -> Lowering/Frame/Reg/Instr`

`Subtarget` 要能提供:

- `getTargetLowering`
- `getFrameLowering`
- `getRegisterInfo`
- `getInstrInfo`

### `AsmPrinter -> MCInstLower -> InstPrinter`

当前最重要的下游链条是:

- `MachineInstr`
- `AsmPrinter`
- `MCInst`
- `InstPrinter`
- asm

如果你现在还没到 `AsmPrinter`, 也完全正常。  
对你当前阶段更关键的其实是上面这两条:

- `TargetMachine -> Subtarget`
- `Subtarget -> Lowering/Frame/Reg/Instr`

因为这两条才是第二节真正的骨架核心。

## 第二节最推荐的实际推进顺序

如果你现在已经能跑出:

- `./build/bin/llc --version`

那么第二节最稳的推进顺序通常是:

1. 先把 `CoralNPUTargetMachine` 补成像样的最小骨架
2. 再实现 `CoralNPUSubtarget`
3. 再让 `Subtarget` 能返回最小 `TargetLowering`
4. 再让 `Subtarget` 能返回最小 `FrameLowering`
5. 最后把 `CoralNPUDAGToDAGISel` 接进 `createPassConfig`

这个顺序的好处是:

- 每一步都只多补一层能力
- 你更容易把下一个报错和缺失对象对应起来

## 第二节完成后的理想状态

第二节完成不等于:

- 已经能正确输出汇编

第二节完成更合理的判断标准是:

- `TargetMachine` 已经不像空壳
- `Subtarget` 已经存在并能返回目标能力对象
- isel pass 已经有地方挂进去
- LLVM 能继续往后走, 然后再暴露更具体的“缺哪些指令/哪些 lowering”问题

这条链你最好反复记。因为从这一节开始, 很多“我明明已经有指令了, 为什么还没出汇编”的困惑, 都是在这里解开的。

更直观地说:

- `DAGToDAGISel` 负责“选出机器指令”
- `AsmPrinter` 负责“把机器指令变成 MC 层指令”
- `InstPrinter` 负责“把 MC 层指令变成字符串”

每一层都少不了。

## 注意事项

- `InstPrinter` 处理的是 `MCInst`, 不是 `MachineInstr`
- `AsmPrinter` 必须实现的核心函数是 `emitInstruction`
- `TargetObjectFile` 虽然看起来“像以后再说”, 实际上很早就会被依赖
- `Subtarget` 不是可有可无的封装, 它是 pass 查询 target 能力的主要入口

## 自查问题

1. `ToyAsmPrinter` 和 `ToyInstPrinter` 的边界在哪里?
2. 为什么 `TargetLowering` 即使在后端很小的时候也必须存在?
3. 为什么 `Subtarget` 会成为 `getRegisterInfo()` / `getInstrInfo()` 的统一入口?
