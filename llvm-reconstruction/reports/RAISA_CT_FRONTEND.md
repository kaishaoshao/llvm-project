# RAISA / CT 前端缺口（G2/G3/G4）修复报告

- 工作树 `wt-raisa/src`（共享重建树副本）+ `wt-raisa/build`（增量，`-j2`）；工具与证据在
  `wt-raisa/tools/`、`parity_*.tsv`、`ext_recheck_after.tsv`。厂商 oracle 为
  `llvm-toolchain/bin/clang-21`，所有对比跑在 `ulimit -c 0; ulimit -f 256MiB;
  timeout -s KILL 90` 下。
- 补丁 `wt-raisa/patches/0034..0039`（0034=G2b，0035=G2a，0036=G3，0037=G4，0038=G2c，
  0039=工具）。**0034 由 `tools/gen_semact.py` 机器生成，不要手改。**

---

## 0. 结论速览

| 缺口 | 状态 | 依据 |
| --- | --- | --- |
| **G3** `l0ashared/l0wshared/l1shared` 断言 | **已闭环（Sema 层）** | 断言消失；3 属性 + `gmem/generic` 全部落到 AST；`.rc` 语料断言 cell 21→0 |
| **G2** CT 内建分类只有 888/1291 | **已闭环** | 从厂商目标文件解出全部 **1194** 个非 default ID；CT 触发条件改回厂商的 ID 区间；`.rc` 语料 CT 诊断 119→0 |
| **G4** `__raisa_*` 无 CodeGen | **已闭环** | 50 个内建全部实现；`-S -emit-llvm` 归一化后与厂商**逐行相同** |
| 遗留 | G1 后端 ISel（`read.reg.*`/`raisa.*`/XTsm）、fork pass、RAISA CodeGen runtime、共享内存地址空间下沉 | `.rc` cell 在**前端全部通过**后死在 `RISCVISelLowering.cpp:14610` |

## 1. 方法：直接反解厂商的 `SemaCT.cpp.o`

第一版重建把 `CheckFixedVector` 的类型约束猜成"**元素个数** ∈ {8/16/32/64}，
并把 403 个 ID 落到 `default:`。本轮改为**从厂商目标文件机械恢复**：

`/home/shaokai/Desktop/work/tx82/llvm-toolchain/lib/libclangSema.a(SemaCT.cpp.o)`
→ `tools/decode_semact.py` 逐条解释 `SemaCT::CheckCTBuiltinFunctionCall`
（5 张 `.rodata` 跳转表 + 位掩码判定 + `movl $L,0x20(%rsp)` 长度常量），
对 `BuiltinID ∈ [0, 5700)` 全部求出到达的"动作"。

三个把第一版推断推翻的发现：

1. **比较的是元素位宽，不是元素个数。**
   `CheckFixedVector` 的轮廓化 lambda 开头就是
   `ASTContext::getBuiltinVectorTypeInfo(Ty)` → `ASTContext::getTypeInfo(Info.ElementType)`
   → 与列表逐项比较（`SemaCT.cpp.o` 的 `_clE`，偏移 0x5c/0x70/0x74…0x95）。
   实测（厂商 clang，`--target=riscv64-tsingmicro-raisa -fsyntax-only`）：

   | 调用 | 厂商 | 说明 |
   | --- | --- | --- |
   | `__riscv_vadd_vv_i8m1` | **拒绝** `with type '__rvv_int8m1_t'` | 8 bit |
   | `__riscv_vadd_vv_i16m1` / `_i16m4` / `_i16mf4` / `_i32m8` | 通过 | 16/32 bit |
   | `__riscv_vadd_vv_i64m1` | **拒绝** | 64 bit |

   `i16mf4`(4 元素)、`i16m4`、`i32m8`(32 元素) 用同一个 `{16,32}` 列表都被接受
   ⇒ 只有"位宽"能解释；第一版的"元素个数"会让 `i16mf4/i16m4` 被误拒。

