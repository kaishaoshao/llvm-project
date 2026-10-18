# TX82 编译器 fork 重建方案（FORK_REBUILD_PLAN）

> 采集：2026-10-16 · 仓库 `/home/shaokai/Desktop/work/tx82`
> 本轮原始证据：`wt-forkplan/`（`run_ea.sh` / `ea_git.log` / `ea_tarball.log` / `hashcheck.py` /
> `isa_oracle2.py` / `isa_dis_oracle.py` / `ec/*.inc`）；完整原始输出见
> [FORK_REBUILD_PLAN.evidence.md](FORK_REBUILD_PLAN.evidence.md)。
> 标注：`[实测]`＝本轮亲手复现；`[文档]`＝前轮报告自报（可复现部分已核对）；`[推测]`。

## 0. 结论摘要

1. `[实测]` **重建基线是 upstream `llvmorg-21.1.0-rc1`（commit `cbcb48a88c`），不是 22.1.8。**
   22.1.8 只是 `riscv-llvm-musl-toolchain/build-llvm-musl/` 里一个 **stock** 构建的版本号（那个 clang-22
   没有任何 fork 内容）。厂商 clang-21 同样是 21.1.0-rc1 ⇒ 两侧版本天然对齐，**不存在版本移植问题**。
2. `[实测]` 25 条 patch 在 rc1 上 **25/25 干净应用**；在 `setup/llvm-project-21.1.0.src`（21.1.0 正式版）
   上 24/25（仅 `0013` 冲突，2 个文件）。⇒ 对 rc1 的移植改动量 = **0**。
3. `[实测]` `third_party/llvm` 的工作树 == “rc1 + 25 条 patch”，**146/146 条被 patch 路径 byte-identical**，
   无任何“只在工作树里”的私货。**重建物在源码层已经等于补丁序列。**
4. `[实测]` `tsm-x82` / `xtsm82s` **确实已定义**，并且用真实 TableGen 生成的
   `RISCVTargetParserDef.inc` 与厂商安装树里 shipped 的同名文件 **md5 完全相同**
   （`3e39e57196f69ee5e23f67be0fd3bded`）。⇒ 一旦构建成功，`-mcpu=tsm-x82`、
   `--print-supported-cpus`、`--print-supported-extensions` **必然**生效。
   “补丁没定义 tsm-x82 ⇒ 重建版不认这个 CPU”的假设被**证伪**。
5. 唯一实质缺口 = **21 个 `TX8*` 后端 pass + TX8 SchedModel + 指令选择 pattern / 向量 legality**
   （厂商只给 `.o`，且 `.o` **无任何 DWARF**）。另有 5 处 `[文档]` 自报的 inferred 项需要收口。
6. 推荐路线：**P0 构建验收（1 会话）→ SchedModel 黑盒恢复（1–2 会话）→ 消融后再决定 pass RE 范围**；
   P2（混合链接）只作差分验证的临时手段，P3（黑盒 oracle）立即固化。**不要现在就开始 RE 21 个 pass。**

---

## 1. 重建现状

### 1.1 基线、规模、验收 `[实测]`

| 项 | 值 | 证据 |
|---|---|---|
| 上游基线 | `llvmorg-21.1.0-rc1` = `cbcb48a88c3a6227cfc09f91880d5cb92a26f5f9` | `third_party/llvm` 的 `HEAD`；`0001` 报告 §Baseline 亦自述 |
| 厂商基线 | `21.1.0-rc1 (…ts-llvm-project b85caf26)` | `llvm-toolchain/bin/clang-21 --version` |
| patch 数 | 磁盘 26 个 `.patch`，**有效序列 25 条**（`0010.new.patch` 被 `0010-raisa-xtsm82s-backend.patch` 取代，未入序列） | `ls patches-series/*.patch`；README 的“27”按名字计数 |
| 序列覆盖 | 146 个路径 = 115 modified + 31 added（clang 116 / llvm 30） | `_status_paths.txt` vs 工作树 `git status --porcelain` |
| 应用结果 | rc1：**OK 25 / FAIL 0**；21.1.0 正式版：**OK 24 / FAIL 1** | `ea_git.log` / `ea_tarball.log` |
| 验收 gate | 工作树 vs “rc1+25 patch” 覆盖层：**identical 146 / differing 0 / missing 0** | `_touched_new.txt` 全量 `cmp` |
| 已应用标记 | 23 个 `.applied_*.patch` 文件（缺 `0006`/`0007`，但其内容确在工作树中） | `third_party/llvm/.applied_*` |
| 现状二进制 | `build-llvm-musl/bin/clang-22` = **stock 22.1.8**，`--print-supported-cpus` 无 `tsm-x82`、`--help` 0 处 raisa | `[实测]` ⇒ **带 fork 的树从未在可复现位置成功构建过** |

