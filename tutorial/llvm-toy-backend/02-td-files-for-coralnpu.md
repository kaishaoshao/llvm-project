# 02 TD Files For CoralNPU

## 学习目标

- 知道当前 `CoralNPU` 代码里已经有哪些 `.td` 文件
- 知道这些 `.td` 文件当前各自负责什么
- 知道当前代码和“推荐整理后的结构”有什么差距
- 知道下一步应该先补哪一个 `.td` 文件, 才能让 `TableGen` 和 `TargetMC` 继续往前走

## 这一章怎么读

这一章不会只讲 “理想上 LLVM target 应该有哪些 `.td` 文件”。  
它会同时区分:

1. 当前仓库代码已经是什么样
2. 当前结构哪里还不合理
3. 你下一步应该先改什么

这样你读完之后, 可以直接回到代码里对照着改。

## 默认视角

从这一章开始, 例子默认以:

- **32 位 CoralNPU**

作为主线来讲。

也就是说:

- 例子优先按 32 位 / `DefaultMode`
- 64 位只在和 32 位不同的地方单独说明

这样安排是因为你当前真实目标是:

- 先让一个最小的 `coralnpu32` 后端站起来

而不是一开始就把 32/64 两条线同时铺开。

## 当前代码里已经有哪些 `.td` 文件

当前目录:

- [CoralNPU.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPU.td)
- [CoralNPUSubtarget.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUSubtarget.td)
- [CoralNPURegisterInfo.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPURegisterInfo.td)
- [CoralNPUInstrFormats.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrFormats.td)
- [CoralNPUInstrInfo.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrInfo.td)
- [CoralNPUCallingConv.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUCallingConv.td)
- [CoralNPUSchedule.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUSchedule.td)

但当前真正已经开始起作用的, 主要只有前三个:

- `CoralNPU.td`
- `CoralNPUSubtarget.td`
- `CoralNPURegisterInfo.td`

`InstrFormats.td` 和 `InstrInfo.td` 目前还基本是占位状态。

## 当前代码状态和当前卡点

如果你现在见到的报错是:

- `Unable to create reg info`
- `No instructions defined!`
- `CoralNPUGenInstrInfo.inc file not found`
- `CoralNPU::RA` / `CoralNPU::GPRRegClassID` 未定义

那这些报错都和 `.td` 层直接相关。

当前代码里最关键的几个现实状态是:

1. [CoralNPU.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPU.td) 还没有真正 include `CoralNPUInstrInfo.td`
2. [CoralNPUInstrInfo.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrInfo.td) 里还没有任何真正的 instruction record
3. [CoralNPU/CMakeLists.txt](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CMakeLists.txt) 目前只生成了:
   - `CoralNPUGenRegisterInfo.inc`
   - `CoralNPUGenSubtargetInfo.inc`
4. `-gen-instr-info` 还没正式接好, 因为一旦打开, 当前代码会立刻报:
   - `No instructions defined!`

所以你现在不能把这一章理解成 “先把所有 `.td` 设计完”, 更应该理解成:

- 先把最小声明式骨架接起来
- 每次只多走一步

## 先回答一个最常见的问题

### `CoralNPU.td` 和 `CoralNPUSubtarget.td` 需要写吗

需要。

而且它们不是“讲概念用的文件”, 而是当前代码真的已经在依赖的文件。

### 为什么不是只写 `CoralNPURegisterInfo.td`

因为 `RegisterInfo.td` 里的:

- `ValueTypeByHwMode`
- `RegInfoByHwMode`

都依赖前面先定义好的 mode。

而 mode 一般来自:

- `CoralNPUSubtarget.td`

所以合理顺序是:

1. `CoralNPU.td`
2. `CoralNPUSubtarget.td`
3. `CoralNPURegisterInfo.td`

## 当前这三个文件各自的职责

### `CoralNPU.td`

它是 TableGen 总入口。

当前代码里它主要负责:

- include 其他 `.td`
- 定义 `def CoralNPUInstrInfo : InstrInfo;`
- 定义 `def CoralNPU : Target { let InstructionSet = CoralNPUInstrInfo; }`

当前文件:

```td
include "llvm/Target/Target.td"
include "CoralNPUSubtarget.td"
include "CoralNPURegisterInfo.td"
def CoralNPUInstrInfo : InstrInfo;
// include "CoralNPUInstrInfo.td"
def CoralNPU : Target {
  let InstructionSet = CoralNPUInstrInfo;
}
```

#### 这份当前写法哪里不够合理

最关键的问题是:

- `CoralNPUInstrInfo.td` 被注释掉了

所以当前 `CoralNPU.td` 还没有真正把指令定义接进来。  
这也是后面 `-gen-instr-info` 报 `No instructions defined!` 的直接原因之一。

#### 当前阶段推荐你把它整理成什么样

