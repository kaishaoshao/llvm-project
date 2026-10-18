# TX82 vendor LLVM fork 相对上游 LLVM 21.1.0 的黑盒增量报告（FORK_DELTA）

- 实验日期：2026-10-16，工作目录 `/home/shaokai/Desktop/work/tx82`，全部为**只读**探针 + 独立 scratch `wt-forks/`。
- 证据口径：`[实测]` = 本轮直接跑命令得到的输出（命令与原始行见各节及 `wt-forks/logs/`）；`[推断]` = 由实测直接推出。
- 未修改 vendor 工具链、未读写 `gemini-cmodel/`（仅读取其 `kernels/*.c` 与 docs 作为假设来源）、未访问 `192.168.100.107`。

## 0. 结论速览

1. **基线达成**：解出官方 `LLVM-21.1.0-Linux-X64.tar.xz` 的 `clang-21 / llc / llvm-mc / llvm-objdump` 到 `wt-forks/stock/`，版本 `21.1.0 (github.com/llvm/llvm-project 3623fe661ae35c6c80ac221f14d85be76aa870f1)`。另有 Debian `llvm-21 21.1.8` 作为第二基线（`wt-forks/stock-deb/` + `wt-forks/stock-bin/`）。
2. **fork 对”上游既有路径“零增量**：`-march=rv64gc` 下 vendor clang 与 stock clang 的 `.s` **逐字节相同**（scalar/vec 语料，`-O0/-O1/-O2`），仅 `.ident/.attribute` 文本不同。
3. **fork 的本质改动 = (a) 新增 `-mcpu=tsm-x82` 目标 + `xtsm82s` feature；(b) 把整个 RVV 指令空间重编码进 custom-3（opcode `0x7B`）；(c) 新增 177 条 TX8 专用助记符（同处 `0x7B`）；(d) RAISA 语言/offload 驱动管线。**
4. **自定义编码普查规模**：`0x7B` 空间共解码出 **31 104** 个可识别编码 / **371** 个不同助记符；其中 **177 个 fork 独有**、**150 个是标准 RVV 助记符的重编码**、**44 个 fork 自己的汇编器无法再汇编**。custom-0/1/2 (`0x0B/0x2B/0x5B`) **完全为空**。
5. **`-O0` 问题**：项目文档中的”`-O1` 把浮点累加错编成读未写入的 `fa0` → NaN“ **无法复现，且根因描述不成立**（`fa0` 就是 lp64f 的第一个浮点参数 `eps`，`-O0` 也照样读它）。**能复现且更严重的是**：`-mcpu=tsm-x82 -O2` 及 `-O2 -fno-inline` 在 `RISC-V Assembly Printer` 阶段**硬失败**（`Unsupported RVV instruction ...`，无目标文件产出）；`-O0` 与 `-O1` 在 qemu 上跑同一内核**输出逐位相同**。

## 1. 基线（实验 1）

`[实测]` 用 `tar -tJf` 先取清单（`wt-forks/logs/stock_tarball_listing.txt`，10 523 行，解压后 10.5 GB），再单趟带 wildcard 抽取所需子集：

```bash
cd wt-forks/stock && ulimit -f 4194304
timeout 1800 tar -xJf .../LLVM-21.1.0-Linux-X64.tar.xz --wildcards \
  'LLVM-21.1.0-Linux-X64/bin/{clang-21,llc,llvm-mc,llvm-objdump,llvm-objcopy,llvm-readelf,llvm-readobj}' \
  'LLVM-21.1.0-Linux-X64/lib/clang/*'          # → 641 MB
```

```
$ wt-forks/stock/.../bin/clang-21 --version
clang version 21.1.0 (https://github.com/llvm/llvm-project 3623fe661ae35c6c80ac221f14d85be76aa870f1)

$ llvm-toolchain/bin/clang-21 --version
clang version 21.1.0-rc1 (ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf26e7a507d3ecb57e76c956dd21b5c822a6)
```

第二基线（Debian，供交叉验证，二者结论一致）：`dpkg-deb -x` 解出 `llvm-21/llvm-21-tools/llvm-21-runtime/libllvm21` 到 `wt-forks/stock-deb/`（224 MB），包装脚本 `wt-forks/stock-bin/{llvm-mc,llc,llvm-objdump,...}` 设好 `LD_LIBRARY_PATH`。Debian 包**不含 clang**，故 clang 级实验只用 tarball 基线。`[实测]`：`LLVM version 21.1.8`。