2. **CT 只在 `BuiltinID ∈ [BI__builtin_rvv_vadd_vv, +0xE60]` 且
   `TI.hasFeature("xtsm82s")` 时被调用。**
   `SemaRISCV.cpp.o`：`2d6: lea -0x5db(%rbx),%eax / 2dc: cmp $0xe60 / 2e1: ja 3f8`
   → `hasFeature("xtsm82s")` → `d88: call CheckCTBuiltinFunctionCall`。
   `gdb` 实测：`__riscv_read_reg_tid_x()`（ID 5348）**不会**进入该函数，
   而 `__riscv_vle16_v_i16m1`（ID 1592）会。
   第一版的 `if (getLangOpts().RAISA) return CheckCTBuiltinFunctionCall(...)`
   把区间外的 RAISA 专用内建（`__riscv_read_reg_*` = 5324+、`__raisa_*` = 5394+）
   也送去 CT，于是 `kernel_spaces.rc` 之类全部被误拒。

3. **`return false` 不等于报错。** `d90: test %al,%al / d92: je 302 / d98: jmp 55f`：
   返回 false 只是"不在这里处理"，随后走常规 RVV 内建检查；
   只有 `default:` 才会发诊断。另外 `DIAG_START_SEMA = 2821`（不是 2820）：
   `0x1456 = err_tsm_unsupported_ct_intrinsic`（plain，default 路径）、
   `0x1457 = err_tsm_unsupported_ct_intrinsic_with_type`（validator）。

## 2. 修复内容

### G2a（patch 0035）CT 触发条件
```c
if (BuiltinID - (unsigned)RISCVVector::BI__builtin_rvv_vadd_vv <= 0xE60 &&
    TI.hasFeature("xtsm82s")) {
  if (CheckCTBuiltinFunctionCall(TI, BuiltinID, TheCall))
    return true;                 // 厂商 d98
  // false → 继续下面的常规 RVV 检查（厂商 d92）
}
```

### G2b（patch 0034）分类表 + validator
- `CheckFixedVector` 改成对 `getTypeInfo(Info.ElementType).Width` 判 `is_contained`；
  守卫用 `BuiltinType::isRVVVLSBuiltinType()`（厂商传给它的 QualType 全是 RVV 向量/掩码）。
  返回语义与厂商一致：命中 → `return false`；未命中 → `Diag(with_type) << Ty`。
- `switch (BuiltinID)` 由 `tools/gen_semact.py` 从 `out_semact.tsv` 重新生成：
  **1194 个 case**，12 组：

  | 组 | 数量 | 组 | 数量 |
  | --- | --- | --- | --- |
  | `VEC(call,{16,32})` | 844 | `VEC(arg0,{16,32})` | 42 |
  | `VEC(call,{32})` | 78 | `VEC(arg1,{16,32})` | 33 |
  | `VEC(call,{16})` | 36 | `VEC(arg2/3,{16,32})` | 7/6 |
  | `VEC(call,{64})` / `{8}` | 12/10 | `BuiltinConstantArgRange(1/2,…)` | 6/6 |
  | `return false` | 114 | `default:`（发诊断） | 区间内其余 |

### G3（patch 0036）共享内存属性
`RAISAL0AShared/RAISAL0WShared/RAISAL1Shared`（外加 `RAISAGMem/RAISAGeneric`）
在 `ProcessDeclAttribute` 里补 `case` + `handleRAISASharedAttr/handleRAISAMemorySpaceAttr`。
厂商确实没有走生成表（`AttrParsedAttrImpl.inc` 里这 5 个都没有
`handleDeclAttribute`，SimpleHandler 计数 92 == 我们），所以是显式 handler。

### G4（patch 0037）`__raisa_*` CodeGen
50 个内建 → 三类下降，全部由厂商 `-S -emit-llvm` 反查得到：

| 类别 | 数量 | 目标 |
| --- | --- | --- |
| `llvm.riscv.raisa.{cos,sin,log2,frsqrt,reduce.sum,reduce.fmax,reduce.fmin,fdiv,fmax,fmin,div,max,min,umax,umin}` | 35 | 直接 `ID = Intrinsic::riscv_raisa_*` |
| `llvm.exp2.*` | 3 | `__raisa_exp2_{f16,f,bf16}` |
| `llvm.experimental.constrained.fadd.*` | 12 | `__raisa_fadd_{rn,ru,rz,rd}_*` → `round.tonearest/upward/towardzero/downward` + `fpexcept.ignore` + 调用点 `strictfp` |