```td
include "llvm/Target/Target.td"

include "CoralNPUSubtarget.td"
include "CoralNPURegisterInfo.td"
include "CoralNPUInstrInfo.td"

def CoralNPUInstrInfo : InstrInfo;

def CoralNPU : Target {
  let InstructionSet = CoralNPUInstrInfo;
}
```

这里最重要的变化不是 `Target` 本身, 而是:

- 把 `CoralNPUInstrInfo.td` 接进总入口

### `CoralNPUSubtarget.td`

它负责定义:

- feature
- predicate
- `HwMode`
- 最小 `ProcessorModel`

当前代码是:

```td
def Feature32Bit :
  SubtargetFeature<"32bit", "Is32bit", "ture", "CoralNPU 32Bit support">;

def Feature64Bit :
  SubtargetFeature<"64bit", "Is32bit", "false", "CoralNPU 64Bit support">;

def Is32Bit : Predicate<"Subtarget->is32Bit()">;
def Is64Bit : Predicate<"!Subtarget->is32Bit()">;

defvar Coral32Bit = DefaultMode;
def Coral64Bit : HwMode<"+64bit", [Is64Bit]>;

def : ProcessorModel<"generic", NoSchedModel, []>;
def : ProcessorModel<"generic-coralnpu64", NoSchedModel, [Feature64Bit]>;
```

#### 这份当前写法哪里不够合理

这里至少有三点你读代码时要心里有数:

1. `"ture"` 是 typo, 应该是 `"true"`
2. 当前命名是:
   - `Feature32Bit`
   - `Feature64Bit`
   - `Coral32Bit`
   - `Coral64Bit`
   这能工作, 但没有教程里常用的 `RV32/RV64` 那么直观
3. `SubtargetFeature<..., "Is32bit", ...>` 这一行意味着:
   - 你后面的 `CoralNPUSubtarget` C++ 类里要有匹配的字段或访问方式

#### 当前阶段应该怎么理解这份文件

先不要急着一次重命名整个文件。  
你现在最应该理解的是:

- 这份文件已经承担了 “mode / feature / processor” 这层职责
- `CoralNPURegisterInfo.td` 已经在依赖这里的 `Coral64Bit`

所以当前它虽然不完美, 但不是一个可以随便删掉重来的空壳。

### `CoralNPURegisterInfo.td`

它负责定义:

- 物理寄存器
- 寄存器类
- `XLenVT`
- `RegInfoByHwMode`

当前代码是:

```td
let Namespace = "CoralNPU" in {
  class CoralNPUReg<bits<5> Enc, string n, list<string> alt = []>
    : Register<n> {
      let HWEncoding{4-0} = Enc;
      let AltNames = alt;
    }

  class CoralNPUGPRReg<bits<5> Enc, string n, list<string> alt = []>
    : CoralNPUReg<Enc, n, alt>;

  let Namespace = "CoralNPU" in {
    def ZERO : CoralNPUGPRReg<0, "x0",  ["zero"]>;
    def RA   : CoralNPUGPRReg<0, "x1",  ["ra"]>;
    def SP   : CoralNPUGPRReg<0, "x2",  ["sp"]>;
    def A0   : CoralNPUGPRReg<0, "x10", ["a0"]>;
    def A1   : CoralNPUGPRReg<0, "x11", ["a1"]>;
  }

  def XLenVT : ValueTypeByHwMode<[DefaultMode, Coral64Bit], [i32, i64]>;

  def GPR : RegisterClass<"CoralNPU", [XLenVT], 32, (add ZERO, RA, SP, A0, A1)> {
    let RegInfos = RegInfoByHwMode<
      [DefaultMode, Coral64Bit],
      [RegInfo<32,32,32>, RegInfo<64,64,64>]
    >;
  }
}
```

#### 这份当前写法哪里不够合理

当前最明显的问题是:

1. `RA`、`SP`、`A0`、`A1` 的 `HWEncoding` 还都写成了 `0`
2. 虽然生成了 `CoralNPUGenRegisterInfo.inc`, 但这些编码值后面迟早要改对
3. 这里用的是:
   - `DefaultMode`
   - `Coral64Bit`
   所以文档里的 32/64 位例子也应该优先围绕这两个名字来讲

## 当前阶段最小合理目标

对你现在的 `CoralNPU` 代码来说, 这一章最现实的目标不是 “把所有 `.td` 设计完”, 而是:

1. `CoralNPU.td` 真正 include `CoralNPUInstrInfo.td`
2. `CoralNPUInstrInfo.td` 里至少有一条真实指令
3. `CoralNPU/CMakeLists.txt` 里能够打开:
   - `tablegen(LLVM CoralNPUGenInstrInfo.inc -gen-instr-info)`
4. `MCTargetDesc/CoralNPUTargetDesc.h` 和 `.cpp` 能正确 include:
   - `GET_INSTRINFO_ENUM`
   - `GET_INSTRINFO_MC_DESC`