## 2. flag / CPU 面与 codegen 差分（实验 2）

### 2.1 接受度

```
$ stock-clang ... -mcpu=tsm-x82 -c tiny.c
clang-21: error: unsupported argument 'tsm-x82' to option '-mcpu='
$ stock-clang ... -march=rv64gc_xtsm82s -c tiny.c
clang-21: error: invalid arch name 'rv64gc_xtsm82s', unsupported non-standard user-level extension 'xtsm82s'
```

vendor clang **两者都接受**；`tsm-x82` 出现在 `-print-supported-cpus` 列表（倒数第 3 项）。`-march=help` 在该版本不存在（报 `invalid arch name 'help'`）。

### 2.2 `-mcpu=tsm-x82` 实际启用的 feature（`-###` 抽取，`wt-forks/logs/tsmx82_cc1_line.txt`）

```
正 feature（21）：+c +f +m +relax +xtsm82s +zbb +zca +zfbfmin +zfh +zfhmin +zicsr +zifencei
                  +zmmul +zve32f +zve32x +zve64x +zvfbfmin +zvfh +zvfhmin +zvl32b +zvl64b
显式关闭：-a -d -v -zve64d -zve64f -zcd -zaamo -zalrsc ...（无 A、无 D、无 V 全量）   -target-abi lp64f
.attribute 5 = rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_zmmul1p0_zfbfmin1p0_zfh1p0_zfhmin1p0_zca1p0
               _zbb1p0_zve32f1p0_zve32x1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_zvfhmin1p0_zvl32b1p0_zvl64b1p0_xtsm82s1p0
```

### 2.3 `-march=rv64gc` vs `-mcpu=tsm-x82`（同一 C 输入）

- `tiny.c`（`int f(int,int)`）：**指令完全相同**（`addw a0,a0,a1; ret`），只有 `.attribute 5` 不同（rv64gc 含 `a/d/zcd`，tsm-x82 无）。
- `scalar.c` 上 `-O2` 的关键差异 `[实测]`（`wt-forks/out/codegen/`）：

| 构造 | `-march=rv64gc` | `-mcpu=tsm-x82` |
| --- | --- | --- |
| `double dmul(double,double)` | `fmul.d fa0,fa0,fa1` | `call __muldf3`（无 D，软浮点） |
| `__atomic_fetch_add(long*,1)` | `amoadd.d.aqrl a0,a1,(a0)` | `call __atomic_fetch_add_8`（无 A） |
| `__builtin_popcount` / `float a*b+c` | rv64gc 无 zbb；`fmadd.s` | `cpopw a0,a0`；`fmadd.s`（同，`-ffp-contract=on`） |

- **vendor `-march=rv64gc` == stock `-march=rv64gc`**：`scalar.c` 与 `vec.c` 在 `-O0/-O1/-O2` 下 `.s` 去掉 `.ident/.attribute` 后**完全相同**（`diff` 空）。`[推断]` fork 未触碰标准 RVLIC-V 后端的选择/调度/优化管线。
- 向量自动向量化 `[实测]`：vendor `-mcpu=tsm-x82 -O2` 把 `axpy` 向量化并发出 `vl2re32.v / vfmacc.vf / vs2r.v`；stock `-march=rv64gcv -O2` 同形但编码为标准 `0x57/0x07/0x27`。vendor 用 `li a3,256`、`li a5,64` **写死 VLEN=256bit（VLENB=32）**，stock 用 `csrr a3, vlenb`。

### 2.4 编码差分（决定性）

同一批助记符、两侧 `-show-encoding`（`wt-forks/out/scalar_insns_*.txt`、`out/census_full.tsv`）：

