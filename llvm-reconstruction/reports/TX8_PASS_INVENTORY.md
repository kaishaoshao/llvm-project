# TX82 厂商 `TX8*` 后端 pass 清单与真实效果（以厂商工具链为 oracle）

- 生成方式：全部结论均来自**厂商二进制实测**（`llvm-toolchain/bin/{clang-21,opt,llvm-mca}` +
  `lib/libLLVMRISCV*.a`），命令、IR/MIR dump、diff、消融结果全部落盘在
  [`wt-passes/`](../../wt-passes/)。
- 机器可读汇总：[`wt-passes/pass_effects.tsv`](../../wt-passes/pass_effects.tsv)（18 pass × 20 字段）。
- 证据索引与复现命令：[`wt-passes/README.md`](../../wt-passes/README.md)。
- 厂商调用一律 `timeout` + `ulimit -f`（见 `wt-passes/setup/vrun.sh`）；未修改
  `gemini-cmodel/`、`llvm-toolchain/`、`riscv-llvm-musl-toolchain/`、`wt-forkbuild/`。

---

## 0. 先纠正一个前提：21 个 `.o` ≠ 21 个 pass

`ar t libLLVMRISCVCodeGen.a | grep TX8` = 21 个成员（`.o`），但 `nm` + `opt -help` +
`-debug-pass=Structure` 三方交叉后，实际是 **18 个注册 pass + 3 个非 pass 支撑 TU**：

| 类别 | 数量 | 成员 |
| --- | --- | --- |
| 注册 pass | **18** | 见 §2 主表第 1–18 行 |
| 非 pass 支撑 TU | **3** | `TX8MemoryUtils.cpp.o`（`llvm::RPU::*` 工具库）、`TX8RegisterConflictDetector.cpp.o`（`RISCVRegisterInfo::createConflictRangeDetector` 虚函数覆盖，无 pass 注册）、`TX8SchedStrategy.cpp.o`（`TX8PreRASchedStrategy`/`TX8PostRASchedStrategy` 调度策略 + `tx8-lmul-sched` 开关） |

决定性证据：`nm -A libLLVMRISCVCodeGen.a | grep createTX8` 只有
**`RISCVTargetMachine.cpp.o`** 一个引用方（[refs_codegen.txt](../../wt-passes/evidence/nm/refs_codegen.txt)）。
`TX8RegisterConflictDetector` 与 `TX8MemoryUtils` 没有任何 `create*Pass` 符号。
另外 `RISCVTargetMachine.cpp.o` 对 `llvm::TX8LegalizeCTInstructionID` /
`llvm::TX8RegisterAlignmentPassID` 有 `U` 引用（走 `PassID` 动态创建），
这解释了为什么这两个 pass 在 pipeline 里但 `nm` 看不到 `create*` 调用。

> 因此下文主表 **21 行**（对应 21 个 `.o`），但"要不要复现"的判断按 **18 个 pass** 计。

---

## 1. 三条编译路径与 pass 可达性（最重要的一张表）

用 `-mllvm -debug-pass=Structure` 在 28 种组合下抓 `Pass Arguments:` 全量清单
（[matrix.tsv](../../wt-passes/logs/pipeline/matrix.tsv)、原始 dump 在
[logs/matrix/](../../wt-passes/logs/matrix/)）：

| 路径 | 调用 | 进入 pipeline 的 fork pass |
| --- | --- | --- |
| **A. 设备 kernel 实际配方** | `--target=riscv64-unknown-linux-musl -mcpu=tsm-x82`（`gemini-cmodel/kernels/compile_kernels.sh`） | **5 个**：`tx8-asan, tx8-legalize-ct, tx8-insert-xfrm, vreg-align, tx8-profiling-kernelfunc`（-O2+ 再 +`vreg-rename`） |
| **B. RAISA 设备路径** | `--target=riscv64-tsingmicro-raisa`（等价 `-x raisa --raisa-device-only -noraisainc`，见 `-###`） | **14 个**：`rpu-always-inline, rpu-remove-inline-device-functions, promote-alloca, config-instr-opt, tx8-asan, rpu-unify-divergent-exit-nodes, tx8-annotate-control-flow, simt2simd, rpu-lower-shared, tx8-legalize-ct, tx8-insert-xfrm, vreg-align, legalize-exec, tx8-profiling-kernelfunc`（-O2+ 再 +`vreg-rename`） |
| **C. x86 宿主** | `--target=x86_64-unknown-linux-gnu` | 0 个 |

