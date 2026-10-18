# TX8InsertXFRM（`tx8-insert-xfrm`）+ `xfrm`/`mem_alloc` 两个物理寄存器

日期 2026-10-16 ｜ 树：`wt-xfrm/`（= `wt-isel/src` 快照 + 本补丁 + patch 0033 `vreg-rename`）
｜ 补丁：`wt-xfrm/patches/0034-tx82-insert-xfrm.patch`

## 0. 结论速览

1. 新增两个物理寄存器（`xfrm`、`mem_alloc`）与一个 RA 前 pass **`TX8InsertXFRM` /
   `tx8-insert-xfrm`**，位置 `riscv-vl-optimizer` → **此** → `riscv-insert-read-write-csr`
   （与厂商 `-mllvm -debug-pass=Structure` 完全一致），规则见 §1。
2. **复现器达成**：`wt-isel/probe/avl100.c`、`avl4.c` 在 `-O2` 下
   ①`-print-before=greedy` 的 MIR 与厂商逐条相同（含 `implicit $mem_alloc`、`implicit $xfrm`
   及其**操作数顺序**）；②目标文件逐字节相同（`.text` 26/26 B、24/24 B，7 insns、5×`0x7b`；
   仅 `.comment` 中编译器版本串不同）。
3. **两条必须写下来的更正**（推翻 `wt-isel/out/t1a/FINDINGS.md` 的因果结论 / 任务前提）：
   - `implicit $mem_alloc` 在厂商侧**不是** `tx8-insert-xfrm` 加的，而是 **`tx8-legalize-ct`**
     在 `finalize-isel` 之后立刻加的（证据见 §3.1）。本树没有 `tx8-legalize-ct`，
     故把该效应并入本 pass（位置更晚，但对 `-print-before=greedy` 的净效应相同，已逐字节验证）。
   - **`v8` → `v0` 不是本 pass 造成的。** 厂商自己的 `-print-after=greedy` 也是
     `renamable $v8 = ... PseudoVFADD_VV_M1_E32 ...`；v8→v0 发生在 **post-RA 的
     `vreg-rename`（TX8RegisterRename）**（逐 pass 二分，见 §3.2）。该 pass 源码是
     reconstruction 的 patch `0033-tx82-vreg-rename.patch`，**存在于**
     `riscv-llvm-musl-toolchain/third_party/llvm`，但 **不在** `wt-isel/src` 快照里；
     把它并入本树后才真正字节相同。⇒ 后端 `-O2` 向量码的最后一块拼图是 `vreg-rename`，
     `tx8-insert-xfrm` 是并列需要的另一半（它决定 RA 前 MIR，进而影响 RA/后续 pass 的选择）。

## 1. 推导出的规则（opcode → 隐式操作数）

规则全部来自**厂商自己 MIR 语料**（`-mllvm -print-before/-after=<pass>`），
并用厂商目标文件 `libLLVMRISCVCodeGen.a(TX8InsertXFRM.cpp.o)` 反汇编交叉验证。

| # | 命中条件（实测口径） | 动作 |
|---|---|---|
| R1 | **RVV 向量访存**：`mayLoad()\|\|mayStore()` 且有 SEW/VL 操作数且有 VR 类寄存器操作数。实测 16/16 命中，覆盖 `VLE16/32`、`VSE16/32`、`VLSE32`、`VSSE32`、`VLUXEI32`、`VSUXEI32`、`VLOXEI32`、`VSOXEI32`、`VLE32_MASK`、`VSE32_MASK`、`VLE32_TUM`、`VLE32_V_M1/M2/M4/M8`、`VSE32_V_M8`；非访存指令 0 命中 | 追加 `implicit $mem_alloc`（**插在显式操作数之后、隐式 `$vl`/`$vtype` 之前**，与厂商操作数顺序一致） |
| R2 | **舍入模式来自 `frm`**：`TSFlags` bit18 `HasRoundModeOp` 且非 bit19 `UsesVXRM`（= `RISCVII::getFRMOpNum(Desc) >= 0`） | `implicit $frm` → `implicit $xfrm`；若该指令没有 `$frm` use（静态舍入），则追加 `implicit $xfrm` |
| R3 | R2 且 `frm` 立即数 ∈ 0..6（**静态**舍入模式）且与上一条 pending 模式不同 | 指令**前**插 `ADDI <gpr>, x0, <mode>` + `vsetxfrm <gpr>`；该指令**后**插 `vsetxfrm $x0` 恢复动态模式；模式映射 **RDN(2)↔RUP(3) 互换**（TX8 硬件的 `xfrm` 编号与 RISC-V `frm` 不同） |
| R4 | 内联汇编 / bundle | 先把 pending 的静态模式用 `vsetxfrm $x0` 收尾，不进入其内部 |

