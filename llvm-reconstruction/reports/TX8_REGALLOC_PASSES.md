# TX82 寄存器分配/调度相关 fork pass 复现报告（T1a）

- 范围：**影响寄存器分配/指令顺序**的 3 个 fork 组件 —— `TX8RegisterRename`（`vreg-rename`）、
  `TX8RegisterAlignment`（`vreg-align`）、`TX8SchedStrategy`（`Pre/PostRA` 调度策略 + `-tx8-lmul-sched`）。
- 结论：**G9 的根因是缺失的 `vreg-rename`，不是调度**。已实现并与厂商逐位对齐；
  `vreg-align` 在 musl 路径上对对象**无影响**（只作用于 RAISA `.rc` 路径）；
  `TX8SchedStrategy` 对 G9 语料**无影响**（未实现，见 §6）。
- 全部结论以**厂商工具链为 oracle**；命令/日志/差分产物在
  [`wt-regalloc/`](../../wt-regalloc/)（`logs/`、`tools/`、`out/`），补丁
  [`patches/0001-tx82-vreg-rename.patch`](../../wt-regalloc/patches/0001-tx82-vreg-rename.patch)。

---

## 1. G9：根因定位（一个开关即可判定）

`probe/avl100.c`、`-O2`、musl triple：

| 编译 | `vfadd`/`vse32` |
| --- | --- |
| 厂商默认 | `vfadd.vv v0, v8, v9` + `vse32.v v0` |
| 厂商 `-mllvm -riscv-vreg-rename=false` | `vfadd.vv v8, v8, v9` + `vse32.v v8` |
| 我们（改动前） | `vfadd.vv v8, v8, v9` + `vse32.v v8` |

决定性证据：**厂商 `-O2 -mllvm -riscv-vreg-rename=false` 的对象与"改动前的我们"逐节完全相同**
（`elfcmp.py` VERDICT: IDENTICAL，仅 `.comment` 版本串不同）。即 G9 的全部差值就是这个 pass。

其余消融（同一 `avl100 -O2`）**均不改变对象**：`-riscv-vreg-align=false`、`-tx8-lmul-sched=false`、
`-riscv-tx8-sched=false`、`-riscv-vr-conflict-detector=true`、`-riscv-vreg-rename-order-opt=true`。

## 2. `vreg-rename` 到底是什么（符号 + MIR 双重取证）

厂商 `TX8RegisterRename.cpp.o` 符号表（无 DWARF）给出算法骨架：

```
(t) VRegReNamer::initRegs()
(t) VRegReNamer::addPhysVRegAntiDeps(MachineInstr&, unsigned)
(t) VRegReNamer::breakAntiDeps(DenseMap<const TargetRegisterClass*, unsigned>&)
(t) std::map<MCRegister, VRegReNamer::RegisterReference>          // 参考表
(U) RegisterClassInfo::{runOnMachineFunction,compute}  TargetRegisterInfo::getAllocatableSet
(U) MachineBasicBlock::livein_begin  MachineFrameInfo::getPristineRegs
(U) MachineRegisterInfo::getCalleeSavedRegs  MCRegInfo::{getSubReg,getSubRegIndex,getCachedAliasesOf}
(U) TargetRegisterInfo::getMinimalPhysRegClass   RISCV::{VR,VRM2,VRM4,VRM8,*NoV0}RegClass
(U) llvm::RestoreAlignedCTInstruction(MachineFunction&, const RISCVInstrInfo*, const RISCVRegisterInfo*)
```

⇒ 它是上游 `lib/CodeGen/CriticalAntiDepBreaker.cpp` 的**独立化改写**（去掉 SUnit/关键路径，只留
"按块反向扫描 + RegRefs + 逐类游标"），并限定在向量寄存器类上。它不是 `TX8RegisterAlignment`
的附属，也不依赖 `TX8RegisterConflictDetector`（该 detector 的 `createConflictRangeDetector`
在本对象里没有被引用）。

