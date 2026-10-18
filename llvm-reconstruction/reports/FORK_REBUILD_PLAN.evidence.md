# FORK_REBUILD_PLAN 原始实验记录（E-a / E-b / E-c）

配套文档：`docs/compiler/FORK_REBUILD_PLAN.md`。本文件是全量原始输出，未做加工。
所有 scratch 脚本位于 `wt-forkplan/`。厂商二进制只读使用：`llvm-toolchain/bin/{clang-21,llvm-mc,llvm-tblgen,clang-tblgen,llvm-mca}`。

## E-a 补丁可应用性

脚本：`wt-forkplan/run_ea.sh`（先建“基线覆盖层”：从基线取 116 条 `--- a/` 路径 → `git init` + commit → 按序 `git apply` 25 条 patch）。
基线 A = `setup/llvm-project-21.1.0.src`（21.1.0 正式版 tarball，非 git）；基线 B = `third_party/llvm` 的 `HEAD`（= `llvmorg-21.1.0-rc1`，用 `git show HEAD:<path>` 取）。

### 基线 B（rc1）— `wt-forkplan/ea_git.log`
```
OK   0001-raisa-fork-delta.patch
OK   0002-raisa-fork-new-files.patch
OK   0003-raisa-attrs.patch
OK   0004-raisa-riscv-target.patch
OK   0005-raisa-builtins.patch
OK   0006-raisa-vector-builtins.patch
OK   0007-raisa-lib-impl.patch
OK   0008-raisa-stmt-nodes.patch
OK   0010-raisa-xtsm82s-backend.patch
OK   0011-raisa-diagnostics.patch
OK   0012-build-glue.patch
OK   0013-raisa-missed-header-deltas.patch
OK   0014-raisa-addrspace-maps.patch
OK   0015-raisa-missed-source-deltas.patch
OK   0016-raisa-missed-driver-deltas.patch
OK   0017-raisa-stmt-node-definitions.patch
OK   0018-raisa-raisa-toolchain.patch
OK   0019-raisa-sema-ct.patch
OK   0020-raisa-launch-parser.patch
OK   0022-raisa-driver-wiring.patch
OK   0023-raisa-offload-job-wiring.patch
OK   0024-raisa-rpu-rdc.patch
OK   0025-raisa-detector-and-link.patch
OK   0026-raisa-cc1-language.patch
OK   0027-raisa-resource-headers.patch
== git: ok=25 fail=0 ==
```

### 基线 A（21.1.0 正式版）— `wt-forkplan/ea_tarball.log`
```
OK   0001-raisa-fork-delta.patch
OK   0002-raisa-fork-new-files.patch
OK   0003-raisa-attrs.patch
OK   0004-raisa-riscv-target.patch
OK   0005-raisa-builtins.patch
OK   0006-raisa-vector-builtins.patch
OK   0007-raisa-lib-impl.patch
OK   0008-raisa-stmt-nodes.patch
OK   0010-raisa-xtsm82s-backend.patch
OK   0011-raisa-diagnostics.patch
OK   0012-build-glue.patch
FAIL 0013-raisa-missed-header-deltas.patch :: error: 打补丁失败：llvm/include/llvm/Analysis/IVDescriptors.h:47|error: llvm/include/llvm/Analysis/IVDescriptors.h：补丁未应用|error: 打补丁失败：llvm/include/llvm/MC/MCDisassembler/MCDisassembler.h:136|error: llvm/include/llvm/MC/MCDisassembler/MCDisassembler.h：补丁未应用|
     detail: error: 打补丁失败：llvm/include/llvm/Analysis/IVDescriptors.h:47|error: llvm/include/llvm/Analysis/IVDescriptors.h：补丁未应用|error: 打补丁失败：llvm/include/llvm/MC/MCDisassembler/MCDisassembler.h:136|error: llvm/include/llvm/MC/MCDisassembler/MCDisassembler.h：补丁未应用|
OK   0014-raisa-addrspace-maps.patch
OK   0015-raisa-missed-source-deltas.patch
OK   0016-raisa-missed-driver-deltas.patch
OK   0017-raisa-stmt-node-definitions.patch
OK   0018-raisa-raisa-toolchain.patch
OK   0019-raisa-sema-ct.patch
OK   0020-raisa-launch-parser.patch
OK   0022-raisa-driver-wiring.patch
OK   0023-raisa-offload-job-wiring.patch
OK   0024-raisa-rpu-rdc.patch
OK   0025-raisa-detector-and-link.patch
OK   0026-raisa-cc1-language.patch
OK   0027-raisa-resource-headers.patch
== tarball: ok=24 fail=1 ==
```

