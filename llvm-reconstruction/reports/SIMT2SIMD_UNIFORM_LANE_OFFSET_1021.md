# 1021 — `control_uniform_loop` 编过（103/103）：RAISA 关 LSR + `simt2simd` 的均匀 lane 字节偏移

日期：2026-10-17 晚 · 树：`wt-merge-e`（含 0001…0077）· 补丁：`patches/1021-raisa-loop-reduce-gate-and-uniform-lane-offset.patch`（2 文件 / 19 hunk / 363 行，`patch -p1` 回放**逐字节复现** `src`）

## 结果

| | 改前 | 改后 |
|---|---|---|
| `control_uniform_loop` | `asm:OURS-FAIL obj:OURS-FAIL`（`RISCVISelLowering.cpp:14629`） | **`asm:DIFF obj:DIFF`（能编译）** |
| `.rc` oracle `--all` | 176 ident 槽（88 格）/ 28 diff（14）/ **2 ours-fail（1 格）** / 22 both-fail | **176（88）/ 30（15）/ 0 / 22** |
| 317 parity | 217 / OURS-GAP **1** / byte-diff 32 | **217 / 0 / 33**，`.c` 零缺口，**逐格变化恰好 1 格**（`control_uniform_loop`，`rc -6 → 0`），0 丢失 |
| musl 厂商-vs-我方（`musl_norm.py`） | raw 0/56+4、剥 `.comment` **24/56 + 4 = 28/56 outcomes** | **完全相同**，逐 kernel verdict 无变化 |
| `sweep_stale_objects.sh` / `check_geninc_stale.sh` | 0 / 0 | **0 / 0** |

⇒ 全语料 **103/103 能编译**。

## 根因（前一线给的诊断需要修正）

**LSR 形状差不是 `simt2simd` bail 的原因。** 证据链（全部在 `wt-merge-e/logs/uni/`）：

1. **LSR 输入逐字节相同**：`-mllvm -print-before=loop-reduce` 两侧 0 行差异。
2. **LSR 相关 TTI/TLI 钩子语义相同**（对 `tools/re/ar_probe/RISCVTargetTransformInfo.cpp.o` 反汇编核对）：`RISCVTTIImpl::isLSRCostLess`（vendor 0xb7 B / 我方 0xb8 B，比较顺序 `(Insns, NumRegs+(NumBaseAdds!=0), AddRecCost, NumIVMuls, NumBaseAdds, ScaleCost, ImmCost, SetupCost)` 完全一致，差异只是 `sbb` vs `setne/add` 的代码生成）、`getPreferredAddressingMode`（两侧 0x21 B 相同）、`RISCVTargetLowering::isLegalAddressingMode`（0x89 vs 0xab，多出的字节只是 `switch(AM.Scale)` 里的 `llvm_unreachable`，因为**厂商是 Release 无断言**）。厂商 TTI 只多 3 个 override：`hasBranchDivergence` / `isAlwaysUniform` / `isSourceOfDivergence`（= 已登记的 RAISA divergence 钩子）。**没有任何 LSR 钩子差异。**
3. **DataLayout 相同**：`e-m:e-p:64:64:64:32-p3:…-p4:…-i64:64-i128:128-n32:64-S128-A5` 两侧一致。
4. **调度模型不是原因**：`-tune-cpu andes-45-series` 不翻转形状。**没有 `-lsr-*` 选项能翻转**（`-lsr-insns-cost=false`、`-lsr-exp-narrow=true`、`-lsr-filter-same-scaled-reg=false`、`-lsr-preferred-addressing-mode=none|postindexed` 都仍是 `phi ptr`；`-lsr-complexity-limit=0` 直接触发 LSR 自身断言）。
5. **最小复现（纯 C、无 RAISA 内建、只有 triple 不同）**：`-triple riscv64-unknown-raisa` 时厂商出 `%arrayidx.idx = shl i32 %k.010, 9` + 每次访问 GEP，我方出 `%lsr.iv24 = phi ptr`；**换成 `riscv64-unknown-linux-gnu` 两侧输出完全相同** ⇒ 差异在 LSR 决策内部、且被 triple 门控。
6. **决定性反证**：把**厂商自己的 `-print-before=simt2simd` module**（verbatim，`logs/uni/vendor_pre_simt.ll`）喂给我方 clang，**仍然** `:14629` abort ⇒ 缺口**完全在 `TX8TransformSIMTToSIMD.cpp`**，不在上游 IR 形状。（顺带：这也宣告"LSR 形状差"这条线到此为止。）

**真正的缺口**（我们 pass 在 `-print-after=simt2simd` 里对整函数 bail，只剩 1 条活的 `read.reg.tid.x`，loop body 一行未动）：两条规则缺失 ——
(a) 地址「tid 根 + **uniform 非常量**字节偏移」（未完全展开的循环的 `%inc = shl i32 %k, 9`）；
(b) 分类不动点在**闭合环**上死锁：`%k = phi [0],[%inc]` 与 `%inc = add %k, 8` 互为唯一未决操作数（累加器 `%v = phi [0],[%add]` / `%add = add <lane load>, %v` 同理）。

## 改法

