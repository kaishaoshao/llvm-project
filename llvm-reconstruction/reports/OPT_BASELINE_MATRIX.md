# TX82 设备内核优化基线矩阵 与 `-O2` 后端崩溃定位

全部结论为**实测**（编译 + `qemu-riscv64-static` 真跑 + 逐位比对），不接受推断。
原始证据（每条命令、每个 `.s`/`.o`/ELF、qemu 输出、比对结果）见
[`wt-optbase/`](../../wt-optbase/)，索引在 §6。

工具链：`llvm-toolchain/bin/clang-21` — `clang 21.1.0-rc1 (b85caf26…)`，
target `riscv64-unknown-linux-musl`，`-mcpu=tsm-x82`（隐式 `+xtsm82s`，ABI `lp64f`，VLEN=256）。
仿真器 `qemu-riscv64-static` 8.2.2。助记符反查用原版汇编器 `wt-forks/stock-bin/llvm-mc`（LLVM 21.1.8）。

## 0. 速览

| 问题 | 结论 |
| --- | --- |
| `-O2` 为什么崩 | `LoopVectorizePass`（只有 `-O2`/`-O3` 真正向量化 FP）生成的 **stock RVV** 指令落进 `RVV2CTFunctMap` 空槽，`fatalUnsupportedRVV()` 直接 `report_fatal_error`，不产出目标文件 |
| 崩在哪三条指令 | `funct6=0b000011 funct3=0b001` = **`vfredosum.vs`**；`funct6=0b101000 funct3=0b101` = **`vfmadd.vf`**；`funct6=0b101100 funct3=0b101` = **`vfmacc.vf`** |
| 哪条 flag 关掉它 | `-fno-vectorize` 或 `-mllvm -vectorize-loops=false`（任一条全灭）。`-ffp-contract=off` 只解决 FMA 那一支、**不解决规约支**；`-fno-slp-vectorize` 完全无效 |
| NaN 说法 | **再次证伪**：`-O0`/`-O1`/`-O2 -fno-vectorize` 输出**逐位一致**，481–962 个浮点值、4 组输入，无一处 NaN 或差异 |
| 推荐基线 | **`-O1`**（零附加 flag、与 `-O0` 逐位一致、不生成任何向量指令）；备选 `-O2 -fno-vectorize` |
| `-O0` 能否放宽 | **能**，放宽到 `-O1` 是唯一"零数值风险"的改动 |

---

## 1. `-O2` 崩溃定位

### 1.1 最小复现（源码 + 确切命令）

不需要任何项目头文件，**两行循环**即可。守卫包装 `wt-optbase/bin/vclang.sh` =
`ulimit -c 0; ulimit -f 262144; timeout <N> clang-21 …`。

```c
/* wt-optbase/src/repro_reduce.c */                     /* wt-optbase/src/repro_fma.c */
float tx82_sum(const float *__restrict__ x, int n) {    void tx82_axpy(float *__restrict__ y,
    float s = 0.0f;                                         const float *__restrict__ x,
    for (int i = 0; i < n; ++i) s += x[i];                  float a, int n) {
    return s;                                               for (int i = 0; i < n; ++i)
}                                                               y[i] = a * x[i] + y[i];
                                                        }
```
```bash
C="llvm-toolchain/bin/clang-21 --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -O2 -c"
$C wt-optbase/src/repro_reduce.c -o /dev/null   # rc=1
#  fatal error: error in backend: Unsupported RVV instruction with
#    funct6=0b000011 funct3=0b001 vs1=0b01000 vs2=0b01010 vm=0b1
$C wt-optbase/src/repro_fma.c    -o /dev/null   # rc=1
#  fatal error: error in backend: Unsupported RVV instruction with
#    funct6=0b101100 funct3=0b101 vs1=0b01010 vs2=0b01000 vm=0b1
```

第二条正是上一会话记录的 `funct6=0b101100 funct3=0b101`，已**精确复现**。
项目 recipe 下 14 个 kernel 的 `-O2` 结果（`logs/02_O2_crashhunt.txt`）：

