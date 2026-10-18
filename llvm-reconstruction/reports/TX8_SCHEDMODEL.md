# TX8 `tsm-x82` 调度模型（SchedModel）重建报告

目标：把厂商 `llvm-toolchain` 里被裁掉的 `TX82Model` 还原成**我们重建树里的 tablegen 源码**，并让
`-mcpu=tsm-x82` 的 `llvm-mca` 行为与厂商逐指令一致。产出：新文件 `RISCVSchedTX82.td`（7827 行）
+ `RISCV.td` 一行 include + `RISCVProcessors.td` 两处（`NoSchedModel`→`TX82Model`、补 tune 列表）。
补丁 [`wt-sched/tx82-schedmodel.patch`](../../wt-sched/tx82-schedmodel.patch)；命令/脚本/日志在
[`wt-sched/`](../../wt-sched/)；厂商工具一律 `timeout` + `ulimit -f`。


## 1. 一句话结论

**厂商 `tsm-x82` 模型可以 100% 机械还原**：两边同名的 14312 个 opcode 中，4522 个有效调度类
**逐字段（uOps / BG / EG / RO / 每 write 延迟 / 资源+ReleaseAtCycle / 读提前量）完全相同**，9790 个在
两边**都是 invalid 类**，**差异 0 条**；25 条 `ProcResource`（名称/NumUnits/BufferSize/顺序）与厂商表
**逐字节相同**。`llvm-mca --instruction-tables` 对标量语料 **89/89 逐行相同**；RVV 的不一致已定位到
**指令归属缺口**（`RISCVVInversePseudosTable` 里没有我们的 `XTsm_*`，不是模型缺口，见 §5.3/§6.3）。
顺带用同一套提取手段钉死了 **G8**（`TSM_X82` 缺 5 个 tune feature），见 §7。


## 2. 提取方法（可复现的确切命令）

模型不在 CodeGen 侧，而在 `llvm-toolchain/lib/libLLVMRISCVDesc.a` 的 `RISCVMCTargetDesc.cpp.o`
（tablegen 生成的 `RISCVGenSubtargetInfo.inc` 编进该 TU）。符号未 strip 且带 size：
`TX82ModelProcResources 0x320(25)`、`TX82ModelSchedClasses 0x14fac(6138)`、
`RISCVWriteProcResTable 0x1d4c(1250)`、`RISCVWriteLatencyTable 0x528(330)`、`RISCVReadAdvanceTable 0x2e20(984)`。

用**厂商公共 MC API**（不反汇编、不猜结构）整体导出 —— `wt-passes/sched/dump_schedmodel.cpp`
（`wt-sched/dump_schedmodel2.cpp` 是加了 BG/EG/RO 的副本）：

```bash
# 关键：厂商是 Release 构建，MCSchedClassDesc 无 NameOff，必须 -DNDEBUG，否则结构体尺寸差 4B → 读到垃圾
g++ -DNDEBUG $(cat cxxflags.txt) dump_schedmodel.cpp -o dump_schedmodel -L$VBIN/../lib \
  $($VBIN/llvm-config --ldflags) $($VBIN/llvm-config --libs riscvdesc riscvinfo riscvtargetmca \
  riscv mc mcparser support) $($VBIN/llvm-config --system-libs) && ./dump_schedmodel tsm-x82 > tx82_schedmodel.txt
```
产出 1.8 MB oracle：标量 / 25 资源 / 1896 有效类的 (uOps,BG,EG,RO,WR,WL,RA) / 17602 条 opcode→类。

TableGen 侧**不需要黑盒反演**：每条数据都能定位到生成逻辑（`SubtargetEmitter.cpp:genSchedClassTables`、
`CodeGenSchedule.cpp:collectSchedClasses`），据此把"每类的聚合描述"反解成"每个 `SchedWrite` 的 `WriteRes`"。
反解管线（`wt-sched/pipeline.sh` 一条命令重跑，**不需要编译**：用已编好的 `llvm-tblgen` 重新生成 `.inc`
后纯 Python 对拍，单轮 < 1 分钟）：

