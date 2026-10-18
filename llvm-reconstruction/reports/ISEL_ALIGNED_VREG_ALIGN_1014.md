# E / ISel-pseudo line — final report: `*_ALIGNED_*` + `vreg-align` (patches 1014–1016)

Tree: `wt-merge-e` (derived from `wt-merge` at 03:40, never written into `wt-merge`).
Patches: `wt-merge-e/patches/1014-*`, `1015-*`, `1016-*` (stack replay reproduces
`wt-merge-e/src` byte-for-byte, 10/10 files; `patch -p1` against the merged baseline).

## 1. Result — the 9 blocking cells are byte-identical

```
python3 wt-merge-e/tools/raisa_oracle.py \
  --cc /home/shaokai/Desktop/work/tx82/wt-merge-e/build/bin/clang-21 \
  --cells mem_l1_size_1,mem_l1_size_3,mem_l1_size_127,mem_l1_size_128,\
mem_l1_size_129,mem_l1_size_256,mem_l1shared_u32_uniform,\
mem_l0ashared_u32_uniform,mem_l0wshared_u32_uniform --jobs 2
```
`18 identical, 0 diff, 0 ours-fail` — **9/9 cells `asm:IDENTICAL obj:IDENTICAL`**.

`mir_ab.sh` (fixed, see §5) on the same 9 cells: **`diff lines: 0` for 9/9**, both
sides non-empty (31 lines), i.e. the *machine* pipeline of both compilers produces
the same output from the same (vendor) `-stop-before=riscv-insert-vsetvli` MIR.

## 2. Root cause (measured, not guessed)

The pre-`riscv-insert-vsetvli` MIR differs in exactly two things, and **two**
vendor behaviours had to be reconstructed:

### 2.1 Every constant AVL is an *immediate* on the RAISA device path (patch 1014, part A)

Vendor evidence (same source text, same `-O2`):

| compiler/target | `PseudoVID_V_M1` AVL | `PseudoVLE16_V_MF2` (fadd_rn_bf16) |
|---|---|---|
| vendor, `riscv64-unknown-linux-musl -mcpu=tsm-x82` | `%2:gprnox0` + `ADDI $x0,128` | same |
| ours, same musl flags | identical to vendor | identical |
| vendor, `riscv64-unknown-raisa` | **immediate `128`** | **immediate `128`** |
| ours, raisa (before) | `%2:gprnox0` (GPR) | `%2:gprnox0` |

⇒ the fork keeps a constant AVL as a `TargetConstant` operand when the triple is
RAISA.  `HasVendorXTsm82s` is useless here (true for musl too); the gate is
`TM.getTargetTriple().isRaisa()` in `RISCVISelDAGToDAG.cpp::selectVLOp`.  Before
the fix our materialised `ADDI $x0,128` was live across the prologue, was
scheduled before `vid` and forced a different vsetvli/vreg colouring
(`li a1,128` hoisted; vendor creates the `ADDI` later inside
`riscv-insert-vsetvli`).

### 2.2 The fork has a pre-RA `vreg-align` pass + a `*_ALIGNED_*` pseudo family (patch 1014, part B)

The earlier "`vreg-align` is a MIR no-op" note is **wrong** for these cells:

```
vendor -mllvm -print-after=twoaddressinstruction : PseudoVSLH_VX_M2   ... (no ALIGNED)
vendor -mllvm -print-after=machine-scheduler     : PseudoVSLH_ALIGNED_VX_M2 ...
```

Vendor implementation, recovered from its own objects:

* `RISCVTargetMachine.cpp.o::RISCVPassConfig::addPreISel()` (relocations + disassembly)
  → `insertPass(&TwoAddressInstructionPassID, &TX8RegisterAlignmentPassID)`, gated on
  `-riscv-vreg-align` (default true);
* `-mllvm -debug-pass=Structure` order:
  `… -twoaddressinstruction -vreg-align -slotindexes -liveintervals -register-coalescer -rename-independent-subregs -machine-scheduler …`;
