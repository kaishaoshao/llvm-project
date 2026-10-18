# 用厂商 CModel 对我的 cmodel 设备层做**数值**差分 (pilot)

日期: 2026-10-16。范围: **3 个 entry** 的端到端数值对拍 (厂商设备机器码 vs 我方 C++ 实现): 方法、
逐位裁决、边界、规模化估算。**不追求全覆盖**, 追求"结论可复现、差异定位到函数"。
工具与产物在 `wt-implcheck/` (**未改 `gemini-cmodel/` 一行**, `git status` 干净)。
一条命令复现: `cd wt-implcheck && bash tools/run_all.sh` (37 用例 / ~3 分钟)。

## 0. 结论 (TL;DR)

| entry | 用例 | 逐位裁决 | 说明 |
| --- | --- | --- | --- |
| `main_kernel_multi_n3d_spy_n3d` (n3d_spy.so) | 26 | **26/26 逐字节一致** | 纯 load/store + 8 字节展开拷贝。我方入口翻译与厂商机器码**数值等价** |
| `main_kernel_82_repeat_interleave` → `_non_last` (repeat_interleave.so) | 5 | **5/5 不一致** | 首个不同字节 `dst+0x21` (vendor=0x01, ours=0x00)。**根因在引擎不在入口**: 我方 `dte_gather_scatter_{g2l,l2g}` 把带迭代/跨度的 DMA 退化成一次平拷贝 |
| `main_kernel_n3d_fill_inplace` (n3d_fill_inplace.so) | 6 | **无法比较** | **厂商 CModel 自己 abort**: `[FATAL][lsu.cpp:736] [WdmaMemSet] invalid dst_addr: 0x26000` (rc=134)。不是我们的 bug |

一句话: 这批 entry 的**入口翻译**看着是对的 (spy 26/26 逐位一致 + 仓库既有调用序列真值), 但**数值
端到端**在 DTE 引擎上分叉, 且差异形状与厂商记录的 DMA 实参**逐段对得上** (§3.2) ⇒ 下一步瓶颈是
**引擎保真度**, 不是 entry 数量。


## 1. 方法 (harness)

两侧吃**同一份文本规格**, 输入逐字节相同, 唯一变量是被执行体:
cmodel 侧 = `txMalloc` buf → `txMemcpy H2D` 填输入 → P[off]=**设备地址** → `txModuleLoad` /
`txModuleGetFunction` / `txLaunchKernelSimd(1 tile, arglen=pbytes)` / `txDeviceSynchronize` →
`txMemcpy D2H` → dump; 我方侧 = `dlopen` 我方 `libtx82_cmodel.so` → `mmap` 低地址窗口的 buf →
直接调同名 entry (P[off]=主机地址) → dump。

* 驱动 `host/{cmodel_run,impl_run}.cpp` + 共用 `host/spec.h`; 用例 `tools/gen_spec.py`;
  单例 `tools/run_one.sh`; 全量 `tools/run_all.sh`; 比对 `tools/compare.py`。
* **隔离 / 限时**: 厂商 CModel 会 SIGABRT ⇒ 每用例两侧各起一个进程, 一律 `timeout` +
  `ulimit -c 0 -f 65536` (厂商会死循环、写 GB 级 trace)。本语料模块加载实测 <1 s
  (与 `CMODEL_EXECUTION_RECIPE.md` §3 的修正一致), 从未超时。
* **我方侧两个必须做对的地方** (踩过, 症状都是"毫无输出的 SIGSEGV"): ① lib 里的 `krt_*` 是
  **weak 未定义** ⇒ 驱动要 `-rdynamic` 且给强定义, 否则 `get_global_tile_id` → `jr 0`;
  ② buffer 地址必须落在 `CModelMemoryManager` 的主机指针直映射窗口 (`0x10000 <= pa < 2^40`)
  ⇒ 用 `mmap` 固定低地址, 不能用 `malloc` (堆指针 ≈0x55… 被拒)。
* **口径**: **逐字节**比较, 无浮点容差 (被对拍的都是整型地址/搬运逻辑)。差异一律报
  "首个不同字节偏移 + 两侧值 + 区间形状"; 不写"基本一致"。
