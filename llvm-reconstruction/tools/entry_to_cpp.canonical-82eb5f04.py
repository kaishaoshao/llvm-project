#!/usr/bin/env python3
"""entry_to_cpp.py —— 把厂商设备内核**入口体**（带栈帧 + 调用的非叶子函数）逐指令直译成 C++。

为什么要这个工具
================
`tools/vendor_oracle/disasm_to_cpp.py` 只吃**叶子**函数（出现 `jal`/写 `sp` 就报错），
而设备入口体恰恰全是"建栈帧 + 打一堆被调者"。手工直译 2~12 KB 的入口体一次要几小时，
16 个唯一体（见 docs/recovered/device-gemm-entry-groups.md）不现实；按语义重写又违反
AGENTS.md §1（拟合 ≠ 还原）。所以这里给出**入口体版**的机械直译：

  * 每条厂商指令 → 一条 C++ 语句，语句后带 `/* 0xADDR  mnemonic operands */`
    （地址与反汇编逐行可对照）；
  * 分支 → `goto L<十六进制地址>`，标签就是厂商 ELF 里的真地址；
  * 通用寄存器 → `X[32]`（`uint64_t`，XLEN=64，X[0] 恒 0）；
  * 栈帧 → 一个 `alignas(16)` 的字节数组，`sp` 初值 = 数组尾；
  * `lw/lwu/ld/...` 的访存 → 对 `(uintptr_t)addr` 的真访存（入口只解引用 P 指针与自己的
    栈，对拍时两者都是真内存）；
  * `auipc`+`addi/ld/...` 的 `.rodata` 基址 → **把该处常量数据快照成本文件的 static 数组**，
    基址指向快照（厂商 `.rodata` 里是常量表，快照后行为一致）；
  * `jal <PLT槽>` → 按被调者真实**实参个数**发起调用（>8 个的从**模拟栈**上取
    `sp+0/8/16/...`，这正是 RISC-V ABI 的栈实参布局）。

适用边界（超出就报错退出，绝不输出"看着能编译"的 C++）
=====================================================
* 只支持标量整型寄存器语义；出现 `jalr`（间接调用/跳转表）、浮点、RVV、或按**编码**
  判定的 TX8 私有指令（低 7 位 ∈ {0x0b,0x2b,0x5b,0x7b}）就报错；
* 尾部调用 `j <PLT槽>` / `j <本 .so 的局部函数>` 也当调用处理（按目标地址判定，
  与 `jal` 共用实参个数表）；实测 matmul.so 的 mm 入口最后一条就是 `j <plt槽>`；
* 只支持 `jal` 目标是 `.plt` 槽且能按 `JUMP_SLOT` 回填名字的情形；
* 只支持 `auipc` 后面能被消费成常量数据基址的情形。

用法
====
    tools/raops_oracle/entry_to_cpp.py <kernel.so> <符号名> [--out f.cpp]
    tools/raops_oracle/entry_to_cpp.py <kernel.so> <符号> --check-only
改动记录 (每次提升到共享 tools/entry_to_cpp.py 前必须在这里加一条)
=================================================================
* 2026-10-18  mm/bmm/scaled 入口体线 (wt-gemmpsum, commit 22e6bc3 + 本提交):
  1. **尾部调用**: `j <PLT槽>` / `j <本 .so 的局部函数>` 现在按调用处理
     (`tail_name()` / `call_args()` 与 `jal` 共用实参表)。实测 `matmul.so` 的
     `main_kernel_mm_tile_m_split_kn @0x1a884` 是 `j 0x114d0`(wait_cmd_done 的 PLT 槽),
     旧版把它当 `goto` ⇒ 生成的 C++ 引用了不存在的 label, 直接编不过。
  2. ARITY 增补 (实参个数从**被调者自己的体**反推, gemm 族那两个 size calculator 例外:
     直接读我方已实现并验证的原型 `src/device/tx82_device_gemm_size_v2.cpp:206/276`):
       get_gemm_compute_align_size 1 / get_gemm_scale_compute_align_size 1 /
       get_gemm_spma_size 5 / get_gemm_spmw_size 6 /
       dte_rdma_src_stride_copy 6 / _with_ratio 7 / _broadcast_with_ratio 8 /
       drt_get_dtype_bits 1 / dev_dtype_2_ne_dtype_with_same_size 2
  3. **回贴** `dte_rdma_gather_copy: 9` (上一版 canonical 把它写成 3 —— 那是回归,
     就是"620 条用例假绿"那个盲区)。复核办法: `gen_gemm_entries.py --check` 必须 16/16
     最新 (重生成只差一行 `// 模拟栈帧 (--stack)` 注释)。
* 2026-10-18  低价符号线 (wt-gemmpsum, lsu_rdma_stride_copy + lsu_write_gemm_data_to_n3d):
  4. **公共辅助的形参/返回值两档** (`ENTRY_ARGS` / `RET_VALUE`)。此前生成的符号一律是
     `extern "C" void <sym>(uint64_t a0)`, 那是"内核入口只拿一个 P"的口径; mem-engine
     公共辅助不一样:
       lsu_rdma_stride_copy        @matmul.so:0x12476        读 a0..a5       => 6 个, 返回 a0
       lsu_write_gemm_data_to_n3d  @n3d_linear_2.so:0x1961c  读 a0..a7 + t3  => 9 个, 返回 a0
     缺省路径**逐字节不变** (已有 16 个 gemm 体 + 11 个 mm 体不受影响, 已用
     `gen_gemm_entries.py --check` / `gen_mm_entries.py --check` 复核)。
     同时 `ret` 指令在 RET_VALUE 符号里生成 `return X[10];` (此前一律 `return;`, 在
     非 void 函数里编不过)。
  5. ARITY 增补: `lsu_rdma_gather: 3`、`lsu_wdma_gather_scatter: 3`、
     `lsu_stride_load_gemm_data_from_n3d: 9` (matmul 版; 9 = a0..a7 + t3,
     与 wt-runtime 已落地的实现/真值同口径)
     (实参个数从被调者自己的体反推: 都是 `(a0,a1,a2)` 三个)。
* 2026-10-18  标量浮点线 (wt-gemmpsum, addmm/dot 两条 C 档入口):
  6. **标量浮点档** (`flw/fsw/fld/fsd` + `fadd.s/fmul.s`)。此前本体里出现任何 `f*`
     指令一律响亮退出 —— 这就是 `main_kernel_addmm_tile_m_split_kn` 与
     `main_kernel_dot_default` 两条入口被拒的原因 (实测 fp 指令集: addmm 只有 6 条
     `flw`; dot 有 19 `flw` / 2 `fsw` / 15 `fadd.s`，**零 `fmul.s`**)。
     口径 (逐条写在生成文件的语义约定里):
       * 寄存器文件 `F[32]` 是 **uint64_t 位容器**: `.s` 只用低 32 位并按 RISC-V
         NaN-boxing 把高 32 位置 1, `.d` 用满 64 位 ⇒ 一份文件同时承载 FP32/FP64 位模式;
       * 运算走 C 的 `float` (IEEE-754 binary32, 就近偶数), 中间量用 `volatile` 强制
         逐条舍入 (禁掉编译器跨语句收缩成 FMA) —— 与 `fadd.s`/`fmul.s` 的"单次舍入"一致;
       * 白名单**恰好**是上面 6 条; 其它 `f*` (含 `fadd.d`/`fmul.d`/`fcvt.*`/`fmv.x.w`)
         仍然响亮退出, 绝不放行"看着能编译"的体。
     缺省路径**逐字节不变** (16 个 gemm 体 + 11 个 mm 体, 两份 `--check` 复核):
     FP 寄存器文件与辅助函数只在 `fp_used` 为真时生成。
  7. **浮点实参表** (`FP_ARGS`): 被调者前面 K 个实参走 `fa0..fa(K-1)` (psABI 的浮点
     寄存器组), 其余整型实参仍走 `a0..a7`/栈。实参总个数 (`ARITY`) 仍含浮点。
     实测 (从被调者自己的体反推, 见 `ARITY` 注释):
       muls_compute_{fp32,fp16,bf16}              1 浮点 + a0..a3 => 5
       mac_compute_with_broadcast_2d_{fp32,fp16,bf16} 1 浮点 + a0..a7 => 9
       float_2_{fp16,bf16}                        1 浮点 => 1
  8. `ENTRY_ARGS`/`RET_VALUE` 增补 `dte_wdma_dst_stride_copy` (6 实参, 返回 a0)。
     缺省路径逐字节不变 (16 gemm + 11 mm 体两份 `--check` 复核)。
     ---
"""
import argparse
import collections
import re
import subprocess
import sys