### 2.1 厂商 MIR 证据（`-mllvm -print-before/-after=vreg-rename`）

`avl100 -O2`（before → after）：

```
-  renamable $v8 = nofpexcept PseudoVFADD_VV_M1_E32 undef renamable $v8(tied-def 0), killed renamable $v8, killed renamable $v9, ...
-  PseudoVSE32_V_M1 killed renamable $v8, killed renamable $x12, ...
+  $v0 = nofpexcept PseudoVFADD_VV_M1_E32 undef $v0, killed renamable $v8, killed renamable $v9, ...
+  PseudoVSE32_V_M1 $v0, killed renamable $x12, ...
```

要点：**只改寄存器、不改指令**；被改写的操作数会同时清掉 `renamable`（`MachineOperand::setReg`
的固有行为）与 `killed`（厂商额外动作，已复现）。`$vle32` 的 `undef $v8(tied-def 0)` 属于**定义**，
随定义一起改；而 `killed $v8`（读旧值）保持不变。

## 3. 用厂商 `llc -run-pass=vreg-rename` 做黑盒定则（20+ 组手写 MIR）

`llc -mtriple=riscv64-unknown-linux-musl -mcpu=tsm-x82 -run-pass=vreg-rename x.mir` 可独立执行该
pass，于是可以逐条钉死规则（脚本 `tools/mirprobe.py`、`tools/mutate.py`）：

1. **候选范围**：只有向量寄存器类（`VR/VRM2/VRM4/VRM8` 及其 `NoV0` 变体，后者对应带 mask 的伪指令）。
2. **触发条件**：某定义 `R` 在该 **MBB 内之前已有一次定义**（"寄存器被重定义"）。
   * 单条 `v8 = VLE32` + `v8 = VLE32` → 第二条被改名；块首首个定义永不被改名。
   * **跨块 live-in 不算**：`bb.1` 里对由 `bb.0` 定义、`bb.1` 使用的 `v8` 重定义 → **不改名**。
3. **保护条件**：若该定义产生的值"必须留在原寄存器"则跳过：
   * 该值 **live-out** 出本块（后继块 live-in 里出现）；
   * 该值有**不可改写的引用** —— 实测就是 `PseudoRET implicit killed $v8`（隐式操作数）。
     例：`A: v8=F(...); B: v8=F(...); VSE32 v8; PseudoRET implicit killed $v8` → 只有 `A` 被改名
     （`B` 的值是返回值）；把 RET 换成 `COPY` 后再 RET 用新寄存器，则 `A`、`B` 都改名。
4. **扫描方向**：**块内反向**（从最后一条往前）。`p_4ind`（4 条独立 `vfadd`）得到程序序
   `v3, v2, v1, v0`，即最后一条拿最低编号。
5. **候选顺序**：该寄存器类**分配顺序的反向**（`RegisterClassInfo::getOrder(RC)` 倒着走）。
   13 条连续 `v8 = VLE32` 的探针给出程序序 `v28,v29,v30,v31,v7,v6,v5,v4,v3,v2,v1,v0`，
   即处理序 `v0..v7,v31..v28` —— 正是 `VR` 分配序 `[V8..V31,V7..V0]` 的逆序。
6. **游标单调**：同一块内已跳过的寄存器不会回头再用（测例：后一条改名时 `v0` 活跃 → 拿 `v1`；
   再往前一条改名时 `v0` 已死 → 仍拿 `v2`），**但每个基本块重置**（两块各一个改名，结果都拿 `v0`）。
7. **别名联动**：改名 `R→R'` 时，活跃信息转移到 `R'`（含其子寄存器）。
   测例：先改 `v8m2→v0m2`，则后面（更靠前）的 `v12→` 会跳过 `v0`、`v1`（`V0M2` 的子寄存器），拿 `v2`；
   把两条指令对调（先改名 M1）则拿 `v0`、`v2m2`。