* **entry 选取** (任务要求闭包完整): ① 从 101 个厂商 `.so` 的 `ExportedDYNSYMTab` 扫出
  **251 个 `txModuleGetFunction` 找得到的 entry 名** (`wt-implcheck/registered_entries.tsv`);
  ② 与我方 `nm -D --defined-only` 求交 ⇒ **64 个**可测候选; ③ 按 `device-layer-verifiability.tsv`
  的 `interposed` 列判"闭包内被调者是否全在库内" ⇒ 23 个; **再逐个人工核过**: `masked_fill` /
  `max_min_*` 的 TSV 行写 "CLOSURE OK (0 callees)", 实为**间接分派 `jalr`**, 目标在
  `tx82_device_masked_fill_kernel.cpp:275+` / `tx82_device_max_min_kernel.cpp:216+` 是
  `std::abort()` **响亮中止桩** ⇒ 剔除。(对"未知被调者"默认按 clean 是
  `marginal_closure.py` 头注释自己警告过的坑。)


## 2. 用例与为什么选它们

| entry | 大小 | 闭包 | 为什么 |
| --- | --- | --- | --- |
| `main_kernel_multi_n3d_spy_n3d` | 302 B | 唯一被调者 `get_global_tile_id` (已实现) | 最简端到端样本: tile 守卫 + 8 字节展开拷贝, **不碰 DMA 引擎** ⇒ 能把"入口翻译"与"引擎保真"分开看 |
| `main_kernel_82_repeat_interleave` | 80 B 分派 + `_non_last` 3644 B | `_non_last` 路径 4 个被调者全已实现 | 仓库"零缺口、落库即可端到端"的样板, 且**含外层迭代的 DMA** ⇒ 正压引擎 |
| `main_kernel_n3d_fill_inplace` | 78 B | `drt_get_dtype_size`/`get_global_tile_id`/`lsu_wdma_memset` 已实现 | 代表 **LSU memset** 另一族引擎, 探"厂商 CModel 到不到得了" |

**注册名的坑**: `_non_last`/`_last`/`_none` **不在** `ExportedDYNSYMTab` 里, `txModuleGetFunction`
找不到 (error=1)。能加载的是分派入口 `main_kernel_82_repeat_interleave` —— 它按
`P+0x450==P+0x454` 且 `P+0x458 != P+0x450-1` 尾跳到 `_non_last`。所以 §2 的用例都跑在**真入口**上,
走的正是运行时的那条路。


## 3. 数值结果

### 3.1 `spy_n3d`: 26/26 逐字节一致

`len ∈ {0,1,7,8,9,16,47,48,49,64,65,128,192} × expected_tile_id ∈ {0,1}`,
256 B 源(确定性模式)/目的(预填 0xAA)。**26/26** `dst`、`src` 两侧逐字节相同, rc 两侧皆 0。
其中 `len=0` 与 `tile_id=1` 是守卫的等号/不匹配侧 (静默返回), `len=8k±1` 压 8 字节展开 + 尾部那次
"重复写"的边界 (`tx82_device_n3d_spy.cpp` 文件头记的厂商行为) —— 连"下标为 8 的倍数被写两次"这种
只在写次数上可见的行为, 最终内存内容也一致。⇒ 这条通道能给**正向**证据。

### 3.2 `repeat_interleave` → `_non_last`: 5/5 不一致, 根因在 DTE 引擎

5 个用例 (rank/s0/dtype/repeats 不同) 全部: `P[0]`(输入张量) 两侧逐字节相同,
`P[8]`(**输出张量**) 不一致。

**最小复现 `specs/ri_05.spec`** (输出缓冲初始全 0, 输入 P[0] = `i & 0xff`):

| | 输出缓冲的非零区间 |
| --- | --- |
| 厂商 CModel | `[0x01..0x09] [0x21..0x29] [0x41..0x49] [0x61..0x69] [0x81..0x89] [0xa0..0xa9] [0xc0..0xc9]` |
| 我方实现 | `[0x01..0x09] [0xa0..0xa9]` |

首个不同字节 **`0x21`: vendor=0x01, ours=0x00** (46 字节不同)。厂商把同一段源数据复制到
`outer` 个位置 (每段 10 B、目的跨度 0x20), 我方只写了 1 段。

**根因 (定位到函数)**: `src/engines/cmodel_dte.cpp`

```c
void dte_gather_scatter_g2l(uint64_t src_addr, uint64_t dst_addr) {   // 与 _l2g 同形
    uint64_t len = g_dte.dst_len ? g_dte.dst_len : g_dte.rdma_data_len;
    CModelMemoryManager::getInstance().memcpy(dst_addr, src_addr, len);   // ← 一次平拷贝
}
```