| kernel | `-O2` | 崩溃字段 | kernel | `-O2` | 崩溃字段 |
| --- | --- | --- | --- | --- | --- |
| rmsnorm / layernorm / swiglu | rc=1 | `0b000011/0b001` | softmax / gelu / sample | rc=1 | `0b101000/0b101` |
| gemm / flash_attn | rc=1 | `0b000011/0b001` | **gemv** | rc=1 | **另一类**：`SEW must be 8 for VFWCVT_F_X_V` |
| add / mul / kv_cache / rope / version | rc=0 | —（`add` 被向量化，但生成的指令都可重映射） |

### 1.2 `funct6/funct3` → 助记符（必答项）

两条独立证据链。

**(a) 反查原版汇编器**（`funct6=(w>>26)&0x3f`、`funct3=(w>>12)&7`）：

| 报错字段 | 原版 `llvm-mc` 汇编该字段得到的指令 | 32 位字 | 语义 |
| --- | --- | --- | --- |
| `0b000011 / 0b001` | `vfredosum.vs v10, v8, v9` | `0x0e849557` | 有序浮点求和规约，`vm=1` |
| `0b101000 / 0b101` | `vfmadd.vf v10, fa0, v8` | `0xa2855557` | 向量×标量 FMA，**不累加**：`vd = vs1*rs1 + vs2` |
| `0b101100 / 0b101` | `vfmacc.vf v10, fa0, v8` | `0xb2855557` | 向量×标量 FMA，**累加到 vd**：`vd += vs1*rs1` |

三条全在 `XTsm82s_ENCODING_TABLE.md` §B 那 **11 条"厂商汇编器自己也会 abort"的助记符** 名单内
（`vfredosum.vs`、`vfmadd.vf`、`vfmacc.vf`）——即崩溃根因是**同一类**：fork 的
`convertRVVALUToCTEncoding()` 对这批 stock RVV 指令在 `RVV2CTFunctMap` 里查不到条目。

**(b) 查表自洽性**：该函数用 `Slot = funct3 + 7*funct6` 索引 `RVV2CTFunctMap`
（`wt-forkenc/rvv2ct_funct_map.txt`，index 由 **stock** funct3/funct6 构成）。
三个报错字段的 Slot 分别是 **22 / 285 / 313**，而 `RVV2CTFunctMap` 在这三格**全为空槽** ——
**正是**调用 `fatalUnsupportedRVV()` 的那个分支 ⇒ 报错里印的是 **stock RVV 的 funct6/funct3**，
不是 TX8 重映射后的值（反证见 `logs/07_funct_interpretation.txt`）。
操作数字段亦吻合：`tx82_rmsnorm` 的 `vs1=v8,vs2=v0` 对应
`vfmv.v.f v8,0.0` 播种 + `vmul.vv v0,…` + `vfredosum.vs vd, v0, v8`；
`tx82_layernorm` 的 `vs2=v10` 是直接对数据向量规约（`s += x[i]`）。

### 1.3 哪条 pass / 哪个 flag；哪条 C 构造

**pass**：`LoopVectorizePass`。`-mllvm -print-after=loop-vectorize` 抓到的 IR：
```
repro_reduce.c -O2 : %22 = call float @llvm.vector.reduce.fadd.nxv4f32(float %18, <vscale x 4 x float> %21)
repro_fma.c    -O2 : %28 = call <vscale x 4 x float> @llvm.fmuladd.nxv4f32(...)
```
（`logs/05_ir_loopvectorize.txt`）该 pass 在 `-O0` **不运行**，`-O1` **运行 1 次但不向量化 FP**，
`-O2`/`-O3` 运行并向量化（`logs/14_pass_pipeline.txt`）——这就是"只有 `-O2`/`-O3` 崩"的机制。

