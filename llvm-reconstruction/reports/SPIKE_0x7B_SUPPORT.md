# Spike 的 0x7B（custom-3）反向映射支持 —— 状态

日期：2026-10-16。**状态：产物完成并自检通过；已不作为主线**（主线改为用厂商 CModel 执行
设备代码，见 [`COMPILER_VS_CMODEL.md`](COMPILER_VS_CMODEL.md)）。本文件留档，
作为"用开源模拟器跑设备代码"这条替代路径的证据。

工作树：`wt-spiketx8/`（源码树、二进制、补丁、脚本、日志）。
`/tmp/spike_src` 的内容已完整拷出，不依赖 `/tmp`。

---

## 1. 做了什么

厂商 LLVM fork 在 `+xtsm82s` 下把**整个 RVV 指令空间**重编码进 RISC-V **custom-3
（opcode `0x7B`）**。上游 Spike 不认识 `0x7B`。本补丁让 Spike 把 `0x7B` 字**反查回
原版 RVV 字**再走既有 RVV 处理器：

```
W(0x7B word) --tx8_remap_word()--> S(stock RVV 0x57 word) --> 既有 RVV handler
```

映射**不是公式，是厂商二进制里的查表**（`RVV2CTFunctMap`，`index = funct3 + 7*funct6`），
所以补丁里带的是一张由厂商工具链**机械反推**出来的表。

改动三个文件（`spike-0x7b-remap.patch`，314 行）：

| 文件 | 改动 |
| --- | --- |
| `riscv/xtsm82s_remap.h`（新增，16.5 KB） | 171 条规则 + `tx8_remap_word()` |
| `riscv/processor.h` | 声明 `static reg_t tx8_remapped_insn(processor_t*, insn_t, reg_t)` |
| `riscv/processor.cc` | `decode_insn()` 里 `(word & 0x7f) == 0x7b` 时返回该 trampoline；trampoline 反查后递归 `decode_insn`；`disasm()` 打印原版 RVV 助记符（原始 `0x7B` 字仍照打） |

规则形式：`S = (W & ~cmask) | (cval & cmask)`，再按 `moves[]` 做位搬移
（`vmv.v.v` 的源寄存器从 `bits[24:20]` 搬到 `bits[19:15]` 这类）。
**查不到规则的 `0x7B` 字一律 `trap_illegal_instruction`**，不会静默变成别的指令。

---

## 2. 表是怎么来的（机械反推，厂商工具链是 oracle）

`wt-spiketx8/tools/build_table.py`：

1. 取 `docs/compiler/xtsm82s_opcodes.tsv` 里 **172 条 `remapped-RVV`**；
2. 每个 `0x7B` 字交给**厂商 `llvm-objdump`**（即跑厂商自己的
   `TX8MCInfo::tryConvertCTToRVVEncoding`）反汇编；
3. 把反汇编出来的助记符+操作数用**原版 `llvm-mc`** 汇编成原版 RVV 字 `S`；
4. 用单比特探测（翻每一位看重汇编助记符是否变）**自动划出**每条指令的"固定位/操作数位"，
   据此生成 `mask/match`（即解码用的判别位）；
5. 用 3000+ 组"同一段汇编文本分别喂厂商/原版汇编器"的样本对**拟合位模型**并逐组校验；
6. 再要求**语义往返一致**：`stock-objdump(remap(W))` 打印出的助记符+操作数必须和
   `vendor-objdump(W)` 完全一样。

结果：

```
TSV remapped-RVV rows          : 172
table entries                  : 171
unsupported (trap)             : 1        # vmsgt.vv 0xa3f5957b：厂商 objdump 自己也是 <unknown>
model holds on all N variants  : 171/171
semantic round-trip ok         : 170/171  # vid.v 的 TSV 模板字 vd=v0 且 vm=0，在原版 RVV 里是非法组合
ambiguous TX8 mask pairs       : 11       # 全部 agree=True（在共享域上两条规则给同一个字），顺序无关
```

