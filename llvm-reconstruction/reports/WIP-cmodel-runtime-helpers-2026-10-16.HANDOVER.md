# HANDOVER — cmodel 设备运行时助手线 (2026-10-16 中断)
分支 `dev/runtime-helpers` (base de94b99, 未提交) · worktree `wt-runtime` · BUILD_DIR `build-runtime`
**完整补丁**: `patches/01-runtime-helpers-COMPLETE.patch` (14 文件 = 8 改 + 6 新, 1375 行, md5 `187603dce4d3aee45070bf61bf9e930b`, 已 `git apply --check` 对 HEAD 通过) ·
父线的残补丁 `patches/00-parent-WIP-4newfiles-ONLY.patch` (只有 4 个新文件, **不要用**).
## ① 符号 / 证据种类 / 状态
| 符号 | 证据种类 | 状态 |
|---|---|---|
| `get_tile_num_per_chip` | **数值差分**: 厂商 addcdiv.so:0x18cd2 机器码在 qemu 下真跑 (die_num×tile_num 54 格) | 完成, 测试绿 |
| `get_prev_tile_id` | **数值差分 + printf 字节差分 + 调用次数**: 厂商 0x18f0e, tile_num 0..8 × tile_id 网格 + 5 条诊断行逐字节 | 完成, 测试绿 |
| `get_die_tile_id` | **数值差分**: 厂商 0x1910c, divuw/remuw 含除 0 语义 | 完成, 测试绿 |
| `printf` (设备侧) | **字节差分**: 上面那 5 条诊断行由本库 printf 打出来, 与厂商设备 printf 逐字节一致 | 完成 |
| `krt_get_tile_id` / `_die_id` / `_chip_id` | **行为差分 (厂商 cmodel)**: 见 ③ 的真值表; 另有库级符号表核对 | 代码完成, **未注册差分测试** |
| `krt_get_die_num_per_chip` / `krt_get_tile_num_per_die` | 同上 (取值随 PG 配置, 见真值表) | 代码完成, **未注册差分测试** |
| `krt_barrier_in_chip` | **行为契约 (厂商 cmodel)**: 满 chip 才放行, 不足则死等 (见 ③) | 代码完成, **未注册测试** |
一个测试覆盖前 4 行: `test/test_device_runtime_tile_topology.cpp` → **761 checks / 0 failures**;
变异自检 2 条(环表项改 0 → 红; not-found 判据 off-by-one → 红/abort), 复原后 md5
`54cd24ac52c24c4fa3aab0d2f5f78441` 且基线复绿。全库 `--target tx82_cmodel` 在改动后构建 **rc=0**;
`nm -D` 里 12 个符号**各恰好 1 个裸 C 定义**, `_Z*` mangle 残留 0 个, `w printf` 变 `W printf`。
**未做**: 改动后的**全量 ctest 没跑过**(中断时按要求停机) —— 这是恢复后第一件事。

