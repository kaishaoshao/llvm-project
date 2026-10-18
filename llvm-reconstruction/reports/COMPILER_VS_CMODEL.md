# 设备侧编译器 × 厂商 CModel 实测矩阵

日期：2026-10-16。**所有结论都是实测**（厂商 `libraisa_sim` CModel 真跑），不是文档推断。
复现命令见 `docs/compiler/CMODEL_EXECUTION_RECIPE.md`，脚本与日志在 `wt-cmodelrun/`。

被测的两个编译器：

| 记号 | 路径 | 说明 |
| --- | --- | --- |
| `vendor` | `llvm-toolchain/bin/clang-21` | 厂商发布的 fork 编译器（ground truth） |
| `recon` | `wt-forkbuild/build/bin/clang` | 我们从 fork 源码重建的编译器（clang 21.1.0-rc1） |

---

## 0. 结论（TL;DR）

1. **重建编译器在标量 FP 路径上已验证正确。** `-O0` / `-O1` / `-O2 -fno-vectorize`
   三档下，`recon` 与 `vendor` 的产物在厂商 CModel 上**输出逐位一致**，并且与 CPU 参考
   **逐位一致**（add 300/300 个 float；muladd 300/300，用"有理数精确 FMA 再一次舍入到
   float32"的参考）。
2. **`-O2` 自动向量化档在设备上不可用，而且不是重建的问题**：编译器把向量循环的
   元素数**硬写成 VLEN=256**（prologue 里 `li t0, 64` 字节/迭代、`li a4, 256`），
   而厂商 CModel 实测 **VLEN=4096**（`vsetvli e32m1` 的 VLMAX=128），于是
   `vl2re32.v` 一次搬 1024 B 而循环只前进 64 B → 越界 →
   `EXEC_ERR_LOAD_ADDR_OVERFLOW` + SIGABRT。vendor 与 recon **同样崩**。
3. **`vfmacc.vv` / `vfredosum.vs` 这类指令 fork 无法重编码**，编译期直接
   `fatal error: error in backend: Unsupported RVV instruction ...`；
   vendor 与 recon 报出**完全相同的 funct6/funct3**（说明重建忠实复现了这张表）。
4. **TX8 专有内建（TDMA / barrier / sync / GEMM / NE）重建编译器全部无法选择**：
   `Cannot select: intrinsic %llvm.riscv.{set.barrier, wait.sync, send.sync,
   lsu.tdma.mem.copy, gemm, gemm.psum, ne.set.mkn}`；vendor 全部成功。
   根因定位到 fork 补丁没有落到重建源码树（见 §4）。
5. 所以**设备侧基线不能从 `-O0` 放宽到 `-O2`（带向量化）**；可以放宽到
   `-O1` 或 `-O2 -fno-vectorize`——这两档在本矩阵里与 `-O0` 一样正确。

---

## 1. 矩阵

语料：`add` = `y[i]=a[i]+b[i]`；`muladd` = `y[i]=a[i]*b[i]+c[i]`；
`red` = `y[0]=Σa[i]`；`gem_add`/`gem_mul` = `gemini-cmodel/kernels/tx82_{add,mul}.c`
（含 TDMA/barrier/sync 内建）。n=300，`+xtsm82x` 单核单 tile，`PG_1DIE_8TILE`。
`7B` 列 = `.text` 里 `(word & 0x7f) == 0x7B` 的 32 位字数。

