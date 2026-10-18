# 用厂商 CModel 执行设备代码 —— 可复现配方

日期：2026-10-16。适用范围：把 `-mcpu=tsm-x82` 编出的 RISC-V 设备模块加载进**厂商自带
`libraisa_sim` CModel** 真跑，并对结果做逐位比对。

> 之前 `docs/compiler/OPT_BASELINE_MATRIX.md` 里"没有任何可用模拟器能执行 0x7B"的结论
> 是**错的**：qemu 不能，厂商 CModel 能。本配方把这条路固定下来。

所有脚本在 `wt-cmodelrun/`，日志在 `wt-cmodelrun/logs/`，结果在 `wt-cmodelrun/out/`。

---

## 0. 一次性环境

```bash
cd /home/shaokai/Desktop/work/tx82
source setup/activate.sh          # 必须；它把 $PWD/cmodel/lib 加进 LD_LIBRARY_PATH
```

没有 `activate.sh` 时 CModel 动态库找不到，`txModuleLoad` 会以
`Load lib failed, ret=2` 之类的形式失败或直接 abort。`activate.sh` 还会设
`SPIKE_LOG_LEVEL=3 KCORE_LOG_LEVEL=3`（设备侧日志级别）。

---

## 1. 设备模块（device .so）

设备侧是一个 **RISC-V 共享对象**，不是 ELF 可执行文件：

```bash
CLANG=llvm-toolchain/bin/clang-21          # 或 wt-forkbuild/build/bin/clang（重建版本）
KRT=cmodel/include/boards/riscv/andes/include/krt_interface

$CLANG --target=riscv64-unknown-linux-musl \
       --sysroot=llvm-toolchain/sysroot \
       -mcpu=tsm-x82 -mabi=lp64f -std=c11 \
       -fPIC -nostdlib -shared -fuse-ld=lld -Wl,-Bsymbolic \
       -I wt-cmodelrun/common -I $KRT \
       -O0 \
       wt-cmodelrun/device/case_entries.c \
       -o build/my_case.vendor.O0.so
```

要点与坑：

1. **`--sysroot` 必须给**：`-nostdlib` 也仍要它来解析 `stdint.h` 这类 target 头。
   重建的 clang（`wt-forkbuild/build/bin/clang`）自己没有 `sysroot/` 目录，
   **只读复用 vendor 的 `llvm-toolchain/sysroot` 即可**（不要改 `llvm-toolchain/`）。
2. **重建树里没有 `ld.lld`**，要显式 `--ld-path=llvm-toolchain/bin/ld.lld`
   （vendor clang 用 `-fuse-ld=lld` 自己就能找到）。
3. `-Wl,-Bsymbolic` 是必需的（probe/gemm 两处既有 recipe 都带）。
4. **模块必须自洽**：`-shared` 允许未定义符号，但 `txModuleLoad` 会因为解析不到而
   直接失败（`[tile_id:N] Load lib failed, ret=2`）。所以不要把用不到的 entry
   一起编进来——`wt-cmodelrun/device/case_entries.c` 里每个 entry 都用
   `#ifdef TX82_WITH_<CASE>` 包着，`build_case.sh` 只定义当前 case 的那个宏。
5. `tx82_add_kernel` 这类 gemini kernel 用的是 `TX82_KERNEL_EXPORT`
   （只有 `visibility("default")`），**不会**进 `ExportedDYNSYMTab`，
   `txModuleGetFunction` 找不到它。做法是加一个 ~20 行的 shim：
   `void <k>_entry(void *packed)` 里解包参数再调用原 kernel，
   并用 `KERNEL_FUNC_EXPORT(<k>_entry)` 导出（`krt_common.h:33`）。
   **不需要改 `gemini-cmodel/`**。

### 设备入口 ABI

```c
void my_entry(void *packed);      /* a0 = 参数块设备地址 */
KERNEL_FUNC_EXPORT(my_entry)      /* 注册进 ExportedDYNSYMTab */
```

参数块布局定义在 `wt-cmodelrun/common/cases.h`，host 与 device 共用同一份头文件
（字段只有 `u64` 设备指针 / `u32` 标量，`sizeof` 有 `static_assert` 守着）。
host 侧**按值**把参数块交给 `txLaunchKernelSimd`，runtime 负责把它放进设备的参数区，
这正是 `a0 = 参数块` 的契约。

---

## 2. Host 驱动（x86）

```bash
source setup/activate.sh
g++ -std=c++17 -O2 \
    -I "$PWD/cmodel/include/include" -I wt-cmodelrun/common \
    wt-cmodelrun/host/tx82_run_host.cpp \
    -L "$PWD/cmodel/lib" -lraisa_sim -Wl,-rpath,"$PWD/cmodel/lib" \
    -o wt-cmodelrun/build/tx82_run_host.elf
```

