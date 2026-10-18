# TX82 编译器工作流：现状盘点（COMPILER_STATUS）

- 盘点日期：本文件由只读侦察生成；未修改任何源码、工具链或既有文件。
- 工作区：`/home/shaokai/Desktop/work/tx82`
- 证据口径：`[实测]` = 本轮直接执行命令/读取二进制得到的输出；`[文档]` = 仅存在于 .md / .docx 的陈述；`[推测]` = 由间接证据推断。
- 结论优先级：编译器 > cmodel > ops。

---

## 1. 身份与来源（有据可查的事实）

### 1.1 安装包与规模

| 项 | 值 | 来源 |
| --- | --- | --- |
| 工具链根 | `/home/shaokai/Desktop/work/tx82/llvm-toolchain/` | `[实测]` |
| 磁盘占用 | **5.4 GB**（`du -sh`），注意：压缩包只有 2.1 GB | `[实测]` |
| 压缩包 | `llvm-toolchain.tgz` = 2 108 477 418 B（≈2.10 GB）；`setup/downloads/llvm-toolchain.tgz` 同尺寸副本 | `[实测]` |
| `bin/` 可执行 | 166 个 | `[实测]` |
| 顶层结构 | `bin/ etc/ include/ lib/ libexec/ riscv64-unknown-linux-musl/ share/ sysroot/` | `[实测]` |
| 已安装 clang 头 | `include/clang/**` 共 751 个 `.h/.def`（含私有头） | `[实测]` |
| 后端静态库 | `lib/libLLVMRISCVCodeGen.a` 13 036 034 B、`libLLVMRISCVDesc.a` 4 257 756 B、`libclang-cpp.so.21.1-rc1` 144 496 240 B | `[实测]` |

### 1.2 版本字符串与 Gerrit 提交

```
$ llvm-toolchain/bin/clang-21 --version
clang version 21.1.0-rc1 (ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf26e7a507d3ecb57e76c956dd21b5c822a6)
Target: riscv64-unknown-linux-musl
Thread model: posix
InstalledDir: /home/shaokai/Desktop/work/tx82/llvm-toolchain/bin
```
`[实测]`。Gerrit 主机 `192.168.100.107:29418`（**内网**），项目 `ts-compiler/ts-llvm-project`，提交 `b85caf26e7a507d3ecb57e76c956dd21b5c822a6`。该字符串同时出现在 `llvm_design_probe.s` 的 `.ident` 和 101 个厂商设备内核 `.so` 的 `.comment` 里 → 三方一致。

### 1.3 已注册后端与 CPU


```
$ llvm-toolchain/bin/llc --version
LLVM version 21.1.0-rc1 / Optimized build / Default target: riscv64-unknown-linux-musl / Host CPU: znver4
  Registered Targets: riscv32, riscv64, x86, x86-64
$ llvm-toolchain/bin/llvm-config --targets-built
RISCV X86
```
`[实测]`。**只有 RISCV 与 X86 两个后端** → x86 宿主 + riscv64 设备是这套工具链的原始设计意图。

`clang-21 --target=riscv64 -print-supported-cpus` 输出 54 行 CPU 列表（andes-*、generic-*、mips-p8700、rocket-*、rp2350-hazard3、sifive-* 等），第 54 行为：

```
	tsm-x82
```
`[实测]`。

```
$ llvm-toolchain/bin/llc -march=riscv64 -mattr=help | grep -i 'tsm\|x82'
  tsm-x82             - Select the tsm-x82 processor.
  xtsm82s                          - 'XTsm82S' (TSM X82 extension).
```
`[实测]`（`-mattr=help` 共 379 行）。即 `tsm-x82` 同时是一个 **CPU**（`RISCVProcessors.td`）和一个 **feature 开关 `xtsm82s`**（`RISCVFeatures.td`）。

### 1.4 `-mcpu=tsm-x82` 精确展开的 feature 集

```
$ clang-21 --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -c -x c p.c -o p.o -### 2>&1
  "-target-cpu" "tsm-x82"
  "-target-feature" "+m" "+f" "+c" "+zicsr" "+zifencei" "+zmmul" "+zfbfmin" "+zfh" "+zfhmin"
                    "+zca" "+zbb" "+zve32f" "+zve32x" "+zve64x" "+zvfbfmin" "+zvfh" "+zvfhmin"
                    "+zvl32b" "+zvl64b" "+xtsm82s"
  （紧跟约 200 条 "-target-feature" "-<x>" 显式负特性，"-target-abi" "lp64f"，"-fno-signed-char"）
```
`[实测]`，与父级给定列表逐字一致。**注意四个要点**：① `+xtsm82s` 是唯一的厂商专属正特性；② 无 `+a`、无 `+d`（**无原子、无双精度**，ABI 是 `lp64f` 单浮点）；③ VLEN 由 `+zvl32b/+zvl64b` 起，物理向量长度未知；④ 实际 ISA 串（写入 `.attribute 5` 与设备内核 `Tag_RISCV_arch`）：

```
rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_zmmul1p0_zfbfmin1p0_zfh1p0_zfhmin1p0_zca1p0_zbb1p0_
zve32f1p0_zve32x1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_zvfhmin1p0_zvl32b1p0_zvl64b1p0_xtsm82s1p0
```
`[实测]`。

### 1.5 RAISA 模式：驱动选项与字符串

`clang-21 --help` 中的 RAISA/TX8 选项（`[实测]`，逐字摘录）：

```
--raisa-compile-host-device  (默认，双侧)   --raisa-device-only   --raisa-host-only
--raisa-path=<value>  RAISA installation path        --raisa-device-lib-path=<value>
--raisa-device-lib=<value>   --raisa-rpu-arch=<value>（实测 tsm-x82）  --raisa-link
--raisa-combine-device  --no-raisa-version-check  --raisa-noopt-device-debug
--rpu-bundle-output / --no-rpu-bundle-output      --tsrpuas-path=<value>
-Xraisa-tsrpuas <arg>   -fraisa-emit-relocatable / -fno-raisa-emit-relocatable
-fraisa-short-ptr (32-bit ptr in shared AS)  -frpu-rdc (separate compilation)
-fraisa-allow-variadic-functions   -fno-raisa-host-device-constexpr
-tx8-asan  Enable TX8 Fat Pointer Based AddressSanitizer
```