| case | 档位 | vendor 7B | vendor 结果 | recon 7B | recon 结果 | 两边输出逐位一致 | vs CPU 参考 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| add | `-O0` | 0 | RAN | 0 | RAN | ✅ | ✅ 300/300 |
| add | `-O1` | 0 | RAN | 0 | RAN | ✅ | ✅ 300/300 |
| add | `-O2 -fno-vectorize` | 0 | RAN | 0 | RAN | ✅ | ✅ 300/300 |
| add | `-O2` | 11 | **CRASH** | 11 | **CRASH** | – | – |
| add | `-O2 -ffp-contract=off` | 11 | **CRASH** | 11 | **CRASH** | – | – |
| muladd | `-O0` | 0 | RAN | 0 | RAN | ✅ | ✅ 300/300（精确 FMA 参考） |
| muladd | `-O1` | 0 | RAN | 0 | RAN | ✅ | ✅ 300/300 |
| muladd | `-O2 -fno-vectorize` | 0 | RAN | 0 | RAN | ✅ | ✅ 300/300 |
| muladd | `-O2` | – | **BUILD_FAIL** | – | **BUILD_FAIL** | – | – |
| muladd | `-O2 -ffp-contract=off` | 15 | **CRASH** | 15 | **CRASH** | – | – |
| red | `-O0` | 0 | RAN | 0 | RAN | ✅ | ✅ 1/1（double 参考，误差 6.3e-6） |
| red | `-O1` | 0 | RAN | 0 | RAN | ✅ | ✅ 同上 |
| red | `-O2 -fno-vectorize` | 0 | RAN | 0 | RAN | ✅ | ✅ 同上 |
| red | `-O2` | – | **BUILD_FAIL** | – | **BUILD_FAIL** | – | – |
| red | `-O2 -ffp-contract=off` | – | **BUILD_FAIL** | – | **BUILD_FAIL** | – | – |
| gem_add | `-O0`/`-O1`/`-O2 -fno-vectorize` | 4 | **CRASH** | – | **BUILD_FAIL** | – | – |
| gem_add | `-O2` / `-O2 -ffp-contract=off` | 27 | **CRASH** | – | **BUILD_FAIL** | – | – |
| gem_mul | `-O0`/`-O1`/`-O2 -fno-vectorize` | 4 | **CRASH** | – | **BUILD_FAIL** | – | – |
| gem_mul | `-O2` / `-O2 -ffp-contract=off` | 27 | **CRASH** | – | **BUILD_FAIL** | – | – |

原始数据：`wt-cmodelrun/out/matrix.json`；逐格日志：`logs/matrix.log`、`logs/cmodel.log`、
`logs/compare.log`；汇总：`logs/matrix.stdout`。

---

## 2. 逐条证据

### 2.1 重建编译器在标量档与厂商逐位一致

```
CELL add.vendor.O0      status=RAN 7b=0 out_sum=993cb7e5
CELL add.recon.O0       status=RAN 7b=0 out_sum=993cb7e5
CMP  add.O0  rc=0 bitwise_vs_other=True bitwise_vs_ref=True ref_words_differing=0 words_total=300
CMP  add.O1  rc=0 bitwise_vs_other=True bitwise_vs_ref=True ref_words_differing=0 words_total=300
CMP  add.O2novec rc=0 bitwise_vs_other=True bitwise_vs_ref=True ref_words_differing=0 words_total=300
```

`muladd` 用**精确参考**再验一次（有理数算出 `a*b+c` 再一次舍入到 float32）：
```
muladd.O0       vs exact-FMA: bitwise_vs_ref=True  ref_words_differing=0
muladd.O1       vs exact-FMA: bitwise_vs_ref=True  ref_words_differing=0
muladd.O2novec  vs exact-FMA: bitwise_vs_ref=True  ref_words_differing=0
muladd.O0       vs split mul+add: bitwise_vs_ref=False (58/300 个字不同)
```
最后一行同时证明：设备侧确实执行的是**融合**乘加（`fmadd.s`），不是先乘后加。

### 2.2 `-O2` 向量化档：VLEN 假设 256 vs 器件 4096

CModel 的向量长度（`wt-cmodelrun/device/case_entries.c` 的 `caps` 入口，
不读 `vlenb`——`csrr vlenb` 会让 CModel 静默挂死）：

```
CASE caps n=16 tag=4660 entry=tx82_entry_caps magic=7b7b0001 mode=1 vlmax=128
vsetvl VLMAX: e32m1=128 e32m2=256 e8m1=512 -> implied VLEN = 4096 bits
```

编译器生成的循环前导（`-O2`，vendor 与 recon 相同）：

```asm
li      a4, 256              # 每个向量块按 256 个元素切
li      t0, 64               # 每次迭代前进 64 字节 = 16 个 float
...
vsetvli a4, zero, e32, m2, ta, ma   # 运行时 vl 实际 = 2*4096/32 = 256
vl2re32.v v8, (t1)           # 一次搬 2*4096/8 = 1024 字节
```

`li t0, 64` 就是"假设 VLEN=256"的直接证据；在 VLEN=4096 的 CModel 上每次迭代多搬
960 字节，很快越界：

```
[SPIKE][FATAL][tx82_cgra_tensor.h:137] [CT][ERR][EXEC] level=2 topcode=0x4000
EXEC_ERR_LOAD_ADDR_OVERFLOW, opcode=0xa50c68 (VFL_W) scalar0=0x0 scalar1=0x4000003000
```

试过但没有效果的"对齐 VLEN"手段（都会退化成同样的前导）：
`-mllvm -riscv-v-vector-bits-min=4096`、`-mllvm -riscv-v-vector-bits-max=4096`、
`-Xclang -target-feature -Xclang +zvl4096b`。也就是说这个 256 是**硬写死的**，
和 `-mcpu=tsm-x82` 展开出的 `+zvl32b/+zvl64b` 一样改不动。
`cmodel_base.h` 也没有 VLEN 相关的配置项（只有 PG 模式 / tile 数 / device 数 /
TUFW 路径 / NE 模式），所以**这一档在当前 CModel 上无解**，除非改编译器或换 CModel 构建。

