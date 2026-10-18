# RAISA 后端缺口：317 格 parity 基线 + 逐格归因（R1）

日期：2026-10-16。工作树 `wt-raisa-be/`（**新起**，不复用 `wt-raisa/`）。
厂商 oracle = `llvm-toolchain/bin/clang`（Release，无断言）；我方 = `wt-raisa-be/build/bin/clang-21`
（`rc1 + 补丁序列 + 0028/0029/0030完整/0031/0032/0033/0034/0040–0044`，Release **+assertions**）。

> 本文只做"量 + 归因"，**不改任何源码**。结论：**93 格真实缺口全部在 `.rc` 设备路径，
> 其中 90 格的主阻塞是缺失的 `simt2simd` pass；另有两条此前未列出的结构性缺件
> （RAISA 目标 datalayout/`rpu_kernel` CC、`TX8LowerSharedPass`）。**

---

## R1'-a 进展：RAISA 目标层已闭环（补丁 0001/0002，2026-10-16 18:0x）

**已落地**（`wt-raisa-be/patches/`）：

| 补丁 | 内容 | 行数 |
| --- | --- | --- |
| `0001-raisa-target-layer.patch` | RAISA 目标层：clang/LLVM 两侧的 RPU datalayout（`p:64:64:64:32` + AS3/AS4 32 位 + `-A5`）、`getDeviceKernelCallingConv`/`setTargetAttributes` → `CallingConv::RPU_KERNEL`、AsmWriter 打印 `rpu_kernel`、LLParser 接受 `rpu_kernel` | 83 |
| `0002-raisa-device-datalayout-check-and-rpu-cc.patch` | ① `BackendUtil` 的 DL 一致性检查改比"模块实际携带的 layout"（RAISA 设备编译里 `CI.getTarget()` 与产生模块的 AST target 不是同一个对象）；② `RISCVISelLowering::LowerFormalArguments` 接受 `RPU_KERNEL`（此前 `fatal error: error in backend: Unsupported calling convention`） | 51 |

**验证（每条都可复现）**：

1. **我方 pre-`simt2simd` 设备 IR ≡ 厂商 `-print-before=simt2simd` 的 IR**（归一化掉值名/`ModuleID`/`clang version` 后逐行相同）：
   `mathdev_raisa_fadd_rn_f` ✅、`mathdev_raisa_fmax_f` ✅。
   ⇒ `simt2simd` 的**输入契约已建立**，实现从"猜"变成"吃 mapping 表"。
2. `-emit-llvm` 的 DL 报错 **1 → 0**；`-S` 从 `Unsupported calling convention` 前进到
   `RISCVISelLowering.cpp:14610` 的 `Don't know how to custom type legalize this intrinsic!`
   （= 唯一剩余的 `simt2simd` 缺件）。
3. **musl 零回退（A/B 实测）**：把 0001+0002 回退重编译出的 `clang-pre-datalayout` 与当前
   `clang-21` 对比，14 kernel × 4 配置（`-O0`/`-O1`/`-O2 -fno-vectorize`/`-O2`）**56/56 `.o` 逐字节相同**。
4. `-O0` 验收基线仍成立：`obj 14/14 IDENTICAL`、`shared 14/14 IDENTICAL`（`out/matrix_*.tsv`）。
5. 全量 317 格复测（`parity/round4_final.tsv`）：`byte-identical 132 / OURS-GAP 93 /
   vendor-fail-too 36 / vendor-fail-same-diag 30 / byte-diff 25 / ours-extra-ok 1`；
   143 个 `.c` 格**仍零缺口**。
   归因：`R1'/simt2simd 90`、`R2/fork-pass(codegen) 8`、`cosmetic(+assertions 值名) 17`、
   `R1'/lower-shared 3`，**`RAISA-target(datalayout/CC)` 由 9 → 0**。

**两条工具/纪律教训（已写进脚本）**：
- 从 `wt-xfrm/build` 拷来的构建目录，其二进制 `.ninja_deps` 仍指向 `wt-xfrm/src`，**纯 `ninja` 不会因
  头文件改动而重编译**（`.cpp` 改动会）。必须先 `ninja -t deps > logs/ninja_deps.txt`，再用
  `tools/minibuild.py --force-src <改过的 .h 和 .cpp>`。我第一次漏了这点，得到过一轮"假绿"。
