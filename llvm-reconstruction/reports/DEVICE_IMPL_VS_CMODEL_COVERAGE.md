# 用厂商 CModel 对我方 cmodel 设备层做数值差分 —— **规模化** (step 2 / scale-up)

日期: 2026-10-16。承接 pilot 报告 [`DEVICE_IMPL_VS_CMODEL.md`](DEVICE_IMPL_VS_CMODEL.md)
(3 个 entry 的方法验证 + 1 个真实引擎缺陷)。本轮的职责是**把数值差分从 3 个 entry 扩到
"已实现的设备层"**, 并给出诚实的覆盖表: 多少 entry 真的数值验证过、多少被什么卡住。

工具与产物: **工作区同级** `wt-implscale/tools/implcheck/implscale/` (pilot 的
`wt-implcheck/` 只读复用; 通道性扩展见 §2)。一条命令复现: `bash tools/run_all.sh`
(8 个网格 / 115 用例); 加 `PREFILTER=1` 再跑 64-entry 可执行性预筛。

## 0. 结论 (TL;DR)

| 指标 | 数量 |
|---|---|
| 厂商注册的 entry 名 (`ExportedDYNSYMTab`) | 251 |
| **我方已实现** (= 可进入数值差分的上界) | **64** |
| 本轮跑了数值用例的 entry | 9 |
| 数值用例总数 | **125** |
| 其中**两侧逐字节一致** (含 6 例两侧都早退的守卫用例) | **105** |
| 其中两侧都中止 (边界一致, 无数据可比) | 5 |
| 其中厂商侧拒绝执行 (拿不到真值) | 15 |
| 其中厂商中止而我方**死循环** (本轮全部修成"同因中止") | 2 (已归零) |
| 64 个 entry 的最终分类 | BITEXACT 8 / PARTIAL 1 / 闭包缺口 34 / 闭包**未知** 8 / 未建布局 9 / 厂商侧拒绝 4 |
| 本轮发现并修复的缺陷 | **4 处 / 3 类** (`lsu_{rdma,wdma}_memset` 填充语义; `segment_size==0` 在 `dte_gather_scatter_{g2l,l2g}` + `dte_gather`/`dte_scatter` 上的静默 no-op 与"兜底 64 字节"; `index_select_by_column` 把设备物理地址当裸主机指针, 三件套修复) |
| 本轮发现、**未修**的实现缺陷 | 0 (全部已修或已登记为后续) |
| 回归 | 完整 `ctest` **183/183, 0 skipped** |

一句话: pilot 修掉的 DTE 分段迭代缺陷在**四条 DTE 通路上都拿到了正向证据** (本轮新增/重跑
**68 例真实数据面逐字节一致**: csc 20 + cdc 20 + sc 9 + gf 12 + isc 7), 同时这次规模化又抓到
**第二个真实引擎缺陷** (`lsu_*_memset` 的元素填充
语义, 见 §5 与 [`../../docs/recovered/device-lsu-memset-semantics.md`](../../docs/recovered/device-lsu-memset-semantics.md))。
剩下的账很清楚: **64 个已实现 entry 里只有 19 个闭包完整**; 34 个卡在"被调者我方未实现"
(端到端跑不起来), **8 个闭包是"未知"** —— verifiability TSV 对**间接分派 (`jalr`) 一律记
0 个被调者** (pilot §5 的工具债), 本轮的实证是 `82_prefix_scan_spatial` 的 5 个分派目标在
我方全是 `std::abort()` 中止桩而 TSV 列是空的; 10 个闭包完整但 P 布局还没建 (纯工作量),
4 个在探测布局下就被厂商模型拒绝。

## 1. 方法 (沿用 pilot, 只加通道性扩展)

两侧吃**同一份文本规格**, 输入逐字节相同, 唯一变量是被执行体:

* cmodel 侧 = `txMalloc` → `txMemcpy H2D` → `P[off]=设备地址` → `txModuleLoad` /
  `txGetFunction` / `txLaunchKernelSimd(1 tile)` / `txDeviceSynchronize` → `txMemcpy D2H`;
* 我方侧 = `dlopen libtx82_cmodel.so` → `mmap` 低地址 buf → 直接调同名 entry
  (`P[off]=主机地址`) → dump。

每例两侧各一个进程, 一律 `timeout` + `ulimit -c 0 -f 65536` (厂商会 abort / 死循环)。
只比**数据缓冲**, 逐字节, 无浮点容差。裁决粒度两个口径 (与 pilot 同规矩):
`verdict_excl_pbin` 是数据面判据 (只豁免 `p.bin` 那一行 —— 它的缓冲区地址字段两侧按构造
不同), `verdict_allfiles` 是历史口径。本目录的 82_*/gf_*/nm_* 规格**不 dump `p.bin`**;
pilot 的 `ri_*.spec` 带, 所以 `ri_*` 两个口径分别是 `BITEXACT 5` / `DIFFER 5 (仅 p.bin)`。

## 2. 相对 pilot 的通道性扩展 (都不是放松裁决)

1. **`put64 <off> <buf>+<delta>`**: 允许把"缓冲区地址 + 非零偏移"写进 P —— 未对齐目的
   基址用例 (probe 的 `unaligned_dst`) 需要它。
2. **`impl_run` 显式 mmap `0x101000000` 的 128 MiB scratch**: 厂商设备码把 `li 0x101 +
   slli 24` 当 SPM 暂存基址 (`fill_align` / `test_lsu` / `test_ne` / `mm_single_tile` …),
   而 `CModelMemoryManager::getHostPtr` 的第 5 条"主机指针直映射" fallback 会把
   `0x10000..2^40` 的任意地址原样当主机指针返回 ⇒ 没有映射就写空指针 SIGSEGV。
   厂商 cmodel 在那个地址上有真实后备存储, 这里只是把同一段地址补齐。