V = "/home/shaokai/Desktop/work/tx82/llvm-toolchain/bin"

# 被调者的**实参总个数**（8 个进 a0..a7，其余按 ABI 放在 sp+0, sp+8, ...）。
# 依据: 被调者自己的体 —— 参 tools/raops_oracle/probes/gemm_v2_trace.c 的形参表。
ARITY = {
    "set_tile_topology": 3,
    "get_global_tile_id": 0,
    "get_die_tile_id": 3,
    "get_tile_num_per_chip": 0,
    "get_prev_tile_id": 1,
    "get_next_tile_id": 1,
    "krt_get_chip_id": 0,
    "krt_get_die_id": 0,
    "krt_get_tile_id": 0,
    "ne_dtype_2_dev_dtype": 1,
    "dev_dtype_2_ne_dtype": 1,
    "get_g3d_ne_layout": 5,
    "compact_to_ne_layout_rvv": 4,
    "ne_to_compact_layout_rvv": 4,
    "dte_set_length": 3,
    "dte_set_rdma_misc": 3,
    "dte_set_rdma_src_itera_stride": 6,
    "dte_set_rdma_dst_itera_stride": 6,
    "dte_set_wdma_misc": 3,
    "dte_set_wdma_src_itera_stride": 6,
    "dte_set_wdma_dst_itera_stride": 6,
    "lsu_set_length": 3,
    "lsu_set_rdma_src_itera_stride": 6,
    "lsu_set_rdma_dst_itera_stride": 6,
    "lsu_set_wdma_src_itera_stride": 6,
    "lsu_set_wdma_dst_itera_stride": 6,
    "dte_gather_scatter_g2l": 2,
    "dte_gather_scatter_l2g": 2,
    "dte_rdma_copy": 3,
    "dte_rdma_gather_copy": 9,  # 厂商体读 a0..a7 + ld 0x60(sp) (optimized_gemm.so @0x11b04): 9 个
    "dte_rdma_stride_copy": 6,
    "lsu_rdma_memcopy": 3,
    "lsu_wdma_memcopy": 3,
    "lsu_rdma_copy": 3,
    "lsu_wdma_copy": 3,
    "lsu_rdma_gather_scatter": 3,
    # lsu_write_gemm_data_to_n3d / lsu_rdma_stride_copy 的被调者 (2026-10-18 低价符号线):
    # lsu_rdma_gather @matmul.so:0x11740 / n3d_linear_2.so 读 a0,a1,a2 => 3 个
    "lsu_rdma_gather": 3,
    "lsu_wdma_gather_scatter": 3,
    # lsu_stride_load_gemm_data_from_n3d @matmul.so:0x2811a: 读 a0..a7 + t3 => 9 个
    # (实现在 wt-runtime 的 patches/03-lsu-stride-gemm; 那边两个同名不同体的实参个数
    #  是 matmul 9 / n3d_linear_2 10, 见其 docs/recovered/device-lsu-stride-gemm.md)
    "lsu_stride_load_gemm_data_from_n3d": 9,
    "lsu_tdma_copy": 3,
    "send": 6,
    "recv": 6,
    "wait_send": 6,
    "wait_recv": 6,
    "wait_cmd_done": 0,
    # ---- matmul.so (mm/bmm/scaled_mm/scaled_bmm 入口族) 的被调者 ----
    # 实参个数从**被调者自己的体**反推 (a-reg 读点), 不是照抄我方实现:
    #   get_gemm_compute_align_size       @0x127a0: mv s0,a0 ; bltu a0,s0 => 1 个 (dtype)
    #   dte_rdma_src_stride_copy          @0x1203a: li a6,1; jal with_ratio => 6 个 (a0..a5)
    #   dte_rdma_src_stride_copy_with_ratio @0x11e46: 体里读 a0..a6   => 7 个
    #   dte_rdma_src_stride_copy_broadcast_with_ratio @0x11f2a: 读 a0..a7 => 8 个
    #   get_gemm_scale_compute_align_size  @0x12812: mv s0,a0       => 1 个
    "get_gemm_compute_align_size": 1,
    # gemm 族里已实现并已验证的 size calculator: arity 直接读我方真实原型
    # (src/device/tx82_device_gemm_size_v2.cpp:206/276, 那两个是 T 真定义),
    # 不再从厂商体重推一遍 (上级 2026-10-18 裁决); 重生成 gemm 体逐字节不变已用
    # gen_gemm_entries.py --check 复核。
    "get_gemm_spma_size": 5,
    "get_gemm_spmw_size": 6,
    "get_gemm_scale_compute_align_size": 1,
    "dte_rdma_src_stride_copy": 6,
    "dte_rdma_src_stride_copy_with_ratio": 7,
    "dte_rdma_src_stride_copy_broadcast_with_ratio": 8,
    "drt_get_dtype_bits": 1,
    "dev_dtype_2_ne_dtype_with_same_size": 2,
    "gemm_v2": 14,
    "gemm_with_psum_v2": 15,
    "gemm_with_input_psum_v2": 17,
    "ne_gemm": 4,
    "ne_gemm_psum": 5,
    # ---- x3d_copy 家族 (n3d_from_to_g3d_v3.so) 的入口被调者 ----
    "drt_get_dtype_size": 1,
    "get_file_name": 0,
    "krt_get_die_num_per_chip": 0,
    "krt_get_tile_num_per_die": 0,
    "krt_barrier_in_chip": 0,
    # printf 是可变参: 两个调用点都是 (fmt, tid, file_name, line, s, s, 值) = 7 个,
    # 由 .rodata 里的那把格式串看出。按 7 个发; 多发的寄存器值两侧不可比.
    "printf": 7,
    "x3d_copy_all": 9,
    "x3d_copy": 10,
    # ---- addmm/dot 两条 C 档入口的被调者 (2026-10-18 标量浮点线) ----
    # 实参个数从**被调者自己的体**反推 (matmul.so 内有定义, 走 PLT 可剪断):
    #   muls_compute_fp32 @0x16016: 读 fa0 (`fmv.x.w a7,fa0`) + a0/a1/a2/a3 => 5
    #   muls_compute_fp16 @0x15ef2 / muls_compute_bf16 @0x15f40: 同上 => 5
    #   mac_compute_with_broadcast_2d_fp32 @0x163ae: 读 fa0 + a0..a7 => 9
    #   mac_compute_with_broadcast_2d_fp16 @0x16114 / _bf16 @0x16236: 同上 => 9
    #   float_2_fp16 @0x3050c / float_2_bf16 @0x3056a: 只读 fa0 (`fsw fa0,0x4(sp)`) => 1
    "muls_compute_fp32": 5,
    "muls_compute_fp16": 5,
    "muls_compute_bf16": 5,
    "mac_compute_with_broadcast_2d_fp32": 9,
    "mac_compute_with_broadcast_2d_fp16": 9,
    "mac_compute_with_broadcast_2d_bf16": 9,
    "float_2_fp16": 1,
    "float_2_bf16": 1,
    # dte_wdma_dst_stride_copy @matmul.so:0x12120 (202 B): 体里读 a0..a5 => 6 个
    "dte_wdma_dst_stride_copy": 6,
    # addmm 的另一个公共辅助被调者: 它自己的 ENTRY_ARGS 已有 6 个 (a0..a5),
    # 作为**被调者**时同一个数 (实参个数是符号的属性, 不是调用点的)。
    "lsu_rdma_stride_copy": 6,
    # dot_default 的两个 krt 助手 (实现在 wt-runtime 的 patches/02-batch-D-*, 本树没有):
    #   krt_barrier_simd(void)                               => 0
    #   krt_flush_dcache_range(uint64_t* start_va, size_t)   => 2
    "krt_barrier_simd": 0,
    "krt_flush_dcache_range": 2,
    # dot_default 的另两个被调者: dte_wdma_copy 3 (与我方实现同原型, 见
    # src/device/tx82_device_dte_copy.cpp:89); dte_wdma_copy 的兄弟族都是 3 实参。
    "dte_wdma_copy": 3,
}