```
助记符                                vendor (-mcpu=tsm-x82)   stock (-mattr=+v,+zbb,+zfh)
vsetvli zero, zero, e32, m1, ta, mu   [0x7b,0x70,0x00,0x45]   [0x57,0x70,0x00,0x05]
vadd.vx v10, v8, s9                   [0x7b,0xd5,0x8c,0x0e]   [0x57,0xc5,0x8c,0x02]
vfmul.vv v12, v11, v9                 [0x7b,0x96,0xb4,0x92]   [0x57,0x96,0xb4,0x92]   ← 仅改 opcode
vse32.v v12, (a1)                     [0x7b,0xc6,0x05,0x12]   [0x27,0xe6,0x05,0x02]
vluxei64.sync.v v13, (zero), v18      [0xfb,0x36,0x20,0x37]   error: unrecognized instruction mnemonic
```

关键点：`vfmul.vv/vfadd.vv` 只改低 7 位 opcode `0x57→0x7B`；`vsetvli/vadd/vid.v/vmv/vmsne/vmul/vdivu/vse32` 除 opcode 外**还有额外 bit 变化**（例如 `vse32.v` 由 `0x27` 迁到 `0x7B` 且 bit12/bit20 重排）。**同一助记符、不同编码**的完整清单见 `wt-forks/out/census_full.tsv`（`class=remapped-RVV`，共 150 条）。

**双解码**`[实测]`：vendor 汇编器在 `+v,+xtsm82s` 下**同时**接受 `0x57` 与 `0x7B` 两种形态并把它们都反汇编成同一助记符；在只有 `+v`（无 `xtsm82s`）时**只发 `0x57`**。→ `[推断]` `xtsm82s` 打开了一个”RVV 重映射“编码模式，是 feature 条件化的，而非全局替换。

**stock 工具无法处理 vendor 产物**`[实测]`（`logs/stock_objdump_on_vendor_bytes.log`）：`stock llvm-objdump -d` 对 device.o 中所有 `0x7B` 指令打印 `<unknown>`；`stock llvm-mc` 汇编 vendor `.s` 时报：

```
error: unsupported non-standard user-level extension 'xtsm82s'         (.attribute)
error: unrecognized instruction mnemonic                                (vsetsimt)
error: unrecognized instruction mnemonic                                (mv128.s.v / nor.ss / set_mem_alloc)
```

## 3. custom-0/1/2/3 编码普查（实验 3，核心制品）

方法：`wt-forks/mc_scan.py` 穷举 `opcode × funct3 × funct7 × rs2`（第二遍扫 `rs1`），每探针后插一条 `ecall` 作定位标记，喂给 `llvm-mc -disassemble`。

```
python3 mc_scan.py custom <llvm-mc> +xtsm82s        out/vendor_custom.txt     # 11 347 编码 / 177 mnemonic
python3 mc_scan.py custom <llvm-mc> +xtsm82s,+v     out/vendor_custom_v.txt   # 31 104 编码 / 371 mnemonic
```

`[实测]` opcode 分布：`Counter({123: 31104})` → **全部为 `0x7B`（custom-3）**；`0x0B/0x2B/0x5B` 命中数为 0。
`[实测]` 从 `llvm-toolchain/include/llvm/IR/IntrinsicsRISCVXTsm.td`（436 行）可读出 TX8 intrinsic 家族（`vset2dvload0/1`、`set_exec`、`gemm/mma/conv`、`dte_*`、`lsu_*dma_*`、`send/recv/wait_*`、`read_reg_tid/ctaid/...`、`raisa_reduce_sum/sin/cos/fdiv` 等），与本普查的 177 条新增助记符一一对应。

### 3.1 分类结果（371 条代表形式，`out/census_full.tsv`）

| class | 条数 | 含义 |
| --- | --- | --- |
| `fork-only-new` | **177** | 两侧 stock 都无此助记符 → TX8 专有 |
| `remapped-RVV` | **150** | stock 同助记符存在，但 vendor 在 `0x7B` 重编码 |
| `vendor-cannot-reassemble` | **44** | vendor **反汇编器认**、但 vendor **汇编器拒绝**（段访存/ff 类） |

### 3.2 TX8 专有助记符编码表（46 条代表，`out/tx8_mnem_enc.txt`；全部 `opcode=0x7B`）

`f3`=bits14:12，`f6`=bits31:26，`vm`=bit25；编码字节为内存序（LE）。**表中 `f3/f6/vm` 只对应每行第一个助记符**，每条的精确字段见 `out/tx8_mnem_enc.txt`。

