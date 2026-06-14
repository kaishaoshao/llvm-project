# 09 TD Files For CoralNPU

## 学习目标

- 知道 `CoralNPU` 后端通常应该有哪些 `.td` 文件
- 知道这些 `.td` 文件分别负责描述什么
- 知道这些文件应该怎么命名、怎么拆分、先写哪个后写哪个

## 为什么要单独讲 `.td` 文件

很多人第一次做 LLVM 后端时, 最容易有两个误区:

- 误区 1: 以为 `.td` 只是“配置文件”
- 误区 2: 以为先随便写几个 `.td`, 后面再整理

实际上 `.td` 文件不是普通配置。  
它们是 LLVM 后端里非常核心的“声明式描述层”, 会直接生成:

- `GenRegisterInfo.inc`
- `GenInstrInfo.inc`
- `GenSubtargetInfo.inc`
- `GenAsmWriter.inc`
- `GenDAGISel.inc`

所以:

- `.td` 文件的命名要稳定
- 拆分方式要清楚
- 每个文件的职责最好一开始就分明

## 对 `CoralNPU` 最推荐的一套 `.td` 文件

如果你现在是从零开始搭 `CoralNPU`, 最推荐先按下面这一套来组织。

### 第一层: target 总入口

- `CoralNPU.td`

作用:

- 声明 target 本身
- include 其他 `.td` 文件
- 定义整个 target 的总入口 record

这个文件通常像目录首页。  
它自己不应该塞太多细节, 更适合做:

- 汇总 include
- 总目标声明
- 少量全局模式/别名定义

## `CoralNPU.td` 里通常放什么

建议放:

- `include "llvm/Target/Target.td"`
- include 其他 `CoralNPU*.td`
- `def CoralNPU : Target { ... }`

不建议一开始就放太多:

- 寄存器定义
- 大量指令定义
- 复杂 pattern

因为这些更适合拆到独立文件。

### 第二层: subtarget / feature

- `CoralNPUSubtarget.td`

作用:

- 定义 CPU 名字
- 定义 feature
- 定义 `HwMode`
- 定义 32/64 位模式或更细的 feature 组合

如果你后面需要:

- `coralnpu32`
- 不同向量能力
- 不同张量扩展

这些都应该从这里长出来。

这个文件回答的是:

- “这个 target 有哪些处理器型号和 feature 开关?”

### 第三层: 寄存器体系

- `CoralNPURegisterInfo.td`

作用:

- 定义物理寄存器
- 定义寄存器别名
- 定义寄存器类
- 定义 `ValueTypeByHwMode`
- 定义 `RegInfoByHwMode`

这个文件回答的是:

- “这个 target 有哪些寄存器?”
- “哪些寄存器可以放在哪类操作数位置?”
- “32 位模式和 64 位模式下, 合法值类型和寄存器宽度是什么?”

这是第一批最重要的 `.td` 文件之一。

如果你后面要支持 CoralNPU 风格的:

- 标量寄存器
- 向量寄存器
- 可能的 accumulator 寄存器

通常也会从这里继续拆分出来。

### 第四层: 指令编码格式

- `CoralNPUInstrFormats.td`

作用:

- 定义一类指令的公共位域布局
- 定义基础指令类模板
- 把“很多条长得差不多的指令”的公共部分抽出来

这个文件回答的是:

- “一条指令的编码壳子长什么样?”

它一般不直接列出很多具体指令, 而是给后面的 `InstrInfo.td` 提供模板。

例如:

- R 型格式
- I 型格式
- 向量格式
- CoralNPU 自己的张量/块操作格式

### 第五层: 具体指令和 pattern

- `CoralNPUInstrInfo.td`

作用:

- 定义具体指令 record
- 绑定 opcode、操作数、输出输入约束
- 写 SelectionDAG pattern
- 定义 pseudo 指令

这个文件回答的是:

- “这个 target 具体有哪些指令?”
- “遇到某种 DAG, 能选成哪条机器指令?”

通常它会依赖:

- `CoralNPUInstrFormats.td`
- `CoralNPURegisterInfo.td`

如果你一开始只做最小闭环, 这个文件里只需要先放:

- `ret`
- `addi` 类最小整数指令
- `load/store`

不要一开始把 CoralNPU 所有想象中的张量指令都塞进去。

### 第六层: 调用约定

- `CoralNPUCallingConv.td`

作用:

- 声明参数/返回值在寄存器和栈上的分配规则

这个文件回答的是:

- “函数调用时, 参数怎么传?”
- “返回值怎么回?”

注意:

- 规则常写在 `.td`
- 真正搬运参数、组装调用序列的行为仍然在 `C++` 的 `LowerFormalArguments` / `LowerCall` / `LowerReturn`

### 第七层: 调度模型

- `CoralNPUSchedule.td`

作用:

- 描述调度资源
- 描述指令延迟/吞吐
- 声明处理器调度模型

如果你现在只是做最小 backend, 这个文件可以先很薄。  
但从命名和组织角度, 最好一开始就预留出来。