* semantics, from a standalone replay (`vendor llc -run-pass=vreg-align` on the
  vendor's own `-stop-before=vreg-align` MIR of `mem_l1_size_1`):
  `early-clobber %6:vrm2 = PseudoVSLH_VX_M2 undef %6, killed %4, killed %5, 128,5,0`
  ⇒ `undef %14.sub_vrm1_0:vrm2 = COPY killed %4` + `PseudoVSLH_ALIGNED_VX_M2 undef %6, killed %14, …`
  (the `*_ALIGNED_*` twin's `$rs2` is one LMUL step wider, so the pass inserts a
  sub-register `COPY` that `register-coalescer` later folds into the defining
  instruction);
* condition (`IsCTInstrRequiresAlignment` + `InstrAlignMap`, decompiled in
  `tools/re/out/TX8RegisterAlignment/decompiled.c`): type value 7 for the
  `XTsm_VSLH_*` base opcodes, and `getLMul(TSFlags) - 1 < 3`, i.e. only
  `LMUL_2/4/8` (the sub-register constants 6/14/18 = `sub_vrm1_0`/`sub_vrm2_0`/
  `sub_vrm4_0` in both trees match the decompiled `alignInstrVReg`).

Our reconstruction (`TX8RegisterAlignment.cpp`, ~170 lines) reproduces that MIR
shape exactly:

```
undef %14.sub_vrm1_0:vrm2 = COPY killed %4:vr
early-clobber %6:vrm2 = PseudoXTsm_VSLH_ALIGNED_VX_M2 undef %6:vrm2(tied-def 0), killed %14:vrm2, killed %5:gpr, 128, 5, 0
```

Name set: the generated `PseudoVSLH_ALIGNED_*` names normalise
(`PseudoXTsm_` → `Pseudo`) to **24/24 = exactly the vendor's**
(`strings -a llvm-toolchain/lib/libLLVMRISCVDesc.a | grep -E '^PseudoVSLH_ALIGNED'`),
`diff` empty.  A corpus-wide census of the vendor's own MIR (114 cells,
`logs/census/aligned_usage.tsv`) shows the vendor selects **only**
`PseudoVSLH_ALIGNED_VX_M2` (21 cells) and `PseudoVSLH_ALIGNED_VV_M2`
(`control_uniform_loop`) — both are in our 24-name set, so the corpus' ALIGNED
usage is fully covered.  The remaining 1184 of the vendor's 1208 `*_ALIGNED_*`
names belong to op families no corpus cell triggers (mechanism is generic; the
TD multiclass can be instantiated for them).

`PseudoToVInst` needed `["_ALIGNED",""]` in `AffixSubsts`
(`RISCVInstrInfoVPseudos.td`) so the twin maps back to `XTsm_VSLH_VV/VX`
(verified in `RISCVGenSearchableTables.inc`:
`{ PseudoXTsm_VSLH_ALIGNED_VX_M2, XTsm_VSLH_VX, … }`).

## 3. Secondary item: `implicit $mem_alloc` on the `_SYNC_` ops (patch 1015)

The vendor marks only the *upstream* RVV memory ops; its own indexed ops carry
nothing (verified on two cells, `mem_l1_size_1`, `alloc_l1shared_base`):

```
PseudoVSUXEI64_SYNC_V_M2_M1 …, -1, 5 :: (store unknown-size)          (no mem_alloc)
PseudoVLUXEI64_SYNC_V_M2_M1 …, -1, 5, 2 :: (load unknown-size)        (no mem_alloc)
PseudoVSE32_V_M1 …, -1, 5, implicit $mem_alloc :: (store …)           (has it)
```

`TX8InsertXFRM.cpp::isVectorMemOp` now excludes the four
`XTsm_V{L,S}UXEI{32,64}_SYNC_V` base opcodes.  Effect measured: our MIR matches
the vendor's, and the patch is **provably inert** — the 317-cell parity TSV and
the 114-cell oracle verdicts are unchanged (only evidence paths differ), musl
ours-vs-ours stays 56/56.

## 4. Gates (all after the final binary)

| gate | baseline (merge) | now |
|---|---|---|
| `.rc` oracle `--all` (114 cells) | 158 slots / 79 cells IDENTICAL, 0 diff, 48 ours-fail | **176 slots / 88 cells IDENTICAL**, 3 diff, 27 ours-fail, 0 non-det |
| 317-cell parity | 208 / OURS-GAP 24 / byte-diff 18 | **byte-identical 217**, OURS-GAP 12, byte-diff 21, `.c` gap 0, 0 regressions |
| musl vendor-vs-ours (`.comment`-stripped) | 24/56 = 28/56 outcomes | **24/56 = 28/56 outcomes** (unchanged) |
| musl ours-vs-ours (`clang-21.pre1014` vs final) | — | **56/56 outcomes, 0 real differences** |
| stale-object sweep (`tools/sweep_stale_objects.sh`) | — | **0 stale** |
| `mir_ab.sh` (fixed) on the 9 target cells | — | **9/9 `diff lines: 0`** |

Parity transitions (12 cells, all forward): the 9 target cells `OURS-GAP →
byte-identical`; `mem_{l1,l0a,l0w}shared_u64_uniform` `OURS-GAP → byte-diff`
(see §5).

## 5. `mir_ab.sh` (fixed) and the extra lever