| 助记符 | 编码字节 | f3 | f6 | vm | 语义 |
| --- | --- | --- | --- | --- | --- |
| `vsetsimt 1` / `vsetbarrier` | `7b f0 00 c0` / `7b 70 00 82` | 7 | 0x30 | 0 | SIMT 模式设置 / 硬件栅栏 |
| `isetexec sr0` / `movexecz a1` | `7b 70 08 be` / `fb 15 00 f6` | 7 | 0x2f | 1 | 执行掩码写入 / 读回标量 |
| `set_mem_alloc t6` | `7b 70 f0 b9` | 7 | 0x2e | 0 | 共享内存分配设置 |
| `send_sync` / `wait_sync` | `7b 00 00 02` / `7b 00 00 0a` | 0 | 0x00 | 1 | 多核同步（`inst_barrier` 需更多操作数，未取到编码） |
| `gemm` / `gemm_psum` | `7b 85 05 46` / `7b 85 05 4e` | 0 | 0x11 | 1 | NE 矩阵乘（含累加） |
| `conv` / `convbw`(+`_psum`) | `7b 85 05 66` / … | 0 | 0x19 | 1 | 卷积 |
| `mma` / `mma_first` / `mma_last` / `mma_first_psum` | `7b 85 05 a6/96/ae` | 0 | 0x29 | 1 | 矩阵乘 |
| `ne_set_prec/misc/mkn/h_oh_ow/conv_pad/conv_swr/length` | `7b 85 05 06` / … | 0 | 0x01 | 1 | NE 配置 |
| `l0_invalid/l0_flush/l1_flush/l1_invalid/l1_sw_prefetch` / `l0_set_*`(8) | `7b e5 05 00/10/…` | 6 | 0x04 | 0 | cache 维护/步长 |
| `lsu_tdma_mem_copy` / `lsu_tdma_nchw2nhwc/nhwc2nchw/bank_scatter` | `7b 85 25 04` / `7b 85 15 c4` | 0 | 0x01 | 0 | TDMA 搬运 / 转置 / bank |
| `lsu_rdma_*`(7) / `lsu_wdma_*`(7) / `lsu_set_*`(8) | `7b 85 05 04` / … | 0 | 0x01 | 0 | RDMA/WDMA/DMA 配置 |
| `dte_gather/scatter/*_g2l/l2g/index_*` / `dte_set_*`(7) | `7b 85 05 80` / … | 0 | 0x20 | 0 | DTE 搬运与配置 |
| `send/recv/direct_write/wait_*`(+`_roce`)；`kcore_ncc_read`/`kcore_wait` | `7b 85 05 ca/da/ea/…`；`fb 05 b0 42` | 0 | 0x32 | 1 | 通信 / KCore NCC |
| `mv128.s.x` / `mv128.s.v` / `mv128.v.s` | `7b a0 a5 3a` / `7b 10 80 e2` | 2 | 0x0e | 1 | 跨域 128bit 搬移（SIMT exec ↔ 标量） |
| `and.ss/or.ss/xor.ss/nand.ss/nor.ss/orn.ss/xnor.ss/andn.ss` | `7b 20 00 82` / `7b 20 00 9a` | 2 | 0x20..0x26 | 1 | SIMT 掩码逻辑 |
| `vbit2fp.m` / `vreverse.v` / `vslh.vv` / `vslh.vx` | `7b 25 b0 36` / `7b 25 b0 da` / `7b 15 94 da` | 2 | 0x0d | 1 | 位域→FP / 反序 / 逻辑左移 |
| `vl2de16/32.v` / `vs2de16/32.v` | `7b b5 05 0e` / `7b c5 05 1e` | 3 | 0x07 | 1 | 2D 访存 |
| `vluxei32/64.sync.v` / `vsuxei32/64.sync.v` | `7b b5 05 26` / `7b c5 05 26` | 3 | 0x0d | 1 | 同步 gather/scatter |
| `vfadd/vfsub/vfmul/vfmax/vfmin.vs(.vx)` | `7b 95 84 06` / … | 1 | 0x01 | 1 | 向量-标量 FP |
| `vand/vor/vxor/vmax/vmin/vmaxu/vminu.vs`；`vfeq/vfle/vflt/vfne/vfgt/vfge.{vv,vx,vs}` | `7b …` | 2/1/5 | — | 0/1 | 向量-标量整数 / 向量比较 |
| `vfsquare/vpow2/vlog2/vsin/vcos.v` | `7b 25 b0 02` / `7b 15 b4 4e` | 2 | 0x00 | 1 | 数学函数单指令（`__raisa_*` 降低目标） |
| `vfwcvt/vfncvt` × {bf16,e4m3,e5m2,fp4} × {f,bf} | `7b 25 80 a2` / `7b a5 b0 a2` | 2 | 0x28 | 0/1 | 窄精度转换（16 条） |
| `sort_sort/sort_top_k/sort_unique/sort_pair_*/sort_set_*/sort_auto_idx/…` | `7b 85 05 28` / … | 0 | 0x0a | 0 | 排序/去重/TopK 加速器 |