### 3.1 管线位置与开关

`-mllvm -debug-pass=Structure` 实测（musl）：

```
… -riscv-expand-pseudo -riscv-expand-atomic-pseudo -unpack-mi-bundles
   -vreg-rename -tx8-profiling-kernelfunc -lazy-machine-block-freq -machine-opt-remark-emitter -riscv-asm-printer
```

* 位置：`addPreEmitPass2()` 的 `UnpackMachineBundles` 之后（= 寄存器分配之后）。
* 使能：`getOptLevel() >= Default`（**-O2/-O3**；-O0/-O1 不在清单里）+ `-riscv-vreg-rename`（默认 true，
  置 false 会把 pass 从清单里**移除**）。
* `-riscv-vreg-rename-order-opt`（"跨 MBB 的改名顺序优化"）默认关，消融 0/21 对象变化。

## 4. 实现

* 新文件 `src/llvm/lib/Target/RISCV/TX8RegisterRename.cpp`（≈370 行）：`MachineFunctionPass`，
  注册名 `vreg-rename`，显示名 "TX8 Rename Registers"；按 §3 的 7 条规则实现，
  逐块反向扫描 + `Refs`/`KeepRegs` + 每类游标（`SmallVector<MCPhysReg>` 逆分配序 + `Next`）。
* 接线：`RISCVTargetMachine.cpp::addPreEmitPass2()`（`isTX8VRegRenameEnabled() &&
  getOptLevel() >= Default`），`RISCV.h` 声明，`CMakeLists.txt` 加入源文件。
* 与厂商的已知省略：未调用 `RestoreAlignedCTInstruction`（依赖未复现的 `vreg-align`；
  该 pass 在 musl 语料上是 no-op，见 §5.4）；未实现 `-riscv-vreg-rename-order-opt` 的跨块顺序启发。

## 5. 差分验证（全部为厂商 vs 我们，逐节 sha256/字节）

工具：`tools/difftest.py`（对象级，四路：厂商、厂商`=false`、我们、我们`=false`）、
`tools/mirdiff.py`（**同一份厂商 MIR 输入**分别喂双方 `llc -run-pass=vreg-rename`）、
`tools/elfcmp.py`、`tools/renamediff.py`。

### 5.1 验收点

| 用例 | 结果 |
| --- | --- |
| **avl100 `-O2`（G9）** | **VERDICT: IDENTICAL**（`.text` 26 B/7 insn/5×`0x7b`；仅 `.comment` 差异） |
| 14 个 `gemini-cmodel/kernels/*.c` `-O0` | **14/14 IDENTICAL** |
| `add`/`red` CModel（`-O0`、`-O1`） | 全部 **RAN**，`bitwise_vs_other=True`（300/300、1/1 字） |

### 5.2 语料对象级（256 个 `.c`：`wt-parity/src`、`gemini-cmodel/kernels`、`wt-isel/probe`、
`wt-regalloc/probe`、`docx/tests/llvm/**` 探针）

| 档 | 逐字节相同 | 不同（**全部** baseline 也不同） | 厂商 pass 生效的文件 | 其中我们相同 | **回归** |
| --- | --- | --- | --- | --- | --- |
| `-O2`（含向量化） | 37 | 8 | 14 | **10** | **0** |
| `-O2 -fno-vectorize -fno-slp-vectorize` | 41 | 13 | 12 | **10** | **0** |
| `-O1`（pass 不在清单） | 43 | 13 | 0 | — | **0** |

* "不同"的文件在 `-riscv-vreg-rename=false` 下**同样不同** ⇒ 与本次改动无关（ISel/编码/VL 优化等既有缺口）。
* 其余 200+ 文件双方之一编不过（厂商 `-O2` musl 的 `convertRVVALUToCTEncoding` fatal、我们树缺部分
  `XTsm_*`/`Kcore_Ncc_Read` 等指令定义），计为 skipped。