关键脚本（`wt-sched/`）：`vendorparse/incparse` 解析两边 dump（类名靠 `DBGFIELD(/*Name*/)`，需
assertions-on 构建）；`segment` 用 `llvm-tblgen -print-records` 的 4309 `SchedWrite`/1607 `SchedRead` 词表
把类名切成 (writes,reads)（5826/5947 唯一切分）；`crossmap/solve/gen3` 按 opcode 名投票、从"单 write 类"
读出 923 个 `WriteRes` 并对 1675 个目标类回验、生成 `.td`；`find_overrides` / `find_xtsm_overrides`
产出 InstRW（后者用 `llvm-mc -show-inst` 桥接同名不了的 `XTsm_*`）；`diff_model/diff_corpus/diff_mca_summary` 做三级差分。

## 3. 哪些是字节精确、哪些是反解/推断

| 项 | 来源 | 精度 |
| --- | --- | --- |
| `IssueWidth=2 MicroOpBufferSize=0 LoadLatency=2 HighLatency=10 MispredictPenalty=5 PostRAScheduler=0 CompleteModel=0`；25 条 `ProcResource`（名称/NumUnits/SuperIdx/BufferSize/顺序） | `MCSchedModel` + `TX82ModelProcResources[]` 直接读 | **字节精确**（资源表逐行相同，无 Super/SubUnits） |
| 每个**有效类**的 uOps / BG / EG / RO / WR（资源,Release,Acquire）/ WL（周期）/ RA（UseIdx,周期） | `TX82ModelSchedClasses[]` + 全局三表直接读 | **字节精确**（`AcquireAtCycle` 全 0） |
| opcode→调度类（17602 条）、哪 4020 个类 invalid | 厂商 `MCInstrInfo` 遍历 | **字节精确** |
| 923 个 `SchedWrite` 的 `Latency` / `ProcResources`+`ReleaseAtCycles`+`NumMicroOps`；2120 个标 `Unsupported=true` | WL 表按 write 位置一一对应；多 write 类取"该类只含这一个 write"的类读出后回验；`Unsupported` = 只出现在 invalid 类里的 write | **反解**，1675 个目标类零冲突、0 反例 |
| 1468 条 `ReadAdvance`（`ReadRotate{Imm,Imm32,Reg,Reg32}=1`，其余 0） | 由每类 `NumRA`/`UseIdx` 与类名里的 read 列表反解 | **反解**，与 1675 个目标类的 RA 计数/周期完全一致 |
| 30 条 InstRW 覆盖 `PseudoVFSUB_VV_*` / 249 条覆盖 `XTsm_*` | 前者：上游把 VFADD/VFSUB 放同一个 `WriteVFALUV`，厂商分成 `TX82VADD`(6)/`TX82VSUB`(5)；后者：用 `-show-inst` 把 `XTsm_*` 配到厂商同名指令（`VADD_VV` 等）照抄其类 | **推断** / **机械桥接**（见 §6.1、§8） |


## 4. 模型数值

```
TX82Model: IssueWidth=2 MicroOpBufferSize=0 LoadLatency=2 HighLatency=10
           MispredictPenalty=5 PostRAScheduler=false CompleteModel=false
ProcResource（24 + InvalidUnit；NumUnits/BufferSize）:
  TX82ALU<2> TX82CSR TX82FDIV TX82FMAC TX82FMISC TX82FMV TX82LSU TX82MDU        (BufferSize=0)
  TX82KCoreWB TX82NCCConfig TX82SIMTStack TX82VADD TX82VCMP TX82VCTRL TX82VCVT
  TX82VDISPATCH TX82VIALU TX82VISPU TX82VLOAD<2> TX82VMISC TX82VMUL TX82VMULTIF
  TX82VSTORE TX82VSUB                                                           (BufferSize=1)
WriteRes 923（含 NumMicroOps>1 的，如 WriteSFB=2） / Unsupported 2120 / ReadAdvance 1468（4 条=1）
InstRW 279。典型：WriteIALU→TX82ALU lat1；WriteLDW/LDD→TX82LSU lat3；WriteIMul→TX82MDU lat3；
WriteVADDV_M1→TX82VIALU release9 lat6（MF2=7/M2=13/M4=21/M8=37）；WriteVMULV_M1→TX82VMUL rel41 lat10；
WriteVLE8_V→TX82VLOAD lat21 release32；WriteIDiv→TX82FDIV lat39
```


## 5. 验证结果

### 5.1 逐 opcode 模型差分（`diff_model.py`）

