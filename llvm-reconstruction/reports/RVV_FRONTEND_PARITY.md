# RVV 前端一致性（riscv_vector.h 与 vsetivli）

工作目录 `/home/shaokai/Desktop/work/tx82`；scratch `wt-rvv/`；源码树
`riscv-llvm-musl-toolchain/third_party/llvm`（基线 `llvmorg-21.1.0-rc1` = `cbcb48a88c`）。
vendor 侧 `.ident` 显示其真身是 `ts-llvm-project b85caf26e7a507d3ecb57e76c956dd21b5c822a6`。

## 1. TL;DR

| 项 | ours（修复前） | vendor | 修复后 |
|---|---|---|---|
| `riscv_vector.h` 行数 | 422 | 533 | **533** |
| md5 | `255ceebde96f450c22e334dcdb0f3a56` | `a1c48d42cd584daa391ea434e4821cf6` | **逐字节相同** |
| `__riscv_vid_v_u32m1(4)` @ `-mcpu=tsm-x82 -O2` | backend fatal `vsetivli is not supported by TX8` | 成功，`.text` 12 字节 | 定位完成；**补丁已给出但未应用**（文件越界） |

两件事彼此独立：头部缺口 = `.td` 里缺两段**原样文本**（已修好并验证）；
`vsetivli` 缺口 = ISel 里 `PseudoVSETIVLI` 的成形条件（只做定位 + 补丁，未改源码）。

## 2. 缺口与根因（含对既有判断的纠正）

**表观缺口。** 我们的头 422 行 vs vendor 533 行，整个差异是**单个 hunk、纯插入 +111 行**
（`wt-rvv/header.diff`，122 行），分两处：

- **A（36 行）** 在 `enum __RISCV_FRM{...};` 之后：`enum __RISCV_SATURATION{__RISCV_SAT,__RISCV_NO_SAT}`、
  `typedef __int128_t __s128_t __attribute__((vector_size(16)))`、`typedef int8_t fp4/f8e4m3/f8e5m2`、
  以及 `vfloat8e4m3*_t`/`vfloat8e5m2*_t`/`vfloat4*_t`（各 7 个 lmul，全部 alias 到 `__rvv_int8*_t`）。
- **B（74 行）** 在 `__riscv_vsetvl_e8mf4` 之前：21 条 `__riscv_vlmul_ext_v_f8*`、
  24 条 `__riscv_vreinterpret_v_*_f8*`/`_f4*` 别名宏、13 条 `__riscv_vsetvl*_bf` 宏
  （调用 `__builtin_rvv_vsetvli_bf`/`vsetvlimax_bf`）。

**纠正 1：我们的 `.td` 源本来就与归档增量逐字节一致。** 把
`docs/compiler/fork-source/tracked-modified/tracked-modified.diff` 中 `riscv_vector.td`
（行 916–3097）与 `riscv_vector_common.td`（行 3098–3184）两段 hunk 打到 pristine HEAD 上，
得 md5 `672b1b6d6c5c3bbee86131b65d53f839`/`935239c2dd26c178a6f7ad1d44582685`，与**修复前工作树完全相同**。
→ 应修正为：**归档的 vendor delta 本身对 `riscv_vector.td` 不完整**，而不是"我们的补丁不全"。
同理 `clang/utils/TableGen/RISCVVEmitter.cpp`（仅跳过 `vsetvli_bf`/`vsetvlimax_bf`）、
`RISCVVIntrinsicUtils.{h,cpp}`、`BuiltinsRISCV.td` 也已与归档增量一致，**无需改动**。

**纠正 2：真正根因是 `RVVHeader` 原样文本缺失。** `-gen-riscv-vector-header` 的唯一输出函数是
`RVVEmitter::createHeader()`，其中 `printHeaderCode()`（`RISCVVEmitter.cpp:793`）只是把每个
`RVVHeader` 记录的 `HeaderCode` 字符串**原样拼接**，记录按 **def 名字典序**输出：
`frm_enum → vlenb_macro → vsetvl_macro → vxrm_enum`（实测，非 `.td` 定义顺序）。
vendor 那 111 行全是 raw text：A 段是排序落在 `frm_enum` 与 `vlenb_macro` 之间的**新 `RVVHeader`**，
B 段是 `vsetvl_macro` 的 `HeaderCode` 开头。三段生成物（builtins / builtin-codegen / builtin-sema）
修复前后 md5 不变 → 这些 f8/f4 **不是新的 f8 builtin，只是 i8 builtin 的头文件级别名**。