`0013` 唯一失败点的根因 `[实测]`：`llvm/include/llvm/Analysis/IVDescriptors.h` 与 `llvm/include/llvm/MC/MCDisassembler/MCDisassembler.h` 在 rc1 与 21.1.0 正式版之间不同（rc1 blob `b985292cc`/`3a7ca1a69`，正是 patch 的 pre-image）；全序列共 10 个文件在两侧有差异，另 8 个的 hunk 上下文恰好不受影响。

### 1.2 patch 序列覆盖的子系统（每条 `.report.md` 的要点）

| patch | 子系统 | 文件 | 报告结论 / 自报验证等级 | 本轮复验 |
|---|---|---|---|---|
| 0001 | clang/llvm **头文件 fork delta** | 35 | 只含公共头；`§239` 明说 `.cpp` 不在范围内 | 应用 OK |
| 0002 | **新增源文件**（`Raisa.h`、`SemaRAISA.h`、16 个 `__clang_raisa_*.h` …） | 20 | 厂商 ship 这些头 ⇒ byte-exact | 资源头 16/16 与厂商安装树一致 `[实测]` |
| 0003 | clang **Attribute** 前端 | 1 | 20 张 Attr 表 byte-exact；`AttrDocs.td` 文本未重建 | — |
| 0004 | **RISC-V 目标描述层**：`tsm-x82` CPU + `xtsm82s` | 2 | 复现厂商 `RISCVTargetParserDef.inc` byte-exact | **md5 相同 `[实测]`** |
| 0005 | **clang builtins**（RISCV） | 1 | `BuiltinsRISCV.td` 半部分 byte-exact，`riscv_vector.td` 半部分当时未做 | **md5 相同 `[实测]`** |
| 0006 | **RVV 向量 builtins** + 3 处 `clang-tblgen` 改动 | 5 | 三张 vendor `.inc` 全部 byte-exact（311 条 `xtsm82s`）；`RISCVVEmitter.cpp:750` 放宽 skip 集 | — |
| 0007 | clang/lib **RAISA 实现**（`SemaRAISA.cpp`/`CGRAISATS.cpp`/`CGRAISARuntime.*`/`Raisa.cpp`） | 9 | 行为级：编译、链接、`-###`、端到端 obj；诊断 companion 未并入 | — |
| 0008 | `StmtNodes.td`（`RAISAKernelCallExpr`） | 1 | byte-exact | — |
| 0010 | **XTsm82s 编码层**（4361 行 `.td` + 反汇编器 + InstPrinter） | 9 | **行为级**：DIS-CORPUS 51525 words/0 mismatch、PER-MNEMONIC dis 397/397 asm 328/397；无 ISel pattern、无 `Sched` | dis **397/397 `[实测]`** |
| 0011 | **诊断**（Sema/Driver/Parse + `RaisaCompat` 组） | 4 | 55 个诊断产物 byte-exact | — |
| 0012 | **build glue**：`__TSM_RISCV__` | 1 | build 验证：`build.ninja` 3288 处 `-D__TSM_RISCV__` | — |
| 0013 | 漏掉的**头文件 delta**（含 `MCSubtargetInfo::getSchedModelForCPU` 变 `virtual`） | 17 | byte-exact 18/18（第 18 个另计） | 仅对 rc1 干净 |
| 0014 | **地址空间映射**（`LangASMap` ×13） | 11 | 用厂商 unstripped `libclangBasic.a` 逐张核对 ⇒ byte-exact | — |
| 0015 | 漏掉的 `.cpp` delta | 2 | `Sema.cpp`（symbol-proven）+ `RISCVTargetParser.cpp`（5 参 `encodeVTYPE`，assembly-proven） | — |
| 0016 | driver `ToolChain.cpp` | 1 | `getSystemRPUArchs`/`AddRaisaIncludeArgs`，vendor `st_size` 精确复现 | — |
| 0017 | `RAISAKernelCallExpr` 的 6 个定义/visitor 站点 | 6 | 17 个 undefined → 0；`StmtProfiler::Visit` 为 **inferred** | — |
| 0018 | **RAISA driver toolchain**（`Raisa.cpp/h` + `Options.td`） | 4 | 76/92 重命名符号 byte-identical；`Options.inc` byte-exact | **md5 相同 `[实测]`** |
| 0019 | **Sema CT**（`SemaCT.cpp` + `SemaRISCV` 调用点） | 3 | 跳表恢复 dispatch；**403/1291 builtin ID 与 lambda 体未恢复（inferred）**；`ninja clang` 完成 | — |
| 0020 | **kernel launch 语法**（`ParseExpr.cpp` 的 `<<<`） | 1 | 符号集对齐（仅 1 处 libstdc++ inline 假象） | — |
| 0022 | **driver wiring**（Triple/Types/Action/Driver/Clang/OffloadArch） | 6 | 三 job argv 与厂商 **669/2341 token byte-identical**；2 处事后修正 | — |
| 0023 | **offload job wiring**（`Clang.cpp` 3 hunks） | 1 | 2 处 assembly-proven + 1 处 **inferred**；host 93/93、device 565/565 | — |
| 0024 | `-frpu-rdc` 并入 `IsRDCMode` | 1 | 4 个 option 序数 assembly-proven | — |
| 0025 | **RAISA 安装探测 + 设备链接** | 1 | ctor/SDL 搬移 assembly-proven，`-target-sdk-version=` behaviour-proven；残余 5776 B 归因 string codegen | — |
| 0026 | **cc1 语言**（`-x raisa`） | 7 | 27/27 cc1 用例 byte-identical；1 处 **inferred**（`.Case("raisa")` 源码位置） | — |
| 0027 | **资源头安装**（`Headers/CMakeLists.txt`） | 1 | 效果 byte-exact（16/16），列表名/位置 **inferred** | — |