3. **`run_one.sh` 参数化** (`LIB=` / `ROOT`), 便于 before/after 换库对照。

## 3. 第 1 步: 可执行性预筛 (`out/executability.tsv`)

`tools/executability.py`: 64 个 entry × 4 个**通用探测布局** (`zero` / `shape82` / `wire` /
`dense`, 见脚本头注释), 每格两侧各一个进程, 记 rc + FATAL 行。之所以是"通用"布局: 逐
entry 建 P 布局正是 1~2 h/entry 的人工成本, 预筛要便宜。

| 每-entry 最佳格 | 数量 |
|---|---|
| `RUNNABLE` (厂商模型正常跑完) | **59** |
| `VENDOR_REFUSED` | 5 |
| `NOT_LOADABLE` / 整 entry 挂死 | 0 |

**"59/64 RUNNABLE" 不能当"59 个都能数值对拍"读**。把 `RUNNABLE` 按"目的缓冲有没有被改"
细分 (见 `out/coverage.tsv` 的 `exec_class`):

| 精化分类 | 数量 | 含义 |
|---|---|---|
| `VENDOR_RAN_WORK` | **15** | 探测布局下真的走了数据通路 (目的缓冲变了) |
| `VENDOR_RAN_EARLY_EXIT` | 44 | 模块加载了、entry 跑完了, 但探测布局不满足它的守卫 ⇒ 早退, **没验证任何数据面** |
| `PROBE_INCONCLUSIVE_ADDR` | 5 | 厂商 FATAL 的地址是 `0x0/0x8/0x2` ⇒ 是**我的探测没把缓冲区接到正确偏移**, 不是 entry 不可执行 |

也就是说: **通用探测只能给出"模块能不能加载 + 入口能不能跑", 不能替代按 entry 建 P 布局**。
真正的可执行性结论来自 §4 的真布局用例。预筛的正向价值是那 15 个 `VENDOR_RAN_WORK`
(它们是最快的下一批目标) 与**拒绝信息的分类学** (下面这张表, 全部是实测日志):

| 厂商拒绝原文 (`logs/exec_*.cmodel.log`) | 触发条件 | 受影响的族 |
|---|---|---|
| `[lsu.cpp:736] [WdmaMemSet] invalid dst_addr: 0x...` | md_set 的 dst 是全局 LPDDR | `n3d_fill_inplace`, `g3d_fill_inplace_i64`, `masked_fill` … |
| `[lsu.cpp:588] [TdmaMemCopy] invalid src_addr/dst_addr` | TDMA 的 src/dst 必须是 N3D/G3D 空间; harness 的 `txMalloc` 给 LPDDR | `n3d_memcpy` |
| `[lsu.cpp:737] [WdmaMemSet] Invalid byte_size: 8 (expected 1, 2, or 4)` | md_set 的 byte_size ∉ {1,2,4} | `g3d_fill_inplace` (dtype 11/12) |
| `[dte.cpp:570] [DteGather] segment_size must be greater than zero` | misc 的 segment_size=0 | `test_dte_stride`, `test_ne`, `test_lsu`, … |
| `[dte.cpp:850] [DteGatherScatterL2G] segment_size must be greater than zero` | 同上 (g2l/l2g 通路) | `82_copy_{src,dst}_contiguous` 的 R=0 用例 |
| `[dte.cpp:650] [DteScatter] ... greater than zero` | 同上 (scatter 通路) | 同上 |
| `[dte.cpp:566] [DteGather] invalid src_addr: 0x0` | 探测布局把 src 接成 0 | 探测假阳性 |
| `[dte.cpp:607] [DteGather] src(LPDDR) address exceed: ... limit=0x5fffffffff` | 算出来的地址越过 LPDDR 上界 | `test_dte_stride` dense 探测 |
| `[lsu.cpp:1081] [RdmaGatherScatter] invalid src_addr` | 同上 (LSU 侧) | `test_lsu` |
| `[RdmaGatherScatter] segment_size must > 0` | LSU 侧 zero-segment (另一处措辞) | `test_lsu`, `gemm` |
| `[dte.cpp:847] [DteGatherScatterL2G] invalid dst_addr: 0x2` | 探测假阳性 | `n3d_to_g3d` 等 |

**pilot §4 里"NE unsupported GEMM feature filtered"那条本轮没有被复现** —— `gemm_2048x2048x2048`
`mm_single_tile` 都是先在 `DteGather`/`RdmaGatherScatter` 上中止 (探测布局的地址是 0),
还没走到 NE。

## 4. 第 2/3 步: 数值差分逐 entry

### 4.1 汇总 (115 例, 被测库 md5 见 `logs/`)

