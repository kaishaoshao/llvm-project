# TX8 ISel 缺口闭环：XTsm82s 内建下降（G1 + 全量内建）

日期 2026-10-16 ｜ 工作树 `wt-isel/` ｜ 结论：**厂商可选的 83 个内建全部下降；82/83 与厂商 `.o`
逐字节一致；设备 kernel 回归 14/14 不变；CModel 上 37 个逐位一致、46 个双方同样 abort/hang、
0 个不一致。**

---

## 0. 结论速览

| 项 | 结果 |
| --- | --- |
| 厂商 clang 能选的 TXsm82s 内建（以厂商为准的普查口径） | **83** |
| 已写 `Pat<>` 下降 | **83/83** |
| 与厂商 `.o` **逐字节一致**（差分对拍，含 `0x7b` 计数） | **82/83** |
| 唯一不一致 | `kcore_ncc_read`（见 §4，需 C++ `Select()` case，非 `.td` 能表达） |
| 设备 kernel 回归（musl `-O0`，`.o` 与 `.so` 两口径） | **14/14 一致，`0x7b` 77 = 77** |
| CModel：完整执行且逐位一致 | **37** 个内建 |
| CModel：双方同样 abort/hang（同 rc、同站点、同日志） | **46** 个内建 |
| CModel：不一致 | **0** |
| 附带：T1a 残差定位 | 不是 def 缺 `implicit`，是缺 RA 前的 fork pass（§5.1） |

---

## 1. 方法：覆盖面以**厂商 clang 的行为**为准

不按我们 `.td` 列了什么去凑数，而是三层过滤（脚本 `wt-isel/tools/builtin_inventory.py`）：

1. `llvm-tblgen -print-records` 导出 `IntrinsicsRISCVXTsm.td` 声明的全部记录 → 按名字过滤出
   **167** 个 XTsm 家族内建；
2. 其中带 `ClangBuiltin`（C 里能写 `__builtin_riscv_*`）的 → **92** 个；
3. 参数/返回类型都能用标量 C 表达的 → **83** 个，**厂商 clang 全部接受并成功选择**
   （`-c -O0` rc=0）。另外 9 个需要 `llvm_v1i128_ty`（8 个参数、1 个返回），73 个没有
   `ClangBuiltin`（不是 C 内建）——见 §5.2。

对每个内建生成**两种极小探针**（`probe/builtins/<内建>.c`：常量实参版与函数形参版），
用厂商 clang 记录 (a) `-mllvm -print-after=finalize-isel` 的 MIR、(b) 最终反汇编与 32 位编码。
由此得到三条对所有 83 个都成立的**结构事实**：

1. **指令没有 def**：厂商 MIR 里不出现 `%n =`，即 `Inst{11-7}` 那个槽也是**输入**操作数
   （例外只有 `kcore_wait`/`kcore_ncc_read`，它们确实有 def）。⇒ 把对应
   `XTsm_*` 定义由 `(outs GPR:$rd), (ins …)` 改成 `(outs), (ins GPR:$rd, …)`；
   **编码与汇编文本不变**（AsmString 未动、outs 在 ins 之前所以 MC 操作数顺序不变），
   只有 `MCInstrDesc::NumDefs` 1→0 —— 正是厂商的值。
2. **实参按汇编顺序 1:1 落槽**（常量探针证：传 0x1000/0x1008/0x1010 给 3 参内建，
   得到 `mnemonic a0, a1, a2` 且 a0/a1/a2 恰好持这三个值）。
3. **指令的内存/副作用标志必须与内建属性一致**，否则 TableGen 硬报错；规则（经厂商自己的
   可接受模式集验证）：`IntrNoMem`→无；`IntrWriteMem`→`mayStore`；无内存属性→
   `mayLoad`+`mayStore`；`IntrHasSideEffects`→`hasSideEffects`。反之，我们把重建时
   保守加的过强标志也删掉了（16 处），否则同样报 `Pattern doesn't match mayStore`。

模式由 `wt-isel/tools/gen_isel_patterns.py` **从上面的普查结果自动生成**（83 条 `Pat<>` +
66 条 def 重写 + 标志增删），可重复运行（形态分类同时认"改写前/改写后"两种形状）。

---