# 前面有 K 个 **float** 实参的被调者 (2026-10-18 标量浮点线)。
# RISC-V psABI 里浮点实参与整型实参**各自编号**: float 走 fa0..fa7, 整型仍走 a0..a7,
# 所以"前 K 个 float + 其余整型"是调用点上**寄存器分配**的完整描述, 与 C 形参表里
# 浮点/整型的相互次序无关 (只要浮点个数与整型个数对上)。
# 记录器与生成体两侧都用同一张表 ⇒ 观测面一致。ARITY 里的总数仍含浮点。
FP_ARGS = {
    "muls_compute_fp32": 1,
    "muls_compute_fp16": 1,
    "muls_compute_bf16": 1,
    "mac_compute_with_broadcast_2d_fp32": 1,
    "mac_compute_with_broadcast_2d_fp16": 1,
    "mac_compute_with_broadcast_2d_bf16": 1,
    "float_2_fp16": 1,
    "float_2_bf16": 1,
}

# 标量浮点白名单 (恰好这 6 条; 其它 f* 一律响亮退出, 见改动记录第 6 条)。
FP_LOADS = {"flw": 4, "fld": 8}
FP_STORES = {"fsw": 4, "fsd": 8}
FP_ARITH = {"fadd.s": "FaddS", "fmul.s": "FmulS"}
FP_SUPPORTED = set(FP_LOADS) | set(FP_STORES) | set(FP_ARITH)

# 少数厂商**静态**函数把第 9/10 个实参放在 t3/t4, 不是标准 psABI 的栈:
#   x3d_copy_all: `mv t4,t3 ; mv t3,a7`  => 第 9 个在 t3
#   x3d_copy:     同上 + `sd t4,-0xd0(s0)` => 第 9/10 个在 t3/t4
# 返回值**不可观测**的被调者: 厂商本体是 `void` (反汇编里 `jal` 落下后 a0 是垃圾, 调用点
# 之后也不读它)。按 void 声明 + 语句调用, 否则生成的 `X[10] = f(...)` 在 C++ 里编不过
# (我方实现是 void)。已核对: n3d_from_to_g3d_v3 的两个调用点之后 X[10] 都是重新赋值。
VOID_RET = {"x3d_copy_all", "x3d_copy",
            # dot_default 的两个 krt 助手: 厂商/我方都是 void (调用点之后 a0 都被重新赋值:
            # 0x18736 `ld a0,0x40(sp)` / 0x18888 `lwu a0,0x10(s7)`), 返回值不可观测。
            "krt_barrier_simd", "krt_flush_dcache_range"}

# 设备运行时提供的 krt_* 助手: 厂商 kernel 里是 **UND 导入** (由 krt/TUFW 固件提供),
# 本仓的分支上还没有实现 (那个补丁在 wt-runtime 的分支里)。声明成 weak:
#   * 库能链接 (否则 libtx82_cmodel.so 会挂三个 undefined reference, 把别的测试一起打红);
#   * 不提供任何兜底实现 —— 没定义时就是空指针, 调用即响铃中止, 绝不静默返回 0
#     (AGENTS §5: 重复的 weak 定义会静默回退, 比没实现更危险);
#   * 测试侧用同名强定义就能顶掉 (探针本来就给了可编程值)。
WEAK_CALLEES = {"krt_get_die_num_per_chip", "krt_get_tile_num_per_die", "krt_barrier_in_chip",
                # addmm 的 6 个 NE 引擎叶子 (muls_compute_* / mac_compute_with_broadcast_2d_*):
                # 它们含 TX8 私有指令, 本树没有实现 (闭包口径里按 private 跳过, 不算"符号缺口",
                # 但库侧必须有定义/弱引用, 否则**所有**链 libtx82_cmodel.so 的探针都链接失败
                # —— 实测一个未定义的名字就干掉了 70+ 个 vendor_oracle 测试). weak 声明 =
                # 没定义时是空指针, 调用即响铃中止, 绝不静默返回 0。
                # 2026-10-18 起不再 weak: 6 个 NE 引擎叶子已在
                # src/device/tx82_device_ne_compute.cpp 真实现 (真值 = 厂商 CModel 执行
                # 厂商指令字节, 差分 test_device_ne_compute 108 用例逐字节一致)。
                # 保留注释说明它们**曾经**在这里, 免得下一个人误以为又缺实现。
                # dot_default 用的两个 krt 助手: 实现在 wt-runtime (patches/02-batch-D-*), 本树
                # 没有 ⇒ 与上面三个同口径: weak 声明, 不提供兜底实现 (调用即响铃中止)。
                "krt_barrier_simd", "krt_flush_dcache_range"}

EXTRA_REGS = {
    "x3d_copy_all": ["t3"],
    "x3d_copy": ["t3", "t4"],
}

# 入口/公共辅助**自己的**形参表 (默认缺省 = "内核入口只拿一个 P", 即 `uint64_t a0`)。
# 设备公共辅助不是内核入口, 它们有真实多参, 而且第 9 个之后的实参可能不走 psABI 栈:
#   lsu_rdma_stride_copy        @matmul.so:0x12476       读 a0..a5      => 6 个
#   lsu_write_gemm_data_to_n3d  @n3d_linear_2.so:0x1961c 读 a0..a5 + t3 => 7 个
#     (本体读 a6/a7 当两段开关+计数: `beqz s8` / `beqz s4`; `mv s7,t3` 是第 9 个实参,
#      走 TX8 的 t3, 不是 psABI 栈; 见 n3d_linear_2.so main_kernel_n3d_linear2_split_k)
ENTRY_ARGS = {
    "lsu_rdma_stride_copy": ["a0", "a1", "a2", "a3", "a4", "a5"],
    "lsu_write_gemm_data_to_n3d": ["a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t3"],
    # dte_wdma_dst_stride_copy @matmul.so:0x12120 (202 B): 体里读 a0..a5
    "dte_wdma_dst_stride_copy": ["a0", "a1", "a2", "a3", "a4", "a5"],
}