- 不要在构建还没 link 完时并跑语料采集：link 是原地覆盖 `bin/clang-21`，会把早跑的格子测成旧二进制
  （`abi_*_front` 9 格就是这么被污染的）。

---

## 0. 结论速览

### 0.1 parity 基线（`parity/round1_parity.tsv`，317 格 = 厂商 manifest 里全部编译类用例）

| 判定 | 格数 | 含义 |
| --- | --- | --- |
| `byte-identical` | **132** | 双方 rc=0 且**归一化后逐字节相同**（见 §1.2 口径） |
| `OURS-GAP` | **93** | 厂商 rc=0、我方失败 —— **真实缺口** |
| `vendor-fail-too` | 36 | 厂商自己失败（含 8 个 hang、厂商 crash / broken function），我方也失败 |
| `vendor-fail-same-diag` | 30 | 双方都失败且**诊断逐字相同**（厂商也没有这个能力） |
| `byte-diff` | 25 | 双方 rc=0 但字节不同（见 §3，均有归因，无一条是"语义分歧"） |
| `ours-extra-ok` | 1 | 厂商失败而我方成功 |

- **93 格缺口全部是 `raisa` 语言（`.rc`）**；143 个 `.c`（catalogue/immediate/config…）**零缺口**。
- 缺口按 group：`mathdev 53 / mem 12 / id 10 / alloc 7 / abi 6 / control 5`。
- 66 格厂商自己失败（`builtin_*` 55、`mem_*` 9、`id_* 2`）——**不算我们的缺口**，已在表里显式标注。

### 0.2 阻塞工作归因（缺口 93 + byte-diff 25 = 118 格）

| block | 格数 | 证据（每格在 TSV `evidence_path` 列） |
| --- | --- | --- |
| `R1'/simt2simd` | **90** | 我方 pre-backend IR 仍带 `llvm.riscv.{raisa.*,read.reg.*}` 调用点，而厂商**同一格**的 `-print-after=simt2simd` 把它们消掉了（`recon/simt2simd/<cell>.{before,after}.ir`，我方 IR 在 `recon/ours_ir/<cell>.ll`） |
| `RAISA-target(datalayout/CC)` | 9 | `.ll` 文本差异里出现 `target datalayout` 与调用约定（§2.1） |
| `R2/fork-pass(codegen)` | 8 | `.s`/`.o` 真差异，指令序列/`ne_set_mkn` 去重不同（§2.3） |
| `cosmetic(+assertions build)` | 8 | 仅 IR 值名/`entry:` 标签/clang 版本元数据不同（§1.3） |
| `R1'/lower-shared` | 3 | 我方报 `Unknown command line argument '-riscv-bank-aware=true'`（该 option 属缺失的 `TX8LowerSharedPass`） |

---

## 1. 口径：为什么必须归一化，且归一化是合法的

### 1.1 复现命令

```bash
source setup/activate.sh                     # 只为 cmodel，编译不需要
cd wt-raisa-be
python3 tools/corpus_parity.py collect --cc /home/shaokai/Desktop/work/tx82/llvm-toolchain/bin/clang   --tag vendor --jobs 2
python3 tools/corpus_parity.py collect --cc /home/shaokai/Desktop/work/tx82/wt-raisa-be/build/bin/clang-21 --tag ours   --jobs 2
python3 tools/vendor_pass_evidence.py --jobs 2      # 厂商 simt2simd before/after IR
python3 tools/ours_pre_ir.py --jobs 2               # 我方 pre-backend IR
python3 tools/round1_attrib.py                      # → parity/round1_parity.tsv
```

`tools/corpus_parity.py` 逐字复刻厂商自己的 `round2/run.py`：`.rc` 走
`-cc1 -triple riscv64-unknown-raisa -target-cpu tsm-x82 -target-feature +xtsm82s
-fraisa-is-device -x raisa -O2`；`.c` 走驱动 `--sysroot … -mcpu=tsm-x82 -O2`
（我方额外显式给 `--target=riscv64-unknown-linux-musl`，即厂商的内建默认 triple）。
每格 `ulimit -c 0; ulimit -f 262144; timeout -s KILL 25`，`--jobs 2`。

