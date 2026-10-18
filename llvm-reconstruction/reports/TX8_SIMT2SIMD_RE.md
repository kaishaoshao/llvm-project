# `simt2simd` 反编译索引（REA + Ghidra，2026-10-16）

## 工具链（无需 root，全在工作区）
| 组件 | 位置 | 状态 |
|---|---|---|
| REA `rea-agents 6.4.0` | `/home/shaokai/Desktop/work/rea`（用户提供的工具） | Ghidra provider **healthy** |
| Ghidra 12.1.4 | `tx82/tools/re/ghidra` | `analyzeHeadless` + native decompiler OK |
| JDK 21 (Temurin) | `tx82/tools/re/jdk` | doctor 通过 |
| Node 22.23.3 | `tx82/tools/node` | REA 要求 ≥22.19 |
| capstone 5.0.7 / pyelftools 0.33 | `tx82/tools/pylibs` | 脚本化反汇编 |

**必须导出的环境变量**：`JAVA_HOME=tx82/tools/re/jdk`、`GHIDRA_INSTALL_DIR=tx82/tools/re/ghidra`、`PATH=tx82/tools/node/bin:tx82/tools/re/jdk/bin:$PATH`

**用法（按函数取，省 token）**：
```bash
cd /home/shaokai/Desktop/work/rea
node scripts/rea.mjs analyze  <obj.o> --provider ghidra --format md
node scripts/rea.mjs decompile <obj.o> --procedure <函数名> --provider ghidra --format md --token-limit 4000
node scripts/rea.mjs decompile <obj.o> --procedure <函数名> --provider ghidra --token-count   # 先量 token 再取
```
（`initializeTX8InsertXFRMPassOnce` 单函数 ≈1260 token，可作成本基准。）

## 已产出的反编译件（**不要再重跑**）
| 目标 | 产物 | 规模 |
|---|---|---|
| **`TX8TransformSIMTToSIMD.cpp.o`**（R1' 的关键） | `tools/re/out/simt2simd/decompiled.c` + `callgraph.txt` | **24385 行 / 438 函数** |
| `TX8LowerSharedPass.cpp.o`（R1'-b） | `tools/re/out/lowershared/decompiled.c` + `callgraph.txt` | 6103 行 |
| `TX8RegisterRename.cpp.o`（已复现 ⇒ 交叉验证） | `tools/re/out/vregrename/decompiled.c` | 进行中/已完成 |

## `simt2simd` 的结构骨架（从函数名直接读出，与 290 组合 mapping 表互为印证）
**`CreateV*` 家族**（构造 IR）：CreateVFMoveVF @ 00100200 CreateVMoveVX @ 001002c0 CreateVAdd @ 00100380 CreateVMerge @ 00100460 CreateVIntIsNotZero @ 00100760 CreateVector @ 00101780 CreateVUnaryOp @ 001020e0 CreateVSLH @ 00102b20 CreateVIntZero @ 001034c0 CreateVInt32FromI64ScalarOrVector @ 00104290 CreateVectorFromScalar @ 00104460 CreateVInt32FromBoolScalarOrVector @ 00105540 CreateVectorPrintCallHW @ 00106580 CreateVBinaryOp @ 00107e90 CreateVReduceOp @ 00109d90 CreateVBinaryOp @ 0010a970 
**`Fold*` 家族**（常量/类型折叠）：FoldCast @ 00100560 FoldExactBinOp @ 001005c0 FoldBinOp @ 00100650 FoldNoWrapBinOp @ 001006c0 FoldCmp @ 00100a50 FoldExtractValue @ 00100ac0 FoldBinOpFMF @ 00101430 any_of<llvm::ArrayRef<llvm::Value*>&,llvm::ConstantFolder::FoldGEP(llvm::Type*,llvm::Value*,llvm::ArrayRef<llvm::Value*>,llvm::GEPNoWrapFlags)const::{lambda(llvm::Value*)#1}> @ 00106090 FoldGEP @ 00106180 ConstantFoldCastInstruction @ 00129060 ConstantFoldBinaryInstruction @ 00129078 ConstantFoldCompareInstruction @ 00129098 ConstantFoldExtractValueInstruction @ 001290a8 ~IRBuilderFolder @ 001297a8 
**入口**：`createTX8TransformSIMTToSIMDLegacy` → `runOnModule`（调用图见 `callgraph.txt`）

## 规矩（不变）
**伪代码只用来提假设、看结构；验收只认 IR/对象与厂商逐字节一致**（`-mllvm -print-before/after=simt2simd` 的 dump 是语义真值，`recon/simt2simd/mapping.tsv` 的 290 组合是收敛判据）。
本会话已有两次"信推断不信厂商产物"翻车的教训，不要重演。