```
vendor: NumSchedClasses=6138 procRes=25      ours: NumSchedClasses=6081 procRes=25
proc-resource mismatches: 0 / 25
shared opcode names: 14312  (ours 14711, vendor 17602)
  exact 4522 / both-invalid 9790 / MISMATCH 0
```
差分字段：`NumMicroOps`、BG/EG/RO、WR 表 `[(ProcResourceIdx, Release, Acquire)]`（含顺序）、
WL 表每 write 周期、RA 表 `[(UseIdx, Cycles)]`。**0 条差异**。

### 5.2 逐指令差分（含名字不同的 `XTsm_*`，`diff_corpus.py`）

用 `llvm-mc -show-inst` 把同一份汇编在两边产生的 MCInst 序号配对：

| 语料 | 条数 | exact | 两边都 invalid | mismatch |
| --- | --- | --- | --- | --- |
| `corpus/schedprobe.s`（标量/标量浮点/RVV/xtsm82s） | 147 | **132** | 15 | **0** |
| `corpus/xtsm_asm_ok.s`（两侧汇编器都接受的 **399** 条 `XTsm_*`，= 全部） | 399 | **259** | 140 | **0** |

### 5.3 `llvm-mca --instruction-tables` 端到端（`diff_mca.sh`）

厂商 vs 我们重建构建，同一语料，`-skip-unsupported-instructions=lack-sched`（两侧 skip 集合也相同，
stderr diff = 0）；按类型拆开（`diff_mca_summary.py`）：

| 类型 | 逐指令完全一致（uOps/延迟/RThroughput/整行资源压力） | 不一致 |
| --- | --- | --- |
| 标量（整数/乘除/Zbb/访存/分支/CSR/标量浮点+Zfh，89 条） | **89** | **0** |
| RVV（vadd/vmul/vdiv/vle… 43 条） | 3 | 40 |

同一差分对 `corpus/xtsm_asm_ok.s`（399 条 `XTsm_*`；该语料**没有 `vsetvli`** ⇒ 不触发 LMUL 重定向）：
**388/399 两侧 `llvm-mca` 行完全相同**；9 条只差 `HasSideEffects`/`MayLoad`/`MayStore` **标志列**
（uOps/延迟/RThroughput/资源压力全同）——那是 `MCInstrDesc` 位，来自指令定义、不属于调度模型；
1 条（`vmv.v.v`）模拟资源压力值不同（vendor 10.00 / 我们 38.00）。

`schedprobe.s` 里 RVV 那 40 条**不是模型差异**：厂商 `llvm-mca` 用前一条 `vsetvli` 的 LMUL 把 `VADD_VV` 换成
`PseudoVADD_VV_M1` 的调度类（实测 vendor：`e32,m1`→9.00、`e16,mf2`→7.00、`e64,m8`→37.00，正是
`WriteVADDV_{M1,MF2,M8}` 的 release），链路是 `RISCVInstrumentManager::getSchedClassID` →
`RISCVVInversePseudosTable::getBaseInfo`（`MCA/RISCVCustomBehaviour.cpp:301`）。而我们树里**没有
`VADD_VV` 这类"重映射编码指令"**（只有另起炉灶的 `XTsm_VADD_VV`），`RISCVGenSearchableTables.inc`
里 XTsm 行数 = **0** ⇒ `getBaseInfo` 返回 null、回落到静态基类（M8=37）。见 §6.3。


### 5.4 厂商 CModel 端到端（`wt-sched/cmodel`，用带本次改动的 clang）

我把 `wt-cmodelrun/{tools,common,device,host}` 复制到 `wt-sched/cmodel`（**不碰** `wt-cmodelrun`），
`run_matrix.py --compilers vendor,sched`（sched = `wt-isel/build/bin/clang`）：

| case | `-O0` | `-O1` |
| --- | --- | --- |
| add / muladd / red | **RAN，与厂商产物逐位相同**（`bitwise_vs_other=True`，300/300 字） | 同左 |
| gem_add / gem_mul | 两侧**都** CRASH（`run=-6`，厂商同样崩，两者 `7b=4`） | 同左 |

⇒ 6/6 可跑格子**逐位一致**，4 个崩溃格子两侧行为相同（与本次改动无关）。

## 6. 与厂商仍然不同的地方（残差）

1. **`PseudoVFSUB_VV_*` 的类划分**（30 类/30 opcode）：上游用同一个 `SchedBinaryMC<"WriteVFALUV",…>`
   描述 VFADD 与 VFSUB，厂商分成 `TX82VADD`/`TX82VSUB`。我们用 30 条 `InstRW` 复现，**逐 opcode 与厂商
   一致**，但"厂商用 InstRW 还是改了指令 `Sched<>`"无法从二进制区分。