### 1.3 本轮新增的独立复验（全部 `[实测]`，见 §附录与 evidence 文件）

本轮新增的独立复验（全部 `[实测]`，原始输出见 evidence 文件）：
- **E-a** 补丁可应用性：rc1 **25/25 OK**；21.1.0 正式版 **24/25**（`0013` 冲突）。
- **基线判定**：86 条带 `index` 的条目中 83 条 pre-image == rc1 blob，3 条例外均为“被序列内前序 patch 改过/新建”的文件（`0023`/`0024` 的 `Clang.cpp`、`0025` 的 `Raisa.cpp`）⇒ 判据 100% 自洽。
- **生成表 byte-exact**：`RISCVTargetParserDef.inc` `3e39e571…`、`Options.inc` `d8aba2c7…`、`BuiltinsRISCV.inc` `cdbd5c3f…` 与厂商 shipped 文件 md5 相同。
- **TableGen 全后端**：7 个后端全部 rc=0；生成 `GET_SUBTARGETINFO_MACRO(HasVendorXTsm82s, false, hasVendorXTsm82s)`；matcher 356 / disassembler 399 / instr-info 1596 处 `XTsm_`。
- **ISA oracle**：397 条恢复编码 → 厂商 `llvm-mc -mcpu=tsm-x82 -disassemble` → mnemonic **397/397 一致**；助记符覆盖 **397/397**（`.td` 390 条 + `InstAlias` `:4333-4359` 7 条）。
- **验收 gate**：工作树 vs “rc1 + 25 patch” 覆盖层 **146/146 identical**。

### 1.4 明确缺口（按优先级）

