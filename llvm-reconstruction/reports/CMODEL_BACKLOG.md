# cmodel 实现待办（2026-10-16 暂停存档，等编译器跑完再开）

由 `docs/recovered/device-layer-verifiability.tsv`（129 可对拍 / 已实现 58 / 剩余 71）+
我方库符号表算出的**边际收益排序**。剩余 71 条里：**零缺口 5 条**、有缺口 66 条、**97 个缺口符号**。
所有缺口符号在厂商 kernel `.so` 里都是**普通设备函数**（`T`），我方库里是 **weak 未定义**（跑到就响亮中止）
⇒ 一律走"逐指令直译（`tools/vendor_oracle/disasm_relocated.sh`）+ 差分 + 探针/真值/变异自检"的成熟流程，
并可叠加**厂商 cmodel 数值差分**做端到端验收（`wt-implcheck/`、`wt-implscale/tools/implcheck/implscale/`）。

## 按解锁量排序（待派）
| 序 | 符号（族） | 解锁 | 厂商定义 | 备注 |
|---|---|---|---|---|
| 1 | `gemm_with_input_psum_v2` + `gemm_with_psum_v2` | **43 + 24** | `addcdiv.so:0x15492 / 0x156ec` | 最大单块；101 个 kernel 同址同体 |
| 2 | `get_g3d_ne_layout` | **27** | `addcdiv.so:0x11290` | 设备函数 |
| 3 | `krt_get_die_id` / `krt_get_tile_id` / `printf` / `get_die_tile_id` / `krt_get_chip_id` / `krt_barrier_in_chip` / `get_tile_num_per_chip` / `get_prev_tile_id` | 22/22/16/9/9/9/8/7 | 运行时/KRT 助手 | 便宜；`printf` 的输出会进真值，必须逐字复现 |
| 4 | `dte_rdma_src_stride_copy` / `..._with_ratio` / `dte_wdma_scatter_copy` | **13 + 9 + 7** | `addcdiv.so:0x126a6` 等 | **必须复用**已修好的三层 `(itera,stride)` 分段原语；不得重新引入平拷贝 / 静默 no-op / 编造 64 字节三个刚修掉的 bug |
| 5 | **零缺口 5 条**（落库即端到端可用） | 5 | — | `mm_tile_m_dont_split_k` 1172 B、`82_repeat_middle` 5212 B、`82_repeat_large` 5348 B、`n3d_from_g3d_v2` 6102 B、`n3d_index*`/`n3d_linear2_split_k` 17484 B |
| 6 | 数值覆盖规模化（**进行中，已暂停**） | — | — | `index_select` → `index_select_last_dim` → `n3d_to_g3d`，再 LSU gather/scatter 3 条；账见 `DEVICE_IMPL_VS_CMODEL_COVERAGE.md` |

## 已完成的 cmodel 工作（勿重做）
- 引擎缺陷 4 修：`lsu_*_memset` 元素语义、`dte_gather_scatter` 三层分段、`segment_size==0` 响亮中止
  （含去掉编造 64 字节的兜底）、`index_select_by_column` 裸设备指针。
- 数值覆盖：9 entry / 125 例 → **105 逐字节一致**（99 真搬运）；账 `NUMERIC_BITEXACT 8 / PARTIAL 1 /
  IMPL_BLOCKED 0 / CLOSURE_GAP 34 / CLOSURE_UNKNOWN_JALR 8 / UNPROBED 9 / VENDOR_REFUSED 4`。
- 两条工具债登记在 `docs/recovered/tool-debt.tsv`（`jalr` 假阴性、覆盖口径假绿）。

## 恢复方式
每条线一个 worktree（`wt-gemmpsum` / `wt-helpers` / `wt-dtecopies` / `wt-implscale`），
按 `gemini-cmodel/AGENTS.md` 纪律开工；构建一律显式 `-j2`、开工前查 `free -h`（可用 < 4 G 就等）、
厂商/cmodel 调用一律 `timeout`。

## 暂停点交接（2026-10-16 18:50，cmodel 线被中断，WIP 已固化）
**正在做**：设备运行时助手（`krt_get_tile_id`/`krt_get_die_id`/`krt_get_chip_id`/`krt_barrier_in_chip`/设备侧 `printf`/`get_tile_num_per_chip`/`get_prev_tile_id`）。
**WIP 补丁（未验证，勿直接合入）**：`docs/compiler/WIP-cmodel-runtime-helpers-2026-10-16.patch`
（原在 `wt-runtime/`，已 `git restore` 复位；涉及 `krt_interface/krt_{barrier,cluster,device}.h`、`src/core/cmodel_zephyr.{cpp,h}`、
`src/core/device_dtype_convert.cpp`、`src/core/CMakeLists.txt`、`test/CMakeLists.txt`）——**恢复时要重新验证**（差分 + 全量 ctest + `nm -D` 计数）。