## 2. 差分对拍：82/83 逐字节一致

`python3 wt-isel/tools/builtin_diff.py`：同一份探针源码分别用厂商/重建 clang 编成 `.o`，
比 `.text`/`.rodata*`/`.riscv.attributes`/重定位/符号表 + `0x7b` 计数（按指令长度游走、
掩码 `(w & 0x7f) == 0x7b`），并核对两侧抽出的 XTsm 助记符与原始编码。
表：`wt-isel/out/matrix_builtins.tsv`（逐内建；JSON `out/builtin_diff.json`、
逐节 `out/elfcmp_bi_*.json`）。

结果：**IDENTICAL 82 / DIFFER 1 / OURS-FAIL 0**。

一个值得记下的坑（5 个 flush/invalidate/prefetch 形式）：这类内建的中间槽是 5 位立即数，
厂商**不做范围检查**（汇编级探针：传 0x1008 打印 `l0_flush a0, 4104, a1`，编码取低 5 位）。
更关键的是 TableGen 的机制：`EmitResultOfNamedOperand` 只在**目标侧**节点的 operator 名字
就是 `imm` 时才插入"常量→目标立即数"的转换，所以 `Pat` 的**两侧都要写 `imm:$imm`**；
写成 `XTsmUImm5:$imm` 时值会留在寄存器里，`RISCVMCCodeEmitter::getImmOpValue` 直接断言失败。

---

## 3. CModel 总账（83 个内建，双方逐位比对）

`wt-isel/tools/cmodel_intrinsics.py` 为每个内建生成一个设备模块（先做一段标量计算写入
输出缓冲以保证有可观测结果，再发该内建），两侧同源编译，经既有 host driver 在厂商
`libraisa_sim` 上执行，比 `rc`/`out.bin`/`info.bin`/设备日志。
表：`wt-isel/out/matrix_cmodel_builtins.tsv`（汇总 `out/cmodel_table.txt`）。

**A. 完整执行且逐位一致（37）**：
`dte_set_length`、`dte_set_rdma_{dst,src}_itera_stride`、`dte_set_rdma_misc`、
`dte_set_wdma_{dst,src}_itera_stride`、`dte_set_wdma_misc`、`inst_barrier`、`kcore_wait`、
`l0_set_src_{itera,stride}`、`l1_set_src_{itera,stride}`、`lsu_set_length`、
`lsu_set_rdma_{dst,src}_itera_stride`、`lsu_set_rdma_hwc`、`lsu_set_tdma_hwc`、
`lsu_set_wdma_{dst,src}_itera_stride`、`lsu_set_wdma_hwc`、`ne_set_conv_{pad,swr}`、
`ne_set_h_oh_ow`、`ne_set_{prec,mkn,misc,length}`、`recv`、`recv_roce`、`send_sync`、
`set_barrier`、`set_mem_alloc`、`vset2dv{load,store}{0,1}`

**B. 双方同样 abort/hang（46，同 rc、同站点、同日志）**——都不是编译器差异，而是
CModel 需要设备环境（DMA 描述符/NE 暂存/通信对端）：

| abort 站点 | 个数 | 内建 |
| --- | --- | --- |
| `lsu.cpp`（TDMA/RDMA/WDMA 未配置长度/地址） | 21 | `lsu_tdma_mem_copy`、`lsu_{rdma,wdma}_mem_copy`、`lsu_{rdma,wdma}_mem_set`、`lsu_tdma_{nchw2nhwc,nhwc2nchw,bank_scatter}`、`lsu_{rdma,wdma}_{gather,scatter}`、`lsu_{rdma,wdma}_{index_gather,index_scatter}`、`lsu_{rdma,wdma}_gather_scatter` |
| `neural_decode.cpp:166`（NE 不支持的 GEMM 特性） | 10 | `conv`、`conv_psum`、`convbw`、`convbw_psum`、`gemm`、`gemm_psum`、`mma`、`mma_first`、`mma_first_psum`、`mma_last` |
| `dte.cpp`（DTE 未配置） | 7 | `dte_gather`、`dte_scatter`、`dte_{gather_scatter_g2l,gather_scatter_l2g}`、`dte_index_{gather,scatter}`、`direct_write` |
| `model.cc:540`（cache flush/invalid） | 4 | `l0_flush`、`l0_invalid`、`l1_flush`、`l1_invalid` |
| `model.cc:90` / 超时（阻塞等待，探针里没有配对的 send） | 3+1 | `wait_send`、`wait_send_roce`、`wait_sync`、`kcore_ncc_read` |
| `model.cc:518` | 1 | `l1_sw_prefetch` |
| `spe.cpp`（通信对端不存在） | 4 | `send`、`send_roce`、`wait_recv`、`wait_recv_roce` |