### 基线 blob 判定 — `wt-forkplan/hashcheck.py` 输出
```
{'new-file': 32, 'old==rc1': 83, 'old==21.1.0rel': 73, 'new==worktree': 81, 'new!=worktree': 5, 'old!=21.1.0rel': 13, 'old!=rc1': 3}
files whose pre-image does NOT match rc1 base: 3
   ('0023-raisa-offload-job-wiring.patch', 'clang/lib/Driver/ToolChains/Clang.cpp', '0b43260cac', '456bfe885f35414b4ff0f16e476397e7fd70fa24', '62613322320cdaa199df1b939dc30f0af15a1be5')
   ('0024-raisa-rpu-rdc.patch', 'clang/lib/Driver/ToolChains/Clang.cpp', '0b43260cac', '456bfe885f35414b4ff0f16e476397e7fd70fa24', '62613322320cdaa199df1b939dc30f0af15a1be5')
   ('0025-raisa-detector-and-link.patch', 'clang/lib/Driver/ToolChains/Raisa.cpp', '4d00b6ac8d62', None, '')
```
说明：3 条例外条目为 `0023`/`0024` 的 `clang/lib/Driver/ToolChains/Clang.cpp`（pre-image 是 `0022` 的 post-image，故不等于 pristine）与 `0025` 的 `clang/lib/Driver/ToolChains/Raisa.cpp`（由 `0018` 新建，pristine 中不存在）。

### 验收 gate（工作树 == “rc1 + 25 patch”）
```
ACCEPTANCE GATE (overlay[25/25 applied] vs third_party/llvm on the 146 patched paths):
  identical=146  differing=0  missing=0
```

## E-b `tsm-x82` / `xtsm82s` 的确切位置

```
### patch 序列内（patches-series/0004-raisa-riscv-target.patch）
10:+// tsm-x82 CPU. Recovered from the vendor toolchain's generated
20:+def FeatureVendorXTsm82s
24:+def HasVendorXTsm82s
40:+// tsm-x82: the TSingMicro TX8 NPU host CPU. The feature list below is written
50:+def TSM_X82 : RISCVProcessorModel<"tsm-x82",

### 重建树 third_party/llvm（工作树 == rc1 + 25 patch）
580:def TSM_X82 : RISCVProcessorModel<"tsm-x82",
595:                                   FeatureVendorXTsm82s]>;
1656:def FeatureVendorXTsm82s
1660:def HasVendorXTsm82s
96:    {OffloadArch::TSM_X82, "tsm-x82", ""},
2311:include "RISCVInstrInfoXTsm82s.td"
class XTsm82sInst<dag outs, dag ins, string mnem, string asmops,
                   list<Predicate> preds>
    : RVInst<outs, ins, mnem, asmops, [], InstFormatOther> {
  // Operand variables referenced by the Inst{...} assignments below.
  // The code emitter and the disassembler map these names back to the
  // operands named in the DAG above.
  bits<5> rd;
  bits<5> rs1;
  bits<5> rs2;
  bits<5> vd;
  bits<5> vs1;
  bits<5> vs2;
  bits<5> imm;
  bits<11> vtypei;
  bit vm;
  let Predicates = preds;
  // XTsm82s gets its own decoder table so that opcode 0x7b does not
  // have to disambiguate against every standard instruction; the table
  // is registered last in RISCVDisassembler.cpp's DecoderList32.
  let DecoderNamespace = "XTsm";

### 厂商 ground truth: llvm-toolchain/include/llvm/TargetParser/RISCVTargetParserDef.inc
105:    {"xtsm82s", {1, 0}},
490:PROC(TSM_X82, {"tsm-x82"}, {"rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_zfbfmin1p0_zfh1p0_zbb1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_xtsm82s1p0"}, 0, 0, 0x00000000, 0x0000000000000000, 0x0000000000000000)

### 指令接线：RISCVInstrInfoXTsm82s.td 规模与 HasVendorXTsm82s 门控
4361 /home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm/llvm/lib/Target/RISCV/RISCVInstrInfoXTsm82s.td
409
4326:def : InstAlias<"vfneg.v $vd, $vs$vm",
4329:def : InstAlias<"vfneg.v $vd, $vs",
4333:def : InstAlias<"vfabs.v $vd, $vs$vm",
4359:def : InstAlias<"vneg.v $vd, $vs",

### 缺口：21 个 TX8 pass 在树/patch 中 0 引用
TX8SchedStrategy -> 0 files
TX8TransformSIMTToSIMD -> 0 files
TX8AddressSanitizer -> 0 files
TX8LegalizeEXEC -> 0 files
TX8RegisterRename -> 0 files
TX8InsertXFRM -> 0 files
TX8MCInfo (唯一存在的 TX8 后端源文件)：
/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm/llvm/lib/Target/RISCV/MCTargetDesc/TX8MCInfo.h
/home/shaokai/Desktop/work/riscv-llvm-musl-toolchain/third_party/llvm/llvm/lib/Target/RISCV/MCTargetDesc/TX8MCInfo.cpp
```