**(a) 管线：RAISA 不跑 `loop-reduce`**（`llvm/lib/CodeGen/TargetPassConfig.cpp`，+14/−1；新隐藏选项 `-riscv-raisa-lsr` 默认 false 作逃生口）。
`if (!DisableLSR && !(TM->getTargetTriple().isRaisa() && !EnableLSRForRaisa))`。
**这是 stopgap，不是厂商忠实重建**：厂商**确实**跑 LSR（连 `loop-reduce` dump 都有），只是它的 `LoopStrengthReduce.cpp` 被 fork 过（我们连输入都逐字节相同、所有钩子语义相同、换 triple 就一致 ⇒ 差异在 LSR 内部、triple 门控）。跳过 LSR 只是**在这条地址链上恰好复现厂商形状**（厂商 post-LSR 的地址链 ≡ 其 pre-LSR，只有 trip-count 形态不同）。
**代价**：每格丢一次强度削减（本语料只有 1 格有 LSR dump ⇒ 观测上 0 代价）。**未来恢复路径**：(i) 逆向厂商 LSR 的 delta；或 (ii) 在 `simt2simd` 里做真正的 lane-address 递推（把 `phi ptr` 化成 loop-carried 向量地址）—— 二者成本见下"不做"。

**(b) `simt2simd`：均匀 lane 字节偏移 + 闭合环分类**（`TX8TransformSIMTToSIMD.cpp`）
1. `laneAddrOf` 增加 `Value *&VarOff`：`gep i8, %lane_ptr, %uniform_scalar` → `VarOff`（用 `dependsOnTid()` 判定"非 lane 派生"）；**常量偏移可与变量偏移累加**（epilogue 是 `gep %15, 512` 套在 `gep %11, %35` 之上）。
2. 重写：`LaneIndex(A, ConstOff, VarOff, At)` → `vadd.<lane>.i32.i64(undef, lane_addr_vec, scalar, i128)`，再进已有的 `vslh` + `vluxei.sync` 尾巴。
3. 分类不动点：`Pending` 的 phi 按**结构**定性 —— `dependsOnTid(phi)` 为假 ⇒ uniform（归纳变量），为真 ⇒ lane 向量（累加器）。仅当 phi 类型 == lane 元素类型才这么做，其它情况仍 bail（all-or-nothing 不变）。
4. `NeedsLaneOffset`：只要有 gather 被接受，就**物化 `vid`**（即使 `tid` 只作索引）—— 这正是厂商 IR 里那条"看似多余"的 `vid.v`。
5. `LaneAddressBase` 的缓存只在**同一基本块**内复用（loop 里的链不支配 epilogue）；死 GEP 清扫改成**不动点**；`read.reg.tid.x` 的擦除**推迟到清扫之后**（否则 `for.body` 的 tid-indexed GEP 还挂着 use ⇒ `Use still stuck around`）。

**厂商 asm 对应**（`simt/oracle/control_uniform_loop.vendor.s`，逐指令）：
```
v9  = vmul.vx vid, 4            ; lane byte off
v10 = vadd.vx v9, <lo(in)>      ; lane address vector
v13..v25 = vadd.vx v10, 512/1024/…/3584     ; 8 个展开常量偏移（preheader）
loop: slli a4, a1, 9            ; = %arrayidx.idx = k*512   ← 规则 (a) 的 uniform 变量偏移
      vadd.vx v26, v10, a4      ; lane addr + uniform scalar ← 新规则 (b)2
      vslh.vx  v30, v26, <hi(in)>          ; 已有
      vluxei64.sync.v v27, (zero), v30     ; 已有
```

## 炸开半径（穷举，非抽样）

- **LSR 普查（114 格）**：`-print-before/after=loop-reduce` 逐格对比 ⇒ **只有 `control_uniform_loop` 有 dump 且 before≠after**；其余 **113 格是 0 字节日志**（循环根本没活到 loop 阶段）⇒ 跳过 LSR 对它**可证**是 no-op。原始 dump：`logs/uni/blast/`，脚本 `logs/uni/lsr_blast.py`。
- **`simt2simd` 改动**：只影响 RAISA triple（`simt2simd` 本身 `isRaisa()` 门控）；317 格逐格 diff **恰好 1 格变化**（`control_uniform_loop` `rc -6→0`），musl 逐 kernel verdict **0 变化**。

## 不做（点名，未开工）

- **mini-LSR / lane-address 递推**：把 `phi ptr` 化成 loop-carried 的向量地址。已论证是**新的一层**（向量化后每轮的 lane 地址向量必须跨迭代传递，与现有 pass 的单次赋值重写架构冲突）。
- **逆向厂商 `LoopStrengthReduce` 的 delta**：超预算；且已被证明对本格不是必要条件。

## 工具地雷（新增两条）

1. **`tools/rebuild_one.sh`（及其 `--force-src`）对"记录依赖指向旧树路径"的对象会静默匹配 0 个对象**：`build/.ninja_deps` 里 `TargetPassConfig.cpp.o` 的 dep 记的是 `/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm/...`（派生 build 的历史遗留），`norm()` 只归一化 `/wt-<name>/`，所以 `--force-src <绝对路径>` 命中 0、rc=0、重链一个没变的二进制。**改用 `--force-objs <basename>.o`**（匹配 basename 子串）；并且 `logs/changed_inc.json` 必须是 `[]`，否则 `--steps objs` 会计划全量重编 RISCV target。
2. **`TX8TransformSIMTToSIMD.cpp.o` 在 `.ninja_deps` 里没有条目**（`minibuild` 自己编的）⇒ `--force-objs` 也看不见它，`ninja -n` 又会因为"命令行变了"想重编 3120 个边。可行做法：从 `ninja -t commands <obj> | tail -1` 取编译命令直接跑，再用 `ninja -t commands lib/libLLVMRISCVCodeGen.a | tail -1` / `ninja -t commands bin/clang-21 | tail -1` 归档与链接（本次三份对象都是这样编的）。