- R1 的 opcode 清单无法在厂商编号里逐个确定（厂商指令编号与本树差 ~3000，见 §5），
  故用「访存 + RVV 寄存器 + SEW/VL」形状判定；厂商 `tx8-legalize-ct` 里
  `CreateReg(mem_alloc, isImp)` 只有一处（`movabs $0x502000000`），形态上同样是"统一追加"。
- R3 的 `ADDI`/`VSET_XFRM` 位置、操作数（`killed` 标记）与厂商 MIR **逐字符相同**
  （复现器 + `static_rm.c` 两个函数的 pre-greedy MIR 只有指令**名**与调度序不同，见 §5）。

## 2. 两个物理寄存器：定义与推断依据

厂商编号（由目标文件立即数反推，无需厂商 `.td`）：

| 事实 | 证据（厂商目标文件 / MIR） |
|---|---|
| `FRM = 4`、`XFRM = 13` | `TX8InsertXFRM.cpp.o`：`findRegisterUseOperandIdx(esi=4)` 后 `setReg(esi=0xd)`；`RISCVInstrInfo::verifyInstruction`（`llc` 反汇编 0x59a079）先查 `0xd` 再查 `0x4` |
| `MEM_ALLOC = 5` | `TX8LegalizeCTInstruction.cpp.o` 0xacd `movabs $0x502000000` = `CreateReg(5, isImp=true)` |
| `X0 = 102` | `TX8InsertXFRM.cpp.o` 0x279 `movabs $0x6604000000`（`ADDI $x0, imm`，MIR 里确实打印 `$x0`） |
| 名称为 `xfrm`/`mem_alloc` | 厂商 MIR：`implicit $xfrm`、`implicit $mem_alloc` |

本树定义（`RISCVRegisterInfo.td`，与 `FFLAGS/FRM/FCSR` 同一节）：

```tablegen
def XFRM      : RISCVReg<0, "xfrm">;
def MEM_ALLOC : RISCVReg<0, "mem_alloc">;
```

- **无 HWEncoding、不属于任何寄存器类** ⇒ 只能被"命名"，永远不会被分配；
  与 `FRM`/`FFLAGS` 完全同类（`RISCVReg<0,...>`）。
- **必须在 `RISCVRegisterInfo::getReservedRegs()` 里 `markSuperRegs`**：两者都只作为隐式
  use 出现、从不被 def；不 reserved 会让 liveness 认为"用了未定义的物理寄存器"。
- **编号落位由 TableGen `LessRecordRegister` 决定**（先按名字分段数、再按字母序）：
  `MEM_ALLOC` 落在 `FRM` 之后、`SF_VCIX_STATE` 之前；`XFRM` 落在 `VXSAT` 之后 ——
  与厂商 4/5/13 的相对次序**一致**。本树 `FRM=3` ⇒ `MEM_ALLOC=4`、`XFRM=12`，
  其余寄存器号整体 +1/+2（`NUM_TARGET_REGS` 605→607）。实测该平移**不改变任何输出字节**
  （厂商本身编号也与我们不同，见 §4）。
- 副作用：`RISCVGenAsmMatcher.inc` / `RISCVGenAsmWriter.inc` 新增这两个寄存器名 —— 厂商同样如此。

## 3. 两条并列必需的附带补丁

### 3.1 `tx8-insert-xfrm` 之外：`mem_alloc` 的真实来源 + CSR pass 关闭

- 厂商 `-print-after=finalize-isel`：**没有** `implicit $mem_alloc`；
  `-print-after=tx8-legalize-ct`：**已有**；`-print-before=tx8-insert-xfrm`：已有。
  ⇒ 厂商由 `tx8-legalize-ct`（`finalize-isel` → 此 → `riscv-vector-peephole`）加入。
  本树无此 pass，故并入 `tx8-insert-xfrm`（对 `greedy` 前的 MIR 净效应相同）。