## ② 取证 / 复现命令
```bash
# 一条命令复现 (3 个 get_* 的 761 项差分)
bash tools/raops_oracle/gen_runtime_tile_topology_ref.sh      # 真值 md5 应为 3056ce4f87e37563119729b93fcea143
/tmp/pycmake/cmake/data/bin/cmake -S . -B ../build-runtime && \
/tmp/pycmake/cmake/data/bin/cmake --build ../build-runtime -j2 --target test_device_runtime_tile_topology
../build-runtime/bin/test_device_runtime_tile_topology          # 末行 0 failures
# 全量验收 (恢复后)
/tmp/pycmake/cmake/data/bin/cmake --build ../build-runtime -j2 --target tx82_cmodel && \
BUILD_DIR=$PWD/../build-runtime /tmp/pycmake/cmake/data/bin/ctest --test-dir ../build-runtime -j2 --timeout 300
```
- **`krt_*` 的行为 oracle (现成, 已跑过)**: `docx/tests/llvm/multicore-dispatch` 那套 (device ELF + `txLaunchKernelSimd`)。
  我把它扩成会打印 `krt_get_die_num_per_chip/tile_num_per_die` 并逐 tile 放置, 用 `setSpikePGMode`
  跑 4 个 PG 模式, 厂商 cmodel 实测 **(全局 start → die/tile, dnpc/tnpd)**:
  `PG_1DIE_8TILE(4): start 0..7 → (0,0..7), 1/8` · `PG_2DIE_8TILE(2): start 0..7 → (0,0..7), start 8..15 → (1,0..7), 2/8`
  · `PG_4DIE_4TILE(0): 4/4` · `PG_4DIE_6TILE(1): 4/6` · chip 恒 0 (单 device)。
  ⇒ **`krt_get_tile_id` 是 die 内下标**(旧实现返回全局下标, 已修); **die_num/tile_num 随 PG 配置变**(旧实现硬编码 4/8, 已修)。
  重建命令: 设备 ELF 用 `clang --target=riscv64-unknown-linux-musl --sysroot=<tc>/sysroot -mcpu=tsm-x82
  -mabi=lp64f -O2 -fPIC -nostdlib -shared -fuse-ld=lld -Wl,-Bsymbolic -I<cmodel>/include/boards/riscv/andes/include/krt_interface`;
  宿主 `g++ -std=c++17 -I<cmodel>/include/include <host>.cpp -L<cmodel>/lib -lraisa_sim -Wl,-rpath,<cmodel>/lib`。
- **`krt_barrier_in_chip` 的行为契约 (厂商 cmodel 实测)**: PG_2DIE_8TILE 下只起 8 个 tile → 该栅栏**死等**(超时);
  起满 16 个 → 16 个全返回; PG_4DIE_4TILE 起 4 个 → 死等。⇒ 参与数 = **整个 chip** 的 tile(dies×tiles_per_die),
  **不是** die 栅障、也不支持部分启动。另: 栅栏**不给**全局内存可见性顺序(满 16 时各 tile 栅栏后读到的 peer 标志只有 1..16)——
  我们的 mutex+condvar 顺序更强, 依赖竞态读的探针不能两边对拍。
- `tools/re/read_symbols.py <kernel.so> <sym>` = 厂商工具链反汇编(**认识 TX8 私有指令**)+ `jal <plt>` 解析;
  两条预读结论已并入实现: `get_tile_num_per_chip = krt_get_die_num_per_chip × krt_get_tile_num_per_die`(mulw);
  `get_prev_tile_id` = 编译期环表查表 + 环绕。

## ③ 未做 / 卡在哪 (恢复后的最小清单)
1. **跑全量 ctest**(`-j2`, `Skipped` 必须 0) —— 改动碰了库的 tile 语义, 必须先证明无回归。
2. **给 `krt_*` 一族 + `krt_barrier_in_chip` 注册差分测试**(当前只有手工观测, 按 §1 不算验收):
   把 ② 的实测真值 dump 成 `.inc`、写成 `test_device_runtime_krt`(编入 `src/core/cmodel_zephyr.cpp`,
   用 `dispatchToCluster` 记录每 tile 的 `krt_*` 值, 与 `.inc` 逐值比; 栅栏用 N/2→N 两段线程验证"不到齐不放行")。
   脚本名建议 `tools/raops_oracle/gen_runtime_krt_ref.sh`, 宿主/设备探针源码见 ② 的编译行 (我写好过, 被清理掉了)。
3. `get_prev_tile_id` 的环表 = **`.rodata.cst32`**: addcdiv.so vaddr `0x20f08`(file off `0x11f08`, 32B),
   内容 `4 2 0 1 3 5 6 7`; 表基址来自 `0x18f32 auipc a0,0x8; 0x18f36 addi s2,a0,-0x2a`。**表只有 8 项**, tile_num>8 时
   厂商越界读相邻 `.rodata`(实测出 13/14)→ 本实现**响亮 abort**(已实测该路径)。
4. 未实现的同族缺口(不在本轮清单, 建议下一轮): `get_next_tile_id` @0x18cee (同表, 下标 +1),
   `get_prev/next_global_tile_id`, `krt_barrier_in_die` / `krt_barrier_simd`(厂商 kernel 里也是 U)。
5. `krt_get_chip_id` 只在单 device 下被实测(chip=0); 多 device 的映射(器件下标)未差分验证。