**C 构造 → 助记符**：浮点**规约**（`s += x[i]`、`s += x[i]*y[i]`）→ `vfredosum.vs`；
**非累加乘加**（`c*x[i] + z[i]`）→ `vfmadd.vf`；**累加型乘加**（`y[i] = a*x[i] + y[i]`、
`acc[i] += a*x[i]`）→ `vfmacc.vf`。

**flag 二分**（4 个最小复现，`-O2`，`logs/03_flag_bisect.txt`）：

| flag | 规约类 `vfredosum.vs` | FMA 类 `vfmadd.vf`/`vfmacc.vf` |
| --- | --- | --- |
| （默认） / `-fno-slp-vectorize` / `-fno-unroll-loops` / `-fno-inline` | 崩 | 崩 |
| `-ffp-contract=off` | **崩** | **过** |
| `-fno-vectorize` / `-mllvm -vectorize-loops=false` | **过** | **过** |

> **`-fno-vectorize`（或等价的 `-mllvm -vectorize-loops=false`）是唯一能同时消灭两类的单条 flag。**
> `-ffp-contract=off` 只绕开 FMA 支；`tx82_softmax -O2 -ffp-contract=off` 仍报 `0b000011/0b001`。

### 1.4 附带发现（实测）

* **`-mllvm -riscv-vreg-align=<n>` 是布尔选项**（`=8`/`=32` 报 `invalid value for boolean argument`）。
  而 `=0` 会把 `tx82_softmax -O2/-O3` 的干净诊断变成 **cc1 裸 SIGSEGV**（`exit code 139`），
  5/5 可复现（`logs/09_vreg_align_segv.txt`）⇒ **该 flag 不可用**。
* `fatalUnsupportedRVV()` 带 `GenCrashDiag=true`，崩溃复现包（`.c`+`.sh`）写到 `$TMPDIR`
  （本机 `/tmp/tx82_softmax-*.c`），**不落 CWD**；已留档两份于 `wt-optbase/logs/`。
* `qemu-riscv64-static` 对 `0x7B` 判 **illegal instruction**（`logs/11_qemu_decodes_0x7b.txt`：
  `0x000100b0: 2d1077fb illegal`，rc=132）⇒ qemu 只能跑零 `0x7B` 的目标码。

---

## 2. 优化档 × flag 矩阵（实测）

flag 集（10 组）：`F0` 默认、`F1 -fno-vectorize`、`F2 -mllvm -vectorize-loops=false`、
`F3 -fno-slp-vectorize`、`F4 -ffp-contract=off`、`F5=F1+F4`、`F6=F2+F4`、
`F7 -mllvm -riscv-vreg-align=0`、`F8 -fno-inline`、`F9=F1+F3+F4`；优化档 `-O0/-O1/-O2/-O3`。
驱动：`wt-optbase/bin/matrix.py`（A/B 各 3×4×10 = **120 个实测格子**）。

### 2.1 矩阵 A — 项目自己的 kernel（编译 + `0x7B` 普查）

recipe 原样编译；`0x7B` 由厂商 `llvm-objdump -d` 逐条统计，`vec` = 去掉 4 条厂商内建
（`lsu_tdma_mem_copy`/`vsetbarrier`/`send_sync`/`wait_sync`）后的条数。
全 120 行见 `logs/16_matrixA_final.txt`。

| kernel | `-O0` | `-O1` | `-O2`/`-O3` 默认 | `-O2`/`-O3` + `F1/F2/F5/F6/F9` |
| --- | --- | --- | --- | --- |
| `tx82_add` | ok，4(0) | ok，4(0) | ok，**26(22)** 向量码 | ok，4(0) |
| `tx82_rmsnorm` | ok，4(0) | ok，4(0) | **rc=1** `vfredosum.vs` | ok，4(0) |
| `tx82_softmax` | ok，4(0) | ok，4(0) | **rc=1** `vfmadd.vf`（`-ffp-contract=off` 时改报 `vfredosum.vs`） | ok，4(0) |