- `RISCVInsertReadWriteCSR::runOnMachineFunction` 增加
  `if (ST.hasVendorXTsm82s()) return false;`。
  厂商 `llc` 反汇编该函数开头：`subtarget+0x1d4`（`hasVInstructions`）必须非 0，
  `subtarget+0x21b`（`hasVendorXTsm82s`）非 0 则**提前返回 false**。
  **没有这条**，R2 把 `$frm` 改名后该 pass 会给每条 FP 指令**再加**一个 `implicit $frm`
  （`emitWriteRoundingModeOpt` 里 `MI.readsRegister(FRM)` 的提前返回不再成立），
  pre-greedy MIR 就不等于厂商、RA 输入也不同。

### 3.2 `v8`/`v0` 的真正来源：post-RA `vreg-rename`

逐 pass 打印厂商 `-print-after=<pass>`，跟踪 `PseudoVFADD` 的目的寄存器：

```
riscv-insert-vsetvli / riscv-dead-defs / riscv-copyelim / postra-machine-sink /
prologepilog / postrapseudos / riscv-post-ra-expand-pseudo / riscv-load-store-opt /
postmisched / riscv-make-compressible / riscv-move-merge / riscv-push-pop-opt /
riscv-expand-pseudo / unpack-mi-bundles        -> $v8
vreg-rename                                    -> $v0   ← 唯一变化点
```

即：**RA 两侧都选 `v8`**，`v0` 是 post-RA 的 `vreg-rename`（TX8 向量寄存器反相关重命名）
产生的。因此：
- 只补 `tx8-insert-xfrm` 时，对象仍差 `v8`/`v0`（本 pass 只把 RA 前的 MIR 补成与厂商一致）；
- 把 `0033-tx82-vreg-rename.patch`（`TX8RegisterRename.cpp` + 3 处 wiring，源码在
  `riscv-llvm-musl-toolchain/third_party/llvm`，`wt-isel/src` 快照里缺失）并入后才字节相同。
  本树按 0033 原样并入（`INITIALIZE_PASS(..., "vreg-rename", "TX8 Rename Registers", ...)`，
  门控 `-riscv-vreg-rename`（默认开）且仅 `-O2` 及以上）。

## 4. 差分结果（全部实测）

命令：`python3 wt-xfrm/tools/diffcheck.py --corpus O2V --mir`、
`python3 wt-xfrm/tools/bytediff.py --opt=-O0`、`python3 wt-xfrm/tools/vsbase.py "-O1"`。

### 4.1 `-O2` 向量语料：对象 + `-print-before=greedy` MIR

| 探针 | 内容 | 对象 | pre-greedy MIR |
|---|---|---|---|
| `probe/avl100.c` | 2×`vle32.v`+`vfadd.vv`+`vse32.v`，AVL=100 | **逐字节相同**（`.text` 26/26 B，7 insns，5×`0x7b`） | **相同**（normalize bb 名/`%ir.*` 后 0 差异） |
| `probe/avl4.c` | 同上，AVL=4 | **逐字节相同**（24/24 B） | **相同** |
| `corpus/vm.c` | 16 类访存 intrinsic：unit/strided/indexed(ux/ox)/masked(`_m`)/`_tum`/M1·M2·M4·M8，f16/f32/i32 | **逐字节相同** | **相同** |
| `corpus/vec2.c` | 10 个 FP/整型向量 kernel（vfadd/vfmul/vfsub/vfmax/链式 vfmul+vfadd/i32/i16/f16 add/m2/m4） | **逐字节相同**（264/264 B，71 insns，51×`0x7b`） | **相同** |
| `corpus/v1.c` | 自动向量化（规约/乘法） | ✗ 双方均崩（厂商自身 `vfredosum.vs` 缺陷） | — |
| `corpus/v2.c`、`probe/zsplat.c` | 常量 AVL 自动向量化 / 零 splat | ✗ 本树崩，**基线同样崩**（mask 类 0x7b 空槽，预存缺口） | — |
| `out/static_rm.c` | 静态舍入模式（RNE/RUP/RDN） | 大小/指令数/`0x7b` 相同（80/80 B、22 insns、16×`0x7b`），仅 `li` 的调度位置不同（§5） | 仅指令名（`VSET_XFRM` vs `XTsm_VSETXFRM`）与调度序不同 |

### 4.2 回归（设备内核 `gemini-cmodel/kernels/tx82_*.c`，14 个 = 历史口径）