二进制内关键报错/诊断串（`strings bin/clang-21`，`[实测]`）：

```
cannot find RAISA installation; Provide its path via '--raisa-path', or pass '-noraisainc' to build without RAISA includes
cannot find libdevice for %0; Provide path to different RAISA installation via '--raisa-path', or pass '-noraisalib' …
unsupported RAISA rpu architecture: %0
RAISA offload target is supported only along with --emit-llvm
riscv64-tsingmicro-raisa                      ← 设备 triple
/txriscv/libdevice                            ← libdevice 期望路径
/libdevice/  /libdevice.10.bc  /nvvm/libdevice
PseudoRAISA_GET_KERNEL_ENTRY_EXEC / PseudoRAISA_DIVERGENT / PseudoRAISA_VMV_V_X
llvm.riscv.raisa.{cos,div,divergent,fdiv,fmax,fmin,frsqrt,get.kernel.entry.exec,log2,max,min,
                  reduce.fmax,reduce.fmin,reduce.sum,sin,umax,umin,vmv.v.x}
llvm.loop.raisa.unroll
__raisa_fatbin_wrapper / __raisa_module_ctor / __raisa_module_dtor / __raisa_rpubin_handle
__raisa_print_vector_%c%u
```

驱动级 host/device 拆分（`[实测]` 自 `docx/tests/llvm/host-device-probe/raisa_split.log`）：设备 cc1 triple = `riscv64-tsingmicro-raisa`、CPU `tsm-x82`、带 `-fraisa-is-device`；随后 `ld.lld -m elf64lriscv -shared`；宿主 cc1 triple = `x86_64-unknown-linux-gnu`、通过 `-fraisa-include-rpubinary` 引入设备镜像。

### 1.6 前端契约与设备头（安装包内已带源码级头）

`include/clang/Basic/Raisa.h`（`[实测]`）：

```c
#define RAISA_RUNTIME_API_MODULE_LOAD      "txRegisterModule"
#define RAISA_RUNTIME_API_MODULE_UNLOAD    "txUnregisterModule"
#define RAISA_RUNTIME_API_REGISTER_FUNCTION "txRegisterFunction"
#define RAISA_RUNTIME_API_REGISTER_VARIABLE "txRegisterVariable"
#define RAISA_RUNTIME_API_LAUNCH           "txLaunchKernel"
#define RAISA_RUNTIME_API_CLUSTER_LAUNCH   "txLaunchClusterKernel"
#define RAISA_RUNTIME_API_PUSH_CONFIG      "txPushCallConfiguration"
#define RAISA_RUNTIME_API_POP_CONFIG       "txPopCallConfiguration"
enum class RaisaVersion { UNKNOWN, RAISA_10, NEW = 10000 };
enum class RAISAFunctionTarget { Device, Global, Host, HostDevice, InvalidTarget };
enum class RaisaFeature { RAISA_USES_NEW_LAUNCH, RAISA_USES_FATBIN_REGISTER_END };
```

`include/clang/Basic/LangOptions.def`（`[实测]`）：

```
LANGOPT(RAISA, 1, 0, …)                        LANGOPT(RAISAIsDevice, 1, 0, …)
LANGOPT(RAISAAllowVariadicFunctions, 1, 0, …)  LANGOPT(RAISAHostDeviceConstexpr, 1, 1, …)   ← 默认开
LANGOPT(RPUAllowDeviceInit, 1, 0, …)           LANGOPT(RAISAUseNewLaunchAPI, 1, 0, …)
```

**设备侧头文件随工具链发布**：`lib/clang/21/include/__clang_raisa_*.h` 共 **17 个、6226 行**（`[实测]`）：`builtin_types/lsu/dte/mma/ne/conv/math/near_mem/builtin_cache/builtin_tma/builtin_vars/builtin_intrinsics/device_functions/cooperative_groups/libdevice/runtime_wrapper/intrinsics`。其中 `__clang_raisa_ne.h` 声明 61 个 `__builtin_riscv_*`（NE 配置、GEMM/MMA、DTE、LSU/TDMA/WDMA、CONV），`clang-21` 内共有 157 个不同 `__builtin_riscv_*` 与 27 个 `__riscv_read_reg_*`（`[实测]`）。

### 1.7 后端 fork 的“可命名资产”（二进制层面可确认）

`ar t` 各库，得到 **25 个 fork 专属编译单元**（`[实测]`）：

```
libclangBasic.a : Raisa.cpp.o
libclangDriver.a: Raisa.cpp.o
libLLVMRISCVDesc.a: TX8MCInfo.cpp.o
libLLVMRISCVUtils.a: TX8BaseInfo.cpp.o
libLLVMRISCVCodeGen.a: TX8SchedStrategy / TX8LegalizeCTInstruction / TX8RegisterConflictDetector /
  TX8RegisterRename / TX8RegisterAlignment / TX8LegalizeEXEC / TX8MemoryUtils /
  TX8AnnotateUniformValues / TX8AnnotateControlFlow / TX8TransformSIMTToSIMD /
  TX8UnifyDivergentExitNodes / TX8PromoteAlloca / TX8AlwaysInline /
  TX8RemoveInlineDeviceFunctions / TX8LowerSharedPass / TX8ProfilingKernelFunc /
  TX8OptimizeConfigInstrs / TX8InsertXFRM / TX8AddressSanitizer / TX8BankAwareShared /
  TX8RecordingNPUInstrPC  (.cpp.o)
```
可用的 pass 名/开关（`[实测]`）：`tx8-address-sanitizer`、`tx8-annotate-control-flow`、`tx8-asan`、`tx8-insert-xfrm`、`tx8-legalize-ct`、`tx8-lmul-sched`、`tx8-num-npu-instrs`、`tx8-profiling-kernelfunc`、`tx8-record-npu-pc`、`tx8-vsetbarrier`；`-mllvm` 级：`-riscv-tx8-sched=`、`-riscv-vreg-align=`、`-riscv-vreg-rename=`、`-print-after=simt2simd`、`-print-after=rpu-lower-shared`。

### 1.8 厂商 opcode 表：`npu_opcode.h`