自检：`wt-spiketx8/analysis/selftest/tx8_remap_selftest.cc`
→ `tx8_remap_word: 337/337 pairs OK, 171 rules`。

复现：`python3 tools/build_table.py && python3 tools/emit_patch.py`。

---

## 3. 构建

```
wt-spiketx8/spike-src/            # 上游 Spike 1.1.1-dev (f8a060b) + 补丁，已构建
wt-spiketx8/bin/spike-tx8         # 打了补丁的 spike
wt-spiketx8/bin/spike-stock       # 未打补丁的对照
wt-spiketx8/logs/build-patched.log  # make -j2，MAKE_EXIT=0
```

---

## 4. 差分级测试（`wt-spiketx8/tools/run_diff.sh`）—— **未跑完**

思路：**同一段 C 源码**，用**厂商 clang `-O3 -mcpu=tsm-x82`** 出 `.s`；
这**同一份汇编文本**分别喂给：

* **原版 `llvm-mc`** → `0x57` 目标文件 → 原版 spike；
* **厂商 `llvm-mc -mcpu=tsm-x82`** → `0x7B` 目标文件 → `spike-tx8`。

两边链接同一份 harness（`-fno-vectorize` 编，逐字节相同），`--isa=...zvl256b`
（厂商编译器硬写 VLEN=256），用 spike 的 `mem` 调试命令把 300 个 float 的**位模式**
读出来逐位 diff。三个 kernel：`y=a*x+b`、`y=a*b+c`、`s+=x[i]`。

**当前状态（实测，不含糊）**：

* 已解决并落到脚本里的两个坑：ELF float-ABI 标志要对齐（两侧 `-mattr=+d`，只为 e_flags）；
  fork 会在向量→标量搬运处插入 fork-only 的 `kcore_ncc_read a2, a2`（rd==rs2 的提示指令），
  它不属于 remapped-RVV，两侧一并删除并记录条数。
* 两侧 ELF **都能成功链出来**（`corpus/kA_add.{stock,tx8}.elf`，同一份汇编、两种编码）。
* **但这个 session 里没有拿到端到端的绿色结果**：`spike --debug-cmd` 的
  `until pc 0 <halt>` 不返回（dump 为空、无 stderr），所以"两边 300 个 float 位模式相同"
  这一步**没有完成**。指令级证据只有 §2 的自检 `337/337 pairs OK`。
* 因此本路径的正确性结论**停留在解码/建模层面**，没有端到端执行证据。这条已经不是主线，
  若要继续：先修 `until`/退出机制（换成写 `tohost` 或直接给 `--instructions` 上界）。

---

## 5. 现在能执行什么 / 还有什么会 trap

| 类别 | 条数 | `spike-tx8` 行为 |
| --- | --- | --- |
| remapped-RVV | 172（表里 171） | 反查成原版 RVV 执行 |
| ├ 其中 `vmsgt.vv 0xa3f5957b` | 1 | **trap**（厂商 objdump 自己解不出这条字） |
| fork-only | 216 | **trap**（含 `and.ss`、`kcore_ncc_read`、NE/GEMM/TDMA 等） |
| vendor-cannot-reassemble | 44 | **trap**（`vfmacc.vv`/`vfredosum.vs`/`vfmadd.vf` …，厂商后端自己也无法重编码） |

## 6. 遗留的语义未知

1. **TX8-only 字的 `vm` 位**：fork-only 类（216 条）没有原版对应物，`vm` 位语义未知。
2. **`vsetvli` 保留位**：`0x1ff5f57b` 的 `vtypei=0x1ff` 在 `0x7B` 空间里的保留位含义
   未验证；表里只按 `opcode+funct3` 判别、其余位透传。
3. **VLEN 假设**：厂商编译器硬写 VLEN=256（本次在 CModel 上也实测确认了这一点，
   见 `COMPILER_VS_CMODEL.md` §2.2），所以差分测试必须 `--isa=...zvl256b`。
4. **覆盖范围**：表覆盖 172 条编码的**解码**，但差分级测试只实际执行了 3 个 kernel
   用到的十几条指令，不是全部 172 条。