# 返回 a0 的公共辅助: 生成体末尾 `return X[10];` (默认内核入口是 void)。
RET_VALUE = {"lsu_rdma_stride_copy", "lsu_write_gemm_data_to_n3d",
             "dte_wdma_dst_stride_copy"}

REGNAMES = ("zero ra sp gp tp t0 t1 t2 s0 s1 a0 a1 a2 a3 a4 a5 a6 a7 "
            "s2 s3 s4 s5 s6 s7 s8 s9 s10 s11 t3 t4 t5 t6").split()
REGS = {}
for _i, _n in enumerate(REGNAMES):
    REGS[_n] = _i
    REGS["x%d" % _i] = _i

# 标量浮点寄存器名 (2026-10-18)。RISC-V F/D 扩展的 ABI 名:
#   f0..f7=ft0..7 / f8,f9=fs0,1 / f10..17=fa0..7 / f18..27=fs2..11 / f28..31=ft8..11
FPREGNAMES = ("ft0 ft1 ft2 ft3 ft4 ft5 ft6 ft7 fs0 fs1 fa0 fa1 fa2 fa3 fa4 fa5 fa6 fa7 "
              "fs2 fs3 fs4 fs5 fs6 fs7 fs8 fs9 fs10 fs11 ft8 ft9 ft10 ft11").split()
FPREGS = {}
for _i, _n in enumerate(FPREGNAMES):
    FPREGS[_n] = _i
    FPREGS["f%d" % _i] = _i


def sh(a):
    return subprocess.run(a, capture_output=True, text=True).stdout


def parse_imm(s):
    if isinstance(s, int):
        return s
    s = s.strip()
    neg = s.startswith("-")
    if neg:
        s = s[1:]
    v = int(s, 16) if s.startswith("0x") else int(s, 0)
    return -v if neg else v


def sext(v, bits):
    m = 1 << (bits - 1)
    v &= (1 << bits) - 1
    return (v ^ m) - m


class So:
    def __init__(self, path):
        self.path = path
        self.blob = open(path, "rb").read()
        self.secs = []
        for l in sh([f"{V}/llvm-readelf", "-S", "-W", path]).splitlines():
            m = re.match(r"\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", l)
            if m:
                self.secs.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16),
                                  int(m.group(4), 16)))
        m = re.search(r"\]\s+\.plt\s+\S+\s+([0-9a-f]+)", sh([f"{V}/llvm-readelf", "-S", path]))
        syms = [l.split()[-3] for l in sh([f"{V}/llvm-readelf", "-r", path]).splitlines()
                if "JUMP_SLOT" in l]
        self.plt = {}
        if m:
            a = int(m.group(1), 16)
            self.plt = {a + 32 + i * 16: s for i, s in enumerate(syms)}
        dis = sh([f"{V}/llvm-objdump", "-d", "--no-show-raw-insn", "--mattr=+xtsm82s,+v", path])
        self.bodies = collections.OrderedDict()
        cur = None
        for ln in dis.splitlines():
            h = re.match(r"^([0-9a-f]+) <(.+)>:$", ln)
            if h:
                cur = h.group(2)
                self.bodies[cur] = []
                continue
            if cur is not None:
                self.bodies[cur].append(ln)
        dis2 = sh([f"{V}/llvm-objdump", "-d", "--mattr=+xtsm82s,+v", path])
        self.raw = {}
        for ln in dis2.splitlines():
            m = re.match(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)", ln)
            if m:
                self.raw[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3))

    def sec_of(self, addr):
        for name, va, off, sz in self.secs:
            if va and va <= addr < va + sz:
                return name, va, off, sz
        return None

    def read(self, addr, n):
        s = self.sec_of(addr)
        if not s:
            return None
        name, va, off, sz = s
        start = off + (addr - va)
        data = self.blob[start:start + n]
        if len(data) < n:
            data += b"\0" * (n - len(data))
        return data


HELPERS = r"""
namespace {

inline uint64_t Ld(uint64_t a) { uint64_t v; std::memcpy(&v, (void*)(uintptr_t)a, 8); return v; }
inline uint64_t Lw(uint64_t a) { int32_t v; std::memcpy(&v, (void*)(uintptr_t)a, 4); return (uint64_t)(int64_t)v; }
inline uint64_t Lwu(uint64_t a) { uint32_t v; std::memcpy(&v, (void*)(uintptr_t)a, 4); return (uint64_t)v; }
inline uint64_t Lh(uint64_t a) { int16_t v; std::memcpy(&v, (void*)(uintptr_t)a, 2); return (uint64_t)(int64_t)v; }
inline uint64_t Lhu(uint64_t a) { uint16_t v; std::memcpy(&v, (void*)(uintptr_t)a, 2); return (uint64_t)v; }
inline uint64_t Lb(uint64_t a) { int8_t v; std::memcpy(&v, (void*)(uintptr_t)a, 1); return (uint64_t)(int64_t)v; }
inline uint64_t Lbu(uint64_t a) { uint8_t v; std::memcpy(&v, (void*)(uintptr_t)a, 1); return (uint64_t)v; }
inline void Sd(uint64_t a, uint64_t v) { std::memcpy((void*)(uintptr_t)a, &v, 8); }
inline void Sw(uint64_t a, uint64_t v) { uint32_t t = (uint32_t)v; std::memcpy((void*)(uintptr_t)a, &t, 4); }
inline void Sh(uint64_t a, uint64_t v) { uint16_t t = (uint16_t)v; std::memcpy((void*)(uintptr_t)a, &t, 2); }
inline void Sb(uint64_t a, uint64_t v) { uint8_t t = (uint8_t)v; std::memcpy((void*)(uintptr_t)a, &t, 1); }
// 32 位运算结果符号扩展到 64 位 (RISC-V 的 *w 指令)
inline uint64_t Sx32(uint64_t v) { return (uint64_t)(int64_t)(int32_t)(uint32_t)v; }

}  // namespace
"""