`cmodel/include/boards/riscv/andes/include/profiling/npu_opcode.h`：718 行，**348 个 `MATCH_*` + 348 个 `MASK_*`，348 个唯一名，opcode 全部在 Custom-3 `0x7B`**（`[实测]`）。分节注释：Vector Configuration / ct arith v / extension instructions / NE / LSU / DTE / SIMT / 拓扑 CSR。示例：

```c
#define MATCH_GEMM        0x4600007b   #define MASK_GEMM        0xfe00707f
#define MATCH_GEMM_PSUM   0x4e00007b   #define MATCH_MMA_FIRST_PSUM 0x9e00007b
#define MATCH_NE_SET_PREC 0x0600007b   #define MATCH_NE_SET_LENGTH  0x3e00007b
#define MATCH_VSETBARRIER 0x8200707b   #define MATCH_SEND_SYNC      0x0200007b
```

**三方交叉验证成立**（`[实测]`）：`design-baseline` 反汇编得到 `gemm` 编码；`raisa-ne-intrinsic` 反汇编得到 `gemm_psum = 0x4e05067b`、`ne_set_prec = 0x0607007b`、`ne_set_misc = 0x0e00007b`、`ne_set_mkn = 0x1e08007b`、`ne_set_length = 0x3ee7877b`；与 `npu_opcode.h` 的 `MATCH_*` 在 `MASK=0xfe00707f` 下完全吻合。

---

## 2. 已经验证过的结论（区分“文档说”和“实测过”）

判定标准：① 有可执行驱动脚本/命令文件；② 有真实退出码与输出；③ 今天能在本机重跑。三者俱备记 **A（硬证据·可复现）**；缺脚本但命令在日志里可照抄记 **B（半可复现）**；只有 prose 记 **C（文档）**。

### 2.1 A 级：硬证据且可复现

| 结论 | 证据路径 | 复现方式 |
| --- | --- | --- |
| `+xtsm82s` 真实存在，`ne_set_*`/`gemm*`/`mma*` 真实发射且编码落在 `0x7B`；IR 侧是 `llvm.riscv.ne.set.*`、`llvm.riscv.gemm.psum`、`llvm.riscv.mma.*`；同时验证了 C（非 RAISA）路径也能发射 `gemm_psum` | `docx/tests/llvm/raisa-ne-intrinsic/{build.py,build.sh,ne_intrinsic.rc,ne_builtin.c,logs/*.command.txt,logs/validation_results.json}` | `python3 docx/tests/llvm/raisa-ne-intrinsic/build.py`（8 条命令全 exit 0，脚本内含断言，`passed:true`） |
| `-O1/-O2` 不可用于设备内核：`-O2 -fno-inline` 触发后端崩溃 | `docx/tests/llvm/gemm-walkthrough/logs/initial_rvv_linux_elf.log:1`：`fatal error: error in backend: Unsupported RVV instruction with funct6=0b101011 funct3=0b110 vs1=0b01000 vs2=0b01000 vm=0b1`；`ANALYSIS_REPORT.md:162`；`gemini-cmodel/kernels/compile_kernels.sh` 注释（`fadd.s fa0, fa0, fa5` 读未定义浮点寄存器→NaN） | 直接照抄日志命令 |
| 厂商 101 个设备内核是用**同一把 fork clang** 编的（`rv64…_xtsm82s1p0`、单浮点 ABI、未 strip） | `gemini-cmodel/tools/raops_wheel_kernels/*.so` 的 `readelf -p .comment` 与 `readelf -A` | `readelf -p .comment gemini-cmodel/tools/raops_wheel_kernels/matmul.so` |
| 这 101 个 `.so` 是 `raops-0.1.0+20260811.6c98b85f-py3-none-any.whl` 内 `raops/lib/device_kernels/tx82/*.so` 的原样抽取（101/101） | 顶层 wheel + `gemini-cmodel/tools/raops_device_kernels.tsv`（101 行，含 sha256_16） | `python3 -c "import zipfile; print(len([n for n in zipfile.ZipFile('raops-…whl').namelist() if 'device_kernels/tx82' in n]))"` → 101 |
| RAISA driver 真的做两次编译（设备 `riscv64-tsingmicro-raisa`+`-fraisa-is-device`，宿主 x86_64+`-fraisa-include-rpubinary`） | `docx/tests/llvm/host-device-probe/{replay.py,results.json,driver_jobs.json,raisa_split.log}` | `python3 docx/tests/llvm/host-device-probe/replay.py` |
| TX8 后端 pass 可逐个开关并观察到汇编差异（SIMT→SIMD、vreg rename/align、sched、lmul-sched） | `docx/tests/llvm/tx8_blackbox/*.s`、`*_vs_*.diff`、`vector_results.json`；`docx/llvm/tx8-toolchain/round2/details.md` + `round2/run.py`（**501 条结构化探测，407 条成功**） | `timeout 55s python3 docx/tests/llvm/tx8_blackbox/round2/run.py abi\|control\|mathdevice\|allocation` |
| Kernel ABI = 参数被打包成一个参数块指针、按自然对齐逐项装入（6 组样例：偏移 0/8/10/12/16，`Pair{u32;u64}` → `[2 x i64]`） | `round2/details.md §1` + `tx8_blackbox/round2/abi_ints.log`、`abi_struct.log`、`manifest.json` | `round2/run.py abi` |
| SIMT→SIMD 的具体 lowering：`vid` + `vmsltu` + `vadd`/`vmul` + `vmerge` + `vse`，AVL=128 | `tx8_blackbox/kernel.rc`、`simt_snapshot.log:39` | 同上 |
| OpenCL C 前端入口存在且可用（`__kernel`/`get_global_id` → `rpu_kernel` + `__clang_ocl_kern_imp_add`），但 `get_global_id` 仍是未 lower 的外部调用，样例无 `vid.v` | `docx/tests/llvm/opencl-front-probe/{add.cl,add.ll,add.s,results.json,README.md}` | 命令见 `results.json` |
| RAISA 与 CUDA 语法在前端是**同构**的（`RAISAKernelCallExpr` vs `CUDAKernelCallExpr`） | `docx/tests/llvm/raisa-cuda-syntax-comparison/{results.json,raisa.ast.txt,cuda.ast.txt}` | 命令见 `results.json`（`-cc1 -x raisa/cuda -fsyntax-only -ast-dump`） |
| 普通 C 的 `gemm`/`inst_barrier`/`read_reg_tid_x` intrinsic 存在：`llvm.riscv.gemm(i64,i64,i64)` → `gemm a0,a1,a2`；`llvm.riscv.inst.barrier()` → `inst_barrier zero,zero,zero` | `docx/tests/llvm/design-baseline/{llvm_design_probe.c,.ll,.s}` | B 级（无驱动脚本，须从 `llvm_design_driver.log` 抄 `-cc1` 行） |
| 多核/多 tile 提交与执行（8CPU×双NPU） | `docx/tests/llvm/multicore-dispatch/`、`eight-cpu-dual-npu/`（后者编译 `-march=rv64imac`，**未链接平台 API，未执行**） | `logs/*.command.txt` |