### G2c（patch 0038）`-x raisa` 设备趟的 CUDA-runtime 断言
`CodeGenModule::getMangledName` 的 `assert(... getCUDARuntime().getDeviceSideName(ND) ...)`
在 RAISA 设备趟（`__attribute__((global))` → `CUDAGlobalAttr`）会对空 runtime 求值。
厂商是 Release（断言整个表达式不求值），旧重建（+assertions）必崩。加
`getLangOpts().RAISAIsDevice ||` 短路，**只影响 Debug 语义，不改变 Release 产物**。

## 3. parity 表（RAISA 语料，`-O0`，137 cell = 26 canonical + 111 round2 `.rc`）

复现：`python3 tools/raisa_parity.py --ours <clang> --cfgs O0 --out <tsv>`

| 判定 | before | after |
| --- | --- | --- |
| BYTE-IDENTICAL | 1 | 1 |
| DIFF（双成功、节不同） | 2 | 2 |
| PARITY-DIAG（双侧同诊断） | 0 | **27** |
| PARITY-DIAG-DIFF（双侧都失败、根因不同） | **31** | **4** |
| OURS-ONLY-GAP（厂商成功、我们失败） | 103 | 103 |
| 源码签名（我们侧）| 119 `with type`、21 属性断言、13 `intrinsic is not supported by CT`、9 `outside the valid range`、1 `Arg access out of range` | **0 前端诊断**；102 个 cell 落到后端（`unreachable@RISCVISelLowering.cpp` / `Cannot select`） |

关键点：`OURS-ONLY-GAP` 计数没变，但**阻塞层从前端整体搬到了后端**——
after 的 103 个失败 cell 里一条 CT/属性/区间诊断都没有。ext 普查（309 probe）同结论。

前端差分（决定性）：`tools/raisa_parity.py` 的 `probe/ct_matrix.c`
（`vadd_vv`/`vmfeq_vf`/`vmv_s_x` × e8/e16/e32/e64，共 12 个调用）
两侧诊断集合 **16/16 完全相同**；`t7.c/t8.c/t9.c` 亦逐字相同
（`__rvv_int8m1_t` / `__rvv_int64m1_t` 拒绝、`vsext_vf2` 走 plain 诊断）。

### 3.1 ext 普查（309 probe，`tools/ext_recheck.py`）

复用 `wt-parity/raw/ext/*.o.log` 里记录的命令行，只把编译器换成我们的重建版本：

| 指标 | before | after |
| --- | --- | --- |
| 带**前端**签名（CT/属性/区间/134）的 cell | **108**（79 `with type` + 24 属性断言 + 5 `outside the valid range`） | **0** |
| 其中仍在别处失败的 | — | 108（103 `unreachable@RISCVISelLowering.cpp:14610`、5 `Cannot select: %llvm.riscv.gemm`） |
| 因此变成完全通过（rc=0）的 | — | 0（后端仍挡住） |
| OURS-GAP 总数 | 238 | 228 |

这 108 个 cell 的**根因层**从前端整体搬到后端：前端签名清零，后端一条不减。

## 4. 字节一致性与 cmodel

- **IR 级逐行一致（G4 的强证据）**：`probe/raisa_all.c`（50 个 `__raisa_*`）两侧
  `-S -emit-llvm`，把 SSA 名/编号/module-id 归一化后 `diff` = **0 行**。
  这里有一步修正：调用点**不能** `CI->setAttributes(F->getAttributes())`，
  厂商的调用点只有 `strictfp`（`attributes #5 = { strictfp }`）。
- **设备对象字节一致**：CT 语料里**没有**任何 cell 两侧同时成功，因此没有新的字节一致
  证据；`k_version__O0` 仍然 `BYTE-IDENTICAL`（不碰 CT 的输入）。原因见第 5 节。
- **cmodel**：无法做 CT 相关对比。厂商对 `mathdev_raisa_*.rc`、纯 `__raisa_*` 的
  `probe/raisa_only.rc` 都会产出设备 ELF，而我们编译同一文件在 ISel 就 abort
  （没有 ELF / 没有 golden 可比）。`wt-cmodelrun/` 已证明"两侧都编得出的输入"
  （`add` 等）在 cmodel 上逐位相同，但那批输入不走 CT 内建，属于另一工作流的证据。
  ⇒ **cmodel 这一步被 R1 挡住，不是被 G2/G3/G4 挡住。**