| 网格 | entry | 厂商 .so | 例数 | 数据面 BITEXACT | 两侧都中止 | 厂商拒绝 | 我方超时 |
|---|---|---|---|---|---|---|---|
| `csc_*` | `main_kernel_82_copy_src_contiguous` | stride_copy.so | 21 | 20 | 1 | 1\* | 0 |
| `cdc_*` | `main_kernel_82_copy_dst_contiguous` | stride_copy.so | 21 | 20 | 0 | 1\* | 1\* |
| `sc_*` | `main_kernel_82_stride_copy` | stride_copy.so | 9 | 9 | 0 | 0 | 0 |
| `spy_*` | `main_kernel_multi_n3d_spy_n3d` | n3d_spy.so | 26 | 26 | 0 | 0 | 0 |
| `ri_*` | `main_kernel_82_repeat_interleave` | repeat_interleave.so | 5 | 5 | 0 | 0 | 0 |
| `gf_*` | `main_kernel_g3d_fill_inplace` | n3d_fill_inplace.so | 14 | 12 | 2 | 0 | 0 |
| `nm_*` | `main_kernel_n3d_memcpy` | n3d_memcpy.so | 13 | 3 (仅早退, 无 DMA) | 0 | 10 | 0 |
| `fill_*` | `main_kernel_n3d_fill_inplace` | n3d_fill_inplace.so | 6 | 0 | 1 | 5 | 0 |
| `isc_*` | `main_kernel_index_select_by_column` | index_select.so | 10 | **10** | 0 | 0 | 0 |

\* `csc_*`/`cdc_*` 的那 1 例都是 `R=0`: `csc_*` 走 g2l/l2g, 本轮修完后**两侧都中止**
  (`[DTE][DteGatherScatterL2G] segment_size must be greater than zero`, rc=134/134);
  `cdc_*` 走的是 `dte_scatter` (另一条, 不在本轮修复范围), 厂商中止而我方仍超时
  (impl_rc=124)。见 §6.1。

* `csc_*` / `cdc_*` 的 20 例覆盖: 1D/2D/3D 连续、目的转置、目的内层跨 3、R∈{1,4,5,2^31-1}、
  奇数维、`dtype ∈ {0,2,6,14,16}` (本仓库 `drt_get_dtype_size` 分段表 ⇒ 元素 1/2/4 字节)、
  rank 5/6、N 小于/等于/大于 shape、N=0、shape 含 0、
  未对齐目的基址 (`dst+7`)、R 高半非零。**这就是 DTE 修复在 1 级 + 多维 stride 两条通路上的
  端到端回归**。
* `sc_*` 是 DTE 线自带的 9 例 (三维 tensor-stride, 其中 2 例判别性), 本轮用**修完 memset
  的库**重跑, 9/9 保持 BITEXACT ⇒ 两个修复互不回归。
* `gf_*` 是本轮抓缺陷的网格 (见 §5)。
* `nm_*` 的 3 个 BITEXACT 是 `count<=0` / `blk==0` 的**早退**用例 (一次 DMA 都不发);
  其余 10 例厂商在 `TdmaMemCopy` 上拒绝 ⇒ 这条 entry 拿不到数据面真值。
* `isc_*` 是 `main_kernel_index_select_by_column` (DTE 影响面: `dte_rdma_stride_copy` -> g2l):
  10 例覆盖恒等/逆序/重复索引/行数 1/2/4/列步长 4/`ds_src≠ds_dst`, 以及三个早退边界
  (`D==0` / `P2==0` / `C==0`)。**修掉 §5c 的裸设备指针缺陷后 10/10 逐字节一致** (此前
  7/10 我方 SIGSEGV)。
* `fill_*` 是 pilot 的 6 例: 5 例厂商 `[WdmaMemSet] invalid dst_addr` 拒绝, 第 6 例
  (`fill_04`, dtype=11 ⇒ ds=8) 现在**两侧都中止** (我方也按厂商语义响亮中止)。

### 4.2 pilot 的"19 个高危 entry"现在各自在哪一格

缺陷爆炸半径 = 闭包内带 stride/gather DMA 的"已实现 entry"。按
`gemini-cmodel/docs/recovered/device-layer-verifiability.tsv` 的 `interposed` 列筛出
**21 个** (pilot 估 19, 差额是含 `lsu_*` 而非 DTE 的几条):