## 如果 CoralNPU 后面更复杂, 还可以继续拆哪些 `.td`

当 target 变大以后, 你很可能还会继续拆出:

- `CoralNPUInstrInfoV.td`
  - 向量指令
- `CoralNPUInstrInfoTensor.td`
  - 张量/矩阵相关指令
- `CoralNPUFeatures.td`
  - 如果 feature 已经很多, 可以从 `Subtarget.td` 独立出去
- `CoralNPUCombine.td`
  - 某些 target combine / pattern 扩展

但这是后话。

如果你当前目标还是“先让骨架站起来”, 不要一开始拆太细。

## 最推荐的命名规则

建议统一按这个模式:

- `CoralNPU.td`
- `CoralNPUSubtarget.td`
- `CoralNPURegisterInfo.td`
- `CoralNPUInstrFormats.td`
- `CoralNPUInstrInfo.td`
- `CoralNPUCallingConv.td`
- `CoralNPUSchedule.td`

核心规则只有两条:

1. 前缀统一
   - 全部都用 `CoralNPU`
2. 后缀表达职责
   - `Subtarget`
   - `RegisterInfo`
   - `InstrFormats`
   - `InstrInfo`
   - `CallingConv`
   - `Schedule`

不建议混成:

- `CoralRegs.td`
- `CoralInstr.td`
- `NPUFmt.td`

这种简称式命名, 因为后面一多就很难看出职责边界。

## 这些文件之间通常怎么 include

最常见的做法是:

- `CoralNPU.td` 作为总入口 include 其他文件

例如:

```td
include "llvm/Target/Target.td"

include "CoralNPUSubtarget.td"
include "CoralNPURegisterInfo.td"
include "CoralNPUSchedule.td"
include "CoralNPUInstrFormats.td"
include "CoralNPUInstrInfo.td"
include "CoralNPUCallingConv.td"
```

这个顺序也不是随便排的。

推荐顺序是:

1. `Subtarget`
2. `RegisterInfo`
3. `Schedule`
4. `InstrFormats`
5. `InstrInfo`
6. `CallingConv`

原因是:

- `RegisterInfo` 往往要依赖 mode / feature
- `InstrFormats` 往往要依赖寄存器类和类型
- `InstrInfo` 往往要依赖前面所有基础定义

## 这些 `.td` 文件的建议书写顺序

如果你自己动手写, 最推荐按这个顺序:

1. `CoralNPU.td`
2. `CoralNPUSubtarget.td`
3. `CoralNPURegisterInfo.td`
4. `CoralNPUInstrFormats.td`
5. `CoralNPUInstrInfo.td`
6. `CoralNPUCallingConv.td`
7. `CoralNPUSchedule.td`

这个顺序背后的逻辑是:

- 先有 target 总入口
- 再有 mode / feature
- 再有寄存器和合法值类型
- 再有指令模板
- 最后才有具体指令和调用约定

不要一上来就先写 `InstrInfo.td`, 因为它通常最依赖前面的定义。

## 每个文件最小应该写到什么程度

### `CoralNPU.td`

最小要求:

- target 总入口存在
- include 结构存在

### `CoralNPUSubtarget.td`

最小要求:

- 至少有一个默认 CPU
- 至少有一组最小 feature

### `CoralNPURegisterInfo.td`

最小要求:

- 至少有一组 GPR
- 至少有一个返回地址寄存器 `RA`
- 至少有一个 `RegisterClass`

### `CoralNPUInstrFormats.td`

最小要求:

- 至少有一类最小整数指令模板

### `CoralNPUInstrInfo.td`

最小要求:

- 至少能定义几条最小指令
- 最好能支持 `ret/addi/load/store` 这一类最小闭环

### `CoralNPUCallingConv.td`

最小要求:

- 先把最基本的整数参数规则占位出来

### `CoralNPUSchedule.td`

最小要求:

- 可以先只有 `NoSchedModel`

## 这一章最想帮你建立的直觉

这些 `.td` 文件不是“分得越细越高级”, 而是:

- 每个文件对应一类稳定职责
- 先把职责边界分清楚
- 后面再按复杂度继续细拆

如果你当前只是从 `CoralNPU` 空壳往前搭, 最稳的起点就是:

- `CoralNPU.td`
- `CoralNPUSubtarget.td`
- `CoralNPURegisterInfo.td`
- `CoralNPUInstrFormats.td`
- `CoralNPUInstrInfo.td`

先把这一套建起来, 再继续扩展。

## 你现在最适合先做什么

如果你已经卡在:

- `createCoralNPUMCRegisterInfo`
- `InitCoralNPUMCRegisterInfo`
- `GenRegisterInfo.inc`

那下一步最适合先补的 `.td` 文件就是:

- `CoralNPURegisterInfo.td`

因为它会直接决定:

- `MCRegisterInfo`
- `RegisterClass`
- 很多后续 `Gen*.inc`
  能不能生成出来