格式 `0x7B 总数 (vec)`。要点：

* **`-O0` 与 `-O1` 代码形态相同**：只有 4 条内建 `0x7B`、`vec=0`、**无任何向量指令**。
* `-O3 -F1/F2` 计数是 6 而非 4：`send_sync`/`wait_sync` 被 `-O3` 复制成 2 份，**`vec` 仍为 0**
  （逐条核对 `wt-optbase/asm/proj/proj_add_O3_F1-no-vectorize.o.dis`）。
* `tx82_add -O2/-O3` 默认**编得过**却发射 22 条向量 `0x7B`（`vfadd.vv`×5、`vl2re32.v`×10、
  `vs2r.v`×5、`vsetvli`×2）；可重映射所以不崩，但这些目标码 **qemu 跑不了**（§2.2）。
* **全 14 个 kernel 的 `-O0`/`-O1` 对照**（`logs/17_O0_O1_all_kernels.txt`）：28 个格子全部 rc=0，
  且 **`-O0` 与 `-O1` 的 `0x7B` 条数逐个相同**。部分 kernel 的额外 `0x7B`
  （`gemm`/`gemm_psum`/`ne_set_*`）是**源码里调用的 NE/GEMM 内建**，与优化档无关；
  `add`/`mul`/`rmsnorm`/`layernorm`/`softmax`/`gelu`/`rope`/`sample`/`version` 的 `vec` 均为 **0**。

### 2.2 矩阵 B — `qemu-riscv64-static` 真跑 + 逐位比对

项目 kernel 的 `.o` **永远含 4 条 `0x7B` 内建**，qemu 一律 SIGILL，无法端到端跑。
故矩阵 B 用**结构完全相同、只把 4 条厂商内建删掉**的纯数学 kernel
（`wt-optbase/src/qk_{add,rmsnorm,softmax}.c`），循环、`#pragma clang loop unroll_count(4)`、
`tx82_fast_inv_sqrt`/`tx82_fast_exp` 逐字保留；**未改动 `gemini-cmodel/` 下任何文件**。

搭法：驱动与参考实现**固定在 `-O0 -ffp-contract=off` 编译一次**（`bin/build_harness.sh`，
md5 见 `logs/12_driver_md5.txt`），与被测 kernel 目标文件静态链接成 `-nostdlib -static
-Wl,--no-relax` 裸 ELF，再 `timeout 60 qemu-riscv64-static wt-optbase/bin/har_<kernel>_<opt>_<flags>`。
每个 binary 跑 **4 组输入**（`n = 64/100/17/300`，第 4 组强制进入 RVV 向量路径），打印全部浮点的
8 位十六进制位型（add 两个变体共 **962** 个值，rmsnorm/softmax 各 **481** 个），**逐位**与
(i) 同进程严格 IEEE 标量参考、(ii) `-O0` 产物比较。全 120 行见 `logs/15_matrixB_final.txt`，
qemu 原始输出在 `wt-optbase/run/har_*.out`。

| kernel | 配置 | `0x7B` | qemu | vs 严格 IEEE 参考 | vs `-O0` 产物 |
| --- | --- | --- | --- | --- | --- |
| `qk_add` | `-O0`/`-O1` 全 10 组；`-O2`/`-O3` `F1/F2/F5/F6/F9` | 0 | rc=0 | **962/962 一致** | **SAME** |
| `qk_add` | `-O2`/`-O3` `F0/F3/F4/F7/F8` | 27 | **SIGILL**@n=300 | 已输出部分 362/362 一致 | 截断 |
| `qk_rmsnorm` | `-O0`/`-O1` 全 10 组；`-O2`/`-O3` `F1/F2/F5/F6/F9` | 0 | rc=0 | **481/481 一致** | **SAME** |
| `qk_rmsnorm` | `-O2`/`-O3` `F0/F3/F4/F7/F8` | — | — | 编译失败（`vfredosum.vs`） | — |
| `qk_softmax` | `-O0`/`-O1` `F0/F1/F2/F3/F7/F8`；`-O2`/`-O3` `F1/F2` | 0 | rc=0 | 126/481（**FMA 收缩**，非错误） | **SAME** |
| `qk_softmax` | 所有 `-ffp-contract=off` 组（`F4/F5/F6/F9`） | 0 | rc=0 | **481/481 一致** | **DIFF** |
| `qk_softmax` | `-O2`/`-O3` `F8 (-fno-inline)` | 4 | **SIGILL**@n=300 | 已输出部分 124/181 | 截断 |