| # | entry | 缺陷通路 | 闭包 | 本次数值裁决 | 阻塞 |
|---|---|---|---|---|---|
| 1 | `main_kernel_82_copy_dst_contiguous` | dte_rdma_gather_copy -> dte_gather -> g2l | FULL | 21 例: BITEXACT 20 / **两侧都中止 1** (R=0) | **已覆盖** |
| 2 | `main_kernel_82_copy_src_contiguous` | dte_rdma/wdma_tensor_stride_copy -> g2l/l2g | FULL | 21 例: BITEXACT 20 / **两侧都中止 1** (R=0) | **已覆盖** |
| 3 | `main_kernel_82_lines_arith` | dte_rdma_stride_copy -> g2l | PARTIAL | — | 闭包缺口: `arith_compute_on_spma` 等 |
| 4 | `main_kernel_82_single_broadcast_arith` | dte_rdma_stride_copy -> g2l | PARTIAL | — | 闭包缺口: `arith_compute_on_spma` 等 |
| 5 | `main_kernel_82_stride_copy` | dte_rdma/wdma_tensor_stride_copy -> g2l/l2g | FULL | 9 例: BITEXACT 9 / 厂商拒绝 0 | **已覆盖** |
| 6 | `main_kernel_gemm_2048x2048x2048` | dte_gather_scatter_g2l/l2g | PARTIAL | — | 预筛探测布局下厂商 FATAL (要真 P 布局) |
| 7 | `main_kernel_index_select` | dte_index_gather + dte_rdma/wdma_stride_copy | FULL | — | P 布局未建 (闭包完整、厂商能跑) |
| 8 | `main_kernel_index_select_by_column` | dte_rdma_stride_copy -> g2l | FULL | **10 例: BITEXACT 10** (修完裸设备指针缺陷后) | **已覆盖** |
| 9 | `main_kernel_index_select_last_dim` | dte_index_gather + stride | FULL | — | P 布局未建 (闭包完整、厂商能跑) |
| 10 | `main_kernel_mm_single_tile` | dte_gather_scatter_g2l/l2g | FULL | — | 预筛探测布局下厂商 FATAL (要真 P 布局) |
| 11 | `main_kernel_multi_n3d_rope` | lsu_rdma_*_gather_scatter_copy | PARTIAL | — | 闭包缺口: `rope_core` 等 |
| 12 | `main_kernel_mv_tile_m_dont_split_k` | dte_gather_scatter_g2l | PARTIAL | — | 闭包缺口: `dte_wdma_src_stride_copy` 等 |
| 13 | `main_kernel_n3d_all_gather` | lsu_rdma_scatter + lsu_set_*_itera_stride | FULL | — | P 布局未建 (闭包完整、厂商能跑) |
| 14 | `main_kernel_n3d_index_select_last_dim` | lsu_rdma_index_gather | FULL | — | P 布局未建 (闭包完整、厂商能跑) |
| 15 | `main_kernel_n3d_repeat` | lsu_rdma/wdma_gather_scatter_copy | FULL | — | P 布局未建 (闭包完整、厂商能跑) |
| 16 | `main_kernel_n3d_to_g3d` | dte_gather_scatter_l2g | FULL | — | P 布局未建 (闭包完整、厂商能跑) |
| 17 | `main_kernel_n3d_to_mat` | lsu_rdma_gather_copy | PARTIAL | — | 闭包缺口: `continue_to_ne_layout_f32m4` 等 |
| 18 | `main_kernel_no_broadcast_arith` | dte_rdma_stride_copy -> g2l | PARTIAL | — | 闭包缺口: `arith_compute_on_spma` 等 |
| 19 | `main_kernel_test_dte_stride` | dte_rdma_src_stride_copy / dte_wdma_dst_stride_copy | PARTIAL | — | 闭包缺口: `dte_rdma_src_stride_copy` 等 |
| 20 | `main_kernel_test_lsu` | lsu_*_gather_scatter + dte_rdma/wdma_copy | FULL | — | 预筛探测布局下厂商 FATAL (要真 P 布局) |
| 21 | `main_kernel_test_ne` | dte_gather_scatter_g2l/l2g | PARTIAL | — | 预筛探测布局下厂商 FATAL (要真 P 布局) |

**读法**: "已覆盖" 的 5 条 (含本轮拿下的 `index_select_by_column`) = 本轮/DTE 线的数值证据; "P 布局未建" 的 5 条闭包完整、厂商也
能跑, 只差按实现源码建 P 布局 (1~2 h/条); "闭包缺口" 的 8 条端到端跑不起来 —— 它们的
被调者 (`*_compute_on_spma` / `rope_core` / `dte_*_src_stride_copy` …) 我方还没实现,
调用会 `jr 0`; 剩下 3 条 (`mm_single_tile` / `test_lsu` / `test_ne` / `gemm`) 闭包内的
GEMM 路径需要成对的 N3D/G3D 缓冲, 通用探测还没走到就中止了。

### 4.3 64 个 entry 的完整覆盖表

机器可读: `out/coverage.tsv` (每行 = entry / verdict / closure / 缺口符号 / 用例数 /
各裁决计数 / 预筛分类 / 预筛证据)。分布:

| verdict | 数量 | 含义 |
|---|---|---|
| `NUMERIC_BITEXACT` | 7 | 有数据面逐字节一致的用例 |
| `NUMERIC_PARTIAL` | 1 | 只有早退/无数据用例, 或缺真值 |
| `IMPL_BLOCKED` | 0 | 我方侧在真干活用例上崩/超时 ⇒ 端到端阻塞 (本轮最后一条已在 §5c 修掉; 口径保留) |
| `CLOSURE_GAP` | **34** | 闭包里有我方未实现的被调者 ⇒ 端到端跑不起来 |
| `CLOSURE_UNKNOWN_JALR` | **8** | TSV 的 `interposed` 列是空的 (**间接分派假阴性**); 实证是分派目标在我方是 `std::abort()` 桩 |
| `UNPROBED` | 10 | 闭包完整、厂商能跑, P 布局未建 (纯工作量) |
| `VENDOR_REFUSED` | 4 | 通用探测布局下就被厂商模型拒绝 |

### 4.4 9 条 `UNPROBED` 各自差什么 (本轮**没有**做完的账)

这 9 条的闭包完整、通用预筛下厂商也能跑, **只差按实现源码把 P 布局变成 spec 网格**。
布局的逐字段偏移都已写在实现文件的文件头里 (下表给出确切文件与行); 每条要做的是:
把偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐)。