## 3. 精确补丁

只改 **一个文件**：`clang/include/clang/Basic/riscv_vector.td`（**+114 行，0 删除**）；
未碰 `riscv_vector_common.td`、未碰任何 emitter、未碰 `RISCVProcessors.td`。
补丁：**`wt-rvv/0001-rvv-fp8-saturation-vsetvlbf-header.patch`**（130 行，`patch -p1` dry-run 通过）；
另附 vs 上游全量 `wt-rvv/riscv_vector.td.vs-pristine-HEAD.diff`。内容即 `wt-rvv/insA.txt`/`insB.txt`
（直接从 vendor 头部提取）：

1. 在 `}] in def frm_enum : RVVHeader;` 之后追加
   `let HeaderCode = [{ <insA> }] in def frm_saturation_fp8_types : RVVHeader;`
   （名字排序落在 `frm_enum` 与 `vlenb_macro` 之间，保证输出位置）。
2. 在 `def vsetvl_macro: RVVHeader;` 的 `HeaderCode` 的 `[{` 之后、`#define __riscv_vsetvl_e8mf4` 之前插入 `<insB>`。

## 4. 逐字节证据（验收）

用**现有已构建**的 `wt-forkbuild/build/bin/clang-tblgen` 重新生成，无需重建 clang
（invocation 取自 `build.ninja:51444`）：

```
clang-tblgen -gen-riscv-vector-header \
  -I <src>/clang/include/clang/Basic/ -I <src>/clang/lib/Headers -I <src>/clang/include \
  -I <build>/tools/clang/include -I <build>/include -I <src>/llvm/include \
  <src>/clang/include/clang/Basic/riscv_vector.td -o out.h
```

| 产物 | 结果 |
|---|---|
| 生成头（补丁后，从真实源码树直接生成） | 533 行，md5 **`a1c48d42cd584daa391ea434e4821cf6`** |
| vendor `llvm-toolchain/lib/clang/21/include/riscv_vector.h` | 533 行，md5 **`a1c48d42cd584daa391ea434e4821cf6`** |
| `-gen-riscv-vector-builtins` | 前后 **UNCHANGED**（`5eb3ef32728c…`） |
| `-gen-riscv-vector-builtin-codegen` | 前后 **UNCHANGED**（`b4fbee062a7b…`） |
| `-gen-riscv-vector-builtin-sema` | 前后 **UNCHANGED**（`dd289efda3e8…`） |

`#include`/路径问题：**不存在**。生成物只含固定的 `#include <stdint.h>`/`<stddef.h>`，
`-I`/`-o` 不进内容，故跨目录可复现。副本：
`wt-rvv/generated-riscv_vector.h.533.md5-a1c48d42cd584daa391ea434e4821cf6`。

## 5. `vsetivli` vs `vsetvli`

复现用例 `wt-rvv/vid/vid.c`：`#include <riscv_vector.h>` +
`vuint32m1_t vid_test(void) { return __riscv_vid_v_u32m1(4); }`，
`--target=riscv64-unknown-elf -mcpu=tsm-x82 -O2 -c`：vendor 成功，ours
`fatal error: error in backend: vsetivli is not supported by TX8`，
栈顶 `MCTargetDesc/TX8MCInfo.cpp:402`（**只是受害者，不是元凶**）。

**定位：ISel（不是前端，也不是 MC）。** `-S` 对比：ours 发
`vsetivli zero, 4, e32, m1, ta, ma`；vendor 发 `li a0, 4` + `vsetvli zero, a0, e32, m1, ta, ma`
——vendor 走的是**寄存器 AVL** 路径。元凶是 `llvm/lib/Target/RISCV/RISCVISelDAGToDAG.cpp:507`：

```cpp
if (isUInt<5>(AVL)) { ... ReplaceNode(Node, MachineNode(RISCV::PseudoVSETIVLI, ...)); }
```

**机制实证（无需重建）**：该 if 只在 AVL<32 时折叠成 VSETIVLI。用 ours 跑 AVL∈{32,100}，
落入 else 分支后结果与 vendor **逐字节相同**：

- AVL=32：两侧 `.text` 均 14 字节 / **0x7b 指令字 2 条**（`0x02000513` `li a0,32`、
  `0x0d05707b` `vsetvli`、`0x5208947b` `vid.v`，另 2 字节 `ret`）；