**三条硬结论：**

1. **没有任何配置产生 NaN 或"读未定义寄存器"式错值**：所有能跑的标量配置与严格 IEEE 标量参考、
   与 `-O0` 产物**逐位相同**（962/962、481/481）。`compile_kernels.sh` 里 "`-O1/-O2/-O3` 把浮点
   累加编译成 NaN" 的注释**与实测不符**，应删除。
2. **`-ffp-contract=off` 会改变数值**，不是"更安全"：`-O0` 本身就已发射 `fmadd.s`（默认
   `-ffp-contract=on`）；`qk_softmax` 的 `q_fast_exp` Horner 链 `p = p*r + c` 因此有
   **355/481 个值不同**（`DIFF`）。两者都"对"但**不是同一组数**：要"与现状逐位一致"就**不能**加它。
3. **含 `0x7B` 的目标码在 qemu 上不可执行**（n≥256 进入向量路径即 SIGILL）⇒
   `tx82_add -O2` 这类"能编出来"的向量化产物，**今天没有任何可用仿真器能验证**。

### 2.3 被否掉的候选

`-mllvm -riscv-vreg-align=0`（布尔选项，且把 softmax 的干净诊断变成 cc1 SIGSEGV）、
`-fno-slp-vectorize`、`-fno-inline`、`-fno-unroll-loops` 全部**否**；后两者对两类崩溃都无效，
且 `-fno-inline` 让 softmax 在 `-O2` 下额外生成 4 条向量 `0x7B`。
另注：`tx82_builtins.h` 注释称"kernels 统一使用 `-fno-inline` 编译"，但 `compile_kernels.sh`
的 `CFLAGS` **没有 `-fno-inline`** —— 注释与脚本不一致。

---

## 3. 推荐基线

| 候选 | 编译 | 数值 | 结论 |
| --- | --- | --- | --- |
| `-O0`（现状） | 全过 | 基准 | 保底可用，放弃全部标量优化 |
| **`-O1`** | **全过（14/14 kernel）** | **与 `-O0` 逐位一致** | ✅ **推荐** |
| `-O2 -fno-vectorize` | 全过 | 与 `-O0` 逐位一致 | ✅ 备选（更激进的标量优化） |
| `-O2 -fno-vectorize -ffp-contract=off` | 全过 | **改变数值**（≠`-O0`） | ⚠️ 仅在明确要求严格 IEEE 时 |
| `-O2` / `-O3` 默认 | rmsnorm/softmax/gelu 等**编译失败** | — | ❌ |
| `-O2 -ffp-contract=off`、`-O2 -fno-slp-vectorize` | 规约类仍失败 | — | ❌ |
| `-mllvm -riscv-vreg-align=0` | softmax cc1 SIGSEGV | — | ❌ |

**主基线 `-O1`**：① 14 个 kernel 全部编译成功，`0x7B` 普查与 `-O0` 逐个相同、`vec=0`；
② 输出与今天的 `-O0` 产物**逐位相同**，切换不需要重跑任何参考值；
③ 无需任何附加 flag，脚本改动最小、最不易被后人改坏。
**备选 `-O2 -fno-vectorize`**：同样与 `-O0` 逐位一致、`vec=0`，可用于需要 `-O2` 标量优化的场合。
**不要**顺手加 `-ffp-contract=off`——它会静默改变 softmax/gelu/swiglu 的 `exp` 链数值。