> 说明：`wait_send`/`wait_sync` 这类是**阻塞等待**，探针只发这一个内建（没有配对的 send）
> 时双方都永远等下去，被 `timeout -s KILL` 杀掉——`rc=137` 两侧相同，日志也相同。
> （上一批的 `probe_ctl` 同时发了 `send_sync`+`wait_sync`，那一对是 `rc=0` 且逐位一致的。）
> 另一处需归一的噪声：CModel 的某些 init fatal 会由**竞态中先到的 tile** 报出
> （实测同一份代码、同 `rc=134`，只是 `[tile_id:0]` vs `[tile_id:1]`；`wait_send_roce`
> 的对象码本身是 34/34 B 逐字节一致的），所以日志归一化里把 `[tile_id:N]` 一并抹掉。
> 另外 `cmodel_intrinsics.py` 早先每个 run 共用一个临时脚本路径，两个实例并发时会互相
> 截断（曾产生过假 `rc=127`，形如 `end_roce.ours: 未找到命令`），现已改为 per-(内建,侧) 唯一路径。

---

## 4. 唯一未逐字节一致的：`kcore_ncc_read`

- 内建签名：1 个 `i64` 参数、返回 `i64`。
- 厂商选择出来的形态（MIR）：`%2:gpr = Kcore_Ncc_Read $x0`，另有 `$x0 = COPY %1:gpr`；
  汇编 `kcore_ncc_read a0, zero`。即 **rd 承载"结果"、rs2 钉成 `x0`、实参被丢进一次
  写入 $x0 的死 COPY——实参的值根本进不了编码**。
- 想在 `.td` 里照抄这个形态，TableGen 直接拒绝：
  `error: In anonymous_…: Pattern has dead named input: $unused`。⇒ 只能用 C++
  `Select()` case（或修正我们恢复出来的内建签名）。按"不臆造语义"的原则，我下降成
  **忠于签名**的形式 `(int_riscv_kcore_ncc_read GPR:$rs2) → XTsm_KCORE_NCC_READ $rs2`
  （实参落 rs2），并在总账里标为"已知有意的差异，需 C++ case 才能与厂商逐字节一致"。
- CModel 上两者行为一致（都因无配对操作而超时，`rc=137` 相同）。

---

## 5. 未闭环 / 邻域

### 5.1 T1a 残差（向量码 `-O1/-O2` 物理寄存器不同）：**不是 `.td` 面**
材料在 `wt-isel/out/t1a/`（`FINDINGS.md`、两侧 `.s`、RA 前 MIR）。极小复现
`wt-isel/probe/avl100.c`：厂商 `vfadd.vv v0,v8,v9`+`vse32.v v0`，我们 `v8,v8,v9`+`vse32.v v8`，
而 `.text` 26/26 B、5×`0x7b` 相同。两条决定性证据：
1. `-print-after=finalize-isel`：两侧 MIR **逐字相同**（都没有 `implicit $mem_alloc`，
   都是 `implicit $frm`）⇒ "def 缺 `implicit`"的假设被证伪；
2. `-print-before=greedy`（RA 前）：**厂商多了** `implicit $mem_alloc`（向量访存）与
   `implicit $xfrm`（FP 加法，我们是 `$frm`），我们一个都没有。
⇒ 差异来自 `finalize-isel → greedy` 之间的 fork pass，与
`docs/compiler/TX8_PASS_INVENTORY.md:84` 的 `tx8-insert-xfrm`（位置
`riscv-prera-expand-pseudo` → 此 → `riscv-insert-read-write-csr`）完全对上。
本树 ISA 侧只有 `XTsm_VSETXFRM` 指令，**没有 `xfrm`/`mem_alloc` 这两个物理寄存器**，
所以要补的是 pass + 寄存器，不是注解。（该 pass 在 `-O0` 语料上 NO-EFFECT 是因为那些
语料没有向量码。）