| # | 缺口 | 证据 | 影响 |
|---|---|---|---|
| G1 | **21 个 `TX8*` pass 源码** | 全树/全 patch `grep TX8SchedStrategy\|TX8TransformSIMTToSIMD\|…` = **0 文件**；厂商 `libLLVMRISCVCodeGen.a` 68 个成员中 21 个是 `TX8*.cpp.o`；`ar` 成员名与 `raisa-port-scope.md` 的 22 个名字（含非 pass 的 `TX8MCInfo.cpp`）吻合 ⇒ 实为 **21 pass + 1 MC 助手**，后者已重建 | 无 SIMT/异构、无 bank 感知、无 register rename/alignment、无 TX8 ASan/UVM |
| G2 | **TX8 SchedModel** | `RISCVProcessors.td:580` 用 `NoSchedModel`；厂商 `llvm-mca -mcpu=tsm-x82` 正常（Dispatch Width 2），资源 `TX82ALU/TX82CSR/TX82FDIV/…` 成体系；同参数下 `generic-rv64` 报 “unable to find instruction-level scheduling information” | 调度质量、`-mllvm -riscv-tx8-sched` 缺失 |
| G3 | **21 个 `.o` 无 DWARF** | 21/21 文件 `readelf -S \| grep -c debug_info` = 0 | 无法用调试信息做 RE，只能反汇编 + 符号 + strings |
| G4 | **ISel pattern / 向量 legality** | `0010` 报告 §9 明确“不声称重建优化层”；厂商会拒绝目标/掩码寄存器重叠，我们偏宽松 | 代码生成质量与诊断保真度 |
| G5 | **5 处 inferred 未收口** | `0017`（`StmtProfiler::Visit`）、`0019`（403/1291 builtin ID + lambda 体）、`0023`（`AddRaisaIncludeArgs` 调用点）、`0026`（`.Case("raisa")` 位置）、`0011`/`0012`/`0027` 的 `.td`/CMake 位置 | 行为差异风险，需语料回归 |
| G6 | **`TX8MCInfo` 是重建物** | `MCTargetDesc/TX8MCInfo.cpp` 自述 “Reconstruction … not a mechanical port”；厂商侧是二进制 | CT 换码边角（segment load/store、b16/b32 限制）需差分验证 |
| G7 | **端到端设备编译依赖外部 RAISA 安装** | `Raisa.cpp:52` 读 `--raisa-path`，`:72` 找 `<path>/txriscv/libdevice`；厂商安装树只有 `lib/clang/21/include/__clang_raisa_libdevice.h`，无 `.bc` | 设备链接需外部 SDK；本仓库未发现该安装 |

---

## 2. 与厂商二进制的差距（逐项：重建版能不能做到）