### 1.2 归一化（`tools/normcmp.py`）——三处差异已逐条证伪为"非代码差异"

| 差异 | 证据 | 处理 |
| --- | --- | --- |
| `.ident` 字符串 | 厂商 `…(ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf2…)` vs 我方 `…(https://github.com/llvm/llvm-project.git cbcb48a…)` | `.s` 丢 `.ident` 行；`.o` 丢 `.comment` 段 |
| `.c` 走驱动 → 驱动加 `-fno-verbose-asm`，`.rc` 走 `-cc1` 不加 ⇒ 厂商 `.c` 输出没有 `# %entry` 这类 MBB 注释 | `diff <(grep -v '\.ident' vendor/x.s) <(grep -v '\.ident' ours/x.s)` 只剩 `# %bb.0:` vs `# %bb.0:   # %entry`，指令行完全相同 | `.s` 去行尾 `#…` 注释后比较 |
| `.ll` 值名 | 厂商 cc1 带 `-discard-value-names`，我方不带 | `.ll` 去 `ModuleID/source_filename/!llvm.ident/clang version` 行，并把 `%name` 归一为 `%X`、空标签行折叠（仅用于判定 `cosmetic`） |

第三条的根因是**构建配置差异**：`-discard-value-names` 是 clang 的
`!IsAssertionsBuild` 默认（我 `--version` 可见 `Build config: +assertions`，厂商无此行）。
它只影响 `-emit-llvm` 文本的值名，**不影响任何机器码**。
⇒ 若要逐字节比 `.ll` 文本，验收构建应改成**无断言的 Release**；比 `.s`/`.o` 不受影响。

### 1.3 单独证伪的 8 格 `cosmetic`

`effects_*`（8 格，`.c` 的 `-emit-llvm`）归一化后骨架完全相同，差异只有
`%p/%a/%b/%c` vs `%0/%1/%2/%3`、多一行 `entry:`、`!8 = !{!"clang version …"}`。
逐格 diff 存 `parity/diffs/<cell>.diff`。

---

## 2. 三条真实结构性缺件（本轮新发现，均非 G2/G3/G4）

### 2.1 RAISA 目标 datalayout 与 `rpu_kernel` 调用约定（9 格）

```
vendor: target datalayout = "e-m:e-p:64:64:64:32-p3:32:32:32:32-p4:32:32:32:32-i64:64-i128:128-n32:64-S128-A5"
ours  : target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
vendor: define dso_local rpu_kernel void @_Z9abi_floatPjfj(...)
ours  : define dso_local               void @_Z9abi_floatPjfj(...)
```

- 厂商 datalayout 多了 **`p:64:64:64:32`（32 位 GEP 索引）**、**地址空间 3/4（32 位）**、**`-A5`**；
  后果在 IR 可见：厂商 `getelementptr … i32 %0`，我方 `zext nneg i32 %0 to i64` + `i64` 索引 → 指针算术指令不同。
- `rpu_kernel` 是厂商多出来的调用约定（我方 `llvm/` 内 `rpu_kernel`/`RPU_KERNEL` 命中 **0**）。
- 证据：`parity/diffs/abi_*_front.diff`、`parity/diffs/config_*_front.diff` 等 9 格。
- 规模：**小**（datalayout 字符串 + 地址空间 + 新增 CC 枚举），但必须逐字对齐。

### 2.2 `TX8LowerSharedPass` / `-riscv-bank-aware`（3 格）

`alloc_l0{a,w}shared_bank_on.rc`、`alloc_l1shared_bank_on.rc`（manifest 带
`-mllvm -print-after=rpu-lower-shared -mllvm -riscv-bank-aware=true`）：

```
clang (LLVM option parsing): Unknown command line argument '-riscv-bank-aware=true'.
```

厂商侧这三格 rc=0。缺 `TX8LowerSharedPass.cpp.o`（归档 53 984 B / 19 函数），
它同时提供 `rpu-lower-shared` pass 与 `-riscv-bank-aware` option。

### 2.3 缺 pass 造成的机器码差异（8 格）

例：`config_across_compute_device`（厂商能编，我方 rc=0 但字节不同）