**`-O0` 可以放宽到 `-O1`。** 证据：3 个 kernel × 10 组 flag × 4 组输入全部逐位一致
（§2.2），加 14 个 kernel 的 `-O0`/`-O1` 编译与 `0x7B` 普查全等（§2.1）；
上一会话"`-O0` 与 `-O1` 逐位相同"的结论被独立复现并扩展到 962/481 个浮点值、
覆盖 FMA 收缩路径与 `exp` 近似路径。

**残余风险**：① `-O1` 产物仍未在**支持 `0x7B` 的真实硬件/仿真器**上跑过（§5），本次只证明了
"4 条内建之外没有编译器生成的向量指令"；② 逐位比对只覆盖 `add`/`rmsnorm`/`softmax` 三个 kernel，
其余 kernel 只做了编译与 `0x7B` 普查（`gemv -O2` 还会撞第三种崩溃 `VFWCVT_F_X_V`），未做数值差分。

---

## 4. 对 `gemini-cmodel/kernels/compile_kernels.sh` 的建议改动（**仅建议，未改文件**）

**(a) 替换 `CFLAGS` 上方那段错误注释**（现注释称 `-O1/-O2/-O3` 会把浮点累加编成 NaN、
`-O2 -fno-inline` 触发崩溃；前半句已被两次独立实测证伪，后半句归因也不对）：

```bash
# 数值与崩溃实测结论 (详见 docs/compiler/OPT_BASELINE_MATRIX.md):
#  * "低优化档把浮点累加编成 NaN" 的说法已证伪: -O0/-O1 在 qemu-riscv64-static 上
#    逐位一致 (962/481 个浮点值, 4 组输入), 与严格 IEEE 标量参考也逐位一致。
#  * 真正的硬故障在 -O2/-O3: LoopVectorizePass 生成 vfredosum.vs / vfmadd.vf /
#    vfmacc.vf, 这三条 stock RVV 指令在本 fork 的 RVV2CTFunctMap 里是空槽, 后端直接
#    report_fatal_error("Unsupported RVV instruction ..."), 不产出目标文件。涉及
#    rmsnorm/layernorm/swiglu/gemm/flash_attn/softmax/gelu/sample。
#  * 故设备 kernel 统一 -O1: 14/14 kernel 编译通过, 0x7B 普查与 -O0 逐个相同, 且除 4 条
#    厂商内建 (lsu_tdma_mem_copy/set_barrier/send_sync/wait_sync) 外不生成任何向量指令。
```

**(b) `CFLAGS` 改为**（主基线；备选一行见注释）：

```bash
CFLAGS="-O1 -ffreestanding -nostdinc -fPIC --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -I${SRC_DIR}"
# 备选 (需更激进的标量优化时; 必须带 -fno-vectorize, 且不要加 -ffp-contract=off,
#  后者会改变 softmax/gelu/swiglu 的 exp 链数值):
# CFLAGS="-O2 -fno-vectorize -ffreestanding -nostdinc -fPIC --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -I${SRC_DIR}"
```

**(c) 加返回码检查**：脚本现在对 `clang -c` 的失败**不检查返回码**（`set -u` 而非 `set -e`），
`-O2` 崩溃时脚本会"成功退出"并留下缺文件的 `libtx82_llm_kernels.so`：

```bash
if ! "${CLANG}" ${CFLAGS} -c "${SRC_DIR}/tx82_${k}.c" -o "${LIB_DIR}/tx82_${k}.o"; then
    echo "[compile_kernels] 编译失败: tx82_${k}.c (CFLAGS=${CFLAGS})" >&2
    exit 1
fi
```

**(d)** `tx82_builtins.h` 头部称 "kernels 统一使用 `-fno-inline` 编译"，但脚本里没有该 flag，
且实测 `-fno-inline` 对崩溃毫无帮助 —— 该句应删除或改为"本仓库不使用 `-fno-inline`"。

---

## 5. 残余不确定性（最重要的一条）