你可以把这 4 件事当成当前 `.td` 阶段最重要的任务。

## 为什么先支持一条 `ret` 指令

因为当前 `-gen-instr-info` 报的是:

- `No instructions defined!`

这说明问题不是:

- 指令太少

而是:

- **一条都没有**

所以当前最合理的第一条指令就是:

- `RET`

原因很简单:

1. 它最容易解释
2. 它不需要一开始就引入复杂算术或访存语义
3. 它足够让 `InstrInfo.td` 不再是空壳

## 一个最小 `ret` 目标应该涉及哪些文件

当前最小路径只需要你先整理三处:

1. [CoralNPU.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPU.td)
2. [CoralNPUInstrFormats.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrFormats.td)
3. [CoralNPUInstrInfo.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrInfo.td)

### `CoralNPUInstrFormats.td` 第一版最小示例

```td
class CoralNPUInst : Instruction {
  let Namespace = "CoralNPU";
  let Size = 4;
}
```

它当前只回答一件事:

- “CoralNPU 的最小指令模板长什么样”

第一版先不要急着把位段编码、操作数字段全塞进来。

### `CoralNPUInstrInfo.td` 第一版最小示例

```td
include "CoralNPUInstrFormats.td"

def RET : CoralNPUInst {
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "ret";
  let Pattern = [];
}
```

#### 为什么这条 `RET` 现在就够用

因为你当前最先要解决的问题不是:

- `ret` 如何从 LLVM IR 正确 lowering 下来

而是:

- 让 `-gen-instr-info` 至少看到一条真正的 instruction record

只要 `RET` 被 TableGen 认成一条指令, 你就已经能从:

- `No instructions defined!`

往后推进一步。

## CMake 和 `tablegen(...)` 要和 `.td` 同步

当前 [CoralNPU/CMakeLists.txt](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CMakeLists.txt) 里, `InstrInfo` 那行还是注释状态:

```cmake
# tablegen(LLVM CoralNPUGenInstrInfo.inc -gen-instr-info)
```

这和当前代码状态是匹配的, 因为:

- 一旦打开
- 如果 `CoralNPUInstrInfo.td` 里还没有指令
- 就会立刻报:
  - `No instructions defined!`

所以正确顺序不是先硬开 CMake, 而是:

1. 先在 `InstrInfo.td` 里加一条最小 `RET`
2. 再打开 `-gen-instr-info`
3. 再处理 `MCTargetDesc.h/.cpp` 里的 `GET_INSTRINFO_*`

## `tablegen` 动作名和输出文件名要分开理解

这里最容易混淆的是:

- `-gen-register-info`
- `CoralNPUGenRegisterInfo.inc`

前者是:

- `llvm-tblgen` 的生成动作名

后者是:

- 你让 CMake 输出的文件名

同理:

- `CoralNPUGenSubtargetInfo.inc` 对应的是 `-gen-subtarget`
- 不是 `-gen-subtarget-info`

原因不是语义上 “不需要 info”, 而是:

- LLVM 这个 emitter 的合法名字本来就叫 `subtarget`

所以这里不要自己猜动作名, 直接记住这几个常用对应关系:

- `-gen-register-info` -> `CoralNPUGenRegisterInfo.inc`
- `-gen-instr-info` -> `CoralNPUGenInstrInfo.inc`
- `-gen-subtarget` -> `CoralNPUGenSubtargetInfo.inc`

## 你现在最推荐的实际推进顺序

如果你现在就在改代码, 最稳的顺序是:

1. 取消 [CoralNPU.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPU.td) 里对 `CoralNPUInstrInfo.td` 的注释
2. 给 [CoralNPUInstrFormats.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrFormats.td) 加最小 `CoralNPUInst`
3. 给 [CoralNPUInstrInfo.td](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CoralNPUInstrInfo.td) 加最小 `RET`
4. 打开 [CoralNPU/CMakeLists.txt](/Users/kaishaoshao/Desktop/code/llvm-project_mips/llvm/lib/Target/CoralNPU/CMakeLists.txt) 里的:
   - `tablegen(LLVM CoralNPUGenInstrInfo.inc -gen-instr-info)`
5. 再回头修改 `MCTargetDesc/CoralNPUTargetDesc.h` 和 `.cpp`

## 这一章完成后的验收标准

这一章做完, 当前最实际的验收标准不是 “后端已经能生成 `ret`”, 而是:

1. `CoralNPU.td` 的 include 结构已经清楚
2. `Subtarget.td` 和 `RegisterInfo.td` 的职责你能分清
3. `CoralNPUInstrInfo.td` 不再是空壳
4. `-gen-instr-info` 不再报 `No instructions defined!`

做到这里, 你才算真正开始拥有一套能继续长大的 `.td` 骨架。
