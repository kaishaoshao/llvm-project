# 重建配方（reconstruction = 基线 + 补丁；**每次改动都要落到这里**）

## 基线
`riscv-llvm-musl-toolchain/third_party/llvm` = upstream **`llvmorg-21.1.0-rc1`** (`cbcb48a88c`) + `../patches-series/` 的 **25 条 patch**
（E-a 实测：rc1 上 OK 25 / FAIL 0；正式版 21.1.0 上仅 0013 失败，因 rc1 与正式版本差异）。
唯一没重建出来的是 21 个 `TX8*.cpp` 后端 TU（见 `../backend-objects/`，只有 `.o`）。

## 增量修复（按需叠加，本目录）
| 补丁 | 内容 | 验收 |
|---|---|---|
| `0001-rvv-fp8-saturation-vsetvlbf-header.patch` | `clang/include/clang/Basic/riscv_vector.td` 补上厂商的 raw `HeaderCode` 文本：`__RISCV_SATURATION`/`__s128_t`/`fp4`/`f8e4m3`/`f8e5m2` + `vfloat8e4m3*`/`vfloat8e5m2*`/`vfloat4*` typedef、21 个 `__riscv_vlmul_ext_v_f8*`、24 个 `__riscv_vreinterpret_v_*_f8*/f4*`、13 个 `__riscv_vsetvl*_bf`（调 `__builtin_rvv_vsetvli_bf`/`vsetvlimax_bf`） | 用**已有**的 `clang-tblgen -gen-riscv-vector-header` 重新生成：**533 行、md5 `a1c48d42cd584daa391ea434e4821cf6`，与厂商安装的 `lib/clang/21/include/riscv_vector.h` 逐字节相同**（父会话已在 `wt-forkbuild/build` 里重建并刷新安装副本，两份 md5 均一致） |

## 关键教训（写在这里以免重踩）
**`../patches/` 与 `tracked-modified/tracked-modified.diff` 里的"厂商 delta"本身可能不完整。**
这次就是：我们的 `riscv_vector.td` 与归档 delta **逐字节一致**，但生成的 `riscv_vector.h` 仍比厂商少 111 行
⇒ 归档 delta 在 `riscv_vector.td` 上有遗漏。**判"重建是否与厂商一致"的最终依据只能是厂商的构建产物**
（安装的头文件、`*Gen*.inc`、`.o`），不是归档的源码 delta。

## 已知架构差异（重要，影响所有 MIR/调度/ISel 层对比）

**厂商把整个 RVV 指令空间的重编码放在 MC 层，而不是定义新指令。**
证据：厂商 `llvm-mc` 里有 `convertRVVALUToCTEncoding` / `TX8MCInfo::tryConvertRVVToCTEncoding` 与
全局查找表 `RVV2CTFunctMap`（`docs/compiler/RVV_TO_0x7B_REMAP.md`），规则是
`index = funct3 + 7*funct6`、`word = (w & 0x03ff8fff) | 0x7b | new_f3<<12 | new_f6<<26` ——
即**同一条上游指令（如 `VADD_VV`）在 `+xtsm82s` 打开时被改写成 0x7B**（双解码：只有 `+v` 时仍发 0x57）。
厂商的 MIR/调度层因此仍用**上游基名** `VADD_VV`。

**我们的重建（`patches-series/0010-...` + fork 源文件）把这 172 条定义为独立的 `XTsm_*` 指令**
（如 `XTsm_VADD_VV`），编码值经普查/`.td`/反汇编三方核对**与厂商逐位一致**，但造成：
- MIR / `-print-after` dump 的指令名与厂商不同（对比时需要 `llvm-mc -show-inst` 之类桥接）；
- `llvm-mca` 的 LMUL 重定向（`RISCVVInversePseudosTable::getBaseInfo`）在我们这里查不到行
  ⇒ 回落到静态基类（残差，见 `docs/compiler/TX8_SCHEDMODEL.md`）；
- 任何按指令名写 `InstRW`/ISel 模式的地方都要做名字映射。

**要做全 parity，最终应把策略改成厂商那样**：保留上游基名 + 在 MC 层重编码（约 0.5 会话，见 SCHEDMODEL 文档残差一节）。
在改之前，**任何"指令名不同"引起的差异都不算模型/语义缺口**。

## 从厂商 MIR 反推出来的两条"反直觉"事实（G1 修复时实测）

1. **10 条 TX8 专有指令的 `rd` 槽是输入，不是输出。** 厂商 `-mllvm -print-after=finalize-isel` 里
   `Gemm killed %15,%16,%17` **没有 `%n =`** ⇒ 这些指令 `NumDefs = 0`。我们的 `.td` 原来写成
   `(outs GPR:$rd)`，必须改成 `(ins GPR:$rd, ...)`：**编码与汇编文本都不变**，但 `NumDefs` 1→0 与厂商一致
   （否则内建永远选不上）。判据：改后 `AsmMatcher`/`AsmWriter`/`MCCodeEmitter` 的 TableGen 产物不变。
2. **统计 0x7B 指令必须按指令长度游走 + 掩码 `(w & 0x7f) == 0x7b`，不能 grep `"7b"`。**
   `gemm`/`ne_set_length` 这类字的低字节是 `0xfb`。厂商 14 个 kernel 的真实 0x7B 总数是 **77**，
   早先按 `grep 7b` 数出来的 44 是错的（会漏）。

## 已归档的增量补丁
- `0028-raisa-rvv-fp8-saturation-header.patch`（RVX 头：`__RISCV_SATURATION`/`vfloat*`/`vsetvl*_bf` 等 111 行）
- `0029-raisa-vsetivli-disable-xtsm82s.patch`（ISel 层禁用 VSETIVLI 折叠）
- `0030-tx82-xtsm82s-isel-lowering.patch`（10 个 `Pat<>` + 9 条 def 的 `NumDefs` 修正；**14/14 kernel 逐字节一致**）
- `0031-tx82-vsetivli-no-fold.patch`（`RISCVInsertVSETVLI.cpp` 第二处 `PseudoVSETIVLI` 生成点的守卫）
- `0032-tx82-schedmodel.patch`（`RISCVSchedTX82.td` 8552 行 + 接线 + 5 个 tune feature）

### 0030 的最终形态（2026-10-16 更新）
`0030-tx82-xtsm82s-isel-lowering.patch` 已由 **10 个内建版**升级为**厂商可选 83 个内建全覆盖版**（1251 行）。
总账：**83/83 下降**（66 条 def 的 `rd` 由 outs 改 inputs，`NumDefs` 1→0；标志加 61 / 删 16）；
**82/83 与厂商 `.o` 逐字节一致**（14 kernel `.o`/`.so` 14/14、`0x7b` 77=77 不变）；
唯一例外 `kcore_ncc_read`（厂商把实参丢掉、`rs2` 钉 `x0`，照抄会被 TableGen 拒 `Pattern has dead named input`，
只能写 C++ `Select()` case）。CModel 逐位：**37 个完整执行且逐位一致 / 46 个双方同样 abort 或阻塞 / 0 不一致**。
另有 **84 个内建不可做标量探针**（73 个没有 `ClangBuiltin`，11 个需 `llvm_v1i128_ty`），本轮未下降。

### 两条 TableGen 机制（踩过，务必照做）
1. **`imm` 的形式**：`EmitResultOfNamedOperand` 只在**目标侧** operator 名叫 `imm` 时才插入"常量→目标立即数"
   转换 ⇒ **`Pat` 两侧都必须写 `imm:$imm`**；写成 `XTsmUImm5:$imm` 会在 MC 层断言
   `getImmOpValue expects only expressions or immediates`。另外厂商对这 5 条 5 位立即数指令**不做范围检查**
   （传 0x1008 打印 `l0_flush a0, 4104, a1`，编码取低 5 位）——**照抄**，不要加 clamp。
2. **标志必须与内建属性"精确相等"（不是"至少"）**：保守多加 `mayLoad/mayStore` 会报
   `Pattern doesn't match mayStore = 1`。规则：`IntrNoMem`→无、`IntrWriteMem`→`mayStore`、
   无内存属性→`load+store`、`IntrHasSideEffect`→`sideEffects`，**多一个都不行**。

### 0034-tx82-insert-xfrm（RA 前的 `tx8-insert-xfrm`）+ **两条因果更正**
实现：向量访存追加 `implicit $mem_alloc`（插在显式操作数之后、`$vl`/`$vtype` 之前）；
`TSFlags` bit18 HasRoundModeOp 且非 bit19 UsesVXRM 时 `implicit $frm` → `implicit $xfrm`；
静态舍入模式（frm 立即数 0..6）前插 `ADDI gpr,x0,mode`+`vsetxfrm`、后插 `vsetxfrm $x0`（RDN↔RUP 互换）。
新寄存器：`FRM=4`、`MEM_ALLOC=5`、`XFRM=13`、`X0=102`（本树 3/4/12；相对次序一致，实测不影响输出字节）。
附带：`RISCVInsertReadWriteCSR::runOnMachineFunction` 在 `hasVendorXTsm82s()` 时 `return false`；
`verifyInstruction` 接受 `$xfrm`。补丁基线 = rc1 + 0001/0002/0030/0031/0032/**0033**。

**更正 1**：`implicit $mem_alloc` 在厂商侧是 **`tx8-legalize-ct`** 加的（`print-after=finalize-isel` 无、`print-after=tx8-legalize-ct` 有），
不是我方这个 pass；本树没有那个 pass 故效应并入（对 `-print-before=greedy` 净效应相同，已验证）。
**更正 2（关键）**：**`v8`→`v0` 不是 RA 前的事** —— 厂商自己的 `-print-after=greedy` 也是 `renamable $v8`，
逐 pass 二分显示首次变 `$v0` 的是 **post-RA 的 `vreg-rename`（0033）**。
⇒ 要 `-O2` 向量对象与厂商字节相同，**必须先有 0033**；`wt-isel/src` 快照里没有 0033，任何基于它的
"RA 前就不同"的结论都要重算。**今后一律从共享树 `third_party/llvm` 取新拷贝**（它才是规范基线）。

## 规范基线与"已构建的参考工具链"
- **规范源码基线 = `/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm`**，
  当前含 rc1 + 以下补丁：`0028` RVV 头、`0029`/`0031` vsetivli 守卫、`0030`(完整版, 83 内建 ISel)、
  `0031` vsetivli 第二处守卫（2026-10-16 补应用）、`0032` SchedModel、`0033` vreg-rename、`0034` insert-xfrm。
  **任何新会话都从这里取拷贝**，
  不要用别人的 `wt-*/src` 快照（它们可能缺后续补丁 —— 已经发生过一次：`wt-isel/src` 缺 `0033`，
  导致"RA 前就不同"的错误归因）。
- **当前唯一已完整构建（含全部补丁）的 clang = `wt-xfrm/build/bin/clang-21`**（`-O0/-O1/-O2` 验证过）。
  要新的构建，先 `rsync` 规范基线树再编，别在别人的构建目录上改。
- 增量重建工具：`wt-isel/tools/minibuild.py`（表驱动，20 s–12 min/轮；libs 步骤可能中途失败，
  用 `wt-xfrm/tools/rearchive.py` 补归档）——已被三条线复用。

## 已知陷阱（2026-10-16 由 RAISA/CT 线实测，务必照做）

**A. 从别人的 `build/` 拷来的构建目录，它的 `.ninja_deps` 是二进制、仍指向旧 `src`**
⇒ **纯 `ninja` 不会因为"头文件改动"而重编译**（改 `.cpp` 会，改 `.h` 不会），会得到"看起来编进去了、
其实还是旧目标文件"的假绿。RAISA/CT 线为此浪费过一整轮（clang 侧 `Targets/RISCV.h` 的
datalayout 没进二进制，反而报出"DL 不匹配"）。

**检查清单（每次改完源码后）**：
1. `( cd <worktree>/build && ninja -t deps > ../logs/ninja_deps.txt )` —— 刷新依赖表；
2. `python3 <worktree>/tools/minibuild.py --steps tblgen,objs,libs,link --jobs 2 \
     --force-src "<所有改过的 .h 和 .cpp，逗号分隔>"` —— **`.h` 必须一起列**；
   （`minibuild.py` 顶部的 `ROOT` / `OLD_BUILD` / `OLD_SRC` 要指向本 worktree 与它派生自的目录，
   例如 `wt-raisa-be/tools/minibuild.py` 的 `OLD_SRC = wt-xfrm/src`。）
3. 自查：改完 `.h` 后先 `ninja -n <target>`（或用 `ninja -t deps` 查受影响 TU）确认受影响的 TU
   **在计划里**；不在就说明依赖表还是旧的，必须先走 `--force-src`，不要直接 `ninja`。

**B. 构建的 link 步骤是"原地覆盖" `bin/clang-21`**
⇒ **不要与语料采集/对照实验并跑**：先跑的格子会被测成旧二进制。
RAISA/CT 线的 `abi_*_front` 9 格就这样被污染过一轮（表现为"我方 IR 还是旧 datalayout"）。
**跑语料前先确认没有任何正在进行的 link / ninja / cc1plus。**

## 资源纪律（血的教训，2026-10-16）
一条 RAISA 线跑了 **裸 `ninja lib/libLLVMRISCVCodeGen.a`**（没带 `-j`）⇒ 在 8 核 / 14 G 的机器上起出
**159 个并发 `cc1plus`**，swap 吃到 **14 G**、可用内存只剩 **452 MB**（本机此前被 OOM 杀过测试进程、
也有过磁盘被写满）。停掉它的构建时，那个 agent 也随之不可达了（它的构建是它自己的子进程树）。

**规矩**：
1. **任何 ninja / 构建都显式带 `-j2`**（`ninja -C <build> -j2 <target>`）；用 `minibuild.py` 之类助手前
   **先看它的并发默认值并强制 2**。本机 8 核 14 G，默认并发不可接受。
2. 大构建前先 `free -h`：**可用内存 < 4 G 就等**（别的线可能也在编）。统一 `timeout` + `ulimit -f`。
3. 一次只跑一个构建；构建被外部停掉**不是代码问题**（ninja 可续跑）。
4. 失控的 cmodel/qemu 进程也要收：曾有一个 `tx82_run_host.elf` **挂了 2.5 小时**占着 196% CPU。
   跑 cmodel 一律 `timeout`，并在用例之间检查残留。
5. 杀进程树时**不要用 `pgrep -f '<包含你自己命令行的字符串>'`** —— 会匹配到自己的 shell 并把自己杀掉
   （已经发生过一次）。用 `pgrep -x` 或先列 PID 再逐个核对 `ps -p <pid> -o args=`。

## 已知重建缺口：**G0 = `CodeGenModule::createRAISARuntime` 缺失**（2026-10-16 实证）
厂商 `clang-21` 里有 `T clang::CodeGen::CodeGenModule::createRAISARuntime()` 与 `T clang::CodeGen::CreateTSRAISARuntime(...)`；
**规范树 `clang/lib/CodeGen/CodeGenModule.{h,cpp}` 里 RAISA 命中数为 0**（`patches-series/0007-raisa-lib-impl.report.md` §8.1
自己也把这条列为"重建未携带的 delta"）。后果：任何 `__attribute__((global))` / `.rc` 设备编译会在
`CodeGenModule::getMangledName → getCUDARuntime()` 的 `assert(CUDARuntime != nullptr)` 上死掉（我们是
Release+Assertions；厂商是 Release 无断言，所以它的断言操作数从不求值）。设备专用编译可用"断言里加
`getLangOpts().RAISAIsDevice ||`"复现 Release 行为；**host 侧 RAISA kernel stub 需要真正的 runtime**。
规模 ≈ 1 个成员 + `createRAISARuntime()` + 4–6 个调用点（finalizeModule / internalizeDeviceSideVar /
handleVarRegistration / getDeviceHandle / device-side name mangling），可从厂商 `.o`/二进制恢复。

## 0040–0044：RAISA/CT 前端（2026-10-16，全部关闭）+ **三条推翻首版重建的发现**
从**厂商目标文件**（`libclangSema.a(SemaCT.cpp.o)` / `SemaRISCV.cpp.o`，用 `wt-raisa/tools/decode_semact.py` 解码）
而不是从首版推断，纠正了三处：
1. **`CheckFixedVector` 比的是元素位宽（bit width），不是元素个数。** 厂商拒 `__riscv_vadd_vv_i8m1`
   （"this intrinsic with type '__rvv_int8m1_t' is not supported by CT"）与 `_i64m1`，接受 `_i16m1/_i16m4/_i16mf4/_i32m8`；
   旧的"按元素个数"读法会**误拒 `m4`/`mf4`**。
2. **CT 只对 `BuiltinID ∈ [BI__builtin_rvv_vadd_vv, +0xE60] && TI.hasFeature("xtsm82s")` 调用**，
   且 `return false` 的语义是"**回落到普通 RVV 检查**"，不是报错。gdb 证实 `__riscv_read_reg_tid_x()`（ID 5348）
   从不进入 `CheckCTBuiltinFunctionCall`。旧的 `if (getLangOpts().RAISA) return CheckCT(...)` **误拒了所有
   `__riscv_read_reg_*` / `__raisa_*` 内建**。
3. **`DIAG_START_SEMA = 2821`**（不是 2820）：`0x1456` 是无类型诊断、`0x1457` 是 `..._with_type`（校验器）。

效果（137 格 RAISA 语料）：我方前端诊断从 **119 `with type` + 21 attr 断言 + 13 普通 CT + 9 越界 + 1 Arg-access → 0**；
与厂商**逐字相同**的诊断 0 → **27**；`__raisa_*` IR 从崩溃 → **50/50 与厂商逐行一致**（仅 SSA 名/module-id 差异）。
⇒ **剩余阻塞全部移到后端**（103 格都在 `RISCVISelLowering.cpp:14610` 死），不再是前端问题。

**以后凡是"从首版推断"出来的 CT/RVV 规则，都必须用厂商目标文件复核**（与"归档 delta 不可信"同源）。

## 写 IR-level fork pass 的四条陷阱（`simt2simd` 重建时实测，2026-10-16）
1. **改 kernel 签名后不能整体拷贝 `AttributeList`** —— 参数属性个数不匹配会报
   `Attribute after last parameter!`；只拷 **fn attrs**。
2. **`Function::Create` 对同名函数会加 `.1`**；替换旧函数后必须 `setName(旧名)`，否则破坏导出符号名。
3. **RVV 内建的重载类型个数很难猜**（例：`vfadd.nxv2f32.nxv2f32.i64` 需要 3 个 overload type：
   结果、第二操作数、frm/VL 常量），`Intrinsic::getDeclaration` 猜错就 assert。
   **按厂商 dump 里的名字手建声明**（`nxv2f32` 由 element type 算），彻底绕开猜测。
4. **删除旧指令必须"先建完所有替换、再逆序删"**：否则 `Use still stuck around`；
   且**定义先入栈**（`reverse` 后才能先删使用者）。

**另加一条纪律重申**：scratch/临时脚本**不要写 `/tmp`**（本会话又发生一次：写了 `/tmp/c*.sh`，
已删）。`/tmp` 被外部清理过多次，且会让"证据可复现"失效 —— 一律写 `wt-<线名>/logs/`。

## `simt2simd` 首格（`mathdev_raisa_fadd_rn_f.rc`）的三处 asm 差异 —— **全部在下游，不是本 pass**
**【2026-10-16 更正：parent 先前"AVL/policy 不同"的假设已被厂商 `after.ir` 逐字证伪。】**
厂商 `recon/simt2simd/mathdev_raisa_fadd_rn_f.after.ir`（verbatim）显示：
- AVL 就是 `ConstantInt 128`（**不是** VLMAX 语义的值），vle/vse 的 passthru 就是 `undef`；
  `vfadd` 的第 4 操作数是 `i64 0`(frm)，**没有**额外 policy 常量；
- 参数解包在 IR 里是 `gep i8,%0,0 / 8 / 16`，**与我方一致**；asm 里的 `ld 8,16,0` 是后端调度/分配的产物；
- 厂商 IR 里 `grep -i simt` **0 命中** ⇒ `vsetsimt 1` 由**下游**发出。

⇒ 我方 `simt2simd` 产出的 IR 与厂商**逐字相同**（5 个 call 的操作数列表 + 解包顺序已核），
剩下三处差异（`vsetsimt 1`、`vsetvli a4,zero,…,ta,mu`、`ld` 顺序）属**后端/其它 pass** 的工作项：
`vsetvli` 的寄存器形式(VLMAX)+`mu` 与 `vsetsimt` 很可能是**同一处 SIMT-aware 逻辑**，应一起定位
（候选：SIMT-aware 的 `RISCVInsertVSETVLI`，或 `tx8-profiling-kernelfunc`/`vsetbarrier`）。
**教训与 §1/§5 同源：任何"我猜是 X 的构造不同"都必须回厂商 dump 逐字核对再下结论。**

## `RISCVInsertVSETVLI` 的 SIMT 判定（parent 直接从 REA 伪代码 + 行为反证得到，2026-10-16）
**判据原文**（`tools/re/out/RISCVInsertVSETVLI/decompiled.c` 第 8082-8086 行，函数 `runOnMachineFunction @ 0x10baa0`）：
```c
if ((*(char *)(*(long *)(param_1 + 0x38) + 0x21b) == '\0') ||      // MF->getSubtargetImpl() 的缓存 bool
    (*(int  *)(*(long *)(param_1 + 0x38) + 0x34)  != 0x2a)) {        // 另一个 int 字段 == 42
  param_1[0xc0] = 0;        // SIMTMode = false
} else {
  param_1[0xc0] = 1;        // SIMTMode = true（后面按 MBB/MI 逐条插 VSET_SIMT + PseudoVSETVLIX0）
}
```
⇒ **`SIMTMode = (subtarget[0x21b] != 0) && (subtarget[0x34] == 0x2a)`**，是 **per-function** 开关（不是 per-query）。

**行为反证（决定性的那一半）**：普通 C（musl + `-mcpu=tsm-x82`，`avl100.c`，`-O2 -fno-vectorize`）厂商产物里
**`vsetsimt` 出现 0 次**，且 vsetvli 是 `vsetvli zero, a3, e32, m1, ta, ma`（寄存器形式 + **ma**）；
而 RAISA 设备格（`.rc`）4/4 都是 `VSET_SIMT 1` + `PseudoVSETVLIX0 $x0, 80`（`ta,mu`）。
⇒ **`hasVendorXTsm82s` 不能当 SIMT 判据**（普通 xtsm82s 明确不走 SIMT 路径）；SIMT 是 **RAISA 设备路径特有**，
与 `+0x34 == 0x2a` 这个字段一致。⇒ 实现时必须在 `runOnMachineFunction` 里按上面两条件置 `SIMTMode`。
（另：`+0x21b` 这个字节在 `RISCVInsertReadWriteCSR` 的分析里也出现过——同一个缓存 bool 被两个 pass 读。）

## Ghidra 对 RISC-V 设备内核的可用性（2026-10-16 实测，cmodel 线有用）
- **REA 的 Ghidra 适配层不支持 RISC-V**：`rea.sh analyze <kernel.so>` → `capability_unavailable / target_unsupported`。
- **裸 Ghidra 12.1.4 支持**：`tools/re/ghidra_decompile.sh <kernel.so> <outdir>` 实测成功——
  `addcdiv.so`（厂商 RISC-V 设备内核）→ **9,614 行伪代码 / 347 个函数** + 18 KB 调用图，
  且 `gemm_with_psum_v2` / `gemm_with_input_psum_v2` / `get_g3d_ne_layout` / `dte_rdma_stride_copy`
  等缺口符号都在。
- **限制**：TX8 私有指令（custom-3 `0x7B` 等）Ghidra 不实现 ⇒ 纯私有指令的函数伪代码会**截断**并标
  `Control flow encountered unimplemented instructions`（例：`gemm_with_psum_v2` 只剩一个薄包装
  `FUN_ram_00010e50(...)`）。⇒ **Ghidra 给"结构与调用关系"，指令级语义仍按我们的编码表逐条翻译**。
- 结论：cmodel 线做"缺口符号"时应**先用 Ghidra 拿结构**（谁是包装、真身在哪、调用图、字符串/printf 用法），
  再对私有指令段做逐指令翻译；不要指望伪代码直接给出算法。

## RAISA/SIMT `.rc` 首次逐字节一致（2026-10-16）+ **5 条经证据纠正的判断**
达成：`mathdev_raisa_fadd_rn_f` / `mathdev_raisa_fmax_f` 的 **`.s` 除 `.ident` 外一致、`.o` 去掉 `.comment` 后逐字节一致**；
MIR 形状/偏移/立即数与厂商同：`8B XT8_VSETSIMT 1` → `120B PseudoVSETVLIX0 killed $x0, 80`（仅**指令名**不同：我方 `XTsm_VSETSIMT` vs 厂商 `VSET_SIMT`，是已登记的命名差异）。
复现：`python3 wt-raisa-be/tools/raisa_oracle.py --cc wt-raisa-be/build/bin/clang-21 --cells mathdev_raisa_fadd_rn_f,mathdev_raisa_fmax_f`

**纠正（每条都有证据，推翻 parent 先前给的方案）**：
1. **`SIMTMode` 判据解出为 OS，不是调用约定**：`RISCVSubtarget+0x21b = bool HasVendorXTsm82s`、`+0x34 = Triple::OSType`，
   `0x2a = 42 = Triple::RAISA`（偏移用 `-fdump-record-layouts` 核过）⇒
   **`SIMTMode = hasVendorXTsm82s() && getTargetTriple().isRaisa()`**；`CallingConv::RPU_KERNEL(0x7c)` **只**用于门控
   `VSET_SIMT` 的发射。**用 CC 当判据会误伤非 RAISA 编译**（143 个 `.c` 格正是靠 OS 判据天然不受影响）。
2. **`insertVSETVLI` 里不需要 SIMT 分支**：厂商的 `PseudoVSETVLIX0 $x0, 80` 是**上游 `Info.hasAVLVLMAX()` 路径**的自然结果；
   我们原来出 `ADDI+PseudoVSETVLI`，根因是**RAISA 靶的 VLEN 是固定 4096 bit**（128 lane × 32 bit）。
   `min=max=4096` ⇒ `RISCVVectorPeephole::convertToVLMAX` 把 AVL 128 变成 `VLMaxSentinel`（VL 槽 `-1`，与厂商 MIR 一致）。
   也解释了厂商 `ADDI+PseudoVSETVLI %29,337` 那种情形（e32/m2 → VLMAX 256 ≠ 128 ⇒ 不给哨兵）。
   证据：厂商 `-print-after=riscv-vector-peephole` 已是 `-1`；且对厂商强推 `-riscv-v-vector-bits-min/max=1024` 仍会转换 ⇒ 它有精确 VLEN。
3. **整寄存器访存形态必须在 RAISA 上抑制**：厂商 **96 个 RAISA SIMT 格 0 次** `vlNr.v/vsNr.v`，而普通 musl `-mcpu=tsm-x82` 编译有 **15 次**。
4. **另外两处 one-liner（MIR diff 才发现）**：① `SET_MEM_ALLOC` 必须 `implicit-def $mem_alloc`（否则 post-RA 调度器把 `vsetsimt` 下沉两个槽位）；② 目标文件 `e_flags` 必须带 **`EF_RISCV_SIMT (0x1000)`**（厂商 0x1001，我们原来 0x1；`EF_RISCV_SIMT` 在我们 ELF.h 里已有，只缺 setter）。
5. **`-riscv-insert-vsetvli-enable-simtmul` 默认为 false，且 SIMT 路径不依赖它**：不加只发 `VSET_SIMT 1`；加了再发 `VSET_SIMTMUL 0`。

**残余**：(a) `IsInstrDemandMaskPolicyUnderSIMT` 的厂商 opcode 白名单未复现（那些 id 索引厂商自己的指令枚举、无法映射到我们位移过的 `XTsm_*` 编号；**可观测效果** `bool* = (destEEW==0)` 已实现）；(b) **BF16 vtype bit 8**（`0x100`，厂商 335 vs 我们 207）缺失 —— 我们 `VSETVLIInfo` 没有 BF16Fmt 字段，**独立缺口**；(c) 向量指令上的 `implicit $exec` 未加（**故意**：`TX8InsertXFRM` 也跑在 musl/xtsm82s 上，加了会破坏 56/56 门槛）。

## RAISA/SIMT 第 1 轮结果（2026-10-16，新会话）：4 格逐字节 + 门槛全过
**门槛**：musl A/B **56/56 结果一致**（46 个 `.o` 逐字节一致 + **10 个"双方都失败且诊断逐字节一致"**；
那 10 个是普通 `-O2` 的 MC 层既有缺口 `Unsupported RVV instruction with funct6=0b000011 funct3=0b001 …`，
厂商也一样失败 ⇒ **"56/56"应读作"结果一致"，不是"56 个都编出对象"**）。
317 格（同一归一化口径）：**逐字节一致 132 → 136**，OURS-GAP **77 → 77**，byte-diff 41 → 37，
vendor-fail-too 36，vendor-fail-same-diag 30，ours-extra-ok 1，**`.c` 侧仍零缺口**；
判定变化**恰好 4 个**，全是 `byte-diff → byte-identical`，无一反向。

**新增逐字节一致的 4 格**：`mathdev_raisa_f{add,max}_f{,16}`（`raisa_oracle.py --all`，一条命令可复现）。

**剩余 110 个 `.rc` 格的分类（同一 oracle run 得出，可复现）**：
| 类 | 量 | 说明 |
|---|---|---|
| 仅物理寄存器选择不同 | **6** | `fadd_{rd,ru,rz}_{f,f16}`：指令序列+立即数全同，厂商 pre-postmisched MIR 把 `x12=ADDI 512`/`x13=第二指针` 与我方**互换** ⇒ 贪心分配器的 vreg 编号 tie-break（IR/ISel 值编号差异，非语义差异）。根因候选 **`implicit $exec`**：厂商向量指令带它，我方树**根本没有 `EXEC` 物理寄存器**（`grep EXEC RISCVRegisterInfo.td` = 0） |
| 仅缺 vtype bit 8（bf16） | **5** | `fadd_{rd,rn,ru,rz}_bf16`、`fmax_bf16`：厂商 imm **335** vs 我方 207 ⇒ `VSETVLIInfo` 要加 `BF16Fmt`（policy 字节 `+0x13` 的 **bit 3**，已是 fork 过的 `RISCVVType::encodeVTYPE` 第 5 参）—— **快赢** |
| 其它 7 个 | 7 | `config_{across_compute,mixed,overwrite,repeat,with_barrier}_device`（无向量指令，缺 fork pass）；`imm_l1_flush_{31,32}_object`（`l1_flush` 操作数不同：厂商 `ld a1,0(a0); ld a0,8(a0); l1_flush a1,31,a0` vs 我方 `l1_flush a0,31,a1`） |
| 编译失败 | **81** | 77 OURS-GAP + 少数厂商也失败：`cos/sin/log2/exp2/fdiv/frsqrt/reduce_{sum,fmax,fmin}/min/max/umax/umin`、`abi_*`/`id_*`/`control_*`/`mem_*` ⇒ **step 2（simt2simd 家族扩表）**，`recon/simt2simd/mapping.tsv` 是收敛判据 |

**批准的下一轮顺序**：① BF16（5 格，快赢）② `simt2simd` 家族（81 格，最大杠杆）③ `EXEC`/寄存器 tie-break（6 格，需新增 RAISA-gated 物理寄存器，参照 patch 0034 的 FRM/XFRM/MEM_ALLOC）。

## ⚠️ 厂商缺陷类：`rpu-lower-shared` 的共享地址布局**依赖 ASLR、不确定**（2026-10-16，parent 独立复验）
**实测（parent 亲自跑，用 oracle 的精确 flags）**：同一份 `alloc_l1shared_base.rc`、同一条命令行，厂商 clang 连编 **8 次 → 2 种输出**（1524 B×5 / 1617 B×3）；
`setarch $(uname -m) -R`（关 ASLR）后 **6/6 一致**。R2 线的测量更细：`alloc_l0{w,a}shared_base`/`mem_l1_multi_default` 也是 2 种；三变量探针 **3 种**；
**pre-pass IR 每次都相同**（全局声明顺序稳定）⇒ 不确定在**pass 内部**（它迭代 pointer-keyed 容器 `getTransitiveUsesOfShared`/`SharedUsesInfoTy`）。
`-R` 下顺序**仍是** `(c,a,b)`/`(b,a)` 这种非声明顺序（且指针值不同）⇒ **即使关 ASLR 也无法逐字节复刻**。

**确定性部分（已解出、必须复刻）**：`base(as3 L0A)=0x40000000`、`base(as4 L0W)=0x40100000`、`base(as2 L1)=0x101000000`；
随后按（不确定的）顺序 `offset_i = align_up(offset_{i-1} + size_{i-1}, align_i)`（用全局自身 `align` 属性与类型 alloc size）——
8 种观测布局**全部**是"某个排列下的顺序分配"。

**验收口径（parent 决定）**：
1. **≥2 个共享变量的格**：**逐字节一致不是良定义目标**，登记为新类 **`VENDOR-NONDET`**；要求 =（a）我方输出等于厂商 N≥8 次运行观测到的**某个**布局（或排列归一化后一致），且（b）**确定性部分**（基址 + 步长规则）逐字复刻；
2. **只有 1 个共享变量的格**（`mem_l1_size_{1,3,127,128,129,256}`、`mem_l1shared_u{8,16,32,64}_uniform`、`mem_l0{w,a}shared_u{8,16,32,64}_uniform`）才是有意义的逐字节一致目标；
3. **oracle 必须两侧都在 `setarch -R` 下跑**，并对多变量格做**排列归一化比较**（比较地址多重集 / 归一化 `inttoptr` 立即数），否则是抛硬币；
4. 这类格的 `byte-diff ⇒ 我方 gap` 归类**作废**（今天的 byte-diff 计数对它们是抖动的）。

**由此改派的工作**：`-riscv-bank-aware` 定义在 `RISCVTargetMachine.cpp`，且**在 `rpu-lower-shared` 上游被消费** ——
4 个 `*_bank_on` 格的 pre-pass IR 里**已经**是 `inttoptr(i32 0x40000000/0x40020000)` 且**没有全局** ⇒ **bank 地址由 `simt2simd`（A 线）产出**（bank 模式 128 KiB 间隔；base 模式 128 B 间隔 `0x40000000/0x40000080`），B 线对那 4 格只需**定义选项 + 注册 pass 名**。

## RAISA/SIMT 第 2 轮 ①：BF16 vtype bit 8 = patch 0009（2026-10-16，A 线）
**补丁**：`wt-raisa-be/patches/0009-raisa-bf16-vtype-format-bit.patch`（2 文件 / 228 行；已用 `patch -p1` 对 pre-image 回放，逐字节复现当前 `wt-raisa-be/src`）。
**MIR 证据**（`raisa_oracle.py --mir`，5 格全部）：`-print-after=riscv-insert-vsetvli` 里
厂商 `PseudoVSETVLIX0 killed $x0, 335` vs 我方**现在也是 335**（改动前 79）。唯一剩余 MIR 差异仍是指令名 `VSET_SIMT`/`XTsm_VSETSIMT`（已登记的命名差异）。
**更正一条口径**：本文件上面写的"厂商 335 vs 我方 207"是**在另一个 pass** 上读到的；在 `riscv-insert-vsetvli` 上我方**本来就是 79**（`e16,mf2,ta,mu`，无 altfmt），
所以唯一差就是 **bit 8（0x100）**。335 = 79 | 0x100。

**根因比"给 VSETVLIInfo 加个字段"深一层（实测）**：厂商为 bf16 算术**选了不同的伪指令** ——
`finalize-isel` 里厂商是 `PseudoVFADD_BFloat16_VV_MF2_E16`，我方是 `PseudoVFADD_VV_MF2_E16`；
厂商 `libLLVMRISCVDesc.a` 里有 **876 个 `_BFloat16_` 伪指令名**；其 `DemandedFields` 多一个字节（第 8 字节 = bit 8 相等性检查，`tools/re/out/RISCVInsertVSETVLI/decompiled.c` 350-356 行），
该字节由 `RISCV::getCTPseudoInfoByBF16/ByHalf`（`RISCVBaseInfo.cpp.o` 里两张 **540 项二分查找表**）按 opcode 决定，条件是 `hasSEWOp && SEW操作数==4`（log2 16）。
上游 LLVM 的做法同源：`AltFmtType` TSFlags 字段 + `PseudoVFADD_ALT` 一族（PR #161158 / #170612，本树基线 rc1 两者都没有）。
**patch 0009 的取舍**：`VSETVLIInfo`/`DemandedFields`/`areCompatibleVTYPEs`/`encodeVTYPE(第5参)` 全部按厂商语义实现（这部分是"真的"），
但**per-opcode 表用函数属性 `"tx8-simt-bf16-fmt"` 代替** —— 由 `TX8TransformSIMTToSIMD` 在真的产出 bf16 向量算术时打上。
理由：`RISCVInsertVSETVLI` 处元素类型**不可恢复**（e16 的 f16/bf16 共用 `VR`/`VRM2`/...，`MRI.getType()` 只给出寄存器类派生的 LLT），
而要拿到 per-opcode 判定必须先把厂商那套 bf16 伪指令集重建出来（= TableGen 全量改动 + `VBF16*` 从 `AllFloatVectors` 拆出，属下一轮的大改）。
**已知边界（记债）**：同一函数里**混用 f16 与 bf16 向量算术**会判错（需要真 per-opcode 表）；今天 317 格里没有这种格。

**结果（逐格，oracle）**：
| 格 | 改前 | 改后 |
|---|---|---|
| `mathdev_raisa_fadd_rn_bf16` | byte-diff | **asm+obj 逐字节一致** |
| `mathdev_raisa_fmax_bf16` | byte-diff | **asm+obj 逐字节一致** |
| `mathdev_raisa_fadd_{rd,ru,rz}_bf16` | byte-diff | 仍 byte-diff，**但只剩 x12/x13 交换**（vtype 行已一致：`ta, mu, bf16`） |

**⇒ 新事实：物理寄存器 tie-break 类不是 6 格而是 9 格。** `fadd_{rd,ru,rz}_{f,f16,bf16}` 九个的 `.s` 差异**全部**是
`li a2,256 / ld a3,16(a0)`（厂商）vs `li a3,256 / ld a2,16(a0)`（我方）及其带出的 `set_mem_alloc`/`vle16.v v9,(aX)` 操作数 ——
即第 ③ 项（`implicit $exec` / 贪心 vreg tie-break）的收益是 **9 格**（外加 `fadd_ru_bf16_object` 同一根因）。
⇒ 第 ③ 项做完后 `fadd_*` 家族只差 `rn` 之外的三个格式各 3 个 = 本轮 0009 之后 317 格账面上仍有 9 格挂在它上面。

**门槛（同一命令、同一口径）**：
- musl A/B：`python3 wt-raisa-be/tools/musl_ab.py --a wt-raisa-be/ab_artifacts/clang-pre-simt --b wt-raisa-be/build/bin/clang-21 --jobs 2`
  → **46/56 逐字节一致 + 10 个双方同样失败 = 56/56 结果一致**，0 real differences（与第 1 轮数字相同）。
  另做了一次"改前 clang vs 改后 clang"的 A/B（`wt-raisa-be/logs/r2/clang-21.pre-r2` → **56/56 一致**）⇒ 本补丁对 musl 产物**零影响**。
- 317 格 parity（`corpus_parity.py collect --tag ours7` → `round1_attrib.py --ours parity/ours7.json --out parity/round7_bf16.tsv`）：
  **byte-identical 136 → 138**、OURS-GAP 77（不变）、byte-diff 37 → 35、vendor-fail-too 36、vendor-fail-same-diag 30、ours-extra-ok 1；
  逐行 diff **恰好 2 行变化**（上面两格 `byte-diff → byte-identical`），`.c` 侧 **0 gap**（197 个 `c` 格无一 gap）。

**复现**：
```bash
python3 wt-raisa-be/tools/raisa_oracle.py --cc wt-raisa-be/build/bin/clang-21 \
  --cells mathdev_raisa_fadd_rd_bf16,mathdev_raisa_fadd_rn_bf16,mathdev_raisa_fadd_ru_bf16,mathdev_raisa_fadd_rz_bf16,mathdev_raisa_fmax_bf16 --mir
```


## RAISA/SIMT 第 2 轮：BF16 vtype bit 8 完成（patch `0009`，2026-10-16 23:0x）
**补丁**：`wt-raisa-be/patches/0009-raisa-bf16-vtype-format-bit.patch`（2 文件/228 行：`RISCVInsertVSETVLI.cpp` + `TX8TransformSIMTToSIMD.cpp`）。
**MIR 证据**：`raisa_oracle.py --mir` ⇒ 厂商与我方**现在都是 `PseudoVSETVLIX0 killed $x0, 335`**。
**口径更正（parent 接受）**：先前"厂商 335 vs 我方 207"是**在别的 pass 上读的**；在 `riscv-insert-vsetvli` 上我方**本来就是 79**（`e16,mf2,ta,mu`）⇒ 唯一差异就是 **bit 8**（`335 = 79 | 0x100`）。

**逐格**：`fadd_rn_bf16`、`fmax_bf16` → **IDENTICAL（4 → 6 格逐字节）**。
**门槛**：musl **56/56 结果一致**（另跑"改前 clang vs 改后 clang" = 56/56 ⇒ 本补丁对 musl 产物**零影响**）；
317 格：逐字节一致 **136 → 138**、OURS-GAP 77（不变）、byte-diff 37 → 35、**逐行 diff 恰好 2 行**、`.c` 侧 0 gap。

**③ 的规模被更正：不是 6 格而是 9–10 格**：`fadd_{rd,ru,rz}_{f,f16,bf16}`（九个）+ `fadd_ru_bf16_object` 的 `.s` 差异**全部**是同一次交换 ——
`li a2,256 / ld a3,16(a0)`（厂商）vs `li a3,256 / ld a2,16(a0)`（我方）。⇒ **建议把 ③ 提到 ② 之前/并行**（确定性收益 vs ② 的"先重建映射再逐格收敛"）。

**根因（比预期深）**：厂商 bf16 算术用**不同伪指令** `PseudoVFADD_BFloat16_VV_MF2_E16`（厂商 `libLLVMRISCVDesc.a` 里共 **876 个 `_BFloat16_` 名**）；
其 `DemandedFields` 多一字节 = **bit 8 相等性检查**，由 `RISCV::getCTPseudoInfoByBF16/ByHalf`（`RISCVBaseInfo.cpp.o` 里**两张 540 项二分表**）按 opcode 决定，前提 `hasSEWOp && SEW 操作数 == 4`。
上游同源做法 = `AltFmtType` TSFlags + `_ALT` 伪指令（PR #161158/#170612），**rc1 基线两者都没有**。

**已记的债（`0009` 的取舍）**：`VSETVLIInfo`/`DemandedFields`/`areCompatibleVTYPEs`/`encodeVTYPE(第5参)` 按厂商语义**真实现**；
但 per-opcode 表用**函数属性 `"tx8-simt-bf16-fmt"`**（由 `simt2simd` 在产出 bf16 向量算术时打上）近似 —— 因为在 `RISCVInsertVSETVLI` 处**元素类型不可恢复**（e16 的 f16/bf16 共用 `VR`/`VRM2`，`MRI.getType()` 只给寄存器类派生的 LLT）。
⇒ **同函数内混用 f16/bf16 算术会判错**（今天无此格）；真解 = 重建厂商 bf16 伪指令集 + `AltFmtType`（TableGen 全量改，另有 `VBF16*` 从 `AllFloatVectors` 拆出的风险）。

## 【更正】③ 寄存器差异的根因 = **pre-RA machine scheduler 的优先级**，**不是** tie-break，**也不是** `implicit $exec`（2026-10-16，A 线 pass 二分，parent 接受）
**证据**（`mathdev_raisa_fadd_rd_f`，产物在 `wt-raisa-be/logs/r2/`）：
1. `-print-before=machine-scheduler`：**厂商与我方的指令顺序/操作数完全相同**（只差 (a) 我方 vreg 号整体 +1、(b) fork 指令名 `SET_MEM_ALLOC`/`VSET_XFRM`/`VSET_SIMT` vs `XTsm_*`、`$ir.N` 调试名）⇒ **不是 ISel 顺序、也不是寄存器分配器的 tie-break**（本节上方"第 1 轮"里那句 tie-break/`$exec` 的说法**作废**）。
2. `-print-before=greedy`（= 调度器输出）：**顺序不同** —— 厂商把 `ADDI $x0,512`（`set_mem_alloc` 的量）提到第 2 槽（`LD 8` 与 `LD 16` 之间），我方压在它的使用者前（第 4 槽）。**整整 3 个 bf16 格 + 6 个 f 格的全部差异就是这一处。**
3. **`implicit $exec` 不可能是原因**：两边 pre-RA 调度的输入/输出里**都还没有 `$exec` 的写者** —— `VSET_SIMT`/`PseudoVSETVLIX0` 是**第一次 greedy 之后**才插入的。
4. **机制候选（有据未定案）**：我方 `RISCVSchedTX82.td` 的向量类**全部 key 在 fork 名 `XTsm_*`**（如 `InstRW<[…], (instrs XTsm_VLE32_V)>`），而 pre-RA 调度器看到的是**上游伪指令** `PseudoVLE32_V_M1`，其基指令是上游 `VLE32_V` ⇒ `RISCVVInversePseudosTable::getBaseInfo` 拿不到 TX82 的类、回落到静态基类（即 `TX8_SCHEDMODEL.md` 登记的"LMUL 重定向残差"）。厂商模型用上游基名 ⇒ 其伪指令能拿到 TX82 延迟。
   ⇒ **修复方向**：把 `InstRW` 目标从 `(instrs XTsm_VLE32_V)` 扩成 **`(instrs XTsm_VLE32_V, VLE32_V)`**（同时挂上游基名），**不要**新增 `EXEC` 物理寄存器。代价：TableGen 全量重生成 + 重编 20~40 个 TU（-j2 下约 20–40 min），风险在门槛。**验收**：3 个 `fadd_*_bf16` 或任意 3 个 `fadd_{rd,ru,rz}_{f,f16}` 转 IDENTICAL + musl 56/56 + 全量 oracle 不翻红。

**顺带（共享族的新事实）**：`alloc_l0ashared_base` 的厂商 `.s` 把共享访问编成 `lui a1,262144` + 地址 splat 进向量 + **`vsuxei64.sync.v ..., (zero), v..`（零基址 gather/scatter）**，且出现 **`vsetvli zero, a1, e32, m2, ta, mu, bf16`（e32 也带 bit 8）** ⇒ 共享族的后端形态比"地址常量"复杂，② 做完 IR 侧后该族**仍需单独的 ISel 尾巴**；且 bf16 的 bit 8 不止 e16（印证 `getCTPseudoInfoByBF16/ByHalf` 是按 opcode 而非仅按 SEW）。

## R2 线成果：`rpu-lower-shared` 重建 + pass 契约模块级验证（2026-10-16 深夜，parent 接受）
**补丁**：`0054-raisa-lower-shared.patch`（349 行/4 文件：新 `TX8LowerSharedPass.cpp`，ModulePass `TX8LowerSharedLegacy`，pass 名 **`rpu-lower-shared`**，RAISA-gated；
语义 = 基址 `as3 L0A 0x40000000` / `as4 L0W 0x40100000` / `as2 L1 0x101000000`、`offset_i = align_up(offset_{i-1}+size_{i-1}, align_i)`、
常量表达式**自底向上在使用者之前物化**（这是复现厂商编号的关键）、`inttoptr` 宽度取**目标地址空间**的指针宽度（as3/as4 是 `i32`、as2 是 `i64`）；定义 `-riscv-bank-aware`；`RISCVTargetMachine.cpp` 两处 additive 接线，插在 `simt2simd` 之后 = 厂商的 `early-cse → rpu-lower-shared` 槽）
+ `0055-asmparser-rpu-kernel-keyword.patch`（10 行，`LLLexer.cpp` 加 `KEYWORD(rpu_kernel)`）。

**⚠️ 发现 `0045` 里的一处死代码（要折进 0045）**：`0045` 给 `LLParser.cpp` 加了 `kw_rpu_kernel` case，但 **`LLToken.h` 有 token、`LLLexer.cpp` 从未产出该 token** ⇒ 任何带 `define dso_local rpu_kernel …` 的**文本 IR** 都会 `error: expected type`。纯 additive、不影响 codegen，但影响 `.ll` 输入（黄金 IR 测试台就是这么撞上的）。**已用 `0055` 补齐**。

**pass 契约验证（`tools/validate_lower_shared.py`）**：把**厂商自己的 pre-pass module**（28 格，verbatim 存档）当 IR 喂给我方 clang（`-O0`），再与厂商 post-pass module 比：
- **20/28 `EXACT-IR-MATCH`** —— 含**全部 19 个厂商 OK 格** + `mem_l0ashared_u8_uniform`（模块级一致：地址、指令顺序、**值编号**、globals 删除都同）；
- 5 格差异**仅因厂商那次运行掷到了另一个布局**（6 个多变量格的 8 次重复性检查确认"我方布局是厂商自己也会产出的之一"，且我方步长规则是确定的那条）；
- 3 格无法比较：**厂商自己的 pre-pass IR 就是非法的**（`mem_{l0a,l0w,l1}shared_u16_uniform`：`vector_insert parameters must have the same element type`，厂商 `simt2simd` 的 u16 bug，厂商侧 rc=70）。

**未声明**：任何共享格的**整格 `.o` 逐字节** —— 在 `simt2simd` 仍把这些内建留在 ISel 里的情况下无法测；**已证明的是 pass 自身契约（post-pass module）逐字一致**。

**崩溃普查（106 个 asm 格，`tools/crash_census.py`）**：`CT-LEGALIZE` 82 → **86**（那 4 个 `*_bank_on` 从"选项解析失败"变成"跑到 ISel 才崩"），`MISSING-OPT` 4 → **0**，`OK` 20 → 20。
⇒ **整个崩溃家族（86 格）都卡在 A 线的 `simt2simd`**（这些格都带 `read.reg.tid.x`/`raisa.*` 进 ISel，而厂商正是在 `simt2simd` 消费它们）。**A 线是唯一关键路径。**

**另两条厂商侧事实**：① `-riscv-bank-aware` **由 `simt2simd` 消费**（`*_base.rc` ≡ `*_bank_on.rc` 逐字节同源；pre-`simt2simd` IR 无差异、pre-`rpu-lower-shared` IR 有差异）⇒ bank 地址必须由 A 线产出（bank 步长 128 KiB，base 128 B）；② **厂商自己的 parity 归类对共享内存格是抖动的**（同一编译器两次运行，3 格在 `vendor-fail-too`/`vendor-fail-same-diag` 之间跳）⇒ parity 表要加注。

**门槛**：musl **56/56 结果一致**；317 格 `byte-identical 136 / OURS-GAP 77 / byte-diff 37 / .c 零缺口`（与基线一致；只有**厂商侧**那 36/30 拆分成 33/33，那是厂商 ASLR 抖动，不是我们）。

## 【parent 操作教训】给"已收工"的 agent 发消息会**唤醒**它 ⇒ 先查状态再派工（2026-10-16 23:30 事故）
**事故**：R2 线（`100c8e04`）已声明"final for this session、`config-instr-opt` 未开始"；我随后给它发了"计费时段纪律"消息 —— **该消息把它的会话重新唤醒**，于是它又在 `wt-raisa-r2` 里实现 `config-instr-opt`（写 `TX8OptimizeConfigInstrs.cpp`、改 `RISCVTargetMachine.cpp`、跑构建），而**同一时刻我已完成派工，把这条活交给了新线 `wt-config`（`ae268c1e`）**。结果：**两条线做同一件事 + 两处并行构建**（可用内存掉到 4.2 GB），靠新线自己读文件时间戳才发现。

**规矩**：
1. **派工前先用 `list_agents` 确认目标线的状态**；"inactive"不等于"不会被我唤醒"。
2. **给已收工的线发任何消息前，先想清楚"我是要它继续干"还是"只是通知"** —— 后者会在它那边开启新回合、消耗预算并可能与别的线撞车。通知类信息应写进**共享文档**（`COMPILER_STATUS.md` 等），而不是逐线发消息。
3. 一条活**同时只能有一个所有者**；改派时先发**停手指令**（要求：等当前构建结束、不启动新构建、草稿标 WIP、列证据路径），再通知新所有者。
4. 并发构建的检测：`wt-*` 树里的文件 mtime + `pgrep -x ninja/cc1plus`；发现两处在构建 ⇒ 立即让一方收手。

## R2 线最终状态（2026-10-16 23:40，parent 记账）
**可交付补丁**：`0054-raisa-lower-shared.patch`（= R2 的 `1001`，**已按其修正后的干净版刷新**；它先前一版把 config 接线漏进共享文件，会引用一个自己不含的 `.cpp`）、`0055-asmparser-rpu-kernel-keyword.patch`（= `1002`）。
**WIP（**不合**）**：`wt-raisa-r2/patches/1900-WIP-config-instr-opt-DO-NOT-MERGE.patch`（274 行；只是 7 个 `ne_set_*` 的支配域去重，**缺**厂商 20 项 `ConfigIntrs[]`/`ConfigUseGroups[0..14]` 与 `config.opt.preheader` 循环提升）——已改派 `wt-config`（D 线）。
**R2 用 17 个探针验出的 config pass 语义（可复用真值）**：槽位 = 内建 ID；**同槽位不同值 ⇒ 失效**（不是按 (id,操作数) 建键）；不同槽位 / 非 config 指令 / `inst.barrier` **透明**；**删后留前**；可见性**支配域受限**。
**关键口径更正（省 D 线一轮）**：`config_repeat_device` **不需要 config 工作**（厂商 before-IR 只有一条 `ne_set_mkn`）却仍有差异：`gemm a1,a2,a0`（厂商）vs `gemm a0,a1,a2`（我方）⇒ 这是**物理寄存器/发射顺序类**（与 6 个 `fadd_*` 格同族），由 **C 线 schedmodel 上游基名映射**负责 ⇒ **config 线不应以"5 格逐字节"为验收目标**。

## 【关键】7 个 byte-diff 格的真正根因 = `simt2simd` 的**签名打包**（不是 CT/ISel/调度）（2026-10-16 23:45，R2 线证据，parent 采纳）
**证据链**：
1. `imm_l1_flush_{31,32}_object` 的"操作数顺序差异"其实是**我方根本没有那条 load**：厂商 `ld a1,0(a0); ld a0,8(a0); l1_flush a1,31,a0`（操作数是**从打包参数块里 load 出来的**）vs 我方 `l1_flush a0,31,a1`（直接用进来的 GPR）。
   厂商 `-print-after=simt2simd` 的 IR 显示：**签名被改写成 `(ptr %0)` 并插入 `getelementptr i8, ptr %0, i32 <offset>` + `load`**；而**这个 body 是纯标量调用**（没有向量化）⇒ **厂商的签名打包与"是否向量化"无关，是无条件的**；我方 pass 把两者绑在一个 all-or-nothing 闸门里 ⇒ 标量 body **整体不动**。
2. **决定性实验**：把**厂商自己的 post-`simt2simd` IR** 喂给**我方** clang 编译（同 triple/cpu/features/`-O2`）⇒ 归一化 `.s` 与厂商**逐字节一致**，覆盖 `imm_l1_flush_{31,32}_object`、`config_repeat_device`、`config_overwrite_device`（含 `gemm a1,a2,a0` 形态）。⇒ **我方的 ISel/CT/寄存器分配/调度本来就能精确复现厂商；差异全在 IR 上游。**
3. **量化杠杆**：当前 `byte-diff` 的非 mathdev 格 **7 个**（`config_{across_compute,mixed,overwrite,repeat,with_barrier}_device` + `imm_l1_flush_{31,32}_object`）在"喂厂商 post-simt2simd IR"下**全部 IDENTICAL** ⇒ 只要把打包做成无条件，**parity 的 byte-identical 136 → 143**（超过我给的 ≥138 目标）。剩下 12 个 mathdev 的差异仍只是那两个已知残余（BF16 vtype、寄存器 tie-break）。
4. ⇒ **`tx8-legalize-ct` 在本语料上无可观测残余**（`ne_set_mkn` 去重是 IR 的 `config-instr-opt`；`implicit $mem_alloc` 的效果已折进 patch `0034` 的 `TX8InsertXFRM`）⇒ **不投线做 CT**。
5. **决定**：**方案 A**（A 线把签名重写移到 dry-run/向量化闸门**之前**，即**无条件打包**）——忠实重建、改动小、与厂商 IR 可逐字比对；**不采用方案 B**（在 `LowerFormalArguments` 里做等价解包：那是后端里不可观测的设计选择，且会与 A 形成两套机制）。
   参考证据（verbatim）：`wt-raisa-r2/recon/legalize_ct/*.postsimt.ll`、`*.simt2simd.dump`；偏移/类型规则 = **按累计 alloc size**（`ptr`=8、`float`=4、`i32`=4；见 `recon/simt2simd/abi_float.after.ir`）。
6. **连带修正 D 线的任务前提**：既然"喂 post-`simt2simd` IR ⇒ 5 个 config 格 asm 已一致"，则**这 5 格可能根本不需要 `config-instr-opt`** ⇒ D 线**先用同一实验验证**再决定是否实现该 pass。

## 【parent 裁决】`config-instr-opt` **必须做** —— R2 的"5 格不需要这条 pass"结论**方向错误**（2026-10-16 23:55，parent 亲自复验）
**决定性证据（我跑的）**：
1. **厂商 pass 顺序 = `config-instr-opt` 在 `simt2simd` **之前****：
   `clang -cc1 … -mllvm -print-before=config-instr-opt -mllvm -print-before=simt2simd` ⇒ dump 头顺序为
   `*** IR Dump Before TX8 Optimize Configuration Instructions (config-instr-opt) ***` → 然后才 `*** … (simt2simd) ***`。
   ⇒ **`-print-before=simt2simd` 拿到的 module 已经是去过重的** —— R2 把这份 IR 喂给我方得到"逐字节一致"，只证明**下游后端已经对了**，**不能**证明这 5 格不需要该 pass。**R2 那个实验的前提被污染了。**
2. **端到端 asm 计数（`config_across_compute_device`，同一命令行）**：厂商 **1** 次 `ne_set_mkn`，我方 **2** 次。
   厂商：`ne_set_mkn a5,a4,a3` + `gemm a1,a2,a0`（两条 gemm 共用前面那条 config）；我方：两条 config 各配一条 gemm。
   ⇒ **差异只能由 `config-instr-opt` 解释**（D 线对 5 格的 dump 统计：`across_compute`/`mixed`/`with_barrier` 各删 1 条；`overwrite`/`repeat` 为 no-op，后者上游 CSE 已去重）。
3. R2 消息内部本就不自洽：表格声称"7 格全 IDENTICAL"，但同一消息枚举的实测结果只列了 **4 格**（`imm_l1_flush_{31,32}_object` + `config_{repeat,overwrite}_device`）—— 恰好是**该 pass 为 no-op 的那 2 个 config 格 + 2 个 l1_flush 格**，与 D 的解释完全吻合。

**口径纠正**：
- **3 格（`across_compute`/`mixed`/`with_barrier`）必须靠 `config-instr-opt`**（D 线）——该线继续，验收 = 契约（黄金 IR）+ 逐格推进 + 门槛；
- **A 线的"无条件签名打包"依然要做**，但杠杆从"7 格"修正为 **2–4 格**：`imm_l1_flush_{31,32}_object`（我方完全没有 load、厂商有 `ld a1,0(a0); ld a0,8(a0)`），以及若干格里 `gemm a1,a2,a0` vs `gemm a0,a1,a2` 的参数形态；
- **集成顺序（给 D 的接线）**：我方管线里 `config-instr-opt` 必须插在 **`simt2simd` 之前**（与厂商一致）；
- **R2 的 3002（`LowerFormalArguments` 解包）正式取消**：诊断本身错了（我方 `config_*_device` 的 asm 里**没有** `ld`，"ld a0,0(a0)" 是误读）⇒ `RISCVISelLowering.cpp` 保持无人占用。
- **`tx8-legalize-ct` 仍然没有可观测残余的结论不变**（`ne_set_mkn` 去重属 IR 的 config pass，不是 CT）。

## R3 侦察结论：**影响 0 格 0 门槛**，且**不是重建、只是接线**（2026-10-16，E 线，parent 采信）
**指向修正（证据）**：**没有 `getRAISARuntime` 这个符号**（厂商二进制与 `ar_probe` 里 0 命中）；真实符号是
`clang::CodeGen::CodeGenModule::createRAISARuntime()` 与 `clang::CodeGen::CreateTSRAISARuntime(...)`，且在 **`libclangCodeGen.a`**（`CGRAISATS.cpp.o` / `CodeGenModule.cpp.o`），**不是** `libLLVMRISCVCodeGen.a`。
**实现早已在我们树里**：`clang/lib/CodeGen/CGRAISATS.cpp`（1329 行、`CreateTSRAISARuntime` 在 1072 行、厂商每个方法都在）、`CGRAISARuntime.{h,cpp}`、AST 节点 `RAISAKernelCallExpr`、`Raisa.h`、16 个 `__clang_raisa_*.h`（runtime wrapper 与厂商安装版**逐字节相同**）。
**缺的只是 4 处连接**：① `CodeGenModule` 无 runtime 成员/`createRAISARuntime()`/`getRAISARuntime()` ⇒ `CreateTSRAISARuntime` **零调用者**；② `CodeGenFunction` 不派发 `RAISAKernelCallExpr`；③ **Sema/Parse 从不构造该 AST 节点** ⇒ host launch 语法不可达；④ `getMangledNameImpl` 用我们的 `RAISAIsDevice` 短路代替 `getDeviceSideName`。
**影响面**：114/114 `.rc` 全是设备侧、`.c`/musl 也不用 `__attribute__((global))`/`__raisa` ⇒ **parity、musl A/B、崩溃家族目标都与 R3 无关**；只影响**host 侧产品路径**。
**差分只有负向**：host 模式下厂商给干净的 `Can't find declaration for txLaunchKernel`，**我方 SIGABRT**（`assert(CUDARuntime != nullptr)`）；且**正向黄金测试当前不可构造**（launch 语法写不出来 + 手写 `txLaunchKernel` 后**厂商自己也崩**：`emitDeviceStubBody` 里 `KernelHandles` 空导致 `UndefValue::get` 空引用）。
⇒ 登记为 **"结构性缺口 + 仅负向差分"**；**优先级排在 ② 之下**；将来接手时最便宜的第一步是**只做 ①**（把我方 SIGABRT 变成厂商的诊断）。

## 【最大后端杠杆】46 格 `OURS-REJECTED-IR`：后端对 fork 内建**零 ISel 连接**（E 线发现，parent **已独立核验**）
用 `postsimt2simd_ab.py` 对 77 个 gap 格喂厂商 post-`simt2simd` IR 后分类：**46 `OURS-REJECTED-IR`** / 16 DIFF（BF16 vtype、寄存器 tie-break）/ **15 已 IDENTICAL**（IR 一对，后端就对）。
**parent 核验（真值）**：指令存在（`RISCVInstrInfoXTsm82s.td`：`XTsm_VCOS_V`@1259、`XTsm_VSIN_V`@3738、`XTsm_VLUXEI64_SYNC_V`@2511、`XTsm_VSUXEI64_SYNC_V`@4128）；内建声明存在（`IntrinsicsRISCVXTsm.td`：`vpow2/vlog2/vsin/vcos`@99-102、`vluxei_sync/vsuxei_sync`@120-121）；
**但 `lib/Target/RISCV` 里对 `int_riscv_{vluxei,vsuxei,vcos,vsin}` 的引用 = 0 个文件**（对照组：`int_riscv_vfadd`、`int_riscv_set_mem_alloc` 各 1 处已接好）⇒ **缺的只是 `Pat`/lowering**。
**构型**：`mem_l1_size_*`/`mem_l1shared_*`/`mem_l0*shared_*` → `Cannot select: intrinsic %llvm.riscv.vluxei.sync`（15 格）；`mathdev_raisa_{sin,cos,log2,exp2}_*` → `vsin/vcos/vlog2/vpow2`；`mathdev_raisa_reduce_*` → `vor.vs`。
**厂商黄金真值已捕获**：`mem_l1_size_1` 的 `-print-after=finalize-isel` MIR：`PseudoVSUXEI64_SYNC_V_M2_M1 killed %2:vr, %7:gpr, %6:vrm2, 128, 5` / `early-clobber %8:vr = PseudoVLUXEI64_SYNC_V_M2_M1 %noreg, %7:gpr, %6:vrm2, 128, 5, 2`。
⇒ **已派给 E 线**（与 A 的 IR 映射互补且并行：A 管"内建 → 向量 IR"，E 管"向量内建 → 指令"）。

## 【parent 采纳的证伪】调度模型**不是** 9 格差异的原因 —— 真因是**缺 `vreg-align` pass**（2026-10-17 00:10，C 线证据）
**被证伪的两条（我先前批准的修复方向作废）**：
1. **`(instrs XTsm_VLE32_V, VLE32_V)` 是可证明的空操作**：用 `llvm-tblgen` 对同一份 `.td` 生成两遍 `RISCVGen{Subtarget,Instr}Info.inc`，`PseudoVLE32_V_M1` 的调度类**连类号都没变（SC=38）**、字段与厂商二进制里该伪指令（SC=39）**逐字段相同**；只有 `VLE32_V` 自己换了个内容相同的类。⇒ 该改动不可能翻转任何一格。（证据 `wt-raisa-c/logs/evidence/tblgen_noop_proof.txt`）
2. **README 那条机制的两个前提都不成立**：`RISCVVInversePseudosTable::getBaseInfo` 在 **codegen 调度路径上没有调用点**（本树只有 `RISCVISelLowering.cpp:21679` 的 lookupMaskedIntrinsic 与 `MCA/RISCV*` 的 llvm-mca）；pre-RA 调度器读 `MCInstrDesc::SchedClass`，由伪指令**自己的 `Sched<>`** 写出 ⇒ **我们的伪指令没有回落**；厂商自己的 pre-RA MIR 也是 `PseudoVLE32_V_M1`。
   逐指令对拍（厂商 `dump_schedmodel` oracle vs 我方 `.inc`）：调度区全部指令 **mismatches = 0**；直接读**厂商二进制**的 `MCSchedModel`：`IssueWidth=2 MicroOpBufferSize=0 LoadLatency=2 HighLatency=10 MispredictPenalty=5 PostRAScheduler=0 CompleteModel=0 EnableIntervals=0` ⇒ **与我方完全一致**（旧脚本漏打的 `EnableIntervals` 也一致）。唯一指令标志差异 = 厂商 `SET_MEM_ALLOC` 多 `HasPostISelHook`（不参与调度）。
   ⇒ **调度模型这条线正式关闭（已验证一致，不是缺口）。**

**真正的因果链（已证实）**：pre-RA 顺序 → **RAGreedy 局部区间优先级**（`DefaultPriorityAdvisor`：`Prio = beginIndex 到函数末尾的距离` ⇒ **定义最早者拿低号寄存器**）→ ADDI 拿 a2 还是 a3 → 每格 7 行的 `.s`/`.o` diff。
- 厂商 pre-RA：`LD8, ADDI512, LD16, LD0` ⇒ ADDI 先 ⇒ `li a2,512` + `ld a3,16(a0)`；我方：`LD8, LD16, LD0, ADDI512` ⇒ 相反。
- **注意：最终指令顺序两边已经一样**（post-RA 调度器已归位，"第 2 槽 vs 第 4 槽"只对 pre-RA 阶段成立）；残留差异**纯粹是 a2/a3 染色**。旁证：我方加 `-mllvm -greedy-reverse-local-assignment=true` ⇒ ADDI 立刻从 a3 变 a1（顺序未变、染色变）⇒ 染色完全由该优先级规则支配。

**新线索（差异在别处）**：`-print-after-all` 逐 pass 对比，厂商机器级**比我方多 3 个 pass**：
1. **`TX8 Registers Alignment (vreg-align)`：在 `twoaddressinstruction` 之后、`machine-scheduler` **之前**** ← **最强嫌疑**（唯一能"让 MIR 文本看似相同却改变调度器状态/染色"的候选）；
2. `tx8-legalize-ct`（`finalize-isel` 之后）、`legalize-exec`（`riscv-copyelim` 之后）；
3. 厂商用 `early-tailduplication`，我方在 `branch-folder` 之后才 `tailduplication`。
（厂商私有 `TX8PreRASchedStrategy/TX8PostRASchedStrategy` 存在，但 `--riscv-tx8-sched=0` 默认 ⇒ 用通用 `GenericScheduler`；通用调度器源码与厂商树 `diff -rq` 零差异，普通 RISC-V 代码两边 `-print-after=machine-scheduler` **逐字节相同** ⇒ 不是通用调度器问题。）
**伪代码已在手**：`tools/re/out/TX8RegisterAlignment/decompiled.c`（**2020 行 / 69 函数**）+ `callgraph.txt`；对象 `docs/compiler/fork-source/backend-objects/TX8RegisterAlignment.cpp.o`；符号 `InitializeTX8RegisterAlignmentPassFlag` / `TX8RegisterAlignment2IDE`。

## 【E 线重定性】fork 内建的 ISel 连接**不是加 `Pat`**，而是缺**整层伪指令**；`vslh` 才是最大杠杆（2026-10-17 00:25，parent 采权）
**调用点清单**（厂商 post-`simt2simd` IR，100 个 module，`wt-raisa-r2/recon/isel_connect/intrinsic_inventory.tsv`）：
| 内建 | 调用点 | module | 我方已连? |
|---|---|---|---|
| **`vslh`**（i32 + 向量形） | **76** | **22** | 否 |
| `vluxei.sync`（i32/f32/f16/bf16） | 47 | 21 | 否 |
| `vsuxei.sync` | 29 | 19 | 否 |
| `mov128.s.v` | 13 | 4 | 否 |
| `and.s` / `raisa.divergent` / `set.exec` | 9 / 9 / 9 | 3 / 3 / 3 | 否 |
| `vor.vs` | 7 | 7 | 否 |
| `vmsne`(nxv2i32 mask) / `raisa.get.kernel.entry.exec` / `nor.s` | 7 / 4 / 4 | 1 / 4 / 4 | 否 |
| `vcos`/`vsin`/`vlog2`/`vpow2` | 每 mathdev 格 | — | 否 |
| `set.mem.alloc` / `ne.set.mkn` / `gemm` | 178 / 6 / 6 | 93 / 5 / 5 | **是**（0030 先例） |
⇒ **46 格是下限不是全集**：编译器**在第一个选不出的内建上就 abort**，所以每格可能还压着别的内建。**首个族应是 `vslh`**（22 个 module 直接受它 gate，93/100 module 里出现）。

**为什么 `Pat` 不够（三件套）**：我方 fork RVV 指令是**没有 VL/VTYPE 的普通指令**（`XTsm_VSLH_VX : (outs VR:$vd), (ins VR:$vs2, GPR:$rs1, VMaskOp:$vm)`），而厂商 MIR 用的是**上游风格伪指令带 (VL,VTYPE)**（`PseudoVSLH_VX_M2 $noreg, %4:vr, %5:gpr, 128, 5`）。
厂商 `mem_l1_size_1` 的 asm 里那段**计算出来的 vsetvli 序列**（`vsetsimt 1` / `vsetvli a2,zero,e32,m1,ta,mu` / `vmv.v.x` / `vid.v` / `vsetvli zero,a2,e32,m2,ta,mu,bf16` / `vslh.vx` / `vsuxei64.sync.v` …）**只有 `RISCVInsertVSETVLI` 从伪指令的 VL/VTYPE 信息才能产出**。⇒ 每族需要三件：**(a)** 厂商 LMUL 后缀的伪指令定义 + VL/VTYPE 操作数；**(b)** 伪指令→真实指令 的 `RISCVInstrInfo::getRVVMCOpcode` 入口；**(c)** 内建→ISel lowering（从内建的 `i64` 参数提取 VL）。
**parent 核验**：我方 `RISCVInstrInfoXTsm82s.td` 里 **Pseudo 定义数 = 0**（证实 (a) 整层缺失）；`RISCVInstrInfo.cpp` 里 `getRVVMCOpcode` 已有 7 处命中（上游函数在，(b) 只需加表项）。

**族划分与所有权（parent 裁决）**：
- **(A) 地址/索引**（`vslh`/`vluxei.sync`/`vsuxei.sync`）+ **(C) 数学**（`vcos/vsin/vlog2/vpow2`）→ **E 线**（同一伪指令层）；**先做 `vslh` 单族试点**（最高杠杆、单次构建即可验证机制）。
- **(B) EXEC/SIMT-mask**（`set.exec`/`raisa.get.kernel.entry.exec`/`raisa.divergent`/`mov128.s.v`/`and.s`/`nor.s`/`vmsne(nxv2i32)`）→ **新第五条线**（厂商有专门的后置 pass `TX8LegalizeEXEC`，伪代码 **1162 行/53 函数**已在手）。
- **E 获得的文件授权**：`RISCVInstrInfoXTsm82s.td`、`RISCVInstrInfoVPseudos.td`、`RISCVInstrInfo.cpp`、`RISCVISelLowering.cpp`（均无人占用）。
- **共享文件纪律**：`RISCVTargetMachine.cpp` / `RISCV.h` / `CMakeLists.txt` 是 **C 线与 D 线都要动的 pass 注册文件** ⇒ 任何改动必须**additive、位置互不重叠**，并在报告里列出自己的 hunk；若需要同一处，由 parent 串行。

## 第 2 轮 A 线（simt2simd IR 侧）结果：patch 0009 / 0010 / 0011 / 0012（2026-10-16 深夜，空闲时段）
（vsetvli 掩码策略白名单那段已被 parent 记为 `0011`；本节只补"结果 + 新解出的厂商规则"，不重复上文。）

**三个补丁**（都用 `patch -p1` 对 pre-image 回放验证过，逐字节复现 `wt-raisa-be/src`）：
- `0010-raisa-simt2simd-read-reg-family.patch`：`TX8TransformSIMTToSIMD.cpp` 的 `read.reg.*` 映射（见下）。
- `0011-raisa-simt-mask-policy-whitelist.patch`：`RISCVInsertVSETVLI.cpp` 的 `IsInstrDemandMaskPolicyUnderSIMT` 白名单 + 从**基指令**读 DestEEW。
- `0012-raisa-simt2simd-unconditional-signature-pack.patch`：签名打包**无条件化**。

**0011 解出的厂商规则（这条是从反编译里"未复现的白名单"变成事实的关键）**：用厂商自带 `llvm-mc -show-inst` 把反编译里的 8 个数字对上厂商自己的 opcode 枚举：
`0x4206 VFMERGE_VFM`、`0x4215 VFMV_V_F`、`0x433d VMERGE_VIM`、`0x433e VMERGE_VVM`、`0x433f VMERGE_VXM`、`0x4382 VMV_V_I`、`0x4383 VMV_V_V`、`0x4384 VMV_V_X`
= **merge/move/splat 家族**（passthru 常为 `undef` ⇒ 上游视掩码策略为 agnostic，SIMT 路径要求 `mu`）。并且 **DestEEW 必须读"基指令"**（`getRVVMCOpcode`）：对这批指令伪指令与基指令的 DestEEW 不一致，这正是 `id_ctaid_x` 出 `ta,ma`(208) 而厂商出 `ta,mu`(80) 的根因。改后 7 个 `id_*` 格的 vtype 行与厂商逐字相同。

**0010 的映射表（厂商 `after.ir` 逐字，`recon/simt2simd/*.after.ir`）**：
`read.reg.tid.x`（作值）→ `vid.nxv2i32.i64(undef,128)`；只作下标 → 丢弃；`ntid.{y,z}` → 也是 `vid`（厂商自己如此，非笔误）；`ctaid.{x,y,z}` → `__get_pid(i32 0/1/2)`；`nctaid.{x,y,z}` → `__grid_dim_{x,y,z}()`；`ntid.x` → `__block_dim_x()`；新声明带 `"no_vector"` 属性。标量值在 **store 处 splat**（`vmv.v.x(undef,%v,128)`，插在 `set.mem.alloc` 之前）。`tid.{y,z}`/`w` 维厂商也失败 ⇒ 保持不支持。

**0012 的坑（纪律）**：签名打包提前到无条件位置后，两个 all-or-nothing 校验循环会看到**自己造的拆包 load**（非 tid-indexed）⇒ 向量化全被拒、已绿的格集体转 OURS-GAP（本轮实测到并已修）。修法：`isPackedParamLoad()` 在两条循环里跳过 `load <ty>, ptr (gep i8, ptr %block, <const>)`。**任何"提前改写"都会进入后续校验视野。**

**317 格 parity（`collect --tag ours10` → `parity/round10_pack.tsv`）**：byte-identical **136 → 145**（+9）、OURS-GAP **77 → 67**（−10）、byte-diff 37 → 38、厂商侧 66、ours-extra 1；**`.c` 侧 0 gap**（67 个 gap 全是 raisa）；逐行 diff **只有改善**。
新增逐字节：`{config_repeat,config_overwrite}_device`、`imm_l1_flush_{31,32}_object`、`mathdev_raisa_f{add_rn,max}_bf16`、`id_{ntid_y,ntid_z,tid_x}`；`id_{ctaid,nctaid}_{x,y,z}`+`id_ntid_x` 由 OURS-GAP → byte-diff。
**musl A/B**（对 `clang-pre-simt`）：**56/56 结果一致**、0 real differences。**全量 oracle**：9 格逐字节，无新 DIFF。
**剩余 7 个 `id_*` 的唯一差异** = 后 RA 调度把 `vse32.v` 排在 `ld ra` 之后（厂商在前）⇒ 与 ③ 的 schedmodel 残差同源（已归 `wt-raisa-c`）；IR/vtype/指令集已逐字对齐。

**下一族（②剩余）**：`mathdev_raisa_{sin,cos,log2,exp2,fdiv,frsqrt,reduce_*,min,max,umax,umin}` —— 我方 pass 的 `Dst` 表已写好名字，但 `BaseName` 白名单只放行 `vfadd`/`vfmax`（其余 `return false`，所以 RAISA 内建活到 ISel 撞 CT legalize）。**逐格判据**用 `spec.tsv` 给出的内建名/`Extra`/AVL 常量（例：`vcos.nxv2f32.i64` 的 `7/128`），先做到**厂商 after-IR 逐字一致**；能否转 byte-identical 取决于 `WT-RAISA-E` 线的伪指令层（`vcos/vsin/vlog2/vpow2` 在本树无 pseudo）。

## ② 第一族 + 掩码白名单 + 无条件打包落地：**parity 136 → 145、OURS-GAP 77 → 67**（2026-10-17 00:40，A 线，parent 采信并归档）
**补丁**：`0056`（= `0010` `read.reg.*` 族映射）、`0057`（= `0011` 掩码策略白名单）、`0058`（= `0012` 无条件签名打包）。
**数字（原口径）**：317 格 **byte-identical 136 → 145（+9）**、**OURS-GAP 77 → 67（−10）**、byte-diff 37 → 38、vendor-fail 36/30、ours-extra 1、**`.c` 零 gap**、**逐行 diff 只有改善**。
新增逐字节 9 格：`config_{repeat,overwrite}_device`（**我预测的连带，真的转了**）、`imm_l1_flush_{31,32}_object`、`mathdev_raisa_f{add_rn,max}_bf16`、`id_{ntid_y,ntid_z,tid_x}`；另有 7 格 OURS-GAP → byte-diff：`id_{ctaid,nctaid}_{x,y,z}` + `id_ntid_x`。
musl A/B **56/56 结果一致**（含"改前 clang vs 改后 clang"= 56/56 ⇒ 本轮含无条件打包对 musl **零影响**）；全量 oracle **9 格 asm+obj 逐字节**、无新 DIFF。

**`0057` 把"未复现的厂商 opcode 白名单"变成事实**：用厂商自带 `llvm-mc -show-inst` 把反编译里的 8 个数字对上厂商枚举 ⇒
`0x4206 VFMERGE_VFM`、`0x4215 VFMV_V_F`、`0x433d/e/f VMERGE_VIM/VVM/VXM`、`0x4382/3/4 VMV_V_I/V_V/V_X` = **merge/move/splat 家族**；
且 **DestEEW 必须从基指令读**（`getRVVMCOpcode`）—— 伪指令与基指令在这批上不一致，这正是 `id_ctaid_x` 出 `ta,ma`(208) 而厂商出 `ta,mu`(80) 的根因。改后 7 个 `id_*` 的 vtype 行与厂商逐字相同。⇒ **`0011` 里 `IsInstrDemandMaskPolicyUnderSIMT` 的"opcode 白名单未复现"这条债已还清**（先前只实现了可观测效果）。

**`0058` 的实测坑（通用教训，已转给其它线）**：把签名打包**提前到无条件位置**后，两个 all-or-nothing 校验循环会看到**自己制造的拆包 `load`** ⇒ 全部向量化被拒、已绿格集体转 OURS-GAP。修法是 `isPackedParamLoad()` 跳过。⇒ **任何"把改写提前"的动作都会进入后续校验视野，改 pass 顺序时必须自查这一点。**

**7 个 `id_*` 格的唯一剩余差异**：后 RA 调度把 `vse32.v` 排在 `ld ra` 之后（厂商在前）。**归因更正**：A 线把它写成"schedmodel 上游基名残差"，但那条假设已由 C 线**证伪**（调度模型与厂商逐字段一致、已关闭）⇒ 真因应是**缺失的 `vreg-align` pass**（C 线当前任务）。已要求 C 线把 `id_*` 加入验收集。

## 第 2 轮 A 线续：一元数学族（patch 0013）+ round 11 parity
`0013-raisa-simt2simd-unary-math-family.patch`（`TX8TransformSIMTToSIMD.cpp`，已 `patch -p1` 回放验证：0010+0012+0013 联合逐字节复现 src）。

**族规则（`recon/simt2simd/spec.tsv` 逐格判定，不是猜的）**：
- `vcos/vsin/vlog2/vpow2`：重载名**只列结果类型**（`vcos.nxv2f32.i64`）+ 常量 `7/128`（§policy 7 + AVL 128）；
- `vfrsqrt7`：同样只列结果类型，但**没有 policy 常量**（`vfrsqrt7.nxv2f32.i64:128`）；
- 二元族（`vfadd/vfmax`，以及将来的 `vmin/vmax/vminu/vmaxu/vdiv`）重载名列**结果 + 一个操作数**（`vfadd.nxv2f32.nxv2f32.i64`），`vfadd` 带 frm、`vfmax` 不带；
⇒ 名字 token 数必须按**内建的重载表**写，不能按"实参个数"推（我第一版按操作数个数生成，`vcos` 就多了个 `.nxv2f32`，厂商 IR 立刻对不上）。

**IR 判据**：15 格（cos/sin/log2/exp2/frsqrt × f/f16/bf16）的 `call`/`vle`/`vse`/`set.mem.alloc` 行与厂商 `after.ir` **逐字一致**。

**parity（`collect --tag ours11` → `parity/round11_math.tsv`）**：byte-identical **145 → 148**（+3）、OURS-GAP **67 → 64**（−3），byte-diff 38、厂商侧 66、ours-extra 1，**`.c` 侧 0 gap**，逐行 diff 只有那 3 格改善。
**musl A/B**：**56/56**，0 real differences。
**新增逐字节 3 格**：`mathdev_raisa_frsqrt_{f,f16,bf16}` —— `vfrsqrt7` 是本树**已可选中**的上游 RVV 内建，所以"IR 侧放行"就直接转 byte-identical（这也反证了其余 12 格此前卡在 IR 而非后端）。
**其余 12 格（cos/sin/log2/exp2）**：IR 已与厂商逐字一致，但本树无 `vcos/vsin/vlog2/vpow2` 伪指令 ⇒ 诊断从 `Don't know how to custom type legalize` 变为 `Cannot select: intrinsic %llvm.riscv.vcos`（仍 OURS-GAP，**不算退步**，且把 E 线要补的东西点名了）。
**下一步同族**：`fdiv`（厂商分解为 `vfrec7(7) + vfmul(7)`）、`min/max/umin/umax`（整数二元、无 policy）、`reduce_*`（`vfmv.v.f` + `vfredusum/fredmin/fredmax` + `vor.vs` 多指令序列）。

## ② 一元数学族落地（patch `0059`）：parity **145 → 148**、gap **67 → 64**（2026-10-17 01:00，A 线）
**新解出的族规则（判据 = `recon/simt2simd/spec.tsv`，非猜测）**：
- `vcos/vsin/vlog2/vpow2`：重载名**只列结果类型**（`vcos.nxv2f32.i64`）+ 常量 `7/128`；
- `vfrsqrt7`：同样只列结果类型，**但没有 policy 常量**（`vfrsqrt7.nxv2f32.i64:128`）；
- 二元族（`vfadd/vfmax`）重载名列**结果 + 一个操作数**（`vfadd.nxv2f32.nxv2f32.i64`），`vfadd` 带 frm、`vfmax` 不带。
⇒ **名字 token 数必须按"内建的重载表"写，不能按实参个数推**（A 线第一版按操作数个数生成，`vcos` 多了一个 `.nxv2f32`，厂商 IR 立刻对不上）。
**数字**：317 格 **byte-identical 145 → 148（+3）**、**OURS-GAP 67 → 64（−3）**、byte-diff 38、`.c` 零 gap；musl **56/56**；新增逐字节 3 格 = `mathdev_raisa_frsqrt_{f,f16,bf16}`。
**重要副产品**：`vfrsqrt7` 是本树**已可选中**的上游 RVV 内建 ⇒ 这 3 格"IR 一放行就 byte-identical"，**反证它们此前只卡在 IR、不在后端**。
**另 12 格（cos/sin/log2/exp2）**：IR 已与厂商**逐字一致**，但本树无 `vcos/vsin/vlog2/vpow2` 伪指令 ⇒ 诊断从 `Don't know how to custom type legalize` 变成 **`Cannot select: intrinsic %llvm.riscv.vcos`**（仍 OURS-GAP，**不是退步**）—— 这**点名了 E 线要补的东西**。

## 【真根因·零重编证明】9 格（及 7 个 `id_*`）差异 = 厂商 `RISCVSubtarget::getSchedModelForCPU` 有一处**我们缺失的 override**（2026-10-17 01:15，C 线，parent 采信）
厂商 `libLLVMRISCVCodeGen.a(RISCVSubtarget.cpp.o)` 里（反汇编逐条可读，.rodata 字面量 = `andes-45-series` 15 字节、`tsm-x82`）：
```cpp
const MCSchedModel &RISCVSubtarget::getSchedModelForCPU(StringRef CPU) const {
  if (CPU == "tsm-x82" && !EnableTX8SchedStrategy)   // 厂商默认 EnableTX8SchedStrategy = 0
    CPU = "andes-45-series";
  return MCSubtargetInfo::getSchedModelForCPU(CPU);   // 尾调用、虚函数 dispatch
}
```
⇒ **厂商 `tsm-x82` 的 codegen 默认用 `Andes45Model` 调度**，`TX82Model` 只在 `-riscv-tx8-sched=true` 时用；而 TX82Model **仍然在静态 MC 表里** —— 这正是为什么 wt-sched 那条**静态**对拍"完全一致"却解释不了 codegen。
我们树经 `0032` 把 `TSM_X82` 静态接成 `TX82Model`、又**没有这个 override** ⇒ 我方 codegen 用 TX82Model ⇒ 延迟不同 ⇒ pre-RA 顺序不同。
**量化（双向）**：`-mllvm -misched-dcpl` 厂商默认 `Critical Path(GS-RR)=6`、我方 `=30`；把厂商 `-riscv-tx8-sched=true` 打开变 30（= 我方）。
**零重编证明（用现成二进制）**：`InitMCProcessorInfo` 是 `CPUSchedModel = &getSchedModelForCPU(TuneCPU)` 的虚调用 ⇒ 用 `-tune-cpu andes-45-series` 即可在**未改动**的二进制上做 A/B：
`raisa_oracle.py --extra-cc1 "-tune-cpu andes-45-series" --cells mathdev_raisa_fadd_{rd,ru,rz}_{f,f16,bf16}` ⇒ **9/9 asm+obj IDENTICAL**（不加时 0/18），`Critical Path` 30 → **6**，`.s` 的 `li a2,512 / ld a3,16(a0)` 与厂商逐字节一致。**先证后编达成。**
**修复（约 10 行、零 TableGen、1 个 TU）**：`RISCVSubtarget.{h,cpp}` 加 override（镜像厂商；**静态 MC 表不动** ⇒ llvm-mca parity 保留）；`MCSubtargetInfo.h` 的 `__TSM_RISCV__ virtual` 已在（`llvm/CMakeLists.txt:1339`）⇒ override 一定生效。补丁 `2001-*`，只动 `RISCVSubtarget.{h,cpp}`（与 D/5 线的 `CMakeLists/RISCV.h/RISCVTargetMachine.cpp` **无重叠**）。

**`vreg-align` 这条线关闭**（我先前批准的改派作废）：厂商 `-mllvm -print-before/after=vreg-align` 对那 9 格 + 7 个 `id_*` + 抽样 3 个普通格**都是 MIR no-op** ⇒ 不可能翻任何一格；它只在我方**会崩**的格上有动作（`mem_l1_size_256`/`alloc_l1shared_bank_on`/`control_divergent_loop`，都撞 `Don't know how to custom type legalize`），且它输出的 `PseudoVSLH_ALIGNED_VX_M2` 族**在我们 TD 里 0 命中**（`grep -c _ALIGNED_ RISCVInstrInfoVPseudos.td` = 0）⇒ 单独实现无意义，必须连 `*_ALIGNED_*` 指令族一起做（属 **ISel/TD 线**，即 E 线领域）。**不为它排 session。**

## A 线第 2 轮收尾（2026-10-17 01:20）：**parity 136 → 148（+12）、OURS-GAP 77 → 64（−13）**
**交付**：`0053`(BF16)/`0056`(read.reg 族)/`0057`(掩码白名单)/`0058`(无条件打包)/`0059`(一元数学族) —— 全部 `patch -p1` 回放验证、逐字节复现 src。
**累计数字**：317 格 **byte-identical 136 → 148（+12）**、**OURS-GAP 77 → 64（−13）**、byte-diff 37 → 38、vendor 36/30、ours-extra 1、**`.c` 零 gap**、**每轮逐行 diff 只有改善**；musl A/B **56/56**（含 pre/post 对照）；全量 oracle 现 **12 格 asm+obj 逐字节**、无新 DIFF。
新增 12 格：`config_{repeat,overwrite}_device`、`imm_l1_flush_{31,32}_object`、`f{add_rn,max}_bf16`、`id_{ntid_y,ntid_z,tid_x}`、`frsqrt_{f,f16,bf16}`；另 7 格 `id_{ctaid,nctaid}_{x,y,z}`+`id_ntid_x` ⇒ OURS-GAP → byte-diff（IR/vtype/指令集已逐字对齐）。

**oracle 工具改造完成（我点的小活）**：两侧编译一律 `setarch -R`（关 ASLR）；新增 `--probe N`（厂商 N 次 distinct 布局 → `parity/vendor_nondet.tsv`）、`IDENTICAL-MODULO-PERM`、`VENDOR-NONDET` 分类。
**实测结论（简化验收口径）**：**在 `-R` 协议下，28 个 shared 格里厂商能编译的 17 格 8/8 次输出完全相同 ⇒ `VENDOR-NONDET` 集合为空**；另 11 格是厂商自己失败。⇒ **`-R` 下这些格的逐字节目标是唯一确定的，不需要排列归一化兜底**（归一化路径保留作兜底）。**据此修正本文件前文**：`VENDOR-NONDET` 在实际协议下为**空集**，其存在意义仅是"若不做 `-R`，多变量格不可比"。

**第 3 方交接（未验证项）**：① 12 格 `mathdev_raisa_{cos,sin,log2,exp2}_{f,f16,bf16}`：IR 已与厂商逐字一致，现卡在 `Cannot select: intrinsic %llvm.riscv.vcos` ⇒ **等 E 线的伪指令层**；② 同族剩余：`fdiv`（厂商分解 `vfrec7(7)+vfmul(7)`）、`min/max/umin/umax`（整数二元、仅 AVL）、`reduce_{sum,fmax,fmin}`（`vfmv.v.f`+`vfredusum/fredmin/fredmax`+`vor.vs` 多指令序列）、`abi_*` 的"保留标量算术 + store splat"规则（需把标量指令加入白名单且禁止其操作数是被向量化过的值）；③ `*_object` 变体走另一套 per-case 命令行，仅在 317 格 parity 里覆盖。
**⚠️ 归因再更正**：A 线收尾报告里仍写"③ 根因 = `vreg-align`"——**这一条已被 C 线进一步证伪**（`vreg-align` 是 MIR no-op），真根因是**缺失的 `RISCVSubtarget::getSchedModelForCPU` override**（见上一节）。后续接手者以"override"为准。

## D 线 `config-instr-opt` 中期强结果 + **两个会静默污染测量的机器级陷阱**（2026-10-17 00:25，parent 采信）
**结果**：5 个 `config_*_device` 格 **全部 asm+obj 逐字节一致**（`across_compute`/`mixed`/`with_barrier` 从 `DIFF→IDENTICAL`；`overwrite`/`repeat` 保持）⇒ 该 5 格的归一化 `.s` 与厂商逐行 diff = **0/0/0/0/0**。
**契约**（比结果更值钱）：喂厂商 pre-`config-instr-opt` module → 我方 → 比 post module ⇒ **5/5 strict EXACT-IR-MATCH**；73 个探针同一 harness ⇒ **37/37 EXACT，DIFF 集合与"拿厂商自己当 harness 编译器"完全相同 ⇒ 0 额外差异**；端到端 `-O2` 10/10 EXACT；`-O1` 探针 `EXACT=30 / SAME-EFFECT=39 / EFFECT-DIFF=0`（1 个 `AFTER-DIFF` 是**厂商自己 SIGSEGV**）。
**真值表完整性**：13 个**没有 clang builtin** 的 config 内建（`lsu.set.*`/`dte.set.*`）用合成 `.ll` 在 IR 层与厂商**逐行一致** ⇒ `ConfigIntrs[20]`/`ConfigUseGroups[0..14]` 表**完整正确**（`wt-config/recon/config_intr_tables.{md,json}`、`recon/CONFIG_INSTR_OPT.md`）。
**门槛**：musl **56/56**；317 格与 A 线基线同（148/38/64、`.c` 侧逐格未变、零 gap）；净效 **+3 格 config**（见下面陷阱①：另 −3 是测量伪影）。

### ⚠️ 陷阱①：**派生 build 目录里可能有"比源码还老"的目标文件**（静默污染测量）
D 线第一轮 parity **掉了 3 格** `mathdev_raisa_{fadd_rn,fmax,frsqrt}_bf16` —— 根因**不是**它的 pass，而是它那个**派生** build 目录里的 `RISCVInsertVSETVLI.cpp.o` **还是 `0009`(BF16) 之前**的对象（目录链 `wt-raisa-r2/build ← wt-raisa-be/build ← wt-xfrm/build`）。
**`ninja` 查不出来**（路径改写让每条边都 dirty），**`minibuild.py` 也查不出来**（它自己编的对象没有 `.ninja_deps` 条目）。⇒ **结论：不能在"从别处派生的 build 目录"上直接采信 parity，必须先扫 stale 对象。**
**共享工具**：`tools/sweep_stale_objects.sh`（提自 `wt-config/tools/`）—— 解析 `build.ninja` 找 `src mtime > obj mtime`，只重编那些。**每条 `wt-*` 线都应跑一次**（尤其 build 目录源自 `wt-raisa-be/build` 的）。

### ⚠️ 陷阱②：`tools/.builds-paused` 会在**别的线大构建**时出现 ⇒ 等待是正确行为
00:05 左右 D 线看到哨兵，同时 `wt-raisa-be` 有一个持续约 25 分钟的大构建（`ninja=1, cc1plus=2`）。**看到哨兵就等、不要判定失败或绕过**（哨兵意味着调速器判定机器忙/内存紧张）。

## D 线最终报告：`config-instr-opt` 重建完成（补丁 `0060`，2026-10-17 00:35）
**5 格 asm/obj = 10/10 槽 IDENTICAL**（基线 4/10）；归一化 `.s` 对厂商逐行 diff 由 `1/21/0/0/1` → **`0/0/0/0/0`**。
**契约**：5 格 **5/5 strict EXACT-IR-MATCH**；73 探针 **37/37 EXACT 且 0 额外差异**；端到端 5 格 before/after **10/10 EXACT**；**全 114 格 `.rc` 语料 `EFFECT-DIFF = 0`**。
**317 格（stale 修复后重采）**：**byte-identical 148 → 151**、byte-diff 35、OURS-GAP 64、ours-extra 1、vendor 33+33；**`.c` 侧仍零 gap 且变更格数 = 0**（唯一其他变动是 3 个 `mem_*_u8_uniform` 的厂商侧 ASLR 抖动）。
**stale 修复**：`tools/sweep_stale_objects.sh` 重编了 3 个"源码比目标新"的对象（`RISCVInsertVSETVLI.cpp.o` 缺 A 的 `0009`、`TX8LowerSharedPass.cpp.o`、`LLLexer.cpp.o`）⇒ 3 个 `*_bf16` 格重新与 A 的二进制一致。
**未做/未主张（如实）**：未动 `RISCVISelLowering.cpp`、未做 `3002`；**未主张循环格的端到端 IR 逐字节**（我方仍缺 `promote-alloca`/`rpu-always-inline`/`rpu-annotate-uniform`/`tx8-annotate-control-flow`，循环格只证到 `EFFECT-DIFF=0`）；未复现厂商多循环时 `config.opt.preheader` 的**命名顺序**（厂商用 `DenseMap<Loop*>` 哈希序 = IR 文本层的厂商不确定性，不影响汇编）。真值表/契约落 `wt-config/recon/`，交接 `wt-config/logs/handoff-config-instr-opt.md`。

## 预算停手点（2026-10-17 00:45）—— 全部 agent 已停
**已验证且在库**：parity **byte-identical 151 / OURS-GAP 64 / `.c` 零 gap**；oracle **12 格 asm+obj 逐字节**；`config-instr-opt`（`0060`）契约 5/5 EXACT、探针 37/37 无额外差异、5 格逐字节；`rpu-lower-shared`（`0054`）；A 线 `0053/0056`–`0059`。
**停手时未完成/未验证（恢复清单）**：
1. **C 线的 schedmodel override 补丁未验证**（`0061-UNVERIFIED-raisa-schedmodel-andes45-override.patch`，来源 `wt-raisa-c/patches/` 最新一份）：真根因已由零重编证明锁定（`-tune-cpu andes-45-series` ⇒ 9/9 IDENTICAL、Critical Path 30→6），但**该补丁落地后的 9 格 `fadd_{rd,ru,rz}_{f,f16,bf16}` 与 7 个 `id_*` 格结果未报**；其 musl 日志显示我方 pre/post A/B **6 处真实差异**（预期：override 会改变所有 `tsm-x82` 编译的调度），而 `vendor_vs_pre`/`vendor_vs_post` 两份日志**同为 3/56**（⇒ 该 fix **没有**改变"我方 vs 厂商"的 musl 关系；但 3/56 这个绝对值异常低，**疑为对照口径/环境问题**，需一次廉价复核）。
2. **E 线伪指令层**（最大块）：`vslh`（76 调用点/22 module）+ `vluxei/vsuxei` + `vcos/vsin/vlog2/vpow2` 的伪指令/pseudo→real/lowering 三件套 —— 状态 `wt-raisa-r2/recon/isel_connect/`（含 `intrinsic_inventory.tsv` 与 `postsimt2simd_ab.py`）。
3. **第五条 EXEC/SIMT-mask 族**（`TX8LegalizeEXEC` 伪代码 1162 行在手）—— 刚开工即停。
4. **A2 剩余 ② 家族**（`fdiv`/`min/max/umin/umax`/`reduce_*`/`abi_*` splat）—— 刚开工即停。
**本会话窗口花费：¥18.50**（18:56 → 00:45，余额 1547.66 → 1529.16）。

## E 线 `vslh` 试点：机制打通（0 格位移，但阻塞直方图改变了靶心）+ 修好 `minibuild.py` 的 stale 漏检（2026-10-17 01:00）
**它先修了一个真 bug（与我记的 stale 陷阱同源，但更隐蔽）**：`wt-raisa-r2/build` 里 `RISCVGen*.inc` 是 23:56 生成的，而 **`RISCVISelDAGToDAG.cpp.o` 还是 18:04** ⇒ 23:57 的 `clang-21` **根本没有 vslh pattern**。
根因：`tools/minibuild.py::norm()` **硬编码** `OLD_BUILD=wt-xfrm/build`，而 `logs/ninja_deps.txt` 里的依赖记的是 `wt-raisa-be/{build,src}` ⇒ "objects to rebuild: **0**"（静默假绿）。
**修法**：`norm()` 改为按 `/wt-<name>/{build,src}/` **通用归一化**；修完后 **重编 62 个 stale 对象 + 4 个归档 + relink，rc=0**。⇒ **所有树都该把这个修法同步过去**（每条 `wt-*/tools/minibuild.py`）。

**`vslh` 机制（patch 将出 `1010`）**：`RISCVInstrInfoXTsm82s.td` 加 `multiclass VPseudoXTsmVSLH_VV_VX`（镜像上游 `VPseudoVWALU_VV_VX`：`MxListW` × {VV,VX} × {plain,MASK}、`@earlyclobber`、`SchedBinary`）+ `defm PseudoXTsm_VSLH` + `defm : VPatBinaryW_VV_VX<"int_riscv_vslh","PseudoXTsm_VSLH",AllWidenableIntVectors>`。
**`getRVVMCOpcode` 无需改动**：`PseudoToVInst` 去掉 `Pseudo`+`_M1` 即得 `XTsm_VSLH_VX`，现成的 `RISCVVPseudosTable` 就提供伪→真映射。
**探针结果**：改前 `fatal error: Cannot select: intrinsic %llvm.riscv.vslh`；改后 MIR `early-clobber %4:vrm2 = PseudoXTsm_VSLH_VX_M1 $noreg(tied-def 0), %0:vr, %1:gpr, killed %3:gprnox0, 5, 0`（形状/early-clobber/VL/SEW 与厂商 `PseudoVSLH_VX_M2 … 128, 5, 0` 一致），asm `vsetvli a2,zero,e32,m1,ta,mu` + `vslh.vx v10,v8,a0` / `vslh.vv`。
**但格位移 = 0**（46/28/26 → 46/28/26）：在 post-`simt2simd` module 里 **`vslh` 从来不是 DAG 上第一个选不出的节点**。
**🔑 阻塞直方图（46 个 REJECTED-IR 格，新工具 `tools/golden_ir_ab.py` 回放缓存 module、确定性）**：
| 阻塞内建 | 格数 |
|---|---|
| **`vluxei.sync`** | **20**（全部 `mem_l1_*`/`mem_*shared_*`） |
| `vor.vs` | 10 |
| `vcos` | 4 |
| `vsin` / `vpow2` / `vlog2` | 3 / 3 / 3 |
| `Don't know how to scalarize the result of this operator` | 3 |
⇒ **靶心应从 `vslh` 改为 `vluxei.sync`**（它一次 gate 20 格）。
**上游选择路径的关键事实（决定实现方式）**：上游对 indexed load/store 是在 **C++** 里选的（`RISCVISelDAGToDAG.cpp:2200` `case Intrinsic::riscv_vluxei → RISCV::getVLXPseudo(...)`），**不是 TableGen `Pat`**；上游伪指令族 `PseudoVLUXEI64_V_M2_M1` 形状 = `(outs VR:$rd), (ins VR:$dest, GPRMemZeroOffset:$rs1, VRM2:$rs2, AVL:$vl, sew:$sew, vec_policy:$policy)` ⇒ 可镜像为 `PseudoXTsm_VLUXEI64_SYNC_V_M*_M*`（⇒ `XTsm_VLUXEI64_SYNC_V`）。
**已知会挡逐字节（不挡选择）的一点**：厂商 vslh 的 `vsetvli` 是 `e32, **m2**, ta, mu, **bf16**`，我方是 `e32, m1, ta, mu` —— fork 的 vslh 伪指令把 **目的 LMUL** 放进 `VLMul`（厂商 `_M2` = 目的 M2、源 VR），而上游 `VPseudoBinaryW_VX<V_M1>` 放的是**源 LMUL**；且 bf16 的 vtype 位**不在 r2 树的 `RISCVInsertVSETVLI`** 里（那是 A 的 `0053`）⇒ E 线要单独把 VLMul 约定与 BF16 位对齐。

## E 线 `vluxei.sync`/`vsuxei.sync` 打通：**20 格 `OURS-REJECTED-IR → DIFF`**（2026-10-17 01:20）
**golden-IR harness（104 个厂商 post-`simt2simd` module 缓存重放，新增 `tools/golden_ir_ab.py`，无厂商调用、确定性）**：
pre `46 REJECTED-IR / 28 DIFF / 26 IDENTICAL` → post **`26 / 48 / 26`**（IDENTICAL 不变、**无回退**）。
**20 格转移**：`alloc_{l0a,l0w,l1}shared_{base,bank_on}`(6)、`alloc_l1_short_ptr`、`control_uniform_loop`、`mem_l1_size_{1,3,127,128,129,256}`(6)、`mem_l1{shared,l0ashared,l0wshared}_u{32,64}_uniform`(6)。
**实现要点（可复用）**：
- 新增 `_SYNC_` 伪指令族 19 combos（idx EEW 32/64 × data EEW 16/32 × 上游 EMUL 规则，含 `_MASK`）；**生成的 72 个名字与厂商 `strings` 提取的 72 个逐一相同**（`diff` 空 ⇒ 命名可机械验证）。
- 选路在 **`RISCVISelDAGToDAG.cpp`**（我新授权）：在既有 `INTRINSIC_W_CHAIN`/`INTRINSIC_VOID` 两个 switch 各加一个 case，复用上游 `addVectorLoadStoreOperands`，**只把 `Operands[1]/[2]`（base/index）对调** —— 因为我们的 `XTsm_*_SYNC_V` 记录是 `(ins VR:$vs2, GPRMemZeroOffset:$rs1, …)`（**index 在前**）而上游 `VIndexedLoad` 是 **base 在前**，而 `lowerRISCVVMachineInstrToMCInst` 是**按位置拷贝**的。`getTgtMemIntrinsic` 加 4 个 case（`RISCVISelLowering.cpp`）。不新增 GenericTable（`RISCV::getVLXPseudo` 的键没有 sync 位；改用文件内 18 行显式表，名字与厂商 1:1）。
**⚠️ 静默陷阱（转给所有线）**：`Select()` 里 **`INTRINSIC_W_CHAIN` 与 `INTRINSIC_VOID` 是两个独立的 `switch (IntNo)`**（该文件 2158 / 2454 行）—— **把 load 型内建放进 VOID switch 会编译通过、链接通过、然后静默地永不匹配**（症状与"没实现"完全一样）。
**为何尚未 IDENTICAL（全部是 vtype/编号/调度，不是选择问题）**：
- **8 个 `mem_l1_*`**：我方**少一条 vsetvli**（厂商 `vmv.v.x v8,a1` vs 我方 `v10,a1`，寄存器编号整体偏移）。两步已定位：
 ① **vslh 的 LMUL 约定**：厂商 `PseudoVSLH_VX_M2` 的 `VLMul` 是**目的** LMUL（出 `e32,m2`），上游 `VPseudoBinaryW_VX<V_M1>` 放**源** LMUL（出 `e32,m1` ⇒ 与已有 vsetvli 同型 ⇒ 不新发）。**已修**（新增 `XTsmWiderLMUL` 映射 + `VPatXTsmBinaryW_VV_VX` 用 `Wti.LMul.MX`）；我方 VSLH 名集从 {MF8..M4} 变为与厂商一致的 **{MF4,M1,M2,M4,M8,MF2}**。
 ② **gather/scatter 的新 vsetvli 唯一差别 = bf16 vtype bit 8**（`e32,m1,ta,mu,bf16`）⇒ 这是 `getCTPseudoInfoByBF16` 的 **per-opcode** 需求（`PseudoVLUXEI64_SYNC_V_*` 在 BF16 集里），**A 的 `0053` per-function 属性机制覆盖不到**（`mem_l1_size_1` 里没有 bf16 算术 ⇒ 属性不会被打上）。⇒ `mem_*` 族逐字节卡在 `RISCVInsertVSETVLI.cpp` 的 `DemandedFields` 需要一个"本伪指令要求 bit 8"的来源。
- **7 个 `alloc_*`**：地址运算（`li a2,1` vs `addi a2,a1,128` ⇒ 共享地址布局那类）；`control_uniform_loop`：寄存器编号。
**`vor.vs`（10 格）归属确认**：厂商真指令 `XTsm_VOR_VS`（存在），由 **reduce 族**产生（`PseudoVOR_VS_M1 $noreg(tied-def 0), %1:vr, %7:vr, 128, 5, 0`，形状 = 上游 `VPseudoBinaryNoMaskPolicy`）⇒ **属 E 的 ISel 层**；其 pattern 需 result/scalar 的 LMUL 关系（语料里两种：`nxv2i16.nxv4i16` 与 `nxv2i32.nxv2i32`，**不是固定 2×**）⇒ 先由厂商 MIR 定死关系再写。
**build #3（进行中）**：vslh VLMul 修正 + math 族（`vcos/vsin/vlog2/vpow2`：`_V_<MX>_E<SEW>` + bf16 孪生 `_BFloat16_V_<MX>_E16`，**168 个名字与厂商 168 个逐一相同**；为让 bf16 名字可被 `PseudoToVInst` 解析，在 `RISCVInstrInfoVPseudos.td` 的 affix 表加 `["_BFloat16",""]`，厂商同款做法、对本树其余伪指令名零影响）。
**门槛**：musl **56/56 结果一致**；317 格用 r2 自己的口径（首次把 `--cc` 写成相对路径 ⇒ 全 rc=127 假失败，已修）。

## C 线收尾（**大跳变**）：schedmodel override 落地 ⇒ **parity 165 / oracle 逐字节 32**（2026-10-17 01:45）
**补丁**：`0062-raisa-tsm-x82-andes45-schedmodel.patch`（**只动 2 文件**：`RISCVSubtarget.h` +9、`RISCVSubtarget.cpp` +23；与 `CMakeLists`/`RISCV.h`/`RISCVTargetMachine.cpp` **无重叠**）。旧的 `0061-UNVERIFIED` 已删除（被 `0062` 取代）。
**验收（最终二进制，重编 1 TU + relink、0 stale 对象）**：
| 门槛 | 改前 | 改后 |
|---|---|---|
| (a) 9 × `fadd_{rd,ru,rz}_{f,f16,bf16}` | 0/9 | **9/9 `asm:IDENTICAL obj:IDENTICAL`** |
| (b) 7 × `id_{ctaid,nctaid}_{x,y,z}` + `id_ntid_x` | 0/7 | **7/7 IDENTICAL** |
| (d) 全量 oracle（114 格） | 6 ident / 16 diff / 81 fail | **32 / 3 / 68**，**0 回退，+26** |
| (d) 317 格 parity | 138 / gap 77 / diff 35 | **165 / gap 64 / diff 21**，**0 回退**，`.c` gap = 0 |
| (c) musl A/B（`clang-pre-simt` vs 我方） | 46/56 .o、56/56 结果 | 43/56、50/56、6 diffs（**全在 `-O2`**，且 none success↔fail） |
| (c) musl **厂商 vs 我方**（剥 `.comment`） | 24/56 .o、28/56 结果 | **完全相同**（0 新一致、0 新不一致） |
**⚠️ 口径澄清（parent 提问，C 已答，结论采纳）**："**46/56**"是 **A/B 闸门**（我方 pre vs 我方 post），**不是**厂商对照。`vendor_vs_ours` 的 3/56 是因为 `musl_ab.py` **比原始字节**，而厂商 `.comment` = `21.1.0-rc1 (ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf26…)`、我方 = `(https://github.com/llvm/llvm-project.git cbcb48a88c…)` ⇒ 每格必不同；**剥掉 `.comment`（oracle 的 norm）后厂商-vs-我方 = 24/56 逐字节 = 28/56 结果一致，且改前改后完全相同**。
⇒ **本修复不改变厂商一致性、不破坏任何 musl 格的成功/失败**；那 6 个 `-O2` 差异是"换用厂商默认模型"的预期伴生（相对的是我们自己过去的**错误模型**二进制）。
**建议（C 提出，parent 采纳）**：**重录 musl A/B 的 baseline**（`clang-pre-simt` 用的是错误模型），否则那 3 个 `-O2` 字节差会被当回归反复追。
**未做**：`vreg-align`/`*_ALIGNED_*`（对这 16 格是 **MIR no-op**，已厂商验证；只对我方 ISel 崩的 `mem_l1_size_256` 等是真缺口）；`-riscv-tx8-sched`/TX8 策略（厂商默认关；将来实现时本 override 要改成厂商的条件式 `if (CPU=="tsm-x82" && !EnableTX8SchedStrategy)`）。
**共享工具 bug（C 报告，parent 已修）**：`tools/sweep_stale_objects.sh` 原先**硬编码 `WT=wt-config` 且忽略入参** ⇒ 在别的树上"跑一次扫描"会去重编 D 线的树。现已改为接受 `$1`（tree，缺省 `wt-config`）与 `$2`（build dir）。

## E 线伪指令层收尾（补丁 `0063`/`0064`）：golden-IR 层 **IDENTICAL 26 → 39、REJECTED 46 → 7**（2026-10-17 02:00）
**成绩（golden-IR harness，100 格）**：`IDENTICAL 26 → 39（+13）`、`OURS-REJECTED-IR 46 → 7（−39）`、`DIFF 28 → 54`。
**逐轮 transition**：`vslh` 0（仅探针证明机制）；`vluxei/vsuxei.sync` **20 × REJECTED→DIFF**；math+VLMul **8 × REJECTED→IDENTICAL + 5 × REJECTED→DIFF**；**bf16 vtype 5 × DIFF→IDENTICAL**；`vor.vs` **6 × REJECTED→DIFF**。
**门槛**：musl **56/56**；317 格 parity（r2 口径）**逐格 diff = 0 行**、`.c` 零缺口。
**🔑 关键方法论（E 提出，parent 采纳）**：**317 格 parity 无法由 ISel 层单独推动** —— `.rc` 路径上的目标格失败在 ISel **之前**（`cos_f`/`reduce_sum_f` 是 `Don't know how to custom type legalize`、`mem_*` 崩在 `rpu-lower-shared`）。我方 **IR 层与 ISel 层是串联的** ⇒ **parity 对该层只是"不回退"门槛，进度指标必须用 golden-IR harness**（已提为共享工具 `tools/golden_ir_ab.py`）。
**你点名的三项**：① `mem_l1_size_1` 的 **vsetvli vtype 已逐字一致（含 bf16）**，唯一残差是第二条的 **AVL 寄存器**（我方 `a1` vs 厂商 `a2`，源于 `li a2,128` 的调度位置）⇒ 归 **C 线的 andes-45 模型类**；② IDENTICAL +13 = 12 个 math 格 + `cos_bf16_object`；③ transition 已分"能选出来/转逐字节"两栏申报。
**名字集机械验证**（每族 diff 为空，建议保留为验收习惯）：`VLUXEI/VSUXEI *_SYNC_V_*` 72/72、`V(COS|SIN|LOG2|POW2)_*` 168/168、`VOR_VS_*` 14/14、`VSLH_*` 24/24。
**bit-8（AltFmt）机制**：按 parent 四条条件交付（`0064`，1 文件/144 行，additive）：TSFlags bit 26 = `AltFmtType`（`RISCVInstrFormats.td` +3、`MCTargetDesc/RISCVBaseInfo.h` +3 ← 二者在其原授权外，已单列），消费端只读 `RISCVII::hasAltFmtType(TSFlags)`；`transferBefore` 把 demand 传下去。验证：`cos_bf16` DIFF→IDENTICAL、无回退。
**⚠️ 边界（诚实记录）**：厂商真机制是 `getCTPseudoInfoByBF16/ByHalf` **两张 540 项 per-opcode 表**；TSFlags 能表达，但**成员表无法从语料恢复** —— 它只标了有观测证据的（`XTsm_VSLH_VV/VX`、`XTsm_VLUXEI64_SYNC_V`/`VSUXEI64_SYNC_V`、四个 `_BFloat16_` math 孪生）。**`vle16.v` 在 bf16 格也带 bit 8，但那是"元素类型是 bf16"而非 opcode 无条件要求 ⇒ 需要厂商 `_BFloat16_` 载入伪指令族**（`0009` 已记的那笔债）。
**剩余 7 格 rejected**：3 × `control_{divergent_loop,early_return,nested}`（`Do not know how to scalarize`）——**【更正】不是 A 线 IR，而是第五条线的 ISel/EXEC 活**：A 线把**厂商自己的 post-pass IR** 喂给我方 clang 后 3/3 同样死在 `ScalarizeVectorResult: v1i128 = llvm.riscv.mov128.s.v`，而我方 post-IR 已与厂商打包 prologue 逐字一致 ⇒ 需 `mov128.s.v`/`and.s`/`nor.s`/`set.exec` 的 Pat + `addRegisterClass(MVT::v1i128, &RISCV::SRRegClass)`（已授权第五条线）+ 4 × `mathdev_raisa_reduce_{sum,fmax,fmin}_bf16(+_object)`（需厂商 `PseudoVFRED*_BFloat16_VS_*` 族）。
**三条"静默失败"教训**：① `minibuild.py::norm()` 只认 `wt-xfrm` ⇒ 62 个 stale 对象隐形（已修）；② `Select()` 的 `INTRINSIC_W_CHAIN`/`INTRINSIC_VOID` 是**两个独立 switch**，load 型放 VOID ⇒ 编译链接全过、永不匹配；③ **新踩的**："**声明了却没人读**" —— `AltFmtType` 声明在伪指令、也在 `DemandedFields` 读了，但 vsetvli 的**实际 immediate 走 `transferBefore` 里的 4 参 `setVTYPE()`**，它把 bit 清掉了 ⇒ 输出一点没变而 TSFlags 检查全对。**教训：加"每指令属性"必须追到最终发射点，而不是只检查 demand 计算。**

## 第五条线（EXEC/SIMT-mask）机制完全逆向（2026-10-17 02:30，parent 采信）
**管线位置**：`… -riscv-insert-vsetvli -riscv-dead-defs … -machine-cp -machinelicm -riscv-copyelim **-legalize-exec** -removeredundantdebugvalues …` ⇒ **post-RA**（`-mllvm -debug-pass=Structure`）。
**`TX8LegalizeEXEC` 三件事**（厂商 MIR + `llc -run-pass=legalize-exec` 在手写 MIR 上的判别实验）：
1. `PseudoRAISA_DIVERGENT <mask>,$rd` ⇒ 在**块首 terminator 处**插 `ISETEXEC <mask>, implicit-def $exec` / `$rd = MOVEXECZ implicit $exec` / `$rd = Kcore_Ncc_Read killed $rd`，再删伪指令；
2. 给所有 **implicit use `$vl`** 的指令加 `implicit $exec`（**只有 `$vtype` 的不加** —— 手写 MIR 3 组实验确认）。**这条同时反证厂商寄存器表 `EXEC == 1`**（把 `$vl` 挤到 8，正是 decompiled 里 `hasRegisterImplicitUseOperand(MI,8)` 的常数）；在 `RISCVRegisterInfo.td` 加 `def EXEC` 后 `llvm-tblgen` 输出正是 **`EXEC = 1, VL = 8`**；
3. 冗余 `ISETEXEC` 删除 + mask 为 live-in 时上提到块首并清 `dead`。
**`get.kernel.entry.exec` 不归 legalize-exec**：厂商在 **`riscv-insert-vsetvli` 内部**换成 `$v9 = PseudoVMSNE_VV_M1 undef $v9, undef $v9, -1, 5`（= `vmsne.vv v9,v9,v9` 全零 b32 向量，IR 自带的 `nor.s` 再取反成全 1 入口 mask）；第五条在 **`RISCVExpandPseudoInsts.cpp`（pre-RA，此前无人认领）**做等价展开。
**`vmsne.nxv2i32.i32.i64` 本来就能选**（既有 RVV `VPatBinaryM_*` → `PseudoVMSNE_VX/VI_M1`），无需新代码。
**实现**：新 `TX8LegalizeEXEC.cpp`；`RISCVInstrInfoXTsm82s.td` 的 EXEC/mask 段（SR 类改为可分配/128bit/`[v1i128]`；`XTsm_ISETEXEC*` 加 `Defs=[EXEC]`；新增 `PseudoRAISA_DIVERGENT`、`PseudoRAISA_GET_KERNEL_ENTRY_EXEC`）+ **9 条 `Pat<>`**（mov128.s.x/v、and.s/or.s/xor.s/andn.s/orn.s/nand.s/nor.s/xnor.s、set.exec、raisa.divergent、raisa.get.kernel.entry.exec）⇒ **不需要 C++ ISel**；注册在 `RISCV.h`/`CMakeLists.txt`/`RISCVTargetMachine.cpp`（`addPostRegAlloc`，紧跟 `riscv-copyelim`）。
**⚠️ 裁决：批准 `RISCVISelLowering.cpp` 的 3 行 hunk**（构造函数末尾、`computeRegisterProperties` 之前）：
```cpp
if (STI.hasVendorXTsm82s()) addRegisterClass(MVT::v1i128, &RISCV::SRRegClass);
```
理由：**`v1i128` 的合法性只能由 `addRegisterClass()` 填 `RegClassForVT`**，否则 DAGTypeLegalizer 必走 `ScalarizeVectorResult`（正是 A 线测到的 abort）；`.td` 无通路。**该 hunk 归第五条线所有**，E 线不得删除或冲突；第五条承诺不再碰该文件其它部分、也不碰 `RISCVISelDAGToDAG.cpp`。
**⚠️ 影响面警告**：`def EXEC`（=1）**会移动寄存器编号（`$vl` 8）** ⇒ 所有线的产物可能整体位移；**合并后每条线都必须重跑门槛**（oracle/parity/musl）。
**`get.pred.mask`：厂商自己也崩**（`Do not know how to promote this operator's operand!`，最小 IR 探针复现）⇒ **单列为厂商缺陷，不追**。
**`set.exec0/1`**：第五条顺手加了 `XTsm_ISETEXEC0/1`（`Defs=[EXEC]`）但未加 Pat（不在本族表内）。

## 【重要更正】E 线解出厂商 540 项 bit-8 表全貌 ⇒ **没有 `_BFloat16_` 载入/存储伪指令族**（2026-10-17 02:45，parent 采信）
E 线用厂商自己的 name 表**解出 `getCTPseudoInfoByBF16/ByHalf` 的 540 项**（540/540 解析成功、0 未解析）：**全部是 bf16 算术孪生**，逐项 `{PseudoX_BFloat16_…, PseudoX_…}` 配对。
- **厂商根本没有 `_BFloat16_` 载入/存储族**：`strings libLLVMRISCVDesc.a | grep -E '^PseudoV(L|S)E'` 里 0 个 `_BFloat16_`；540 项里 `VLE/VSE/VLUXEI/VSUXEI/VSLH/VOR_VS` **0 命中**。
- ⇒ **`vle16.v` 在 bf16 格上的 bit 8 是"继承"自同一 vtype 块里的 bf16 算术伪指令**，不是 load 自己的要求。**（我先前给 E 线的"补 `_BFloat16_` 载入族"的任务前提是错的，已更正。）**
- 同时更正 E 线一条旧记录：`PseudoVLUXEI64_SYNC_V`/`XTsm_VSLH_VX` **不在**那张表里 ⇒ 它们的 bit 8 来自 fork 自己的**每指令标记**，不是 `getCTPseudoInfoByBF16`。
- **唯一真解 = 补 `_BFloat16_` 算术孪生**（E 线已做：10 族、13 种 opcode、**156 个名字**，E16 子集与厂商**逐名 100% 相同**）⇒ **不要再让任何线去补"bf16 载入族"**。

## 【重要】`HasVendorXTsm82s` **不是** musl/RAISA 的判别器（E 线实测，parent 采纳）
`RISCVProcessors.td:580` 的 `TSM_X82` Features 里**就有 `FeatureVendorXTsm82s`**，而 musl A/B 与全部 `.c` 格都用 `-mcpu=tsm-x82` ⇒ **该 predicate 在 musl 上恒真**，用它做 gate 等于不 gate。
真正能分开 musl 的**只有 triple（`isRaisa()`）**，而 **TableGen Predicate 表达不了** ⇒ 需要在 C++ 侧门控。**所有线在"要不要给新 pattern 加 RAISA gate"时都必须知道这条**（否则会以为 gate 住了其实没 gate）。

## 【方法学】C 线的"零重编 proxy"（`-tune-cpu andes-45-series`）**不等价于 `0062`**（E 线实测）
E 线在 golden-IR 管线上实测：**proxy 对 100 格 0 个 verdict 变化**，而 **in-build `0062` 翻了 6 个 `fadd_{rd,ru,rz}_{f,f16}`**。原因：`-tune-cpu` 不只换 sched model，还换**整个 tune 特征集**；`0062` 只换 model、保留 `TuneCPU=tsm-x82`。⇒ **别把 proxy 当 0062 的等价基线**（C 线当初用它做"先证后编"仍然成立 —— 那是在它自己的场景下，但不适用于 golden-IR 管线）。

## E 线中途成绩：golden-IR **64 IDENTICAL / 33 DIFF / 3 REJECTED**（2026-10-17 02:45）
`39/54/7 → 64/33/3`（25 格转移、0 回退）：4 个 `reduce_*_bf16` 的 `vfred*` 全部关闭（3 格直接 IDENTICAL，`reduce_sum_bf16`/`_object` 也 IDENTICAL）；9 个 `reduce_{sum,fmax,fmin}×{f,f16,bf16}` 全 IDENTICAL；bf16 vtype 类全 IDENTICAL；6 个 `fadd_{rd,ru,rz}_{f,f16}` 转 IDENTICAL（归 in-build `0062`）。剩余 3 个 REJECTED = `control_*`（`mov128.s.v`，第五条线的活）。
**`vmv` 折叠是两处、不是一处**（E 线更正 A 线）：`:2400` 整数 `sew32simm5 → PseudoVMV_V_I`；**`:2638` fp 孪生 `vfmv.v.f + fpimm0 → PseudoVMV_V_I`（这才是 `reduce_sum_*` 的主因）**；再加一处：整数零 splat 要在 **ISel 就落 `$x0`**（否则两个零 splat 操作数不同 ⇒ MachineCSE 不合并 ⇒ 多一条指令）。三处合并后 `reduce_sum_f` 也 IDENTICAL（补丁 `1013`）。

## A 线第 3 轮：② 剩余家族（`fdiv`/`min|max|umin|umax`/`reduce_*`/`abi_*`）落地 —— 补丁 `0014`（归档 `0065`）：**parity 165 → 181、oracle IDENTICAL 32 → 53（+21）**（2026-10-17 02:35）
**交付**：`wt-raisa-be/patches/0014-raisa-simt2simd-binary-reduce-abi-families.patch`（**833 行 / 17 hunks / 1 文件**，只动 `TX8TransformSIMTToSIMD.cpp`；已用 `patch -p1` 对 0013 态 pre-image 回放，**逐字节复现** `wt-raisa-be/src`）。**本节数字是"core+装饰"版（767 行）；随后加入的第 5 族 `vx` 见文末"追加"节（最终 833 行、parity 182、oracle 55 格）。**
基线前提：先按指令应用 `0062`（`tsm-x82` ⇒ andes-45 调度）并跑 `tools/sweep_stale_objects.sh <tree> <build>`（本轮 0 stale）。

**四个家族规则（判据 = `recon/simt2simd/spec.tsv` + 104 个 `*.after.ir`，不是猜的）**
- **`fdiv`** → `vfrec7.nxv2f32.i64(undef, B, i64 7, i64 128)` + `vfmul.nxv2f32.nxv2f32.i64(undef, A, <rec>, i64 7, i64 128)`（两处 policy=7）。
- **`min/max/umin/umax/div`**（整数二元）：`vmin/vmax/vminu/vmaxu/vdiv.nxv2i32.nxv2i32.i64(undef, A, B, i64 128)` —— **无 policy 常量**，重载名 = 结果 + 一个操作数。
- **`reduce_{sum,fmax,fmin}`**（多指令，含 16 位加宽）：`vfmv.v.f.nxv2f16.i64(undef, 0.0, 128)`（**仅 sum**）→ 若结果类型比 lane 向量宽则 `vector.insert.<nxv4T>.<nxv2T>` ×2（第 2 条插 `undef`、偏移 = 子向量元素数 2）→ `vfredusum/vfredmax/vfredmin.<nxv4T>.<nxv2T>.i64(undef, V, seed[, i64 7], i64 128)` → **zero 向量也要同样加宽成整数型**（`vector.insert.nxv4i16.nxv2i16` ×2）→ `bitcast` → `vor.vs.nxv2i16.nxv4i16.i64(undef, zero, red, 128)` → `bitcast` 回。zero = 入口块**第一条** `vmv.v.x.nxv2i16.i64(undef, i16 0, i64 128)`（在参数解包之前）。
- **`abi_*`**：标量算术**保留**（`BinaryOperator`/`CastInst`/`ExtractValueInst`，且每个操作数都是常量或已知 uniform），到 store 处 `vmv.v.x` splat；**操作数已被向量化的不算**（`abi_two_pointers` 走 `vadd.nxv2i32.i32.i64`，是另一族）。

**三个新解出的事实（都有证据）**
1. **pre-0014 的 pass 对 `abi_*`（5 格）直接 SIGABRT**（parity 里 rc=-6，`IR Dump After … simt2simd`）——不是"没做"，是崩。本补丁修掉：`vle/vse` **惰性声明**（纯标量 store 的 kernel 模块里根本没有 `vle`）、`Widen` 参数化成 `(源类型, 目标类型)`（zero 是 `nxv2i16`、seed 是 `nxv2f16`，原来用同一组名字 ⇒ `Calling a function with a bad signature!` 断言）。6 个 16 位 reduce 格因此从 **rc=-6 崩溃** 变成**干净诊断**。
2. **厂商把保留的标量指令重新物化**：扫全部 104 个 `*.after.ir` 的 kernel body，**标识符型 SSA 名存活数 = 0**（`%conv`/`%add` 一律变 `%7`/`%8`）⇒ 从 IR 层复现只需 `Instruction::setName("")`，**不需要** clone/RAUW（clone 版有真 bug：`%add` 的操作数被 RAUW 成无名 clone 后就不再满足 uniform 判定，第二条 clone 不会生成）。
3. **打包签名带一整套"常量"修饰**：104/104 的 `define` 行都是 `(ptr noalias noundef nonnull readonly captures(none) %0) local_unnamed_addr`，**与源参数属性无关**（单参数格 `mem_l1shared_u8_uniform` 也是同一串）⇒ 照抄即对；`local_unnamed_addr` 必须显式 `NF->setUnnamedAddr(F.getUnnamedAddr())`（`Function::Create` 从 `UnnamedAddr::None` 起，我们原来把它丢了）。**语义上也成立**：参数块只被读、不被写、不被捕获。

**IR 逐字判据（最终二进制，`tools/simt2simd_ab.py --all-recon`）**：22/22 家族格（fdiv×3、整数二元×5、reduce×9、abi×5）
`-print-after=simt2simd` 的 **kernel body 字节、call-site 声明（名字+签名）、`define` 行（含 `#N`）全部与厂商相同，且不需要 SSA 归一化**。
唯一模块级残差 = 厂商 pass **预先声明**、body **从不引用**的 8–10 条 boilerplate 声明（`vid`/`vmul`/`vadd`/`vector.insert`/`vluxei.sync`…）：我们按需声明（`ours_only = 0`，即我们不多声明任何东西）——惰性、不影响 body/属性组编号/codegen。

**门槛**（REF = `0062+0001..0013` 的 `logs/r12/ref0062/clang-21-ref`；FIN = `0062+0014`；两轮都在 `sweep_stale_objects.sh` 0 stale 后采集）
| 门槛 | REF（= 你的全局基线） | FIN |
|---|---|---|
| 317 格 parity | **165 / gap 64 / diff 21**、vendor 36+30、extra 1、`.c` gap 0（**逐格复现**你记录的基线） | **181 / gap 48 / diff 21**、vendor 36+30、extra 1、`.c` gap 0 |
| 全量 oracle `--all`（114 格） | 32 IDENTICAL / 3 DIFF / 68 OURS-FAIL | **53 / 3 / 47**（**+21，0 回退**；3 个 DIFF 仍是 `config_*_device`，需 `0060`） |
| musl 厂商 vs 我方（**剥 `.comment`**） | 24/56 逐字节 `.o` = 27/56 结果 | **24/56 = 27/56**（不变） |
| musl 自家 pre/post（REF vs FIN） | — | **56/56（raw 与剥 comment 都 56/56）** ⇒ 本补丁对 musl **零影响** |
| musl canonical A/B（`clang-pre-simt` vs FIN） | — | 43/56 + 7 = **50/56**、6 处真实差异**全在 `-O2`**（= `0062` 已登记的伴生，非本轮引入） |

**317 格 transition（16 格，全为 `OURS-GAP → byte-identical`，0 反向）**：`abi_{float,ints,struct,pointer_u32_u64,pointer_u64_u32}`、`mathdev_raisa_fdiv_{f,f16,bf16}`、`mathdev_raisa_fmin_{f,f16,bf16}`、`mathdev_raisa_{min,max,umin,umax,div}`。
**oracle transition（21 格 `OURS-FAIL → IDENTICAL`）**：上一行 16 格 + `abi_*_front` ×5（oracle 有独立的 `_front` 格）。

**剩余（点名下一步，都不是 IR 层）**
- **`reduce_*` 9 格**：IR 已逐字，卡 **`Cannot select: intrinsic %llvm.riscv.vor.vs`**（E 的 `XTsm_VOR_VS`，`0063` 在 `wt-raisa-r2` 里、本树没有）。再叠一层 `reduce_sum_*` 的 `vmv.v.i` vs `vmv.v.x`：**已定位到 ISel 的 TableGen**——最小 `.ll`（`vmv.v.x(undef, i32 0, i64 128)`+`vse`）在 **`-print-after=finalize-isel` 就已是 `PseudoVMV_V_I_M1`**，模式源 = `RISCVInstrInfoVVLPatterns.td:2400-2406` 的 `Pat<(riscv_vmv_v_x_vl $passthru, (sew32simm5 simm5:$imm5), VLOpFrag), PseudoVMV_V_I…>`（fp 孪生在 `:2638-2640`）。**本 pass 产出的 IR 已与厂商逐字相同（`vmv.v.x(…, i32 0, …)`）⇒ IR 层无解**，需 ISel 行做 RAISA-gated 抑制。
- **`control_{divergent_loop,early_return,nested}`：归第五条线（ISel/DAG），不是 A 线 IR。** 决定性证据：把**厂商自己的** post-`simt2simd` IR 喂我方 clang（同 triple/cpu/features/`-O2`）仍然 `ScalarizeVectorResult #0: t21: v1i128 = llvm.riscv.mov128.s.v …` → `Do not know how to scalarize the result of this operator!`；我方这三格的 post-pass IR **打包序言已与厂商逐字相同**、body 正确地未被向量化（多 BB）⇒ 缺的是 `v1i128` 的 `addRegisterClass` 合法性（= 本文件第 682 行第五条线的 3 行 hunk）+ `mov128.s.v`/`and.s`/`nor.s`/`set.exec` 的 `Pat`。**上面第 670 行"3 × control_* ⇒ A 线 IR"这句作废。**
- **`abi_two_pointers`**（`abi_two_pointers_front` 是 IR-action 格 ⇒ 仍 `byte-diff`，属 `-discard-value-names`/`.ident` 那类 cosmetic）：其 `add` 取**已向量化**的 load ⇒ 厂商产 `vadd.nxv2i32.i32.i64`（向量+标量混合形），本任务给 `abi_*` 的规则明确排除此形 ⇒ 仍 OURS-GAP，另立一族。

**⚠️ 新陷阱（静默污染，务必加到清单）**：**`minibuild.py` 自己编过的对象在 `.ninja_deps` 里没有条目，`--force-src` 也扫不到它** —— 本轮 `TX8TransformSIMTToSIMD.cpp.o`（00:21 由 minibuild 编、而 src 02:08 才改）**没被重编**，第一版"改后"二进制其实还是旧 pass + 新 schedmodel。`tools/sweep_stale_objects.sh` 能发现它，但本树当时缺 `tools/rebuild_one.sh`（已从 `wt-config` 拷入 `wt-raisa-be/tools/rebuild_one.sh`，含路径修正）。**纪律：改 pass 的 `.cpp` 后必须核 `TX8TransformSIMTToSIMD.cpp.o` 的 mtime > src**（或直接跑 sweep），不要只看 minibuild 的 `objects to rebuild: N`。
**另一条（口径）**：`tools/musl_ab.py` 比**原始字节**，对"厂商 vs 我方"永远 ~3/56（`.comment` 里的仓库 URL 不同）；**厂商口径必须用新工具 `wt-raisa-be/tools/musl_norm.py`**（raw + 剥 `.comment` 两栏同时报，`normcmp.norm_object` 实现）。

**复现**：`bash tools/sweep_stale_objects.sh wt-raisa-be wt-raisa-be/build` → `python3 wt-raisa-be/tools/simt2simd_ab.py --all-recon`（22/22 家族格逐字）→
`python3 wt-raisa-be/tools/raisa_oracle.py --cc wt-raisa-be/build/bin/clang-21 --all --jobs 2` →
`python3 wt-raisa-be/tools/corpus_parity.py collect --cc wt-raisa-be/build/bin/clang-21 --tag oursB_final --jobs 2 && python3 wt-raisa-be/tools/round1_attrib.py --ours parity/oursB_final.json --out parity/roundB_final.tsv` →
`python3 wt-raisa-be/tools/musl_norm.py --a <vendor clang> --b wt-raisa-be/build/bin/clang-21`。

## A 线 ② 收尾（`0065`，末版 833 行/17 hunks/1 文件）+ **两条新增机器级陷阱**（2026-10-17 03:00）
**逐族（判据 = 厂商 `-print-after=simt2simd` IR 逐字）**：22/22 族格的 **kernel body 字节 + call-site 声明（名字+签名）+ `define` 行（含 `#N`）全部与厂商一致、无需 SSA 归一化**；唯一模块级残差是厂商 pass **预声明** 8–10 个其 body 从不调用的样板内建（我们是按需声明，`ours_only=0`）⇒ **惰性差异**。
① `fdiv` = `vfrec7(undef,B,7,128)` + `vfmul(undef,A,rec,7,128)`，3/3 IR 且**逐字节**；② `min/max/umin/umax/div` = 无 policy 的 `vmin/vmax/vminu/vmaxu/vdiv`，5/5 IR 且**逐字节**；③ `reduce_{sum,fmax,fmin}×{f,f16,bf16}` 9/9 IR 逐字（`vfmv.v.f(undef,0.0,128)` → `vector.insert` ×2 加宽种子 → `vfred{usum,max,min}`（**仅 sum 带 policy 7**）→ 用**整数** `vector.insert.nxv4i16.nxv2i16` ×2 加宽零向量 → `bitcast` → `vor.vs(undef,zero,red,128)` → `bitcast`；零向量是入口块第一条指令）；**6 个 16-bit 格从 SIGABRT(rc=-6) 变成干净诊断**（`Cannot select: %llvm.riscv.vor.vs`，属 ISel 行）；④ `abi_*` = 保留标量算术 + store 处 `vmv.v.x` splat + 拒绝消费已向量化操作数，5/5 IR 且**逐字节**（含 `_front`）。
**门槛**：317 parity **165/64/21 → 181/48/21**（`.c` gap 0、0 回退）；oracle **32 → 53**（+21、0 回退）；musl 厂商-vs-我方 24/56 **不变**；自身 pre/post **56/56**（0014 对 musl **零影响**）。
**⚠️ 新增陷阱①：`minibuild.py` 看不见"它自己编出来的对象"**（无 `.ninja_deps` 条目）⇒ `--force-src` 会**静默跳过** pass 的 `.o`；A 线第一份 "after" 二进制其实是"旧 pass + 新 schedmodel"，会得出假数字。**规则：改完 pass `.cpp` 后必须核对 `<pass>.cpp.o` 的 mtime > src（或跑 sweep），永远不要相信 "objects to rebuild: N"。** 已提共享 `tools/rebuild_one.sh`。
**⚠️ 新增陷阱②：`musl_ab.py` 比原始字节** ⇒ "厂商 vs 我方"会**永远钉在 ~3/56**（`.comment` 里的仓库 URL 不同）。已提共享 `tools/musl_norm.py`（**一次跑给出 raw 与剥 `.comment` 两个口径**）——**厂商口径只看后者**。
**第三条交接**：`abi_two_pointers` 应单列一族 —— 它的 `add` 消费向量化值，厂商发 `vadd.nxv2i32.i32.i64(undef, vle, i32 %6, i64 128)`（**vector+uniform 的 `vx` 形态**），超出既定的 `abi_*` 规则。**parent 已批准它单做（约 30 行 + 1 次重编）。**

## 第五条线第一轮完成（`0066`）：EXEC/SIMT-mask 族 **7/7 内建全部能选**，3 格解封（2026-10-17 03:10）
**门槛**：golden-IR（100 格）`REJECTED-IR 46→44、DIFF 54→56`，**transition 2 格**（`control_early_return`/`control_nested`：REJECTED→DIFF），无回归；第 3 格 `control_divergent_loop` 的阻塞点**从它的族换成 E 线的 `vluxei.sync`** ⇒ **本族已全清**。
parity（其树基线 = `wt-raisa-be/src@23:55`，不含 0062–0065）：`byte-identical 139`，**0 transition**；musl **厂商-vs-我方（剥 `.comment`）24/56 + 3 = 27/56 outcomes**（≥24 达标，29 处差异全是既有 RVV 缺口）；**musl 自身 pre/post = 46/56 + 10 = 56/56 outcomes、0 real difference** ⇒ **`def EXEC`/`ISETEXEC`/`implicit $exec` 对 musl 零影响、无 success↔fail**（这是本条线最关键的验收，因为 xtsm82s 门控在 musl 上无效）。
**逐 intrinsic（探针 `t_exec7.ll`，7 个同模块，rc=0）**：`mov128.s.x`→`mv128.s.x`；`mov128.s.v`→`mv128.s.v`；`and.s`→`and.ss`；`nor.s`→`nor.ss`；`set.exec`→`isetexec`；**`raisa.divergent`→`isetexec`+`movexecz`+`kcore_ncc_read`（与厂商逐指令一致；原 `set.exec` 的 ISETEXEC 按厂商规则被删、三指令插在块首 terminator 处）**；`get.kernel.entry.exec`→`vmsne.vv vd,vd,vd`（全零 b32）+ 配套 vsetvli；`vmsne.nxv2i32`→`vmsne.vi v0`（既有 RVV 路径）。
**未做/不追**：`get.pred.mask`（厂商自己也崩，已最小复现）；`set.exec0/1`（指令属性已加 `Defs=[EXEC]`，Pat 未加）。
**交付**：`TX8LegalizeEXEC.cpp`（pass 名 `legalize-exec`）；注册 hunk **明确列出**：`RISCV.h`、`CMakeLists.txt`、`RISCVTargetMachine.cpp`（`addPostRegAlloc()` 里 `riscv-copyelim` 之后，**triple 门控 `isRaisa()`** —— 这是唯一的 RAISA 门控点，xtsm82s predicate 无效）；`RISCVInstrInfoXTsm82s.td`（SR 类 128bit/可分配/`[v1i128]`、`XTsm_ISETEXEC*` 加 `Defs=[EXEC]`、`XRTsm_MOVEXECZ` 加 `Uses=[EXEC]`、2 伪指令、**9 条 `Pat<>`**）；`RISCVRegisterInfo.td`（`def EXEC`）；`RISCVISelLowering.cpp`（**仅 3 行**，已批准）；`RISCVExpandPseudoInsts.cpp`（pre-RA 展开 entry-exec）。
**必须转达的四点**：① **`def EXEC`=1 会整体挪寄存器编号（`$vl`→8）** —— 对 musl（46 格逐字节全同）与 parity（0 transition）实测**惰性**，但**合并后每条线仍要重跑自己的门槛**；② **xtsm82s 门控无效**，唯一 RAISA 门控是 `addPostRegAlloc` 的 `isRaisa()`（pass 不进 musl 管线）；③ **给 harness 的建议：把 `0055`（`rpu_kernel` 的 lexer 关键词）并入公共基线** —— 该线的树没有它，喂厂商 IR 会在 IR 解析阶段 100 格全失败；它的绕过法是 `rpu_kernel void` → **`cc 124 void`**（RPU_KERNEL 同号），效果与 A 线一致；④ **`control_nested`/`control_early_return` 现在只差细节**：指令顺序/寄存器分配 + **一处 vsetvli 策略**（厂商 `ta, ma`，我方 `ta, mu`）—— 在它的纯函数探针里我方也是 `ta, ma` ⇒ 疑为 **A 线 `IsInstrDemandMaskPolicyUnderSIMT` 与邻近指令的交互**，已转 A 线。
**⑤ 警告**：E 线若删 `addRegisterClass(MVT::v1i128, …)` 会立刻回到 `ScalarizeVectorResult` abort。

### 追加：`abi_two_pointers` 一族（vector+uniform `vx` 形态）—— parity **181 → 182**、oracle **53 → 55 格**（2026-10-17 03:00，parent 批准）
**动因（parent 裁定）**：`abi_two_pointers` 的 `add` 消费**已向量化**的 load ⇒ 超出既定 `abi_*` 规则，厂商发
`%8 = call <vscale x 2 x i32> @llvm.riscv.vadd.nxv2i32.i32.i64(<vscale x 2 x i32> undef, <vscale x 2 x i32> %7, i32 %6, i64 128)`。
**先证后编（leads 到一条口径更正，已用 `cc 124` 复验）**：把厂商 post-`simt2simd` IR 里的 `rpu_kernel` **去掉**后喂我方 clang（为绕开本树无 `rpu_kernel` 关键字）得到 `.text`/`.s` 与厂商**不同**，一度像是"IR 映射不足以翻格"。
**更正（含两条）**：① 去掉 `rpu_kernel` 就丢了 `CallingConv::RPU_KERNEL` ⇒ `VSET_SIMT` 不再发射（第 229 行的门控）—— **正确做法是第五条线给的 `cc 124`**（`cc` 与 `RPU_KERNEL` 同号，保留 CC）：改用它后 `vsetsimt 1` 正常出现；② **即使 CC 保留**，该探针仍与厂商差**标量 setup 的指令次序**（`ld a0,0(a0)`/`li a3,512`/`vsetvli` 的位置）—— 因为 `-O2` IR 管线会在**已优化过的** module 上再跑一遍并改变机器序。⇒ **"喂厂商 IR"的探针对"端到端能否逐字节"没有预测力**（本例：探针 DIFF，而真实 `.rc` 端到端 **byte-identical**）。**判据只认 `.rc` 端到端**；做探针时必须用 `cc 124`，并只用它判断"能不能选出来/IR 形状"，不要用它判断逐字节。
**实现（同文件，evidence-bounded）**：只承认语料里唯一出现过的形状 —— `add`、整型、标量类型 == 元素类型、**(向量, 标量) 方向**：
`isVectorScalarAdd()` + `intTypeName()`；两个校验循环各放行一处、重写处发 `llvm.riscv.vadd.<VT>.<scalar>.i64(undef, lane, scalar, i64 128)`。
**已知不覆盖**（无证据，故意不动）：常量右操作数（那是 `vi` 形，如 `control_divergent` 的 `add i32 %1, 3`）、别的 opcode、标量在左的方向。
**爆破半径核对（逐格，非抽样）**：`simt2simd_ab.py --all-recon` = 67 → **68/106 EXACT，唯一新增 `abi_two_pointers`，0 回退**；另用脚本扫了全部 34 个含 `vx` 形调用的格：`vslh/vluxei/vsuxei`（寻址族）、`vmul/vand/vmseq/vmsne/vmsltu/vsll/vxor`（谓词/寻址）都不满足本规则，`mem_*_uniform` 的 before IR 里**根本没有**混合 `add`（那些 `vadd/vmul/vslh` 是 pass 自己造的地址算术）。
**IR**：`abi_two_pointers` 的 `-print-after=simt2simd` **body 字节 + call-site 声明 + `define` 行全部逐字**（`ours_only = 0`）。
**门槛（本行全部含 `0062`；采数前 `sweep_stale_objects.sh` = 0 stale）**：
| 门槛 | 上一版（core+装饰） | 加 `vx` 族 |
|---|---|---|
| 317 parity | 181 / gap 48 / diff 21 | **182 / gap 47 / diff 21**（`.c` gap 0，`gaps by language: raisa 47`） |
| 全量 oracle `--all` | 106 slots（53 格）/ 6 diff / 94 fail | **110 slots（55 格）/ 6 diff / 90 fail** |
| musl 自家 A/B（上一版 vs 本版） | — | **56/56（raw 与剥 comment 均）** ⇒ 本族对 musl 零影响 |
| musl 厂商 vs 我方（剥 `.comment`） | 24/56 | **24/56**（不变，≥ 门槛） |
| musl canonical A/B（`clang-pre-simt`） | 50/56 | 50/56、6 处真实差异全在 `-O2`（仍是 `0062` 伴生集） |
**累计（REF=`0062`+`0001..0013` → 最终）**：317 parity **165 → 182（+17）**、OURS-GAP **64 → 47（−17）**、byte-diff 21 不变、`.c` gap 0；oracle IDENTICAL **32 → 55 格**、OURS-FAIL 68 → 45、DIFF 3（3 个 `config_*_device`，需 `0060`）；**17 格全部是 `OURS-GAP → byte-identical`，0 反向**。
补丁：`patches/0014-…patch` = 833 行/17 hunks（已重新回放验证），归档 `0065` 同步刷新（md5 `22ea9c4dfbab0a7958c0d0289dbe519d`）。

### 【诊断·非本层】`control_nested`/`control_early_return` 的 `ta,mu` vs `ta,ma`：**不是 `IsInstrDemandMaskPolicyUnderSIMT`，是入口两条向量指令的次序**（2026-10-17 03:00，A 线，parent 点名的诊断）
**证据（同一条 IR：把厂商 post-`simt2simd` IR 喂给含第五条补丁的 `wt-exec`，`-mllvm -print-after=riscv-insert-vsetvli`，与厂商对同一 `.rc` 的 dump 对齐）**：
| | 厂商 | 我方（wt-exec，喂厂商 IR） |
|---|---|---|
| `control_nested` bb.0 | `PseudoVSETVLIX0 $x0, 208` → `$v9 = PseudoVMSNE_VV_M1 undef $v9,undef $v9,-1,5` → `PseudoVSETVLIX0X0 80` → `$v8 = PseudoVID_V_M1` | `PseudoVSETVLIX0 $x0, 80` → `$v8 = PseudoVID_V_M1` → `PseudoVSETVLIX0X0 208` → `$v9 = PseudoVMSNE_VV_M1 …` |
| `control_early_return` bb.0 | 同上（208, VMSNE, 80, VID_V） | 同上（80, VID_V, 208, VMSNE） |
**结论**：**每一条向量指令拿到策略都是对的** —— `PseudoVMSNE_VV_M1`（`get.kernel.entry.exec` 的物化）两侧都是 **208 (`ta,ma`)**，`PseudoVID_V_M1` 两侧都是 **80 (`ta,mu`)**。差的是**这两条在入口块的先后**，连带把 "`PseudoVSETVLIX0`（新 AVL 形）↔ `PseudoVSETVLIX0X0`（只改 vtype 形）" 的归属和 vsetvli 条数（6 vs 5）也换了。这解释了为什么"纯函数探针里我方也是 `ta,ma`"：**策略判定本身没坏**。
**归属**：厂商在 **`riscv-insert-vsetvli` 内部**把 `get.kernel.entry.exec` 就地换成 `PseudoVMSNE_VV_M1`（第 679 行）；第五条线是在 `RISCVExpandPseudoInsts.cpp` 里做等价展开 ⇒ 物化点/次序不同。**不是 `0057` 的白名单，也不是 `RISCVInsertVSETVLI.cpp` 的 demand 逻辑**（A 线文件里没有可修的东西）。
**打算怎么做（待批，不在本轮改）**：① 先让第五条线确认它的展开点能否**对齐到原 intrinsic 的位置**（vendor 形态 = 就地替换）；② 若不能，A 线可在 `simt2simd` 侧把 `raisa.get.kernel.entry.exec` 的 **IR 位置**压到 `vid` 之前（但厂商 IR 本来就是这个次序，我方 ISel 后次序变了 ⇒ 更可能是 ISel/展开点问题）；③ 验证方式与其它格一致：`golden_ir_ab` + oracle 逐格，且必须含 `0062`。

## A 线追加：`abi_two_pointers`（vector+uniform `vx` 族）落地 + `control_*` 诊断**摘清**（2026-10-17 03:25）
**`abi_two_pointers`**（`0065` 刷新为 833 行/17 hunks/1 文件，md5 `22ea9c4d…`）：
- **实现的规则（证据边界内的唯一已证形态）**：普通整数 `add`、**左**=向量化 lane 值、**右**=元素类型相同的 uniform 标量 ⇒ `llvm.riscv.vadd.<VT>.<scalar>.i64(undef, lane, scalar, i64 128)`。**故意不含**（无证据）：常量右操作数（那是 `vi` 形态，如 `control_divergent` 的 `add i32 %1, 3`）、其它 opcode、标量在左。
- **爆炸半径是枚举而非抽样**：`simt2simd_ab --all-recon` **67 → 68/106 EXACT**，唯一新增就是 `abi_two_pointers`，**0 回退**；并扫了全部 34 个"厂商 IR 里有 `vx` 形态调用"的格，确认 `vslh`/`vluxei`/`vsuxei`/`vmul`/`vand`/`vmseq`/`vmsne`/`vmsltu`/`vsll`/`vxor` 与 `mem_*_uniform`（其 `vadd`/`vmul`/`vslh` 是 pass 自己的地址算术）都不满足该规则。
- **门槛**：317 parity **181/48/21 → 182/47/21**（`.c` gap 0）；oracle **53 → 55 格**（106 → 110 slots）；musl 自身 **56/56**（raw 与剥 `.comment` 皆同）⇒ 零 musl 影响；厂商-vs-我方剥 `.comment` **24/56**（达标）。
- **累计**：REF(`0062`+0001..0013) → 现在 = parity **165 → 182（+17）**、OURS-GAP **64 → 47（−17）**、byte-diff 21 不变；oracle IDENTICAL **32 → 55**、OURS-FAIL 68 → 45、DIFF 3（`config_*_device` 三格等 `0060`）。**17 处 transition 全是 `OURS-GAP → byte-identical`，0 反向。**

**🔑 方法学更正（A 线，已要求转发所有线）**：喂厂商 IR 的探针**必须同时用 `cc 124`**（`rpu_kernel` 的调用约定号；剥掉 `rpu_kernel` 会让 `VSET_SIMT` 永不发射、探针撒谎）——**但即使加了 `cc 124`，探针仍会在标量准备指令的顺序上与厂商不同**，因为 `-O2` 会在**已优化过的 module** 上再跑一遍。这里探针报 DIFF 而真实 `.rc` 端到端**逐字节一致** ⇒ **这类探针只能用于"能不能选出来 / 形状是什么"，绝不能用于"会不会逐字节一致"。**

**`control_nested`/`control_early_return` 诊断（A 线，结论推翻了我的假设）**：**不是** `IsInstrDemandMaskPolicyUnderSIMT`、**`RISCVInsertVSETVLI.cpp` 无需改动**。
证据（把厂商 post-`simt2simd` module 喂给 **EXEC-capable 的 `wt-exec` 二进制**，与厂商同 `.rc` 的 dump 比）：`control_nested` bb.0 ——
厂商：`PseudoVSETVLIX0 $x0, 208` → `$v9 = PseudoVMSNE_VV_M1 …` → `PseudoVSETVLIX0X0 80` → `$v8 = PseudoVID_V_M1`；
我方：`PseudoVSETVLIX0 $x0, 80` → `$v8 = PseudoVID_V_M1` → `PseudoVSETVLIX0X0 208` → `$v9 = PseudoVMSNE_VV_M1 …`。
⇒ **每条指令的策略两边都正确**（`PseudoVMSNE_VV_M1` 两边都是 `208 (ta,ma)`；`PseudoVID_V_M1` 两边都是 `80 (ta,mu)`）；**差的是这两个向量 op 在入口块里的相对顺序**，从而交换了 `PseudoVSETVLIX0`(带 AVL) ↔ `PseudoVSETVLIX0X0`(仅 vtype) 两种形态、并把 vsetvli 条数从 5 变 6。
**归属：第五条线** —— 厂商是在 **`riscv-insert-vsetvli` 内部原地**改写 `get.kernel.entry.exec`，而第五条线在 `RISCVExpandPseudoInsts.cpp`（pre-RA）展开 ⇒ 物化点落到 `vid` **之后**。**修法**：第五条线把展开对齐到原内建的位置（厂商 = 原地替换）；A 线"把 IR 位置前移"是错的（厂商 IR 本来就在 `vid` 之前 ⇒ 顺序是在 A 的 pass **之后**丢的）。

## E 线 round 3 收尾（`0067`/`0068`）：厂商 **540 项 bit-8 真表恢复** + bf16 归约族 + fork 不做 `_I` 折叠（2026-10-17 03:45）
**① bf16 归约族**：补 `PseudoVFRED{USUM,MIN,MAX}_BFloat16_VS_<LMUL>_E16[_MASK]`（36 名与厂商逐名相同）⇒ 4 个 `Cannot select: vfred*` 全关，**3 格直接 IDENTICAL**。
根因（值得记）：上游归约 multiclass 用 `!cast<VTypeInfo>("VF"#SEW#"M1")` 取累加器类型 = `vfloat16m1_t`（**不是** bf16 的 M1）⇒ **bf16 `vfred*` 从来没有 pattern 命中**；新增 `VPatXTsmReductionV_VS{,_RM}`（显式类型表 + `m1 = !if(bf16, VBF16M1, …)`）。
**② 厂商真表恢复（里程碑）**：位置 `libLLVMRISCVDesc.a(RISCVBaseInfo.cpp.o)`：`CTFloatPseudosTable`（sec 108，**2160 B = 540×u32**）+ `getCTPseudoInfoByHalf::Index`（sec 107，**4320 B = 540×8 B**）⇒ **540/540 恢复、0 未解析**；45 kinds × 6 LMUL × {plain,_MASK}，每项 `First` 是 `_BFloat16_` 名、`Second` 是其 f16 孪生。**并用厂商自己的 `RISCVInstrNameData/Indices`（17602 opcodes）把编号解成名字** ⇒ "厂商 opcode 编号对不上"这个阻塞**彻底消除**。
成员测试（反编译 verbatim）：`DemandedFields::BF16Fmt`（第 8 字节）= `hasSEWOp && SEW 操作数 == 4 && op ∈ (bf16 侧 ∪ f16 侧)`。
落地：10 族/13 kinds/**156 个 `_BFloat16_` 孪生** + pattern 分流（`VPNONBF16FloatVectors`/`VPBF16FloatVectors`），TSFlags bit 26 = `AltFmtType`，消费点 `transferBefore → encodeVTYPE → PseudoVSETVLI*`。**名字集 10/10 族 EXACT**（`tools/verify_bf16_names.py`）。
**诚实缺口**：厂商 45 kinds 中**还有 32 个未做**（`VM*` 掩码比较、`VFSQRT`、`VFRSUB`、`_ALIGNED_` 族、以及我们拼作 `_VFPR16` 的 `_VX` 形），逐个列在 `recon/bf16/name_set_check.txt`；**语料里都没被触发**，都是同一套机械扩展。
**③ 额外发现：fork 系统性地不做 `_I` 立即数折叠**（`0068`）：厂商探针证明 `vmv.v.x(0)`→`vmv.v.x vd,zero`、`vadd(+5)`→`li`+`vadd.vx`、`vfmv.v.f(0.0)`→`vmv.v.x vd,zero`。三处：整数 simm5 pattern、fpimm0 孪生（注释本来写 vmv.v.x 但 pattern 用了 `_I`）、**整数零要在 ISel 落 `$x0`**（否则两个零 splat 操作数不同名、MachineCSE 不合并）—— 第三处正是 `reduce_sum_f` 最后卡的那格。`PseudoVMV_V_I` 仍经 `BUILD_VECTOR` 可达（未删指令）。
**门槛（其树口径，含 in-build `0062`，不含 `0065`/第五条 hunk/`def EXEC`）**：golden-IR **39/54/7 → 64 IDENTICAL / 33 DIFF / 3 REJECTED**（25 格转移全 `→ IDENTICAL`、0 回退）；`.rc` oracle **4/18/81/11 → 15/7/81/11**（+11、OURS-FAIL 81→81 **0 回退**）；317 parity **136 → 148**（+12）、byte-diff 37→25、OURS-GAP 77 不变、`.c` 零缺口；musl own pre/post **48/56 结果一致、无 success↔fail**；厂商-vs-我方剥 `.comment` **24/56**。
**四条转发用的更正**：① `-tune-cpu andes-45-series` proxy **≠** `0062`；② `corpus_parity.py` **必须传绝对 `--cc`**（它 `cwd=ROOT`，相对路径会静默全 rc=127）；③ **`HasVendorXTsm82s` gating 保护不了 musl**；④ `musl_ab.py` 已加 `--strip-comment`。
**⚠️ 合并注意**：本树若合并第五条的 `def EXEC`，**寄存器编号会整体位移 ⇒ 必须重跑它那 100 格 golden-IR 基准再比较**。`RISCVInstrInfoVVLPatterns.td` 是它**唯一超出原授权**的文件（报告 §3 有理由与证据，与第五条无重叠）。

## 【方法学·级别的重要更正】"喂厂商 post-pass IR 回编译器"这条 oracle **被污染**（2026-10-17 04:00，第五条线发现，parent 采信）
**症状**：把厂商的 post-`simt2simd` module 喂回编译器时，**接收方的 `simt2simd` 会再跑一遍、把已经打包好的 kernel 签名二次打包**：
我方 `%2 = load ptr, ptr %1` → `%3 = load ptr, ptr %2`、`%6 = gep %2, 16` ⇒ MIR 里多出 `LD %4:gpr, 0` / `LW %5:gpr, 16`，而**厂商只有 `LW %4:gpr, 16`**。
**而且厂商自己也崩**：厂商 clang 喂它**自己的** post-`simt2simd` module ⇒ **segfault rc=139**（`TX8PromoteAlloca` → `getVoidTy`）。
⇒ **两边不是同一个输入**，所以该模式下产生的 DIFF **不能归因**给被测 pass；同理 A 线观测到的 `kernel.entry.exec ↔ vid` 顺序差**至少部分来自二次打包**。
**新规矩**：所有"喂厂商 IR"的 harness（`postsimt2simd_ab.py`、`golden_ir_ab.py`）**只能用于"能不能选出来 / 形状是什么"，绝不能用于逐字节判定**；其历史数字（含 E 线的 `64/33/3`）**一律标注为近似**。

**正解：MIR 回放 oracle（第五条线已搭好并验证）** —— `wt-exec/tools/mir_ab.sh` + `build_llc.sh`：
1. 厂商 clang `-mllvm -stop-before=riscv-insert-vsetvli -o x.pre.mir`；
2. 厂商 `llc -start-before=riscv-insert-vsetvli x.pre.mir` ⇒ **逐字节复现厂商 `.s`**（已验证：`vsetsimt 1 / lw a1,16(a0) / vsetvli a2,zero,e32,m1,ta,ma / vmsne.vv v9,v9,v9 / …`）；
3. 我方 `llc` 同参数 → 对比。
**它还差 4 处名字对齐**：(a) `0055`（`rpu_kernel` → `cc 124`，MIR 头内嵌 IR 必踩）；(b) **SR 表改名**（我方 `$sr0`/`SR` vs 厂商 `$s0`/`srf128`；**纯改名、编码/汇编不变**，但会让全项目 MIR 可比）；(c) 厂商有 `PseudoMOV128_S_V_B32` 伪指令层（我方把 `mov128.s.v` 直接选成真指令）；(d) 厂商 def 名 `NOR_S`/`AND_S`/`ISETEXEC`/… vs 我方 `XTsm_*`。
**parent 裁决**：**(a)+(b) 并入公共基线**（(a) 集成树已含；(b) 出独立补丁 `40xx-*`，做后必须重跑 musl ours-vs-ours 与 317 parity 且不回退，并在报告里单列改动清单）；**(c)+(d) 不在 `.td` 里对齐** —— 把 `XTsm_*` 改成厂商名会**改动全树指令名**，而每条线的补丁都引用 `XTsm_*` ⇒ 等于让今晚所有补丁作废；改为**在 `mir_ab.sh` 里用 sed 映射表做"比较时归一化"**并在报告里给出该表。
**第五条线本轮门槛（改动后重测）**：逐 intrinsic 探针 **7/7 rc=0 仍全选**；musl ours-vs-ours **46/56 + 10 = 56/56 outcomes、0 real difference**；golden oracle（100 格）**DIFF 56 / REJECTED 44、transition 仍 2 格（`control_nested`/`control_early_return` REJECTED-IR→DIFF）、无回退**；317 parity 与 vendor-vs-ours musl **本轮未重跑**（上一轮 139/0 transition、24/56）。

## 第五条线执行完四条裁决（`0069` = SR 改名独立补丁）+ 两个门槛在本 build 钉住（2026-10-17 04:15）
**门槛（本 build，未含 SR 改名）**：317 parity **byte-identical 139 / OURS-GAP 83 / both-fail 66 / byte-diff 28 / EXTRA-OK 1**，**vs 它自己的基线 0 transition**（基线 = `wt-raisa-be@23:55`，仅 0001..0012，**不含 0062/0063/0064/0065**）；musl **厂商-vs-我方（剥 `.comment`）24/56 逐字节 = 27/56 outcomes**（29 处差异全是既有 RVV 缺口，非其族）；musl **我方-vs-我方 56/56 outcomes、0 real difference**；golden（**近似口径，已按裁决降级**）DIFF 56 / REJECTED 44、transition 仍 2 格。
**SR 改名（`0069`，210 行 / 3 文件 / 19 hunk，增量：先 `0066` 再 `0069`）** —— 让 MIR 与厂商可比：
1. `RISCVInstrInfoXTsm82s.td`：`def SR#Index` → `def S#Index`（**RISCVReg 的 name 仍是 `"sr0"` ⇒ 汇编不变**）；`def SR` → `def SRF128` + `(sequence "S%u",0,31)`；**23 处指令操作数 `SR:$x` → `SRF128:$x`**（注释写明：**MIR 打 record 名、AsmPrinter 打 RISCVReg 名**，这正是厂商 `$s0` vs `sr0` 的由来）；
2. `RISCVISelLowering.cpp`：**仅那一行** `&RISCV::SRRegClass` → `&RISCV::SRF128RegClass`；
3. `Disassembler/RISCVDisassembler.cpp:97`：`RISCV::SR0 + RegNo` → `RISCV::S0 + RegNo`（**全树唯一一处按名字拼 SR 寄存器号的地方**，已 grep 出来一起改）。
**tblgen 自证**：`RISCVGen{RegisterInfo,InstrInfo,DisassemblerTables,DAGISel}.inc` 全部 rc=0，且 **`EXEC = 1`/`VL = 8` 不变**（厂商编号关系保住）、`S0 = 15`、`SRF128RegClassID` 就位 ⇒ MIR 现在会打 `$s0`/`srf128`。**重编（.td 触发的 63 目标级联）+ musl + parity 复测进行中**，两者不得回退。
**名字归一化进 `mir_ab.sh`（不动 `.td`，`MAPNAME` 表）**：`NOR_S/AND_S/OR_S/XOR_S/ANDN_S/ORN_S/NAND_S/XNOR_S`→`XTsm_*_SS`；`MOV128_S_V`→`XTsm_MV128_S_V`；**`PseudoMOV128_S_V_B32`→`XTsm_MV128_S_V`**（我方无伪层）；`MOV128_S_X`/`ISETEXEC{,0,1}`/`MOVEXECZ`/`VSET_SIMT`/`SET_MEM_ALLOC`→对应 `XTsm_*`；`Kcore_Ncc_Read`→`XTsm_KCORE_NCC_READ`；`srf128`/`$sN`→`sr`/`$srN`；`rpu_kernel void`→`cc 124 void`。⇒ 配合 `0055` 与 `0069`，**`tools/mir_ab.sh <cell>` 成为秒级、无 IR 污染的逐字节判据**（方法已自证：厂商 `llc -start-before=riscv-insert-vsetvli` 能逐字节复现厂商 `.s`）。

# ★ 集成完成（2026-10-17 04:30）：**parity 182 → 208、oracle 55 → 79 格、0 回退**
报告：`wt-merge/MERGE_REPORT.md`（日志在 `wt-merge/logs/`）。补丁序列：`0055→0054→0060→0063→0064→0066→0067→0068`（基树已含 `0001/0002`、`0047–0052`、`0053`、`0056–0059`、`0062`、`0065`）。**未改任何补丁文件、无 `.rej` 残留。**

**三处冲突（并集 + 机械残差）**：
1. **`0064` × `0053`（`RISCVInsertVSETVLI.cpp`）**：10/13 hunk reject（`0064` 的 pre-image 里 A 的 BF16 机制被剥掉）；两者是**同一个 bit**（`isAltFmt`/`isBF16Fmt` = `VType & 0x100`）。并集：`BF16Fmt` 与 `AltFmt` **都保留、在全部 9 个决策点取或**（usedVTYPE/demandVTYPE/`|=`/areCompatibleVTYPEs/getDemanded/VSETVLIInfo/getter/setVTYPE/encodeVTYPE/operator==/发射/transferBefore）。残差 = 恰好 5 行被改写为并集形式，其余 `0064` 新增行**逐字在位**。
   **⇒ 我准备的"表优先/属性兜底"仲裁规则不需要了**：实测 **0 回退、+26 新一致，且 15 个 bf16 `.rc` 格全部 asm+obj IDENTICAL、逐格厂商 MIR `PseudoVSETVLIX0 …, 335` 相同（15/15）** ⇒ **并集在语料上并不比厂商宽**（那条 2 行改动保留可用但无据）。
2. **`0066` 的 `.td` 尾 hunk × `0063`（同一 `@@ -4424,3` 插入点）**：并集 —— `0066` 的 98 行 EXEC 段追加到**真正 EOF**、在 E 的段之后；**`0066` 的 13 条 `def : Pat<` 逐条验证在位**（mov128.s.x/.v、8 条 `.s` 族、set.exec、raisa.divergent、get_kernel.entry.exec）；tblgen 通过、重生成 11 张 `RISCVGen*.inc`。
3. **注册（`RISCV.h`/`CMakeLists.txt`/`RISCVTargetMachine.cpp`）**：三条线全保留（3× `initialize…(*PR)` + 3× `addPass(create…)`）；管线顺序核对无误（`config-instr-opt` 在 `simt2simd` **前**、`rpu-lower-shared` 在**后**、`legalize-exec` 在 `addPostRegAlloc` 且 `isRaisa()` 门控、`riscv-copyelim` 之后）；`addRegisterClass(MVT::v1i128, &RISCV::SRRegClass)` 3 行 hunk **存活**。`EXEC = 1, VL = 8` 确认。

**⚠️ 扩展 stale sweep 抓到真活**（这是今晚第四次，且仍是同一族）：除 src-mtime 外还扫"记录依赖 mtime + 生成 `.inc` 级联"（对无依赖记录的对象限制在 `lib/Target/RISCV`）⇒ 找到 **6 个 `minibuild` 看不见的对象**，含 **`TX8TransformSIMTToSIMD.cpp.o`（A 的 `0065`）** 与包含 `0063` 的 `RISCVBaseInfo.h` 的 MCTargetDesc/TargetInfo 对象 ⇒ 重编 + 重归档 + relink（`clang-21` 03:12:11，**四道门槛都在其后跑**）。现在两套 sweep 都报 **0 stale**，9 个关键对象的 `.o` mtime > src 逐一核过。

**四道门槛（命令在报告里）**：
- **`.rc` oracle**：**158/228 slots = 79/114 格 IDENTICAL、0 diff**，48 ours-fail slots（24 格）、22 both-fail slots（11 格）。基线 A（`0062`+`0065`）= 110 slots/55 格 ⇒ **+24 格**；3 个 `config_*_device` diff 格因 `0060` 变 identical；**15 个 bf16 格全 identical**。
- **317 parity**：**byte-identical 208 / OURS-GAP 24 / byte-diff 18**（vendor-fail 33+33 抖动、ours-extra 1、**`.c` gap 0**）。vs 基线 182/47/21 ⇒ **+26 / −23 / −3**，**逐格 0 回退**（23 个 OURS-GAP→ident、3 个 byte-diff→ident、3 个厂商 ASLR 抖动）。**18 个 byte-diff 的归因：17 个 cosmetic（`-discard-value-names`）+ 1 个 codegen（`kcore_ncc_read`）**。
- **musl**（`musl_norm.py`）：raw 0/56；**剥 `.comment` 24/56 逐字节 + 4 同失败 = 28/56 outcomes**（bar ≥24 达标），23 处 DIFF 全是"双方都成功但字节不同"，**无 success↔fail**。
- **golden-IR**：**不可比（0/100/0）** —— 正如第五条线所料，**我方 `simt2simd` 对已打包签名二次打包**（合并树含 A 的 `0058` 无条件打包，E 的树没有），它在一份缓存 module 上直接复现了该现象（输入 `%2 = load ptr, ptr %1` → 我方 pass 输出多一层 `load ptr, ptr %2`，正是 harness 的第一行 diff）。**已标注为近似/仅形状**；唯一真信号是 **REJECTED-IR 3 → 0**（第五条线的格现在都能选）。
- **★ `mir_ab.sh` 机器管线 A/B（无污染）：6/6 格 `diff lines: 0`** —— `control_nested`、`control_early_return`、`mathdev_raisa_fadd_rn_bf16`、`mem_l1_size_1`、`abi_two_pointers`、`config_across_compute_device`。**这是整份报告里最强的逐字节断言。**

**★ 下一步杠杆（集成线给出，已派工）**：**剩余 24 个 parity gap / oracle 失败格全部死在 `RISCVISelLowering.cpp:14629 Don't know how to custom type legalize this intrinsic!`**，而那些格里经我方 `simt2simd` 之后**唯一残留的 RAISA 内建是 `llvm.riscv.read.reg.tid.x`（每格 2 条）** ⇒ **残余是 A 线 `simt2simd` 的 IR 缺口（非向量化块里的 `tid.x`），不是 ISel/EXEC/pseudo**；而 `mir_ab` 已证明 `mem_l1_size_1` 的**下游后端逐字节精确** ⇒ 修好这个 IR 缺口应当能把这批格直接转成 identical。

**未验证/边界（也在报告里）**：golden-IR 跨树不可比（只有 6 格 mir_ab 抽样了机器管线）；并集的 bf16 行为**未在"同函数混用 f16/bf16"**上测过、E 的 32 个未实现 bf16 kinds 语料未触发；17 cosmetic + 1 codegen 的 byte-diff 未追；厂商 ASLR 抖动仍在共享内存格上；`0062` 的 `-riscv-tx8-sched=true` 分支未走；`mir_ab` 比的是 asm 文本（那 6 格未比 `.o`）。

## 【诊断更正·重要】剩余 gap 的真凶是 **clang 前端**（不是 `simt2simd` pass）（2026-10-17 04:45，A4 证据，parent 采信）
**A4 的决定性证据**：我方 shared 内存格的 **pre-`simt2simd` IR 与厂商的 `before.ir` 完全不同** ——
厂商：`@_ZZ13mem_l1_size_1PjE7scratch = internal addrspace(2) global [1 x i32] undef, align 4`（**共享内存 global，AS2**）；
我方：`%scratch.sroa.0 = alloca i32, align 4, addrspace(5)`（**私有/局部 AS**）+ lifetime markers。
**在 `-O0`、同 `-cc1` 参数下**：厂商 clang 出 `@_ZZ1kPjE7scratch = internal addrspace(2) global [4 x i32] undef`，我方出 `%scratch = alloca [4 x i32], align 4, addrspace(5)`；**全局作用域同理**：厂商 `@g = external addrspace(2) global`，我方 `@g = global [4 x i32] zeroinitializer`（**无 AS**）。
**根因（已定位到我们树里的具体位置）**：
(a) `SemaDeclAttr.cpp::handleRAISASharedAttr` **只挂属性** —— 既不设变量类型的地址空间，也没做 CUDA 式的"隐含 static 存储"（`SemaDecl.cpp:8040-8047` 只覆盖 `CUDASharedAttr`/`CUDAConstantAttr`，**不含** RAISA 的 l0a/l0w/l1 属性）；
(b) 于是 `CodeGenModule::GetGlobalVarAddressSpace`（RISCV 用默认 = 类型的 AS）**永远看不到 AS 2/3/4**。映射表其实已存在（`Attr.td`/`RISCV.h` 把 `raisa_{l0a,l0w,l1}shared` 映射到 **AS 3/4/2**）。
**后果**：24 格里有 **13 格**（`mem_l1_size_*` 6 格 + `mem_*shared_u{32,64}_uniform` 6 格 + 与 `alloc_*` 的重叠）**无法只靠 `TX8TransformSIMTToSIMD.cpp` 修** —— 变量变成 AS5 alloca 后 **L0A/L0W/L1 的身份就丢了**（三族都变成逐字节相同的 alloca），pass **无法恢复**共享基址（`0x40000000`/`0x40100000`/`0x101000000`）。另：`mem_*shared_u8/u16_uniform` 是**厂商自己坏**（`vector.insert` 类型非法）⇒ both-fail，不是目标。
**其余 5 格**（`control_{divergent,divergent_loop,early_return,nested,uniform_loop}`）无共享变量 ⇒ 是 A4 文件内真正的**多 BB 向量化**工作。
**⚠️ 另一处文档更正（A4 指出）**：`recon/simt2simd/*.after.ir` 对 shared 格其实是 **post-`rpu-lower-shared`**，**不是** post-`simt2simd`；真正的 post-`simt2simd` IR 在 **`wt-raisa-r2/recon/rpu_lower_shared/*.in.ll`**（**常量表达式形态**，如 `vmv.v.x(undef, i32 ptrtoint (ptr addrspacecast (@scratch) to i32), 128)` + `lshr(ptrtoint(...),32)` + `vslh` + `vsuxei.sync`），而 `*.after.ir` = 该 module 经 `0054` 把常量表达式物化成 `inttoptr` 链之后的样子 ⇒ **`simt2simd` 必须产出"常量表达式形态"**，与 `0054` 的分工是干净的。
**parent 裁决**：**批准 A4 的 A/B/C 三步**（A = 两个前端 hunk：Sema 属性→类型 AS + 隐含 static；CGDecl 的 undef 初始化镜像；B = `simt2simd` 的共享 global 访问规则，目标 = `rpu_lower_shared/*.in.ll` 逐字节；C = `control_*` 多 BB 族，目标 = `control_*.after.ir` 逐字节），条件见下方派工消息（RAISA-gated、musl/.c 逐格 success/fail、不碰其它线）。

## A 线第 4 轮（simt2simd IR 缺口，patch `0015`）：13 格从"编译失败"变成"能编译"，**门槛全过、0 回退**（2026-10-17 04:0x）
**补丁**：`wt-merge/patches/0015-raisa-simt2simd-shared-addressing-and-control-family.patch`（**5 文件 / 25 hunk / 937 行**；已用 `patch -p1` 对 pre-image 回放，**逐字节复现** `wt-merge/src`）。归档同 `wt-raisa-be/patches/0015-*.patch`。

### 结果（四道门槛，同一二进制 `wt-merge/build/bin/clang-21`）
| 门槛 | 合并基线 | 本轮 | 判定 |
|---|---|---|---|
| `.rc` oracle `--all` | 158/228 slots ident（79 格）/ 0 diff / **48 ours-fail** / 22 both-fail | **158 / 0 / 26 / 22** | 79 格无回退；ours-fail 槽 48→26，13 格新转"能编译" |
| 317 parity | 208 ident / **24 OURS-GAP** / 18 byte-diff / 33+33 vendor-fail / extra 1、`.c` gap 0 | **208 / 11 / 31 / 33+33 / 1**、`.c` gap 0 | **0 反向**：恰好 13 格 `OURS-GAP → byte-diff` |
| musl 厂商-vs-我方（`musl_norm.py`） | raw 0/56+4；**剥 `.comment` 24/56 + 4 = 28/56** | 同左（24/56+4） | 达标 ≥24，**无 success↔fail** |
| musl **我方-vs-我方** | —（合并时的 `parity/ab_norm/b_*.o` 已先存档） | **47/47 `.o` 逐字节相同** | 前端+管线改动对 musl **零影响**（实证，不是推断） |
| `mir_ab.sh` | 集成线报"6/6 diff 0" | **该结论作废**（见下） | 用修正版重放：`control_divergent` 仅 10 行**物理寄存器染色**差异 |

**13 个新转"能编译"的格**：`mem_l1_size_{1,3,127,128,129,256}`、`mem_{l0a,l0w,l1}shared_u32_uniform`（9 格 `asm+obj` 都出得来，`asm:DIFF obj:DIFF`）、`mem_{l0a,l0w,l1}shared_u64_uniform`、`control_divergent`（4 格 `asm:DIFF`，`obj` 仍失败——见"下游阻塞"）。

### 【最大发现·前端缺口】RAISA tile-memory 变量在**前端**就丢了地址空间（不是 pass 的问题）
厂商 `-O0`（同一 `-cc1` 参数）：`@_ZZ1kPjE7scratch = internal addrspace(2) global [4 x i32] undef, align 4`；我方（改动前）：`%scratch = alloca [4 x i32], align 4, addrspace(5)` —— **零 pass 参与，纯前端**。同样地，文件作用域 `l1shared` 变量厂商给 `external addrspace(2) global`，我方给 AS0 `global zeroinitializer`。
根因两处：① `SemaDeclAttr.cpp::handleRAISASharedAttr` 只挂属性，**既不设类型 AS、也不给 CUDA 那样的"隐含 static 存储"**（`SemaDecl.cpp:8040-8047` 的规则只覆盖 `CUDASharedAttr`/`CUDAConstantAttr`）；② `CodeGenModule::GetGlobalVarAddressSpace` 没有 RAISA 分支（RISCV 用默认实现 = 读类型的 AS，所以永远拿不到 2/3/4）。`Attr.td` + `Targets/RISCV.h` 的 AS 映射（l1shared=2 / l0ashared=3 / l0wshared=4）**本来就在**。
**后果（为什么 13 格在原理上修不了）**：变量一旦变成 AS5 alloca，**L0A/L0W/L1 身份被抹平**（三者字节相同），pass 无法恢复共享基址（0x101000000 / 0x40000000 / 0x40100000）。
**证据（改动后）**：我方 `-print-before=simt2simd` 与厂商 `recon/simt2simd/*.before.ir` 的 **body 逐字节相同 12/13**（6 `mem_l1_size_*` + 6 `mem_*shared_u{32,64}_uniform` + `alloc_l1shared_base` 中 12 个 SAME；`alloc_l1shared_base` 只因 `global-merge` 差异 DIFF）。

### 【自我更正·三条】先做错再纠正的记录（parent 要求单列）
1. **把地址空间放在"类型"上 → `ItaniumMangle.cpp:2800 "Not a language specific address space"` abort。** 第一版把 `VD->setType(getAddrSpaceQualType(...))` 当作正解；一编译 `getMangledName` 就撞 mangler 的 AS switch。**正解**：照 CUDA 的做法——类型不动，只加 `CodeGenModule::GetGlobalVarAddressSpace` 分支（厂商的 mangled 名 `_ZZ...E7scratch` 也不含 AS，与该结论一致）。
2. **把共享地址链做成"每变量一次" → 与厂商 live dump 不符。** `rpu_lower_shared/*.in.ll` 里同一变量只有一个地址链，但那已经是 **early-cse 之后**的形态；厂商真正的 `-print-after=simt2simd` 里 **load 自己一条链**。⇒ 必须**每次访问各建一条**（CSE 由后面的 early-cse 做）。**教训：`rpu_lower_shared/*.in.ll` 是"post-simt2simd + early-cse"，不是 post-simt2simd 本身；判据只能用 `-print-after=simt2simd` 的 live dump。**
3. **构建工具陷阱（新的一条）**：`wt-merge/logs/changed_inc.json` 保留了上一次合并构建的 `.inc` 列表，`minibuild.py --steps objs,...` 会**重新编译整个 RISCV target**（63 目标级联）。**把该文件置 `[]`（或先删）再跑**；否则会误以为是"我的改动"在重编。另：`TX8TransformSIMTToSIMD.cpp.o` 在 `.ninja_deps` 里**没有条目**（minibuild 自己编的），`--force-src` **扫不到它**，必须用 `tools/rebuild_one.sh`。

### 【管线缺口】`simt2simd` 与 `rpu-lower-shared` 之间缺一个 `early-cse`
厂商 `-mllvm -debug-pass=Structure`：`… -tx8-annotate-control-flow -simt2simd -domtree -early-cse -rpu-lower-shared -domtree …`；我方（改前）：`… -simt2simd -rpu-lower-shared …`（背靠背）。
**可观测**：改前我方 `-print-after=rpu-lower-shared` 里 ① 第一个参数解包仍带 `getelementptr i8, ptr %0, i32 0`（厂商已折成 `load ptr, ptr %0`），② 共享地址链**重复两份**（厂商一份）⇒ vreg 编号/染色与厂商不同（`mem_l1_size_1` 的 `li a2,128` 位置与 `a1/a2` 互换）。加上 `addPass(createEarlyCSEPass())`（RAISA 块内）后，我方 `-print-after=rpu-lower-shared` 与厂商 **body 逐字节相同**。

### 【pass 规则（patch 0015，只动 `TX8TransformSIMTToSIMD.cpp` + 上述 4 个文件）】
1. **tile-memory 访问**（`addrspacecast (ptr addrspace(2|3|4) @g to ptr)`，offset 0）：→ `lshr/trunc(ptrtoint(addrspacecast(@g) to i64))` + `vmv.v.x(i32 ptrtoint(addrspacecast(@g) to i32))` + `vslh.nxv2i64.…i32.i64` + `vsuxei.sync/vluxei.sync …p0.nxv2i64.i64(<value>, ptr null, idx, 128)`。**常量表达式**由 `0054 rpu-lower-shared` 物化成 `inttoptr` 链（分工与 parent 裁决一致）。**每次访问一条链**。
2. **i64 分块**（`mem_*shared_u64_uniform`）：store 拆成两条 i32 scatter（值 splat 全在前、地址链用 `vadd(low, 4)` 复用同一 high），load 只取低 i32（后续 `trunc` 被丢弃）。
3. **control 族（`vi`/掩码）**：`and/mul/add %vec, 常量` → `vand/vmul/vadd.<lane>.i32.i64(undef, vec, C, 128)`；`icmp eq/ult` → `vmseq/vmsltu`（结果 `nxv2i1`）；`select i1 %mask, A, B` → `vmerge(undef, B, A, mask, 128)`；`get.pred.mask(i1 %cmp)` → `mov128.s.v.nxv2i1(<lane mask>)`；`raisa.get.kernel.entry.exec`/`mov128.s.v`/`nor.s`/`and.s`/`set.exec`/`raisa.divergent` **原样保留**。
4. **多 BB**：改成逐 BB 重写（保留 CFG）；**但控制流必须保持 uniform**——分支条件若传递依赖被向量化的值就**整体 bail**（`dependsOnVectorised`）。这条正是 `control_nested`/`control_early_return` 在我方 bailing 的原因（见下）。

### 每格归属（16 格仍有问题，阻塞点全部点名）
- **`mem_l1_size_*`(6) + `mem_*shared_u32_uniform`(3)**：**IR 与厂商逐字节相同**（`-print-after=simt2simd` 9/9 SAME，含 `-print-after=rpu-lower-shared`），asm/obj 仍 DIFF ⇒ **阻塞 = `PseudoVSLH_ALIGNED_VX_M2`（`*_ALIGNED_*` 族，我方 TD 0 命中）+ 厂商 AVL 走立即数**。我方选 `PseudoXTsm_VSLH_VX_M2`（AVL 是寄存器 `%2:gprnox0`）⇒ 多一条 `li a1,128`、vsetvli 归属不同、vreg 染色不同。**属 E 线（已另派 `wt-merge-e`）**。
- **`mem_*shared_u64_uniform`(3) + `control_divergent`(1)**：IR 逐字节相同，`asm:DIFF`，`obj` 死在 **MC 层 `Unsupported RVV instruction … funct3=0b011`**（OPIVI）：我方 ISel 折成 `vadd.vi/vand.vi/vmseq.vi`，厂商是 `li` + `.vx`（README 第 824 行已登记的"fork 不做 `_I` 折叠"，E 线范围）。
- **`alloc_*`(7)**：**不是 pass 能修的**。我方 `global-merge`（`RISCVPassConfig::addPreISel`）在 `simt2simd` **之前**就把两个 AS2 全局并成 `@_MergedGlobals`（+alias），而厂商的同名 pass 不并它们（其 `-print-before=simt2simd` 仍是 `@…E1a`/`@…E1b`）。⇒ pass 看到的 b 访问是 `gep @_MergedGlobals, 0, 2`（非零常量偏移），按"不确知就 bail"的纪律整函数不动。**已批准的修法（下一轮，未做）**：在 `GlobalMerge` 的收集循环里对 RAISA 目标跳过非默认地址空间的 globals（`TM->getTargetTriple().isRaisa() && AddressSpace != 0 → continue`），或 RAISA 时不在 `addPreISel` 加该 pass。**另一个独立问题是"每变量地址物化顺序"**：厂商 `alloc_l1shared_base` 的 after IR 先物化 **b** 再 **a**（非声明序，README 早已登记为指针键容器/`VENDOR-NONDET`）⇒ 即使管线修好，这 7 格也应登记为"顺序待定"，不要为了逐字节去猜。
- **`control_nested`/`control_early_return`(2)**：**我方 pre-IR 与厂商差一整套控制流注解**——厂商 `before.ir` 有 `raisa.get.kernel.entry.exec`→`mov128.s.v`→`nor.s`→`get.pred.mask(icmp)`→`and.s`→`raisa.divergent`→`icmp eq i64`→`br`，我方只有 `%cmp = icmp ult i32 %tid, %n; br i1 %cmp`（**lane 相关分支**）。厂商在 `simt2simd` **之前**多一个 **`tx8-annotate-control-flow`**（+ `rpu-unify-divergent-exit-nodes`/structurizecfg）我们没有 ⇒ 这不是 IR pass 能补的；`dependsOnVectorised` 让这两格**安全 bail**（保持 OURS-FAIL，不产生"对向量值分支"的非法 IR）。
- **`control_{uniform,divergent}_loop`(2)**：循环/展开体（多 phi、`get.pred.mask` 变体），本轮不做。

### 【方法学·重要更正】`tools/mir_ab.sh` 目前的判定是**空转**（集成线的"6/6 diff lines: 0"作废）
两个独立原因：① 它的 `MAPNAME` 用 BRE 括号 `s/\$s([0-9]+)/\$sr\1/g`，GNU sed 直接报 `非法引用 \1` ⇒ `norm_vendor` 输出为空；② 更根本的是 step [4/4] 的**我方 llc 从来没写出 `*.ours.llc.s`**：厂商 pre-vsetvli MIR 里是厂商指令名（`SET_MEM_ALLOC`/`VSET_SIMT`/`$s0`/`srf128`，以及 `PseudoVSLH_ALIGNED_VX_M2`），我方 MIR 解析器只认 `XTsm_*`/`$sr0`/`sr`——**名字映射必须作用在"喂进去的 MIR"上，而脚本只映射了输出 asm**。⇒ 两侧都是空文件，"0 diff" 是假绿；`mem_l1_size_1`（也就是被引用来论证"下游已经逐字节精确"的那一格）**恰恰因为 `PseudoVSLH_ALIGNED_VX_M2` 连解析都过不去**。
**修正后的重放（把映射应用到输入 MIR）**：`control_divergent` 我方 llc 能跑通，与厂商 llc 同输入 MIR 的 asm **差 10 行，全部是物理寄存器染色**（`vand.vx v0/v3`、`vmul.vx v8/v1` 之类；指令序列、立即数、vsetvli 全同）；`control_nested` 映射后仍因厂商专有伪指令解析失败。⇒ "下游逐字节精确"这个前提**没有被证实**（对 `fadd_*` 那批间接成立，因为端到端已逐字节），任何据此推出的结论都要重算。

### 下一步（按优先级，都已点名）
1. **E 线**：`*_ALIGNED_*` 伪指令族（`PseudoVSLH_ALIGNED_VX_*`，AVL 立即数）+ `_I` 折叠抑制（`vand.vi/vmseq.vi/vadd.vi` → `li`+`.vx`）+ `implicit $mem_alloc` 位置（我方 sync 访存多带了它，厂商只在 `PseudoVSE32_V_M1` 上）。做完后本轮的 13 格（`obj`）与 9 格（`asm`）应能继续转 identical。
2. **A 线下一轮**：`alloc_*` 的 `global-merge` RAISA 跳过（一行 hunk + 重跑门槛）；`tx8-annotate-control-flow` 的重建（2 格 + 循环格的前置）。
3. 修 `tools/mir_ab.sh`（映射输入 MIR + `sed -E`），否则它给的所有"0 diff"都不可信。

# ★★ 【必须撤回】集成线的 `mir_ab.sh` "6/6 格 0 diff" **是空文件对空文件**（2026-10-17 05:00，A4 发现，parent 采信）
**撤回对象**：本文件上面"`mir_ab.sh` 机器管线 A/B（无污染）：6/6 格 `diff lines: 0` —— 这是整份报告里最强的逐字节断言"**整段作废**。
**原因（两条，任一致命）**：
(i) 它的 `MAPNAME` 用了 BRE 括号（`s/\$s([0-9]+)/\$sr\1/g`）⇒ GNU sed 直接 abort，`norm_vendor` 产出为空；
(ii) **更决定性**：我方 `llc` **根本没写出 ours 侧文件** —— 厂商的 pre-vsetvli MIR 带的是**厂商**指令名（`SET_MEM_ALLOC`、`$s0`、`srf128`、`PseudoVSLH_ALIGNED_VX_M2`），而那张映射表**只用在了输出 asm 上、没有用在喂进去的 MIR 上**。于是两侧都是空文件。
⇒ **特别是 `mem_l1_size_1`（集成线用来宣称"下游后端已逐字节精确"的那格）根本没法被我方 llc 解析**（`unknown machine instruction name 'PseudoVSLH_ALIGNED_VX_M2'`）。
**A4 重做了正确的版本**（映射**输入** MIR、用 `sed -E`）：`control_divergent` 在**同一份输入 MIR** 上与我方 llc 对比 ⇒ **差 10 行，全是物理寄存器着色**（`vand.vx v0` vs `v3`、`vmul.vx v8` vs `v1`；序列/立即数/vsetvli 相同）。
⇒ **"下游后端逐字节精确"这条并未成立**；任何依赖它的结论都要重算（**`mir_ab.sh` 必须先修好再引用**）。

## A4 终报（补丁 `0070`，5 文件 / 25 hunks / 937 行，回放验证逐字节）：**24 格中 13 格现在能编译**
- **9 格到 `asm:DIFF obj:DIFF`**（`mem_l1_size_{1,3,127,128,129,256}`、`mem_{l0a,l0w,l1}shared_u32_uniform`）；**4 格到 `asm:DIFF`**（`mem_*shared_u64_uniform`、`control_divergent`，`obj` 仍卡在 MC 层）。
- **四道门槛（同一二进制，03:47:04 链接，0 stale）**：
  - `.rc` oracle `--all`：**158/228 slots = 79 格 ident / 0 diff / 26 ours-fail slots / 22 both-fail** vs 基线 `158/0/48/22` ⇒ 79 格未动、**ours-fail slots 48→26**（"两槽都失败"的格 24→11；"任一槽失败"的格 24→15）。
  - **317 parity**：**byte-identical 208（不变）/ OURS-GAP 24→11 / byte-diff 18→31**、vendor 33+33、ours-extra 1、**`.c` gap 0**；transition = **恰好 13 × `OURS-GAP → byte-diff`，0 反向**。
  - musl 厂商-vs-我方（`musl_norm.py`）：raw 0/56+4；**剥 `.comment` 24/56 + 4 同失败 = 28/56 outcomes**（达标）、**无 success↔fail**。
  - musl 我方-vs-我方：改前二进制被原地覆盖 ⇒ 它用集成时**记录的** `b_*.o` 与新 `b_*.o` 比：**47/47 逐字节相同** ⇒ **零 musl 影响（经验证据，不只是"按构造 gated"）**。
- **实现**：① 前端 2 文件 3 hunk（RAISA 共享变量获得隐含 static 存储期 + `GetGlobalVarAddressMap` 映射三属性到 `raisa_{l0a,l0w,l1}shared` + `CGDecl` 去初始化器）—— 证明：我方 `-print-before=simt2simd` body 与厂商 `before.ir` **12/13 格逐字节一致**，`-O0` IR 也逐字一致；② pass（共享寻址常量表达式链 + `i64` 重切成两条 `i32` scatter + `control` 族的 `vand/vmul/vadd/vmseq/vmsltu/vmerge` + 多 BB 重写且**硬规则：控制流必须保持 uniform**，依赖向量化值的分支整体 bail）；③ 管线 1 hunk（`createEarlyCSEPass()` 插在 `simt2simd` 与 `rpu-lower-shared` 之间，与厂商 `-debug-pass=Structure` 一致）—— 效果：`-print-after=rpu-lower-shared` body 变**逐字节一致**。
- **剩余 16 格的精确阻塞**：**9 u32** = 需 `*_ALIGNED_*` 族 + 立即数 AVL（**E 线**，我派了 `wt-merge-e`）；**3 u64 + `control_divergent`** = 需抑制 `_I` 折叠（我方把 `vadd/vand/vmseq` 折成 `.vi`，厂商发 `li`+`.vx`，MC 报 `Unsupported RVV instruction … funct3=0b011`）（**E 线**）；**7 `alloc_*`** = 我方 `global-merge` 在 `addPreISel`（早于 `simt2simd`）把两个 AS2 global 并成 `@_MergedGlobals`，厂商不会 ⇒ **一行 RAISA-gated 跳过**（A4 **故意本轮没做**：单独做翻不了任何格，且会作废刚跑完的四道门槛；已在 README 完整规格化）；**2 `control_nested`/`early_return`** = 我方 pre-IR **缺整段 divergence 注解**（`get.kernel.entry.exec`→`mov128.s.v`→`nor.s`→`get.pred.mask(icmp)`→`and.s`→`raisa.divergent`→`icmp`→`br`）⇒ 缺的是 **`tx8-annotate-control-flow`**（+`rpu-unify-divergent-exit-nodes`/structurizecfg），**不是 IR pass 缺口**；**2 `control_*_loop`** = 展开的循环体，未尝试。
- **它的四条自我更正**（README §"自我更正·三条"）：① 把 AS 放在**类型**上会 abort（`ItaniumMangle.cpp:2800`），正确落点是 CodeGen 钩子；② "每变量一条地址链"错（`.in.ll` 是 post-early-cse；live dump 是**每次访存一条**）；③ 构建工具陷阱：`wt-merge/logs/changed_inc.json` 残留会让 `minibuild` 静默计划全量重编，且 `TX8TransformSIMTToSIMD.cpp.o` 无 `.ninja_deps` ⇒ 必须用 `rebuild_one.sh`；④ `mir_ab.sh` 的"0 diff"是空文件（见上）。

# ★★ E 线：9 格全部转 `asm:IDENTICAL obj:IDENTICAL`（2026-10-17 05:20）
**翻绿**：`mem_l1_size_{1,3,127,128,129,256}`、`mem_l1shared_u32_uniform`、`mem_l0ashared_u32_uniform`、`mem_l0wshared_u32_uniform` = **9/9 IDENTICAL（18/18 slots）**，0 diff / 0 ours-fail（`raisa_oracle.py` 绝对 `--cc`，`wt-merge-e` 树；sweep 0 stale、三个关键对象 mtime > src）。
**根因不是"ISel 选不出 ALIGNED"，而是两件事，都按厂商证据做实**：
1. **AVL 必须是立即数（RAISA 专属）**：`RISCVISelDAGToDAG.cpp::selectVLOp` 加"`isRaisa()` 时任意常量 AVL → TargetConstant"。证据：**同一份源码**的 musl 路径给 `%n:gprnox0 = ADDI $x0,128` + 寄存器，**RAISA 路径给立即数**（`PseudoVSLH_VX_M2 …, 128, 5, 0`；VID/VMV/VLE/VSE 同样）⇒ **triple 门控**（`HasVendorXTsm82s` 在 musl 上恒真、无判别力）。改后我方多余的 `li a1,128` 提前调度消失、vsetvli 形态与厂商一致。
2. **`vreg-align` pass + `*_ALIGNED_*` 族（E 线重建了 pass 本身）**：厂商机制（从 `RISCVTargetMachine.cpp.o` 的重定位 + 反编译读出）：`addPreISel()` 里 `insertPass(&TwoAddressInstructionPassID, &TX8RegisterAlignmentPassID)`（`-riscv-vreg-align` 默认开），执行序 `-twoaddressinstruction -vreg-align -slotindexes -liveintervals -register-coalescer …`。语义（`llc -run-pass=vreg-align` 单跑实证）：把 `PseudoVSLH_VX_M2` 的 `$rs2` 换成 `*_ALIGNED_*` 孪生（操作数类**宽一档 LMUL**）并插 `undef %14.sub_vrm1_0:vrm2 = COPY killed %4`；条件 = 反编译的 **`getLMul(TSFlags)-1 < 3`（只 LMUL_2/4/8）**。我方实现与厂商**逐行同形**：`undef %14.sub_vrm1_0:vrm2 = COPY killed %4:vr` + `early-clobber %6:vrm2 = PseudoXTsm_VSLH_ALIGNED_VX_M2 undef %6(tied-def 0), killed %14:vrm2, killed %5:gpr, 128, 5, 0`。**名字集 24/24 与厂商 `PseudoVSLH_ALIGNED_*` diff 空**（全量 1208 名中其余属别的 op、语料未触发；它在做 114 格 ALIGNED 使用普查）。
**改动文件（全部 additive）**：新 `TX8RegisterAlignment.cpp`；`RISCVInstrInfoXTsm82s.td`（ALIGNED multiclass）、`RISCVInstrInfoVPseudos.td`（affix 表加 `["_ALIGNED",""]`）、`RISCV.h`/`CMakeLists.txt`/`RISCVTargetMachine.cpp`（init + `cl::opt` + `insertPass`）。构建走派生 build（`tools/build_e.sh`），`.cpp` 级 ~1 分钟、`.td` 级联 67 对象 ~10 分钟。

## 【parent 和解】C 线的"`vreg-align` 是 MIR no-op" **与** E 线的"`vreg-align` 真实存在" **两者都对**
C 线测的是那 **9 个 `fadd_{rd,ru,rz}_{f,f16,bf16}` + 7 个 `id_*`** 格；E 线读出厂商条件是 **`getLMul(TSFlags)-1 < 3`，即只对 LMUL_2/4/8 生效**。
⇒ 那 9+7 格里多数是 **LMUL_1** ⇒ 在它们上 `vreg-align` **确实是 no-op**（C 线结论成立）；而 `mem_l1_size_*`/`mem_*shared_u32_uniform` 用的是 **LMUL_2** ⇒ ALIGNED 重写生效（E 线结论成立）。
**教训（写下来免得再吵）**：一个 pass 是否 no-op **必须连"它的触发条件"一起报**——"在我抽样的格上是 no-op"不等于"这个 pass 是 no-op"。

# ★★★ 收官盘点（2026-10-17 05:30）：**parity 217 逐字节 / gap 12；oracle 88/114 格逐字节；0 回退**
E 线收官补丁：**`0071`**（`*_ALIGNED_*` 族 + `vreg-align` pass 重建 + RAISA 立即数 AVL）、**`0072`**（`_SYNC_` 去掉 `implicit $mem_alloc`）、**`0073`**（`_I`/`.wi` 折叠抑制，新 TD 谓词 `IsNotRaisaDevice` 门控 6 个 `_VI` pattern 类）。三补丁按序回放 = 逐字节复现 `wt-merge-e/src`（10/10 文件）。报告：`docs/compiler/ISEL_ALIGNED_VREG_ALIGN_1014.md`。

**最终门槛（E 线最终二进制，采数前 sweep = 0 stale）**
| 门槛 | 今晚起点 | 合并基线 | **最终** |
|---|---|---|---|
| **317 parity 逐字节** | 132 | 208 | **217**（gap 93 → **12**；diff 21） |
| **`.rc` oracle 逐字节** | 0 | 79/114 | **88/114 格**（176/228 slots；diff 3；ours-fail 27 slots；0 non-det） |
| musl 厂商-vs-我方（剥 `.comment`） | 24/56 | 24/56 | **24/56 = 28/56 outcomes**（不变） |
| musl 我方-vs-我方 | — | — | **56/56 outcomes、0 real difference** |
| **`mir_ab.sh`（已修）** | — | （空文件假象，已撤回） | **9/9 格 `diff lines: 0`（两侧非空、31 行）** |
| 回退 | — | — | **两条口径均 0 回退** |

**E 线的三条根因/工具更正**
1. **RAISA 常量 AVL = 立即数**（`selectVLOp` 里 `isRaisa()` 门控）—— 同源码 musl 路径给寄存器、RAISA 给立即数。
2. **`vreg-align` pass**（`addPreISel` 里 `insertPass(&TwoAddressInstructionPassID, &TX8RegisterAlignmentPassID)`；条件 `getLMul(TSFlags)-1 < 3` 即只 LMUL_2/4/8；subreg 常数 6/14/18 与厂商一致）；**114 格厂商 MIR 普查显示全语料只用 `PseudoVSLH_ALIGNED_{VX,VV}_M2`（21+1 格）** ⇒ 语料需求已覆盖（其余 1184 名属未触发 op 族）。
3. **`mir_ab.sh` 三个 bug 已修**（`sed -E`；**名字映射也作用于输入 MIR**；两侧产物断言非空，失败报 `HARNESS FAILED`），并新增 **`tools/mir_sync_swap.py`** —— 因为**我方 `_SYNC_` 记录的操作数序是 `(vs2, index, base)`，而厂商/上游是 `(vs2, base, index)`**（ISel 里靠对调补偿，是**已登记的复建差异**）⇒ 喂厂商 MIR 前必须换回。方法自证：厂商 llc replay 复现厂商 clang 的 asm（仅差 MIR 不含的 3 行 `.L…$local` 标签）。
4. **`.vi`/`.wi` 抑制的效果是"部分"**：厂商 237 个 `.s` 里 **0 条 `.vi`/`.wi`**；我方 3 格发 `vadd.vi`（obj 死在 MC `funct3=0b011`）⇒ 抑制后 **3 个 `mem_*shared_u64_uniform` 从 `obj:OURS-FAIL → obj:DIFF`**（**对象能编了**），**0 格翻绿、0 回退**。残余 = **单个物理寄存器**：两侧 pre-vsetvli MIR 完全相同（第二个 `vslh` 都是 `$v8m2`），厂商最终 asm 是 `vslh.vx v2, v10, a2`、我方留 `v8` ⇒ **差异在 post-RA（vreg-rename 层），不在 ISel**。`control_divergent` 未变（仍更早在 `RISCVISelLowering.cpp:14629` 死）。

**剩余 12 gap / 27 ours-fail slots 的归属（逐条点名，无猜测）**
- **7 `alloc_*`**：需一行 **RAISA-gated 跳过 `global-merge`**（我方把它放在 `addPreISel`、早于 `simt2simd`，把两个 AS2 global 并成 `@_MergedGlobals`；厂商不并）+ **每变量物化顺序**（已登记为 pending/可能 nondet）。
- **3 `mem_*shared_u64_uniform`**：只差 **1 个 post-RA 寄存器**（见上）。
- **2 `control_nested`/`control_early_return`**：需缺失的 **`tx8-annotate-control-flow`** 整段 divergence 注解（+`rpu-unify-divergent-exit-nodes`/structurizecfg）⇒ **需要再逆向一个 pass**（成本最高）。
- **2 `control_{divergent,uniform}_loop`**：展开循环体，未尝试。
- 其余为厂商自身的 fail/抖动（ASLR、`_u8/_u16` 族的厂商坏 IR）。

# ★ `tx8-annotate-control-flow` 重建（补丁 `1017`/`1018`，2026-10-17 10:0x，control-flow 线）
**结论：pass 本身已重建并**逐字节**复现厂商的 pre-`simt2simd` IR（`control_nested`/`control_early_return` `EXACT-IR`）；两个格从 `OURS-FAIL`（ISel abort）变成能编译到 asm（`asm:DIFF`）。逐字节仍差 **机器块布局 + 2 个 vreg**（见下）。先读两条"前置事实"，否则会重复踩。**

## 前置事实 ①：`wt-merge-e` 缺 **`0070` 的 pass 那一半**（只有它的前端/管线那一半）
`wt-merge-e/src/.../TX8TransformSIMTToSIMD.cpp`（1241 行）里赫然是 **`if (F.size() != 1) return false;  // Only single-basic-block kernels are handled for now.`** —— 即 **pre-0070** 版本；而 `0070` 的 clang 前端 hunk（`GetGlobalVarAddressSpace` 的 RAISA 分支）与 `RISCVTargetMachine.cpp` 的 `early-cse` 接线**都在**。`wt-merge` 的同名文件（1588 行）才是 0070 之后的（多 411 行 / 改 64 行，含多 BB 重写 + `vectorBinOpName`/`vectorCmpName`/`isVectorConstOp`/`isLaneCmp`/`isLaneSelect`/`get.pred.mask` 规则）。
⇒ **不补这一半，annotation 造出来的多 BB IR 会被 `F.size()!=1` 直接拒掉**，`control_*` 永远不可能动。补丁 `1017`（649 行 / 1 文件 / 21 hunk）= 把 `wt-merge` 的该文件搬回（E 线的 `1014/1015/1016` 都不碰它，`diff` 已核）。**教训：`wt-merge-e` ≠ "base + 0070 + 0071..0073"，交接前请按文件 diff 验证，不要按报告顺序假设。**

## 前置事实 ②：本 build 目录有 **8 个比 `RISCVGen*.inc` 还老的对象**，造成**opcode 枚举错位**（比"stale"更毒）
生成时间：`.inc` = 04:04，而 `TX8LegalizeEXEC.cpp.o` = **03:07**。症状**不是**"没生效"，而是**指令被印成别的指令**：
`raisa.divergent` 的 `legalize-exec` 展开本应出 `isetexec`/`movexecz`/`kcore_ncc_read`，实际 asm printer 前 MIR 是
`XPERM8 killed $sr1` / `re def $x11 = XTsm_LSU_SET_LENGTH`（def/uses 与指令表完全不符）⇒ AsmPrinter 里
`SmallVector<MCOperand>::operator[] idx < size()` **断言崩溃**。
**8 个**：`TX8LegalizeEXEC.cpp.o`、`TX8LowerSharedPass.cpp.o`、`TX8OptimizeConfigInstrs.cpp.o`、`RISCVConstantPoolValue.cpp.o`、`RISCVSelectionDAGInfo.cpp.o`、`MCTargetDesc/TX8MCInfo.cpp.o`、`MCTargetDesc/RISCVMCAsmInfo.cpp.o`、`TargetInfo/RISCVTargetInfo.cpp.o`。
`tools/sweep_stale_objects.sh` **没报它们**（它只扫到 `TX8TransformSIMTToSIMD.cpp.o`）⇒ **新判据：`find lib/Target/RISCV -name '*.o' ! -newer lib/Target/RISCV/RISCVGenInstrInfo.inc`，必须为空。** 重编后 `control_divergent` 也从 `OURS-FAIL → asm:DIFF`。（`188/228` 门槛不受影响：这 88 格不用这些指令。）

## pass 的语义（`tools/re/out/TX8AnnotateControlFlow/decompiled.c` + 厂商 dump 双向确认）
管线位置（厂商 `-debug-pass=Structure` 原文）：`… -structurizecfg -loops -cycles -uniformity -tx8-annotate-control-flow -simt2simd -domtree -early-cse -rpu-lower-shared …`。
**`rpu-unify-divergent-exit-nodes` 与 `structurizecfg` 对这两格是 IR no-op**（`-print-before` 三连对比已证）⇒ 按"语料不需要就不 port"只做了 annotate 一个 pass。
算法（全部有 dump 证据）：
1. **入口 mask** 在**第一次访问到条件分支**时懒创建（`entry` 块 `getFirstInsertionPt`）：`raisa.get.kernel.entry.exec` → `mov128.s.v` → `nor.s(a,a)`；缓存在 `EntryMask`。这就是为什么**全语料只有 4 格**被本 pass 改动（106 格逐格 A/B：3 个真注解 + `control_uniform_loop` 只多出这条**死**链，其余 102 格逐字节不变），也是为什么 `abi_*`/`mathdev_*` 那些**没有分支**的格一行都不多。
2. **条件分支**：`Mask = getBBEntryMask(BB)`（入口 mask，或**栈顶的 `and.s` 结果**）；**先取 mask 再判 uniform**。
   - uniform：只 `push(succ(1), Mask, Mask)`（且 succ(1) 以 `ret` 收尾时不 push）⇒ 关闭时 `Saved==Mask` ⇒ **不插 `set.exec`**。
   - **divergent**：`%p=get.pred.mask(i1 %c)`；`%m=and.s(%p, Mask)`；`%d=raisa.divergent(%m)`；`br i1 (icmp eq i64 %d, 0)`；`push(succ(1), %m, Mask)`。**分支极性原样保留**（两格原始 true 边都是"进 region"），`icmp` 恒为 `eq 0`。
3. **close**（栈顶 == 当前 BB）：pop；`Saved==Mask` ⇒ 什么都不做；否则在 `getFirstInsertionPt` 插 `set.exec(Saved)`（loop header 时先 `SplitBlockPredecessors` 掉 latch）。`else` 形（条件是指向本 BB 的 PHI 且 `isElse`）走另一支：`set.exec(Saved)` + `or.s(Saved, Mask)` + divergent + icmp，两格不触发。
4. **uniform 判据**：厂商用它自己的 `RISCVTTIImpl::isSourceOfDivergence`（**实测 `hasBranchDivergence()` 返回 1**）+ `structurizecfg.uniform` 元数据；**我们不 port 那个 TTI override**（它全模块可见，会影响 unroll/uniformity），改为 pass 内 `dependsOnLaneValue()`（传递依赖 `llvm.riscv.read.reg.*` 即 divergent）。**106 格逐格 A/B 证明选中的分支集与厂商完全相同**（唯一差别只是厂商那 50 格"死入口链"我们也有，见第 1 条）。
5. **已知未实现**：`getBBEntryMask` 的 **loop-header PHI 分支**（厂商那里为 loop header 建 `<1 x i128>` PHI）。**全语料不触发**（含 `control_divergent_loop`，其循环在 pass 之前已完全展开，post-pass IR 里没有任何 mask PHI）⇒ 按纪律登记为 gap，没有猜。

## 验收（同一二进制，采数前已重编 8 个 stale 对象）
| 门槛 | 基线（`1016`） | 本轮（`1017`+`1018`） |
|---|---|---|
| `-print-after=tx8-annotate-control-flow` vs 厂商 | — | **`control_nested`/`control_early_return` `EXACT-IR`**（函数体逐字节）；`control_divergent` 也 `EXACT`（两侧都不动）；两个 loop 格 DIFF 且**差异在 pass 之前**（我方无 structurizecfg/loop-exits，CFG 与 `icmp` 极性本就不同） |
| `.rc` oracle `--all` | 88 ident / 3 diff / 12 ours-fail / 11 both-fail | **88 ident / 5 diff / 10 ours-fail / 11 both-fail**（`control_divergent`、`control_nested` `OURS-FAIL→DIFF`，**0 回退**） |
| 106 格 annotate A/B | — | **恰好 4 格被改动**，其余 102 格逐字节不变 |

## 剩余残差（点名，`control_nested` 的 `.s` 只剩 16 行）
1. **机器块布局**：我方 `if.end8` 作 fallthrough（`beqz a1, .LBB0_2`=if.then），厂商 `if.then` 作 fallthrough（`bnez a1, .LBB0_2`=if.end8）。**语义等价、分支极性相反**；`riscv-insert-vsetvli` 处两侧的 `successors:` 概率行**完全相同**（37.5/62.5）⇒ 差在 MBP/`early-tailduplication`（厂商用 `early-tailduplication`，我方 `branch-folder` 之后才 `tailduplication`，README 早已登记）。
2. **2 个 vreg**：我方 `vadd.vx v2` / `vmul.vx v1` / `vse32.v v1` vs 厂商全部 `v8`（= `vand.vx` 的结果，可 coalesce）。即 `undef` passthru 的 `tied-def` 没跟 `vand` 的目的寄存器合并。
3. **`control_early_return` 还差一族**（逐字证据在 `wt-raisa-be/recon/simt2simd/control_early_return.after.ir`）：我方 `simt2simd` 的 `vectorBinOpName()` 只认 `Add/Mul/And`，而该格需要 **`shl`→`vsll.nxv2i32.i64.i64(undef, %vec, i64 C, i64 128)`**（注意移位量是 **i64**）、**`xor`→`vxor.nxv2i32.i32.i64`**、以及 **vector+vector 的 `vadd.nxv2i32.nxv2i32.i64(undef, %a, %b, i64 128)`**（两操作数都已向量化，不是 `vx` 形）⇒ 当前未分类 ⇒ 整函数 bail ⇒ 仍 `OURS-FAIL`。约 40 行 + 1 次重编。
4. **两个 loop 格**仍需 `rpu-unify-divergent-exit-nodes` + `structurizecfg`（它们的 pre-pass CFG 与我方不同，annotate 只是被动跟随）。

## 补跑的两个门槛（parent 亲自跑，2026-10-17 10:10，`wt-merge-e` 最终二进制 09:56:07）
- **317 parity**（`corpus_parity.py collect --tag cf1` + `round1_attrib.py`）：**byte-identical 217 / OURS-GAP 10 / byte-diff 23** / vendor-fail-too 33 / vendor-fail-same-diag 33 / ours-extra-ok 1。相对 E 线基线（217/12/21）⇒ **2 格 `OURS-GAP → byte-diff`（`control_divergent`、`control_nested`），0 反向**。阻塞归因：cosmetic(+assertions) 17、R1'/simt2simd 7、R2/fork-pass(codegen) 6、R1'/lower-shared 3；gaps by language `{'raisa': 10}` ⇒ **`.c` 零缺口**。
- **musl**（`musl_norm.py --a 厂商 --b wt-merge-e`）：**raw 0/56 + 4 = 4/56；剥 `.comment` 24/56 + 4 = 28/56 identical outcomes** ⇒ **与基线完全一致、无回退**。
⇒ `0075`（`tx8-annotate-control-flow`）**门槛全绿**，可以合并。

## 【新失败类别 + 新共享检查】`.o` 比 `RISCVGenInstrInfo.inc` 旧 ⇒ **opcode-enum 错位**
**症状**：不是"pass 没跑"，而是**指令被打印成错误的名字**（`raisa.divergent` 的展开被打成 `XPERM8` / `XTsm_LSU_SET_LENGTH`，def/use 列表根本不可能属于该指令）+ **AsmPrinter 断言**（`SmallVector<MCOperand>::operator[] idx < size()`）。`sweep_stale_objects.sh` **不会**报这个。
**检查（已落成共享工具 `tools/check_geninc_stale.sh`，每条线都必须跑）**：
```bash
find <build>/lib/Target/RISCV -name '*.o' ! -newer <build>/…/RISCVGenInstrInfo.inc
```
必须为空。control-flow 线重建了 8 个对象（`TX8LegalizeEXEC`、`TX8LowerSharedPass`、`TX8OptimizeConfigInstrs`、`RISCVConstantPoolValue`、`RISCVSelectionDAGInfo`、`MCTargetDesc/TX8MCInfo`、`RISCVMCAsmInfo`、`TargetInfo/RISCVTargetInfo`）后，**光这一步就让 `control_divergent` 从 OURS-FAIL 变 asm:DIFF**。

## 【教训】"树看起来像 base+X"要**逐文件**核对
`wt-merge-e` 有 `0070` 的前端 hunk 与 `early-cse` 钩子，但 **`TX8TransformSIMTToSIMD.cpp` 还是 pre-0070 的单 BB 版本**（1241 行 vs `wt-merge` 的 1588 行）⇒ 注解 pass 在它上面等于 no-op。已用 `0074`（649 行）恢复。**教训：按报告顺序假设合并状态会出错，必须逐文件 `diff`。**

# ★ 编译完整性收官（补丁 `1019`，2026-10-17 21:0x，wt-merge-e）：**8 个目标格全部能编译，0 回退**
**验收口径是"能编译"，不是逐字节**（用户明确：byte-identity 非必需）。八格全部 `OURS-FAIL → asm:DIFF obj:DIFF`：

| 格 | 改前 | 改后 | 残留 |
|---|---|---|---|
| `alloc_l0ashared_base` / `alloc_l0ashared_bank_on` / `alloc_l0wshared_base` / `alloc_l0wshared_bank_on` / `alloc_l1shared_base` / `alloc_l1shared_bank_on` / `alloc_l1_short_ptr`（7） | `asm:OURS-FAIL obj:OURS-FAIL` | **`asm:DIFF obj:DIFF`** | 指令族已与厂商同（`vslh.vx`/`vsuxei64.sync.v`/`vluxei64.sync.v`/`vadd.vv`/`vmv.v.x`/`lui`），差 **多/少一条 vsetvli + 指令次序 + 物理寄存器编号**（= 已登记的 post-RA/AVL 类） |
| `control_early_return`（1） | `asm:OURS-FAIL obj:OURS-FAIL` | **`asm:DIFF obj:DIFF`** | `.s` 只剩 **8 行**：机器块布局的 fallthrough 相反（`beqz a1,.LBB0_2` vs 厂商 `bnez a1,.LBB0_2`，语义等价）= README 第 1032 行的 MBP/`early-tailduplication` 残差 |
| `control_divergent_loop` / `control_uniform_loop`（2） | `OURS-FAIL` | `OURS-FAIL`（**未动，按裁决不做**） | 需 `structurizecfg` + `rpu-unify-divergent-exit-nodes`（其 pre-pass CFG 与我方不同） |

**两步修复（都在 `1019`，2 文件 / 165 行，`patch -p1` 对 pre-image 回放 = 逐字节复现 src）**
1. **`RISCVTargetMachine.cpp::addPreISel`：RAISA 时不加 `global-merge`**（一行 gate）。理由不变（厂商的 `simt2simd` 看到的是 `@…E1a`/`@…E1b`，我方看到 `gep @_MergedGlobals, 0, 2`），**但本轮实测两条更正**：
   - **失败点不是 `rpu-lower-shared` 崩溃**：本树（含 `0074`）的可观测症状是 `Don't know how to custom type legalize this intrinsic!` @ `RISCVISelLowering.cpp:14629`，即 `simt2simd` **bail**、AS2/AS3/AS4 访存活到 ISel。`*** IR Dump After … rpu-lower-shared ***` 只是 parity 收集器报的"最后一条 dump 头"。
   - **`-global-merge` 在本管线里排在 `simt2simd` 之后**（`-debug-pass=Structure`：`… -simt2simd -domtree -early-cse -rpu-lower-shared … -dwarf-eh-prepare -barrier -global-merge …`），**但 pre-`simt2simd` module 里 `@_MergedGlobals` 已经存在**（用 `-mllvm -riscv-enable-global-merge=false` 对照：per-variable global 逐字恢复 ⇒ 那个开关确实控制它）。⇒ "在 `addPreISel`、早于 `simt2simd`"的表述与 dump 的**列表顺序**不符，但**跳过它**确实是正确修法（结论不变、证据更硬）。
2. **`TX8TransformSIMTToSIMD.cpp`：补上三个缺失的 IR 家族**（`alloc_*` 与 `control_early_return` 卡在同一处）：
   - **vector+vector** `vadd.nxv2i32.nxv2i32.i64(undef,%a,%b,i64 128)`（`isVectorVectorAdd`；**只放行 `add`**，因为存档 `after.ir` 里只有它；`vx`/`vi` 形已有族）；
   - `shl` → `vsll.nxv2i32.i64.i64(undef,%vec,i64 C,i64 128)`（**移位量是 `i64`**，不是 lane 宽度；`isShiftScalarOp` + 改写时按 `i64` 建常量与签名）；
   - `xor` → `vxor.nxv2i32.i32.i64`（`vectorBinOpName` 加一项）。
   ⇒ **只跳过 `global-merge` 一格都不动、只加这三个家族也一格都不动**（两者必须同时；这正是先前"一行 hunk 单独做翻不了任何格"的原因，且现在给出了缺失的另一半）。

**爆炸半径（穷举，非抽样）**：扫全部 114 格 `.rc` 的 pre-`simt2simd` module，**只有 10 格**含 `@_MergedGlobals` —— 7 个 `alloc_*` + `mem_l1_multi_{bank_on,default,short_ptr}`（后 3 格是 both-fail，改前改后 verdict 不变）。⇒ **global-merge 跳过在构造上不可能动到任何已逐字节一致的格。**

**三道门槛（同一二进制 21:05:09；采数前 `sweep_stale_objects.sh` 与 `check_geninc_stale.sh` 均 0 stale，两个 `.o` mtime > src）**
| 门槛 | 改前（实测，本次会话重跑） | 改后 |
|---|---|---|
| `.rc` oracle `--all` | 176 ident slots（88 格）/ 10 diff（5 格）/ **20 ours-fail（10 格）** / 22 both-fail（11 格） | **176 / 26 / 4（2 格）/ 22**；**逐格变化恰好 8 个，全 `OURS-FAIL → DIFF`，`IDENTICAL` 集合一格不丢** |
| 317 parity（`collect --tag cf2` + `round1_attrib`） | **217 / gap 10 / diff 23**（`cf1.json` 复算一致） | **217 / gap 2 / diff 31**（0 回退；8 格 `OURS-GAP → byte-diff`）；`.c` 零缺口；gaps by language `{'raisa': 2}` |
| musl 厂商-vs-我方（`musl_norm.py`） | 24/56 + 4 = **28/56 outcomes** | **24/56 + 4 = 28/56**（不变，**无 success↔fail**） |
| musl 我方-vs-我方（pre1019 vs post） | — | **56/56 outcomes 相同**（47 个 `.o` 逐字节 + 9 个同样失败）⇒ 本补丁对 musl **零影响** |

**剩余 2 个 OURS-GAP** = `control_divergent_loop` / `control_uniform_loop`，**唯一的阻塞**是缺失的 `structurizecfg` + `rpu-unify-divergent-exit-nodes`（`tx8-annotate-control-flow` 已就位，只是被动跟随；两格 pre-pass CFG 与我方不同）。**这是本树"能编译"目标上最后剩下的两格。**
**复现**：`python3 wt-merge-e/tools/raisa_oracle.py --cc <abs>/wt-merge-e/build/bin/clang-21 --cells alloc_l0ashared_base,alloc_l0ashared_bank_on,alloc_l0wshared_base,alloc_l0wshared_bank_on,alloc_l1shared_base,alloc_l1shared_bank_on,alloc_l1_short_ptr,control_early_return`

# ★★★ 编译完整性收官（`0076` = 1019）：**仅剩 2 格编不过**
**8/8 目标格现在全部能编译**（0/8 → 8/8）：7 个 `alloc_*` + `control_early_return`，全部 `asm:OURS-FAIL → asm:DIFF obj:DIFF`。
**门槛（全绿、0 回退）**：
| 门槛 | 前 | 后 |
|---|---|---|
| `.rc` oracle `--all` | 176 ident 槽（88 格）/ 10 diff（5）/ **20 ours-fail（10）** / 22 both-fail（11） | **176 / 26（13）/ 4（2）/ 22** ⇒ 逐格变化**恰好 8 格、全为 `OURS-FAIL→DIFF`、IDENTICAL 集一个没丢** |
| **317 parity** | 217 / OURS-GAP **10** / byte-diff 23 | **217 / OURS-GAP 2 / byte-diff 31**，`.c` gap 0，**8 格 `OURS-GAP→byte-diff`、0 反向** |
| musl 厂商-vs-我方 | 24/56 + 4 = 28/56 | **24/56 + 4 = 28/56**、无 success↔fail |
| musl 我方-vs-我方 | — | **56/56 outcomes**（47 `.o` 逐字节 + 9 同失败）⇒ 补丁对 musl **零影响** |

**补丁两半（缺一不可）**：① `RISCVTargetMachine.cpp::addPreISel` 对 `isRaisa()` **跳过 `global-merge`**（一行）；② `TX8TransformSIMTToSIMD.cpp` 补三个 IR 族：**vector+vector** `vadd.nxv2i32.nxv2i32.i64(undef,%a,%b,i64 128)`（`isVectorVectorAdd`，证据只支持 `add`）、`shl → vsll.nxv2i32.i64.i64(undef,%vec,i64 C,i64 128)`（**位移量是 i64，不是 lane 宽度**，单独标量类型/名字路径）、`xor → vxor.nxv2i32.i32.i64`。
**爆炸半径是穷举而非抽样**：114 格里 pre-`simt2simd` module **只有 10 格**含 `@_MergedGlobals`（7 个 `alloc_*` + 3 个 both-fail 的 `mem_l1_multi_*`，后者 verdicts 不变）⇒ **不可能影响任何现有逐字节一致的格**。

**⚠️ 两处诊断更正（已并入认知）**：
1. **`alloc_*` 在当前树里不是 `rpu-lower-shared` 崩溃** —— 有了 `0074` 后，可观测失败是 **`Don't know how to custom type legalize this intrinsic!` @ `RISCVISelLowering.cpp:14629`**（`simt2simd` bail、AS2/3/4 访存活到 ISel）。parity collector 记的 `*** IR Dump After … rpu-lower-shared ***` 只是"最后一个 dump 头"。
2. **`-global-merge` 在 `-debug-pass=Structure` 里列在 `simt2simd` 之后**（`… -simt2simd -early-cse -rpu-lower-shared … -barrier -global-merge …`），**但 `@_MergedGlobals` 已在 pre-`simt2simd` module 里**（`-mllvm -riscv-enable-global-merge=false` 能恢复厂商的 per-variable `@…E1a`/`@…E1b` 逐字一致）⇒ **结论（对 RAISA 跳过）成立，但"它跑在 simt2simd 之前"的措辞与 dump 顺序矛盾**。
3. **`alloc_*` 与 `control_early_return` 是同一个缺失族**（vector+vector add）—— 这就是为什么"只加 global-merge 跳过"一格都翻不动。

**剩余 2 个 OURS-GAP**：`control_divergent_loop`、`control_uniform_loop`，需 **`structurizecfg` + `rpu-unify-divergent-exit-nodes`**（我方 pre-pass CFG 与厂商不同；`tx8-annotate-control-flow` 已就位、只是被动跟随）。**这是"能编译"目标的唯一剩余障碍。**
**接近度（非必需）**：`control_early_return` 的 `.s` 只差 **8 行** = 机器块布局/分支极性（`beqz` vs `bnez`，语义等价）+ `if.end`/`cleanup` 落下；`alloc_*` 的指令族/计数已与厂商一致（`vslh.vx`/`vsuxei64.sync.v`/`vluxei64.sync.v`/`vadd.vv v0,v1,v2`/`vmv.v.x`/`lui a1,262144`），残余是**一条多出的 vsetvli**（地址链用 LMUL_1）、指令顺序、物理 vreg 号。

# ★ 编译完整性：`control_divergent_loop` 翻绿（补丁 `1020`，2026-10-17 21:45，wt-merge-e）：**1 of 2**
**结论：`control_divergent_loop` `OURS-FAIL → asm:DIFF obj:DIFF`（能编译）；`control_uniform_loop` 仍未动（阻塞点已在下面第 5 条点名）。0 回退。**

**① 管线缺的其实只有 `structurizecfg` 一个 pass（不是两个）。** 厂商 `-mllvm -debug-pass=Structure` 的原文是 AMDGPU 分歧栈的**逐字复制**：
`… -rpu-unify-divergent-exit-nodes -domtree -cycles -fix-irreducible -loops -unify-loop-exits -postdomtree -domfrontier -regions -structurizecfg -loops -cycles -uniformity -tx8-annotate-control-flow -simt2simd …`，对应上游 `AMDGPUUnifyDivergentExitNodes → FixIrreducible → UnifyLoopExits → StructurizeCFG → SIAnnotateControlFlow`。
- **`rpu-unify-divergent-exit-nodes` = AMDGPU 那个 pass 改名**：`tools/re/out/TX8UnifyDivergentExitNodes/decompiled.c` 里类名 `TX8UnifyDivergentExitNodes`、pass 名 `rpu-unify-divergent-exit-nodes`、需要 DomTree+PostDomTree+UniformityInfo+TTI、用 `simplifyCFG`、字符串 `DummyReturnBlock`/`TransitionBlock`/`UnifiedUnreachableBlock`/`UnifiedReturnBlock`/`UnifiedRetVal` —— 与 `lib/Target/AMDGPU/AMDGPUUnifyDivergentExitNodes.cpp` 逐项同形，只少了 `amdgcn_unreachable` 那一句（.o 里没有该未定义符号）。
- **但那三个前置 pass 在本语料上是 IR no-op**：厂商 `-print-before/after` 对 `rpu-unify-divergent-exit-nodes`/`fix-irreducible`/`unify-loop-exits` 在 `control_{divergent,uniform}_loop`、`control_nested`、`control_early_return` 上**每个 dump 都与其输入逐字节相同**。⇒ 只接 `addPass(createStructurizeCFGPass())`（上游 pass 本树就有，`llvm/Transforms/Scalar.h` 声明，legacy PM 用 `RGPassManager` 调度，R600 目标本来就这么接）。**接上之后我方 pre-`simt2simd` IR 与厂商逐字节相同**（`control_divergent_loop` 的函数体 diff = 0 行）。

**② 只接 pass 还不够：`simt2simd` 少了三样东西**（这就是先前"一行 hunk 单独做翻不了格"的另一半）。
1. **`icmp ne` → `vmsne`**：`vectorCmpName()` 原来只有 `eq`/`ult`。`structurizecfg` 会把每个被它改写的出口条件**取反**（`icmp eq` → `icmp ne`），所以循环格全体走 `ne` 分支 —— 厂商 after.ir 里正是 `vmsne.nxv2i32.i32.i64`。
2. **phi**：`Flow` 块/循环出口块每个都带 phi。分流规则与厂商一致：**lane 值的 phi 变成 lane 向量类型的 phi**（累加器 `v`），归纳变量/LCSSA 载体保持标量（存的时候 `vmv.v.x` 广播）。因为 phi 与其入值在回边上互为操作数，**分类必须跑到不动点**（本补丁把原来的单遍分类改成 `while (true)` 不动点 + `Unclassified` 标志；不收敛就整体 bail）。phi 的入值在 body 重写**之后**才回填（常量在前驱块末尾 splat，与厂商 `%17 = vmv.v.x … i32 0` 位置一致）；老 phi 先 `dropAllReferences()` 再删。
3. **"参数指针 + 常量字节偏移"的 lane 地址族**：`%b = getelementptr i32, ptr %p, i32 %tid` 的偏移 0 是**单位步长 `vle`**（lane 连续）；`%a = getelementptr i8, ptr %b, i32 <C>` 走 gather，索引向量 = `vslh(vadd(vmul(vid,4) + lo(%p), C), hi(%p))` —— 与厂商 `%18/%19…%23` + `%30/%31/%32` 逐条同形（`vmul`/`ptrtoint`/`lshr`/`trunc` 两个半字 + `vadd`）。

**③ 爆炸半径是穷举的**：114 格 pre-`simt2simd` IR 里**只有 4 格有 >1 个基本块**（`control_uniform_loop` 12、`control_divergent_loop` 9、`control_nested` 3、`control_early_return` 3），其余 110 格是单块 ⇒ 上游 `StructurizeCFG::run()` 对 top-level region 立刻返回 ⇒ **构造上不可能动到任何逐字节一致的格**。实测也如此：逐格 verdict 对比**只有 1 格变化**。

**④ 三道门槛（同一二进制 21:39:05；采数前 `sweep_stale_objects.sh`/`check_geninc_stale.sh` 均 0 stale，两个 `.o` mtime > src）**
| 门槛 | 改前 | 改后 |
|---|---|---|
| `.rc` oracle `--all` | 176 ident 槽（88 格）/ 26 diff（13）/ **4 ours-fail（2）** / 22 both-fail（11） | **176（88）/ 28（14）/ 2（1）/ 22（11）**；**逐格变化恰好 1 格**，IDENTICAL 集合 88→88（lost 0、gained 0） |
| 317 parity（`collect --tag cf3` + `round1_attrib`） | 217 / OURS-GAP **2** / byte-diff 31 | **217 / OURS-GAP 1 / byte-diff 32**，`.c` gap 0（gaps by language `{'raisa': 1}`） |
| musl（`musl_norm.py` 厂商-vs-我方） | 24/56 + 4 = **28/56 outcomes** | **24/56 + 4 = 28/56**（不变，无 success↔fail） |

**⑤ 剩下的 `control_uniform_loop`：阻塞点已定位，没做、也没猜。** 它是个**没被完全展开的真循环**，我方的 `-loop-reduce` 把地址做成了**指针归纳**（`%lsr.iv24 = phi ptr [ %scevgep, %for.body ], [ %invariant.gep16, … ]`，`%scevgep25 = getelementptr i8, ptr %lsr.iv24, i32 -2048`），而厂商的 LSR 给的是**整数归纳 + 每次访问自己算 GEP**（`%arrayidx.idx = shl i32 %k.010, 9`，`%arrayidx = getelementptr i8, ptr %4, i32 %arrayidx.idx`）。要覆盖我方这个形状，需要把 "lane 地址的递推 phi" 认出来（等价于一个 mini-LSR：把 `phi ptr` 化成 `phi i32 字节偏移`）或对 phi 做仿射分析 —— 这是新的一层，不是接线，所以按纪律**没做**。
**注意**：这条差异的根在 **LSR/TTI 的形状差异**（我方 pointer-induction，厂商 integer-induction），**不在 `simt2simd`**；改 TTI 会全域影响（调度/寻址模式），不是"最后 1 格"该付的代价。

# ★★★★ 编译完整性：**102 / 103 格能编过**（`0077` = 1020，2026-10-17 21:39）
**`control_divergent_loop` 翻过**（`OURS-FAIL → asm:DIFF obj:DIFF`）；**全语料只剩 1 格编不过**：`control_uniform_loop`。
| 门槛 | 前 | 后 |
|---|---|---|
| `.rc` oracle `--all` | 88 ident / 13 diff / **2 ours-fail** / 11 both-fail | **88 / 14 / 1 / 11** |
| 317 parity | 217 / gap **2** / diff 31 | **217 / gap 1 / diff 32**，`.c` zero gap |
| musl | 28/56 outcomes | **28/56 outcomes**（无 success↔fail） |
逐格变化**恰好 1 格**（`control_divergent_loop`）；88 格 identical 集**一个没丢**。

**修法（1 行管线 + 三个 simt2simd 缺口）**：
1. **管线只缺一个 pass**：`StructurizeCFG` 本就在树里（`Transforms/Scalar.h` 已暴露），厂商那套是 **AMDGPU divergence 栈的逐字副本**（`rpu-unify-divergent-exit-nodes → fix-irreducible → unify-loop-exits → regions → structurizecfg → loops → cycles → uniformity → tx8-annotate-control-flow`）。反编译证明 `TX8UnifyDivergentExitNodes` = `AMDGPUUnifyDivergentExitNodes` 改名；而**前三个 pass 在 4 个多 BB 格上都是 IR no-op** ⇒ **只需 `addPass(createStructurizeCFGPass())`（RAISA 门控）**。改后 `control_divergent_loop` 的 pre-`simt2simd` IR 与厂商**逐字节一致**。
2. **`icmp ne → vmsne`**：structurizecfg 会反转每个出口条件，所以 loop 格分支在 `ne` 上（`vectorCmpName` 原来只有 `eq`/`ult`）。
3. **PHI 支持**（lane-valued → 向量 phi；归纳/LCSSA → 标量；**分类做成不动点**，因为 phi 与输入在回边上互为操作数）+ **"参数指针 + 常量字节偏移"的 lane 寻址族**（偏移 0 = 单位步长 `vle`；偏移 N = `vslh(vadd(vmul(vid,4)+lo(base), N), hi(base))` + `vluxei.sync`）—— 与厂商逐指令对应。
**爆炸半径穷举**：114 格里只有 **4 格 pre-`simt2simd` 有 >1 BB**；`StructurizeCFG::run` 对顶层 region 立即返回 ⇒ 110 个单 BB 格**可证不可能被碰**，并被"恰好 1 格差值"实证。

**最后 1 格的精确阻塞（未尝试，按预算纪律）**：`control_uniform_loop` 是**真循环（未完全展开）**，我方 `-loop-reduce` 产出**指针归纳**（`phi ptr` + `scevgep`），厂商产出**整数归纳 + 每访问 GEP**（`%arrayidx.idx = shl i32 %k.010, 9`）⇒ pass 看不到 `tid` 根的基址。修它需要 **lane 地址递归分析（mini-LSR：`phi ptr` → `phi i32` 字节偏移）**或对 phi 做仿射分析 —— **是新的一层，不是接线**；根因是 **LSR/TTI 形状差**，不在 `simt2simd`。

**⚠️ 两条新的工具地雷（已修进共享工具）**：
1. `wt-merge-e/tools/rebuild_one.sh` 是 `wt-merge` 那份的**逐字副本**、硬编码 `BUILD=$ROOT/wt-merge/build` ⇒ **从 `wt-merge-e` 调用会写进 `wt-merge/`**！根目录 `tools/rebuild_one.sh` 也硬编码 `wt-config`。**已重写**：必须显式给 `<tree> <绝对 src 路径>`，并校验 src 在 tree 下、且必须是绝对路径。
2. `build_e.sh --force-src` 在**相对路径**下会**静默匹配 0 个对象然后 rc=0**（重链一个没变的二进制）。**已给共享副本加保护**：`--force-src` 非绝对路径直接报错退出。

# ★★★★★ 【目标达成】编译完整性：**103 / 103 格全部能编译**（`0078` = 1021，2026-10-17 22:05）
最后一格 `control_uniform_loop`：`asm:OURS-FAIL obj:OURS-FAIL`（`RISCVISelLowering.cpp:14629`，rc=-6）→ **`asm:DIFF obj:DIFF`（rc=0，能编译）**。
**四道门槛全绿、0 回退（同一二进制 22:05:30，采数前两 sweep 均 0）**：
| 门槛 | 前 | 后 |
|---|---|---|
| `.rc` oracle `--all` | 176 ident 槽（88 格）/ 28 diff（14）/ **2 ours-fail（1 格）** / 22 both-fail | **176（88）/ 30（15）/ 0 / 22** |
| 317 parity | 217 / OURS-GAP **1** / diff 32 | **217 / OURS-GAP 0 / diff 33**，`.c` 零缺口 |
| musl（厂商-vs-我方） | 24/56 + 4 = 28/56 | **完全相同**、逐 kernel verdict 无变化 |
| 两 sweep | 0 / 0 | **0 / 0**（`.o` mtime > src、bin > `.o`） |
**逐格证明**：cf3 vs cf5 全 317 格逐条 diff ⇒ **恰好 1 格变化**（`control_uniform_loop`：rc −6→0、诊断清空）；**88 个 identical 一格没丢**。

## 新规则 ↔ 厂商 asm 逐指令对应（`control_uniform_loop`）
```
v9  = vmul.vx vid, 4                        ; lane byte offset
v10 = vadd.vx v9, <lo(in)>                  ; lane address vector
v13..v25 = vadd.vx v10, 512/1024/…/3584     ; 8 个展开常量偏移（preheader）
loop: slli a4, a1, 9                        ; = %arrayidx.idx = k*512
      vadd.vx v26, v10, a4                  ; ← 新规则：lane addr + uniform scalar
      vslh.vx  v30, v26, <hi(in)>           ; 已有（1020）
      vluxei64.sync.v v27, (zero), v30      ; 已有
```
实现：`laneAddrOf` 新增 **`VarOff`**（`gep i8, %lane_ptr, %uniform_scalar`，用 `dependsOnTid()` 判"非 lane 派生"；常量与变量偏移可累加，因为 epilogue 是 `gep %15,512` 套 `gep %11,%35`）+ `LaneIndex` 发 `vadd.<lane>.i32.i64`。**另外三处必须一起做否则编不过**：① **闭合环 phi 的结构化分类**（`%k=phi[0],[%inc]`、`%inc=add %k,8` 互为唯一未决操作数；lane 累加器同理，靠 `dependsOnTid` 分流）；② **gather 时强制物化 `vid`**（这正是厂商 IR 里那条"看似多余"的 `vid.v`）；③ **`read.reg.tid.x` 的擦除推迟到死 GEP 清扫之后**（否则 `for.body` 的 tid-GEP 还挂着 use ⇒ `Use still stuck around`）。

## LSR gate 的定性（按此措辞记录）
**它是 stopgap，不是厂商忠实重建**：厂商**确实**跑 LSR，只是其 `LoopStrengthReduce.cpp` **被 fork 过**。三重证据：LSR 输入逐字节相同；`isLSRCostLess`/`getPreferredAddressingMode`/`isLegalAddressingMode` 反汇编语义相同、DataLayout 相同、调度模型与全部 `-lsr-*` 选项都不能翻转；**换 `-triple riscv64-unknown-linux-gnu` 两侧输出完全相同** ⇒ 差异在 LSR 内部且被 triple 门控。
**代价**：每格丢一次强度削减；**观测为 0** —— 114 格普查里**只有 1 格**走到 `loop-reduce`（其余 113 格 0 字节日志）。
**未来恢复路径**：(i) 逆向厂商 LSR 的 delta；(ii) 在 `simt2simd` 里做真正的 lane-address 递推（`phi ptr` → loop-carried 向量地址）。**重要修正：本格最终不需要 (i)/(ii)** —— 决定性反证是"把厂商自己的 `-print-before=simt2simd` module 喂我方 clang 仍然 abort" ⇒ 缺口只在 pass 里，所以 (i)/(ii) 从"修这格的必要手段"降级为"纯优化恢复"。逃生口：`-mllvm -riscv-raisa-lsr`（默认关）。

## ⚠️ 三条工具地雷（前两条已修进共享 `tools/rebuild_one.sh`）
1. **`--force-src` 对"依赖记录指向旧树路径"的对象会静默 0 匹配**：`build/.ninja_deps` 里 `TargetPassConfig.cpp.o` 的 dep 记的是 `…/riscv-llvm-musl-toolchain/third_party/llvm/…`，`norm()` 只归一化 `/wt-<name>/` ⇒ 命中 0、rc=0、**重链一个没变的二进制**。**解法：加 `--force-objs <basename>.o`**（共享脚本现已同时传，并断言重编数 > 0）。
2. **`.ninja_deps` 里没有条目的对象**（如 `TX8TransformSIMTToSIMD.cpp.o`）连 `--force-objs` 都看不见，而 `ninja -n` 会因"命令行变了"想重编 3120 边 ⇒ **用 `ninja -t commands <obj> | tail -1` 取裸命令直接跑**，再用 `ninja -t commands lib/libLLVMRISCVCodeGen.a / bin/clang-21 | tail -1` 归档/链接。共享脚本在 0 匹配时会打印这三条命令。
3. `logs/changed_inc.json` 必须清成 `[]`，否则 minibuild 会静默计划全量重编。

## 【重要更正】17 个"cosmetic" byte-diff **只是 `.ident` 那一行**，不是代码生成（2026-10-17 23:20，parent 亲验）
**证据**：对 `abi_float_front` 直接比两侧 `.s`（不滤任何行）⇒ **唯一的差异是**：
```
< 	.ident	"clang version 21.1.0-rc1 (https://github.com/llvm/llvm-project.git cbcb48a88c3a6227cfc09f91880d5cb92a26f5f9)"
> 	.ident	"clang version 21.1.0-rc1 (ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf26e7a507d3ecb57e76c956dd21b5c822a6)"
```
（即厂商构建时嵌入的仓库 URL/commit 与我们不同 —— 与 musl 对照里那个 `.comment` 伪影是**同一件事**）。
**验证**：前 6 个 cosmetic 格（`abi_{float,ints,struct,pointer_u32_u64,pointer_u64_u32,two_pointers}_front`）**去掉 `.ident` 后 diff = 0**。
⇒ **真实 byte-diff 只有 16 个（codegen）**；把 `.ident` 纳入归一化后，**parity = 234/317 = 74%**（而不是 217/68%），`.rc` 侧 = **105/114 = 92%**（88 + 17 仅差 `.ident`）。
**两个已证伪的假设**（别再试）：`-mllvm -disable-tail-duplicate` 对 `control_*` **毫无变化**；`-discard-value-names` **反而更差**。
**⇒ 对比工具应把 `.ident`（asm）与 `.comment`（obj）一起归一化**；musl 侧已有 `musl_norm.py --strip-comment`，parity/oracle 侧应补 `.ident` 归一化。

# ★ `RequireStructuredCFG` = RAISA 门控（补丁 `1022`）：`control_early_return` 转**逐字节一致**（2026-10-18 00:1x，RE 反查线）
**一句话**：厂商 `RISCVTargetMachine` 在 triple 是 `*-raisa` 时置 `TargetMachine::RequireStructuredCFG = true`；我方从未置过 ⇒ 我方 RAISA 管线多跑一个 late `tailduplication`、MBP 会做 tail duplication/tail merge、BranchFolding 也做 tail merge。这是**纯 fork 行为**，不是 stock-pass 选项。

**证据 1（厂商目标文件，`libLLVMRISCVCodeGen.a(RISCVTargetMachine.cpp.o)`，`RISCVTargetMachine::RISCVTargetMachine` `+0x16a..+0x18b`）**：
```
mov 0x2c(%rbp),%eax      ; Triple::OSType
cmp $0x2a,%eax           ; 0x2a = 42 = Triple::RAISA
jne 192
orb $0x1,0x2b0(%rbx)     ; TargetMachine::RequireStructuredCFG |= 1  (bitfield bit0)
```
我方同名对象**没有**这条 store。`RequireStructuredCFG : 1` + `O0WantsFastISel : 1` 在 `TargetMachine.h:117`，setter 在 `:256`。

**证据 2（管线，同文件两侧 `-mllvm -print-after-all`）**：厂商 `.rc` 机器管线 `branch-folder` 之后**没有** `tailduplication`；厂商 musl `.c` 管线**有**。而该 pass 的注册处正是 `TargetPassConfig::addMachineLateOptimization()` 的 `if (!TM->requiresStructuredCFG())`（`TargetPassConfig.cpp:1551`）⇒ 该 flag 在厂商侧**RAISA 真 / musl 假**。

**它的四个消费者（正好是 SIMT 需要的）**：`TargetPassConfig:1551`（late taildup）、`MachineBlockPlacement.cpp:632`（`allowTailDupPlacement()` ⇒ 无 placement tail-dup、无 MPDT、`:3624` 无 MBP 内 tail merge）、`BranchFolding.cpp:126/154`（无 tail merge）、`MachineBasicBlock.cpp:1406`（拒绝 split critical edge —— 注释原文就是 exec-mask HW）。

**为什么 `control_early_return` 之前差 8 行**：两侧 `-print-after=gc-analysis` 的块结构相同（`[bb.0, bb.1(if.end), bb.2(cleanup)]`、`BNE … bb.2`）；我方 MBP 把 `bb.2` **tail-duplicate 进 `bb.1`** 并把 `bb.2` 排在前 ⇒ 分支变 `BEQ … bb.1`、`isetexec sr0; ret` 出现两份。
**⇒ 先前"多一条 `isetexec sr0`"不是 `TX8LegalizeEXEC` 的插入点问题**（`-print-after=legalize-exec` 两侧一致，`isetexec` 都在 `ret` 前），而是**重复的返回块**。
**零重编证明**：pre-`1022` 二进制加 `-mllvm -tail-dup-placement=false` ⇒ diff 8 → **0**。

**验收（同一二进制，两 sweep 0 stale、`changed_inc.json == []`）**
| 门槛 | 前 | 后 |
|---|---|---|
| `.rc` oracle slots | 176 ident（88 格）/ 30 diff（15）/ 0 ours-fail / 22 both-fail | **178（89 格）/ 28（14）/ 0 / 22**，逐格变化**恰好 1 格** |
| 317 parity（raw） | 217 / gap 0 / diff 33 | **218 / 0 / 32**，`.c` gap 0 |
| musl | 24/56 + 4 = 28/56 | **24/56 + 4 = 28/56**，无 success↔fail |

# 16 个 codegen 格逐格定性（同日，同一条线）+ 三个精确阻塞
分类口径：把 `vN/aN/sN/srN/N` 全部归一化后再 diff；**归一化后 0 行 = 纯物理寄存器染色**。
| 类 | 格 | 结论 |
|---|---|---|
| 已修 | `control_early_return` | `1022` ⇒ asm+obj 逐字节 |
| **纯染色（6）** | `control_nested`(6)、`control_divergent`(10)、`mem_{l1,l0a,l0w}shared_u64_uniform`(各 4)、`alloc_l1_short_ptr`(2) | 归一化后 **0 行**；`vadd.vx v8/vmul.vx v8/vse v8`（厂商）vs `v2/v1/v1`（我方） |
| 次序+vsetvli 归属（2） | `control_{divergent,uniform}_loop` | 归一化后 41/192 行：`srli`/`vadd`/`mv128.s.v`/`set_mem_alloc`/`vmsne` **次序**与 vsetvli 归属不同；**已无块布局差**（135→101、335→332） |
| `rpu-lower-shared` 布局（7） | `alloc_*` | base 族：厂商把 `b@+0`、`a@+576`（非声明序）；我方 `a@0、b@128`。bank 族：厂商 `0x40000000`/`0x40020000`（**+128 KiB**），我方 `0x40000000`/`0x40000080`（**+128 B ⇒ `-riscv-bank-aware` 的 bank 步长没实现**） |
| 内建 lowering（1） | `builtin_builtin_riscv_kcore_ncc_read`（`.c` 格） | 厂商 `kcore_ncc_read a1,zero; mv zero,a0; mv a0,a1`（`rs2` 钉 x0、`rd` 是另一个输入）；我方 `kcore_ncc_read a0,a0` ⇒ 属 `0030` 里唯一那条 C++ `Select()` |

## 阻塞 ①（最有价值）：`vreg-rename`（`0033`）的规则与厂商不同 + 我方有潜在误编译
**判据工具（秒级、无需重编）**：厂商 `llvm-toolchain/bin/llc -mtriple=riscv64-unknown-raisa -mcpu=tsm-x82 -mattr=+xtsm82s -run-pass=vreg-rename <MIR>`，MIR 由厂商 `-mllvm -stop-before=vreg-rename` 产出（`wt-merge-e/logs/re16/vr/`）。
**我方的缺陷（三个探针 + 两个真实格）**：
1. 厂商**只在同一指令同时"非 tied 地读 R"时才改其目的寄存器**——把 `$v11=VID; $v11=VAND undef $v11, killed $v11` 改成 `$v0=VAND undef $v0, killed $v11` 并把使用者改成 `$v0`（`p_rmw11.mir`，**该 def 是块内首次定义也照样改**）；当被读的是**另一个寄存器**时（`pvid11.mir`）不改。
2. **被 tied passthru 读的 def 不改**：厂商 `control_divergent` 保留 `$v8 = PseudoVMUL_VX_M1_MASK $v8(tied-def 0), …`（= `riscv-vector-peephole` 折叠出的 masked `vmul` 的 passthru），我方把 `vadd` 与 `vmul` 各自改名 ⇒ 最终 asm `vadd.vx v2` + `vmul.vx v1, …, v0.t` —— **masked-off lane 的 passthru 丢了（潜在误编译）**，厂商是同一寄存器 `v8`。
3. 厂商 pass 还有一步 **`RestoreAlignedCTInstruction`**（`runOnMachineFunction` 里调用、见 `tools/re/out/TX8RegisterRename/decompiled.c`）：把 `PseudoVSLH_ALIGNED_VX_M2` 还原成 `PseudoVSLH_VX_M2`、操作数从 `$v8m2` 降成 `$v8`、加 `implicit $align2`，**并**把目的改成 `$v2m2`。我方**完全没有这一步** ⇒ mem-u64 三格永远少这一次改名。
⇒ 结论：`vreg-rename` 要按（同一指令非 tied 自读）+（块内更早的非 tied 读且未被改写）+ `RestoreAlignedCTInstruction` 重写；**先修它再测两个 loop 格**（它们的次序差很可能是它喂给 postmisched 的染色差）。

## 阻塞 ②：`rpu-lower-shared`（`0054`）缺 bank 步长 + 缺厂商的物化次序（见上表）
## 阻塞 ③：`kcore_ncc_read` 的 C++ `Select()` 形状（见上表）

# `.ident` 归一化落地（`tools/corpus_parity.py`）
新增 `sha_n`（asm 去 `.ident` 行 / ir 去 `!N = !{!"clang version …"}` 行），`compare` 在两侧都有该字段时多打一行 `[.ident-stripped] byte-identical/modulo-ident N`；**raw `sha` 与全部既有 verdict / `round1_attrib.py` 行为不变**（`vendor.json` 没有该字段，故今天的 stripped 数字由 `logs/re16/stripped.py` 实测）：
- **9 格只差 `.ident`** = `abi_*_front`(6) + `id_{tid_y,ntid_y,ntid_z}_front`(3) ⇒ **218+9 = 227/317 = 71.6%**；
- **8 格 `effects_*` 还差 SSA 值名与 `entry:` 标签**（= 厂商 `-discard-value-names`）⇒ 全算上 **235/317 = 74.1%**（= parent 的 234 + 本轮 +1）。

# ★★ RE 反查（`0079`）：`RequireStructuredCFG` 是 RAISA 门控 ⇒ `control_early_return` 逐字节；找到**唯一已证潜在误编译**（2026-10-18 00:04）
## 1) 根因：`if (TT.isRaisa()) setRequiresStructuredCFG(true)`
厂商 `RISCVTargetMachine.cpp.o` 的构造函数 `+0x16a..+0x18b` 反汇编：`cmp $0x2a`（0x2a=42=**Triple::RAISA**）`jne` + `orb $0x1,0x2b0` ⇒ 厂商在 RAISA 下**置 `RequiresStructuredCFG`**。我方**从未置过**。
该 flag 的**四个消费者正好是 SIMT 要的**：① late taildup（`TargetPassConfig::addMachineLateOptimization()` 里 `if (!TM->requiresStructuredCFG())`）；② MBP `allowTailDupPlacement()`（无 placement tail-dup / 无 MPDT / 无 MBP 内 tail merge）；③ `BranchFolding` 的 tail merge；④ `canSplitCriticalEdge`（注释原文就是 exec-mask 硬件）。
**行为确认**（同文件两侧 `-print-after-all`）：厂商 `.rc` 机器管线 `branch-folder` 之后**没有** `tailduplication`，厂商 **musl `.c` 管线有** ⇒ 该 flag 在厂商侧「RAISA 真 / musl 假」。
**⇒ `set.exec` 归属问题有答案：不是 `TX8LegalizeEXEC`。** 两侧 `-print-after=legalize-exec` 一致、`isetexec` 都在 `ret` 前；`control_early_return` 那 8 行是**返回块被 MBP tail-duplicate 成两份**（我方 MBP 把 `cleanup` 复制进 `if.end` 且排前面 ⇒ 分支极性 `BNE`→`BEQ` + 多一份 `isetexec sr0; ret`）。**零重编证明**：pre-1022 二进制 + `-mllvm -tail-dup-placement=false` ⇒ diff 8 → **0**。（这也解释了我试的 `-disable-tail-duplicate` 为何无效：真正起作用的是 `-tail-dup-placement`，且 MBP 还有 `canTailDuplicateUnplacedPreds` 一条不看 size 阈值的路径。）
⇒ **`control_early_return`：`asm:IDENTICAL obj:IDENTICAL`**（`.rc` 89 格逐字节）。

## 2) 16 个 codegen byte-diff 的逐格定性（归一化 v/a/s/sr/立即数后再 diff）
| 类 | 格 | 关键差异 |
|---|---|---|
| 已修 | `control_early_return` | asm+obj 逐字节 |
| **纯染色 6** | `control_nested`(6 行)、`control_divergent`(10)、`mem_{l1,l0a,l0w}shared_u64_uniform`(各 4)、`alloc_l1_short_ptr`(2) | 归一化后 **0 行**；厂商 `vadd.vx v8/vmul.vx v8/vse v8` vs 我方 `v2/v1/v1` |
| 次序+vsetvli 归属 2 | `control_{divergent,uniform}_loop` | 归一化后 41/192 行（`srli`/`vadd`/`mv128.s.v`/`set_mem_alloc`/`vmsne` 次序与 vsetvli 归属），**已无块布局差** |
| shared 布局 7 | `alloc_*` | base 族：厂商 `b@+0, a@+576`（**非声明序**）vs 我方 `a@0,b@128`；**bank 族：厂商 `0x40000000`/`0x40020000`（+128 KiB），我方 `0x40000000`/`0x40000080`（+128 B）⇒ `-riscv-bank-aware` 的 bank 步长根本没实现** |
| 内建 lowering 1 | `builtin_builtin_riscv_kcore_ncc_read`（`.c` 格） | 厂商 `kcore_ncc_read a1,zero; mv zero,a0; mv a0,a1`（rs2 钉 x0、rd 是另一个输入）vs 我方 `kcore_ncc_read a0,a0` |

## 3) ⚠️ **最大阻塞 = `vreg-rename` 规则不同，且我方有潜在误编译**
秒级 oracle：厂商 `llc -run-pass=vreg-rename` 跑**厂商自己的** `-stop-before=vreg-rename` MIR（探针 `logs/re16/vr/`）。三条实证规则：
① **仅"同一指令的非 tied 自读"**才改目的寄存器（首次定义也改，`p_rmw11.mir`）；
② **被 tied passthru 读的 def 不改**；
③ 还有一步 **`RestoreAlignedCTInstruction`**（`ALIGNED_VX_M2`→`VX_M2` + `$v8m2`→`$v8` + `implicit $align2` + 目的改 `$v2m2`）——**我方完全没有**。
**危害**：我方把 `riscv-vector-peephole` 折叠出的 masked `vmul` 的 **tied passthru 链拆开**（`vadd.vx v2` + `vmul.vx v1,…,v0.t`）⇒ **masked-off lane 的 passthru 丢失 = 潜在误编译**；厂商用同一个 `v8`。
**范围**：6 个纯染色格 + 2 个 loop 格（次序差应是染色差喂给 postmisched）+ 3 个 `alloc_*` 的部分染色都挂在它上面。**这是整 pass 重写 + 用厂商 llc 当 oracle**（不需要先重编），不是 stock 选项。

## 4) 门槛（同一二进制 00:04；两 sweep 0、`changed_inc.json == []`）
`.rc` oracle **176 ident(88 格)/30 diff(15)/0 ours-fail/22 both-fail → 178(89 格)/28(14)/0/22**（逐格变化**恰好 1 格**，88 格一个没丢）；317 parity raw **217/0/33 → 218/0/32**；musl **28/56** 不变。
**`.ident` 归一化**：`corpus_parity.py` 已加 `sha_n`（asm 去 `.ident` 行 / ir 去 `!llvm.ident` 元数据行）+ `compare` 多打 `[.ident-stripped]`（**raw `sha` 与既有 verdict/round1_attrib 行为不变**）。实测 **9 格只差 `.ident`**（`abi_*_front` 6 + `id_{tid_y,ntid_y,ntid_z}_front` 3 —— 这 9 格是 **ir 格**，差的是 `!llvm.ident` 元数据）；另有 8 格 `effects_*` 还差 SSA 值名与 `entry:` 标签（`-discard-value-names`）。⇒ **227/317 = 71.6%（严格剥 ident）、235/317 = 74.1%（再含 discard-value-names 类）**。

## 5) 建议下一步（parent 采纳）
① **重写 `vreg-rename`**（阻塞①，6 纯染色 + 2 loop + 部分 alloc，且是**唯一已证潜在误编译**）；
② **`alloc_*` 的 bank 步长**（阻塞②，`TX8LowerSharedPass` 里 3 行：128 B → **128 KiB** 的 bank 步长）。

## `vreg-rename` 首次翻转（2026-10-18 00:30）：**5/6 纯染色格逐字节**
`control_nested`、`control_divergent`、`mem_{l1,l0a,l0w}shared_u64_uniform` = **`asm:IDENTICAL obj:IDENTICAL`**；第 6 格 `alloc_l1_short_ptr` 实为 **`rpu-lower-shared` 布局**格（`+576` vs `+128`，需 `-fraisa-short-ptr`），归次级目标。
**真正的规则（被上一线"def-after-def"说法掩盖）**：**WAW 反相关** —— 只要 R（或与之**共享 register unit** 的寄存器）在本块内**此前有定义**就改名，**不需要**同指令自读、也不需要该值有使用者（**死 def 也改**，`vid v8; vid v8` 第二处就改）。
**两条 passthru 保护（潜误编译的真正根因）**：① 定义指令自身的 tied passthru 若读的是**活值**（非 `undef`）⇒ 该 def 不改；② 某条指令的**活 passthru** 所消费的值必须留在原寄存器 ⇒ 反向扫描遇到的**下一个定义**被跳过（一次性）。这条正好保住 `vand→vadd→masked vmul` 链。
**`V0` 永不作为改名来源**（`$v0 = VMSEQ/VAND` 不动），但 V0 仍是合法目标（`control_divergent` 里 `VAND→v0` 的来源）。
**⚠️ 别名必须按 register unit 判、不是 alias set**：`MCRegAliasIterator($v9)` 会带出 `$v8m2`（共享 unit 9）⇒ 照它做 WAW/活出保护会**误跳**（`control_early_return` 的 `vmsltu` 就是这样被漏掉）；改 `MCRegUnitIterator` 后 mem-u64 的 `$v8m2→$v2m2`、`$v8→$v0` 与厂商逐字相同。
`RestoreAlignedCTInstruction` 已实现（ALIGNED→plain、宽操作数按 `sub_vrm1_0` 收窄）；**`implicit $align2` 未加**（本树无 ALIGN2 寄存器；只影响 MIR 文本，asm/obj 不可观测）。
**潜误编译已修**：masked `vmul` 的 tied passthru 现在与其生产者共用一个寄存器（`control_divergent`/`control_nested` 的 asm 与厂商逐字节相同）。

## ⚠️ 又一条跨树污染（已修）：`tools/sweep_stale_objects.sh <tree>` 内部会调 `<tree>/tools/rebuild_one.sh`，而各树里的那份是**硬编码别的树**的副本
后果：**它把 `wt-merge-e` 的新源码编进了 `wt-merge` 的构建目录**（该线中招一次，已用 `wt-merge/build` 自己的 ninja 命令重编该对象 + 重归档 + 重链修复，`wt-merge/build/bin/clang-21` 00:22:31 与 `wt-merge/src` 一致）。
**修法（parent 已做）**：把**所有 `wt-*/tools/rebuild_one.sh` 副本**换成**自解析包装器**（从 `dirname $0/..` 推出 TREE，再委托共享 `tools/rebuild_one.sh <tree> <绝对 src>`，后者校验 src 在 tree 下且必须绝对路径）。
**教训**：任何"共享工具"一旦被复制进各树，就会在别的树上变成**写错树的定时炸弹** —— 优先用**自解析路径**或**显式 tree 参数**，绝不硬编码兄弟树。

# ★★★ 阻塞①解决：`vreg-rename` 按厂商三条规则重写 + `RestoreAlignedCTInstruction`（补丁 `1023`，2026-10-18 00:35，wt-merge-e）
**结果：6 个"纯染色"格里 5 个转 `asm:IDENTICAL obj:IDENTICAL`**（`control_nested`、`control_divergent`、`mem_{l1,l0a,l0w}shared_u64_uniform`）；**潜在误编译已消除**（masked `vmul` 的 tied passthru 与生产者共用一个寄存器）。

## 判据工具（秒级、不用整轮重编）
- `logs/re16/vr/vr_ab.py <cell>`：厂商 `-stop-before=vreg-rename` 出 MIR → 名字映射成我方拼写 → **两侧各跑 `llc -run-pass=vreg-rename`** → 归一化 diff。（`@file.mir` 可直接跑探针。）
- `logs/re16/vr/mkprobe3.py <name> <spec>`：用 `control_divergent` 的机器函数骨架生成 **liveness-clean** 探针（`vid/vand/vadd/vsub/vmulm/vmseq/vle/vse`，`^` 后缀 = killed，passthru 恒 `undef`）；`logs/re16/vr/batch.sh <name> <spec>` 一次给出两侧结果。
- 坑：`-run-pass` 会**强制**打开 MIR 校验；本树 pre-MIR 的 `XTsm_VSETSIMT` 不声明 `implicit-def $exec` ⇒ 多 BB 粒度的自家 MIR 回放会 `Using an undefined physical register`。`vr_ab.py` 已在该行补 `implicit-def $exec`；自家 MIR 手工回放需同样的补丁。

## 真规则（20/20 探针两侧逐字一致后归纳，全部有探针）
1. **触发 = WAW 反相关**：R（或与 R **共享 register unit** 的寄存器）在本块内**此前有定义**就改名。**不需要**同指令自读、**不需要**该值有使用者（死 def 也改：`vid v8; vid v8` 第二处改）。上一线"def-after-def"的措辞碰巧描述了同一集合，但配套的"仅同一指令非 tied 自读"结论是**过窄误读**（只有 `p_rmw11`/`pvid11` 两个探针）。
2. **两条 passthru 保护**（潜在误编译的真正根因）：
   - 定义指令**自身**的 tied passthru 读的是**活值**（非 `undef`）⇒ 该 def 不改（`... = VSLH undef` 改，`... = VAND renamable $v8` 不改）；
   - 任何指令的**活 passthru** 所消费的值必须留在原寄存器 ⇒ 反向扫描遇到的**下一个定义**被跳过（**一次性**，被该 def 消费）。这条正是 `vand → vadd → masked vmul` 链：厂商全程 `$v8`，旧实现拆成 `vadd.vx v2` + `vmul.vx v1,…,v0.t` ⇒ masked-off lane 的 passthru 丢失。
3. **落在 V0 的 def 永不改名**（`$v0 = VMSEQ/VAND` 保持 v0），但 **V0 是合法目标**（`control_divergent` 里 VAND→`v0` 就是它）。旧实现在这里会把第二条 `vmseq` 改到 v1，与厂商不符。
4. **别名必须用 register unit，不能用 `MCRegAliasIterator` 的 alias set**：`MCRegAliasIterator($v9)` 会带出 `$v8m2`（共享 unit 9）⇒ 用 alias set 做"活出保护/WAW"会**误跳过**合法改名（`control_early_return` 的 `vmsltu` 就被漏掉，表现为 `vmsltu.v9` vs 厂商 `v0`）。改用 `MCRegUnitIterator` 后：`$v8m2→$v2m2`、`$v8→$v0`、`$v9→$v0` 全部与厂商逐字相同。
5. **`RestoreAlignedCTInstruction`**（厂商在 `runOnMachineFunction` 最前面调用，定义在 `TX8RegisterAlignment.cpp.o`）：`*_ALIGNED_*` → 普通孪生、被 `vreg-align` 加宽的操作数（op 2）按 `sub_vrm1_0/sub_vrm2_0/sub_vrm4_0` 收窄、def 保持不变。厂商还会追加 `implicit $align2`；**本树没有 ALIGN2 寄存器**（厂商 `libLLVMRISCVDesc.a` 的 reg 名表里有 `align2/align4`），该 marker 只影响 MIR 文本、**asm/obj 不可观测**，故未加（要逐字 MIR 保真需再加寄存器，TableGen 级联 ~10min）。
6. 目标寄存器仍走"类分配序反向 + 单调游标"（旧实现这部分是对的，探针 q1/q2 早就证明）。

## 门槛（同一二进制 00:28，两 sweep 0，`changed_inc.json == []`）
| 门槛 | 改前 | 改后 |
|---|---|---|
| `.rc` oracle `--all` | 178 ident 槽（89 格）/ 28 diff（14）/ 0 ours-fail / 22 both-fail | **195（94 格）/ 11（9 格）/ 0 / 22**；逐格变化**恰好 5 格**（`control_nested`、`control_divergent`、3 个 `mem_*_u64_uniform`），89 格一个没丢；另 7 个 `alloc_*` 从 `obj:DIFF` 变 **`obj:IDENTICAL`** |
| 317 parity raw | 218 / gap 0 / diff 32 | **226 / 0 / 24** |
| 317 parity（build-identity 行也归一化） | 227 | **235** |
| musl（`musl_norm.py` 厂商 vs 我方） | 24/56 + 4 = 28/56 | **24/56 + 4 = 28/56**，无 success↔fail |
| `mir_ab.sh` 机器管线 A/B | — | **`control_divergent` diff 0 行**（`control_{nested,early_return}` 因 `PseudoMOV128_S_V_B32` 无对应伪层、harness 解析失败；端到端 oracle 已 asm+obj 逐字节） |

## 剩余残差（逐格点名，都不是本 pass）
- **`alloc_*`（7）**：asm 的差异是 **shared 布局**（`addi a2,a1,576` vs `addi a1,a1,128`；bank 族 128 B vs **128 KiB**）+ 厂商指针键容器的物化次序 ⇒ 归 `TX8LowerSharedPass`（阻塞②）。注意这些格 `obj:IDENTICAL` 而 `asm:DIFF` 是**布局彩票**（同一编译器 asm/obj 两种命令行的指针序可不同，见"厂商缺陷类"）。
- **`control_{divergent,uniform}_loop`（2）**：**不是染色、也不是 vreg-rename**。归一化名后，两侧 **pre-`vreg-rename` MIR 已差 178 / 728 行**（`$x13` vs `$x10` 这类**标量**物理寄存器、`implicit killed $exec` vs `implicit $exec` 的 kill 标记、`PseudoMOV128_S_V_B32` 伪层缺失、`BNE` 位置），本 pass 只再贡献 12 / 11 行（其后果）。⇒ 上游（ISel/RA 染色 + EXEC kill 标记 + MOV128 伪层）先修才可能翻绿。
- **`vmsltu.vx v0` 这条曾经是"假绿"**：旧 pass 的 def-after-def 规则把 `vmsltu` 的 def 从 `$v9` 改到 `$v0`，**恰好**和厂商一致（厂商是 WAW：`$v9` 先被 `vmsne` 定义过，改名时选到空闲的 v0）。换成正确规则后我方一度露出 `vmsltu.v9`，原因是**活出保护用了 alias set**（`$v9` 与 `$v8m2` 共享 unit 9 ⇒ 被 `$v8` 的 live-out 误保护）；改 register unit 后 v0 由 WAW 自然产出，**不需要改 .td**（`XTsm_MV128_S_V` 的操作数类保持收任意 VR，与厂商行为一致）。

# ★★★ `vreg-rename` 按厂商规则重写（`0080` = 1023）：**`.rc` 逐字节 89 → 94 格；parity raw 218 → 226**（2026-10-18 00:28）
**6 个"纯染色"格中 5 个 `asm:IDENTICAL obj:IDENTICAL`**：`control_nested`、`control_divergent`、`mem_{l1,l0a,l0w}shared_u64_uniform`。第 6 格 `alloc_l1_short_ptr` **不是染色格**：差异是 shared 布局（厂商 `addi a2,a1,576` vs 我方 `addi a1,a1,128`，需 `-fraisa-short-ptr`；**obj 已 IDENTICAL**）⇒ 归阻塞②。
**四道门槛（同一二进制 00:28，两 sweep 0、`changed_inc.json=[]`）**：
| 门槛 | 前 | 后 |
|---|---|---|
| `.rc` oracle `--all` | 178 ident 槽 / 28 diff / 0 / 22 | **195 ident 槽 / 11 diff / 0 ours-fail / 22 both-fail** ⇒ **94/103 格逐字节**（前 89）；逐格变化**恰好 5 格、全 DIFF→IDENTICAL**、89 格一个没丢；另 **7 个 `alloc_*` 的 `obj:DIFF → obj:IDENTICAL`** |
| 317 parity | raw 218 / gap 0 / diff 32 | **raw 226 / gap 0 / diff 24**；**build-identity 归一化 = 235**（前 227）—— 两门槛都过 |
| musl（厂商-vs-我方） | 24/56+4 = 28/56 | **不变、无 success↔fail** |
| `mir_ab.sh` | — | **`control_divergent` diff 0 行**（独立证据） |
**潜在误编译专项**：探针 `logs/re16/vr/r4.mir`（`vid; vand(rmw); vmseq; vadd; vmul.mask tied; vse`）两侧 **post-`vreg-rename` MIR 逐字相同** —— masked `vmul` 的 tied passthru 与 `vadd` 同寄存器；端到端 `control_divergent`/`control_nested` 的 asm 里 `vadd.vx v8` / `vmul.vx v8,…,v0.t` / `vse32.v v8` 与厂商**逐字节一致**（旧实现是 `v2`/`v1`）。⇒ **今晚第一个真正的正确性 bug 已关闭。**

**规则要点（定稿措辞）**：触发是 **WAW 反相关**（**不要**再叫 def-after-def）；两条 passthru 保护（自身活 passthru 的 def 不改 + 活 passthru 消费的值由"反向遇到的下一个 def"一次性保护）；**V0 永不作改名来源**（但可作目标）；**别名/活出必须按 register unit**（alias set 会因 `$v9`↔`$v8m2` 共享 unit 9 而**误保护**，这正是 `control_early_return` 被漏掉的原因）；`RestoreAlignedCTInstruction` 已实现（`implicit $align2` 未加 —— 本树无 ALIGN2 寄存器、asm/obj 不可观测）。

**两个 loop 格的精确阻塞（已证不是本 pass）**：归一化指令名后，两侧 **pre-`vreg-rename` MIR 已差 178 / 728 行**（`$x13` vs `$x10` 之类**标量**物理寄存器染色、`implicit killed $exec` vs `implicit $exec` 的 kill 标记、缺 `PseudoMOV128_S_V_B32` 伪层、`BNE` 位置），本 pass 只再贡献 12 / 11 行 ⇒ **需上游先修**（ISel/RA 染色 + EXEC kill 标记 + MOV128 伪层）。

## ⚠️ 判定工具口径 bug（`0080` 线发现，已派修）：`tools/raisa_oracle.py` **忽略 manifest 的 per-cell options**
它把 `CC1` 写死成裸命令行（`-cc1 -triple … -O2`），**不读 `case["options"]`** ⇒ 带 `-fraisa-short-ptr` / `-mllvm -riscv-bank-aware=true` 的格是**在错误配置下比的**。
**实证**：`alloc_l1_short_ptr`、`alloc_l1shared_bank_on` 在**正确 options** 下两侧 asm **完全相同（0 行差）**，而 oracle 仍报 `asm:DIFF`。
（`corpus_parity.py` 读 manifest ⇒ **parity 数字不受影响**；只有 oracle 受影响。）
⇒ **修正后 `.rc` 逐字节应为 ≥96/103**（94 + 这 2 格）；修法 ~10 行（照 `corpus_parity.command()` 把 manifest options 过滤掉 `-print-*` 后拼上）。**已派 final-mile 线先修这个，再动别的。**

## `bank-aware-shared` 是**独立 pass**（不是 `TX8LowerSharedPass` 里几行）
厂商管线（`-debug-pass=Structure`，`alloc_l0ashared_bank_on -mllvm -riscv-bank-aware=true`）：`… -simt2simd **-bank-aware-shared** -rpu-lower-shared …`。
证据：厂商 **post-`simt2simd`** 的 module 里 bank 格的共享变量**已经变成 `inttoptr (i32 1073741824 / 1073872896)` 且 global 被删**（+128 KiB × 下标、声明序、**确定性**）；我方同一 dump 仍是 `@_ZZ…E1a/E1b` ⇒ `rpu-lower-shared` 走 base 路径给 +128 B。
**RE 已有**：`tools/re/out/TX8BankAwareShared/decompiled.c`（**12751 行** + callgraph）。**范围只 2 格**（l0a/l0w bank_on）；`alloc_l1shared_bank_on` 厂商也保留 globals ⇒ 那格属"布局"类。尺寸 ~200–300 行 + 注册 3 处 + 1 次构建。

## 修正口径后的**真实基线**（2026-10-18 00:45，`1024` 修了 oracle 的 per-cell options）
`.rc` oracle：**96 / 103 格逐字节 = 93%**（旧口径 94）；7 格部分一致；0 ours-fail。
**按正确 options 翻绿**：`alloc_l1_short_ptr`（`-fraisa-short-ptr`）、`alloc_l1shared_bank_on`（`-mllvm -riscv-bank-aware=true`）。
**口径变化（预期、非回退）**：`alloc_l0ashared_bank_on`/`alloc_l0wshared_bank_on` 在**错误** options 下是 `asm:DIFF obj:IDENTICAL`，在**正确** options 下是 `asm:DIFF obj:DIFF` —— 这正是缺 `bank-aware-shared` 的证据。
**剩余 7 格部分一致**：2 bank（`alloc_l0ashared_bank_on`/`alloc_l0wshared_bank_on`，缺本 pass）、3 base 布局（`alloc_{l0a,l0w,l1}shared_base`，仅 asm）、2 loop（`control_divergent_loop`/`control_uniform_loop`）。
**厂商 `bank-aware-shared` 的精确事实（已用 dump 确认）**：位置 `-simt2simd -domtree -early-cse **-bank-aware-shared** -rpu-lower-shared`；**flag 关时该 pass 根本不插入**（`-debug-pass=Structure` 只列 `rpu-lower-shared`、`-print-after=bank-aware-shared` 无 dump）；**只降 AS3(L0A)/AS4(L0W)**：L0A base `0x40000000`/6 banks/容量 `0xc0000`，L0W base `0x40100000`/2 banks/`0x40000`，**bank 步长 `0x20000`**，本语料一变量一 bank；**AS2(L1) 留给 `rpu-lower-shared`**；`setarch -R` 下确定性（6/6 次同两地址）。

# ★★★★ `bank-aware-shared` 重建（`0082` = 1025）：**`.rc` 逐字节 96 → 98 / 103 格（95%）**（2026-10-18 00:43）
**实现（证据有界）**：新 `TX8BankAwareShared.cpp`（ModulePass，`bank-aware-shared`）+ `RISCV.h`/`RISCVTargetMachine.cpp`/`CMakeLists.txt`/`TX8LowerSharedPass.cpp`；**仅当 `-riscv-bank-aware` 时插入**（与厂商一致：无 flag 时 `-debug-pass=Structure` 只列 `rpu-lower-shared`、`-print-after=bank-aware-shared` 无 dump），位置 `early-cse` 与 `rpu-lower-shared` 之间。
**语义**：① 先物化共享 global 的**所有常量表达式使用**（每使用点一次、自底向上 —— 这也解释了 AS2-only 的 `alloc_l1shared_bank_on` 为何只在此处变化）；② **只给 AS3(L0A base `0x40000000`, 6 banks) 与 AS4(L0W base `0x40100000`, 2 banks)** 按"一变量一 bank、步长 `0x20000`、声明序"摆放，超容量 = 厂商同样的 fatal；③ 换成 `inttoptr(i32 <addr>)` 并删 global；**AS2(L1) 留给 `rpu-lower-shared`**。
**几何交叉验证**：反编译的 `processAddressSpace`（`0x40000000/0xc0000/6`、`0x40100000/0x40000/2`、`uVar36=0x20000`）+ 6/6 重复运行 + 2/3 变量探针（**即使能塞进一个 bank 也一变量一 bank**）。
**未建模（文件头已注明）**：`BankConflictColoringSolver` —— 只有"多 bank 变量或 bank 溢出"才会走到，语料两者都没有；厂商所有 dump 走的都是简单路径。
**验收**：两个 bank 格 `asm:IDENTICAL obj:IDENTICAL`；`.rc` oracle **199 ident 槽 / 7 diff / 0 ours-fail ⇒ 98/103 格逐字节**（前 96），变化**恰好 2 格、96 集完整**；317 parity **226 → 228**、diff **24 → 22**、`.c` gap 0；build-identity 归一化 **235 → 237**；musl **28/56** 不变；潜误编译探针仍绿。
**剩余 5 格**：3 个 base 布局格（`alloc_{l0a,l0w,l1}shared_base`，**仅 asm**；`rpu-lower-shared` 布局/VENDOR-NONDET 类）、2 个 loop 格（上游 ISel/RA/EXEC-kill/MOV128 伪层）。

# final-mile 线：`1024`（判定工具口径）/ `1025`（`bank-aware-shared`）/ `1026`（`kcore_ncc_read`），2026-10-18 01:0x，`wt-merge-e`

**最终门槛（同一二进制 `wt-merge-e/build/bin/clang-21` 00:50:22；采数前两 sweep 均 0、`changed_inc.json == []`、`bin` mtime > 两个被改对象的 mtime）**

| 门槛 | 本线起点（旧口径） | 终点 |
|---|---|---|
| `.rc` oracle `--all` | 195 ident 槽 / 94 格（**错误口径**） | **199 ident 槽 / 7 diff / 0 ours-fail / 0 non-det = 98/103 格** |
| 317 parity（raw） | 226 / gap 0 / diff 24 | **229 / gap 0 / diff 21**，`.c` gap 0 |
| 317 parity（build-identity 归一化） | 235 | **238** |
| musl（`musl_norm.py` 厂商-vs-我方） | 24/56 + 4 = 28/56 | **24/56 + 4 = 28/56**，无 success↔fail |
| 潜误编译探针 | 绿 | 绿（`control_nested`/`control_divergent` asm+obj 逐字节） |

## 0) `1024`：oracle 忽略 manifest 的 per-cell options（**先修正口径再测**）
`tools/raisa_oracle.py` 原来把 `CC1` 写死成裸命令行，不读 `docx/tests/llvm/tx8_blackbox/round2/manifest.json` 的 `case["options"]` ⇒ 8 个格（`-fraisa-short-ptr`：`alloc_l1_short_ptr`/`mem_l1_multi_short_ptr`；`-mllvm -riscv-bank-aware=true`：4 个 `*_bank_on` + `mem_l1_multi_bank_on`）是在**厂商从未跑过的配置**下比的。
修法（照 `corpus_parity.py::command()`）：每格读 manifest、丢掉 `-print-*`/`-debug-*`/诊断类 flag（`-mllvm` 成对处理），其余追加到**两侧、每个 flavour（asm/obj/mir/--probe）**；同一 stem 出现两套非诊断 options 时直接报错；并把用到的 options 打到输出里。**这是 `tools/` 补丁，从树根 `patch -p1`（不是 `src/`）。**
**修正后基线 = 195 ident 槽 / 96 格**（旧口径 94）：`alloc_l1_short_ptr` 与 `alloc_l1shared_bank_on` 在正确 options 下 **asm+obj 逐字节**；同时 `alloc_l0{a,w}shared_bank_on` 由（错误口径的）`obj:IDENTICAL` 变 `obj:DIFF` —— 那正是缺 `bank-aware-shared`。

## 1) `1025`：`bank-aware-shared` 是独立 pass（**只 2 格，均转逐字节**）
厂商 `-debug-pass=Structure`（`alloc_l0ashared_bank_on -mllvm -riscv-bank-aware=true`）：`… -simt2simd -domtree -early-cse -bank-aware-shared -rpu-lower-shared …`；**不带该 flag 时 `bank-aware-shared` 根本不在管线里**（同一命令只剩 `rpu-lower-shared`，`-print-after=bank-aware-shared` 无 dump）⇒ **按 flag 条件插入**。
语义（厂商 `-print-before/after=bank-aware-shared` + `tools/re/out/TX8BankAwareShared/decompiled.c` 的 `processAddressSpace`）：
1. **先物化所有**共享 global 的常量表达式（`RPU::eliminateConstantExprUsesOfShared`，每个 use site 各一条链、不 CSE）——这也解释为什么 **AS2-only 的 `alloc_l1shared_bank_on` 也在这里被改**（但最终 asm 不变）；
2. 只给 **AS3(L0A) / AS4(L0W)** 排 bank：L0A base `0x40000000`、6 bank、容量 `0xc0000`；L0W base `0x40100000`、2 bank、容量 `0x40000`；**bank 步长 `0x20000`（128 KiB）**；变量占 `ceil(alignTo(size,align)/0x20000)` 个 bank，语料里每变量 1 个 ⇒ **声明序、每变量一 bank**（`a=base`、`b=base+0x20000`）。超容量 = 厂商的两条 `report_fatal_error`；
3. 换成 `inttoptr (i32 <addr>)` 并删除 global；**AS2 留给 `rpu-lower-shared`**。
**未复现**：`BankConflictColoringSolver`（只在"单变量跨 bank / bank 需求溢出"时可达，语料不触发；厂商 dump 走的就是上面的简单路径）。
**证据**：`setarch -R` 下 6/6 次同址；2 变量/3 变量探针也是"一变量一 bank"（不打包）。
**结果**：`alloc_l0ashared_bank_on`/`alloc_l0wshared_bank_on` **asm+obj 逐字节**；oracle 96→98 格；parity 226→228；0 回退。

## 2) `1026`：`kcore_ncc_read` 的 C++ `Select()` 形状（`.c` 格）
厂商 `finalize-isel`：`%0:gpr = COPY $x10; $x0 = COPY %0:gpr; %1:gpr = Kcore_Ncc_Read $x0; $x10 = COPY %1:gpr` ⇒ asm `kcore_ncc_read a1, zero; mv zero, a0; mv a0, a1`。旧 `Pat` 把实参直接接进 `rs2` ⇒ `kcore_ncc_read a0, a0`。TableGen 无法表达"丢掉具名输入"，故在 `RISCVISelDAGToDAG::Select` 的 `INTRINSIC_W_CHAIN` 里加 case：`getCopyToReg(X0, Arg)` 造出那条 `$x0 = COPY`，机器节点读 `$x0`，并把 intrinsic 的 **chain 改接到该 copy 上**（否则 `RemoveDeadNodes` 会把它删掉，同时也保证 copy 在 read 之前）。
**结果**：`.c` 格 `builtin_builtin_riscv_kcore_ncc_read` parity raw **byte-diff → byte-identical**（唯一残差 `# %bb.0  # %entry` 注释，parity 口径会剥）；228→229、diff 22→21、归一化 238；`.c` gap 仍 0；musl 不变。

## 3) 三个 `alloc_*_base`：**布局随命令行/堆状态变**（登记 VENDOR-NONDET，放弃）
`alloc_l0ashared_base`/`alloc_l0wshared_base`/`alloc_l1shared_base` 的 asm 差异 = shared **布局顺序**（厂商 `addi a2,a1,576` = `b@+0, a@+576`；我方 `addi a1,a1,128` = `a@+0, b@+128`）。
**实测（都是同一源码、`setarch -R`）**：
- 同一条命令行 **8/8 次 md5 完全相同**（`a87bc2…` 等，3 格各自稳定）⇒ **不是 run-to-run 抖动**；
- **但只要加一个纯诊断 flag 就翻转**：无 flag / `-print-before`+`-print-after` ⇒ `b@+0,a@+576`；只加 `-print-after` 或只加 `-print-before` ⇒ `a@+0,b@+128`（= **我方布局**）。重复 2–3 次稳定。
⇒ 布局由**进程堆/指针值**（`getTransitiveUsesOfShared`/`SharedUsesInfoTy` 的指针键容器）决定，命令行任何分配变化都会改它。**它属于 `VENDOR-NONDET`（命令行/堆相关），逐字节不是良定义目标**；我方产出**恰好等于厂商自己在另一种 flag 组合下（以及 obj 口径下）给出的布局**，所以 `obj:IDENTICAL`。**不要为它改 `rpu-lower-shared` 的排序。**
（附带更正：`wt-raisa-r2/tools/rpu_nondet.py` 的 `check_sequential()` 公式写错（`(off+(1<<40))//al*al` 会返回 `2^40`），它报的 `seq-perm=NO` 不可信；其 map 本身在 `-R` 下是正确的。）

## 4) item 2（`control_{uniform,divergent}_loop`）尺寸评估：**没有做，建议不做**
用 `-stop-before=vreg-rename` 的 MIR（内含 post-`simt2simd` IR）逐层量：
- **IR 层就已经不同**：归一化后 `control_uniform_loop` **406 行**、`control_divergent_loop` **292 行**（`Flow` 块编号/CFG、`nor.s` 死入口链、lane 地址 `ptrtoint/lshr/trunc+vmv/vadd` 形状、`vle`/`vmsne`/`and.s` 次序）⇒ 不是"只差机器层"，**先要 simt2simd 的 lane 地址/CFG 对齐**（与 `-mllvm -riscv-raisa-lsr` 那个 stopgap 同源）。
- **机器层**（只 diff 机器函数体）：`control_uniform_loop` 480 行（`$xN` 染色 207、`implicit [killed] $exec` 156、其它 104、`$vN` 13）；`control_divergent_loop` 195 行（`$exec` 95、`PseudoMOV128_S_V_B32` 16、`$xN` 78、其它 6）。
- 四件里没有一件是"接线"：EXEC kill 标记要按块尾 use 计算（并在 IR/染色对齐后重测）、MOV128 伪层要 TableGen 全量重建（~10 min/次级联）、染色差多半是前三件的**后果**（每改一件必须重测）。
⇒ **建议停在 98/103**：这两格只影响 parity 的 2/317，且改动面（annotate / simt2simd / legalize-exec / .td）**正好压着现在逐字节的 `control_nested`/`control_divergent`/`control_early_return`**，回归风险 > 收益。

# ★★★★★ 编译器收尾总账（2026-10-18 01:00）
## 最终水位（`wt-merge-e`，同一二进制 00:50:22，两个 sweep 0 / geninc OK / changed_inc=[]）
| 维度 | 结果 |
|---|---|
| **能编译**（厂商能编的 103 格） | **103 / 103 = 100%** |
| **逐字节一致** | **98 / 103 = 95%**（199 ident 槽 / 7 diff / 0 ours-fail / 0 non-det） |
| 其中 **VENDOR-NONDET** | **3 格**（`alloc_{l0a,l0w,l1}shared_base`）—— 见下，**不是可定义的差距** |
| **317 parity raw** | **229** / gap **0** / diff 21；**build-identity 归一化 238 / 317 = 75%**；`.c` 零缺口 |
| musl（厂商-vs-我方，剥 `.comment`） | **24/56 + 4 = 28/56 outcomes**、无 success↔fail |
| **潜误编译** | **已修并验证**（`control_nested`/`control_divergent` 的 asm+obj 逐字节） |
| 补丁 | **`0047`–`0083`**（37 个，全部 `patch -p1` 回放验证） |

## 3 个 `alloc_*_base` 是 **VENDOR-NONDET**（决定性证据，别再追）
同一命令 **8/8 md5 相同**（`setarch -R` 下**不是** run-to-run 随机），但**只要进程堆状态一变布局就翻转**：
无 print flag ⇒ `b@+0, a@+576`；**只加 `-print-after` 或只加 `-print-before` ⇒ `a@+0, b@+128`（正是我方布局）**；各复现 2–3 次。
⇒ 布局由厂商的**指针为键的容器**驱动，**byte-identity 在此不是良定义目标**；而我方输出**恰好是厂商自己观测到的布局之一**（且是它 `-emit-obj` 口径下那个，所以 `obj:IDENTICAL`）。**不要改 `rpu-lower-shared` 的排序去凑它。**
（另：`wt-raisa-r2/tools/rpu_nondet.py::check_sequential` 的 `((off+(1<<40))//al)*al` 有 bug、返回 2^40 ⇒ 它的 `seq-perm=NO` 无意义；其 per-variable 映射在 `-R` 下可用。）

## 2 个 loop 格的尺寸评估（**已测、不推荐做**）
`control_{divergent,uniform}_loop` 的差异**不只在机器层**：`-stop-before=vreg-rename` MIR 显示 **IR 层已差 406 / 292 归一化行**（`Flow` 块编号/CFG、死掉的 `nor.s` 入口链、lane 地址 `ptrtoint/lshr/trunc+vmv/vadd` 形状、`vle`/`vmsne`/`and.s` 顺序）⇒ 前提是 `simt2simd` 的 lane-address/CFG 对齐（与 `-riscv-raisa-lsr` stopgap 同根）。
机器体另差：uniform loop **480** 行（`$xN` 染色 207、`implicit [killed] $exec` 156、其它 104、`$vN` 13）；divergent loop **195** 行（`$exec` 95、`PseudoMOV128_S_V_B32` 16、`$xN` 78）。
**四项都不是接线**：EXEC kill 要按块 last-use 逻辑且每修一项要重测；MOV128 伪层要整轮 TableGen 级联（~10 min）；染色差很可能是**结果**而非独立修复。**爆炸半径正好压在已逐字节的 `control_nested`/`control_divergent`/`control_early_return` 上**。**估 1.5–3 个专注 session + 真实回归风险，换 2/317 parity、0/103 oracle ⇒ 不推荐。**

## 今晚总账（从 `parity 132 / oracle 0 / 大量格编不过` 到）
- **能编译 103/103**；**逐字节 98/103（+3 VENDOR-NONDET）**；**parity raw 229 / 归一化 238**；`.c` 零缺口；**0 回退**。
- **找到并修掉 1 个真正的误编译**（`vreg-rename` 拆开 masked op 的 tied passthru）。
- **重建的 fork 部件**：`simt2simd`（全部族 + lane 地址 + 多 BB）、前端共享内存地址空间、`tx8-annotate-control-flow`、`structurizecfg` 接入、`TX8LegalizeEXEC`、`vreg-align`+ALIGNED、`vreg-rename`、`bank-aware-shared`、伪指令层、540 项 bit-8 真表、bf16 孪生/归约、`config-instr-opt`、`rpu-lower-shared`、调度模型 override、LSR stopgap。
- **六类工具地雷**全部入册并修复：stale 对象四连、`RISCVGenInstrInfo.inc` 错位、`--force-src` 静默 0 匹配、跨树 `rebuild_one.sh` 污染、喂厂商 IR 的 oracle 污染、`mir_ab` 空文件假象；外加 **oracle 不读 per-cell options**（口径 bug）。

## 5) `1027`：3 个 `alloc_*_base` 正式登记为 `VENDOR-NONDET`（**工具改动，无编译器改动**）
- `tools/raisa_oracle.py`：新增 `VENDOR_NONDET_CELLS`（3 格）+ `NONDET_VARIANT`（`-mllvm -debug-pass=Structure`，纯诊断）。对这些格**额外把厂商再编一次**（只变堆状态），我方 asm 等于**任一**厂商布局 ⇒ `asm:IDENTICAL-VENDOR-NONDET`；汇总**同时打两个数**：
  `== cells: 98/103 byte-identical;  + 3 VENDOR-NONDET = 101/103 under the layout-set caliber -- NOT 103/103`（明确不伪装成 100%）。
- `tools/round1_attrib.py`：同样 3 格给 `vendor-nondet` 判定（**不计入** byte-identical、不参与 byte-diff 归因），并打同一对数字（parity 口径下这 3 格中 `alloc_l0wshared_base` 恰好直接 byte-identical，因为 parity 的命令行带着 `-print-after`，厂商在该堆状态下就给出我方布局 ⇒ 实际是 `229 + 2 = 231/317`）。
- `wt-raisa-r2/tools/rpu_nondet.py::check_sequential` 的公式 bug 已修（`(off+(1<<40))//al*al` → `(off+al-1)//al*al`；旧式对任何小 `off` 都返回 `2^40`，使**所有**布局被判 `seq-perm=NO`）。它打印的 per-variable map 一直是对的，只有那个判定是错的。

## 收尾工具改动（`0084` = 1027）：把 VENDOR-NONDET 做成**显式类别**并**实际校验**（2026-10-18 01:10）
- `tools/raisa_oracle.py`：新增 `VENDOR_NONDET_CELLS`（3 个 `alloc_*_base`）+ `NONDET_VARIANT = -mllvm -debug-pass=Structure`（**仅诊断用**）⇒ 对这些格**再编一次厂商**（该 flag 只移动厂商堆状态），我方 asm 等于**那次厂商产出的布局**时报 `asm:IDENTICAL-VENDOR-NONDET` ⇒ "落在厂商自身观测布局集合内"是**被校验的**、不是断言的。headline **打两个数**（slots 行也有独立的 `identical-vendor-nondet` 桶）。
- `tools/round1_attrib.py`：同样 3 格 verdict = `vendor-nondet`，**排除出 byte-diff 归因、也绝不计入 byte-identical**，并打同样两个数。
- 证据写入两个工具的注释 + README：**同一命令行 8/8 md5 相同**（`setarch -R` 下不是 run-to-run 随机），但**一个纯诊断 flag 就能让 `b@+0,a@+576` ↔ `a@+0,b@+128`（=我方）翻转**；且**哪个 flag 会翻转取决于命令行其余部分**（源码绝对/相对路径、`-o` 文件 vs `/dev/stdout`、flag 位置）⇒ 由**进程堆状态**（指针为键的容器）驱动 ⇒ **byte-identity 不是良定义**。
- **`wt-raisa-r2/tools/rpu_nondet.py::check_sequential` 的 bug 已修**：`((off+(1<<40))//al)*al` → `((off+al-1)//al)*al`（旧式对任何小 offset 都返回 2^40 ⇒ **每个布局都打 `seq-perm=NO`**）；smoke 后现在正确。

## 最终数字（同一二进制 `clang-21` 00:50:22；两 sweep 0/0、`changed_inc.json=[]`、此后未构建）
- **`.rc` oracle**（`logs/oracle_final.txt`）：`== asm+obj slots: 199 identical, 3 identical-vendor-nondet, 0 perm, 4 diff, 0 ours-fail` ⇒ **98/103 逐字节；+3 VENDOR-NONDET = 101/103（layout-set 口径，明示"NOT 103/103"）**；4 个 diff slots = 那 2 个 loop 格。
- **317 parity**（`parity/cf9.tsv`）：**byte-identical 229 / byte-diff 19 / vendor-nondet 2** / vendor-fail 33+33 / ours-extra 1 / **OURS-GAP 0、`.c` gap 0** ⇒ **229/317 raw；+2 VENDOR-NONDET = 231/317 layout-set 口径**。
  - **为何 parity 只有 2 个**：parity 自己的命令带 `-mllvm -print-after=rpu-lower-shared`，**在该堆状态下厂商对 `alloc_l0wshared_base` 恰好就发我方布局** ⇒ 它在本口径下已是 `byte-identical` —— 这**独立印证了堆依赖**。
- **build-identity 归一化 parity**：**238/317**（本 session 开始时 235；若把 2 个 `vendor-nondet` 按 layout-set 口径计入 = 240，标注为派生）。
- **musl**：**raw 0/56 + 4；剥 `.comment` 24/56 + 4 = 28/56 outcomes**、无 success↔fail。
- **潜误编译探针**：`control_nested`/`control_divergent` **asm:IDENTICAL obj:IDENTICAL**（`vadd.vx v8` / `vmul.vx v8,…,v0.t` / `vse32.v v8` 完整）。

## 最终补丁清单（全部回放验证逐字节）
`1024`（162 行，tools）oracle 读 per-cell options · `1025`（494 行/8 hunk/5 文件，src）`bank-aware-shared` · `1026`（58 行/1 hunk，src）`kcore_ncc_read` Select 形状 · `1027`（191 行/7 hunk/2 tools）VENDOR-NONDET 显式类 + 第二布局校验。

## REA 轮 2（GEMM/NCC tensor 路径）：字段表 + 10/10 逐位验证 + 三处更正（2026-10-18 12:10）
**① GEMM 不走 `Tx82_Cgra_Tensor::decode`**（那是 CGRA 向量 op 的链）。真实链：`NCC::handle_kcore_to_ncc_if_protocol_engine_b_transport` → `SubmitCommand` → 调度队列 → `DispatchReadyModuleQueues` → `DispatchSchedulerQueueEntry`（按 `module_id` 路由）→ `NCC::NeDispatch` → `ne::handle_ncc_to_ne_if_protocol_engine_b_transport` → `ne_split_*` + `GemmOp`（`gemm_opt.cc:765`）。**厂商日志前缀 `[NE]` 就是证据。**
**② `param0/1/2` = `X[rs1]`/`X[rs2]`/`X[rd]`**（位段 19:15/24:20/11:7）—— **轮 1 的 `rs2/rs1/rd` 是反的**，8 条日志逐位钉正。
**③ 陷阱：`llvm-objdump` 把 custom-3 私有指令打印成 `<op> rd, rs2, rs1`**（不是标准 R 型 `rd, rs1, rs2`）。例 `dte_set_length a2,a0,a1` 真值 **rd=a2、rs2=a0、rs1=a1** ⇒ **照打印文本当 rs2 会把 param0/param1 整体换位**（轮 1 犯的就是这个）。**判操作数只认位段。**
**GEMM/CONV/MMA 共用字段表（已验证域）**：`opcode=(insn>>7)&0x1ffffff`；`param0[0:31]`=**spmA**、`param0[32:63]`=**spmW**（窗口 `0x4010_0000`）、`param2[0:31]`=**dst**、`param2[32:63]`=**psum**（无则 0），**单位全为 2 字节（`addr>>1` 打包）**，硬证据 `GemmOp::GemmOp` 的 `host = base + reg*2 - 0x40000000`（spmA/dst/psum）与 `- 0x40100000`（spmW）；`param1`=aparse（**gemm/conv 的 4 参形式里被丢弃**）；`reg[25..27]`=`ne_set_mkn` 的 M/K/N（u16 元素数）。
**opcode 实测**：`gemm`=0x8c0a0b、`gemm_psum`=0x9dca0c（conv*/mma* 仅静态证据）。**quirk**：`gemm` 丢 aparse；`mma_first_psum` 的 psum **只放 param2 高半**。
**验证 10/10**：裸 `.word` 发 5 条私有指令跑厂商 cmodel，`[NE] CMD_ID … param0/1/2` 与预测**逐位相同**（+轮 1 的 4 DTE + 1 KCORE）。**日志在命令提交时就打，GEMM 即使 abort 也是有效 oracle。**
**分区**：已验证 = opcode 抽取/param 归属/`(addr>>1)` 打包/param2 高低半/GEMM·GEMM_PSUM opcode/`ne_set_mkn/precision/length` 位域/GEMM 属 NE；仅静态 = conv*/mma*、`ne_split_*` 落点、`GemmOp` 字段消费；未做 = `GemmOp` 内部数值语义（`src/engines` 层）。

## ⚠️ 口径更正（batch D 实测，parent 采纳）：「可开工」= 基线 8 + 增量，不是增量本身
固定工具、同一 `.so`（`build-runtime/lib/libtx82_cmodel.so`）实测：
- 基线（**无** `--impl-extra`）= **8** 可开工；
- `--family` gemm+DTE+x3d（16 符号）= **+34** ⇒ 可开工 **42**（**不是 34**）；
- spma 三符号进库后 = **44**（spma 贡献 **+2** ✓）。
⇒ 此前"34 of 75 startable"应读作"**解锁 34 条，加基线 8 = 42 条可开工**"。
**batch D 的 4 家族合计 +7（不是 +6）**：fill +1、krt pair +2、spma +2、**process_a +2** —— 因为把 13 个 `impl-extra` 放进闭包后，`process_a_with_part_b` 的闭包缩到 **1892 B**（本体 1222 + `dte_load_gemm_data_from_ddr` 346 + `dte_write_gemm_data_to_ddr` 324），可解锁 **2 条**（`mm_tile_m_dont_split_k` + `mm_tile_mn_dont_split_k_noc_m`），**但那两个 DTE 助手尚未实现 ⇒ 做 process_a 必须连它们一起做**。
**最终预期：+41（8+41 = 49 / 75）**，与 `CMODEL_MARGINAL` 的"全部 21 符号 → +37 / 25 符号 → +41"一致。
**另**：`lsu_stride_load_gemm_data_from_n3d`（314 B）是**独立 +1** 候选（解锁 `matmul/main_kernel_scaled_bmm_tile_m_split_bkn`），便宜。
**spma triad 已通过**：137 checks / 0 failures、13/13 变异全红；两条真值钉出来的事实：① `get_next_tile_id` 的回绕支取 `table[0]` 被 GCC **折成常量 4**（厂商 `18ee6: li a5,0x4`）⇒ 该支**不再调** `krt_get_tile_num_per_die`，而 `get_prev_tile_id` 的回绕支会再调一次（调用次数进对拍：`{1,4,4,1}` vs `{1,4,0,2}`）；② `spma_n3d_once_sendrecv_diff_size` 里 `krt_get_die_num_per_chip` 在两条主路径**各调一次（共 2 次）**，`tile==0` 的跨 die 支是 3 次。
**另**：`wt-runtime` 工作树曾被回滚而构建目录残留已删源的 `.o` ⇒ 直接重建会让 **14 个运行时助手符号从 .so 消失**（可开工 8→0）；batch D 先落了基线提交 `9eacc78`（`patches/01` 原样 apply + 补进修好的 `marginal_closure.py` 与 `device_callees.strict.json`），重建后 `nm -D` 与改动前**符号集完全一致**。**注意 `patches/00` 是 `patches/01` 的子集**，只 apply 01。

# ★★★★ 【通用解锁】0x7B(custom-3) 不是"另一种语义"，而是**标准 RVV 的厂商重编码**（2026-10-18 12:25，RVV 布局线实测）
**结论**：厂商 LLVM 把标准 RVV **重编码**进 `0x7B`（custom-3）：`funct6/funct3` 查 `RVV2CTFunctMap`，**`vd/vs1/vs2/立即数原样不动**；**厂商自己的 CModel 两种编码都认并执行**（`docs/compiler/RVV_TO_0x7B_REMAP.md`）。
⇒ **"含 TX8 私有向量指令 ⇒ 没有数值预言机"这个结论，对向量族是错的。** 把函数字节原样装进一个设备模块（只把 3 处 `jal <PLT槽>` 改成符号调用），用 `txModuleLoad` + `txLaunchKernelSimd` 喂参数、**由厂商 CModel 跑**，就是**数值预言机**。
**⚠️ 坑（会给出"假真值"）**：出入 SPM **必须走厂商 DTE**（`dte_rdma/wdma_memcopy`）。**CPU 写 SPM 与向量单元写 SPM 之间有 D-cache 别名**：CPU 的脏行会在向量写之后被写回/被读到 ⇒ 症状是"输出头部若干词恒为旧值，且每次长度不同"。它先拿到两批这样的假真值，靠"**两次跑逐字节相同**"这条硬断言卡住才定位。**`fence` 不够，换 DTE 才干净。**

## RVV 布局转换器：两个符号都 bit-exact（`tx82_device_layout_rvv.cpp`）
- 实现：逐指令直译（每条语句带厂商地址），地址翻译走 `tx82_layout_rvv_host_ptr` 接缝（库里=CModelMemoryManager、差分测试里=原样返回）；`tx82_device_layout_rvv_mem.cpp` 提供库内那份；**abort 存根已删，两个符号现在是真 `T`**。
- 真值：`docs/recovered/device-layout-rvv-oracle.inc`，**20 用例**（主块组/余块组/两者/都没有/多外块 64 行/多行块 33,64/dtype 0,1,2,3,5,6,9,10），**每用例两侧各跑两遍且逐字节相同才收录**。
- 差分测试：`test/test_device_layout_rvv.cpp`，20 用例 × {c2ne, ne2c, round-trip} ⇒ **26,861 词逐字节、0 失败**。
- **冒烟：第一个 gemm 入口 `..._2k_2k_2k_tile32_b_trans` 已完整 returned**；其余 19 个正按"一入口一进程 + gdb 断点计数"逐个统计。

## ⚠️ 撤回一条前提 + 一个新的测试盲区（2026-10-18 12:40，夹具线实测）
**撤回**："6 个 gemm 入口卡住 = 夹具限制" —— **错**。真正原因是**入口生成器的调用点 ABI 缺口**：
- `entry_to_cpp.py` 的 `ARITY` 表把 `dte_rdma_gather_copy` 写成 **3** 个实参，而厂商体 `optimized_gemm.so @0x11b04` 是 **9 实参**（`mv s7,a0; mul a0,a6,a2; ld s0,0x60(sp); mul a0,a0,a7; mul s9,a0,s0; … dte_set_rdma_src_itera_stride(a8,a5,a7,a4,a6,a3)`）；
- 入口**自己确实**在每条 `jal` 前写好 a3..a8（`X[13]=X[26]; X[14]=0; X[15]=0; X[16]=256; X[17]=1; sd s7,0(sp)`），但**生成的 C++ 只传 3 个参数** ⇒ 那 6 个从未到达被调者；
- 症状（gdb 6 s SIGINT）：PC 停在 `dte_gather+246`，栈 `dte_gather ← dte_rdma_gather_copy ← main_kernel_gemm_3072x1024…`；`dte_set_rdma_src_itera_stride` 实参是**宿主垃圾寄存器**（`itera0=0x2000000020=1.37e11`、`itera2=栈地址`）⇒ `dte_gather` 三层环永远转不完。
**⚠️ 新测试盲区（重要）**：差分测试**看不见**这一层 —— 探针 `probes/gemm_entry_trace.c` 的 `REC3(dte_rdma_gather_copy,…)` **也只有 3 个形参**，两侧都只记 a0..a2 ⇒ **620 用例全绿也照样漏**。**教训：记录器的元数必须等于厂商被调者的真实元数**，否则"记录器口径 = 入口发出的实参"这条假设会把 a3..a8 整段藏起来。
**修法与结果**：`ARITY 3 → 9`（注释给厂商体证据）+ 重生成（**只动 8 文件/144 行**，全是该符号的 proto 与 9 实参调用点，含 `Ld(X[2]+0)` 取栈实参）+ 重建 rc=0 ⇒ 冒烟：`3072x1024_tile24` **c2ne=6 ne2c=8 RETURNED**（之前 c2ne=0/40 s 超时），**5 个 `*_b_trans` 全部 RETURNED**。
**仍存在的夹具边界**（不是本次原因）：`1k_4k_2k` 两族在 gmem 扩容时 SIGABRT/`bad_alloc`；`1k_4k_2k_tile32` 与 `2048x2048x2048` 仍超时。
**下一步**：探针记录器改 9 形参并打印 a0..a8 → 重生成真值 → 重跑 620 用例（必须仍绿，这次真的把 a3..a8 钉住）→ 20/20 表 → ctest → 文档。

# ★★★★★ gemm 20/20 入口**全部端到端跑通**（2026-10-18 12:55，夹具线 + ABI 修复）
**100 次运行（20 入口 × tile {0,3,4,5,7}）全部 RETURNED**，0 次 SIGSEGV/SIGABRT/CG_OOM/MEMCAP；单次 0.62–2.80 s。
**非平凡证据**：两个 RVV 转换器**真的被执行 4–24 次**；设备侧张量被真的改写 **1.0–256 KB**；SPM 侧 wdma/rdma 实测在 **`0x40040000 ↔ 0x101000000`** 之间搬 **128–256 KB**。（0.00 s 的那些是**厂商入口自己的** tile 守卫早退，非夹具限制。）

## 三个**夹具真错**（都值得记，因为每一个都会造出"假卡住"或"假绿"）
1. **P 的 64 位字段必须按 64 位写**：`+0x00/+0x08/+0x10/+0x18`（四个地址）与 `+0x20/+0x28` 都是被 `lwu 低半 + lwu 高半 + slli 32 + or` 读的**一整个 8 字节字段**；旧夹具在 `+0x20/+0x24` 各写一个 32 位值 ⇒ 64 位读出来 `0x20_00000020` = **1.37e11**。**改对后 `1k_4k_2k_tile24(_b_trans)` 的 `bad_alloc` 与 `2048x2048x2048(_b_trans)` 的 40 s 超时同时消失。**
2. **张量按入口名里的真实 M/N/K 分配**（A[M,K] B[K,N] C[M,N] D[M,N]，bf16 2 B，各 +32 MiB；最大 64 MB，远低于 2 GiB cgroup）。
3. **拓扑写真实 PG**：`+0x38=32`（4 die × 8 tile 的全局 tile 数，入口守卫 `global_tile_id < P+0x38`）、`+0x3c=4`、`+0x40=8`（只被 `set_tile_topology` 自检；写对后厂商那两行 "user DIE_NUM(…) is not equal" 噪声消失 —— 厂商 printf 实参本身错位，真值里已钉死）。

## 根因（c 类真缺口，已修，**全族只有 1 个**）
`entry_to_cpp.py` 的 `ARITY` 把 **9 实参**的 `dte_rdma_gather_copy` 记成 3 → 生成入口只传 a0..a2 ⇒ 我方 9 实参实现读到宿主垃圾。**修复三件**：ARITY 3→9（重生成只动 8 文件/144 行）+ 探针改 9 形参打印 a0..a8 + 真值重生成（md5 `0050142e…`，厂商侧两次逐字节相同）。**全族 29 个被调者按"读之前没写过"重核 ⇒ 只有这 1 个少传**；补两条变异（a6 256→257、栈实参 a8 1→3）均转红。
**验收**：620 块 / **6,663,028** 字节逐行一致（旧 6,523,636 的差就是新记的 6 个实参）；`ctest 190/190、Skipped 0`。

## 残留分类
- **(a) 夹具足够：20/20**；
- **(b) 需要本仓没有的数据**（只影响"是否符合真硬件"，不影响"能否跑"）：`P+0x20/+0x28` 的真实语义 —— 本仓**没有**厂商 host 侧 launcher（`docx/tests/llvm/gemm-walkthrough` 那套 48 字节 `GemmModuleArgs` 与 oplib 的 P 块**不是同一套**，不能当字段证据）。行为反推：`+0x20=v` 时 gather 源偏移按 `(v-1)*0x1000` 增长、`stride2=2v`；夹具取 1（最小非退化，取 0 = **假成功**），实测 v∈{1,2,8,32,1024,4096} 六档结论不变；
- **(c) 真实现缺口：1 个，已修**（就是上面那个 arity）；引擎/转换器无缺口。

## 两条给后续线的提醒
1. **`build-gemmpsum/lib/libtx82_cmodel.so` 已按 HEAD 重建**；任何人依赖 12:39 那份旧库做冒烟，会看到 6 个入口 `c2ne=0`（那是旧库）。
2. 入口 `.cpp` 是**生成物**：跨 worktree 搬运时**先取 `entry_to_cpp.py`** 再跑 `gen_gemm_entries.py --check`，否则重生成会把 9 实参改回 3。

# ★ 工具修正：`jalr` 常量目标 + LOCAL `t` 不可插桩（2026-10-18 13:40）
**根因 1**：`device_callees.py` 用"线性最近定义"解析 `jalr` 目标 ⇒ "auipc+addi ×2 → beqz 选择 → jalr" 只保留一个目标。修法 `augment_conditional_indirect()`（基本块 CFG + 前向常量传播；块入口 must-merge、值集合并、按丢弃加宽使递减环收敛）。全语料 **+10 边 / 9 个多候选 jalr 点**。
**根因 2（校验时发现）**：`marginal_closure.clean()` 把**所有** private callee 当"可切"，但 **LOCAL `t` 不可插桩** —— `n3d_fill_head_tail_3d` 是 LOCAL（2,752 B，含 RVV）⇒ 即使补了边也被静默丢掉。修法 `required()`（clean 或 LOCAL-private 必须；private 的 GLOBAL/WEAK/UND 才可切）。只有 **1/75** 条受影响（`n3d_fill`）。
**新基线**（同一 lib `build-runtime` 13:32，2209 dynsyms）：75 条可验证未实现；**12 可开工**。工具 fix 本身 12→12（文档里的"8"是 10-16 的旧库；8→12 是 batch D 的 spma+krt）。
**修正后的 top 表**：`x3d_copy_all` **+3/5,540 B**；`process_a_with_part_b` **+2/8,000 B**（闭包含 gemm 族）；`n3d_fill_all` **+1/4,374 B**；`gemm_with_psum_v2` **+1/6,136 B**；`gemm_one_block` **+1/11,520 B**。
**家族 old→new**：X3D 3/3,506/+3 → **4/5,540/+3**；FILL 3/1,622/+1 → **4/4,374/+1**（语义真实闭包 3/4,260 B，因 `a6=0`）；G 9/8,652/+20 → **+21**、G+DTE 13/9,272/+31 → **+32**（**lib 漂移，非本次 fix**）；G+DTE+lsu 14/9,586/**+33**（lsu 单独 **+0**）；process_a+2 DTE 3/1,892/**+2**；K2 与 SPMA **+2 → 0**（batch D 已实现）。
**新发现的 7 条此前不可见边**：`max_min` 的 `process_vs_{bf16,fp16,fp32}_* + rvv_*_max_vf_wrap`、`process_vv_* + rvv_*_max_vv_wrap`（6 条）、`pool/main_kernel_pool3d_ndhwc + process_{bf16,fp16}_impl` —— **谁实现它们就必须把 rvv wrap / process impl 一起做**。
全量 129 行 old→new TSV：`docs/recovered/device_callees-correction-2026-10-18.tsv`；回归测试 6/6 OK（模拟回退 4 RED）；更正写在 `CMODEL_MARGINAL_2026-10-18.md` 的"更正 2"。
**派工含义：fill 不是 1,622 B 的小件（≥4,260 B 含 RVV）；x3d 是 5,540 不是 3,506。符号层 top 边际已只剩 +1/+2/+3，真正的大头转到入口本体。**

## ⚠️ 共享工具提升的教训：**必须合并两边的修复，不能只取一份**（2026-10-18 13:50，parent 之误）
我把 `wt-helpers` 的"扩展版" `entry_to_cpp.py` 提为共享 `tools/entry_to_cpp.py` 时，**丢了 wt-gemmpsum 版里的 `ARITY["dte_rdma_gather_copy"] = 9` 修复**（那正是造成 620 条假绿的 bug）—— 等于把已修的 bug 重新引入。
mm/bmm 线发现并**合并**了两版（canonical 的 `sym_at`/`EXTRA_REGS`/`VOID_RET`/`--stack` + 9 实参），用 `gen_gemm_entries.py --check` 验证：重生成 16 个 gemm 体**只差一行注释**、无功能差异。
**parent 已把合并版提回共享 `tools/entry_to_cpp.py`**。**教训：提升共享工具前先 `diff` 两份、逐条合并已修项；"版本更新"不等于"功能更全"。** 任何 worktree 取用这份 canonical 时，应同时跑 `gen_*_entries.py --check` 验证 `ARITY`。

## matmul/mm/bmm 入口体：A 档 6 条落库（2026-10-18 14:10）
**30 入口 → 30 唯一体、零别名**（`_new`/`_no_trans`/`inter_die` 两两都不同，与 gemm 20→16 相反）⇒ 这批没有"别名派生"可省。
**6 条体已还原**：真值 51,276 行 / 3.54 MB（厂商 qemu 两跑逐字节相同）；差分 **60 块 / 0 行差异**；变异 **4/4 红→绿**（含 M1 生成器 `ARITY[with_ratio] 7→6` 与 M4 **探针记录器 arity 7→6** —— 正是"少传/少记"两侧盲区的闸门）；`ctest 191/191、Skipped 0`（gemm 620 块与 RVV 无回归）。
**诚实分类**：**(a) 已还原并对拍 5 条**；**`bmm_tile_m_split_bkn` = (b)**（读集不收敛 + 无 host launcher ⇒ 真值只留 cold 用例，并做**反向断言**"它若进主路径即报错"，防登记过期）；**3 条 `mm_*` 的冒烟 = (c)**（停在 `dte_rdma_src_stride_copy(_with_ratio)` 的**响亮中止 weak 存根**，实现在 DTE 批 wt-implscale 未合入）。
**两个新坑（都已固化）**：
1. **生成器漏"尾部调用"**：mm 入口最后一条是 `j <wait_cmd_done 的 PLT 槽>`，旧 generator 当 `goto` ⇒ 编不过（label 未定义）。补 `tail_name()/call_args()`，`jal` 与 `j` 共用实参表；**重生成 16 个 gemm 体逐字节不变**。
2. **夹具要"读集发现"**：mm 入口的守卫/循环计数藏在**寄存器间接寻址**的字段里（`P+0xa8/0xc0/0xc8`），只扫 `base+立即数` ⇒ 入口在第一条守卫就退出（trace 2~3 行）。做法：给生成体的 `Lw/Lwu/Ld` 加钩子记录**实际读到的 P 内偏移** → 迭代填充到集合收敛（每入口 37~43 字段），调用数从 2~3 涨到 **177/503/504/1004/1005**。**代价**：把 64 位字段分两个 32 位写会让值变 8.6e9，厂商侧 **60 s 写出 1.3 GB trace**（磁盘一度 306 G）⇒ **"8 对齐偏移按 8 字节写"已固化成夹具约定**。
**整合债务（MERGE BLOCKER）**：`src/device/tx82_device_mm_entry_prereq_stubs.cpp` 里两个**响亮中止 weak 存根**（`dte_rdma_src_stride_copy(_with_ratio)`）必须在 **DTE 批合入时删除**，否则双重定义。
**B/C 档未开工**：B 档差 `get_gemm_spma_size/spmw_size` 的 arity（**parent 提示：这两个已在 gemm 族实现过，直接读其函数原型即可，不用再逆向**）；C 档 `dot_default` 等 wt-runtime 的 krt 批、`addmm` 缺的 `lsu_rdma_stride_copy` 是 **LOCAL 不可剪断 ⇒ 必须真实现**（登记为阻塞）。

## 低价符号线：2 符号落地，两个入口被**实证的工具缺口**挡住（2026-10-18 14:40）
**落地**（`eec32b6`，patch `07-*`）：`lsu_rdma_stride_copy`（106 B，6 实参，GLOBAL T）+ `lsu_write_gemm_data_to_n3d`（326 B，9 实参 = a0..a7 + **t3**，LOCAL；`n3d_linear_1.so` 是**同名同体**，一份导出服务两个内核）。真值 59 用例、差分 **3,852 + 12,076 B 逐行 0 差异**、变异 **12/12 RED**、**ctest 192/192 Skipped 0**。生成器补 `ENTRY_ARGS`（真实多参/第 9 个在 t3）与 `RET_VALUE`（`return X[10]`），canonical 新 md5 `f424bdf1…`，全部 `--check` 绿。
**两个入口没还原的原因是实证缺口，不是预算**：
- **`addmm_tile_m_split_kn` / `dot_default`**：生成器对它们**响亮退出** —— `不支持: flw / fsw / fadd.s …`，本体含**标量浮点** ⇒ 需要给 `entry_to_cpp.py` 加**标量 FP 寄存器组 + flw/fsw/fadd.s/fmul.s 直译档**（工具工程，不是小件）；
- `dot_default` 另有 krt 缺口（本库无 `krt_barrier_simd`/`krt_flush_dcache_range`）；
- `n3d_linear2_split_k`：**17,484 B + 36 条本地 `t` 边** ⇒ AGENTS §2 的"独立会话"档，且要 10 实参的 **n3d 变体**（与导出名那版不同体）。
**浮出的下一个 +1**：`addmm` 的最后符号缺口 = **`dte_wdma_dst_stride_copy`（202 B，零 TX8 私有指令：17 mv/16 li/10 sd/7 mul/6 jal）** ⇒ 可用现成生成器直接做。
**新情报**：`krt_barrier_simd`（+1）现在解锁 `n3d_to_g3d/main_kernel_n3d_to_g3d_v2` —— **wt-runtime 的 `patches/02-batch-D` 合入即得**。
**两条可复用教训**：① **一份探针只能链一个内核 `.so`** —— 两个 `.so` 都定义 `lsu_*` 族，同时链会互相干扰 PLT/GOT 解析（实测混跑在 stride 段第 23 例崩到 `matmul.so+0x1184a`，而**每个目标单独跑都 rc=0**）；② **观测面里必须留返回值**（`lsu_write_gemm_data_to_n3d` 两段都关时把 `get_gemm_compute_align_size` 的返回值留在 a0，变异 B7 只在这个用例上才红）。
**MERGE BLOCKER 四段弱存根原样保留**（`nm -D` 仍是 `W`）。

# ★★★ 6 个 NE 引擎叶子全部 bit-exact + **一条会影响所有 e16/bf16/fp16 数值工作的厂商伪影**（2026-10-18 15:20）
**完成**：`muls_compute_{fp32,fp16,bf16}` + `mac_compute_with_broadcast_2d_{fp32,fp16,bf16}`（`tx82_device_ne_compute.cpp` + `_mem.cpp` 地址翻译接缝）；真值 = **厂商 CModel 执行厂商指令字节**（20 个函数体原样搬运、只改 3 处 `j <PLT槽>`；SPM 出入走厂商 DTE）；差分 **102 用例 / 103,480 字节逐字节 0 差异**（厂商每例两跑一致）。
**三条发现**：
1. **尾调用必须保留 `rd`**：`mac_compute_with_broadcast_2d_*` 第 1 分支是 `j <PLT槽>`（= `jal x0`）；改成 `jal ra,` 后被调者 `ret` 会落回分支 2 入口 ⇒ **死循环 rc=124**。**判据是 `jal` 的 rd 字段** —— 对任何"把 PLT 槽改成符号调用"的 oracle 搬运都成立。
2. **`muls_compute_*` 的 `stride` 是"块间字节步长"**，不是元素步长：块内 vl 个元素连续，`off += vl*(uint32)stride`。VLMAX 实测 e32/m4=512、e16/m4=1024（用 stride 网格的块间空洞独立复核）。
3. ⚠️ **厂商 CModel 的 e16 向量 FP 数据通路不是逐 lane 正确舍入**：一个 vl 内**第 1/3/5… 个 32-lane 块的结果是截断**（偶数块才就近偶数），**e32 无此现象**。实测：bf16 `0x0001*0.5586` 块0=0x0001 块1=0x0000；bf16 `0x4049*0.6992` 块0=0x340d 块1=0x340c；fp16 `0x03fe*(1/3)` 块0=0x0155 块1=0x0154；同一 `0x3f800001*(1/3)` 在 fp32 上 64 lane 全 0x3eaaaaac。而且**不是一致舍入模式**（tie 向上、非 tie 向下）⇒ **模型实现伪影，不是 ISA 语义**。实现按 **ISA 就近偶数**；真值网格把 e16 大块用例换成精确算术（alpha=1,y=0 ⇒ dst=x；alpha=0 ⇒ dst=y），舍入覆盖留在 ≤32 lane 用例；边界在 `.inc` 头、`logs/ne/README.md`、`device-ne-leaves.md` 三处登记。
**⇒ 凡是要拿厂商 CModel 当 e16/bf16/fp16 向量数值真值的线（GEMM/NE 引擎、算子核）都会撞上这条** —— 必须转告所有后续数值线。