| 维度 | 厂商 `llvm-toolchain/bin/clang-21` | 重建版（rc1 + 25 patch，构建后） | 判定 |
|---|---|---|---|
| 版本 | `21.1.0-rc1 (b85caf26)` | 同 rc1 | **一致** |
| CPU 表 | `--print-supported-cpus` 含 `tsm-x82` | 生成表 byte-exact ⇒ 含 | **能做到 `[实测]`** |
| 扩展表 | `xtsm82s 1.0 'XTsm82S'` | 同表 byte-exact | **能做到 `[实测]`** |
| `-march` 展开 | `rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_zmmul1p0_zfbfmin1p0_zfh1p0_zfhmin1p0_zca1p0_zbb1p0_zve32f1p0_zve32x1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_zvfhmin1p0_zvl32b1p0_zvl64b1p0_xtsm82s1p0` | 同一 CPU 记录 + 上游 `RISCVISAInfo.cpp`（**未被 fork 改动**）⇒ 展开结果相同 | **一致 `[实测]`+`[推测]`** |
| ABI / feature | `-target-abi lp64f`；无 `+a`、无 `+d`、无 `+v`（仅 `zve*`） | 同 CPU 记录 | **一致 `[实测]`** |
| 汇编/反汇编 | 397 助记符、opcode 恒为 `0x7b` | 397/397 覆盖，反汇编 oracle 397/397 | **能做到 `[实测]`** |
| CT 换码（RVV→0x7b） | `TX8MCInfo` 二进制 | 重建版 `TX8MCInfo.cpp`（行为级，`report_fatal_error` 文案已对齐） | **大体能做到，边角待验 `[推测]`** |
| driver 开关 | 26 个 raisa/rpu/tx8 开关；`-tx8-asan`、`--raisa-noopt-device-debug`、`-frpu-rdc`、`--raisa-path=`、`--tsrpuas-path` … | `Options.inc` **md5 相同** ⇒ 解析层完全一致 | **一致 `[实测]`** |
| pass 层开关 | `-mllvm -riscv-tx8-sched`、`-mllvm -tx8-lmul-sched`、`-mllvm --tx8-vsetbarrier`、`-mllvm --TX8-VPrintf-HW`、`-mllvm -riscv-insert-vsetvli-enable-simtmul` | 树中 **0 引用**（`vsetbarrier`/`vsetsimtmul` 仅作为 `.td` 指令存在，`:3660`/`:3670`） | **做不到** |
| pass 管线 | stock RISC-V 管线 + 21 个 TX8 pass 插桩（`RISCVTargetMachine.cpp.o` 内含） | **无任何 TX8 插桩**（`RISCVTargetMachine.cpp`/`RISCVPassRegistry.def` 0 处 TX8） | **做不到** |
| 调度模型 | 完整 `TX82*` 资源模型；`llvm-mca` 可用 | `NoSchedModel` ⇒ `llvm-mca` 报无调度信息 | **做不到** |
| `-tx8-asan` | fat-pointer ASan | 前端开关/`CodeGenOpts.Tx8Asan` 在，pass 缺 | **只能解析，不生效** |
| 设备侧链接 | 需 `--raisa-path` + `<path>/txriscv/libdevice` + `tsrpuas` | driver 逻辑在（`0025`），外部安装缺失则同样失败 | **对等（都依赖外部 SDK）** |
| 头/资源 | `include/` 全量 + `lib/clang/21/include/__clang_raisa_*.h`(16) | `0022`/`0027` 已装，16/16 byte-identical | **一致 `[实测]`** |

---

## 3. 三条路线 + 成本

### P1 从零补全（RE 21 个 pass + 从零写 SchedModel + ISel/legality）

- **需要的证据**：反汇编（`objdump -dr`）与符号（`nm -C`）给出 21 个 pass 的名字、构造、`getAnalysisUsage`、
  调用的 TM/Subtarget 方法；`llcc --help-hidden` 给出各自的选项与语义；`llvm-mca` 给出 SchedModel 的
  可观测面。**没有 DWARF ⇒ 无变量名、无行号、无类型**。
- **最便宜的决定性实验**：对 `TX8SchedStrategy.cpp.o` 与 `TX8TransformSIMTToSIMD.cpp.o` 各做一次
  `nm -C --defined-only` + `objdump -dr`，统计“新引入符号数 / 对外部 LLVM API 的引用面 / 跳转与常量表规模”，
  得到一个 pass 的 RE 规模标定（≤1 小时）。
- **预计会话数**：SchedModel 1–2（若走黑盒恢复，见 T2）；21 个 pass ≈ **15–40 会话**，
  且 `TX8TransformSIMTToSIMD`/`TX8RegisterRename` 这类语义变换 **`[推测]` 不可从二进制恢复**。
- **风险**：**高**。收益（性能）最大，但正确性无法证明；可能永远达不到位等价。

### P2 混合链接（重建源码 + 厂商 pass `.o` / 整个厂商后端 `.a`）

- **需要的证据**：厂商 `libLLVMRISCVCodeGen.a` 有 68 个成员（含 `RISCVTargetMachine.cpp.o` 与 21 个
  `TX8*.cpp.o`），版本同为 21.1.0-rc1；`llvm-toolchain/lib/` 下 `libLLVM*.a`/`libclang*.a` 齐全。