**残差定位（G9 之外、与本改动无关）**：`p_4ind.c` 这类"多条独立向量链"的 -O2 差异出在
**RA 的物理寄存器选择**（我们 `vle32.v v9/v10/v11` vs 厂商 `v10/v11/v12`），
`-print-before=vreg-rename` 的 **pre-rename MIR 已经不同**；已排除：
`-riscv-tx8-sched=false`、`-tx8-lmul-sched=false` 都不改变厂商产物（sha256 相同），
双方 LLVM IR 语义一致（仅值命名风格不同）。⇒ 属另一条线的既有缺口（疑与我们的
`XTsm_*` 指令定义缺少 `implicit $mem_alloc` / 调度类归属有关），不是 `vreg-rename`。

### 5.3 MIR 级（最严格的隔离测试）

对 256 个输入，用厂商 clang `-stop-before=vreg-rename` 取到**厂商 pass 的真实输入**，
清掉厂商专有寄存器（`$mem_alloc`、`$alignN`、`$xfrm`→`$frm`）后，分别喂双方 llc：

```
total 256 | IDENTICAL 28 | DIFFER 0 | skipped 228
（121 个厂商自己 -O2 编不过；106 个我们的树解析不了厂商 MIR 的指令/寄存器；1 个解析）
```

即**凡是双方都能处理的输入，输出 MIR 逐行相同**（含 avl100、p_chain2 的返回值保护、
p_t10 / p_4ind 的游标、tx82_add 的 `v8m2→v24m2…` 别名联动）。

### 5.4 `vreg-align` 消融（厂商 oracle）

`-mllvm -riscv-vreg-align=false` 对 6 个 musl 向量语料 × `-O1/-O2`：**对象全部 IDENTICAL**。
⇒ 在设备 kernel 路径（musl triple）上它是 **no-op**；厂商 `pass_effects` 也只在 RAISA triple 的
`.rc` 探针上观察到 `PseudoVSLH_VX_M2 → PseudoVSLH_ALIGNED_VX_M2` 类改写（本线不复现，见 §6）。

## 6. 仍未做 / 下一步

| 项 | 说明 | 估算 |
| --- | --- | --- |
| `TX8RegisterAlignment`（`vreg-align`） | 只在 RAISA `.rc` 路径生效；需要 `getCTAlignedPseudoInfo`/`getCTInversedAlignedPseudoInfo` 两张表 + `alignInstrVReg`，并补齐 `-riscv-vreg-align` 消融对比 | 1.5 会话 |
| `RestoreAlignedCTInstruction` | 被 `vreg-rename` 调用；`vreg-align` 复现后接入（当前 musl 语料无 ALIGNED 伪指令，可省） | 0.2 |
| `TX8SchedStrategy`（`Pre/PostRASchedStrategy` + `-tx8-lmul-sched`） | 只影响 `-O1+` 指令顺序；对 G9 语料 0 对象变化；实现前需先确认厂商 Pre-RA 版是否真的改变对象（本语料未观测到） | 1.0 |
| 106 个"我们树解析不了厂商 MIR"的输入 | 是 XTsm 指令定义缺口（`Kcore_Ncc_Read` 等），属 ISel 线；补完后可把 MIR 级差分覆盖率从 28/256 提到 ~130/256 | ISel 线 |
| 跨块改名顺序启发 `-riscv-vreg-rename-order-opt=true` | 默认关、0/21 对象变化；低优先 | 0.3 |
| **本线剩余** | | **≈1.5–2 会话**（主要是 `vreg-align` 的 RAISA 路径） |

> 注：`-O2` 向量化档在厂商侧仍有 `convertRVVALUToCTEncoding` fatal error、`simt2simd` SIGSEGV
> 等**独立阻塞**（见 `TX8_PASS_INVENTORY.md` §1.1），补齐 pass 并不能解掉它们。