2. **`XTsm_*` 指令名/归属不同**：我们的 `vadd.vv` 汇编成 `XTsm_VADD_VV`（厂商 `VADD_VV`），机器码完全相同
   （`[0xfb,0x90,0x21,0x0e]`）。这类指令在 `RISCVInstrInfoXTsm82s.td` 里**一个 `Sched<>` 都没有**，
   本任务用 `-show-inst` 桥接给 249 条补了 InstRW，**399 条全部对齐**（259 exact / 140 两边都 invalid）。
   **厂商自己也不建模** conv/gemm/direct_write/dte_set_length 等 tensor 指令（厂商 `llvm-mca` 同样 skip），
   天然一致。
3. **RVV 的 LMUL 重定向**（§5.3）：没把 172 条 remapped-RVV 指令登记进 `RISCVVInversePseudosTable`
   ⇒ `llvm-mca` 不按 `vsetvli` 的 LMUL 换类。**静态模型本身逐 opcode 一致**，这是指令归属缺口；
   修法 = 让它们占用上游基名（`VADD_VV` …）或补表行。
4. **`fence`**：厂商的类走 `SchedWriteVariant`，其自身 `llvm-mca` 报 `unable to resolve scheduling
   class for write variant`（厂商模型在 fence 上是坏的）；我们把对应 write 标 `Unsupported`，两边都 skip。
5. `TX8Pre/PostRASchedStrategy` 未实现（§9）。


## 7. `TSM_X82` 的完整 ProcessorModel 属性（G8，已修）

`TSM_X82` 的 feature/tune 集合可直接从厂商二进制读出：`wt-sched/dump_procfeat.cpp`（同 §2 的链接方式）
枚举 `MCSubtargetInfo::getAllProcessorFeatures()` 并用 `checkFeatures("+X")` 过滤：

```
厂商 (27): 64bit andes45 c f i m no-default-unroll prefer-vsetvli-over-read-vlenb
           short-forward-branch-opt use-postra-scheduler xtsm82s zbb zca zfbfmin zfh zfhmin
           zicsr zifencei zmmul zve32f zve32x zve64x zvfbfmin zvfh zvfhmin zvl32b zvl64b
我们 (22): 64bit c f i m xtsm82s zbb zca zfbfmin zfh zfhmin zicsr zifencei zmmul
           zve32f zve32x zve64x zvfbfmin zvfh zvfhmin zvl32b zvl64b
```

差集 5 个**全是 tune feature**：`TuneAndes45`、`TuneNoDefaultUnroll`、`TunePreferVsetvliOverReadVLENB`、
`TuneShortForwardBranchOpt`、`TunePostRAScheduler`。⇒ `fork-source/` 恢复的 `TSM_X82` 少了**五个** tune
feature（不只是 unroll），导致 `enableDefaultUnroll()` 为真（G8：`-O2 -fno-vectorize` 我们 86 B vs 厂商 550 B）。

**修法**（已落盘）：放进 `RISCVProcessorModel` 的**第 4 个参数（tune 列表）**，arch feature 列表一字不动
⇒ `printMArch` 的 `rv64i2p1_..._xtsm82s1p0` 不变。原始位向量也交叉验证：逐个单独放一个 tune feature 重新
tblgen，得到的 tune 位下标 `TuneAndes45=289 TuneNoDefaultUnroll=297 TunePostRAScheduler=307
TunePreferVsetvliOverReadVLENB=309 TuneShortForwardBranchOpt=312`，与一次放 5 个时置位的位集合完全一致。

**端到端实测**（ISel 线用带本次改动的 clang 重编后）：`-O0/-O1/-O2 -fno-vectorize/-O3 -fno-vectorize`
在 `tiny_vec_fpreduce.c` 与 `tiny_fma_chain.c` 上 `.text` 的**长度与 sha256 全部与厂商相同**
（G8 那一格 `-O2 -fno-vectorize`：改前 **86 B** = stock 行为 → 现在 **550 B**，厂商也是 550 B）；
证据 `wt-sched/logs/g8_ab.txt`。


## 8. 已知架构差异（会影响后续 MIR/ISel/调度对比）：remapped-RVV 的指令归属

