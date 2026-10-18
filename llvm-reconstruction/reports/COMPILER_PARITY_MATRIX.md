# TX82 编译器产物级 parity 矩阵（厂商 clang ↔ 重建 clang）

- 口径：**只做编译/产物级**（无 cmodel 执行，执行级由另一 agent 负责）。每条编译都在
  `timeout -s KILL 60` + `ulimit -c 0` + `ulimit -f 256MiB` 下跑（厂商后端会 SIGSEGV/abort）。
- 原始材料：`wt-parity/raw/matrix.tsv`（260 cell × 2 side）、`wt-parity/raw/ext_matrix.tsv`
  （309 探针 × 2 side）、`wt-parity/raw/enc_matrix.tsv`（228 个 `.s` 的汇编器对拍）、
  判定汇总 `wt-parity/raw/summary.json`、每条命令与完整 stderr 在 `wt-parity/raw/cells/*.log`。
- 一条命令复现：`./wt-parity/run.sh`（`main` ≈3 min / `ext` ≈25 min / `analyze`）。
- 全程**只读**：未修改 `gemini-cmodel/`、`cmodel/`、`llvm-toolchain/`、
  `riscv-llvm-musl-toolchain/`、`wt-forkbuild/`；scratch 全在 `wt-parity/`（不用 `/tmp`）。

---

## 0. 结论速览

1. **重建 clang 在两条 triple 上都一条 fork pass 都没有**（`-mllvm -debug-pass=Structure`
   实测：musl 无 `tx8-asan/tx8-legalize-ct/tx8-insert-xfrm/vreg-align/tx8-profiling-kernelfunc`，
   raisa 无那 14 条）。`TX8_PASS_INVENTORY.md` §3 的"`.text`/`.rodata`/`.riscv.attributes` 完全相同"
   是**只在 `tx82_version.c`（无函数、0 条 `0x7b`）上**成立的结论；本轮把它扩展到另外 4 个
   函数级产物（`probe_device.c`、`gemm_module_entry.c`、2 个手写探针），**但它们同样一条 `0x7b`
   都没有**。⇒ **在真正承载 xtsm82s 编码（`0x7b`）的输入上，产物级 parity 至今未被证明过。**
2. 260 个 cell：**IDENTICAL 14 / DIFF 34 / OURS-ONLY-GAP 131 / PARITY 1 / PARITY-CRASH 4 /
   PARITY-DIAG-DIFF 76**。真实 kernel 配方（musl `-O0`）22 个语料里 **17 个 ours 编译失败**，
   其中 **12/14 个 gemini kernel 死在同一个内建**：`Cannot select: intrinsic %llvm.riscv.set.barrier`。
3. **唯一被证明的强 parity**：把两侧都缺的 10 个内建用同一份 `-include` 宏中和掉后，
   `tx82_flash_attn.c`（musl `-O0`）的 `.text` **1698 B 逐字节相同**，`.rodata.cst4`/`.rela.text`/
   `.riscv.attributes`/`.symtab`/`.strtab` 亦相同，只有 `.comment` 不同。
   ⇒ ISel + MC 编码 + `TX8MCInfo` + regalloc 在"不碰缺失内建"的部分是忠实的。