它**完全忽略** `dte_set_*_itera_stride` 写下的三级 (`itera`,`stride`) 配置, 把
"(outer-1)*stride + elem 字节连续搬一遍"当语义; 厂商是**分段迭代**。

**证据 (不是推断)**: 用仓库既有厂商真值
(`docs/recovered/device-82-repeat-interleave-non-last-oracle.inc`, `M43_rep2` 与 ri_00 同参数)
按"`outer` 段 × `elem` 字节; 第 i 段源 `a0+i*src_stride`、目的 `a1+i*dst_stride`"重算:

```
trace: rdma a0=P[0] a1=SPMA     a2=0xa a3=0x20 a4=0x20 a5=0x2
       wdma a0=SPMA a1=P[8]     a2=0xa a3=0x0  a4=0x20 a5=0x5
       wdma a0=SPMA+0x20 a1=P[8]+0xa0 a2=0xa a3=0 a4=0x20 a5=0x2
预测厂商输出 = {0x00,0x20,0x40,0x60,0x80,0xa0,0xc0} 各 10 B 被写; 实测 = 7 个非零区间, 逐段相同
同一规则预测我方输出 = [0x0..0x9] + [0xa0..0xa9] —— 与实测完全一致
```

两侧差异因此可精确解释为"我方引擎少做 `outer` 次迭代"。

两个附带发现 (避免下一个人重踩):
* **方向标注反了**: 两个 helper 的数据流恒为 `a0 → a1` (rdma: P[0]→SPMA; wdma: SPMA→P[8])。
  探针文件头与 `tx82_device_dte_stride_copy.cpp` 注释里的 "`a0 = dst_addr, a1 = src_addr`"
  与实际相反。调用序列真值只记实参不记方向, 所以一直没暴露 —— 这是**标注 bug, 不是数值 bug**。
* **爆炸半径**: `tx82_device_dte_tensor_stride_copy.cpp:91/109` 的两个 tensor-stride helper
  **也**收在同一个 `dte_gather_scatter_{g2l,l2g}` 上。在"已注册 + 已实现"的 64 个 entry 里,
  闭包内带 stride/gather DMA 的有 **19 个** (`82_stride_copy` / `82_copy_{src,dst}_contiguous` /
  `index_select*` / `mm_single_tile` / `test_{ne,lsu,dte_stride}` …)。

### 3.3 `n3d_fill_inplace`: 厂商 CModel 自己 abort (边界, 非我方 bug)

6 个用例 (dtype∈{i8,bf16,i16,i64}, elem∈{0,8,16,32,64}, value 16/64 位) 全部
`opcode:0xc82a0b(LSU_WDMA_MEM_SET)` + `[SPIKE][FATAL][lsu.cpp:736] [tile_id:0] [WdmaMemSet]
invalid dst_addr: 0x26000` → **rc=134**。厂商模型的 `LSU_WDMA_MEM_SET` 只接受**本地 (L0W/SPM)
目的地址**, 而本 entry 的 P 块给的是全局 LPDDR 地址; 我方同输入正常返回 (rc=0)。
这条路径**拿不到数值真值**, 属于 `COMPILER_VS_CMODEL.md` §2.5 那类"厂商模型侧边界"。

附带一个**可疑但本轮无法判定**的点: 我方 `n3d_fill_inplace` 尾调用把 `elem`(元素字节数)
当成 `lsu_wdma_memset` 的 **byte_size**, 于是 `dma_fill` 只填 `elem` 字节 (dtype=i8 时 1 字节);
按参数名 `(value, dst, byte_size, elem_num)` 直觉应是 `elem_num` 个元素。厂商侧拒绝执行 ⇒
**无真值**, 不要凭直觉改。

### 3.4 通道能力的实测 (非数值差异)

参数选错 (`rank == s0-1`, 如 `rank=1,s0=s3=2`) 会走 `_last` 臂: **厂商 rc=0 正常跑完,
我方 rc=139**(`_last` 未实现 → PLT 槽解析到 0 → `jr 0`, 正是仓库设计的响亮中止)。
说明 harness 能检出闭包洞, 也提醒 `_non_last` 的用例必须满足 `s0==s3 && rank != s0-1`。


## 4. 阻塞 / 边界 (诚实清单)

