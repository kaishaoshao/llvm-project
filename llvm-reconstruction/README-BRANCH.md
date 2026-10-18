# LLVM 重建分支（非 cmodel 主线）

这个分支只放 **TX82 编译器（LLVM 21.1.0-rc1 私有分叉）的重建产物**，与 `master` 的 cmodel 主线分开。

## 为什么只有补丁，没有 LLVM 源码
LLVM 基线是厂商自带的分叉（GB 级，不是我们的产物）。这里版本化的是**能把它复现出来的东西**：

## 目录
- `patches/` —— `0001`–`0084` 顺序应用的补丁系列（每条都经 `patch -p1` 回放验证）。
  基线：厂商 `llvm-toolchain` 对应的 LLVM 21.1.0-rc1 源树。
- `README.md` —— 重建全过程的权威记录（每个补丁的作用、每一处厂商证据、六类工具陷阱）。
- `reports/` —— 各条线的详细报告（`COMPILER_STATUS.md`、`CMODEL_MARGINAL_*`、`DEVICE_IMPL_VS_CMODEL_COVERAGE.md`、`RVV_TO_0x7B_REMAP.md` 等）。
- `tools/` —— 重建过程中写的脚本（真值生成、差分、分组、callee 图、MIR 对比、预算闸门等）。

## 怎么用
```bash
git clone -b llvm/reconstruct-2026-10-18 <repo> tx82
cd <LLVM 源树根>
for p in ../tx82/llvm-reconstruction/patches/*.patch; do patch -p1 < "$p" || break; done
```
按编号顺序应用；`patches/` 里的编号就是应用顺序（`0047`–`0084` 是本轮新增的重建补丁，`0001`–`0046` 是更早的基线补丁）。

## 状态（2026-10-18）
- 功能：**103/103 个厂商可编译 kernel 全部能编译**；
- 逐字节：`.rc` **98/103** 逐字节（+3 格为厂商自身的 `VENDOR-NONDET`）、317 parity **229 raw / 238 build-identity 归一化**；
- 已知边界与剩余项见 `reports/COMPILER_STATUS.md` 与 `README.md` 末尾。