- **门控是 triple，不是 flag**：`-tx8-asan`、`-frpu-rdc`、`--raisa-noopt-device-debug`、
  `-mllvm -riscv-tx8-sched`、`-mllvm -tx8-lmul-sched`、`-mllvm -tx8-vsetbarrier`
  **都不改变 pass 清单**（28/28 组合实测）。
- **只有两个 `-mllvm` 真的控制"插入"**：`-riscv-vreg-align=false` 与
  `-riscv-vreg-rename=false` 会把对应 pass 从清单里移除 → 默认值 **true**，
  这就是它们可做消融的原因。
- **`vreg-rename` 只在 -O2/-O3 进入**（-O0/-O1 用 `regallocfast` 路径）；
  `vreg-align` 在 O0 就在，位置在 `twoaddressinstruction` 之后、regalloc 之前。
- **`legalize-exec` 只在 RAISA triple 出现**（musl triple 无）。
- **`-O1` 与 `-O0` 的 fork pass 集合相同**（host 与 raisa 都是），-O2/-O3 才加入
  `vreg-rename` / `riscv-move-merge` / `riscv-push-pop-opt`。

### 1.1 两个硬性阻塞（决定了我们只能用 -O0 取证）

| 配方 | 现象 | 证据 |
| --- | --- | --- |
| RAISA triple + `-O2` + 任何真实 kernel | **SIGSEGV in `TX8 Transform SIMT To SIMD`** | `logs/pipeline/raisa_O2.raw.txt`, `logs/matrix/raisa_O2.raw.txt` |
| musl triple + `-O2` | **`convertRVVALUToCTEncoding` fatal error**（`TX8MCInfo.cpp`，asm printer 阶段） | `logs/pipeline/legacy_O2.txt` |
| RAISA triple + `gemini-cmodel/kernels/*.c`（任何 -O） | **`Cannot select: intrinsic %llvm.riscv.vid`** | `logs/cc_*.err`、`corpus/*` |

⇒ ①设备 kernel 语料**只能**走路径 A 且 **-O0**；②RAISA 路径只能用 `.rc` 探针语料
（`wt-passes/corpus/kernel{,_shared,_spaces,_simt}.rc`，取自/仿 `docx/tests/llvm/tx8_blackbox/`）。
**`vreg-rename` 因此在真实 kernel 配方下永远不会执行。**

---

## 2. 主表：21 个 `TX8*.cpp.o` 的身份与真实效果

列说明：**效应列**基于 (a) `-mllvm -print-before/-after=<pass>` 的 IR/MIR 逐字节 diff
（`wt-passes/evidence/effects/*/`，共 809 条实测行 [`effects.tsv`](../../wt-passes/evidence/effects/effects.tsv)）
与 (b) 目标文件级消融（[`ablation/sha.tsv`](../../wt-passes/ablation/sha.tsv)、
[`ablation/sha2.tsv`](../../wt-passes/ablation/sha2.tsv)，21 输入 × 6 开关的 sha256 比对）。
规模：S≈0.5 会话 / M≈1.5 / L≈4。