### 2.2 B 级：有产物但脚本缺失或部分环节未闭环

- `design-baseline`：`llvm_design_probe_backend.log` **0 字节**（空），只有 `llvm_design_driver.log`（8.7 KB）与 `.s`（1516 B）。命令藏在 driver 日志里，可照抄但**没有 build 脚本**。
- `gemm-walkthrough`（192 文件）与 `hip-compatibility`（162 文件）：两者都带成对的 `*.command.txt` + `*.log`，并以 `.json` 汇总；`gemm-walkthrough/run_raisa.sh`、`tx82-c-runtime-vs-raisa/run_all.sh` 可驱动。闭环程度：能编译→链接→在 cmodel/Spike 上执行（见下），但**未在真机执行**。
- `install-inspection/*.log`：只是 tarball 文件清单扫描，无分析结论。

### 2.3 C 级：纯文档，无实验支撑（**不得当作证据**）

| 文档 | 问题 |
| --- | --- |
| `docx/llvm/新NPU编译器开发方案与任务清单.md`(40KB)、`AI编译器架构师实施指南_TX82与自研加速器.md`(110KB)、`NPU语言接口草案_矩阵向量与异步依赖.md`、`Tensor扩展从编译器到CPU分发及ELF运行链.md`、`8CPU每CPU双NPU的编程与编译方案.md`、`多CPU双NPU编译器与Runtime评审问题清单.md`、`端到端软件架构简图*.md`、`00-导读-LLVM后端.md` | 方案/草案/评审/导读，无实验 |
| `gemini-cmodel/docs/TX82_RISCV_OFFICIAL_ISA_MANUAL.md`（38 KB，自称“全量完备官方标准规范”“184 条指令”） | **不可信，已证伪**：其“前言”称依据 `IntrinsicsRISCVXTsm.td`（437 行、177 个 `def/defm`），但该仓库树里该文件实际 **436 行、39 个 `def`**；第 11 章 184 行表格的 funct7 列与真实编码矛盾（例：表内 `vfadd_vs` funct7=`0010101`，真实 `MATCH_VFADD_V_VS=0x0000507B` → funct7=0；`vsetvli_bf` funct7 写 `0000001`，真实 `MATCH_VSETVLI_NS=0x0000707B` → funct7=0）；助记符名（`_vs`、`_vs_mask`、`vsetvli_bf`）在 `npu_opcode.h` 与 `.td` 中都不存在；全文只出现 1 个 8 位十六进制数，无任何反汇编引用。**真实 opcode 以 `npu_opcode.h` + `.td` 为准（348 条）**。 |
| `gemini-cmodel/docs/SPIKE_EMBEDDING_AND_INTEGRATION_GUIDE.md`、`docx/llvm/llvm-project-design-analysis.md`（自述“当前目录没有完整 llvm-project 源码”）、`docx/llvm/riscv-chip-recognition-analysis.md` | 结论正确/受限于当时无源码 |

### 2.4 文档与实测的**冲突点**

1. **opcode 条数**：`ANALYSIS_REPORT.md` §1 写 “339 条 `MATCH_*`”，实测 `npu_opcode.h` 为 **348 条**；`TX82_RISCV_OFFICIAL_ISA_MANUAL.md` 写 “184 条”。三者不一致，实测值 = 348。
2. **SIMT 是否有硬件支撑**：`TX82编译器如何区分主机侧与设备侧.md` 谨慎地不下结论；而 `gpu`-风格 API 在头文件里是**纯模拟**（见 §3.4）。`CMA/SPIKE_EMBEDDING` 等文档倾向暗示“有硬件 barrier/scheduler”。
3. **`-O2` 可用性**：多数设计文档默认设备 kernel 用 `-O2`；实测必须 `-O0`。
4. **`llvm-project-design-analysis.md` 早期章节** 把 `llvm-toolchain/` 当“安装产物、无源码”，但 `/tmp` 下确实存在含 `IntrinsicsRISCVXTsm.td` 的源码树（§3.1）——该文档已在头部自我更正。

---

## 3. 尚未证明 / 未知的

### 3.1 fork 源码：**部分可得，但只存在于 `/tmp`，随时可能被清理**（最高风险项）

`[实测]` `/tmp` 下有至少 10 份同名树（`tv25, tv25f, vfy21, seqfinal, series0022, series0024, v19, vseq6, vseq22, tv`），每份 ≈2.5 GB。以 `/tmp/tv25` 为例：