运行：

```bash
wt-cmodelrun/build/tx82_run_host.elf <module.so> <case> <n> <tag> <indir> <outdir>
# 输入：<indir>/in0.bin ... 原始 float32；输出：<outdir>/out.bin + info.bin
```

调用序列（顺序不要改，照抄 `docx/tests/llvm/multicore-dispatch/probe_host.cpp`）：

```c
setSpikePGMode(PG_1DIE_8TILE);      // CModel 启动配置，不是编译选项
setTileNumPerChipForCModel(2);
setDeviceCountForCModel(1);
txSetDevice(0);
txModuleLoad(&module, image, size);         // 见下方"装载耗时"说明，务必 timeout
txModuleGetFunction(&fn, module, "<entry>");
txMalloc(&d, bytes); txMemcpy(d, host, bytes, txMemcpyHostToDevice);
txLaunchKernelSimd(fn, /*tileCount*/1, /*startTile*/0, &args, sizeof(args), nullptr);
txDeviceSynchronize();
txMemcpy(host, d, bytes, txMemcpyDeviceToHost);
txModuleUnload(module);
```

每次 CModel 调用都要 `timeout` + `ulimit -c 0 -f 262144`，并且**一个用例一个进程**：
设备侧某些路径会让 CModel 直接 `abort()`（SIGABRT），同进程跑会把后面的结果全丢掉。

---

## 3. 踩过的坑（都是实测）

### 装载耗时（对"40–60 s"的实测修正）

本次语料是 KB 级小模块（~10 KB `.so`、单 tile），**整条 host 进程实测 < 1 s**：
`build/tx82_run_host.elf` 从 `txSetDevice` 到 `txModuleUnload` 跑完 add/muladd/red 都在
1 秒内（`out/matrix.json` 的 `run_seconds` 全是 0）。只有崩掉的格子才会挂到 timeout。
所以"CModel 装载要 40–60 s"这个说法对本配方的小模块**不成立**——那是大模型/多 tile 场景的
数量级。仍然要 `timeout`，因为越界/死循环会真的挂住。

| 现象 | 根因 | 处理 |
| --- | --- | --- |
| `Load lib failed, ret=2`、`txModuleLoad` error=1 | 模块里有解析不到的未定义符号（把用不到的 gemini entry 一起编进来了） | 每个 entry 用 `#ifdef` 隔开，模块只留当前 case |
| `csrr vlenb` 之后 CModel **静默挂死**（无任何日志，直到 timeout） | 该板 CSR 0xC22 路径不安全 | **不要读 `vlenb`**；用 `vsetvli ...(-1)` 的 VLMAX 反推 VLEN |
| `[CT][ERR][EXEC] EXEC_ERR_LOAD_ADDR_OVERFLOW, opcode=... (VFL_W)`，随后 SIGABRT | 编译器按 **VLEN=256** 生成向量循环，而 CModel 的 VLEN 是 **4096**，`vl2re32.v` 一次搬 1024 B 而循环只前进 64 B | 设备侧**不要开自动向量化**：用 `-O0`/`-O1`/`-O2 -fno-vectorize` |
| 编译期 `fatal error: error in backend: Unsupported RVV instruction with funct6=...` | fork 的 `RVV2CTFunctMap` 里没有 `vfmacc.vv`/`vfmadd.vf`/`vfredosum.vs` 这类条目 | 该档位记为 BUILD_FAIL；或 `-ffp-contract=off` 把 FMA 拆成 `vfmul.vv`+`vfadd.vv` |
| 只想验证 kernel A，却因为同一 TU 里的 kernel B 带 FMA 而整模块编不过 | 一个 TU 一起编 | 一个 case 一个 TU / 用宏隔开 |
| `llvm-mc` 链出的目标文件 ABI 对不上（`cannot link ... different floating-point ABI`） | `-mcpu=tsm-x82` 是 `lp64f`（无 `+d`），llvm-mc 的 `-triple=riscv64` 默认 `lp64d` | 两边都加 `-mattr=+d`（只为把 e_flags 对齐，不产生 D 指令） |

---

## 4. 复现整张矩阵

```bash
cd wt-cmodelrun
source ../setup/activate.sh
python3 tools/run_matrix.py --out out/matrix.json        # 全部
python3 tools/run_matrix.py --cases add --levels O0,O2vec # 单个格子
```

每个格子：`build_case.sh` → `count_ops.py`（按 32 位字统计 `(word & 0x7f)==0x7b`）
→ `tx82_run_host.elf` → `compare.py`（与 CPU 参考、以及与另一个编译器产物逐位比对）。
结果同时落在 `logs/matrix.log`（构建/设备日志）、`logs/cmodel.log`（设备运行日志）、
`logs/compare.log` 和 `out/matrix.json`。