4. **三个 flagged 项**：①RAISA `-O2` SIGSEGV = **厂商 bug**（栈帧 #3 `Running pass
   'TX8 Transform SIMT To SIMD'`，4 cell `rc=-11`）；②musl `-O2` `convertRVVALUToCTEncoding`
   fatal = **厂商 bug**，且 ours 逐字节复现同一诊断（1 个 exact PARITY cell）；③RAISA triple
   `Cannot select %llvm.riscv.vid` = 厂商同样失败（**不是我们的缺口**），但 ours 死在**更早的
   `set.barrier`**，所以是"被掩盖"而非"已对齐"。
5. 最大真缺口不是 pass 层，而是 **XTsm 内建的选择/下降层整体缺失**：本树
   `RISCVInstrInfoXTsm82s.td` 有 413 个指令定义但 **`Pat<>` 数量 = 0**；
   `IntrinsicsRISCVXTsm.td` 的 43 个内建没有任何 ISel 通路。实测 **83 个内建**厂商能选、我们不能
   （`wt-parity` 全日志去重）。其中 **10 个**（`set.barrier` + 9 个 NE/DMA/sync）就能解锁全部 14 个 kernel。
6. pass 层在 kernel 配方下的净收益确实是 0（本轮独立佐证：musl `-O0` 上，把 10 个内建中和后
   产物与厂商逐字节一致，而 ours 没有任何 TX8 pass）；但 **`-O1` 起的 15 个 DIFF cell** 与
   **RAISA triple 的全部 19 个 DIFF cell** 都要靠 pass 层/调优层才能收敛。

---

## 1. 方法与口径

| 项 | 值 |
| --- | --- |
| 厂商 | `llvm-toolchain/bin/clang` `21.1.0-rc1 …b85caf26…`，另用 `llvm-mc/llvm-objdump/llvm-nm` 做中立读取 |
| 重建 | `wt-forkbuild/build/bin/clang`（同 base + 25 patch，`+assertions`，无 `opt/llc`） |
| 执行包装 | `ulimit -c 0; ulimit -f 262144; timeout -s KILL 60 <cmd>` |
| musl 配方 | `--target=riscv64-unknown-linux-musl --sysroot=… -mcpu=tsm-x82 -mabi=lp64f -std=c11 -fPIC -nostdlib -ffreestanding -nostdinc` |
| RAISA 配方（`.c`） | `--target=riscv64-tsingmicro-raisa -mcpu=tsm-x82 -std=c11 …` |
| RAISA 配方（`.rc`） | `--target=x86_64-unknown-linux-gnu -x raisa --raisa-rpu-arch=tsm-x82 -noraisainc -noraisalib -std=c++17 --raisa-device-only`（厂商自己的 `build.py`/`run.py` 口径） |
| config | `-O0`、`-O1`、`-O2`、`-O2 -fno-vectorize`、`-O3 -fno-vectorize` |
| 语料 | `gemini-cmodel/kernels/*.c`（14）+ `multicore-dispatch/probe_device.c` + `gemm-walkthrough/src/{gemm_rvv,gemm_module_entry}.c` + `raisa-ne-intrinsic/ne_builtin.c` + 4 个手写边界探针 + 8 个 `.rc` ⇒ 30 个语料 |
| 手写探针 | `wt-parity/src/tiny_{vec_fpreduce,fma_chain,ne_gemm,barrier}.c`（FP 归约 / FMA 链 / NE-GEMM 内建 / barrier） |
| 比较顺序 | `rc` → 诊断签名 → IR（归一化路径/module id）→ asm → `.text`/`.rodata`/`.riscv.attributes` 字节（`llvm-objdump -s`）→ `nm` 符号 → `.text` 里 `(w & 0x7f) == 0x7b` 的 32-bit 字数 |
| 头文件公平性 | 显式 `-isystem` 一律指向**该编译器自己的** `lib/clang/21/include`（否则 ours 的 `#include_next` 会命中自己的 resource dir，制造假差异；该陷阱已修，见 §8） |

---

## 2. 主矩阵（260 cell）

### 2.1 判定计数

| triple | IDENTICAL | DIFF | OURS-ONLY-GAP | PARITY | PARITY-CRASH | PARITY-DIAG-DIFF | 小计 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| musl（22 语料×5） | 9 | 15 | 76 | 1 | 0 | 9 | 110 |
| raisa（30 语料×5） | 5 | 19 | 55 | 0 | 4 | 67 | 150 |
| **合计** | **14** | **34** | **131** | **1** | **4** | **76** | **260** |

判定定义：`PARITY` = 双侧同 `rc` 同诊断；`PARITY-CRASH` = 双侧都失败且厂商是信号崩溃；
`PARITY-DIAG-DIFF` = 双侧都失败但根因不同；`OURS-ONLY-GAP` = 厂商成功、我们失败；
`VENDOR-ONLY` = **0 个**（没有一处是"厂商挂而我们能过"）；`DIFF` = 双成功但某些节不同。

### 2.2 musl `-O0`（真实 kernel 配方）逐 cell

| 结果 | 语料 |
| --- | --- |
| IDENTICAL（5） | `k_version`、`d_probe_device`、`d_gemm_module_entry`、`x_fma_chain`、`x_vec_fpreduce` |
| OURS-ONLY-GAP（17） | 13 个 kernel（除 `k_version`）+ `d_gemm_rvv`、`d_ne_builtin`、`x_ne_gemm`、`x_barrier` |

失败根因（去重后，全矩阵 + 扩展普查）：

| 诊断（ours） | cell 数 | 直接原因 |
| --- | --- | --- |
| `Cannot select: intrinsic %llvm.riscv.set.barrier` | 62 | 缺 `__builtin_riscv_set_barrier` 的 ISel 下降（→ `vsetbarrier`） |
| `Cannot select: intrinsic %llvm.riscv.gemm.psum` | 20 | 缺 NE/GEMM 内建下降 |
| `error: this intrinsic with type 'int' is not supported by CT` | 20 | `SemaCT.cpp` 内建分类表缺 ~403 个 ID |
| `assert: AL.isTypeAttr() && "Non-type attribute not handled"` | 15 | `l0ashared/l0wshared/l1shared` 在 `Attr.td` 有定义但 `SemaDeclAttr.cpp` 无 case |
| `Unsupported RVV instruction with funct6=0b010111 funct3=0b011 vs1=vs2=0` | 5 | ours 前端仍是 stock RVV：对零 splat 发 `vmv.v.i` 而厂商发 `vmv.v.x`；MC 层（两侧一致，见 §5.3）拒绝该编码 |
| `Cannot select: intrinsic %llvm.riscv.ne.set.length` | 4 | 同 NE 家族 |

**关键补充**：`set.barrier` 之后还有第二、第三道同类阻塞。把 10 个内建用同一份
`-include` 宏中和后（两侧完全同源），**14 个 kernel 全部编译成功**，阻塞链为
`set.barrier → lsu.tdma.mem.copy → wait.sync → send.sync`（全员），
`gemm/gemv/swiglu` 再加 `ne.set.prec/misc/mkn/length/gemm/gemm.psum`，`flash_attn` 全部 9 个。
⇒ **P0 的验收判据很干净：实现这 10 个内建即可让 14/14 kernel 在 musl `-O0` 下编出产物。**

厂商 14 个 kernel 产物的 `0x7b` 目标字数（ours 目前一个也编不出，实测 = 0）：
`add 2, flash_attn 5, gelu 2, gemm 3, gemv 7, kv_cache 4, layernorm 2, mul 2, rmsnorm 2,
rope 2, sample 2, softmax 4, swiglu 7, version 0`，**合计 44 条**——这些 `0x7b` 恰恰来自
缺失的 NE/DMA 内建下降，所以 MC 编码 parity 只能在 P0 修完之后才能端到端测。

### 2.3 双成功 cell 的 10 个内建中和实验（唯一端到端"带 CT 意图"的比对）

| 语料 | `.text` | `.rodata.cst4` | `.rela.text` | `.riscv.attributes` | `.symtab`/`.strtab` | 0x7b |
| --- | --- | --- | --- | --- | --- | --- |
| `tx82_flash_attn.c`（中和 10 内建） | **1698 B 相同** | 24 B 相同 | 960 B 相同 | 207 B 相同 | 相同 | 0/0（内建被中和掉） |
| `tx82_add.c`（中和 4 内建） | **112 B 相同** | — | — | 207 B 相同 | 相同 | 0/0 |

复现（中和宏两侧同源，产物见 `wt-parity/raw/art/reduced_*.o`）：

```bash
for side in v o; do
  bin=llvm-toolchain/bin/clang;    inc=llvm-toolchain/lib/clang/21/include
  [ $side = o ] && { bin=wt-forkbuild/build/bin/clang; inc=wt-forkbuild/build/lib/clang/21/include; }
  $bin --target=riscv64-unknown-linux-musl --sysroot=$PWD/llvm-toolchain/sysroot \
       -mcpu=tsm-x82 -mabi=lp64f -std=c11 -fPIC -nostdlib -ffreestanding -nostdinc \
       -I gemini-cmodel/kernels -isystem $inc -O0 \
       -include wt-parity/src/nb_stub_flash_attn.h \
       -c gemini-cmodel/kernels/tx82_flash_attn.c -o wt-parity/raw/art/reduced_flash_attn_$side.o
done
```

---

## 3. 三个 flagged 项裁决

### 3.1 RAISA triple `-O2` SIGSEGV in `TX8 Transform SIMT To SIMD` ⇒ **厂商 bug（PARITY-CRASH 4 cell）**

厂商 stderr（`wt-parity/raw/cells/x_vec_fpreduce__O2__raisa_vendor.log`）：

```
PLEASE submit a bug report …
0. Program arguments: … clang --target=riscv64-tsingmicro-raisa -mcpu=tsm-x82 … -O2 -c tiny_vec_fpreduce.c
1. <eof> parser at end of file
2. Code generation
3. Running pass 'TX8 Transform SIMT To SIMD' on module '…/tiny_vec_fpreduce.c'
timeout: 被监视的命令已核心转储          # rc = -11
```

`k_layernorm/k_rmsnorm/k_softmax/x_vec_fpreduce` 是干净崩溃；`k_gemm/k_swiglu` 是
前端先报 `error: Intrinsic used in a wrong way, please recompile this file with "-g"` 再崩。
⇒ 厂商 RAISA `-O2` 路径**不可用**，不是我们的缺口（我们连 pass 都没有，自然不会在那里崩）。

### 3.2 musl `-O2` `TX8MCInfo::convertRVVALUToCTEncoding` fatal ⇒ **厂商 bug，且 ours 逐字节复现（PARITY 1 + PARITY-DIAG-DIFF 9）**

| cell | vendor | ours |
| --- | --- | --- |
| `x_vec_fpreduce -O2`（musl） | `fatal error: error in backend: Unsupported RVV instruction with funct6=0b000011 funct3=0b001 vs1=0b01000 vs2=0b01010 vm=0b1` | **逐字节同一行**（该编码 = `vfredosum.vs v0, v10, v8`） |
| `k_flash_attn/k_gelu/k_gemm/k_layernorm/k_rmsnorm/k_sample/k_softmax/k_swiglu/k_gemv -O2` | 同上或 `SEW must be 8 for VFWCVT_F_X_V` | ours 更早死在 `set.barrier`（根因不同、结论同类） |

⇒ 厂商的 RVV→CT 换码器**本身就不完整**（拒绝 `vfredosum.vs`/`vfmadd.vf`/`vmv.v.i`），
而**我们的 `TX8MCInfo` 是忠实的**：同一个输入给出同一 fatal。这是重建质量最强的一个正面证据。

### 3.3 RAISA triple + gemini kernel `Cannot select: intrinsic %llvm.riscv.vid` ⇒ **不是我们的缺口，但我们也还没对齐**

- 厂商：12/14 kernel 报 `%llvm.riscv.vid`，2/14 报 `error: Intrinsic used in a wrong way` ⇒ **厂商在 RAISA triple 下编不了这批 kernel**。
- ours：同样 `rc=1`，但诊断是 `%llvm.riscv.set.barrier`（更早）。
- ⇒ 对"厂商能不能编"这件事是 PARITY（都不能）；对"ours 是否也缺 `vid` 下降"**未测**（被掩盖）。
  修完 P0 后必须重测这一格；在那之前不要把它记成"已对齐"。

---

## 4. 字节差汇总（48 个双成功 cell）

- **34 个 DIFF cell 全部只差 `.text`**；`.rodata` 与 `.riscv.attributes` 在 48/48 个双成功 cell 中**完全相同**；
  `nm` 的**符号名/类型集合 48/48 相同**（14 个 cell 只是地址随 `.text` 长度平移）。
- 三类根因（按 triple 分层）：

| 类别 | cell | 现象（`.text` vendor → ours，first-diff 偏移） | 根因 |
| --- | --- | --- | --- |
| musl `-O1` | 5 | 84→84、88→88、92→92，首差在字节 6/8 | 纯寄存器分配选择差异（如 `sd s3` vs `sd s2`），指令数不变 — 厂商 `vreg-align`/`tx8-legalize-ct` 在 `-O1` 不再是无操作（P1） |
| musl `-O2/-O3` 未展开 | 6 | `x_vec_fpreduce -O2 -fno-vectorize` **550 → 86**；`x_fma_chain -O2` **486 → 134**；`O3` 626 → 86 | **`-mcpu=tsm-x82` 的循环展开调优缺失**：`-mllvm -debug-only=loop-unroll` 显示 ours `will not try to unroll loop with runtime trip count -unroll-runtime not given`，即 `RISCVSubtarget::enableDefaultUnroll()` 在 ours 的 `tsm-x82` 上为 true。同一份源码在 stock clang `-march=rv64gc` 下也是 86 B（= ours），而厂商 `-mcpu=sifive-u74` 552 B / `sifive-p670` 556 B（= 同样展开）⇒ 是 **TSM_X82 的 tune feature（`TuneNoDefaultUnroll` 之类）在我们重建的 `RISCVProcessors.td` 定义里漏了**，不是 pass 问题。修法≈1 行 + 对拍 |
| musl `-O2`（vendor 崩） | 9（PARITY-DIAG-DIFF）+1（PARITY） | — | §3.2 |
| raisa 全部 | 19 | vendor 822–2418 B、`0x7b` 66–279；ours 84–236 B、`0x7b` 0 | 缺 14 条 RAISA pass（首要 `simt2simd`）：厂商发 `vsetsimt/vmv2r.v/vid.v/mv128.*`+.variant_cc，ours 出纯标量码 |

---

## 5. 两条正交的扩展普查

### 5.1 round2 全量探针（309 probe × 2 side，`-O0`，选项取厂商自己 `manifest.json`）

| 组 | n | both-ok | OURS-GAP | vendor-only | both-fail |
| --- | --- | --- | --- | --- | --- |
| catalogue（内建全集） | 143 | 0 | 88 | 0 | 55 |
| mathdevice（RAISA 数学） | 50 | 0 | 50 | 0 | 0 |
| immediate | 41 | 0 | 41 | 0 | 0 |
| memory | 21 | 0 | 12 | 0 | 9 |
| control | 17 | 0 | 15 | 0 | 2 |
| config | 18 | 0 | 17 | 0 | 1 |
| abi | 12 | 0 | 8 | 0 | 4 |
| allocation | 7 | 0 | 7 | 0 | 0 |
| **合计** | **309** | **0** | **238** | **0** | **71** |

**`vendor-only = 0`**：没有一处是"厂商编不过而我们能编"。71 个 both-fail 对缺口判定无信息
（厂商自己在这套 `-O0` 口径下也挂：`Cannot select` s128 store / `RISCVISD::BREAK` /
`riscv.raisa.*`，以及厂商自己的 `llvm-mc` 无法再汇编 44 个 fork 助记符）。

扩展普查的 OURS-GAP 诊断分类：CT 类型检查 73、属性断言 16、`unexpected builtin ID`(SIGABRT=134) 10，
其余为逐内建的 `Cannot select`。

### 5.2 缺失内建清单（厂商能选、我们不能，全日志去重 **83 个**）

高频：`set.barrier`(132 次)、`gemm.psum`(21)、`ne.set.length`(11)、`l1.flush`(11)、
`l0.invalid/l1.invalid/l0.flush/l1.sw.prefetch`(各 9)、`gemm`(7)。其余 70 余个为
`lsu.{rdma,wdma,tdma}.*`、`dte.*`、`kcore.*`、`send/recv/wait.*`、`conv/convbw/conv.psum`、
`mma/mma.first/mma.last`、`ne.set.*`、`vset2dv{load,store}{0,1}`、`direct.write` 等，
完整表见 §6 与 `wt-parity/raw/*.log`。

### 5.3 MC/汇编器层对拍（228 个 round2 `.s`，两侧 `llvm-mc --mcpu=tsm-x82 --show-encoding`）

| 结果 | 数 |
| --- | --- |
| 双侧成功 | 182 |
| 其中编码逐行相同（口径含 `.byte/.short/.word` 行） | 128 |
| 其中**有指令 `encoding:` 行不同** | 75 个文件 / 96 行（口径只比指令行，故比 182−128=54 更严） |
| ours 单独失败 | **0** |
| vendor 单独失败 | **0** |
| 双侧同样失败（失败集合完全一致） | 46 |

96 行差异**全部是 `vsetvli`**（84 行 `ta, mu`、12 行 `ta, ma`），且**只差一个 bit**：
word bit29（`vtypei[9]`，79 行，对应 `mu`）、word bit30（`vtypei[10]`，17 行，对应 `ma`）。
厂商自己的 `RISCVInstPrinter.cpp` 补丁注释写明："Its assembler still writes bits 10 and 9 for the
forms whose rd/rs1 is x0, but its disassembler ignores them" ⇒ 组装器路径的保留位怪癖。
但厂商 **codegen** 产物里也出现 `vtypei[9]`（`d_gemm_rvv.o` 11 条 `0x7b` 中 2 条），
所以这条在 P0 修完后必须重测（见 §6 R2），不能简单归为 cosmetic。

---

## 6. 真实缺口优先级列表（含成因与规模）

规模沿用 `TX8_PASS_INVENTORY.md`：S≈0.5 会话、M≈1.5、L≈4。

| # | 优先级 | 缺口 | 成因（已定位到文件/符号级） | 证据 | 规模 |
| --- | --- | --- | --- | --- | --- |
| **G1** | **P0** | XTsm 内建 ISel/下降层整体缺失：厂商能选 **83** 个内建，我们 0 个 | `RISCVInstrInfoXTsm82s.td` 有 413 个 `def` 但 **`Pat<>` 计数 = 0**；`IntrinsicsRISCVXTsm.td` 43 个内建无任何 `case Intrinsic::riscv_*` / Pattern 通路。厂商 ISel 产出的是**另一套 pseudo 名**（MIR 实测 `VSET_BARRIER`、`Ne_Set_Prec`），这些名字在我们树和所有 recovered 材料里都**不存在** ⇒ 需要重建 pseudo + pattern，而不是改名 | §2.2、§5.2、`wt-parity/raw/cells/*ours.log` | **先做 10 个（解锁 14/14 kernel）＝M；83 个全做 + 对拍＝L** |
| **G2** | **P0** | `SemaCT.cpp` 内建分类表缺 ~403 个 ID ⇒ RAISA/CT 前端直接拒绝一切 CT 内建 | `SemaCT.cpp` 自己的 provenance 注释："888 of the 1291 IDs are reproduced here; the remaining ~403 fall through to `default:`"，default 即 `err_tsm_unsupported_ct_intrinsic_with_type` | 20 matrix cell + 73 ext cell | **M**（机械，需补 5 张跳转表的剩余目标） |
| **G3** | **P0** | RAISA 地址空间属性 `l0ashared/l0wshared/l1shared` 无法使用（`clang` 直接 assert） | `Attr.td` 有 `RAISAL0AShared/RAISAL1Shared/RAISAL0WShared`，但 `SemaDeclAttr.cpp` 无对应 case，落到 `ProcessDeclAttribute` 的 `AL.isTypeAttr()` 断言；`tracked-modified.diff` 里也**没有** SemaDeclAttr 的 delta | 15 matrix + 16 ext cell | **S**（补 case）；若地址空间下沉也要做则 +M |
| **G4** | **P0** | `__raisa_*` 设备数学内建没有 CodeGen | `clang/lib/CodeGen/TargetBuiltins/RISCV.cpp:195` `UNREACHABLE("unexpected builtin ID")`，rc=134（SIGABRT）；`BuiltinsRISCV.td` 已声明（含 `fadd_rn/ru/rz/rd`、`exp2/log2/sin/cos/div/frsqrt/fmax/fmin/reduce_*`、`umax/umin`） | 10 ext cell | **S–M**（机械：枚举→intrinsic→下降） |
| **G5** | **P1** | 普通 RVV C 触发 `vsetivli is not supported by TX8`（厂商同一输入出 `vsetvli`） | ours ISel 选 `PseudoVSETIVLI`，厂商选 `PseudoVSETVLI`；该 fatal 字符串在**厂商二进制里同样存在** ⇒ 差异在 MC 之前（候选：缺 `tx8-insert-xfrm` 重写，或 subtarget/ISel 钩子） | `wt-parity/src/tiny_vid.c` | **M**（需先定位钩子；这是被 inventory 漏掉的阻塞项） |
| **G6** | **P1** | 5 条（musl）/ 14 条（RAISA）fork pass 全部缺失 | `-mllvm -debug-pass=Structure` 实测 ours 0 条；其余 agent 已证明它们在 kernel 配方 `-O0` 下是 MIR 逐字节 no-op，但 (a) RAISA triple 全部 DIFF、(b) musl `-O1+` 15 个 DIFF（regalloc 选择）都由它们造成 | §2.1、§4 | **L**（但对 kernel 配方 byte-parity 收益为 0；先做 `vreg-align` 与 `simt2simd` 收益最大） |
| **G7** | **P1** | 前端 RVV 仍是 stock：ours 构建出的 `lib/clang/21/include/riscv_vector.h` = **422 行**，厂商 **533 行**（缺 `vfloat8e4m3*/vfloat8e5m2*/vfloat4*` 别名、`fp4/f8e4m3/f8e5m2`、`__RISCV_SATURATION`、`__s128_t`） | 该头由 `clang-tblgen -gen-riscv-vector-header` 从 `riscv_vector.td` 生成；`riscv_vector.td`/`_common.td`/`RISCVVEmitter.cpp` 都在厂商 tracked-modified 集里，而我们生成的产物内容更少 ⇒ 我们的 patch 在这三个文件上不等价 | 直接 `diff -rq` 两侧 resource dir（另：ours 多出 `hlsl.h`/`hlsl` 属 stock 自带，无关） | **S–M** |
| **G8** | **P2** | `-mcpu=tsm-x82` 的 **循环展开调优**缺失（musl `-O1+` 产物偏小 6 倍） | `RISCVSubtarget::enableDefaultUnroll()` 在 ours 的 `tsm-x82` 上为 true（→ 走 BasicTTI 默认、不展开）；厂商 `tsm-x82` 与 `sifive-u74`（552 B）/`sifive-p670`（556 B）行为一致 ⇒ 厂商的 `TSM_X82` 带 "no-default-unroll" 类 tune feature，而 `docs/compiler/fork-source/tracked-modified.diff` 里还原的 `TSM_X82` 定义只有 3 个参数（无 tune 列表）⇒ **快照里的 `RISCVProcessors.td` delta 与实际厂商不符**。验证：ours 加 `-mllvm -unroll-runtime` 后 86→386 B（方向一致） | §4 | **S**（1 行 + `-O1/-O2` 对拍） |
| **G9** | **P2** | `vsetvli` 的 `vtypei[10:9]` 保留位不写 | 组装器路径 96/182 行差一个 bit；厂商 printer 注释承认是它汇编器写的、反汇编器忽略；但厂商 codegen 产物里也有 `vtypei[9]` | §5.3 | **S–M**（先等 G1 落地再实测 codegen 是否真差） |
| **G10** | **P2** | `vmv.v.i` / 零 splat：ours 发 `PseudoVMV_V_I`，MC 层拒绝；厂商发 `vmv.v.x` | 两侧 MC 完全一致（`llvm-mc` 对 `vmv1r.v`/`vmv.v.i` 给同一 fatal）⇒ 差异在 ISel/内建层，属 G7 的一部分 | `d_gemm_rvv` 10 cell | **M** |
| **G11** | **P4** | RAISA triple 的 `%llvm.riscv.vid` | 厂商同样失败（厂商限制）⇒ **不做**；但 ours 修完 G1 后必须重测是否同样只差 `vid` | §3.3 | 0（仅重测） |
| **G12** | **P4** | 厂商自身缺陷（**不要跟**） | ① RAISA `-O2` `simt2simd` SIGSEGV；② musl `-O2` `convertRVVALUToCTEncoding` 拒绝 `vfredosum.vs`/`vfmadd.vf`/`vmv.v.i`；③ `-cc1 -triple riscv64-unknown-raisa` 下 `fexp2`/`strict_fp_*` 等 `Cannot select`；④ 46/228 个 `.s` 厂商自己汇编不回去 | §3.1、§3.2、§5.1、§5.3 | 0 |

**会话估算**

- **只求"14/14 kernel 在 musl `-O0` 编出产物并与厂商逐字节对拍"**：G1(10 内建) 1.5 + G8 0.5 +
  G9 实测 0.5 + G7 局部 0.5 ≈ **3 会话**（G3/G4 不改 kernel 却会挡住 `.rc` 与 NE 类语料）。
- **打通 RAISA/CT 设备路径**：再加 G2 1.5 + G3 0.5 + G4 1 + G5 1.5 + G1 余量 2.5 ≈ **+7 会话**。
- **完整 parity（含 pass 层与 -O1+ 调优层）**：再加 G6 4 + G7 全量 1 ≈ **总计 12–15 会话**。

---

## 7. 复现

```bash
cd /home/shaokai/Desktop/work/tx82
./wt-parity/run.sh main      # preflight + 260-cell 主矩阵 + summary.json（≈3 min）
./wt-parity/run.sh ext       # 309 个 round2 探针（≈25 min）
python3 wt-parity/enc.py     # 228 个 .s 的 MC/汇编器对拍
```

产物：`wt-parity/raw/{matrix,ext_matrix,enc_matrix,preflight}.tsv`、`summary.json`、
`cells/*.log`（每 cell 完整命令+stderr）、`art/*.{o,ll,s}` 与 `*.objdump.txt`/`*.nm.txt`、`ext/`、`enc/`。

---

## 8. 局限与未决

1. **没有一条真实 `0x7b` 端到端 parity 数据**：所有能产生 `0x7b` 的 C 输入都被 G1 挡在
   `Select()` 之前。目前只有 (a) 中和内建的逐字节相同（0 条 `0x7b`）、(b) 汇编器层对拍（§5.3）。
   P0 落地后必须重跑 §2.2 并逐节比对 44 条 `0x7b`。
2. `wt-forkbuild/build/bin` **只有 `clang/llvm-mc/llvm-objdump`，没有 `opt/llc`**，因此 MIR
   检查点只能用 `-mllvm -print-before/-after=<pass>`（`clang 21 -cc1` 无 `-stop-after`，实测
   `error: unknown argument: '-stop-after'`）。用厂商 `llc/opt` 处理我们的 IR 会造成口径污染，
   故未采用。
3. ours 是 `+assertions` 构建、厂商是 Release：assert 触发的 cell（如 G3）在 Release 下
   可能表现为 UB 而非崩溃，但结论方向不变。
4. 扩展普查的 RAISA `.rc` 组用 `-O0`（厂商自己的 round2 runner 用的是 `-O2`），所以 71 个
   both-fail 里有相当一部分是"该口径下厂商也过不了"，对缺口判定无信息；`vendor-only = 0`
   这一结论不受影响。
5. 头文件公平性陷阱已修但仍然值得记录：显式 `-isystem <厂商 resource dir>` 时，ours 的
   `#include_next <stdint.h>` 会命中它自己的 resource dir（`uint32_t` 找不到，制造出一个假
   OURS-ONLY-GAP）。最终口径是**每个编译器引自己的 resource dir**。
6. 「`set.barrier` 之后的阻塞链」是用**两侧同源的中和宏**测出来的（宏把调用变成 `0`），
   因此只证明"没有别的后端阻塞"，不构成产物 parity 证据。