```
vendor: ld a1,0(a0); ld a2,8(a0); ld a0,16(a0); gemm a1,a2,a0; gemm a1,a2,a0
ours  : ld a0,0(a0); ld a1,8(a0); ld a2,16(a0); gemm a0,a1,a2; ne_set_mkn a5,a4,a3; gemm a0,a1,a2
```

⇒ `tx8-legalize-ct` / `optimize-config-instrs` 一类 CT 指令整理/配置去重缺失
（另见 `docs/compiler/TX8_ISEL_GAP.md` 的 `kcore_ncc_read` 例外）。
逐格 diff：`parity/diffs/<cell>.diff`。

---

## 3. 主阻塞：`simt2simd`（90/118）——已有可判定证据

厂商在 **IR 层**用 `tx8-transform-simt-to-simd` 消费掉 RAISA/SIMT 内建，之后 ISel 匹配的是
**上游 RVV 基名**（`vcos.v` 等）。我方 `llvm/lib/Target/RISCV` 内
`RAISA` 命中 0、`simt2simd` 命中 0 ⇒ pass 整体缺失，内建直落 backend ⇒
`RISCVISelLowering.cpp:14610`（`ReplaceNodeResults → ISD::INTRINSIC_WO_CHAIN` 的
`default: llvm_unreachable("Don't know how to custom type legalize this intrinsic!")`）→ SIGABRT。

**反向铁证（决定"不要给标量内建补 lowering"）**：在非设备的纯 `.c` 上下文，**厂商自己也失败**：
`fatal error: error in backend: Cannot select: intrinsic %llvm.riscv.raisa.{cos,sin,reduce.sum,fdiv,…}`
（本轮实测 55 格 `builtin_*` 全部如此，且 30 格双方诊断逐字相同）。厂商的
`llvm.riscv.raisa.*` **本来就没有 ISel pattern**；替它发明一个只会偏离厂商。

**被 `simt2simd` 消费的内建（call-site 级，46 条，逐格可查 `recon/simt2simd/consumed.json`）**：

| 内建 | 格数 | 内建 | 格数 |
| --- | --- | --- | --- |
| `llvm.riscv.read.reg.tid.x` | 101 | `llvm.riscv.raisa.{cos,sin,log2,frsqrt,exp2}` ×{f16,f32,bf16} | 各 1 |
| `llvm.riscv.get.pred.mask` | 3 | `llvm.riscv.raisa.{fdiv,fmax,fmin}` ×3 | 各 1 |
| `llvm.riscv.read.reg.{ctaid,nctaid,ntid}.{x,y,z}` | 各 1 | `llvm.riscv.raisa.{reduce.sum,reduce.fmax,reduce.fmin}` ×3 | 各 1 |
| `llvm.riscv.read.reg.tid.{y,z}` | 各 1 | `llvm.riscv.raisa.{max,min,umax,umin,div}` | 各 1 |

映射结果（厂商 dump，例）：`raisa.cos.f32` → `llvm.riscv.vcos.nxv2f32.i64(undef, v, 7, 128)`；
`read.reg.tid.x` → `llvm.riscv.vid.nxv2i32.i64`；并伴随 `llvm.riscv.set.mem.alloc(i64 512)`。
幸存（有 ISel）的内建全部是 `llvm.riscv.v*`（`vle/vse/vadd/vfmax/vfredusum/vslh/vluxei.sync…`）。

---

## 4. 厂商自身失败清单（66 格，**不是**我们的缺口）

- `builtin_*` 55 格（`.c`，纯 `__builtin_riscv_*`/`__raisa_*`）：厂商
  `Cannot select: intrinsic %llvm.riscv.raisa.*`、`RISCVISD::BREV8`、少量 `argument … must be a constant integer`。
  其中 8 个 `builtin_*_{and,andn,nand,nor,or,orn,xnor,xor}_s` **hang**（厂商自己的 round2 日志就是 exit 124）。
- `mem_*` 9 格：厂商 `Broken function found` / SIGSEGV（`mem_l0ashared_u8_uniform` 厂商 crash 并打印
  `print-after=rpu-lower-shared` 的 IR dump）。
- `id_tid_y` / `id_tid_z`：厂商 SIGSEGV。
这 66 格在表里是 `vendor-fail-too` / `vendor-fail-same-diag`；**任何"补齐后应该通过"的预期都不适用于它们**。