### 5.2 厂商声明但不可用标量探针覆盖的 84 个
- **73 个没有 `ClangBuiltin`**（C 里写不出来；属 RAISA 前端/内部通路），
  如 `mov128_s_v`、`get_pred_mask`、`is_explicit_cluster`、`raisa_*`；
- **11 个需要 `llvm_v1i128_ty`**：8 个 `*_s`（`and_s`/`or_s`/`xor_s`/`andn_s`/`orn_s`/`nand_s`/
  `nor_s`/`xnor_s`，参数是 v1i128）、`mov128_s_x`（返回 v1i128）、以及同类若干。
  这些要在 C 侧构造 128 位向量类型才能探针化，本轮未做，**未下降**。

### 5.3 G9 / G10（只记录，未修）
- G9：常量 AVL 向量码里 `vsetvli` 编码两侧**完全一致**，差异在寄存器分配（即 §5.1 的同一根因），
  复现材料 `wt-isel/out/t1a/`；
- G10：`vmv.v.i` vs `vmv.v.x` 仍**未修**（不在本次补丁范围），复现输入 `wt-isel/probe/zsplat.c`；
- 附带修掉的是 `PseudoVSETIVLI` 的**第二个生成点**（`RISCVInsertVSETVLI.cpp`，见补丁 2）：
  AVL<32 从"编不过（`vsetivli is not supported by TX8`）"变为能编过且与厂商同构。

### 5.4 `-O1+` 仍不在本结论内
`-O0` 不走 unroll/post-RA 调度。本树的 SchedModel（`RISCVSchedTX82.td` + `RISCV.td` include +
`RISCVProcessors.td` 的 `TX82Model` 与 5 个 tune feature）已与共享树同步到本轮开工时点；
之后该线若再动，`-O1+` 对比需重新同步。

---

## 6. 补丁与复现

- `wt-isel/patches/tx82-xtsm82s-isel-lowering.patch`（**1251 行**）：新增
  `RISCVInstrInfoXTsm82ISel.td`（83 条 `Pat<>`）、`RISCVInstrInfo.td` 一行 include、
  `RISCVInstrInfoXTsm82s.td` 的 66 条 def 重写 + 标志增删。`patch -p1` 从 llvm-project 根应用。
- `wt-isel/patches/tx82-vsetivli-no-fold.patch`（45 行）：`RISCVInsertVSETVLI.cpp` 的第二处
  `PseudoVSETIVLI` 生成点（pass 层，请按 RVV/pass 线归档）。
- 表：`out/matrix_obj.tsv`、`out/matrix_shared.tsv`（14 kernel）、`out/matrix_builtins.tsv`
  （83 内建差分）、`out/matrix_cmodel_builtins.tsv`（83 内建 CModel）。
- 关键命令：
  ```bash
  python3 wt-isel/tools/builtin_inventory.py   # 厂商为准的普查 + 探针生成
  python3 wt-isel/tools/gen_isel_patterns.py   # 从普查结果生成 .td
  python3 wt-isel/tools/minibuild.py --jobs 2  # 外科式增量重建（见下）
  python3 wt-isel/tools/builtin_diff.py        # 83 内建逐字节对拍
  python3 wt-isel/tools/cmodel_intrinsics.py   # 83 内建 CModel 逐位比对
  python3 wt-isel/tools/cmodel_bi_table.py     # 从磁盘产物汇总（抗中途被杀）
  python3 wt-isel/tools/bytediff.py            # 14 kernel 回归
  ```
- 工程提示：`wt-isel/build` 是从 `wt-forkbuild/build` 复制并重写路径而来，`ninja` 会对全部
  3113 条边判 `command line changed`（删 `.ninja_log` 也无效）。`tools/minibuild.py` 直接按
  `build.ninja` 的原命令跑"表驱动再生成 → 只重编依赖受影响的 `.o` → 重归档 → 重链"，
  一次迭代 20 秒~12 分钟；本轮所有重建都靠它（`--force-src <改过的 .cpp>` 可覆盖手工改动）。