# 标量浮点辅助 (2026-10-18)。**只在本体真的用到 FP 时生成** —— 这样已有的 16 个 gemm 体
# 与 11 个 mm 体逐字节不变。
# 口径:
#   * F[32] 是 uint64_t 位容器 (XLEN=64)。`.s` 只用低 32 位, 并按 RISC-V 的 NaN-boxing
#     把高 32 位置 1; `.d` 用满 64 位。⇒ 一份寄存器文件同时承载 FP32/FP64 的位模式。
#   * 运算走 C 的 float (IEEE-754 binary32, 就近偶数)。中间量声明成 volatile ⇒ 每条
#     fadd.s/fmul.s 都**单独舍入**, 编译器不能跨语句收缩成 FMA (否则两次运算只舍入一次,
#     与 RISC-V 的"逐条舍入"不同)。
#   * 已知差异 (只在差分覆盖到的值域内被验证): RISC-V 规定算术结果是 canonical NaN
#     (0x7fc00000), x86 SSE 传播源 NaN 的载荷; 非规格化数两侧都按 IEEE 处理 (x86 没有
#     开 FTZ/DAZ)。两个入口的用例网格只喂 P 里的位模式与 0x4004_00xx 窗口的常数,
#     差分逐行一致 ⇒ 这些值域上等价。
FP_HELPERS = r"""
namespace {

// 标量浮点 (FP32/FP64) —— 与本体里的 flw/fsw/fld/fsd/fadd.s/fmul.s 一一对应。
inline uint32_t FlwBits(uint64_t a) {
    uint32_t v;
    std::memcpy(&v, (void*)(uintptr_t)a, 4);
    return v;
}
inline void FswBits(uint64_t a, uint32_t v) { std::memcpy((void*)(uintptr_t)a, &v, 4); }
inline float F32Val(uint64_t b) {
    float f;
    uint32_t v = (uint32_t)b;
    std::memcpy(&f, &v, 4);
    return f;
}
inline uint64_t F32Box(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return 0xffffffff00000000ull | (uint64_t)b;  // RISC-V NaN-boxing
}
inline uint64_t FaddS(uint64_t a, uint64_t b) {
    volatile float x = F32Val(a), y = F32Val(b);
    volatile float r = x + y;  // binary32, 就近偶数, 逐条舍入
    return F32Box(r);
}
inline uint64_t FmulS(uint64_t a, uint64_t b) {
    volatile float x = F32Val(a), y = F32Val(b);
    volatile float r = x * y;  // binary32, 就近偶数, 逐条舍入
    return F32Box(r);
}

}  // namespace
"""