### 2.3 不可重编码的 RVV 指令：两个编译器报同一处

```
vendor case=muladd O2vec  :: funct6=0b101100 funct3=0b001 vs1=0b01000 vs2=0b01010 vm=0b1
recon  case=muladd O2vec  :: funct6=0b101100 funct3=0b001 vs1=0b01000 vs2=0b01010 vm=0b1
vendor case=red    O2vec  :: funct6=0b000011 funct3=0b001 ...
recon  case=red    O2vec  :: funct6=0b000011 funct3=0b001 ...
```

用原版 `llvm-mc` 还原这两条字（`funct6/funct3/vs1/vs2` 直接拼 32 位字再反汇编）：

* `funct6=0b101100 funct3=0b001` → **`vfmacc.vv`**（word `0xb2a41057`）
* `funct6=0b000011 funct3=0b001` → **`vfredosum.vs`**

两者都**不在** `docs/compiler/xtsm82s_opcodes.tsv` 的 172 条 `remapped-RVV` 里，
即属于 fork 无法重编码的类别。`-ffp-contract=off` 能把 `vfmacc` 拆成
`vfmul.vv`+`vfadd.vv`（muladd 的 `O2vec_nofma` 因此能编出来），但 `vfredosum.vs`
拆不掉（red 两档都 BUILD_FAIL）。

### 2.4 重建编译器缺失 TX8 内建的选择模式

`wt-cmodelrun/tools/builtin_probe.sh` 对 `gemini-cmodel/kernels` 用到的每个
`__builtin_riscv_*` 各编一个最小 TU：

| 内建 | vendor rc | recon rc | recon 报错 |
| --- | --- | --- | --- |
| `set_barrier` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.set.barrier` |
| `wait_sync` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.wait.sync` |
| `send_sync` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.send.sync` |
| `lsu_tdma_mem_copy` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.lsu.tdma.mem.copy` |
| `gemm` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.gemm` |
| `ne_set_mkn` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.ne.set.mkn` |
| `gemm_psum` | 0 | 1 | `Cannot select: intrinsic %llvm.riscv.gemm.psum` |
| `ne_set_prec` / `ne_set_length` / `ne_set_misc` | 1 | 1 | 探针参数个数写错（两边同样报 arity），未判定 |

**根因**（只读检查重建源码树，未修改）：

```bash
S=/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm/llvm
grep -c riscv_set_barrier  $S/include/llvm/IR/IntrinsicsRISCV.td      # → 0
grep -rln set_barrier      $S/lib/Target/RISCV/                        # → 无
grep -rln set_barrier      /home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/patches/  # → 命中
#   patches/llvm/0002-raisa-fork-new-files.patch
#   patches/llvm/0005-raisa-builtins.patch           ← 内建声明/模式在这里
#   patches/llvm/0019-raisa-sema-ct.patch
```

即：**内建声明与选择模式来自 fork 补丁，而这些补丁当前没有落在源码树上**
（`git branch: feature/raisa-xtsm-port`，`HEAD=51b450f`）。`wt-forkbuild/build/include/`
里生成的 `IntrinsicsRISCV.h` 里有这些枚举，说明构建时的源码状态与现在的工作树不一致。
重建树的 RVV→0x7B 编码路径是好的（§2.3 已证），缺的是**内建 → 指令的 ISel 模式**。

### 2.5 gemini TDMA kernel 在 CModel 上崩溃（vendor 也崩）

`gem_add` / `gem_mul` 用 vendor clang 能编出（4 条 0x7B），但在 CModel 上
**2 秒内 SIGABRT**，各档都一样。这不是重建的问题，而是这条路径当前在 CModel 上跑不通。
可查的下一条线索：`cmodel_base.h` 的 `setSpikeTUFWPath(const char*)` 默认为空
（`gemini-cmodel/src/core/cmodel_base.cpp:8`），设备固件（TUFW）没装载——
TDMA/NE 这类路径很可能需要它。**未验证**，留给下一步。

---

## 3. 尚未复现 / 未覆盖（说清楚边界）

1. **向量化档"跑通"没有做到**，而且用公开配置项做不到（§2.2）。所以"设备侧能不能用
   `-O2`"这个问题的答案是：**在本 CModel + 本编译器组合下不能**，因为两侧 VLEN 假设不一致。
   要变成能，需要 (a) 编译器按 4096 生成循环，或 (b) 一个 VLEN=256 的 CModel 构建。