1. **厂商 CModel 拒绝执行**的族: `LSU_WDMA_MEM_SET` 带全局目的地址 (§3.3); 加上前序会话记录的
   TDMA/GEMM/NE 内建崩溃 (`COMPILER_VS_CMODEL.md` §2.5)。⇒ 不是所有 entry 都能走这条通道;
   投 P 布局工时前先做"可执行性预筛" (~10 分钟/族)。
2. **只有注册过的 entry 能被加载**: 502 个 `__dynsym_*` 表项 / 251 个唯一名; `_non_last`/`_last`/
   `_none` 只能经分派入口跑到 ⇒ 用例必须按分派条件构造, 且 `repeat_interleave` 我方只实现
   `_non_last` ⇒ 该 entry **不算端到端可用**, 只有该域可测。
3. **主要人工成本是 P 块布局**: 每个 entry 要从我方实现源码/反汇编读出 P 布局 (运行只要 2–4 s)。
4. **未覆盖**: 浮点/向量数值路径、多 tile/die 通信 (send/recv)、`fill_align` 族、以及全部"厂商
   拒绝执行"的族。`p.bin` 两侧地址字段必然不同, **不计入裁决**。


## 5. 规模化计划 (含工作量)

* **第 0 步 (已完成)**: 通道 = `wt-implcheck/`。上界 **251 个注册 entry**; 我方已实现 64 个
  ⇒ 现在最多测 64 个; 另 187 个注册 entry 还没实现, 无从差分 (那是入口翻译的活)。
* **第 1 步 (1 个会话, 必做)**: 修 `src/engines/cmodel_dte.cpp` 的 `dte_gather_scatter_{g2l,l2g}`
  —— 按三级 itera/stride + `segment_size` 做真正的**分段迭代**, 约 **40–60 行 / 2 个函数**;
  同时修好 4 个 DTE helper (它们都收在这一对函数上)。回归: `ri_*` 5 个用例 DIFFER→BITEXACT,
  `spy_*` 26 个保持 BITEXACT。
* **第 2 步 (2–3 个会话, 可并行)**: 对 19 个"带 stride/gather DMA"的已实现 entry 逐个建 P 布局
  + 三类边界用例 (字段取 0 / 两字段相等 / 符号位与低位对齐), 跑数值差分。~1–2 h/entry,
  64 个全量一轮 ≈ 3–4 个会话。
* **第 3 步 (0.5 会话)**: "厂商侧能不能执行"预筛 —— 每族用零/小参数跑一次只看 rc 与 `FATAL` 行,
  把 TDMA/GEMM/NE/LSU-memset 等先登记成"不可数值对拍"。本轮抽样 3 个 entry 就有 1 个被拒,
  预计真正能数值对拍的比例**明显低于** 64。
* **第 4 步 (工具债, 0.5 会话)**: 把**间接分派目标**补进闭包检查 —— `device-layer-verifiability.tsv`
  对 `jalr` 一律记 0 个被调者, 会把 `masked_fill` / `max_min_*` / `paged_attention` 误判成
  "CLOSURE OK"。

**建议**: 边际收益目前不在"多测几个 entry", 而在**先补齐 DTE 引擎的分段语义** (一处 40–60 行
→ 4 个 helper / 19 个已实现 entry), 再用现成 harness 做一轮 64-entry 数值回归 ——
这是 1–2 个会话能拿到最大真值增量的一步。


## 6. 复现

```bash
cd /home/shaokai/Desktop/work/tx82/wt-implcheck && source ../setup/activate.sh
cmake -S ../gemini-cmodel -B build-ours && cmake --build build-ours --target tx82_cmodel -j2
g++ -std=c++17 -O2 -rdynamic host/impl_run.cpp -ldl -o host/impl_run.elf   # + 见 §1 的 cmodel 驱动
bash tools/run_all.sh                      # 37 用例 → logs/summary.tsv
bash tools/run_one.sh ../gemini-cmodel/tools/raops_wheel_kernels/repeat_interleave.so \
     main_kernel_82_repeat_interleave ri_05      # 单用例 (最小复现)
```

产物: `out/<case>/{cmodel,impl}/*.bin` (逐字节可比), `logs/<case>.{cmodel,impl}.log`, `logs/summary.tsv`, `specs/*.spec`。

## 7. 本 pilot **没有**证明的事