- **最便宜的决定性实验**：构建重建树的 `RISCV` 目标（T1），把生成的
  `build/lib/libLLVMRISCVCodeGen.a` **替换**为厂商同名 `.a`，重新链接 `clang`，检查
  ① 链接是否成功 ② `-mllvm --help-hidden` 是否出现 `riscv-tx8-sched` ③ `llc -mcpu=tsm-x82` 是否给出
  与厂商相同的汇编。**这是全方案里信息量/成本比最高的实验之一**（≤1 会话）。风险点是跨编译单元的
  结构体布局（`RISCVSubtarget`/`RISCVTargetMachine` 由我们重建的头文件定义，厂商 `.o` 用的是它自己的
  头文件）——若布局不同会静默错误，因此必须用 ③ 的语料差分兜底。
- **预计会话数**：2–4。
- **风险**：中。技术上可行且能立刻得到“全功能”编译器，但**交付物含厂商二进制**，不满足
  “从源码重建”的目标；只适合作为给 P1 提供差分 oracle 的临时件，或作为最终产品的兜底方案。

### P3 黑盒（厂商二进制做 oracle，重建物用于 ISA/ABI 文档与验证）

- **需要的证据**：厂商二进制 + 已恢复的 397 条 ISA 表 + `xtsm82s_fields.tsv` + `-###` argv 对齐数据。
- **最便宜的决定性实验**：固化 golden 语料（T6）并把 ISA/ABI 文档的每条断言挂到一个可重跑命令上
  （本轮已完成 3 条：`-print-supported-cpus`/`-print-supported-extensions`/397-397 dis-oracle）。
- **预计会话数**：1–2。
- **风险**：低。不产出可交付编译器，但它让 P1/P2 的每一步都有判据，是另外两条路线的前置条件。