## E-c 生成表与厂商 shipped 文件的一致性 + TableGen 全后端验证

用厂商自带的 `llvm-tblgen` / `clang-tblgen`（版本 21.1.0-rc1）跑**重建树的 .td**；
`RISCVTargetDefEmitter.cpp` 未被 fork 改动（不在 146 条改动路径内），故其输出即我们构建后应得的输出。

```
### 1) -gen-riscv-target-def（输入: llvm/lib/Target/RISCV/RISCV.td）
3e39e57196f69ee5e23f67be0fd3bded  wt-forkplan/ec/RISCVTargetParserDef.inc
3e39e57196f69ee5e23f67be0fd3bded  /home/shaokai/Desktop/work/tx82/llvm-toolchain/include/llvm/TargetParser/RISCVTargetParserDef.inc
105:    {"xtsm82s", {1, 0}},
490:PROC(TSM_X82, {"tsm-x82"}, {"rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_zfbfmin1p0_zfh1p0_zbb1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_xtsm82s1p0"}, 0, 0, 0x00000000, 0x0000000000000000, 0x0000000000000000)

### 2) -gen-opt-parser-defs（输入: clang/include/clang/Driver/Options.td）
d8aba2c7557432feb5132860196e8791  wt-forkplan/ec/Options.inc
d8aba2c7557432feb5132860196e8791  /home/shaokai/Desktop/work/tx82/llvm-toolchain/include/clang/Driver/Options.inc

### 3) -gen-clang-builtins（输入: clang/include/clang/Basic/BuiltinsRISCV.td，见 clang/include/clang/Basic/CMakeLists.txt:108-110）
cdbd5c3f03951447a69796a0afeb1c95  wt-forkplan/ec/BuiltinsRISCV2.inc
cdbd5c3f03951447a69796a0afeb1c95  /home/shaokai/Desktop/work/tx82/llvm-toolchain/include/clang/Basic/BuiltinsRISCV.inc
（反面样本：误用 Builtins.td 会得到 54966cc29542e15f1b3d432370a7bd7f，与厂商不同，属输入错误而非重建缺陷）

### 4) 全部 TableGen 后端 rc 与产物大小
  -gen-subtarget -> 19215091 B
  -gen-instr-info -> 5526184 B
  -gen-asm-matcher -> 932998 B
  -gen-disassembler -> 835834 B
  -gen-asm-writer -> 1162530 B
  -gen-register-info -> 1143265 B
  -gen-dag-isel -> 98433377 B
569:GET_SUBTARGETINFO_MACRO(HasVendorXTsm82s, false, hasVendorXTsm82s)
136252:  if (Bits[RISCV::FeatureVendorXTsm82s]) HasVendorXTsm82s = true;
XTsm_ 命中数: matcher=356 disassembler=399 instr-info=1596
```

## ISA 层 oracle（重建的 397 条助记符 vs 厂商工具）

### 反汇编方向：397/397 一致
`wt-forkplan/isa_dis_oracle.py`：把 `recovered-docs/xtsm82s_isa.tsv` 的 397 条示例编码按小端拼成
`llvm-mc -disassemble` 的十六进制输入，逐行取回助记符比对。

```
DIS-ORACLE: recovered encodings -> vendor llvm-mc -mcpu=tsm-x82 mnemonic: match=397/397 mismatch=0
```

### 汇编方向：不能拿 tsv 的 example_operands 当输入（这是本轮的一条重要负面结论）
`wt-forkplan/isa_oracle2.py`：逐条 `mnemonic + example_operands` 喂厂商 `llvm-mc -show-encoding`。

```
ISA ORACLE (recovered xtsm82s_isa.tsv  vs  vendor llvm-mc -mcpu=tsm-x82), 397 mnemonics
  {'match': 191, 'error': 181, 'mismatch': 7, 'crash': 18}
 top error messages:
     49  <stdin>:1:9: error: the destination vector register group cannot overlap the mask register
     33  <stdin>:1:10: error: the destination vector register group cannot overlap the mask register
     18  LLVM ERROR: Segment load/store is not supported by TX8
     11  <stdin>:1:12: error: the destination vector register group cannot overlap the mask register
     10  <stdin>:1:14: error: the destination vector register group cannot overlap the mask register
      9  <stdin>:1:11: error: the destination vector register group cannot overlap the mask register
 mismatches: 7
    ('vmv.v.v', '0xde00107b', '0xf600507b', 'v0, v0')
    ('vmv.x.s', '0x4200207b', '0xdc00507b', 'zero, v0')
    ('vs1r.v', '0x0280407b', '0x1280407b', 'v0, (zero)')
    ('vs2r.v', '0x4280407b', '0x5280407b', 'v0, (zero)')
```
（`error` 绝大多数是厂商自己的合法性诊断 “the destination vector register group cannot overlap the
mask register”；`crash` 是厂商 `llvm-mc` 直接 `LLVM ERROR: Segment load/store is not supported by TX8`
中止。⇒ 汇编方向的回归必须改用**厂商 `llc` 产出的合法汇编语料**。）