---

## 5. R1'（`simt2simd` 重建）实现规模估计

| 证据 | 数值 |
| --- | --- |
| 厂商 TU `TX8TransformSIMTToSIMD.cpp.o` | 352 328 B / 176 函数（对照：已重建的 `TX8InsertXFRM` 17.8 KB/17、`TX8RegisterRename` 71 KB/41；`TX8SchedStrategy` 47 KB/49 + 8552 行 `.td`） |
| 需要复现的语义 | 46 条内建的映射（已从 pass dump 抽出）+ SIMT→SIMD 循环变换 + AVL/LMUL/policy 常量 + `set_mem_alloc`/`vsetsimt` 次序 |
| 语料分层 | `mathdev` 单基本块逐元素 53 格（易）；`mem/alloc/control` 28 格需要分歧/共享内存/CT 整理（难，另有 §2.2、§2.3 两个 pass） |

**估计（含验收）**：

- **R1'-a**：`read.reg` + 逐元素 `raisa.*` 子集，拿到**第一个逐字节一致的设备对象**并扩到 `mathdev` 53 格 → **1–2 个会话**。
- **R1'-b**：`mem/alloc/control` + `TX8LowerSharedPass` + `TX8LegalizeCTInstruction`/config 去重 → **再 2–3 个会话**。
- **§2.1 的 RAISA 目标层**（datalayout/地址空间/`rpu_kernel` CC）→ **0.3–0.5 个会话**（小，但必须逐字对齐，且它会影响**所有** `.rc` 的指针算术）。
- **合计到"厂商能编的 `.rc` 全部逐字节一致"：4–6 个会话**（不含 cmodel 逐位与 ctest 回归）。
- **风险**：`simt2simd` 的 176 个函数里含大量内联的 LLVM/IRBuilder 模板，反汇编"按符号重建结构"可行，
  但决策逻辑（何时向量化、何时放弃）必须靠 pass dump 逐格对齐；建议以 `mathdev` 的
  `-print-after=simt2simd` 输出**当单元测试**逐步推进。

---

## 6. 遗留与建议

1. **验收构建应改为无断言 Release**（否则 `.ll` 文本永远差 `-discard-value-names`；`.s`/`.o` 不受影响）。
2. 建议把 R1' 拆成可验收的两个里程碑：M1 = `mathdev` 53 格逐字节一致 + 1 格 cmodel 逐位；
   M2 = `.rc` 全量一致 + musl 零回退 + ctest。
3. `simt2simd` 落地前，**不要**给 `llvm.riscv.raisa.*` 补标量 ISel（会偏离厂商，见 §3 反向铁证）。
4. 本轮未产生任何源码改动，故 `wt-raisa-be/patches/` 为空。

## 附：本目录工具与产物

| 路径 | 说明 |
| --- | --- |
| `tools/prep.sh` `tools/logfix.py` | 从 `wt-xfrm/build` 派生构建目录（路径重写 + `.ninja_log` command hash 重算，使 `ninja -j2` 只重建 97 条边而不是 2437 条） |
| `tools/corpus_parity.py` | 317 格采集（复刻厂商 `run.py`） |
| `tools/vendor_pass_evidence.py` | 厂商 `simt2simd` before/after IR（可判定证据） |
| `tools/ours_pre_ir.py` | 我方 pre-backend IR 与待消费内建 |
| `tools/normcmp.py` `tools/round1_attrib.py` | 归一化比较 + 归因，产出 `parity/round1_parity.tsv` |
| `parity/round1_parity.tsv` | **机器可读主表**（cell/verdict/vendor_rc/ours_rc/vendor_diag/ours_diag/block/evidence_path） |
| `parity/diffs/<cell>.diff` | 25 个 `byte-diff` 格的归一化 diff |
| `recon/simt2simd/<cell>.{before,after}.ir` | 厂商 pass 证据（106 格） |
| `recon/simt2simd/mapping.tsv` | **R1' 的规格**：逐格 `consumed` 内建 → `produced` RVV 内建 + 实参常量（101 格、290 个不同 (输入→输出) 组合） |
| `recon/ours_ir/<cell>.ll` | 我方 pre-backend IR（111 格） |