**没有任何可用仿真器能执行 `0x7B` 指令**，设备内核的**端到端**行为今天无法验证：
`qemu-riscv64-static` 判其为 `illegal instruction`（§1.4），仓库内没有 `spike`，
`gemini-cmodel/src/iss/` 仅有一个依赖未安装 spike 的 `cmodel_spike_bridge.cpp`。
本次所有"真跑"都建立在**删掉 4 条厂商内建、`0x7B` 计数为 0** 的等价 kernel 上（§2.2），
验证的是**标量浮点数值语义**，**不覆盖** `lsu_tdma_mem_copy`/`set_barrier`/`send_sync`/
`wait_sync` 及任何 RVV 语义。即：**"`-O1` 与 `-O0` 逐位一致"已被实测钉死；
"`-O1` 的产物在设备上正确"仍无任何证据支持。** 要钉死这层需要一个支持 `+xtsm82s`
的 qemu fork 或 spike 补丁。

---

## 6. 证据索引（`wt-optbase/`）

| 路径 | 内容 |
| --- | --- |
| `bin/vclang.sh`、`bin/matrix.py`、`bin/census.py`、`bin/scan7b.py`、`bin/build_harness.sh` | 厂商调用守卫 / 矩阵驱动 / `0x7B` 普查 / 二进制扫描 / 固定驱动重建 |
| `logs/00_all_commands.txt`、`logs/01_stock_mnemonic_map.txt` | 全部确切命令（含 qemu 调用行） / 原版 `llvm-mc` 给出的 11 条助记符 → funct6/funct3 |
| `logs/02_O2_crashhunt.txt` | 14 个 kernel 在 `-O2` 下的 rc + 崩溃原文 |
| `logs/03_flag_bisect.txt` / `logs/04_repro_messages.txt` / `logs/14_pass_pipeline.txt` | 排除法二分 / 4 个最小复现完整报错 / 各档是否运行 `LoopVectorizePass` |
| `logs/05_ir_loopvectorize.txt`、`asm/repro_{reduce,fma}_O2_loopvec_IR.txt` | `LoopVectorizePass` 产出的 `vector.reduce.fadd` / `fmuladd` |
| `logs/07_funct_interpretation.txt` | "报错字段是 stock 值"的证明（查表空槽 + 11 助记符名单） |
| `logs/09_vreg_align_segv.txt`、`logs/11_qemu_decodes_0x7b.txt`、`src/probe_7b.S` | vreg-align cc1 SIGSEGV / qemu 判 `0x7B` 为 illegal 的直证 |
| `logs/16_matrixA_final.txt`、`logs/matrix_A{,_final}.json` | 矩阵 A 全 120 行（rc / `0x7B` / `vec` / 助记符） |
| `logs/15_matrixB_final.txt`、`logs/matrix_B{,_final}.json` | 矩阵 B 全 120 行（rc / `0x7B` / qemu rc / 逐位比对） |
| `logs/17_O0_O1_all_kernels.txt` | 全 14 个 kernel 的 `-O0` vs `-O1` 编译与 `0x7B` 普查 |
| `logs/recheck_*.err`、`run/recheck_*.out` | 独立复算：从头重建驱动（md5 相同）后 `-O0` 与 `-O2 -fno-vectorize` 的 qemu 输出 md5 相同 |
| `obj/proj_*.o`、`obj/all_*.o`、`asm/proj/*.dis`、`obj/har_*.o`、`bin/har_*`、`run/har_*.out` | 目标文件、厂商反汇编、ELF 与 qemu 原始输出 |
| `src/repro_*.c`、`src/qk_*.c`、`src/qdriver.c`、`src/qstart.S`、`src/qcommon.h` | 最小复现 / 等价 kernel / 固定驱动 / 严格 IEEE 参考 / 裸 `_start` |

复现：`python3 wt-optbase/bin/matrix.py A|B`（各约 120 次编译，单线程，对 `-j2` 并发构建干扰可忽略）。