- 是 git 仓库，**只有 1 个提交 `4a919091d "base"`，无 remote**，即“上游 21.1.0 基线 + 未提交的 fork 改动”。
- `git status --short` = **29 个 `??`（新增）+ 104 个 ` M`（修改）**。
- 新增（29 个）**包含真正的 fork 源头文件**：
  - `llvm/lib/Target/RISCV/RISCVInstrInfoXTsm82s.td` —— **4071 行、423 个 `def/defm`**，完整的 xtsm82s 指令编码定义。例：
    ```
    def XTsm_GEMM : XTsm82sInst<(outs GPR:$rd), (ins GPR:$rs2, GPR:$rs1), "gemm", "$rd, $rs2, $rs1", …> {
      let Inst{6-0} = 0b1111011;  let Inst{14-12} = 0b000;  let Inst{31-25} = 0b0100011; … }
    def XTsm_NE_SET_PREC : … { let Inst{31-25} = 0b0000011; … }
    ```
    → `0x4600007b` / `0x0600007b`，与 `npu_opcode.h` **逐位一致**。
  - `llvm/include/llvm/IR/IntrinsicsRISCVXTsm.td`（436 行、39 个 `def`：`int_riscv_raisa_*`、`int_riscv_read_reg_*`、`int_riscv_get_pred_mask`、`int_riscv_is_explicit_cluster` …）
  - `llvm/include/llvm/Support/TX8AddrSpace.h`
  - `clang/lib/Sema/SemaRAISA.cpp`、`clang/lib/Sema/SemaCT.cpp`、`clang/lib/CodeGen/CGRAISATS.cpp`、`CGRAISARuntime.{cpp,h}`、`clang/lib/Driver/ToolChains/Raisa.{cpp,h}`、`clang/lib/Basic/Raisa.cpp`、16 个 `__clang_raisa_*.h`
- 修改（104 个）覆盖 `RISCVFeatures.td`、`RISCVProcessors.td`、`RISCVInstrInfo.td`、`RISCVDisassembler.cpp`、`RISCVInstPrinter.cpp`、`RISCVTargetParser.*`、`Triple.cpp`、`CallingConv.h`、`MCContext.h`…
- **缺口**：`libLLVMRISCVCodeGen.a` 里那 22 个 `TX8*.cpp.o` 对应的 **`.cpp/.h` 源文件在 `/tmp/tv25` 中完全不存在**（`ls llvm/lib/Target/RISCV/TX8*` → 不存在；`grep -rn TX8SchedStrategy …/*.cpp` → 无命中）。因此该树**无法完整重建发行版后端**。
- 工作区自身：**没有任何 `ts-llvm-project` 的 tarball / patch / git 对象**；`setup/llvm-project-21.1.0.src` 与 `setup/downloads/llvm-project-21.1.0.src.tar.xz` 均为 stock。改内网 Gerrit 不可达（无内网假设）。

**结论**：encodings + intrinsic 声明 + clang 前端 RAISA 语义 **目前可得（/tmp，易失）**；TX8 后端 pass 实现 **不可得**。

### 3.2 `xtsm82s` 到底加了什么：编码已知，语义大多未知

- 已有：348 条 opcode 的 match/mask（`npu_opcode.h`）、423 个 TableGen 定义（`.td`）、real 汇编/反汇编往返（`raisa-ne-intrinsic`）。
- 未有：**绝大多数指令的操作数语义、副作用、内存效果、保留字段、异常行为**。已由实测确认语义级别的仅：`gemm / gemm_psum / mma / mma_first / mma_first_psum / mma_last / ne_set_{prec,misc,mkn,length} / inst_barrier / l0_flush / l1_flush`（以及 `round2 §10` 提到的编码目录）。
- 未有：**设备 ABI 完整规范**。`round2/details.md §1` 只覆盖 6 组样例，明确未覆盖 packed struct、非平凡 C++ 对象、嵌套聚合、参数块尾部填充；无 host 侧打包规范。
- 未有：**`vsetxform`/`vsetstart`/`vset2dvload0/1`/`isetexec*`/`mem_allocate` 等控制指令的语义**（只在 opcode 表里存在）。
- 未有：显式 `-mattr=+xtsm82s` 与 `-mcpu=tsm-x82` 是否等价（两者都接受，但未做差分实验）；也未验证 `-march=rv64gcv_xtsm82s` 是否可用（`ANALYSIS_REPORT` 称可用，本轮未复核 → `[文档]`）。

### 3.3 RAISA 如何决定 host vs device

- 已得（头文件级）：`RAISAFunctionTarget{Device,Global,Host,HostDevice,InvalidTarget}`；`SemaRAISA::IdentifyTarget`/`inheritTargetAttrs` 符号存在；`RAISAHostDeviceConstexpr` 默认 1（未标注 constexpr 视为 `__host__ __device__`）；`RAISAKernelCallExpr` 与 `CUDAKernelCallExpr` 同构；`-fno-raisa-host-device-constexpr` 可关。
- 已得（driver 级）：`--raisa-host-only/device-only/compile-host-device` 控制**两趟编译**，`-fraisa-is-device` 标记设备趟。
- **未得**：函数级判定规则（哪些隐式函数/模板/虚函数会进入设备侧）、`__global__` 宿主 stub 的完整生成契约、跨 device 调用的合法性诊断全集、`--raisa-combine-device` 的产物形态。`SemaRAISA.cpp` 在 `/tmp/tv25` 里可读，但**未读**；即便读了也没有对应测试可验证。

### 3.4 SIMT 是**模拟**的，不是硬件语义

`lib/clang/21/include/__clang_raisa_device_functions.h`（`[实测]`，逐字）：

```cpp
__device__ __forceinline__ static uint32_t __laneid() { return threadIdx.x & 0x7f; }   // 128 lane
template <typename T> T __shfl_sync(__uint128_t mask, T var, uint32_t srcLane, uint32_t width = 128) {
  __shared__ T smem[128]; smem[__laneid()] = var; … return smem[srcLane + groupId * width]; }
__device__ __forceinline__ static void __threadfence_block() { }        // 空函数
static __device__ __forceinline__ void __syncthreads() { }              // “there is not tile scheduler”
__device__ __forceinline__ static void __threadfence() { __builtin_riscv_l0_flush(0,2,0); __builtin_riscv_l1_flush(0,2,0); }
```
→ **warpSize=128、`__syncthreads()` 是空操作、`__shfl_*` 用 shared memory 轮转、`blockDim.y/z` 恒为 1**。这解释了 `gemm-walkthrough`/`multicore-dispatch` 里的同步为何依赖 `inst_barrier`/`vsetbarrier` 而非 tile scheduler。**尚无任何实验证明这些模拟语义与真机一致。**

### 3.5 `libdevice` / `--raisa-path` / `tsrpuas`