2. **TX8 专有指令的数值正确性没有验证**：`gem_add`/`gem_mul` 在 CModel 上崩，vendor
   编译器也崩，所以这条链路只走到"能编译"（vendor）/"不能编译"（recon）。
3. **RAISA device-only 路径**（`--raisa-device-only` / `-fraisa-emit-relocatable`）没有做。
4. **0x7B 覆盖范围有限**：本矩阵实际只执行了各 kernel 用到的 11–27 条 0x7B 指令
   （add 11 条、muladd 15 条、gem_* 27 条），不是整个 172 条 `remapped-RVV` 空间。
5. `red` 只有 1 个输出元素，参考是 double 累加，**不能**逐位比对（规约顺序未定义）；
   `add`/`muladd` 是 300/300 逐位比对。
6. `ne_set_prec/length/misc` 三个探针的参数个数写错，未判定（不影响 §2.4 的结论）。

---

## 4. 复现

```bash
cd /home/shaokai/Desktop/work/tx82/wt-cmodelrun
source ../setup/activate.sh
python3 tools/run_matrix.py --out out/matrix.json      # 整张矩阵（每格一个进程 + timeout）
bash tools/builtin_probe.sh                            # §2.4 的内建探测表
```

单格：
```bash
python3 tools/run_matrix.py --cases add --levels O0,O2vec
# 或手工：build_case.sh → tx82_run_host.elf → compare.py
```

---

## 附：本次工作留下的另一条（已被放弃的）路径

在转向厂商 CModel 之前，本 session 先做完了"给开源 Spike 打 0x7B 反向映射补丁"这条路：
`wt-spiketx8/` 下有可构建的 spike 源码树 + `bin/spike-tx8`、171 条规则的
`spike-0x7b-remap.patch`、自检 337/337 通过、以及 3 个 kernel 的差分测试脚本。
它**不再作为主线**，仅作为"开源替代路径"的证据保留。

---

## 【parent 更正 · 2026-10-16】④ 的根因判断有误：**不是"构建目录与源码不同步"**

`a2631855` 报告里把"重建 clang 选不了任何 TX8 内建"的根因归为"`patches/llvm/000{2,5,19}` 没落在
`third_party/llvm/llvm`，而构建产物的 `IntrinsicsRISCV.h` 却有那些枚举 ⇒ 构建目录与工作树不同步"。
**parent 已逐条复核，该判断不成立**（证据）：

```bash
T=/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm
grep -c set_barrier $T/llvm/include/llvm/IR/IntrinsicsRISCV.td      # → 0   （查错了文件）
grep -c set_barrier $T/llvm/include/llvm/IR/IntrinsicsRISCVXTsm.td  # → 2   ← 定义就在这里
grep -n 'IntrinsicsRISCVXTsm' $T/llvm/include/llvm/IR/IntrinsicsRISCV.td  # → 1901: include "llvm/IR/IntrinsicsRISCVXTsm.td"
sed -n '157,158p' $T/llvm/include/llvm/IR/IntrinsicsRISCVXTsm.td
#   def int_riscv_set_barrier : DefaultAttrsIntrinsic<[], [], [IntrNoMem, IntrHasSideEffects]>,
#                               ClangBuiltin<"__builtin_riscv_set_barrier">;
grep -c set_barrier wt-forkbuild/build/include/llvm/IR/IntrinsicsRISCV.h    # → 1   与源码一致
grep -rl 'set_barrier\|SetBarrier' $T/llvm/lib/Target/RISCV/                  # → 空  ← 真因在这里
```

⇒ ① 内建声明**在树里**（`IntrinsicsRISCVXTsm.td:157`，被 `IntrinsicsRISCV.td:1901` include），
构建产物与源码**一致**，时间戳也正常；② 之所以在 `IntrinsicsRISCV.td` 和 `lib/Target/RISCV/` 里 grep 到 0，
是因为**查错了文件**（声明在 `IntrinsicsRISCVXTsm.td`）。

**真因**：`RISCVInstrInfoXTsm82s.td` 有 413 个 `def` 却 **0 个 `Pat<>`**、`lib/Target/RISCV/` 里
没有任何内建→指令的 ISel 模式 ⇒ 这**就是** `COMPILER_PARITY_MATRIX.md` 的 **G1**，也是当时正在修的
那条关键路径（`wt-isel/`）。**结论不变（内建选不了是 P0），但不要按"重放 0002/0005/0019 后重配重编"去修
—— 那不会解决问题**，必须补齐 ISel 降低。