**成本对比**：P1 证据中(无 DWARF)/会话 15–40/风险高/**满足源码重建**；P2 证据高/会话 2–4/风险中/**含厂商二进制**；P3 证据高/会话 1–2/风险低/不产出编译器。

## 4. 推荐路线与第一批任务

**推荐：P0（构建验收）+ P3（oracle 固化）并行 → T2（SchedModel 黑盒恢复）→ T3（pass 必要性消融）
→ 只有消融显示必需时才启动 P1 的按需 RE；P2 仅作为 T3/T4 的差分工具链，不作为交付物。**

理由：① `[实测]` 重建物在源码层已完整（146/146 gate），唯一没做过的事是**把它构建出来**——这是当前
成本最低、信息量最大的缺口；② `[实测]` 20 个生成表已 byte-exact，说明前端/driver/ISA 层不是风险点；
③ 后端 21 个 pass 的价值必须先用消融量化，否则会投入 15–40 会话去 RE 可能对输出无影响的 pass；
④ SchedModel 可以用 `llvm-mca` **黑盒反演**，不需要 RE。

### T1 — P0 构建验收（会话 1；必须 -j2）

- **目标**：用重建树构建出 `clang`/`llvm-mc`/`llvm-objdump`，证明 `-mcpu=tsm-x82` 生效。
- **完成判据**：三条命令全部命中（见下），且与厂商输出一致。
- **命令**（源码树只读，构建目录在 scratch；`LLVM_PARALLEL_LINK_JOBS=1` + `-j2` 防 OOM；磁盘余量 601 GB `[实测]`）：

```bash
SRC=/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm/llvm
B=/home/shaokai/Desktop/work/tx82/wt-forkplan/build
timeout 900 cmake -S $SRC -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_TARGETS_TO_BUILD="RISCV" \
  -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_PARALLEL_LINK_JOBS=1
timeout 10800 ninja -C $B -j2 clang llvm-mc llvm-objdump
$B/bin/clang --print-supported-cpus --target=riscv64-unknown-linux-musl | grep -x ' *tsm-x82'
$B/bin/clang --print-supported-extensions --target=riscv64-unknown-linux-musl | grep xtsm82s
echo 'and.ss sr1, sr2, sr3' | $B/bin/llvm-mc -triple=riscv64-unknown-linux-musl -mcpu=tsm-x82 \
  -show-encoding -    # 期望 # encoding: [0xfb,0xa0,0x21,0x82]（厂商实测值）
```

### T2 — TX8 SchedModel 黑盒恢复（会话 2–3）

- **目标**：复现功能等价的 `RISCVSchedXTsm82s.td`，替换 `RISCVProcessors.td:580` 的 `NoSchedModel`。
- **完成判据**：在固定语料上，我们构建的 `llvm-mca -mcpu=tsm-x82` 与厂商的
  `Dispatch Width / Latency / RThroughput / 资源清单` 全部一致。
- **命令**：

```bash
V=/home/shaokai/Desktop/work/tx82/llvm-toolchain/bin
$V/llvm-mca -mcpu=tsm-x82 -mtriple=riscv64 -iterations=1 \
  -instruction-info -resource-pressure -instruction-tables corpus.s > vendor.mca   # 397 助记符语料
# 生成 .td 后：
$B/bin/llvm-mca -mcpu=tsm-x82 -mtriple=riscv64 -iterations=1 \
  -instruction-info -resource-pressure -instruction-tables corpus.s > ours.mca
diff <(grep -E '^ *[0-9]' vendor.mca) <(grep -E '^ *[0-9]' ours.mca)   # 期望为空
```

### T3 — pass 必要性消融（会话 2，与 T2 并行）

- **目标**：量化每个缺失 pass 对输出的实际影响，决定 P1 的 RE 范围。
- **完成判据**：一张 `开关 × 语料` 的汇编差异表；`0 差异` 的 pass 直接降级/延后。
- **命令**：

```bash
V=/home/shaokai/Desktop/work/tx82/llvm-toolchain/bin/clang-21
for opt in -riscv-tx8-sched -tx8-lmul-sched --tx8-vsetbarrier --TX8-VPrintf-HW \
           -riscv-insert-vsetvli-enable-simtmul; do
  $V --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -O2 -S corpus.c -o /dev/null -mllvm $opt=1
  $V --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -O2 -S corpus.c -o on.s  -mllvm $opt=1
  $V --target=riscv64-unknown-linux-musl -mcpu=tsm-x82 -O2 -S corpus.c -o off.s -mllvm $opt=0
  echo "$opt: $(diff on.s off.s | wc -l) diff lines"
done
```

### T4 — ISA 层双向回归，用**厂商产出的合法语料**（会话 3）

- **目标**：把 `0010` 的 “asm 328/397” 升级为可复跑的 0-mismatch 回归。
- **完成判据**：`vendor llc -mcpu=tsm-x82` 产出的每一行汇编，被我们的 `llvm-mc` 汇编出的编码与厂商
  `llvm-mc` 的编码逐条相同。
- **注意 `[实测]`**：不能拿 `xtsm82s_isa.tsv` 的 `example_operands` 当汇编输入——该列来自反汇编；
  本轮实测 397 条里厂商自己的汇编器 **181 条报 mask-overlap 合法性错误、18 条直接 `LLVM ERROR`
  中止、7 条落到不同指令**（如 `vmv.v.v`/`vsetvli`），只有 191 条编码一致。
- **命令**：

```bash
$V/llc -mcpu=tsm-x82 -mtriple=riscv64 -O2 corpus.ll -o corpus.s     # 合法汇编语料
$V/llvm-mc -triple=riscv64-unknown-linux-musl -mcpu=tsm-x82 -show-encoding corpus.s > ref.txt
$B/bin/llvm-mc -triple=riscv64-unknown-linux-musl -mcpu=tsm-x82 -show-encoding corpus.s > got.txt
diff <(grep encoding ref.txt) <(grep encoding got.txt)              # 期望为空
```

### T5 — P2 可行性 link test（仅在 T1 成功后、且需要差分工具链时做，会话 4）

`cp llvm-toolchain/lib/libLLVMRISCVCodeGen.a $B/lib/libLLVMRISCVCodeGen.a && ninja -C $B -j2 clang`，
判据：链接成功且 `$B/bin/clang -mllvm --help-hidden | grep riscv-tx8-sched` 命中；
随后用 T4 的语料做 `vendor vs 混合版` 逐字节差分。**`[推测]` 主要风险是重建头文件与厂商 `.o`
的结构体布局不一致**，因此必须用语料差分兜底，不能只看链接成功。

### T6 — golden 语料与 oracle 固化（会话 1，与 T1 并行）

```bash
mkdir -p /home/shaokai/Desktop/work/tx82/docs/compiler/golden
V=/home/shaokai/Desktop/work/tx82/llvm-toolchain/bin/clang-21
$V --print-supported-cpus > golden/vendor-cpus.txt 2>&1
$V --print-supported-extensions > golden/vendor-extensions.txt 2>&1
$V/bin/llc --help-hidden 2>/dev/null | grep -iE 'tx8|raisa|simt' > golden/vendor-mllvm-switches.txt || true
```

---

## 附录 A：原始实验记录（关键行）

**E-a 补丁可应用性**（完整日志：`wt-forkplan/ea_git.log`、`ea_tarball.log`）
```
== git   (rc1, cbcb48a88c): ok=25 fail=0
== tarball (setup/llvm-project-21.1.0.src): ok=24 fail=1
FAIL 0013-raisa-missed-header-deltas.patch :: llvm/include/llvm/Analysis/IVDescriptors.h:47 补丁未应用 |
     llvm/include/llvm/MC/MCDisassembler/MCDisassembler.h:136 补丁未应用
```
基线判定：`index b985292cc..1dc73205a` 的 pre-image 命中 rc1、不命中正式版；
86 条 `index` 中 83 条 pre-image == rc1（另 3 条为序列内前序 patch 改过/新建的文件）。

**E-b `tsm-x82` 与 `xtsm82s` 的确切定义位置**（重建物 = rc1 + 25 patch）
```
patch 0004-raisa-riscv-target.patch:50   +def TSM_X82 : RISCVProcessorModel<"tsm-x82",
patch 0004-raisa-riscv-target.patch:20   +def FeatureVendorXTsm82s : RISCVExtension<1, 0, "TSM X82 extension">
third_party/llvm/llvm/lib/Target/RISCV/RISCVProcessors.td:580  def TSM_X82 : RISCVProcessorModel<"tsm-x82", NoSchedModel,
third_party/llvm/llvm/lib/Target/RISCV/RISCVProcessors.td:595  FeatureVendorXTsm82s]>;   RISCVFeatures.td:1656 def FeatureVendorXTsm82s (1660 HasVendorXTsm82s)
third_party/llvm/clang/lib/Basic/OffloadArch.cpp:96  {OffloadArch::TSM_X82, "tsm-x82", ""},   RISCVInstrInfo.td:2311 include "RISCVInstrInfoXTsm82s.td"
third_party/llvm/llvm/lib/Target/RISCV/RISCVInstrInfoXTsm82s.td:41  class XTsm82sInst<...>  (opcode 0x7b)
vendor  ground truth: llvm-toolchain/include/llvm/TargetParser/RISCVTargetParserDef.inc:105 {"xtsm82s", {1, 0}}
                     同文件 :490 PROC(TSM_X82, {"tsm-x82"}, {"rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_
                     zfbfmin1p0_zfh1p0_zbb1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_xtsm82s1p0"}, ...)
```

**E-c 生成物与厂商 shipped 文件的一致性**（vendor 的 `llvm-tblgen`/`clang-tblgen`，21.1.0-rc1）
```
RISCVTargetParserDef.inc  3e39e57196f69ee5e23f67be0fd3bded  25710 B   两者相同
Options.inc               d8aba2c7557432feb5132860196e8791 1815799 B  两者相同
BuiltinsRISCV.inc         cdbd5c3f03951447a69796a0afeb1c95  58569 B   两者相同
  （注意：用 Builtins.td 而非 BuiltinsRISCV.td 会得到 54966cc2…，不可比）
TableGen 7 个后端全部 rc=0；gen-subtarget.inc:569 GET_SUBTARGETINFO_MACRO(HasVendorXTsm82s, false, hasVendorXTsm82s)
ISA 反汇编 oracle: match=397/397；ISA 表助记符覆盖 397/397
验收 gate: identical=146 differing=0 missing=0
```