**厂商做法**：保留上游"编码用"基名 `VADD_VV`/`VMUL_VV`/…，在 MC 层把它们的编码改成 Custom-3 `0x7B`
（RVV→0x7B 重映射）；于是 `RISCVVInversePseudosTable` 里 `(BaseInstr=VADD_VV, VLMul, SEW) →
PseudoVADD_VV_M1` 这些行天然成立。**我们的重建做法**：把这 172 条标准 RVV 助记符**另起炉灶定义成独立
指令族 `XTsm_*`**（`RISCVInstrInfoXTsm82s.td`）。三个可测后果：① 汇编 `vadd.vv` 两边机器码相同
（`0e2190fb`）但记录名不同（厂商 `VADD_VV` / 我们 `XTsm_VADD_VV`）；② 我们树里**没有 `VADD_VV` 记录**，
`RISCVGenSearchableTables.inc` 里 XTsm 相关行数 = **0**；③ 399 条 `XTsm_*` 在 .td 里**连一个 `Sched<>`
都没有**（grep `Sched` = 0 命中）。

**后果（本次实测）**：`llvm-mca` 的 `RISCVInstrumentManager::getSchedClassID`
（`MCA/RISCVCustomBehaviour.cpp:301`）靠 `RISCVVInversePseudosTable::getBaseInfo` 用前一条 `vsetvli`
的 LMUL/SEW 换调度类；我们查不到 ⇒ 回落到静态基类（M8 worst-case），故 RVV 的 `llvm-mca` 行与厂商不同
（标量 89/89 完全一致）。**影响面**：任何依赖"MC 层重映射 / LMUL-SEW 变体 / 逆伪表"的对比都会命中
（`llvm-mca` 的 RVV 行、`RISCVISelLowering.cpp:21678` 的逆查表路径）；而 sched 模型本身（逐 opcode
静态调度类）已与厂商完全一致。**修法方向**：让这 172 条占用上游基名，或在 `RISCVVInversePseudosTable`
补行，≈0.5 会话。本次已用 `llvm-mc -show-inst` 桥接给 249 条补 InstRW，使**静态**类一致。

## 9. `TX8SchedStrategy`（RE 结论，未实现）

从 `llvm-toolchain/lib/libLLVMRISCVCodeGen.a(TX8SchedStrategy.cpp.o)` 抽出（`wt-sched/schedstrategy/`）：

```
T TX8SchedStrategeCommon::{initialize,releaseTopNode,releaseBottomNode,schedNode}
T TX8PreRASchedStrategy::{initialize,pickNode,pickNodeBidirectional,pickOnlyChoice,schedNode}
T TX8PostRASchedStrategy::{initialize,tryCandidate,schedNode}
t getVLMUL(llvm::SUnit*)      b EnableTX8LMULSched (= -mllvm -tx8-lmul-sched)
U GenericScheduler::{initialize,initPolicy,pickNodeFromQueue,schedNode}   → Pre-RA 继承它
U PostGenericScheduler::{initialize,pickNode,schedNode}                   → Post-RA 继承它
U tryLatency/tryPressure/tryGreater/tryLess/SchedBoundary::* / TargetSchedModel::*
```
⇒ Pre-RA 版在 `GenericScheduler` 的 bidirectional 选点里插入 TX8 自己的比较（`getVLMUL(SUnit*)` +
`-tx8-lmul-sched`，最小化相邻向量指令的 LMUL 切换），Post-RA 版在 `PostGenericScheduler::tryCandidate`
里插同样内容；它 `U` 引用 `TargetSchedModel::hasInstrSchedModel/resolveSchedClass` ⇒ 必须先有 SchedModel。
该开关在 21 个对象上 **0/21 变化**，只影响指令顺序。


## 10. 剩余工作与会话估算

| 项 | 会话 |
| --- | --- |
| 本任务：模型提取+接入+`TSM_X82` tune 修复+三级对拍（逐 opcode/逐指令/标量 `llvm-mca`） | **已完成（≈1）** |
| 172 条 remapped-RVV 并回上游基名 / 补 `RISCVVInversePseudosTable`（§8） | 0.5 |
| 给 9 条 `XTsm_*` 补 `hasSideEffects/mayLoad/mayStore`（`llvm-mca` 标志列，§5.3） | 0.1 |
| 复现 `TX8Pre/PostRASchedStrategy` + `-print-after=machine-scheduler` 对拍 | 1.0 |
| 厂商 `fence` 的 variant 类（厂商自身 `llvm-mca` 就报错，可选） | 0.2 |