**恢复时的最小清单**：
1. **取证路径分两类**：`krt_get_tile_id/die_id/chip_id/barrier_in_chip/printf` 在厂商 kernel 里是 `U`、也不在 x86/RISC-V 的 cmodel 库里 ⇒ 由**设备运行时（krt/TUFW 固件）**提供、工作区无 ⇒ **只能行为差分**；现成 oracle = `docx/tests/llvm/multicore-dispatch`（`probe_device.c` 在厂商 cmodel 上 24 次 placement 全 PASS，逐 tile/die/chip 取值就在真值里）。`printf` 的**格式必须逐字对齐**（进真值）。
2. **内核里定义的 `T`**（`get_tile_num_per_chip@addcdiv.so:0x18cd2`、`get_prev_tile_id@0x18f0e`）走**指令级机械翻译**：
   - 新工具 **`tools/re/read_symbols.py <kernel.so> <符号...>`** = 厂商 `llvm-objdump`（**认识 TX8 私有指令，0 unknown**）+ **PLT→符号名解析**（`.plt` +32 字节表头、每 16 字节 stub 对第 i 个 `JUMP_SLOT`）+ Ghidra 结构。
   - 已读出的结论：`get_tile_num_per_chip = krt_get_die_num_per_chip() * krt_get_tile_num_per_die()`（`mulw`）；
     `get_prev_tile_id` = `krt_get_tile_num_per_die()` 取模环绕 + **编译期硬编码拓扑表查表**（`auipc+addi` 定位，需把该表从 `.rodata` dump 出来）。
   - **新发现两个台账漏掉的运行时助手**：`krt_get_die_num_per_chip`、`krt_get_tile_num_per_die`。
3. **Ghidra 分工**：REA 只对 **x86-64** 可用（cmodel 核心 `libraisa_sim_core.so` = 设备模型语义真值，已挂 REA 快照 `tx82_core`）；RISC-V 内核 REA 不支持，用**裸 Ghidra** 拿结构（TX8 私有指令会截断，只当结构用）。

## 恢复包（2026-10-16 19:00，由 wt-runtime 线重建，parent 已独立 `git apply --check` 通过）
**权威补丁**：`docs/compiler/WIP-cmodel-runtime-helpers-2026-10-16.COMPLETE-14files.patch`
（14 文件 = 8 改 + 6 新，1375 行，md5 `187603dce4d3aee45070bf61bf9e930b`；原在 `wt-runtime/patches/01-runtime-helpers-COMPLETE.patch`）
交接：`...HANDOVER.md`（60 行：符号/证据种类、取证命令、未做与卡点、一条复现命令）。
**注意**：`docs/compiler/WIP-cmodel-runtime-helpers-2026-10-16.patch` 是**半成品（只有 4 个新文件），别用**。

**已完成且已验证**：`get_tile_num_per_chip` / `get_prev_tile_id` / `get_die_tile_id` / `printf`
（真值 = 厂商 riscv64 机器码在 qemu 下真跑，探针自带可编程 `krt_*`；`test_device_runtime_tile_topology` **761 checks / 0 failures**；变异自检 2 条转红后复绿；`--target tx82_cmodel` rc=0；`nm -D` 12 个符号各恰好 1 个裸 C 定义）。
**已实现但未注册测试**（6 个）：`krt_get_chip_id/_die_id/_tile_id`、`krt_get_die_num_per_chip/_tile_num_per_die`、`krt_barrier_in_chip`。
新增的厂商真值（**重要**）：① tile 是 **die 内下标**（PG_2DIE_8TILE 下全局 start=8 → die=1,tile=0）；
② `die_num`/`tile_num` **随 PG 配置变化**（4 模式 = 1/8、2/8、4/4、4/6；旧实现硬编码 4/8 已修）；
③ `krt_barrier_in_chip` = 整 chip tile 到齐才放行（只起 8/16 → 死等；满 16 → 全返回），**且不提供全局内存可见性顺序**。

**恢复时的最小清单**：① **先跑全量 ctest**（`-j2`、`Skipped` 必须 0）—— 停机时唯一未验的回归面；② 给上述 6 个符号注册差分测试（`gen_runtime_krt_ref.sh` + `test_device_runtime_krt`，探针编译行见 HANDOVER ②）；③ 同族缺口：`get_next_tile_id@0x18cee`、`get_prev/next_global_tile_id`、`krt_barrier_in_die/_simd`。

### 口径更正（2026-10-16 19:05，恢复包作者提出，parent 接受）
parent 先前给的解锁量（`krt_get_die_id` 22 / `krt_get_tile_id` 22 / `printf` 16 / `krt_get_chip_id` 9 /
`get_die_tile_id` 9 / `krt_barrier_in_chip` 9 / `get_tile_num_per_chip` 8 / `get_prev_tile_id` 7）是
**"剩余未实现条目"口径**（129 verifiable / 58 implemented / **71 remaining**）。
恢复包作者按 `device-layer-verifiability.tsv` **全部 278 行**的 `interposed` 列重扫，得到
**42 / 42 / 26 / 12 / 10 / 9 / 8 / 7，并集 77**（含已实现条目）。两个口径都对，但**含义不同**：
- "并集 77" = 这些符号出现在多少条 entry 的闭包里（含已实现的）；
- "剩余 71" = 还没实现的条目数（**派工口径**）。
**两者都不是"边际解锁"**：按 §2，真正的派工数字是**严口径 gap=0 的增量**（`marginal_closure.py --callees`），
**尚未计算** —— 恢复时必须补算，否则又会掉进"被 N 个 entry 依赖 ≠ 解锁 N 个"的坑。