* 没有证明"DMA 之外的所有语义都对": 只测了 3 个 entry 的有限输入。
* 没有证明厂商 CModel 的 DTE 语义在**所有** (itera, stride, segment_size) 组合下就是 §3.2 的分段
  模型 —— 只证明它能**逐字节解释** 2 个用例的实测输出 (M43/ri_00, ri_05)。
* 没有判定 §3.3 的 `byte_size`/`elem_num` 疑点 (厂商侧拒绝执行, 无真值); `p.bin` 两侧地址字段
  必然不同, 裁决只看数据缓冲。

## 8. 后续: §5 第 1 步已落地 (2026-10-16, `dev/dte-segmented`)

`src/engines/cmodel_dte.cpp` 的 `dte_gather_scatter_{g2l,l2g}` 已按厂商 cmodel 的**设备模型
二进制** (不是从输出反推) 改成分段迭代: `cmodel/lib/libraisa_sim_core.so` 里的
`Kuiper::Spe::DTE::dteGatherScatterG2L(Inst) @0x573b70` / `...L2G @0x575390` (`dte.cpp:708..819/821..`)
—— 三层嵌套环、**上界取 source 侧 itera**、**每段长度取 `*_misc` 的 `segment_size`**、
**方向恒 `a0 → a1`**。完整证据 + 未验证边界:
[`gemini-cmodel/docs/recovered/device-dte-gather-scatter-segmented.md`](../../gemini-cmodel/docs/recovered/device-dte-gather-scatter-segmented.md)。

### 8.1 `run_all.sh` 的裁决粒度: 两个口径都要看 (26/5/6 不等于"没修好")

本 harness 的 `run()` 过去对 `compare.py` 的**全部**输出 grep `^DIFF`, 而 `specs/ri_*.spec`
固定带 `dump P p.bin` —— `p.bin` 的两个地址字段 (P[0x00]/P[0x08]) 是两侧**各自的缓冲区地址**
(厂商 `txMalloc` 给 `0x400002c000…`, 我方 `impl_run.cpp` 固定 mmap `0x3000000000…`),
**按构造不可能相等**, 且修复前后是同样的 7 字节差 (§4 早就写明它"不计入裁决")。
后果: 5 个 `ri_*` 用例永远 DIFFER, 引擎修没修好从 headline 上看不出来。

所以 `tools/run_all.sh` 现在**同时打印两个口径** (只豁免 `p.bin` 这一个文件, 数据缓冲
`out/in/src/dst` 一个不落):

| 状态 | `verdict_excl_pbin` (数据面) | `verdict_allfiles` (历史口径) |
|---|---|---|
| 修复前 (`libtx82_cmodel.so` md5 `93e94af7…`) | 26 BITEXACT / **5 DIFFER** / 6 VENDOR_RC134 | 26 / 5 / 6 |
| 修复后 (md5 `1ff8cddd…`) | **31 BITEXACT / 0 DIFFER** / 6 VENDOR_RC134 | 26 / **5** / 6 (= 只剩 p.bin 地址字段) |

逐文件明细 (`out/<case>/{cmodel,impl}/*.bin`): 修复前 `ri_00/01/02` 的 `in.bin` 差
50/50/80 字节、`ri_03` 差 12、`ri_05` 的 `dst.bin` 差 46 (首差 `dst+0x21`: vendor=0x01 ours=0x00);
修复后 5 个用例的 `out/in/src/dst` **全部 EQUAL**, 26 个 `spy_*` 保持 BITEXACT。

### 8.2 三维 tensor-stride 通路 (`dte_*_tensor_stride_copy`) 也拿了证据

同一对 `g2l/l2g` 还托着三维 helper, 只有 1 级 stride 的 `ri_*` 覆盖不到它。
新增 `tools/implcheck/dte_segmented/` (`gen_sc_specs.py` + `run.sh`), 以
`main_kernel_82_stride_copy` 为被对拍 entry 做端到端数值差分 (9 条用例):

* 契约内 (`m == ndim+1`) 时三维 helper **恒退化成"单层 + 段内连续"** (入口读数组的守卫
  `index > ndim` 使 stride 全取默认 1), 旧平拷贝本来就同结果 ⇒ 7 条用例两侧都 BITEXACT,
  证明修复对该通路**不回归**;
* 契约外 (`m > ndim+1`) 才有"多层 + 段间不相邻" (`itera=2/2/1`, `stride0=6≠seg=2`):
  旧实现差 152/136 字节, 新实现逐字节一致 ⇒ **判别性**证据 (修复真的改了这条通路的行为)。