- `[实测]` 期望布局为 `<raisa-path>/libdevice`（bundled string 里有 `/txriscv/libdevice`、`/libdevice/`、`/libdevice.10.bc`）。
- `[实测]` 工作区内**不存在**任何 RAISA 安装目录：`/txriscv`、`/opt/txriscv` 均不存在；全盘 `find -iname "*libdevice*"` 只命中 stock LLVM 测试夹具、triton/numba 的 CUDA libdevice；**不存在 `tsrpuas`**（RAISA 的 RPU 汇编器，`--tsrpuas-path` 指向它）。
- `[实测]` `-noraisainc -noraisalib` 能绕过两者并成功编译到 `.s`（`raisa-ne-intrinsic` 走的就是这条路）→ **设备汇编路径可用，设备链接/打包路径不可用**。

### 3.6 `libraisa_sim*.so` 是什么、谁用

`[实测]` `cmodel/lib/` 下三份：`libraisa_sim.so` 17 664 856 B、`libraisa_sim_core.so` 49 462 784 B、`libraisa_sim_asan.so` 17 664 776 B（另有 Open3D-PIMC 与 build-repil 的副本）。`libsystemc.so.2.3` 与之同目录 → SystemC 周期模型。

`[文档+实测交叉]` `ANALYSIS_REPORT.md` §1：`libraisa_sim.so` = 宿主 API 层（`txMalloc/txFree/txMemcpy/txModuleLoad/txLaunchKernel*`、`txRegisterFunction`、`tccl*`、`hw_dispatch_*`、`setSpikePGMode`、C++ 类 `MultiTileSpikeSim`(58 方法)/`TxStreamImpl`/`MemAllocator`），导出 **105 个 C API**（此数字亦被 `gemini-cmodel/runtime/src/tx_runtime_tccl.cpp` 的注释独立复述）；`libraisa_sim_core.so` 内嵌 Spike（7116 个 RISC-V/Spike 符号，一 tile 一线程 + `TileEventManager`）。

**谁用**：`gemini-cmodel/runtime/` 是**厂商库的替代实现**（重写同名 `tx*`/`tccl*` 符号），不是消费者；`cmodel/` 只是二进制真值。**编译器侧没有任何测试链接或调用它**。厂商 kernel `.so` 的未定义符号是 `krt_*`（`krt_get_chip_id / krt_get_tile_id / krt_get_die_num_per_chip / krt_barrier_in_chip / krt_barrier_simd / krt_flush_dcache_range / krt_print_vector`，`[实测]` `nm -D --undefined-only`），对应设备头里的 `#include "krt.h"` 与 `KRT_ABORT()`；`krt.h` 在工作区只有 Andes 板级副本（`cmodel/include/boards/riscv/andes/include/krt_interface/krt.h`）。**`krt_*` 由谁实现、跑在哪 — 未证明。**

### 3.7 厂商 kernel `.so` 的构建配方：**工作区内不存在**

`[实测]` 事实：`gemini-cmodel/tools/raops_wheel_kernels/` 101 个 RISC-V `.so` 与 `raops-…whl` 内 `raops/lib/device_kernels/tx82/*.so` 一一对应（101 行 TSV 带 sha256_16）；`.comment` = fork clang 21.1.0-rc1 `b85caf26…`；`Tag_RISCV_arch` 含 `xtsm82s1p0`；`Flags: RVC, single-float ABI`；带 debug_info、未 strip；每个 `.so` 静态链了一份公共运行时（274–275 个定义符号、262 个 `main_kernel_*` 入口、跨内核 1810 个唯一符号）。

`[实测]` **没有**任何 Makefile / CMake / shell / Python 脚本能重建它们。工作区里唯一的设备内核构建脚本是 `gemini-cmodel/kernels/compile_kernels.sh`（把 `tx82_*.c` 编成 7 个自研 `.so`，**不是**这 101 个），其命令为：

```
clang-21 -O0 -ffreestanding -nostdinc -fPIC --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -I<src> -c <k>.c
```
即厂商那 101 个的**等价配方只能由这个脚本 + `.comment` 反推**，无法证明确实相同（`[推测]`）。`raops_wheel_inventory.py` 只做只读盘点，不解包到仓库。

### 3.8 其它未解

- `-tx8-asan`（TX8 Fat Pointer ASAN）、`TX8ProfilingKernelFunc`(PMU)、`TX8RecordingNPUInstrPC`、`TX8InsertXFRM`、`TX8OptimizeConfigInstrs` 的**语义与是否默认开启**未知。
- `-O1/-O2` 浮点错编的**根因**未知（是 ISel 还是 `TX8SchedStrategy`/register 分配？），因此**无法判断修复代价，也无法判定其它优化等级下还有多少错误**。
- 运行时/ABI 层：`txLaunchKernel` 等宿主 API 的签名可从 `Raisa.h`+头文件得，但**未与具体 runtime 版本对齐**；`--raisa-rpu-arch` 支持哪些 arch 值、版本检查（`RAISA_10 / NEW`）如何工作，未知。

---

## 4. 可选路线与判定性实验（按性价比排序）

成本单位：≈1 个 subagent session 的机械工作量。