---

## R1'-a 第 3 轮：`simt2simd` 最小版落地（补丁 0003）+ "三处差异全在下游"的证伪

**已落地**：`wt-raisa-be/patches/0003-reconstruct-simt2simd.patch`（610 行）。新 pass
`TX8TransformSIMTToSIMD.cpp`（pass 名 `simt2simd`，ModulePass，`isRaisa()` 门控，
`RISCVTargetMachine::addIRPasses` 末尾挂载）。语义全部取自厂商 dump：kernel 签名打包
（`gep i8,%0,i32 8*i` + `load ptr`）、tid-indexed 标量 load/store → `vle/vse`（AVL=128，
`<vscale x 2 x T>`）、每个向量访存前 `set.mem.alloc(128*sizeof(T))`、
`constrained.fadd` → `vfadd`（frm 由 `round.*` 元数据映射 0/1/2/3）、`raisa.fmax` → `vfmax`。
**设计要点：改写前先"干跑"校验（模拟每个值是否会被映射），不满足就整体不动** —— 避免"改了一半"
的假绿；实现在拦住了 3 次（取参 load 被误当 lane load、metadata 操作数、擦除顺序）。

**准入门槛（第 3 轮）**：musl A/B **56/56 `.o` 逐字节相同**；317 格复测
`byte-identical 132 / OURS-GAP 93→77 / byte-diff 25→41 / vendor-fail 66 / 额外成功 1`，
`gaps by language = {raisa: 77}` ⇒ **143 个 `.c` 仍零缺口**。归因：`R1'/simt2simd 90→74`、
`R2/fork-pass(codegen) 8→24`（16 个 `.rc` 格从"崩在 14610"变成"编译成功、字节不同"）。
证据：`wt-raisa-be/logs/gate_round3.log`、`parity/round5_simt2simd.tsv`。

**证伪记录（重要）**：曾推测 `vsetvli a4,zero,…,ta,mu` 与 `vsetsimt 1` 的差异源于 `simt2simd`
构造的 AVL/policy 不同。回厂商 `after.ir` **逐字核对后证伪**：
`vle(undef, ptr, i64 128)`、`vfadd(undef, a, b, i64 0, i64 128)`、`vse(v, ptr, i64 128)`
—— AVL 就是 `ConstantInt 128`、passthru 就是 `undef`，厂商 IR 里**没有任何 SIMT 标记**，
解包顺序 `0/8/16` 与我方一致。⇒ 三处 asm 差异（`vsetsimt`、vsetvli 形态、`ld` 顺序）**全部在
`simt2simd` 下游**，不属本 pass 职责。

**下游归属（REA 定位，第 4 轮）**：`vsetsimt` 发射者 = **`RISCVInsertVSETVLI.cpp`**（厂商
`libLLVMRISCVCodeGen.a` 全部成员中只有它命中 `vsetsimt|VSET_SIMT`；fork-only 符号
`EnableSIMTMul`、选项 `-riscv-insert-vsetvli-enable-simtmul`、
`IsInstrDemandMaskPolicyUnderSIMT`）。厂商 MIR 铁证
（`-mllvm -print-after=riscv-insert-vsetvli`，存 `wt-raisa-be/logs/vendor_mir_vsetvli.txt`）：

```
120B  dead %11:gprnox0 = PseudoVSETVLIX0 killed $x0, 80, implicit-def $vl, implicit-def $vtype
```

即 **AVL = x0（VLMAX，不物化常量）+ vtype 立即数 80（e32,m1,ta,mu）**；我方是 `li a4,128` +
`PseudoVSETVLI a4`（`0031` 的守卫把常量 AVL 物化进 GPR）。
⇒ 下一步 = **移植厂商 `RISCVInsertVSETVLI` 的 fork 改动**（SIMT 下 `PseudoVSETVLIX0` +
`vsetsimt(mul)` 发射 + `IsInstrDemandMaskPolicyUnderSIMT` 判定），按 `xtsm82s` 门控；
同时把 `simt2simd` 的映射表从 2 个扩到 `spec.tsv` 的其余家族。

---

## R1'-a 第 6/7 轮：`RISCVInsertVSETVLI` 的 SIMT 分支——判据、行为反证、落点与待定项