| entry | 实现文件 (P 布局文档都在文件头) | 文件头里的布局证据 | 缺什么 |
|---|---|---|---|
| `main_kernel_82_paged_attention` | [tx82_device_82_paged_attention.cpp](src/device/tx82_device_82_paged_attention.cpp#L40) | `0x038..0x068  6 个 qword 的描述符 (A, A+t, A+2t, A+3t, A+4t, prod+A), a2 = sp+0x38` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_g3d_fill_inplace_oneline_not_align` | [tx82_device_g3d_fill_inplace_oneline_not_align.cpp](src/device/tx82_device_g3d_fill_inplace_oneline_not_align.cpp#L67) | `1551c or   s6, a1, a0            ; s6 = u64(args+0x28, args+0x2c)` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_index_select` | [tx82_device_index_select.cpp](src/device/tx82_device_index_select.cpp#L30) | `+0x00 F00   源行基址的加数 (s5 的初值加项)` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_index_select_last_dim` | [tx82_device_index_select_last_dim.cpp](src/device/tx82_device_index_select_last_dim.cpp#L18) | `+0x00 F00   主循环源地址的加数 (src = F00 + A*F60)` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_multinomial` | [tx82_device_multinomial.cpp](src/device/tx82_device_multinomial.cpp#L62) | `a6=[sp+0x60]=F18(符号扩展的 lw P+0x38), a7=diag1 恒 1 / diag2 = F20)` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_n3d_all_gather` | [tx82_device_n3d_all_gather.cpp](src/device/tx82_device_n3d_all_gather.cpp#L20) | `sp += 0x10; ret; }` —— 本文件**完整直译** (见下), 不是 weak extern。` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_n3d_index_select_last_dim` | [tx82_device_n3d_index_select.cpp](src/device/tx82_device_n3d_index_select.cpp#L22) | `dsz0 = drt(lw P+0x08);  dsz1 = drt(lw P+0x0c)              [i32, 符号扩展]` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_n3d_repeat` | [tx82_device_n3d_repeat.cpp](src/device/tx82_device_n3d_repeat.cpp#L17) | `flag0 = lw   P+0x00        (i32)                   [0x150b0]` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |
| `main_kernel_n3d_to_g3d` | [tx82_device_n3d_to_g3d_entry.cpp](src/device/tx82_device_n3d_to_g3d_entry.cpp#L60) | `mode = lw(args+0x14); tile = get_global_tile_id()                  // 0x15190/8c` | 把这些偏移变成 spec 网格 + 三类边界 (字段取 0 / 两字段相等 / 符号位与低位对齐) |

其中 `index_select*` (3 条) 与 `n3d_to_g3d` 直接压在 DTE 缺陷通路上 (`dte_index_gather` /
`dte_rdma_stride_copy` / `dte_gather_scatter_l2g`), 是下一轮优先级最高的; `n3d_repeat` /
`n3d_all_gather` / `n3d_index_select_last_dim` 压在 LSU 的 gather/scatter 通路上 (与本轮
修的两处 DTE/LSU 语义同族)。

### 4.5 `CLOSURE_UNKNOWN_JALR` 的 8 条 —— **动之前必须先核"分派目标是不是桩"**

`82_arith_vs` / `82_arith_vs_dev` / `82_lines_arith_vs` / `82_lines_arith_vs_dev` /
`82_prefix_scan_spatial` / `masked_fill` / `max_min_no_broadcast` / `max_min_with_broadcast`
—— verifiability TSV 的 `interposed` 是空的 (间接分派), 按"无缺口"读会得出假结论。
本轮的实证: `prefix_scan.so` 的 4 个表槽 + 1 个硬编码默认体在我方都是
`std::abort()` 桩 (`tx82_device_prefix_scan_spatial.cpp:345+`), 所以这条 entry 目前
**不可数值验证**; 其余 7 条要先按 `docs/recovered/device-layer-partially-verified.tsv`
与各自实现文件核一遍分派目标是不是桩, 再决定能不能测。

`CLOSURE_GAP` 的 34 条里 **13 条**的缺口是 `*_compute_on_spma` / `*_reduce_on_spma_*` /
`spma_scatter_i32_to_i64` 这一族计算核 (`82_lines_arith` / `82_lines_bitwise` /
`82_single_broadcast_arith` / `no_broadcast_arith` / `no_broadcast_bitwise` / `gelu` /
`silu` / `softplus` / `unary` / `unary_bitwise` / `hyperbolic` / `inverse_hyperbolic` /
`minmax_last_dim_keep_dim`); 其余 21 条的缺口更分散, 每条 1~2 个符号:
`__get_pid` (concat / split 2 条)、`*_single_card` (layernorm / rmsnorm 2 条)、
`82_arange_{fp,int}` / `82_repeat_large|middle` / `82_softmax_last_dim_{large,small}` /
`82_repeat_interleave*` 的分派臂 / `fill_align_i64` / `l2norm_{params_init,shape_init,dispatch_ndim2,ndim4}` /
`log_softmax_backward_data_*` / `compute_{bf16,fp16,fp32}` / `rope_core` /
`dte_{rdma,w}dma_*src_stride_copy` / `continue_to_ne_layout_*` / `bias_grad_nhwc` /
`handle_{comb,pre,post}` / `single_dim_anyall_*` / `reduce_chunk_on_spma_*` 等。

**这 34 条不是"数值差分做不了", 是"入口翻译还没做完"**: 这 71 个缺口符号实现之后, 用本轮
的同一套 harness 直接就能跑 (71 个缺口符号里大半是 `*_compute_on_spma` 一族)。

## 5. 本轮发现并修复的引擎缺陷: `lsu_{rdma,wdma}_memset`

完整证据 + 语义来源: [`../../docs/recovered/device-lsu-memset-semantics.md`](../../docs/recovered/device-lsu-memset-semantics.md)。

* **症状**: `main_kernel_g3d_fill_inplace` 的 11/12 个可判定用例 `dst` 逐字节不一致, 形状
  都一样 —— **首差恒在 `dst+0x1`**, 且只有前 N 个字节不同。
* **根因**: `src/engines/cmodel_lsu.cpp:lsu_rdma_memset` (wdma 转发) 把
  `(value, dst, byte_size, elem_num)` 理解成"填 `byte_size` 个 `value&0xff` 字节"。
* **厂商语义 (取自 `cmodel/lib/libraisa_sim_core.so` 的设备模型二进制)**:
  `RdmaMemSet @0x502920` / `WdmaMemSet @0x502ce0` 都走
  `Kuiper::LSU::(anonymous namespace)::FillElements @0x4ffb80` —— 填 **`elem_num` 个元素、
  每元素 `byte_size` 字节**, 图案 = value 的低 `byte_size` 字节, **总字节数 =
  `byte_size × elem_num`**; 且 `byte_size ∉ {1,2,4}` 直接 FATAL
  (`Invalid byte_size: %u (expected 1, 2, or 4)`)。
* **修复**: 按上面语义重写 `lsu_rdma_memset`; 越界宽度**响亮中止** (复现厂商 FATAL,
  不静默换长度); `dma_fill` 同步改。`test_symbol_parity` 里那条"填 byte_size 个字节"的
  自期望断言是错的口径, 一并改成 1/2/4 字节元素三个断言。
* **验收**: `gf_*` 从 `11 DIFFER / 1 BITEXACT` 翻成 **`12 BITEXACT / 2 两侧都中止`**;
  `csc_*` / `cdc_*` / `sc_*` / `spy_*` / `ri_*` 全部不回归; 完整 `ctest` 183/183 (0 skipped)。

## 5b. 第二个引擎缺陷 (窄口径修复): `dte_gather_scatter_segmented` 的 `segment_size == 0`

* **症状**: `csc_R0` / `cdc_R0` (`R=0`) 厂商中止, 我方 `impl_rc=124` (**死循环**, 60 s 超时)。
* **根因**: 厂商 `[dte.cpp:850] [DteGatherScatterL2G] segment_size must be greater than zero`
  / `[dte.cpp:650] [DteScatter] ...` **中止**; 我方
  `src/engines/cmodel_dte.cpp:dte_gather_scatter_segmented` 里是 `if (segment_size == 0) return;`
  (**静默 no-op**), 于是忠实翻译的入口内层 `s2 += cnt` 不前进 ⇒ 死循环。
* **修复**: 只把这一处的前置条件换成 `stderr` + `abort()` (与 `lsu_*_memset` 同一风格),
  方向名进消息 (`DteGatherScatterG2L` / `DteGatherScatterL2G`, 与厂商字符串同名)。
  **严格不动** `dte_gather`/`dte_scatter` 的 `misc ?: set_length` fallback (那条路没有数值真值)。
* **验收**: `csc_R0` 从 `IMPL_RC124` 变成 `IMPL_RC134`, 消息与厂商同因; `csc_*` 其余 20 例、
  `cdc_*` 20 例、`sc_*` / `spy_*` / `ri_*` 全部保持 BITEXACT; 完整 `ctest` 183/183。
* **第二批 (同一轮取证后扩到 `dte_gather`/`dte_scatter`)**: 这两处旧代码更糟 ——
  `seg = misc ? misc : set_length; if (seg == 0) seg = 64;` 是**编造 64 字节长度**
  (AGENTS.md「绝不返回编造值」)。取证三条 (按整合者要求):
  1. **同一输入厂商确实中止**: `cdc_R0` → `[SPIKE][FATAL][dte.cpp:650] [tile_id:0]
     [DteScatter] segment_size must be greater than zero` (rc=134);
     `csc_R0` → `[dte.cpp:850] [DteGatherScatterL2G] ...`。字符串站
     `strings libraisa_sim_core.so`: 213438 `[DteGather] ...`, 213447 `[DteScatter] ...`。
  2. **`misc` 未配置时是否也中止**: 现有语料里**不可分别构造** —— 我方(以及厂商)所有
     gather/scatter 调用点都在之前写 misc: `tx82_device_dte_copy.cpp:98/110`、
     `tx82_device_layout.cpp:40/68`、`tx82_device_dte_rdma_gather_copy.cpp:82`。
     但"未配置"与"配成 0"在寄存器层面**等价** (寄存器默认 0, 两者都走同一中止分支);
     而且 `dte_set_length` 的值按厂商定义是**跨度**(只做 range check), 拿它当段长本身
     就是语义错误 —— 所以那个 fallback 按构造就是错的, 去掉它不会改变任何
     "misc 已被写成非 0"的路径。
  3. **回归**: `gf_*` 12 BITEXACT + 2 两侧都中止 / `sc_*` 9 / `spy_*` 26 / `ri_*` 5
     全部保持; `csc_*` / `cdc_*` 20+20 保持 BITEXACT, 两个 `*_R0` 从 `IMPL_RC124`
     变成 `IMPL_RC134` 且消息与厂商**同因同站**; 完整 `ctest` 183/183。
  范围仍**不含** `set_length` 的跨度语义 (那条没动)。

## 5c. 第三个缺陷 (实现层, **已修**): `index_select_by_column` 把设备物理地址当裸主机指针

* **症状**: `main_kernel_index_select_by_column` 的 7 个"真干活"用例在**我方**全部
  `impl_rc=139` (SIGSEGV), 厂商侧 rc=0 —— 这条压在 DTE 缺陷通路上的 entry 进不了数值差分。
* **根因**: 入口 11a34 的直译把上游 `dte_rdma_stride_copy` 刚搬到 **0x40000000 (tile0 L0A 的
  物理地址)** 的 index 张量 `reinterpret_cast` 成**进程地址**解引用; 只有"进程恰好映射了
  该地址"才不崩。
* **修法 (三处一起改, 任一单独改都会打破另一侧)** (提交 `e17a727`):
  1. `src/device/tx82_device_index_select_by_column.cpp`: 读 index 改走
     `CModelMemoryManager::getInstance().getHostPtr(...)` —— 读到的就是引擎
     (`dte_rdma_stride_copy` -> `dte_gather_scatter_g2l`) 写进 tile0 L0A 后备存储的**同一块**
     内存; 未映射时 stderr + abort(), 不静默编造 `idx = 0`;
  2. `tools/raops_oracle/probes/index_select_by_column_trace.c`: 索引表所在 PA 的预填改用
     弱钩子 `tx82_trace_spm_map` —— 厂商侧为 NULL (仍是 `mmap(0x40000000)` + 裸地址写,
     **一字未动**), cmodel 侧由 test 给强定义走内存管理器; 于是两侧各走**自己原生**的翻译链,
     **厂商真值不需要重生成**;
  3. `test/CMakeLists.txt`: 给该 target 编入 `src/core/cmodel_memory.cpp` (先例: 5g2
     `test_device_maxmin_with_broadcast` 用同一个手法解决同一类"0x40000000 是 tile0 L0A"
     的问题)。
* **踩到的坑 (留给下一个人)**: 钩子里不能按厂商侧习惯要 32 MiB —— `getHostPtr` 只在
  "offset + size <= 窗口" 时走**窗口翻译**, 超窗口会退化成第 5 条"主机指针直映射"把
  `0x40000000` 原样当进程指针返回 (实测就是 SIGSEGV 的同一症状)。现在按 L0A 窗口 (1 MiB) 截断。
* **验收**:
  * `test_device_index_select_by_column` **OK: 与厂商真值逐行一致 (28488 字节)**, 真值未重生成;
  * `isc_*` **7/10 SIGSEGV → 10/10 逐字节一致**;
  * 该 entry 的裁决 `IMPL_BLOCKED` → `NUMERIC_BITEXACT`;
  * 完整 `ctest` **0 failure** (干净 detached 树: `183/183`; 首次全量构建后 examples 下的
    `libcuda_relu.so` 存在, 会多注册 2 个 CUDA 例子测试 → `185/185`, 与本改动无关)。
* **半成品不落库的记录**: 中间那版只改入口的提交 `164a4ae` 因为打破了独立测试 target 的
  构建与期望模型, 已被 `e950ceb` revert, 并在 `3650a1c` 把限制与正确修法写进实现文件头。
  本提交就是那份"三件套"。

## 6. 本轮发现但**没有**修的 divergence (诚实清单)

1. ~~**DTE `segment_size == 0`**~~ —— **已按窄口径修复, 见 §5b**。
   `csc_R0` (g2l/l2g) 与 `cdc_R0` (`dte_scatter`) 现在**两侧都同因中止**
   (`csc`/`cdc` 的 20+20 例 BITEXACT 全部保持)。
2. **厂商的地址空间合法性判据没有复现**: `RdmaMemSet`/`WdmaMemSet`/`TdmaMemCopy`/
   `DteGather` 都会先查地址类型/上界 (`invalid dst_addr` / `address exceed`)。本模型没有
   "n3dram/LPDDR/G3D 地址类型"概念, 于是 `main_kernel_n3d_fill_inplace` 这类**厂商拒绝、
   我方照跑**的 entry 拿不到数值真值 (pilot §3.3 已登记)。这是"厂商侧边界", 不是我方缺陷,
   但它同时意味着**这些 entry 永远无法数值对拍**, 除非把地址空间模型补齐。
3. **`main_kernel_n3d_memcpy` 的 TDMA 通路无法对拍**: `txMalloc` 只给 LPDDR 地址, 而厂商
   `TdmaMemCopy` 要求 N3D/G3D 空间 ⇒ 10/13 例 rc=134。要覆盖它需要 harness 侧能拿到
   N3D/G3D 缓冲 (`tyMalloc` 之类), 本轮的 spec 通道做不到。

## 7. 复现

```bash
cd /home/shaokai/Desktop/work/tx82/wt-implscale/tools/implcheck/implscale
bash tools/run_all.sh                 # 8 网格 / 115 用例 + out/coverage.tsv (~10 min)
PREFILTER=1 bash tools/run_all.sh     # 再加 64-entry 可执行性预筛 (~35 min)
LIB=<另一个 libtx82_cmodel.so> bash tools/run_all.sh   # before/after 对照
```

每个网格的单条复现 (两侧各一个进程, 逐字节比对; `$VK` = `gemini-cmodel/tools/raops_wheel_kernels`):

```bash
source tools/env.sh; export LIB=$HARNESS_LIB        # 被测库 = 构建产物
# 一个网格 (整族):
bash tools/run_grid.sh "$VK/stride_copy.so" main_kernel_82_copy_src_contiguous specs csc_ out/csc.tsv
bash tools/run_grid.sh "$VK/stride_copy.so" main_kernel_82_copy_dst_contiguous specs cdc_ out/cdc.tsv
bash tools/run_grid.sh "$VK/stride_copy.so" main_kernel_82_stride_copy         specs sc_  out/sc.tsv
bash tools/run_grid.sh "$VK/n3d_spy.so"     main_kernel_multi_n3d_spy_n3d       specs spy_ out/spy.tsv
bash tools/run_grid.sh "$VK/repeat_interleave.so" main_kernel_82_repeat_interleave specs ri_ out/ri.tsv
bash tools/run_grid.sh "$VK/n3d_fill_inplace.so"  main_kernel_g3d_fill_inplace  specs gf_  out/gf.tsv
bash tools/run_grid.sh "$VK/n3d_memcpy.so"        main_kernel_n3d_memcpy        specs nm_  out/nm.tsv
bash tools/run_grid.sh "$VK/n3d_fill_inplace.so"  main_kernel_n3d_fill_inplace  specs fill_ out/fill.tsv
# 单个用例 (最小复现): 两侧各一个进程 + compare.py
bash tools/run_one.sh "$VK/n3d_fill_inplace.so" main_kernel_g3d_fill_inplace gf_pat_ds2_wide specs/gf_pat_ds2_wide.spec
bash tools/run_one.sh "$VK/stride_copy.so" main_kernel_82_copy_src_contiguous csc_R0 specs/csc_R0.spec
```

产物: `out/<case>/{cmodel,impl}/*.bin` (逐字节可比)、`out/{csc,cdc,sc,nm,gf,spy,ri,fill}.tsv`、
`out/executability{,_full}.tsv`、`out/coverage.tsv`、`logs/<case>.{cmodel,impl}.log`。

**每条 `VENDOR_REFUSED` 的中止消息与站点**在 §3 的拒绝分类学表里 (原文取自
`logs/exec_*.cmodel.log` / `logs/<case>.cmodel.log`); 每条 `NUMERIC_BITEXACT` 的复现命令
用上面 `run_grid.sh` 的对应行 + `out/<网格>.tsv` 逐例结果; 每条 `UNPROBED` 差的布局在 §4.4。

## 8. 剩余覆盖的会话估算

* **最快的一批 (0.5 个会话)**: 预筛里 `VENDOR_RAN_WORK` 的 15 个 entry —— 通用布局已经
  走到了数据通路, 按实现源码把布局坐实 (每条 0.5~1 h) 就能拿到数值证据;
  加上 pilot 提到的 `82_*` 家族里闭包完整的几条。
* **闭包完整的 9 个 `UNPROBED` (2~3 个会话)**: 1~2 h/entry 建 P 布局 + 三类边界用例
  (字段取 0 / 两字段相等 / 符号位与低位对齐), 其中 `index_select*`、`n3d_*`、
  `g3d_fill_inplace_oneline_not_align` 是 DTE/LSU 通路的重点。
* **34 个 `CLOSURE_GAP` (3~5 个会话, 但不是"验证"而是"实现")**: 先补这 71 个缺口符号
  (大头是 `*_compute_on_spma` 一族 + `__get_pid` / `*_single_card` / `*_reduce_on_spma_*`),
  然后每条 0.5~1 h 跑数值差分。**在缺口补齐之前, 这 34 条连"端到端跑起来"都做不到**,
  所以它们的"数值覆盖"必须先有入口翻译的工作量。
* **明确拿不到真值的 (0 会话, 永久阻塞)**: `n3d_fill_inplace` / `g3d_fill_inplace_i64` /
  `n3d_memcpy` 的搬运用例 (厂商地址空间判据), 以及所有依赖 N3D/G3D 目的地址的 memset。
  建议在台账里记成"厂商模型侧边界, 无数值真值", 不要反复投入。

合计: **把 64 个已实现 entry 的数值覆盖推到"能推的极限"约 7~10 个会话** (其中 3~5 个
会话其实是入口翻译)。若只算闭包完整的 19 条 + 闭包未知的 8 条 (后者要先核分派目标
是不是桩), 是 **4~5 个会话**。
| main_kernel_test_dte_stride | test_ne.so | NUMERIC_BITEXACT | FULL | 8 | 8 | 0 | 0 | 0 | 0 | RUNNABLE | VENDOR_RAN_WORK |

---

## 附录：batch D（2026-10-18，`wt-runtime`，分支 `dev/runtime-helpers`）

台账全文（符号 / 真值种类 / 边际 / 变异 / 未做项的原因）在
**`wt-runtime/docs/recovered/device-batch-D-coverage.md`**；补丁 `wt-runtime/patches/02-batch-D-krt-pair-spma.patch`
（基线 `9eacc78` = patches/01 落地后）。

- 新增实现的设备符号 **5 个**，全部是 `.so` 里真的 `T`：
  `get_next_tile_id` / `spma_n3d_once_sendrecv_diff_size` / `exchange_spma_n3d_once_sendrecv`
  （真值 = 厂商 riscv64 机器码在 qemu-riscv64 真跑）、`krt_barrier_simd` / `krt_invalid_dcache`
  （真值 = **厂商 cmodel 行为差分**，这两个在 101 个内核里都是 `U`/0 字节本体，
  工作区无 RISC-V 实现；结论表 `wt-runtime/docs/recovered/device-krt-barrier-simd-invalid-dcache.md`）。
- 严口径可开工 entry：**42 → 46（+4）**。测的 `.so` 是 `build-runtime/lib/libtx82_cmodel.so`，
  `--impl-extra` = 别的 worktree 已落地的 16 个（gemm/layout 9 + dte_rdma 4 + x3d_copy 3）。
- **没有新增 `RUNNABLE` entry**（本线只补被调者，不动 entry），所以本文件的分档表不变。
- 回归：`BUILD_DIR=<build-runtime> ctest -j2 --timeout 300` ⇒ **188/188, 0 failed, Skipped = 0**。
- 两条交回口径（不是"来不及"）：
  1. `n3d_fill` 族的工具闭包少算 63%（`device_callees.py` 不解析 `jalr` 的 `auipc+addi` 常量目标
     ⇒ 漏掉 `n3d_fill_head_tail_3d` 2,752 B，多算不可达的 `n3d_fill_body_3d` 114 B）；
     同形状在 x3d_copy 族已发生过一次。已登记 `wt-runtime/docs/recovered/tool-debt.tsv`。
  2. `process_a_with_part_b` 在 `--impl-extra 16` 下解锁 **2** 条（不是 1 条），
     闭包 1,892 B = 本体 1,222 + 两个**未实现**的 DTE 助手 346/324；
     该函数是 17 实参（含 `t3`–`t6` 扩展寄存器）的局部 `t`，登记为待专门会话。