| # | 待解问题 | 最便宜的决定性实验 | 成本 | 结果解读 |
| --- | --- | --- | --- | --- |
| **E1** | `/tmp/tv25` 是否真是 fork 源码、内容是否完整 | `cd /tmp/tv25 && git status --short \| wc -l; ls llvm/lib/Target/RISCV/RISCVInstrInfoXTsm82s.td; grep -c '^def' llvm/include/llvm/IR/IntrinsicsRISCVXTsm.td`，然后**把 29 个 `??` 与 104 个 `M` 的 diff 归档到工作区**（只复制，不改动工具链） | 0.5 | 若确认 → 立刻固化，fork 前端语义 + 全部指令编码到手；后端 pass 仍缺 |
| **E2** | TX8 后端 pass 源码是否真的不可得 | 在全盘（含 `/tmp`、`~/.cache`、`*.tgz` 成员表、`lib*.a`）搜 `TX8SchedStrategy.cpp`/`TX8TransformSIMTToSIMD`：`find / -name 'TX8*.cpp' 2>/dev/null`；`tar tzf /tmp/*/tx8be-mlir.tgz \| grep -i tx8`；`strings -a libLLVMRISCVCodeGen.a \| grep -c .` 作兜底 | 0.5 | 命中任一 → 后端可改造；全空 → 只能在“黑盒 + 反汇编”路线上做（与现有 TX8 探测路线一致） |
| **E3** | `xtsm82s` 指令语义（除已测 11 条外） | 写一个**参数化汇编往返探针**：从 `.td`/`npu_opcode.h` 生成 348 条指令的 `.s`，逐条喂 `clang -c` + `llvm-objdump -d --mattr=+xtsm82s,+v`，比对编码是否回环一致；不一致者即 printer/parser 缺口的清单 | 1–2 | 回环一致率 = 后端对该指令的支持成熟度；失败项是后端待修清单 |
| **E4** | `-O2` 错编的根因与影响面 | 最小化复现：对 `float acc=0; for(i) acc+=a[i]*b[i];` 分别用 `-O0/-O1/-O2/-O3`、开关 `-mllvm -riscv-tx8-sched=false`、`-fno-slp-vectorize -fno-vectorize`、`-fno-unroll-loops` 编到 `.s`，diff 出「写未定义寄存器」的那一档；再对全 101 个厂商 kernel 的源等价物做同样扫描 | 2 | 定位到具体 pass → 可用 `-mllvm` 规避（本机可解）；定位不到 → 必须等 fork 后端源码 |
| **E5** | host/device 判定规则 | 构造语法矩阵（`__host__`/`__device__`/`__global__`/`__host__ __device__`/constexpr/模板/虚函数/隐式构造）× `--raisa-host-only`/`--raisa-device-only`，`-fsyntax-only -ast-dump` 收集诊断；与 `SemaRAISA.cpp` 的 `IdentifyTarget` 源码对读 | 1.5 | 得到可写进规范的判定表；诊断缺失项 = 前端待补 |
| **E6** | `libdevice` 内容 | 在 `raops-*.whl` / `torch_raisa-*.whl` / `cmodel/` / `open3d` 副本里搜 `libdevice`、`.bc`、`__clang_raisa_libdevice` 的实现符号；`nm -D cmodel/lib/libraisa_sim.so \| grep -i libdevice\|__nv\|rsqrt` | 0.5 | 找到 → `--raisa-path` 可自建；找不到 → 只能用 `-noraisalib` + 头文件内联实现 |
| **E7** | 厂商 101 内核的等价构建配方 | 用 `compile_kernels.sh` 的配方编 `tx82_gemm.c`，与 `gemini-cmodel/tools/raops_wheel_kernels/matmul.so` 做 `.comment`/`Tag_RISCV_arch`/undefined-symbol/入口命名对比；并检查 `torch_raisa` whl 内是否有编译期脚本 | 1 | 结构一致 → 配方可复现；不一致 → 明确标注“厂商配方未知” |
| **E8** | SIMT 模拟语义 vs 真机 | 无真机则**不可判定**。可做的是：证明 `__shfl_*_sync`/`__syncthreads` 只在寄存器/shared 层实现（读 IR 确认无 barrier 指令），并把结论降级为“编程模型” | 0.5 | 只能得到“编译器行为”结论 |
| **E9** | `krt_*` 提供者 | 在 `libraisa_sim.so` 与 101 个 kernel `.so` 内查 `krt_*` 的定义/入口：`nm -D --defined-only cmodel/lib/libraisa_sim*.so \| grep krt_`；对 kernel `.so` 看是否走 PLT 到 runtime | 0.5 | 找到 → 设备 runtime ABI 可写 |

---

## 5. 建议的任务清单（可逐个交给新 subagent 会话）

格式：**目标 / 完成判据（决定性证据）/ 规模**。全部只依赖本工作区，不假设内网。

| 优先级 | 任务 | 目标 | 完成判据 | 规模 |
| --- | --- | --- | --- | --- |
| **T1** | **固化 fork 源码快照（E1+E2）** | 把 `/tmp` 下包含 `RISCVInstrInfoXTsm82s.td`、`IntrinsicsRISCVXTsm.td`、`clang/lib/**/RAISA*` 的树，连同 `git diff` 归档进工作区（例如 `docs/compiler/fork-source/`）；同时穷尽搜索后端 pass 源码 | 工作区内出现 `RISCVInstrInfoXTsm82s.td`(4071 行) + `IntrinsicsRISCVXTsm.td` + 29 个新增文件的 diff；对 `TX8*.cpp` 给出“有/无”的确定性结论 | 1 |
| **T2** | **指令编码权威表（E3）** | 以 `.td` + `npu_opcode.h` 为准，生成 348 条 `xtsm82s` 的机器可读表（助记符/funct7/funct3/操作数/来源），并标注“已实测往返通过” | 表存在且与实测反汇编逐位一致；不一致项单列 | 1–2 |
| **T3** | **`-O2` 正确性缺陷定位与规避矩阵（E4）** | 给出最小复现 + 每个优化等级 × 每个 `-mllvm` 开关的 pass/fail 矩阵；决定设备内核的官方编译基线（现为 `-O0`） | 一份 `-O0..-O3 × {tx8-sched, vreg-align, vreg-rename, lmul-sched, vectorize}` 的实测矩阵 + 最小复现 `.c/.s` | 2 |
| **T4** | **host/device 语义规范（E5）** | 产出“RAISA 函数侧判定表 + 诊断清单”，可替代 `SemaRAISA.cpp` 的模糊描述 | 语法矩阵 30+ 用例的 `-ast-dump`/诊断输出 + 与源码对读结论 | 1.5 |
| **T5** | **设备 ABI 契约** | 把 `round2 §1` 的 6 个样例扩展到 packed/嵌套/多 ABI/尾部填充，冻结参数块布局 | 参数块布局规范 + 可编译的 host/device 双向样例 | 2 |
| **T6** | **设备 kernel 构建基线（E7）** | 把 `compile_kernels.sh` 提升为“官方设备内核基线”，明确 `-O0`、`-ffreestanding -nostdinc`、`lp64f`，并说明与厂商 101 内核的异同 | 脚本 + 与厂商 `.so` 的结构对比报告 | 1 |
| **T7** | **把 CMODEL 的编译器侧证据并入编译器工作流** | `TX82_RISCV_OFFICIAL_ISA_MANUAL.md` 标注为**不可信**并给替代（T2 的表）；`ANALYSIS_REPORT.md` 的 “339 条” 更正为 348 | 文档更新 + 一条 `grep -c MATCH_` 复现命令 | 0.5 |
| **T8** | **设备 runtime ABI（E6+E9）** | 明确 `krt_*` 提供者、`libdevice` 是否存在、`libraisa_sim.so` 105 个 API 与 `tx*` 签名清单 | 符号清单 TSV + 结论“可自建/不可自建” | 1.5 |