class Gen:
    def __init__(self, so, sym, stack=0x1000):
        self.so = so
        self.sym = sym
        self.stack = stack
        self.lines = []
        self.data = collections.OrderedDict()
        self.ro = b""
        self.ro_lo = 0

    def parse(self):
        body = self.so.bodies.get(self.sym)
        if body is None:
            sys.exit("符号 %s 不在 %s 里" % (self.sym, self.so.path))
        for ln in body:
            m = re.match(r"^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$", ln)
            if not m:
                continue
            addr, mn, ops = int(m.group(1), 16), m.group(2), m.group(3).strip()
            ops = re.sub(r"\s*<[^>]*>.*$", "", ops).strip()
            self.lines.append((addr, mn, ops))
        if not self.lines:
            sys.exit("空函数体")
        # 标量浮点档是否被用到 (决定生成 FP 寄存器文件/辅助函数; 缺省路径逐字节不变):
        # ① 本体里有白名单内的 FP 指令; ② 或有被调者带 float 实参 (FP_ARGS)。
        self.fp_used = any(mn in FP_SUPPORTED for _a, mn, _o in self.lines)
        for _a, mn, ops in self.lines:
            if mn in ("jal", "j") and not self.fp_used:
                nm = self.plt_name(ops) if mn == "jal" else self.tail_name(ops)
                if nm in FP_ARGS:
                    self.fp_used = True

    def check(self):
        bad = []
        for addr, mn, ops in self.lines:
            raw = self.so.raw.get(addr)
            if raw and (raw[0] & 0x7F) in (0x0B, 0x2B, 0x5B, 0x7B):
                bad.append("0x%x %s (TX8 私有指令编码)" % (addr, mn))
            if mn in ("jalr", "jr", "call", "tail", "ebreak", "unimp"):
                bad.append("0x%x %s" % (addr, mn))
            # 标量浮点只放白名单里的 6 条 (改动记录第 6 条); 其余 f*/v* 一律拒绝。
            if (mn[:1] in ("f", "v") and mn not in ("fence", "fence.i")
                    and mn not in FP_SUPPORTED):
                bad.append("0x%x %s" % (addr, mn))
        return bad

    def collect_data(self):
        """把入口用到的**只读常量区**快照成一段连续数组, 并算出每个 auipc 的偏移。

        为什么需要"算 final": 同一个常量在**同体不同名**的两份入口里, 编译器给出的
        `auipc` 基址可以不同 —— 实测 3073x1024 是 `auipc 0x1d` + `addi +0x5a0`,
        3328x1024 是 `auipc 0x14` + `addi -0x482`, **两者的 final 都是 0x42e48**。
        所以窗口必须由"auipc 基址 + 消费它的立即数"的**最终地址**决定, 否则负的
        `addi` 会把指针退到数组前面 (实测 3328x1024 就是这么读错的)。

        窗口 = [min(所有 auipc 基址, 所有 final) - 0x400,
                max(所有 auipc 基址, 所有 final) + 0x800)
        —— 上下都留余量给"变量下标"与"基址本身也是合法指针"这两件事。
        """
        self.auipc = {}
        pend = {}
        addrs = []
        for addr, mn, ops in self.lines:
            parts = [p.strip() for p in ops.split(",")] if ops else []
            if mn == "auipc":
                rd = REGS[parts[0]]
                base = addr + (sext(parse_imm(parts[1]) & 0xFFFFF, 20) << 12)
                if self.so.sec_of(base) is None:
                    sys.exit("0x%x auipc 基址 0x%x 不在任何节里" % (addr, base))
                pend[rd] = base
                self.auipc[addr] = base
                addrs.append(base)
                continue
            if not parts:
                continue
            used = None
            if len(parts) == 3 and parts[1] in REGS and REGS[parts[1]] in pend:
                try:
                    used = (REGS[parts[1]], parse_imm(parts[2]))
                except ValueError:
                    used = (REGS[parts[1]], 0)
            elif len(parts) >= 2:
                mem = self.parse_mem_soft(parts[1])
                if mem is not None and mem[0] in REGS and REGS[mem[0]] in pend:
                    used = (REGS[mem[0]], mem[1])
                elif parts[1] in REGS and REGS[parts[1]] in pend:
                    used = (REGS[parts[1]], 0)
            if used is not None:
                addrs.append(pend[used[0]] + used[1])
                del pend[used[0]]
                continue
            if parts[0] in REGS and REGS[parts[0]] in pend:
                del pend[REGS[parts[0]]]
        if not addrs:
            self.ro_lo = 0
            self.ro = b""
            return {}
        lo = max(0, min(addrs) - 0x400)
        hi = max(addrs) + 0x800
        if hi - lo > 0x20000:
            sys.exit("只读常量区跨度太大: 0x%x..0x%x" % (lo, hi))
        buf = bytearray()
        addr = lo
        while addr < hi:
            sec = self.so.sec_of(addr)
            if sec is None:
                buf.append(0)
                addr += 1
                continue
            name, va, off, sz = sec
            take = min(hi, va + sz) - addr
            buf += self.so.blob[off + (addr - va): off + (addr - va) + take]
            addr += take
        self.ro_lo = lo
        self.ro = bytes(buf)
        return {}

    def slabel(self):
        return "kRo"

    def emit(self, out):
        src = self.so.path.replace("\\", "/").split("/")[-1]
        out.append("// 本文件由 tools/raops_oracle/entry_to_cpp.py 生成, 请勿手改。")
        out.append("// 源: %s 的 %s" % (src, self.sym))
        out.append("//     (= 逐指令机械直译; 每条语句后是厂商地址 + 助记符, 可逐行对照反汇编)")
        out.append("//")
        out.append("// 语义约定 (与 tools/vendor_oracle/disasm_to_cpp.py 的不变量一致):")
        out.append("//   X[32] 一律 uint64_t (XLEN=64); 只有 *w / addiw 截断到 32 位并符号扩展;")
        out.append("//   分支一律 goto L<厂商地址>; auipc 定位的 .rodata 常量表已快照成本文件的数组。")
        if self.fp_used:
            out.append("//   F[32] 是标量浮点寄存器文件 (uint64_t 位容器; `.s` 用低 32 位 + NaN-boxing,")
            out.append("//   `.d` 用满 64 位); 算术走 C 的 float (IEEE-754 binary32, 就近偶数, 逐条舍入)。")
        out.append("#include <cstdint>")
        out.append("#include <cstring>")
        out.append("")
        out.append("extern \"C\" {")
        proto = collections.OrderedDict()
        for addr, mn, ops in self.lines:
            if mn == "jal":
                nm = self.plt_name(ops)
                if nm is None:
                    continue
            elif mn == "j":
                # 尾部调用 (`j <PLT槽>` / `j <本 .so 的局部函数>`): 也是"入口发出的实参",
                # 记录器口径必须算进来。实测 matmul.so: mm_tile_m_split_kn 最后一条是
                # `j 0x114d0` (= wait_cmd_done 的 PLT 槽), 只认 jal 会漏掉它。
                nm = self.tail_name(ops)
                if nm is None:
                    continue
            else:
                continue
            if nm not in ARITY:
                sys.exit("未知被调者 %s 的实参个数 (请补 ARITY)" % nm)
            proto[nm] = ARITY[nm]
        for nm, k in proto.items():
            kfp = FP_ARGS.get(nm, 0)
            ps = ", ".join(["float p%d" % i for i in range(kfp)] +
                           ["uint64_t p%d" % (kfp + i) for i in range(k - kfp)])
            ret = "void" if nm in VOID_RET else "uint64_t"
            weak = " __attribute__((weak));" if nm in WEAK_CALLEES else ";"
            out.append("%s %s(%s)%s" % (ret, nm, ps, weak))
        out.append("}")
        out.append(HELPERS)
        if self.fp_used:
            out.append(FP_HELPERS)
        if self.ro:
            out.append("// 只读常量区快照: vaddr 0x%x..0x%x (%d B); auipc 基址 = 本数组 + 节内偏移"
                       % (self.ro_lo, self.ro_lo + len(self.ro), len(self.ro)))
            out.append("static const unsigned char %s[] = {" % self.slabel())
            for i in range(0, len(self.ro), 16):
                out.append("    " + " ".join("0x%02x," % c for c in self.ro[i:i + 16]))
            out.append("};")
        out.append("")
        eargs = ENTRY_ARGS.get(self.sym)
        ret = "uint64_t" if self.sym in RET_VALUE else "void"
        if eargs:
            out.append("extern \"C\" %s %s(%s) {"
                       % (ret, self.sym, ", ".join("uint64_t " + r for r in eargs)))
        else:
            out.append("extern \"C\" void %s(uint64_t a0) {" % self.sym)
        out.append("  uint64_t X[32];")
        out.append("  for (int i = 0; i < 32; ++i) X[i] = 0;")
        if self.fp_used:
            out.append("  uint64_t F[32];  // 标量浮点寄存器文件 (位容器; 见文件头语义约定)")
            out.append("  for (int i = 0; i < 32; ++i) F[i] = 0;")
        out.append("  alignas(16) unsigned char g_stk[0x%x];  // 模拟栈帧 (--stack)" % self.stack)
        out.append("  std::memset(g_stk, 0, sizeof g_stk);")
        out.append("  X[2] = (uint64_t)(uintptr_t)(g_stk + sizeof g_stk);  // sp = 栈顶")
        if eargs:
            for r in eargs:
                out.append("  X[%d] = %s;%s// 形参 %s" % (REGS[r], r, " " * 40, r))
        else:
            out.append("  X[10] = a0;                                          // a0 = P (入参)")
        # labels
        labels = set()
        for addr, mn, ops in self.lines:
            parts = [p.strip() for p in ops.split(",")] if ops else []
            if mn in BRANCHES and parts:
                try:
                    labels.add(parse_imm(parts[-1]))
                except ValueError:
                    pass
            if mn in ("j", "jal") and parts:
                t = self.jump_addr(ops)
                if t is not None and mn == "j" and self.tail_name(ops) is None:
                    labels.add(t)
        base = self.lines[0][0]
        pend_auipc = {}
        for addr, mn, ops in self.lines:
            if addr in labels:
                out.append("L_%x:" % addr)
            for st in self.stmt(addr, mn, ops, pend_auipc):
                out.append("  " + st + "  /* 0x%x  %s %s */" % (addr, mn, ops))
        out.append("  return X[10];" if self.sym in RET_VALUE else "  return;")
        out.append("}")
        out.append("")

    # ---- helpers ----
    def sym_at(self, addr):
        """目标地址上的**已定义符号名** (含 `t` 局部符号)。

        厂商入口除了 `jal <PLT槽>` 还会直接 `jal <本 .so 的局部函数>`
        (实测: n3d_from_to_g3d_v3 的 3 个入口都 `jal x3d_copy_all` / `jal x3d_copy`)。
        这种调用不经过 PLT, 所以必须按地址回填名字, 否则会误报"未知被调者"。
        """
        if not hasattr(self, "_symmap"):
            self._symmap = {}
            for ln in sh([f"{V}/llvm-readelf", "-s", "-W", self.so.path]).splitlines():
                m = re.match(r"\s*\d+:\s+([0-9a-f]+)\s+\S+\s+(FUNC|IFUNC|NOTYPE)\s+"
                             r"\S+\s+\S+\s+\S+\s+(\S+)", ln)
                if m:
                    a = int(m.group(1), 16)
                    if m.group(2) in ("FUNC", "IFUNC"):
                        self._symmap.setdefault(a, m.group(3))
        return self._symmap.get(addr)

    def plt_name(self, ops):
        try:
            t = parse_imm(ops.split(",")[0])
        except ValueError:
            return None
        nm = self.so.plt.get(t)
        if nm:
            return nm
        return self.sym_at(t)

    def tail_name(self, ops):
        """`j <目标>` 是**尾部调用**吗? 是就返回被调者名, 否则 None.

        厂商入口里既有 `jal <PLT槽>` 也有尾部 `j <PLT槽>` (实测 matmul.so:
        `main_kernel_mm_tile_m_split_kn @0x1a884  j 0x114d0`, 0x114d0 是 wait_cmd_done
        的 PLT 槽)。反汇编在两种情况下都印 `j`, 所以要按**目标地址**区分:
        落在 `.plt` 槽或某个已定义 FUNC 的起始地址 => 尾调用。
        """
        t = self.jump_addr(ops)
        if t is None:
            return None
        nm = self.so.plt.get(t) or self.sym_at(t)
        if nm is None or nm not in ARITY:
            return None
        return nm

    def call_args(self, nm):
        """按被调者真实实参个数取实参 (a0..a7 走寄存器, 其余在模拟栈 sp+0/8/...).

        浮点实参 (FP_ARGS 的 K 个) 走 **fa0..fa(K-1)** —— RISC-V psABI 里浮点与整型
        各自编号, 所以这里前 K 个从 `F[10..]` 取 (按位转成 float), 其余整型仍从
        `X[10..]` 取。生成体与探针记录器两侧同一张表, 观测面才一致。
        """
        k = ARITY[nm]
        kfp = FP_ARGS.get(nm, 0)
        if kfp:
            args = ["F32Val(F[%d])" % (10 + i) for i in range(kfp)]
            nint = k - kfp
            args += ["X[%d]" % (10 + i) for i in range(min(nint, 8))]
            for i in range(max(0, nint - 8)):
                args.append("Ld(X[2] + %d)" % (8 * i))
            return args
        if nm in EXTRA_REGS:
            # 第 9/10 个实参在 t3/t4 (见 EXTRA_REGS 的注释)
            args = ["X[%d]" % (10 + i) for i in range(8)]
            args += ["X[%d]" % REGS[r] for r in EXTRA_REGS[nm]]
            assert len(args) == k, (nm, k, len(args))
            return args
        args = ["X[%d]" % (10 + i) for i in range(min(k, 8))]
        for i in range(k - 8):
            args.append("Ld(X[2] + %d)" % (8 * i))
        return args

    def jump_addr(self, ops):
        try:
            return parse_imm(ops.split(",")[0])
        except ValueError:
            return None

    def stmt(self, addr, mn, ops, pend):
        parts = [p.strip() for p in ops.split(",")] if ops else []
        if mn == "auipc":
            return ["X[%d] = (uint64_t)(uintptr_t)%s + %d;"
                    % (REGS[parts[0]], self.slabel(), self.auipc[addr] - self.ro_lo)]
        if mn == "j":
            nm = self.tail_name(ops)
            if nm is not None:
                args = self.call_args(nm)
                if nm in VOID_RET:
                    return ["%s(%s);" % (nm, ", ".join(args)), "return;"]
                tail = ("return X[10];" if self.sym in RET_VALUE else "return;")
                return ["X[10] = %s(%s);" % (nm, ", ".join(args)), tail]
            return ["goto L_%x;" % self.jump_addr(ops)]
        if mn == "ret":
            # RET_VALUE 的公共辅助: 厂商 `ret` 时返回值在 a0 (= X[10]) 里。
            return ["return X[10];" if self.sym in RET_VALUE else "return;"]
        if mn == "nop":
            return []
        if mn in ("fence", "fence.i"):
            return []
        if mn == "jal":
            nm = self.plt_name(ops)
            if nm is None:
                sys.exit("0x%x jal 目标不是 PLT 槽: %s" % (addr, ops))
            args = self.call_args(nm)
            if nm in VOID_RET:
                return ["%s(%s);" % (nm, ", ".join(args))]
            return ["X[10] = %s(%s);" % (nm, ", ".join(args))]
        if mn == "li":
            return ["X[%d] = (uint64_t)(int64_t)%d;" % (REGS[parts[0]], parse_imm(parts[1]))]
        if mn == "lui":
            rd = REGS[parts[0]]
            v = (parse_imm(parts[1]) & 0xFFFFF) << 12
            return ["X[%d] = (uint64_t)(int64_t)(int32_t)0x%xu;" % (rd, v)]
        if mn == "mv":
            return ["X[%d] = X[%d];" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "not":
            return ["X[%d] = ~X[%d];" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "neg":
            return ["X[%d] = (uint64_t)(0 - X[%d]);" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "negw":
            return ["X[%d] = Sx32((uint64_t)(0 - X[%d]));" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "seqz":
            return ["X[%d] = (X[%d] == 0);" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "snez":
            return ["X[%d] = (X[%d] != 0);" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "sltz":
            return ["X[%d] = ((int64_t)X[%d] < 0);" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "sgtz":
            return ["X[%d] = ((int64_t)X[%d] > 0);" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "zext.b":
            return ["X[%d] = X[%d] & 0xff;" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "zext.h":
            return ["X[%d] = X[%d] & 0xffff;" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "sext.b":
            return ["X[%d] = (uint64_t)(int64_t)(int8_t)X[%d];" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "sext.h":
            return ["X[%d] = (uint64_t)(int64_t)(int16_t)X[%d];" % (REGS[parts[0]], REGS[parts[1]])]
        if mn == "sext.w":
            return ["X[%d] = Sx32(X[%d]);" % (REGS[parts[0]], REGS[parts[1]])]
        # ---- 标量浮点 (2026-10-18 白名单: flw/fsw/fld/fsd/fadd.s/fmul.s) ----
        if mn in FP_LOADS:
            fd = FPREGS.get(parts[0])
            if fd is None:
                sys.exit("0x%x %s 的目标不是 F 寄存器: %s" % (addr, mn, ops))
            base, off = self.parse_mem(parts[1])
            if mn == "flw":
                # flw 只写 4 字节, 高 32 位按 RISC-V NaN-boxing 填 1
                return ["F[%d] = 0xffffffff00000000ull | (uint64_t)FlwBits(%s);"
                        % (fd, self.addr_expr(base, off))]
            return ["F[%d] = Ld(%s);" % (fd, self.addr_expr(base, off))]
        if mn in FP_STORES:
            fs = FPREGS.get(parts[0])
            if fs is None:
                sys.exit("0x%x %s 的源不是 F 寄存器: %s" % (addr, mn, ops))
            base, off = self.parse_mem(parts[1])
            if mn == "fsw":
                return ["FswBits(%s, (uint32_t)F[%d]);"
                        % (self.addr_expr(base, off), fs)]
            return ["Sd(%s, F[%d]);" % (self.addr_expr(base, off), fs)]
        if mn in FP_ARITH:
            fd, f1, f2 = FPREGS.get(parts[0]), FPREGS.get(parts[1]), FPREGS.get(parts[2])
            if None in (fd, f1, f2):
                sys.exit("0x%x %s 的操作数不是 F 寄存器: %s" % (addr, mn, ops))
            return ["F[%d] = %s(F[%d], F[%d]);" % (fd, FP_ARITH[mn], f1, f2)]
        if mn in LOADS:
            rd = REGS[parts[0]]
            base, off = self.parse_mem(parts[1])
            return ["X[%d] = %s(%s);" % (rd, LOADS[mn], self.addr_expr(base, off))]
        if mn in STORES:
            base, off = self.parse_mem(parts[1])
            return ["%s(%s, X[%d]);" % (STORES[mn], self.addr_expr(base, off), REGS[parts[0]])]
        if mn in R3:
            f = R3[mn]
            return ["X[%d] = %s;" % (REGS[parts[0]], f.format(rd="X[%d]" % REGS[parts[0]],
                                                              a="X[%d]" % REGS[parts[1]],
                                                              b="X[%d]" % REGS[parts[2]]))]
        if mn in I3:
            rd = REGS[parts[0]]
            imm = parse_imm(parts[2])
            f = I3[mn]
            return ["X[%d] = %s;" % (rd, f.format(rd="X[%d]" % rd, a="X[%d]" % REGS[parts[1]],
                                                  i="%d" % imm, u="%du" % (imm & 0xFFFFFFFF)))]
        if mn in BRANCHES:
            tgt = self.jump_addr(parts[-1])
            if mn in BR1:
                cond = BR1[mn].format(a="X[%d]" % REGS[parts[0]])
            else:
                cond = BR2[mn].format(a="X[%d]" % REGS[parts[0]], b="X[%d]" % REGS[parts[1]])
            return ["if (%s) goto L_%x;" % (cond, tgt)]
        sys.exit("0x%x 不支持的指令: %s %s" % (addr, mn, ops))

    def parse_mem_soft(self, s):
        m = re.match(r"^(-?0x[0-9a-f]+|-?\d+)?\((.+)\)$", s.strip())
        if not m:
            return None
        return m.group(2).strip(), (parse_imm(m.group(1)) if m.group(1) else 0)

    def parse_mem(self, s):
        m = re.match(r"^(-?0x[0-9a-f]+|-?\d+)?\((.+)\)$", s.strip())
        if not m:
            sys.exit("不认识的访存操作数: %s" % s)
        off = parse_imm(m.group(1)) if m.group(1) else 0
        return m.group(2).strip(), off

    def addr_expr(self, base, off):
        if base == "DATA":
            return "(uint64_t)(uintptr_t)%s" % self.dlabel(int(off))
        r = REGS[base]
        o = parse_imm(off)
        if o == 0:
            return "X[%d]" % r
        return "X[%d] + %d" % (r, o)


LOADS = {"lb": "Lb", "lbu": "Lbu", "lh": "Lh", "lhu": "Lhu", "lw": "Lw", "lwu": "Lwu",
         "ld": "Ld"}
STORES = {"sb": "Sb", "sh": "Sh", "sw": "Sw", "sd": "Sd"}

R3 = {
    "add": "{a} + {b}",
    "sub": "{a} - {b}",
    "and": "{a} & {b}",
    "or": "{a} | {b}",
    "xor": "{a} ^ {b}",
    "sll": "{a} << ({b} & 63)",
    "srl": "{a} >> ({b} & 63)",
    "sra": "(uint64_t)((int64_t){a} >> ({b} & 63))",
    "slt": "(uint64_t)((int64_t){a} < (int64_t){b})",
    "sltu": "({a} < {b})",
    "mul": "{a} * {b}",
    "mulw": "Sx32({a} * {b})",
    "addw": "Sx32({a} + {b})",
    "subw": "Sx32({a} - {b})",
    "sllw": "Sx32((uint64_t)(uint32_t){a} << ({b} & 31))",
    "srlw": "Sx32((uint64_t)((uint32_t){a} >> ({b} & 31)))",
    "sraw": "Sx32((uint64_t)(int32_t)((int32_t){a} >> ({b} & 31)))",
    "divw": "Sx32(::Divw({a}, {b}))",
    "divuw": "Sx32(::Divuw({a}, {b}))",
    "remw": "Sx32(::Remw({a}, {b}))",
    "remuw": "Sx32(::Remuw({a}, {b}))",
    "div": "::Div({a}, {b})",
    "divu": "::Divu({a}, {b})",
    "rem": "::Rem({a}, {b})",
    "remu": "::Remu({a}, {b})",
    "max": "(uint64_t)((int64_t){a} > (int64_t){b} ? (int64_t){a} : (int64_t){b})",
    "min": "(uint64_t)((int64_t){a} < (int64_t){b} ? (int64_t){a} : (int64_t){b})",
    "maxu": "({a} > {b} ? {a} : {b})",
    "minu": "({a} < {b} ? {a} : {b})",
}

I3 = {
    "addi": "{a} + {i}",
    "andi": "{a} & {i}",
    "ori": "{a} | {i}",
    "xori": "{a} ^ {i}",
    "slti": "(uint64_t)((int64_t){a} < (int64_t)({i}))",
    "sltiu": "({a} < (uint64_t)(int64_t)({i}))",
    "slli": "{a} << ({i} & 63)",
    "srli": "{a} >> ({i} & 63)",
    "srai": "(uint64_t)((int64_t){a} >> ({i} & 63))",
    "addiw": "Sx32({a} + {i})",
    "slliw": "Sx32((uint64_t)(uint32_t){a} << ({i} & 31))",
    "srliw": "Sx32((uint64_t)((uint32_t){a} >> ({i} & 31)))",
    "sraiw": "Sx32((uint64_t)(int32_t)((int32_t){a} >> ({i} & 31)))",
}

BRANCHES = {"beq", "bne", "blt", "bge", "bltu", "bgeu", "beqz", "bnez", "blez", "bgez",
            "bltz", "bgtz", "bgt", "bgtu", "ble", "bleu"}
BR1 = {"beqz": "{a} == 0", "bnez": "{a} != 0", "blez": "(int64_t){a} <= 0",
       "bgez": "(int64_t){a} >= 0", "bltz": "(int64_t){a} < 0", "bgtz": "(int64_t){a} > 0"}
BR2 = {"beq": "{a} == {b}", "bne": "{a} != {b}",
       "blt": "(int64_t){a} < (int64_t){b}", "bge": "(int64_t){a} >= (int64_t){b}",
       "bltu": "{a} < {b}", "bgeu": "{a} >= {b}",
       "bgt": "(int64_t){a} > (int64_t){b}", "bgtu": "{a} > {b}",
       "ble": "(int64_t){a} <= (int64_t){b}", "bleu": "{a} <= {b}"}

DIV_HELPERS = r"""
namespace {
// RISC-V 语义: 除零不 trap (div 返回 -1, divu 返回全 1, rem 返回被除数)
inline uint64_t Div(uint64_t a, uint64_t b) {
    if (b == 0) return ~0ull;
    if ((int64_t)a == INT64_MIN && (int64_t)b == -1) return (uint64_t)a;
    return (uint64_t)((int64_t)a / (int64_t)b);
}
inline uint64_t Divu(uint64_t a, uint64_t b) { return b == 0 ? ~0ull : a / b; }
inline uint64_t Rem(uint64_t a, uint64_t b) {
    if (b == 0) return a;
    if ((int64_t)a == INT64_MIN && (int64_t)b == -1) return 0;
    return (uint64_t)((int64_t)a % (int64_t)b);
}
inline uint64_t Remu(uint64_t a, uint64_t b) { return b == 0 ? a : a % b; }
inline uint32_t Divw(uint64_t a, uint64_t b) {
    int32_t x = (int32_t)a, y = (int32_t)b;
    if (y == 0) return 0xffffffffu;
    if (x == INT32_MIN && y == -1) return (uint32_t)x;
    return (uint32_t)(x / y);
}
inline uint32_t Divuw(uint64_t a, uint64_t b) {
    uint32_t y = (uint32_t)b;
    return y == 0 ? 0xffffffffu : (uint32_t)a / y;
}
inline uint32_t Remw(uint64_t a, uint64_t b) {
    int32_t x = (int32_t)a, y = (int32_t)b;
    if (y == 0) return (uint32_t)x;
    if (x == INT32_MIN && y == -1) return 0;
    return (uint32_t)(x % y);
}
inline uint32_t Remuw(uint64_t a, uint64_t b) {
    uint32_t y = (uint32_t)b;
    return y == 0 ? (uint32_t)a : (uint32_t)a % y;
}
}  // namespace

"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("so")
    ap.add_argument("symbol")
    ap.add_argument("--out")
    ap.add_argument("--stack", type=lambda x: int(x, 0), default=0x1000,
                    help="模拟栈帧字节数 (入口在自己的栈上开 VLA 时要放大; 默认 0x1000)")
    args = ap.parse_args()
    so = So(args.so)
    g = Gen(so, args.symbol, args.stack)
    g.parse()
    bad = g.check()
    if bad:
        sys.exit("不支持: " + "; ".join(bad[:8]))
    left = g.collect_data()
    if left:
        sys.exit("有 auipc 没被解析成常量数据基址: %s" % {k: hex(v) for k, v in left.items()})
    out = []
    g.emit(out)
    text = "\n".join(out) + "\n"
    text = text.replace("namespace {\n\ninline uint64_t Ld", DIV_HELPERS.strip() + "\n\nnamespace {\n\ninline uint64_t Ld", 1)
    if args.out:
        open(args.out, "w").write(text)
        print("wrote %s (%d lines, %d data bases, %d insns)"
              % (args.out, text.count("\n"), len(g.data), len(g.lines)))
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