### 3.3 真实编译产物中的普查（`logs/device_o_custom_census.txt`）

`--raisa-device-only -c gemm_raisa.rc` 得到的 device.o 共 284 条 32-bit 指令，其中 **197 条 opcode=0x7B**。`[实测]` 实际出现的 TX8 专有助记符只有：
`vsetsimt, isetexec×2, movexecz, set_mem_alloc, mv128.s.v×2, and.ss, nor.ss, kcore_ncc_read, vslh.vx×30, vluxei64.sync.v×30, vse32.v`
——真实 RAISA 内核只用到 `0x7B` 的一小撮，但**整个 RVV 空间都已迁移**：`vsetvli / vadd.vx / vfmul.vv / vfadd.vv / vmv.v.v / vid.v / vmul.vx / vdivu.vx / vsub.vv / vmsne.vv / vmsltu.vx` 也全部编码在 `0x7B`。

### 3.4 fork 自身的不一致（重要缺陷）

`[实测]` vendor clang **发出的汇编，vendor 自己的汇编器拒绝**：

```
# A) bf16 vtype：clang -mcpu=tsm-x82 -O2 发出 `vsetvli zero, s1, e32, m2, ta, mu, bf16`
$ llvm-mc -triple=riscv64 -mcpu=tsm-x82  < device.s
out/.../device.s:113:20: error: operand must be e[8|16|32|64],m[1|2|4|8|f2|f4|f8],[ta|tu],[ma|mu]
        vsetvli zero, s1, e32, m2, ta, mu, bf16                              # ← 汇编器缺 bf16 vtype

# B) 向量 FMA 家族：llvm-mc 直接 LLVM ERROR（进程 abort）
$ printf 'vfmacc.vf v10, fa0, v8\n' | llvm-mc -triple=riscv64 -mcpu=tsm-x82 -show-encoding
LLVM ERROR: Unsupported RVV instruction with funct6=0b101100 funct3=0b101 vs1=0b01010 vs2=0b01000 vm=0b1
```

逐条测试（`wt-forks/corpus/fma.txt`）确认**无法汇编**的助记符：`vfmacc.vf/vv`、`vfmadd.vf/vv`、`vfnmacc.vf`、`vfmsac.vf`、`vfmv.f.s`、`vfredosum.vs`、`vfwadd.vv`、`vfwredosum.vs`、`vfdiv.vv`（共 10 条）。段访存/FF 类 44 条（`vle16ff/vlseg3e16/vlsseg*/vluxseg*/vsseg*/vssseg*/vsuxseg*`）为”能反汇编、不能汇编“。其余实测可往返（`vfredusum.vs`、`vfredmax/min.vs`、`vfsqrt.v`、`vfcvt.*`、`vfirst.m`、`vcpop.m`、`vrgather.vv`、`vmv1r.v`…）。

## 4. RAISA 管线形态（实验 4）

`[实测]` 最小 `.rc`：

```c
__global__ void k_add(float *a, float *b, float *c){ unsigned i = threadIdx.x; c[i]=a[i]+b[i]; }
```

| 调用 | 结果 |
| --- | --- |
| `--raisa-device-only -S k1.rc`（默认 include） | `error: use of undeclared identifier 'threadIdx'`（`-noraisainc` 关掉了 SDK 头） |
| 去掉 `-noraisainc`（默认 RAISA 头链） | `…/__clang_raisa_device_functions.h:14:10: fatal error: 'krt.h' file not found`（全盘 6 层内无 `krt.h`） |
| `-noraisainc` + `__builtin_riscv_read_reg_tid_x()` | `use of undeclared identifier ...; did you mean '__riscv_read_reg_tid_x'?` |
| 裸名 `__riscv_read_reg_tid_x()`（无声明） | `error: reference to __host__ function '__riscv_read_reg_tid_x' in __global__ function`（host/device 检查生效） |