**推荐先启动：T1 → T2 → T3。** T1 是唯一有时效性的（`/tmp` 易失），T2 是后续一切编译器工作的地基，T3 直接决定设备内核能不能用优化编译。

---

## 附：复现本文件所有 `[实测]` 结论的最小命令集

```bash
cd /home/shaokai/Desktop/work/tx82
llvm-toolchain/bin/clang-21 --version
llvm-toolchain/bin/llc --version; llvm-toolchain/bin/llvm-config --targets-built
llvm-toolchain/bin/clang-21 --target=riscv64 -print-supported-cpus | tail -5
llvm-toolchain/bin/llc -march=riscv64 -mattr=help | grep -iE 'tsm|x82'
llvm-toolchain/bin/clang-21 --help | grep -iE 'raisa|rpu|tsrpuas|tx8-asan'
strings -a llvm-toolchain/bin/clang-21 | grep -i raisa | sort -u
for f in llvm-toolchain/lib/*.a; do ar t "$f" | grep -E '^(TX8|Raisa)'; done
grep -c '^#define MATCH_' cmodel/include/boards/riscv/andes/include/profiling/npu_opcode.h
readelf -p .comment gemini-cmodel/tools/raops_wheel_kernels/matmul.so
git -C /tmp/tv25 log --oneline; git -C /tmp/tv25 status --short | head
wc -l /tmp/tv25/llvm/lib/Target/RISCV/RISCVInstrInfoXTsm82s.td
python3 docx/tests/llvm/raisa-ne-intrinsic/build.py
python3 docx/tests/llvm/host-device-probe/replay.py
timeout 55s python3 docx/tests/llvm/tx8_blackbox/round2/run.py abi
```

---

## 9. 进度总览（**parent 维护**，随每轮更新；详细证据见 `fork-source/reconstruction/README.md` 与 `RAISA_BACKEND_GAPS.md`）
**更新：2026-10-16 19:20**

| 口径 | 数值 | 证据/复现 |
|---|---|---|
| musl 主路（14 内核 × 4 配置） | **56/56 结果一致**（46 个 `.o` 逐字节一致 + 10 个"双方都失败且诊断逐字节一致"） | `python3 wt-raisa-be/tools/musl_ab.py --a wt-raisa-be/ab_artifacts/clang-pre-simt --b wt-raisa-be/build/bin/clang-21 --jobs 2` |
| 317 格 parity | 逐字节一致 **136** / OURS-GAP **77** / byte-diff 37 / 厂商侧 66 / 我方额外 1；**`.c` 侧零缺口** | `wt-raisa-be/parity/round6_norm.tsv` |
| RAISA `.rc` oracle | **4 格逐字节一致**（`fadd_rn_{f,f16}`、`fmax_{f,f16}`）/ 18 格编译成功但字节不同 / 81 格编译失败 / 11 格双方都失败 | `python3 wt-raisa-be/tools/raisa_oracle.py --cc wt-raisa-be/build/bin/clang-21 --all --jobs 2` |
| 补丁归档 | RAISA 线 `0001–0008` 已折算为 **`0047–0054`**，落三处：`fork-source/reconstruction/`、`fork-source/patches-series/`（现 73 个）、`wt-raisa-be/patches/` | 本文件同目录 `fork-source/reconstruction/0047…0054` |

**剩余工作分类（110 格）**：① 仅寄存器选择不同 **6** 格（根因 `implicit $exec`，我方无 `EXEC` 物理寄存器，需 RAISA-gated 新增）；② 仅缺 BF16 vtype bit 8 **5** 格（在跑）；③ 其它 **7** 格（`config_*_device` 缺 `TX8OptimizeConfigInstrs`；`imm_l1_flush_{31,32}` 操作数顺序）；④ **编译失败 81 格** —— 其中 **53–66 格**卡在 `simt2simd` 的**内建映射表**（`read.reg.{tid,ctaid,nctaid,ntid}.{x,y,z}` → `vid`/`vmv.v.x`；`raisa.{sin,cos,log2,exp2,fdiv,frsqrt,reduce.*,max,min,umax,umin}` → 向量指令），其余为 `rpu-lower-shared`（共享地址常量：L0A `0x40000000`、L0W `0x40100000`、`mem_l1_size_1` `0x101000000`）与家族余项。

**并行线**：A `wt-raisa-be`（`simt2simd`/BF16）、B `wt-raisa-r2`（`rpu-lower-shared` + R2 pass）。cmodel 线暂停（恢复包与清单见 `CMODEL_BACKLOG.md`）。

---

## 10. 计费时段纪律（**所有 agent 必须遵守**；2026-10-16 起）
官方规则（[Models & Pricing](https://api-docs.deepseek.com/zh-cn/quick_start/pricing/)）：**北京时间周一至周五（不含法定节假日）9:00–12:00、14:00–18:00 = 高峰（全价）**；
**其余（含周末、法定节假日全天）= 空闲（半价）**。`deepseek-flash`：输入未命中 1元/2元、输出 4元/8元 每百万 tokens。

**闸门命令**：`tx82/tools/offpeak.sh`（exit 0 = 空闲；exit 1 = 高峰；输出含"距切换多久"）。
**纪律**：
1. **高峰时段不启动新的重活**（新一轮构建/大语料/新族实现）：先跑闸门，若为高峰 ⇒ 在**干净边界**收手（当前工作落补丁/交接），`sleep 300` 循环等，或直接报告"等空闲时段再继续"。
2. 已开始的**小步**（单个构建、一次 oracle）允许跑完，不要半途中断留下半成品。
3. 空闲时段可以放开跑（含夜间、周末）。
4. `tools/cn_holidays.txt` 维护法定节假日期（每行 `YYYY-MM-DD`），缺失时按普通工作日判——**节假日按普通工作日判会误收全价，发现日期缺失请补**。