`tools/mir_ab.sh` was silently comparing two empty files (A4's finding).  Fixed:
`sed -E` instead of BRE; the name map is now applied to the **input MIR** as well
as to both asm products; both products are asserted non-empty with plausible
line counts (a failure is reported as `HARNESS FAILED`, never as `0 diff`); and
the `_SYNC_` operands are swapped back
(`tools/mir_sync_swap.py`), because this tree's indexed pseudos record
`(vs2, index, base)` while the vendor/upstream record `(vs2, base, index)` — a
registered reconstruction difference, compensated in `RISCVISelDAGToDAG.cpp`.
Method check: the vendor `llc` replay reproduces the vendor `clang` asm modulo
the `.L…$local` label/type/size lines the MIR does not carry.

**Extra lever (patch 1016, `.vi`/`.wi` suppression).** The fork never folds an
immediate into `_VI`: all 237 vendor `.s` files of the round-2 corpus contain
**zero** `.vi`/`.wi` instructions, while our side emitted them in 3 cells
(`PseudoVADD_VI_M1` vs the vendor's `li`+`PseudoVADD_VX_M1`), and `-emit-obj`
died in the MC layer (`Unsupported RVV instruction … funct3=0b011`).  The
`_VI` pattern classes are now gated with a new triple predicate
(`IsNotRaisaDevice`, defined in `RISCVInstrInfoVPseudos.td`; TableGen predicates
cannot use `HasVendorXTsm82s`, which is true for musl as well).  Result:
`mem_{l1,l0a,l0w}shared_u64_uniform` move from `obj:OURS-FAIL` to `obj:DIFF`
(their objects now encode); 0 cells flip to identical, and all gates are
unchanged (oracle 176 identical, parity 217/12/21, musl 56/56 and 24/56).  Their
residual is **one physical register**: the pre-`riscv-insert-vsetvli` MIR of
both compilers is *identical* (`$v8m2` for the second `vslh`), but the vendor's
final asm has `vslh.vx v2, v10, a2` while ours keeps `v8` — i.e. a **post-RA**
difference (the vendor's `vreg-rename` moves it), not ISel.  `control_divergent`
is unchanged (`asm:OURS-FAIL`; it dies earlier in
`RISCVISelLowering.cpp:14629`, so it is not an `.vi` problem end-to-end).

## 6. Honest limits

* Only the `VSLH` sub-family of the vendor's 1208 `*_ALIGNED_*` names is
  instantiated (24/24 exact).  The corpus never selects another one (§2).
* The `_SYNC_` operand-order difference (index/base) remains in the MIR text;
  the asm/obj are identical, and the harness compensates (§5).
* The 3 `*_u64_uniform` cells are one post-RA register away (§5); their
  remaining cause lives in `vreg-rename`/post-RA territory, not in this ISel
  layer.
* bf16/twin-table behaviour and the vendor's `-riscv-tx8-sched=true` branch were
  not touched.

## 7. Reproduce

```bash
cd /home/shaokai/Desktop/work/tx82
bash wt-merge-e/tools/build_e.sh "<edited srcs>" "<new objs>"   # derived-build driver
python3 wt-merge-e/tools/raisa_oracle.py --cc "$PWD/wt-merge-e/build/bin/clang-21" --all --jobs 2
python3 wt-merge-e/tools/corpus_parity.py collect --cc "$PWD/wt-merge-e/build/bin/clang-21" --tag e1016 --jobs 2
python3 wt-merge-e/tools/round1_attrib.py --vendor "$PWD/wt-merge/parity/vendor.json" \
        --ours "$PWD/wt-merge-e/parity/e1016.json" --out "$PWD/wt-merge-e/parity/e1016.tsv"
python3 wt-merge-e/tools/musl_norm.py --a "$PWD/llvm-toolchain/bin/clang" --b "$PWD/wt-merge-e/build/bin/clang-21"
python3 wt-merge-e/tools/musl_ab.py --a "$PWD/wt-merge-e/logs/bin/clang-21.pre1014" --b "$PWD/wt-merge-e/build/bin/clang-21"
for c in mem_l1_size_1 mem_l1_size_3 mem_l1_size_127 mem_l1_size_128 mem_l1_size_129 \
         mem_l1_size_256 mem_l1shared_u32_uniform mem_l0ashared_u32_uniform mem_l0wshared_u32_uniform; do
  bash wt-merge-e/tools/mir_ab.sh "$c"; done
```

Logs: `wt-merge-e/logs/{oracle_all_1016,parity_attrib_1016,musl_prepost_1016,musl_vendor_1016}.log`,
`logs/mir/`, `logs/census/aligned_usage.tsv`, `logs/mir_ab/`.
Pre-1014 binary kept at `wt-merge-e/logs/bin/clang-21.pre1014` for A/B.

## 8. Process note (self-reported)

`tools/mir_ab.sh` originally wrote its scratch to `$WT/recon/mir_ab`, and
`$WT/recon` was (temporarily) a symlink to the `wt-merge` line's `recon/` dir, so
my first nine `mir_ab` runs wrote a scratch subdirectory
(`wt-merge/recon/mir_ab/`, 73 files, all with this harness's own file names)
inside the other line's tree.  That directory has been resolved-and-verified and
**removed**; the harness now writes `wt-merge-e/logs/mir_ab/` (72 files, 9 cells,
all `diff lines: 0` re-verified after the fix), `round1_attrib.py` reads the
shared recon/vendor data by absolute path, and both symlinks are gone.  No
source, build or parity file of `wt-merge` was touched (verified with
`find wt-merge/parity wt-merge/recon -newermt … -type f` = empty).