文档给出的完整 RAISA 示例**在本机可以编译通过**（`wt-forks/out/raisa_example/`）：

```bash
TX8_CC=.../llvm-toolchain/bin/clang
"$TX8_CC" --target=x86_64-unknown-linux-gnu -x raisa --raisa-rpu-arch=tsm-x82 \
  -noraisainc -noraisalib -std=c++17 -O2 -ffp-contract=off \
  -I docx/tests/llvm/gemm-walkthrough/include -I cmodel/include/include \
  --raisa-device-only -S .../gemm_raisa.rc -o device.s        # rc=0, 10 912 B
  #（-c 同样 rc=0，得 ELF64 UCB RISC-V, RVC, single-float ABI）
```

`-###` 实际阶段（`logs/raisa_device_ddd.log`、`logs/raisa_combined_ddd.log`）`[实测]`：
1. device cc1：`-triple riscv64-tsingmicro-raisa -aux-triple x86_64-unknown-linux-gnu -mllvm -vectorize-loops=false -O2 -emit-obj -aux-target-cpu x86-64 -fraisa-is-device -target-cpu tsm-x82 … -target-abi lp64f -o /tmp/gemm_raisa-tsm-x82-*.o`
   - **注意：设备侧驱动自动加 `-mllvm -vectorize-loops=false`**，宿主侧没有 → 这是”设备内核别向量化“的官方开关。
   - `-internal-isystem .../lib/clang/21/include/raisa_wrappers`（SDK 注入点）
2. `ld.lld -m elf64lriscv --whole-archive -shared -o /tmp/gemm_raisa-*.out device.o --no-whole-archive`
3. host cc1：`-triple x86_64-unknown-linux-gnu -aux-triple riscv64-tsingmicro-raisa -O2 -emit-obj … -fraisa-include-rpubinary /tmp/gemm_raisa-*.out`
4. `--raisa-combine-device` 在 `-c` 下报 `warning: argument unused during compilation`。预定义宏（`-dM -E`）device 侧 `__RAISA__=1 __RAISA_DEVICE_COMPILE__=1 __TSRPU__=1 __global__=__attribute__((global)) __riscv_xtsm82s 1000000`，host 侧 `__RAISA_HOST_COMPILE__=1`。
5. 未验证：`krt.h`、`libraisa_sim`、`libdevice` 均不在工具链内 → 设备运行链路在本机无法完成（本轮只做到编译/汇编/链接设备 ELF）。

## 5. `-O0` 问题（实验 5）

### 5.1 项目文档的原断言（`gemini-cmodel/docs/TX82_LLVM_VS_CMODEL_PAIRWISE_VERIFICATION.md` §7、`ANALYSIS_REPORT.md` §3）

> `-O1/-O2/-O3` 下 `out[]` 全为 NaN；根因是”读到从未被写入的浮点寄存器“，例 `fadd.s fa4, fa0, fa4`，而 `fa0` 在函数体内从未写入；`-O2 -fno-inline` 还会 `Unsupported RVV instruction` 后端崩溃。

### 5.2 复现结果

**(a) 能复现、且是最硬的失败：`-O2` 后端崩溃** `[实测]`

```bash
$ llvm-toolchain/bin/clang-21 --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -O2 -c corpus/vec.c
fatal error: error in backend: Unsupported RVV instruction with funct6=0b101100 funct3=0b101 vs1=0b01010 vs2=0b01000 vm=0b1
4. Running pass 'RISC-V Assembly Printer' on function '@axpy'          # 无目标文件
```
用项目自己的 kernel 与 flags 也复现（`corpus/rmsnorm_run.c`；见 §8 制品）：
```bash
$ clang-21 -O2 -ffreestanding -nostdinc --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 \
    --sysroot=$SR -I$SR/include corpus/rmsnorm_run.c -static -fuse-ld=lld -o /tmp/x.elf
fatal error: error in backend: Unsupported RVV instruction with funct6=0b000011 funct3=0b001 vs1=0b01000 vs2=0b00000 vm=0b1
```
`[推断]` 触发条件 = 自动向量化 + FP 收缩产生**向量 FMA / 向量 FP 算术**；`-O0`、`-O1` 不触发。