## 5. 剩余缺口与规模

| # | 缺口 | 证据 | 规模 |
| --- | --- | --- | --- |
| **R1** | 后端 ISel 缺 `llvm.riscv.read.reg.*` / `llvm.riscv.raisa.*` / XTsm 家族的 legalize | 纯 `__raisa_*` 的 `.rc`（`probe/raisa_only.rc`）：厂商 rc=0 / 1712 B，我们 `Cannot select: intrinsic %llvm.riscv.raisa.reduce.sum`；`alloc_l1shared_*` 等 103 个 cell 停在 `RISCVISelLowering.cpp:14610`。注意 `.rc` 设备流水线里这些 intrinsic 由 fork pass 下降，所以在 musl 配方下**厂商自己**也会 `Cannot select` | = 原 **G1**（10 内建解锁 14 kernel；83 内建全做），M→L |
| **R2** | fork pass 缺席（`simt2simd`、`TX8LegalizeCTInstruction`、`InsertXFRM`、shared-memory 下沉） | 厂商 `attrs.rc` 出 `vsetsimt/vmv1r.v/vluxei64.sync.v` + `s2 ±1024`，我们出纯标量栈访问；`.rc` 里 `set_mem_alloc` 等 | = 原 **G6**，L |
| **R3** | RAISA CodeGen runtime 未接线（`CreateTSRAISARuntime` 有实现、`CodeGenModule` 没有 `getRAISARuntime()`） | `CodeGenModule.h` 无 RAISA runtime 成员；kernel/module 注册（`__raisa_module_ctor`、`txRegisterFunction`）不会生成 | M |
| **R4** | `gmem/generic` 的 Subject 诊断与厂商不同 | 厂商对函数内 `__attribute__((generic))` 报 `'generic' attribute cannot be applied to a declaration`，我们不报 | S |
| **R5** | `-x raisa` 设备链接（`ld.lld -m elf64lriscv -shared`）未打通 | 只能到 `.o` | S–M |

**结论**：G2/G3/G4 已在前端闭环并有逐字/逐行证据；RAISA/CT 设备对象的端到端
byte-parity 现在**只**被 R1（后端 ISel）+ R2（fork pass）挡住，与前端无关。

## 6. 复现

```bash
cd /home/shaokai/Desktop/work/tx82/wt-raisa
# 1) 反解厂商分派表（一次性，~1 min）
ar x ../llvm-toolchain/lib/libclangSema.a SemaCT.cpp.o     # -> vendorobj/
python3 tools/decode_semact.py vendorobj/SemaCT.cpp.o 5700 > out_semact.tsv
./host/dump_builtins                                       # -> out_builtin_ids.tsv
# 2) 把 5 处改动打到任意重建树上（幂等；0034 的 switch 由 gen_semact.py 生成）
python3 tools/apply_raisa_fixes.py <llvm-root>
# 3) 增量重建（表驱动，20 s – 12 min），再 relink（TX8RegisterRename.cpp 需手工补进归档）
echo '[]' > logs/changed_inc.json
python3 tools/minibuild.py --steps objs,libs --jobs 2 --force-src \
  clang/lib/Sema/SemaCT.cpp,clang/lib/Sema/SemaRISCV.cpp,clang/lib/Sema/SemaDeclAttr.cpp,\
  clang/lib/CodeGen/TargetBuiltins/RISCV.cpp,clang/lib/CodeGen/CodeGenModule.cpp
bash tools/relink.sh
# 4) 对拍
python3 tools/raisa_parity.py --ours build/bin/clang-21 --cfgs O0
python3 tools/ext_recheck.py  build/bin/clang-21
```

**环境注意**：`wt-raisa/` 同时被另一个 worker 使用，它会周期性把共享树重新同步进
`wt-raisa/src` 并**覆盖手工编辑**；所以改动都固化成 `patches/0034..0038` +
`tools/apply_raisa_fixes.py` 一键重放，已构建好的二进制另存为
`wt-raisa/build/bin/clang-21.raisa-ct`。另外 `build.ninja` 是 `wt-forkbuild/build`
的副本，`ninja` 会认为 ~3100 条边全脏，必须用 `tools/minibuild.py`。