## 厂商二进制的实测特性（对照数据）

```
### --version
clang version 21.1.0-rc1 (ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf26e7a507d3ecb57e76c956dd21b5c822a6)
Target: riscv64-unknown-linux-musl
Thread model: posix
### --print-supported-cpus | grep tsm
### --print-supported-extensions | grep xtsm
    xtsm82s              1.0       'XTsm82S' (TSM X82 extension)
### -mcpu=tsm-x82 的 ABI 与 .attribute arch
	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_f2p2_c2p0_zicsr2p0_zifencei2p0_zmmul1p0_zfbfmin1p0_zfh1p0_zfhmin1p0_zca1p0_zbb1p0_zve32f1p0_zve32x1p0_zve64x1p0_zvfbfmin1p0_zvfh1p0_zvfhmin1p0_zvl32b1p0_zvl64b1p0_xtsm82s1p0"
### llc --help-hidden: TX8/SIMT 相关 pass 选项
 --TX8-VPrintf-HW - printf vector reg's value in HW
 --riscv-insert-vsetvli-enable-simtmul - Enable vsetsimtmul
 --riscv-tx8-sched - Enable TX8 Scheduling Strategy for RISC-V
 --tx8-lmul-sched - Enable TX8 scheduling to minimize lmul switching
 --tx8-vsetbarrier - add vsetbarrier at the start position and end position of the kernel function for profiling
### llvm-mca -mcpu=tsm-x82（证明厂商有真实 SchedModel）
error: wt-forkplan/vendor/k.s: No such file or directory
### 对照：generic-rv64 无 SchedModel（命令在 wt-forkplan/vendor 下执行）
error: unable to find instruction-level scheduling information for target triple 'riscv64' and cpu 'generic-rv64'.
### 厂商 libLLVMRISCVCodeGen.a：成员数 68，其中 TX8* 21 个（全部是 pass；TX8MCInfo 不是 pass）
21
TX8SchedStrategy.cpp.o TX8LegalizeCTInstruction.cpp.o TX8RegisterConflictDetector.cpp.o TX8RegisterRename.cpp.o TX8RegisterAlignment.cpp.o TX8LegalizeEXEC.cpp.o TX8MemoryUtils.cpp.o TX8AnnotateUniformValues.cpp.o TX8AnnotateControlFlow.cpp.o TX8TransformSIMTToSIMD.cpp.o TX8UnifyDivergentExitNodes.cpp.o TX8PromoteAlloca.cpp.o TX8AlwaysInline.cpp.o TX8RemoveInlineDeviceFunctions.cpp.o TX8LowerSharedPass.cpp.o TX8ProfilingKernelFunc.cpp.o TX8OptimizeConfigInstrs.cpp.o TX8InsertXFRM.cpp.o TX8AddressSanitizer.cpp.o TX8BankAwareShared.cpp.o TX8RecordingNPUInstrPC.cpp.o 
### 21 个 TX8*.o 均无 DWARF（无法用调试信息 RE）
21/21：对 backend-objects/ 下每个 .o 执行 `readelf -S <f> | grep -c debug_info` 均为 0
### 重建树中的 driver 开关（Options.td）
28
1193:defm rpu_rdc : BoolFOption<"rpu-rdc",
1321:def : Flag<["-"], "fraisa-rdc">, Alias<frpu_rdc>;
1322:def : Flag<["-"], "fno-raisa-rdc">, Alias<fno_rpu_rdc>;
5884:def raisa_path_EQ : Joined<["--"], "raisa-path=">, Group<i_Group>,
5893:def raisa_noopt_device_debug : Flag<["--"], "raisa-noopt-device-debug">,
### 缺失的 pass 级开关（树中 0 引用）
riscv-tx8-sched -> 0 files
tx8-lmul-sched -> 0 files
### 磁盘余量（构建可行性）
/dev/nvme0n1p2  937G  289G  601G   33% /
```

## 复现方式

```bash
cd /home/shaokai/Desktop/work/tx82/wt-forkplan
./run_ea.sh              # E-a（基线 A + 基线 B）
python3 hashcheck.py     # 基线 blob 判定
python3 isa_dis_oracle.py# ISA 反汇编 oracle
# 生成表（E-c）：见 FORK_REBUILD_PLAN.md 的 T1/T4 命令
```