**(b) 无法复现”`-O1` 数值错“** `[实测]`

用与 `gemini-cmodel/kernels/tx82_rmsnorm.c` 完全同构、仅去掉 4 个 TX8 builtin 的源码（`wt-forks/corpus/rmsnorm_run.c`），vendor clang 编译 → **产物中 0 条 `0x7B` 指令**（纯标量），于是可以用 `qemu-riscv64-static` **真实执行** TX8 目标代码：

```bash
$ clang-21 --target=riscv64-unknown-linux-musl --sysroot=llvm-toolchain/sysroot -mabi=lp64f \
    -mcpu=tsm-x82 -O0|-O1 -ffreestanding -nostdinc corpus/rmsnorm_run.c -static -fuse-ld=lld
$ qemu-riscv64-static out/rmsnorm_run_O0.elf   # 与 -O1 输出 diff 为空
0 -0.18174395 0xbe3a1b16   1 -0.0373747572 0xbd191646   2 0.125315383 0x3e0052ad
3 0.306326509 0x3e9cd6d4   4 0.505658567 0x3f0172d7   5 0.723311543 0x3f392af2
6 0.959285557 0x3f7593bd   7 1.21358049 0x3f9b569b
```
**`-O0` 与 `-O1` 浮点结果逐位相同**（含 `fast_inv_sqrt` 的位运算与 FMA 收缩）。

**(c) ”读未写入 fa0“ 的根因描述不成立** `[实测]`
`-O1` 的 `fadd.s fa4, fa0, fa4`（`logs/rmsnorm_O1.s:28`）中 `fa0` **是 lp64f 的第一个浮点参数 `eps`**，与 `-O0` 的 `fsw fa0,-48(s0) … flw fa4,-48(s0); fadd.s fa0,fa5,fa4` **语义一致**；`-O0/-O1` 都把 `eps` 放在 `fa0` 传入。因此”fa0 从未被写入“是把 ABI 参数寄存器当成未初始化临时寄存器。
**(d) `-O1` 与 stock 的差异只是调度顺序** `[实测]`
vendor `-mcpu=tsm-x82 -O1` vs stock `-march=rv64i…zve64x_zvl64b -O1`（等价 feature 集）：**助记符多重集完全相同**，仅 6 处指令顺序不同（都在同一依赖链内，数据依赖保持）；`-O0` 两侧**完全相同**。→ `[推断]` fork 改变了 `-O1` 的调度/寄存器分配顺序，但没有改变指令选择，也不改变数值。

**结论**：本项目”必须 `-O0`“的真实、可复现理由是 **(a) `-O2` 直接编译失败**，而不是 `-O1` 的数值错。`-O1` 在数值上等价于 `-O0`（本机可执行验证）。修掉 `vfmacc/vfmadd` 的编码路径后 `-O2` 才有讨论余地。

## 6. fork 可证明增加/改变了什么

| # | 增量 | 证据 |
| --- | --- | --- |
| 1 | `-mcpu=tsm-x82` 目标：21 个 feature（无 A、无 D，含 zbb/zfh/zve32f/zvl64b/xtsm82s），ABI `lp64f` | §2.2；stock 拒绝 |
| 2 | `xtsm82s`（`__riscv_xtsm82s=1000000`）**同时是”RVV 重映射编码模式“开关** | §2.4 |
| 3 | **整个 RVV 空间重编码进 opcode `0x7B`（custom-3）**，150 个标准助记符受影响 | §2.4、§3.1 |
| 4 | **177 条 TX8 专有助记符**（SIMT exec/mask、NE/MMA/conv、cache、DMA/DTE、send/recv、2D 访存、sync gather/scatter、窄浮点转换、数学函数、sort），全在 `0x7B` | §3.2；`IntrinsicsRISCVXTsm.td` 一致 |
| 5 | RAISA 前端（`-x raisa`、`__global__/__device__`、`rpu_kernel`、`threadIdx`、`<<<>>>`）+ host/device 双 cc1 + `ld.lld` 设备链接 + `-fraisa-include-rpubinary` 管线；设备侧默认 `-mllvm -vectorize-loops=false` | §4 |
| 6 | `tsm-x82` 下 **VLEN 写死 256bit（VLENB=32）**，不读 `vlenb`；**未增加**标准 RISC-V 编码/指令选择/调度 | §2.3、`out/scalar_insns_*.txt` |