| # | 文件 / 类 | 注册名(arg) / 显示名 | 使能方式（默认 / 门控） | pipeline 邻居 | **效应（路径A kernel / 路径B RAISA / 对象消融）** | 规模 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `TX8AddressSanitizer.cpp` / `TX8AddressSanitizerLegacy`(Module) | `tx8-asan` / `tx8-address-sanitizer` | 常插；驱动 `-tx8-asan` 决定是否动作，**默认关** | A: `expand-fp`→此→`atomic-expand`；B: `config-instr-opt`→此→`atomic-expand` | A: MIR **NO-EFFECT 16/16**；加 `-tx8-asan` 后 MIR diff **670 行**、前端 IR diff 27 行、**18/21 目标文件变化** | L |
| 2 | `TX8LegalizeCTInstruction.cpp` / `TX8LegalizeCTInstruction`(MF) | `tx8-legalize-ct` / `TX8 Legalize CT(RVV) Instruction` | 常插，无开关 | `finalize-isel`→此→`localstackalloc` | A: **NO-EFFECT 15/15**（-O0 几乎无 RVV 可换码，见 §3）；B: MIR CHANGED 6/6（tiny+scalar+gemm/gemv/rope/swiglu，diff 40–587 行）；RC 探针 CHANGED 4/4 | M |
| 3 | `TX8InsertXFRM.cpp` / `TX8InsertXFRM`(MF) | `tx8-insert-xfrm` / `TX8 Insert VSETXFRM` | 常插，无开关 | `riscv-prera-expand-pseudo`→此→`riscv-insert-read-write-csr` | A: **NO-EFFECT 15/15**；B: CHANGED 6/6（diff 2–115 行）；RC: CHANGED 1/4 | S |
| 4 | `TX8RegisterAlignment.cpp` / `TX8RegisterAlignment`(MF) | `vreg-align` / `TX8 Registers Alignment` | **`-riscv-vreg-align`（默认 true，控插入）** | `twoaddressinstruction`→此→`regallocfast`/`machine-scheduler` | A: **NO-EFFECT 15/15**；B: CHANGED 6/6（diff 3–211 行，观测到 `PseudoVSLH_VX_M2`→`PseudoVSLH_ALIGNED_VX_M2`+COPY）；RC: CHANGED 4/4；消融 `=false` **5/21 目标文件变化（4/4 RAISA 探针 + RAISA O2）** | M |
| 5 | `TX8LegalizeEXEC.cpp` / `TX8LegalizeEXEC`(MF) | `legalize-exec` / `TX8 Legalize EXEC` | 常插；**仅 RAISA triple** | `regallocfast`/`greedy`→此→`removeredundantdebugvalues` | A: **不在 pipeline**；B: CHANGED 6/6（diff 86–1337 行）；RC: CHANGED 4/4 | M |
| 6 | `TX8ProfilingKernelFunc.cpp` / `TX8ProfilingKernelFunc`(MF) | `tx8-profiling-kernelfunc` / `TX8 Profiling Kernel Function` | pass 常插；**`-tx8-vsetbarrier` 控动作，默认关** | `unpack-mi-bundles`→此→`riscv-asm-printer` | A: **NO-EFFECT 15/15**；B: CHANGED 4/16（gemm/gemv/rope/swiglu）；加 `-tx8-vsetbarrier` **4/21 对象变化（全部 RAISA 探针）** | S |
| 7 | `TX8RegisterRename.cpp` / `TX8RegisterRename`(MF) | `vreg-rename` / `TX8 Rename Registers` | **`-riscv-vreg-rename`（默认 true，控插入）；仅 -O2/-O3** | `unpack-mi-bundles`→此→`tx8-profiling-kernelfunc` | A(-O0): 不在 pipeline；RAISA -O2 RC: CHANGED 3/4；消融 `=false` **1/21（RAISA O2）**；另 `-riscv-vreg-rename-order-opt` **0/21** | L |
| 8 | `TX8TransformSIMTToSIMD.cpp` / `TX8TransformSIMTToSIMDLegacy`(Module) | `simt2simd` / `TX8 Transform SIMT To SIMD` | 常插；**仅 RAISA** | `tx8-annotate-control-flow`→此→`domtree` | B: 强 CHANGED（tiny 110→218 行）；RC: CHANGED 4/4；**-O2 大输入 SIGSEGV** | L |
| 9 | `TX8AnnotateControlFlow.cpp` / `TX8AnnotateControlFlowLegacy`(Fn) | `tx8-annotate-control-flow` / `Annotate TX8 Control Flow` | 常插；仅 RAISA | `uniformity`→此→`simt2simd` | B: CHANGED 15/16；RC: CHANGED 4/4 | M |
| 10 | `TX8UnifyDivergentExitNodes.cpp` / `TX8UnifyDivergentExitNodes`(Fn) | `rpu-unify-divergent-exit-nodes` / `TX8 Unify divergent function exit nodes` | 常插；仅 RAISA | `uniformity`→此→`domtree` | B: CHANGED 13/16；RC: **NO-EFFECT 4/4**（探针无多返回点） | S |
| 11 | `TX8LowerSharedPass.cpp` / `TX8LowerSharedLegacy`(Module) | `rpu-lower-shared` / `TX8 Lower Shared variables` | 常插；仅 RAISA | `early-cse`→此→`callbrprepare` | B: CHANGED 13/16；RC: CHANGED 1/4（仅 `__shared__` 探针） | M |
| 12 | `TX8OptimizeConfigInstrs.cpp` / `TX8OptimizeConfigInstrs`(Fn) | `config-instr-opt` / `TX8 Optimize Configuration Instructions` | 常插；仅 RAISA | `loops`→此→`tx8-asan` | B: CHANGED 13/16；RC: **NO-EFFECT 4/4**（探针循环体内无 NE config） | M |
| 13 | `TX8PromoteAlloca.cpp` / `TX8PromoteAlloca`(Fn) | `promote-alloca` / `TX8 Promote Alloca` | 常插；仅 RAISA | `cycles`→此→`loops` | B: CHANGED 13/16；RC: NO-EFFECT 4/4 | M |
| 14 | `TX8AlwaysInline.cpp` / `TX8AlwaysInline`(Module) | `rpu-always-inline` / `RPU Inline All Functions` | 常插；仅 RAISA；`-rpu-stress-function-calls` 反向 | `expand-fp`→此→`always-inline` | B: CHANGED 13/16；RC: NO-EFFECT 4/4（无被调函数）；`-rpu-stress-function-calls` **0/21** | S |
| 15 | `TX8RemoveInlineDeviceFunctions.cpp` / `TX8RemoveInlineDeviceFunctions`(Module) | `rpu-remove-inline-device-functions` / `TX8 Remove Inline Device Functions` | 常插；仅 RAISA | `always-inline`→此→`infer-address-spaces` | B: CHANGED 13/16；RC: NO-EFFECT 4/4 | S |
| 16 | `TX8BankAwareShared.cpp` / `TX8BankAwareSharedLegacy`(Module) | `bank-aware-shared` / `TX8 Bank-Aware Shared Memory Allocation` | **不可达**（28/28 组合不在 pipeline） | — | **0/809 行有任何 dump** | **L（不做）** |
| 17 | `TX8AnnotateUniformValues.cpp` / `TX8AnnotateUniformValuesLegacy`(Fn) | `rpu-annotate-uniform` / `Add TX8 uniform metadata` | **不可达**，且**唯一一个 `create*` 无人引用的 pass** | — | **0/809** | **S（不做）** |
| 18 | `TX8RecordingNPUInstrPC.cpp` / `TX8RecordingNPUInstrPC`(MF) | `tx8-record-npu-pc` / `TX8 Recording NPU PC` | **不可达**（虽被 `RISCVTargetMachine.cpp.o` 引用但从未实例化） | — | **0/809**；`-tx8-num-npu-instrs` 为需要取值的选项，触发 rc=1 | M（缓） |
| 19 | `TX8MemoryUtils.cpp` | — **非 pass** | `llvm::RPU::{getL0Align,getL1Align,isKernelFunction,getTransitiveUsesOfShared,removeLocalVarsFromUsedLists,eliminateConstantExprUsesOfShared,...}` | 被 #11/#16 复用 | 无 pass 语义，随 #11 一起做 | S |
| 20 | `TX8RegisterConflictDetector.cpp` | — **非 pass**；覆盖 `RISCVRegisterInfo::createConflictRangeDetector(MF&,LiveIntervals&,MachineLoopInfo&,bool)` | `-riscv-vr-conflict-detector`（默认关；**0/21 对象变化**） | 被 #7 消费 | 单独实测：置位后 **0/21** 目标文件变化 | M |
| 21 | `TX8SchedStrategy.cpp` | — **非 pass**；`TX8PreRASchedStrategy`/`TX8PostRASchedStrategy` | `-riscv-tx8-sched` 选策略、`-tx8-lmul-sched` 开 LMUL 最小切换；**不改 pass 清单** | `machine-scheduler`/`postmisched` 位置 | `-riscv-tx8-sched=false` **0/21**、`-tx8-lmul-sched=false` **0/21**（本语料未观察到对象差异；仅影响 -O1+ 指令顺序） | M（模型见 §4） |

