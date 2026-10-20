# TX82 / RAISA — LLVM 21.1.0-rc1 私有分叉的编译器重建

> 本分支（`tx82/raisa-artifacts-2026-10`）独立历史，只放**重建产物**，不含 LLVM 源码本体（厂商分叉的 1.3 GB / 72,930 文件基线不是我们的产物）。

## 这是什么

厂商给 TX82 加速器发的是一个 **LLVM 21.1.0-rc1 的私有分叉**（`ts-llvm-project`，commit `b85caf26`）。本项目把它里面 **RAISA/TX82 专属**的部分逐块重建出来，使我们的编译器对同一份 kernel 源码产出与厂商**一致**的目标文件。

## 目录

| 路径 | 内容 |
|---|---|
| `patches/` | **52 个补丁**（编号即应用顺序，`0001`–`0084`），每条都经 `patch -p1` 回放验证 |
| `reports/` | **27 份报告**：`COMPILER_STATUS.md`、`CMODEL_MARGINAL_2026-10-18.md`、`DEVICE_IMPL_VS_CMODEL_COVERAGE.md`、`RVV_TO_0x7B_REMAP.md` … |
| `tools/` | **23 个重建脚本**：真值生成、逐行差分、入口体分组、callee 图（含条件间接分派修正）、MIR 回放对比、预算/负载闸门 |
| `README-BRANCH.md` | 原始分支说明与用法 |

## 用法

```bash
git clone -b tx82/raisa-artifacts-2026-10 https://github.com/kaishaoshao/llvm-project.git llvm-artifacts
cd <LLVM 21.1.0-rc1 源树根>        # 厂商分叉对应的基线
for p in /path/to/llvm-artifacts/patches/*.patch; do
  patch -p1 < "$p" || { echo "FAILED: $p"; break; }
done
```

## 状态（2026-10-18）

| 维度 | 结果 |
|---|---|
| **能编译**（厂商能编的 103 个 kernel） | **103 / 103 = 100%** |
| **`.rc` 逐字节** | **98 / 103**（另 3 格是厂商自身的 `VENDOR-NONDET`，布局随进程堆翻转） |
| **317 格 parity** | **229 逐字节**（gap 0）；构建串归一化后 **238 / 317** |
| **`.c` 侧** | 零缺口 |
| 回归 | 0 |

## 重建了哪些厂商部件（按层）

`simt2simd`（全部算子族 + lane 地址 + 多 BB）· clang 前端共享内存地址空间 · `tx8-annotate-control-flow` · `structurizecfg` 接入 · `TX8LegalizeEXEC` · `vreg-align` + `*_ALIGNED_*` · `vreg-rename` · `bank-aware-shared` · 伪指令层 · **540 项 bit-8 真表** · bf16 孪生/归约 · `config-instr-opt` · `rpu-lower-shared` · 调度模型 override（`misched-dcpl` 30→6）· LSR stopgap。

## 已知边界

- 2 个 `control_*_loop` 格需上游 ISel/RA 工作（`$exec` kill 标记、`MOV128` 伪层、标量染色），成本高、未做；
- 厂商 `LoopStrengthReduce.cpp` 被私自分叉过，我们用"对 RAISA 跳过 LSR"的 stopgap 复现其地址形状（逃生口 `-mllvm -riscv-raisa-lsr`）；
- 完整记录见 `reports/COMPILER_STATUS.md`；工具陷阱清单见 `README-BRANCH.md`。

## 相关仓库
- cmodel 侧（设备层实现、真值台账、合并库）：`kaishaoshao/cmodel`（master）