## 7. 仍未确定 / 未知

1. **`0x7B` 重映射的完整位级规则**：150 条 remap 中有的只换 opcode、有的还改了 bit12/bit20 等中间位；没有规则表就**无法写 cmodel 解码器**（只能逐条查 `census_full.tsv`）。vm 位语义是否与 RVV 一致也未验证。
2. **TX8 专有指令的真实语义/操作数编码**：`gemm/mma/conv/sort_*` 只有助记符+字段，无操作数宽度、累加器/内存副作用、`exec mask` 语义；`IntrinsicsRISCVXTsm.td` 只给 IR 签名。
3. **`-O2` 崩溃的完整边界**：已知触发于向量 FP 算术/FMA，但两条报错各自”真正想发哪条指令“未确定，也未确认”仅补编码表即可通过“。
4. **`-O1` 调度差异是否在别的 kernel 上放大成真错**：本报告只证明 `rmsnorm` 等价；`tid.y/z` 崩溃（docx round2）等路径未复测。
5. **RAISA 设备运行链路**：`krt.h`、`libraisa_sim`、device rt 均缺失 → 只能编到设备 ELF，无法验证启动/同步/数值。
6. **`0x7B` 之外是否还有新增指令**：宽扫（`mc_scan.py broad`，4.19M 探针）因 16-bit 指令一行两条导致解析错位，只拿到 118 条即中止，未完成。

## 8. 制品索引（全部在 `wt-forks/`）

| 路径 | 内容 |
| --- | --- |
| `out/census_full.tsv` | **371 条 census 主表**：mnemonic / asm / vendor 编码 / base(0x57) 编码 / stock 编码 / class |
| `out/vendor_custom.txt`、`out/vendor_custom_v.txt` | 原始编码普查（11 347 / 31 104 行） |
| `out/tx8_mnem_enc.txt`、`mc_scan.py` | 46 条 TX8 助记符编码；普查脚本（穷举 + `ecall` 定位标记） |
| `corpus/`、`out/codegen/`、`out/rmsnorm/` | 语料（`tiny/scalar/vec/fpacc/rmsnorm_pure/rmsnorm_run.c`、`fma.txt`、`tx8_mnemonics.txt`）与全部 `.s` 差分样本 |
| `out/raisa_example/{device.s,device.o,device.o.dis}`、`logs/raisa_*_ddd.log` | RAISA device 产物（10 912 B asm / ELF）与 `-###` 全量阶段 |
| `logs/repro_O2_backend_crash.log`、`logs/repro_mc_vfmacc_abort.log`、`logs/stock_rejects.log`、`logs/stock_objdump_on_vendor_bytes.log` | 崩溃/拒绝/`<unknown>` 原始输出 |
| `logs/rmsnorm_O0.s`、`logs/rmsnorm_O1.s`、`out/rmsnorm_run_{O0,O1}.elf`、`out/rmsnorm_O{0,1}.txt` | 项目 kernel 的 `-O0/-O1` 汇编、可执行文件、qemu 结果（逐位相同） |
| `stock/`、`stock-deb/`、`stock-bin/`、`logs/stock_tarball_listing.txt` | 两个 stock 基线与 2 GB tarball 完整清单 |

### 复现最关键的三条命令

```bash
# 1) 普查 0x7B 空间
cd wt-forks && python3 mc_scan.py custom ../llvm-toolchain/bin/llvm-mc +xtsm82s,+v out/vendor_custom_v.txt
# 2) 复现 -O2 崩溃
../llvm-toolchain/bin/clang-21 --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -O2 -c corpus/vec.c
# 3) 复现汇编器 abort
printf 'vfmacc.vf v10, fa0, v8\n' | ../llvm-toolchain/bin/llvm-mc -triple=riscv64 -mcpu=tsm-x82 -show-encoding
```