**没有一个是完全 no-op 的死 pass**（在它自己的门控语料上都能观测到 IR/MIR 变化）；
但**在真实 kernel 配方（路径 A + -O0）下，5 个可达 pass 全部是 MIR 逐字节 no-op**。

---

## 3. 关键量化：真实 kernel 配方下 fork pass 层的净贡献 = 0

- 路径 A、`-O0`、14 个 kernel + `tiny.c` + `scalar.c`：5 个可达 pass 的
  before/after MIR **完全一致**（15/15，`tx82_version.c` 因**没有任何函数**而 0 marker）。
- 独立佐证：用重建工具链 `wt-forkbuild/build/bin/clang-21` 编 `tx82_version.c`，
  与厂商目标文件逐节比对 → **`.text` / `.rodata` / `.riscv.attributes` 完全相同**，
  只有 `.comment`（版本串）不同（1280 vs 1296 字节）。说明差分基线是干净的。
- 真实 kernel 的设计意图在 **MC 编码层**而非 pass 层：`-O0` 目标文件里每 kernel 有
  3–10 条 `0x7B` 指令（`tx82_gemm` 6、`tx82_flash_attn` 10…），这些来自 ISel + `TX8MCInfo`
  的 RVV→CT 换码，**不经过任何 TX8 pass**。
