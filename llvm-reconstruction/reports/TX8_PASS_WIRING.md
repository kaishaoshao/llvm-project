# TX8 后端 pass：接线点与被依赖的公共助手（实证，2026-10-16，parent 亲测）

证据命令（可复现；`$A=/home/shaokai/Desktop/work/tx82/llvm-toolchain/lib/libLLVMRISCVCodeGen.a`）：

```bash
nm -A --undefined-only "$A" | grep -E 'createTX8|initializeTX8|RPU' | awk -F: '{print $2}' | sort | uniq -c | sort -rn
nm -A --undefined-only "$A" | grep -oE '_ZN4llvm3RPU[0-9]+[A-Za-z0-9_]+' | sort -u | c++filt
```

## 1. 管线接线点 = `RISCVTargetMachine.cpp`

| 引用方成员 | 引用 TX8 符号数 |
|---|---|
| **`RISCVTargetMachine.cpp.o`** | **33** |
| `TX8LowerSharedPass.cpp.o` | 8 |
| `TX8BankAwareShared.cpp.o` | 8 |
| `TX8AddressSanitizer.cpp.o` | 2 |

⇒ 21 个 pass 的**注册与插入位置全在 `RISCVTargetMachine.cpp`**（`addPassesToEmitFile` /
`RISCVPassConfig` 那一带）。复现 pass 时，改这一个文件就能把它们接回管线；
具体顺序用厂商 clang 的 `-mllvm -debug-pass=Structure` 实测（见 `TX8_PASS_INVENTORY.md`）。

## 2. pass 依赖的公共助手 API（`llvm::RPU::*`）

被未定义引用到的 9 个助手（定义散在 `libLLVMRISCVUtils.a`(2) 与 `libLLVMRISCVCodeGen.a`(16)，
其中一部分**就在缺失的 `TX8*` 对象里**）：

```
llvm::RPU::isKernelFunction(llvm::Function const*)
llvm::RPU::isDynamicL0AShared(llvm::GlobalVariable const&)
llvm::RPU::isL1SharedVariableToLower(llvm::GlobalVariable const&)
llvm::RPU::isL0ASharedVariableToLower(llvm::GlobalVariable const&)
llvm::RPU::isL0WSharedVariableToLower(llvm::GlobalVariable const&)
llvm::RPU::getL0Align(llvm::DataLayout const&, llvm::GlobalVariable const*)
llvm::RPU::getTransitiveUsesOfShared(llvm::CallGraph const&, llvm::Module&, llvm::RPUAS::AddressSpace)
llvm::RPU::removeLocalVarsFromUsedLists(llvm::Module&, llvm::ArrayRef<llvm::GlobalVariable*>)
llvm::RPU::eliminateConstantExprUsesOfShared(llvm::Module&)
```

⇒ 复现时**先补这批助手 + `RPUAS::AddressSpace` 枚举**（它们是被多个 pass 共用的判定/改写工具），
否则每个 pass 都会重复实现一遍同样的"什么是 kernel 函数 / 哪块 shared 要下沉"。

## 3. 其它已挖到的事实

- 21 个对象里 **`TX8MemoryUtils` 与 `TX8RegisterConflictDetector` 没有任何 pass 类符号** ⇒ 很可能是
  工具函数集合，不是 pass（先当成"不需要复现的 pass"）。
- `vsetbarrier` 不是独立对象：它注册在 **`TX8ProfilingKernelFunc`** 里（`tx8-vsetbarrier`）。
- 唯一直接绑调度模型的类：**`TX8PreRASchedStrategy` / `TX8PostRASchedStrategy`**（`TX8SchedStrategy.cpp.o`），
  与"TX8 SchedModel 缺失"是同一件事。
- 注册名 / `-mllvm` 选项 / 类符号的完整清单见 `tx8_pass_symbols.tsv`（同目录，由 `strings`+`nm -C` 生成）。