| 口径 | 结果 |
|---|---|
| `-O0` vs **厂商** | **14/14 逐字节相同**（138/138、1780/1780、772/772、320/320、1376/1376、962/962、630/630、138/138、446/446、732/732、2130/2130、822/822、1070/1070、0/0） |
| `-O0` vs **旧编译器**（`wt-isel/build`） | **14/14 未变** |
| `-O1` vs 旧编译器 | **14/14 未变**（vs 厂商 5/14，属预存差异，与本补丁无关：旧编译器同样 5/14） |
| `-O2 -fno-vectorize` vs 旧编译器 | **13/14 未变**；`tx82_flash_attn` 双方都失败（预存，`wt-cmodelrun` 矩阵里厂商成功、我们失败，与本补丁无关） |

### 4.3 CModel 执行（厂商 `libraisa_sim`，`wt-cmodelrun` 主机驱动）

`add`、`red` 两个 case，`-O0` / `-O1`，厂商模块 vs 本树模块 ⇒ `out.bin` **逐位相同**
（`add` out_sum=`993cb7e5`、`red` out_sum=`0033e461`，四种组合各一次）。

## 5. 遗留

1. **静态舍入的调度序**（`static_rm.c`）：指令/编码/数量完全一致，但厂商把 `li` 提到
   `vle32.v` 之前、我们在其后（pre-greedy MIR 里已如此）。根因：厂商插的是**自己的
   pseudo `VSET_XFRM`**（MIR 名即 `VSET_XFRM`，`RISCVSchedTX82.td` 里**没有**它的 `InstRW`
   ⇒ 走默认调度类），我们复用了可编码的 `XTsm_VSETXFRM`（`RISCVSchedTX82.td:8455` 有
   `InstRW<[TX82Ov259_W0]>`）。修法：新增同名 `VSET_XFRM` 记录（沿用 `XTsm_VSETXFRM` 编码、
   **不加** `InstRW`）并在 pass 里改用它。musl `-O2` 语料不可达（默认 DYN 舍入）。
2. **R3 的 ×8 特例未实现**：厂商对 `getRVVMCOpcode(op) ∈ [0x4218,0x421B]` 的 4 个
   RVV opcode 把模式 `×8`；这 4 个是**厂商编号**（本树指令编号整体不同，§0-更正），
   无法从归档资产（.td 缺失、只有 .o）判定是哪 4 条。
3. **整寄存器 / 段访存未实测**：`VLR*/VSR*` 在前 RA 点不会被生成；段访存 intrinsic
   （`vlseg2e32_v_f32m1x2`）被厂商 CT 前端拒绝（`intrinsic is not supported by CT`）。
4. **预存缺口（与本补丁无关，基线同样失败）**：mask 类 0x7b 空槽
   （`zsplat.c`/`v2.c`：`funct6=0b010111 funct3=0b011`、`funct6=0b011101 funct3=0b101` =
   `vmerge.vvm` 等）、`v1.c`（厂商自身 `-O2` 崩溃）、`tx82_flash_attn -O2 -fno-vectorize`。
5. `tx8-legalize-ct` / `riscv-merge-base-offset` 仍不在本树：前者被本 pass 覆盖了
   `mem_alloc` 效应，其余效应（0x7b 重编码）由 patch 0030 在 ISel 层实现。

## 6. 复现

```bash
# 复现器：MIR + 对象
cd wt-xfrm && python3 tools/diffcheck.py --corpus O2V --mir
# 14 内核 -O0（对厂商）/ 各档（对旧编译器）
python3 tools/bytediff.py --opt=-O0 --flavour obj --force
python3 tools/vsbase.py "-O1"
python3 tools/vsbase.py "-O2 -fno-vectorize"
# CModel add/red
bash tools/build_case_xfrm.sh ours $PWD/build/bin/clang-21 add O1 -O1   # 再跑 wt-cmodelrun 主机驱动
```

改动文件（补丁 `0034-tx82-insert-xfrm.patch`）：
`RISCVRegisterInfo.td`、`RISCVRegisterInfo.cpp`、`RISCVInstrInfo.cpp`、
`RISCVInsertReadWriteCSR.cpp`、`RISCV.h`、`RISCVTargetMachine.cpp`、`CMakeLists.txt`、
新增 `TX8InsertXFRM.cpp`（另含 0033 `TX8RegisterRename.cpp` + wiring，单独列出）。