- 因此：**`tx8-legalize-ct` / `tx8-insert-xfrm` / `vreg-align` / `tx8-profiling-kernelfunc`
  在 gemini kernel 语料上不改变目标文件**；`tx8-asan` 只有显式 `-tx8-asan` 才生效。

---

## 4. `TX8SchedModel`：位置、提取方法、成本

### 4.1 位置（已定位到符号级）

```
strings libLLVMRISCVDesc.a | grep '^TX82'   -> 24 个资源名
  TX82ALU TX82CSR TX82FDIV TX82FMAC TX82FMISC TX82FMV TX82KCoreWB TX82LSU TX82MDU
  TX82NCCConfig TX82SIMTStack TX82VADD TX82VCMP TX82VCTRL TX82VCVT TX82VDISPATCH
  TX82VIALU TX82VISPU TX82VLOAD TX82VMISC TX82VMUL TX82VMULTIF TX82VSTORE TX82VSUB
```
命中成员唯一：**`libLLVMRISCVDesc.a` 的 `RISCVMCTargetDesc.cpp.o`**
（即 tablegen 生成的 `RISCVGenSubtargetInfo.inc` 被编进该 TU，
`RISCV_MC::createRISCVMCSubtargetInfo` 在此）。符号**未 strip**，且带 size：

| 符号 | size | 元素 | 条数 |
| --- | --- | --- | --- |
| `llvm::TX82ModelProcResources` | 0x320 | `MCProcResourceDesc` 32B | **25** |
| `llvm::TX82ModelSchedClasses` | 0x14fac | `MCSchedClassDesc` 14B | **6138** |
| `llvm::RISCVWriteProcResTable` | 0x1d4c | `MCWriteProcResEntry` 6B | **1250** |
| `llvm::RISCVWriteLatencyTable` | 0x528 | `MCWriteLatencyEntry` 4B | **330** |
| `llvm::RISCVReadAdvanceTable` | 0x2e20 | `MCReadAdvanceEntry` 12B | **984** |

（`nm -S -C wt-passes/evidence/sched/RISCVMCTargetDesc.cpp.o` 可直接复核；
`RISCVCodeGen/Utils/Info/TargetMCA` 里 `TX82` 字符串数均为 0 → 模型不在 CodeGen 侧。）

### 4.2 推荐方法：**链接厂商静态库、用公共 API 整体导出**（已跑通 PoC，成本 ≈0.5 会话）

`wt-passes/sched/dump_schedmodel.cpp`（PoC，已编译运行成功）：
`llvm-config --cxxflags` + **`-DNDEBUG`**（关键：厂商是 Release 构建，
`MCSchedClassDesc` 不含 `NameOff`，不加 `-DNDEBUG` 结构体尺寸差 4B→读到垃圾）
→ 链接 `libLLVMRISCVDesc.a` 等 → `TargetRegistry::lookupTarget` →
`createMCSubtargetInfo(triple,"tsm-x82","")` → 直接读 `MCSchedModel` +
`MCSubtargetInfo::getWriteProcResBegin/End`、`getWriteLatencyEntry`、
`getReadAdvanceEntries`，并遍历 `MCInstrInfo` 导出 opcode→sched class。