- AVL=100：两侧均 14 字节，md5 `273aa4f773dc820effe8698daca803ac`，0x7b 2 条；
- AVL=4（失败档）：vendor 12 字节 / 0x7b 2 条（`4511` 是 2 字节压缩 `li a0,4` + `vsetvli` + `vid.v` + `ret`）；
- vendor 在 AVL=1,2,7,15,16,17,31,32,63,64 **一律**发寄存器形 `vsetvli` → 其 fork 对 `xtsm82s`
  **无条件关闭**了 VSETIVLI 折叠；且后续 `RISCVInsertVSETVLI` 不会把寄存器 AVL 再折回 VSETIVLI。

**修复补丁（已给出、未应用）**：`wt-rvv/0002-PROPOSED-vsetivli-disable-for-xtsm82s.patch` —— 条件改为
`if (isUInt<5>(AVL) && !Subtarget->hasVendorXTsm82s())`。访问器存在
（`RISCVGenSubtargetInfo.inc:569` `GET_SUBTARGETINFO_MACRO(HasVendorXTsm82s,...)`），`Subtarget` 在该函数可见。
该文件**不在本 agent 允许的编辑范围**（仅 `clang/include/clang/Basic/riscv_vector*.td`），
且验证需重建 clang，故**未应用、未做构建级验证**；但 AVL≥32 已逐字节证明目标形态。

## 6. 仍缺什么 / 交接

1. **刷新安装头**：`wt-forkbuild/build/lib/clang/21/include/riscv_vector.h` 仍是旧 422 行版，
   需 `ninja -C wt-forkbuild/build clang`（或该头目标）才能落盘；本 agent 无该目录写权限。
   任何由本源码树构建的其它 build 目录同样需要刷新。
2. **应用 ISel 补丁并重建**，回归 AVL=4（应得 12 字节 / 0x7b 2 条 / 与 vendor 逐字节相同），
   并跑 `__riscv_vsetvl*` 用例确认无回归。
3. **归档修正**：`tracked-modified.diff` 对 `riscv_vector.td` 缺 111 行 raw text，建议并入
   `patches-series/0006-raisa-vector-builtins.patch`（或紧随其后新增一集），避免重建再丢头。
4. 更保险的做法：`RISCVInsertVSETVLI.cpp:1093` 也有 `PseudoVSETIVLI` 成形点（仅在
   `Info.hasAVLImm()` 时触发），可一并加同一 feature 守卫。
5. 未查：vendor 是否还改了其它生成头（`build.ninja` 中确无其它 `-gen-riscv-vector-*`
   头部目标，风险低）。

## 7. 复现命令

```bash
L=.../third_party/llvm; B=wt-forkbuild/build
$B/bin/clang-tblgen -gen-riscv-vector-header -I$L/clang/include/clang/Basic \
  -I$L/clang/lib/Headers -I$L/clang/include -I$B/tools/clang/include \
  -I$B/include -I$L/llvm/include $L/clang/include/clang/Basic/riscv_vector.td -o out.h
md5sum out.h llvm-toolchain/lib/clang/21/include/riscv_vector.h   # 两侧 a1c48d42…
V=llvm-toolchain/bin/clang; O=$B/bin/clang
$V --target=riscv64-unknown-elf -mcpu=tsm-x82 -O2 -c wt-rvv/vid/vid.c -o v.o  # OK
$O --target=riscv64-unknown-elf -mcpu=tsm-x82 -O2 -c wt-rvv/vid/vid.c -o o.o  # fatal
```

## 8. 产物清单（均在 `wt-rvv/`）

`0001-rvv-fp8-saturation-vsetvlbf-header.patch`（**已应用的补丁本体**）、
`riscv_vector.td.vs-pristine-HEAD.diff`、`0002-PROPOSED-vsetivli-disable-for-xtsm82s.patch`（未应用：越界）、
`generated-riscv_vector.h.533.md5-a1c48d42cd584daa391ea434e4821cf6`、`header.diff`、
`vendor-riscv_vector*.td.diff`、`insA.txt`/`insB.txt`、`regen-riscv_vector.h`（旧 422 行）、`vid/`、`td/`、`build_td.py`。
源码改动：`clang/include/clang/Basic/riscv_vector.td`（工作树 md5 `ebd732027c8e544dbfec83cd5297c33c`）。