**厂商判据（`tools/re/out/RISCVInsertVSETVLI/decompiled.c:8082-8086`，`runOnMachineFunction`）**
```c
if ((subtarget[0x21b] == 0) || (subtarget[0x34] != 0x2a))  SIMTMode(this[0xc0]) = 0;
else                                                       SIMTMode = 1;
```
即 **`SIMTMode = (subtarget+0x21b 的 bool != 0) && (subtarget+0x34 的 int == 42)`**，per-function 开关。
`+0x21b` 这个缓存 bool 在 `RISCVInsertReadWriteCSR` 里也被读（我们树那里读的是 `ST.hasVendorXTsm82s()`）。

**行为反证（关键，防止把 `hasVendorXTsm82s` 当 SIMT 判据）**：厂商 clang 编**普通 C**
（musl + `-mcpu=tsm-x82`，`-O2 -fno-vectorize -fno-slp-vectorize`，`wt-isel/probe/avl100.c`）：
```
vsetsimt 出现次数 = 0 ;  vsetvli 形态 = vsetvli zero, a3, e32, m1, ta, ma   （寄存器 AVL + ma）
```
而 RAISA 设备格 4/4 是 `VSET_SIMT 1` + `PseudoVSETVLIX0 killed $x0, 80`（`ta,mu`）。
⇒ **普通 xtsm82s 明确不走 SIMT 路径**；SIMT 是 **RAISA 设备路径**特有。

**成员名未定（如实）**：我们树 `llvm/lib/Target/RISCV/RISCVSubtarget.h` 里
`grep -i 'raisa|simt|tsrpu'` = **0 命中**，`RISCVFeatures.td` 也只有 "TSM X82 extension" 注释，
没有对应 feature 名 ⇒ `+0x21b` / `+0x34==0x2a` 目前**无法直接映射**到具体成员。
待定项（不阻塞实现）：反编译厂商 `RISCVSubtarget.cpp.o`（`tools/re/ar_probe/` 已解包）搜"写这两个偏移"的地方。

**第 7 轮的等价实现条件（写入文件头注明厂商判据原文）**：
`SIMTMode = ST.hasVendorXTsm82s() && (F.getCallingConv() == CallingConv::RPU_KERNEL)`
—— 依据：RAISA 设备内核是唯一带 `RPU_KERNEL` 约定的函数（`recon/simt2simd/*.before.ir` 里 106 格 `.rc`
全是 `define dso_local rpu_kernel`），而普通 C（反证里那格）是 CC_C ⇒ 与厂商行为一致。
风险与验证方式：若厂商对该字段的定义比 `RPU_KERNEL` 更宽（例如也覆盖非 kernel 的 `__device__` 函数），
需用 `raisa-ne-intrinsic` 那类"只有 `__device__` 函数"的格做 MIR 反证；未验证前按上面条件落地并在注释里标注。

**实现落点（已验证的行号，全部来自本树）**：`RISCVInsertVSETVLI.cpp`
`getInfoForVSETVLI @940`、**`0031` 守卫 @1092**（`hasAVLImm() && hasVendorXTsm82s()` → `ADDI`+`PseudoVSETVLI`）、
非 SIMT `PseudoVSETIVLI @1118`、`PseudoVSETVLIX0(AVL=x0) @1129`。
**SIMT 分支必须插在 1092 之前**，且非 SIMT 的 xtsm82s 继续走 1092（守住 `0031`）。

**验收 oracle（本轮的 MIR 证据命令）**
```bash
llvm-toolchain/bin/clang -cc1 -triple riscv64-unknown-raisa -target-cpu tsm-x82 \
  -target-feature +xtsm82s -fraisa-is-device -x raisa -O2 -S \
  -mllvm -print-after=riscv-insert-vsetvli <cell>.rc -o /dev/null
# 期望（4/4 格实测）：8B VSET_SIMT 1, implicit-def $vl, implicit-def $vtype
#                      120B dead %11:gprnox0 = PseudoVSETVLIX0 killed $x0, 80, ...
```
证据文件：`wt-raisa-be/logs/vendor_mir_vsetvli_4cells.txt`（另 `vendor_mir_vsetvli.txt`）。