产出（[`wt-passes/sched/tx82_schedmodel.txt`](../../wt-passes/sched/tx82_schedmodel.txt)，
186KB，脚本同时生成 `.td` 骨架
[`tx82_schedmodel_regen.td`](../../wt-passes/sched/tx82_schedmodel_regen.td)）：

```
IssueWidth=2  MicroOpBufferSize=0  LoadLatency=2  HighLatency=10
MispredictPenalty=5  PostRAScheduler=0  CompleteModel=0
ProcResource（25，含 InvalidUnit）逐条：NumUnits / SuperIdx / BufferSize / SubUnits
  TX82ALU<2>、TX82VLOAD<2>、TX82KCoreWB{buffer=1}、TX82NCCConfig{buffer=1}、…
SchedClass 有效项 1896 条（WR 1897 / WL 1930 / RA 5696 行），WR 的资源索引全部 ≤24（未越界）
Opcode 表 17602 行（其中 7704 条有有效 sched class；其余是 G_*/PHI 等无调度类）
```

残差（可控，≈0.2 会话）：6138 个 sched class 里 4020 个是 variant/invalid
占位（`MCInstrInfo` 的 generic opcode 所致），需要按
`MCSubtargetInfo::resolveVariantSchedClass(SC, MCInst*, MCInstrInfo*, CPUID)` +
`RISCV_MC::resolveVariantSchedClassImpl` 逐 opcode 合成 MCInst 来解析；
另有 `MakeWriteRes` 的 `.td` 名字（`SchedWrite`/`SchedRead` 符号名）在 Release
构建里被 `NDEBUG` 裁掉（`SchedClassNames` 不可用），需要用上游
`RISCVSchedModel.td` 的同序定义回填名字——属机械映射，不需猜测。

### 4.3 备选/交叉验证

- **B（0.5 会话，零链接）**：直接按上表 size 从 `.data.rel.ro`/`.rodata` 读数组 +
  `readelf -r` 重定位解指针。需要 `MCSchedule.h` 的结构布局（厂商头文件自带），
  信息量与 A 等价但更笨。
- **C（0.5 会话，独立验证，必做）**：把提取结果编译进我们的树后，对 397 助记符语料
  跑 `llvm-mca -mcpu=tsm-x82 -instruction-info -instruction-tables -resource-pressure`，
  与厂商输出逐行 diff（`wt-passes/corpus/mca_probe.s` 已做通验证：
  `Dispatch Width 2`、24 资源表、`vadd.vv` Lat=6 / RThroughput 37.00 均对齐）。
- **不要**试图用 `llvm-tblgen` 反推（无 `.td` 输入）；也**不要**只靠 `llvm-mca` 黑盒反演
  （拿不到 `BufferSize`/`SubUnitsIdxBegin`/`MicroOpBufferSize`，且 6138 类的工作量更大）。

**结论：SchedModel 可 100% 机械提取，成本 ≈0.5 会话（提取）+1 会话（接入与 `llvm-mca` 对拍）。**

---

## 5. 建议的复现顺序

**T0（≈1.5 会话，先做，机械且收益确定）**
1. `TX8SchedModel` 提取 + 接入（替换 `RISCVProcessors.td:580` 的 `NoSchedModel`）+ `llvm-mca` 对拍。
2. `TX8SchedStrategy`（`TX8Pre/PostRASchedStrategy` + `-riscv-tx8-sched`/`-tx8-lmul-sched`）。

**T1（≈9 会话，RAISA 设备路径的"对象级"可验证部分）**
3. `vreg-align`（M，有 `=false` 消融开关，可差分验证，5/21 对象变化）。
4. `legalize-exec`（M）+ `tx8-insert-xfrm`（S）+ `tx8-legalize-ct`（M）。
5. `rpu-lower-shared` + `TX8MemoryUtils`（M）。
6. `tx8-profiling-kernelfunc`（S，`-tx8-vsetbarrier` 可控，4/21 对象变化）。

**T2（≈10 会话，SIMT/IR 语义，风险最高）**
7. `tx8-annotate-control-flow`（M）→ `rpu-unify-divergent-exit-nodes`（S）→ `simt2simd`（L，需 ISA/SIMT 规格）。
8. `config-instr-opt`（M）+ `promote-alloca`（M）+ `rpu-always-inline` + `rpu-remove-inline-device-functions`（S）。

**T3（只在有产品需求时做）**
9. `tx8-asan`（L，需 fat-pointer ASan 运行时契约）。
10. `vreg-rename` + `TX8RegisterConflictDetector`（L，且只在 -O2 生效，而 -O2 目前会崩）。

**明确不做（0 会话）**：`bank-aware-shared`、`rpu-annotate-uniform`、`tx8-record-npu-pc`
—— 三者在 28/28 组合中都不在任何 pipeline，且厂商 `opt` 是 new-PM-only，
legacy 注册无法从命令行触达，属**出厂即死代码**。

---

## 6. 会话估算

| 分层 | 内容 | 会话 |
| --- | --- | --- |
| T0 | SchedModel + SchedStrategy | 1.5 |
| T1 | 6 个 pass（含 MemoryUtils） | 9 |
| T2 | 6 个 pass | 10 |
| T3 | ASan + vreg-rename + conflict detector | 9.5 |
| 不做 | 3 个不可达 pass | 0 |
| 集成/验证 | 与厂商差分回归 + `-O2` 崩溃规避 | +25% |
| **合计（全部可达 15 个 pass + SchedModel）** | | **≈30 会话（区间 24–40）** |
| **最小可用（T0+T1）** | | **≈10.5 会话** |

比 `FORK_REBUILD_PLAN.md` 的"15–40"收敛了区间，并把 **3 个 pass 直接归零**、
把 **ASan/vreg-rename 移出关键路径**。**注意**：即使 pass 全部补齐，
`gemini-cmodel/kernels/*.c` 在真实配方下也不会改变目标文件——这些 pass 的价值全部在
**RAISA `.rc` 设备路径与 -O1+ 优化路径**上；`-O2` 的 `simt2simd` SIGSEGV 与
`TX8MCInfo::convertRVVALUToCTEncoding` fatal error 是**独立于 pass 复现的、更优先的阻塞项**。

---

## 【parent 订正 · 2026-10-16】`tx8-insert-xfrm` 的 "A 配方 NO-EFFECT 15/15" **对 `-O2` 向量码不成立**

原文把 `tx8-insert-xfrm` 记为"A 配方（musl/kernel）下 15/15 无效果"。**那是语料问题**：那 15 个输入在
`-O0` 下**没有向量码**，而这个 pass 只在向量码上动手。反例（`wt-isel/out/t1a/FINDINGS.md`，
复现 `wt-isel/probe/avl100.c` @ `-O2`，`.text` 26/26 B、7 条指令、5 个 `0x7b` 字全同，**只差物理寄存器**）：

```
厂商: vle32.v v8,(a0)  vle32.v v9,(a1)  vfadd.vv v0,v8,v9  vse32.v v0,(a2)
我们: vle32.v v8,(a0)  vle32.v v9,(a1)  vfadd.vv v8,v8,v9  vse32.v v8,(a2)
```

两条决定性证据：
1. `-mllvm -print-after=finalize-isel`：两侧 MIR **逐字相同**（同样 vreg/操作数，**两侧都没有**
   `implicit $mem_alloc`，FP 加法两侧都是 `implicit $frm`）⇒ "我们 `.td` 缺注解"的假设**被证伪**。
2. `-mllvm -print-before=greedy`（RA 前）：**厂商多两个隐式操作数**——
   向量访存带 `implicit $mem_alloc`，FP 运算把 `implicit $frm` 改写成 `implicit $xfrm`。
   ⇒ 差异起源于 `finalize-isel → greedy` 之间的这个 fork pass；隐式 physreg 使用改变 RA 自由度，
   于是 `v0` vs `v8`。

**并且**：本树 `RISCVRegisterInfo.td` 里**连 `xfrm` 与 `mem_alloc` 这两个物理寄存器都没有**
（ISA 侧只有同名的 `XTsm_VSETXFRM` 指令）。⇒ 修法是"补两个物理寄存器 + 实现 RA 前的
`tx8-insert-xfrm`"，**不是**补 `implicit` 注解。已另派专线（`docs/compiler/TX8_INSERT_XFRM.md`）。

**教训**（与 §1 的"任何'还剩多少活'的口径先问它排除了什么"同源）：**pass 的"无效果"结论必须按
优化档 × 语料是否有向量码分层给出**；只在 `-O0` 无向量码语料上测出的 no-effect，不能外推到 `-O2`。
