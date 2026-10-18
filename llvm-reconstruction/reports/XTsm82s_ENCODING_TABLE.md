# XTsm82s encoding table — human-readable summary

Machine-readable table: [`xtsm82s_opcodes.tsv`](xtsm82s_opcodes.tsv) (432 rows + header).
Remap derivation and validation: [`RVV_TO_0x7B_REMAP.md`](RVV_TO_0x7B_REMAP.md).

## Sources and how much each contributes

| source | what it is | rows witnessed |
| --- | --- | --- |
| `td` | `RISCVInstrInfoXTsm82s.td` (399 defs / 10 aliases, 2489 `Inst{}` assignments) | 399 |
| `census` | `wt-forks/out/census_full.tsv` — 371 black-box 0x7B words | 327 |
| `asmenc` | `wt-forks/out/tx8_mnem_enc.txt` — vendor `-show-encoding` | 46 |
| `npu_opcode` | `cmodel/.../npu_opcode.h` — 339 `MATCH_`/`MASK_` pairs | 348 |
| `recov` | `recovered-docs/xtsm82s_isa.tsv` (397 rows, NOT authoritative) | 397 |
| `vdis` | vendor `llvm-mc -disassemble -mcpu=tsm-x82` round-trip | 423 |
| `vbin` | `RVV2CTFunctMap` extracted read-only from the vendor binary | 111 |

## Row census

| class | rows | meaning |
| --- | --- | --- |
| `remapped-RVV` | 172 | standard RVV mnemonic, re-encoded into `0x7B` |
| `fork-only` | 216 | TX8-proprietary mnemonic, exists only in `0x7B` |
| `vendor-cannot-reassemble` | 44 | vendor *disassembles* it, vendor *assembler* refuses it |

`agree`: **yes 423 / no 9 / partial 0**.

Independent witnesses (`.td` excluded): 1=14 rows, 2=88 rows, 3=8 rows, 4=172 rows, 5=150 rows.

Three-way agreement `td`+`census`+`npu_opcode`: **312 rows**; four-way with `recov`: 310;
five-way with the vendor disassembler: 310.

## The remap rules in prose

1. **Opcode.** `bits[6:0] := 0x7B` (custom-3), for every remapped instruction.
   `0x57` (OP-V/OP-FP), `0x07` (LOAD-FP) and `0x27` (STORE-FP) all land in
   `0x7B`.
2. **Function code.** `bits[31:26]` and `bits[14:12]` are replaced by TX8's
   own codes (`wt-forkenc/rvv2ct_funct_map.txt`, 103 entries). This is a
   lookup, not a formula. 23 of the 150 rows change *only* the opcode (all the
   `OPFVV` float arithmetic and the compares); the other 127 also change
   funct6/funct3.
3. **Operands.** `vd`/`rd` (`11:7`), `vs1`/`rs1` (`19:15`), `vs2`/`rs2`
   (`24:20`) and the immediate payload are copied unchanged, with exactly one
   exception: `vmv.v.v` moves its source from `19:15` to `24:20`.
4. **`vm` (bit 25).** Copied unchanged, except where the `.td` freezes it
   (`vmv.x.s`: forced to 0) and except for the families whose `.td` fixes
   `bits[31:25]` as one 7-bit field (`vmadc`/`vsbc`/`vmsbc`/`vmadc.vvm`,
   `vmand.mm`-family, …).
5. **Loads/stores.** `funct3` becomes `011` (load) or `100` (store); the RVV
   `nf`/`mew`/`mop`/`width` split is replaced by TX8 `funct6`, while the
   `lumop`/`sumop` field in `bits[24:20]` survives (that is what distinguishes
   `vle16.v` from `vl1re16.v`).

Validated: 150/150 remapped rows reproduced bit-exactly, exit status 0, and
the binary's own `RVV2CTFunctMap` agrees with the `.td` on all 111 rows it
covers (0 mismatches).

## TX8-proprietary mnemonics (`opcode = 0x7B`, none of these exist in stock RVV)

Representative encodings from the vendor assembler (`tx8_mnem_enc.txt`); the
full 216-row `fork-only` set is in the TSV. `f3`=bits14:12, `f6`=bits31:26.

| mnemonic | operands | bits (LE) | word | f3 | f6 |
| --- | --- | --- | --- | --- | --- |
| `vsetsimt` | `1` | `0x7b,0xf0,0x00,0xc0` | `0xc000f07b` | 7 | `0x30` |
| `vsetbarrier` | `` | `0x7b,0x70,0x00,0x82` | `0x8200707b` | 7 | `0x20` |
| `isetexec` | `sr0` | `0x7b,0x70,0x08,0xbe` | `0xbe08707b` | 7 | `0x2f` |
| `movexecz` | `a1` | `0xfb,0x15,0x00,0xf6` | `0xf60015fb` | 1 | `0x3d` |
| `send_sync` | `zero, zero, zero` | `0x7b,0x00,0x00,0x02` | `0x0200007b` | 0 | `0x00` |
| `wait_sync` | `zero, zero, zero` | `0x7b,0x00,0x00,0x0a` | `0x0a00007b` | 0 | `0x02` |
| `send` | `a0, zero, a1` | `0x7b,0x85,0x05,0xca` | `0xca05857b` | 0 | `0x32` |
| `wait_send` | `a0, zero, a1` | `0x7b,0x85,0x05,0xd2` | `0xd205857b` | 0 | `0x34` |
| `recv` | `a0, zero, a1` | `0x7b,0x85,0x05,0xda` | `0xda05857b` | 0 | `0x36` |
| `direct_write` | `a0, zero, a1` | `0x7b,0x85,0x05,0xea` | `0xea05857b` | 0 | `0x3a` |
| `kcore_ncc_read` | `a1, a1` | `0xfb,0x05,0xb0,0x42` | `0x42b005fb` | 0 | `0x10` |
| `gemm` | `a0, zero, a1` | `0x7b,0x85,0x05,0x46` | `0x4605857b` | 0 | `0x11` |
| `gemm_psum` | `a0, zero, a1` | `0x7b,0x85,0x05,0x4e` | `0x4e05857b` | 0 | `0x13` |
| `conv` | `a0, zero, a1` | `0x7b,0x85,0x05,0x66` | `0x6605857b` | 0 | `0x19` |
| `mma` | `a0, zero, a1` | `0x7b,0x85,0x05,0xa6` | `0xa605857b` | 0 | `0x29` |
| `mma_first` | `a0, zero, a1` | `0x7b,0x85,0x05,0x96` | `0x9605857b` | 0 | `0x25` |
| `mma_last` | `a0, zero, a1` | `0x7b,0x85,0x05,0xae` | `0xae05857b` | 0 | `0x2b` |
| `ne_set_prec` | `a0, zero, a1` | `0x7b,0x85,0x05,0x06` | `0x0605857b` | 0 | `0x01` |
| `l0_flush` | `a0, 0, a1` | `0x7b,0xe5,0x05,0x10` | `0x1005e57b` | 6 | `0x04` |
| `l0_invalid` | `a0, 0, a1` | `0x7b,0xe5,0x05,0x00` | `0x0005e57b` | 6 | `0x00` |
| `l1_sw_prefetch` | `a0, 0, a1` | `0x7b,0xe5,0x05,0xa0` | `0xa005e57b` | 6 | `0x28` |
| `lsu_tdma_mem_copy` | `a0, a1` | `0x7b,0x85,0x25,0x04` | `0x0425857b` | 0 | `0x01` |
| `lsu_tdma_nchw2nhwc` | `a0, a1` | `0x7b,0x85,0x15,0xc4` | `0xc415857b` | 0 | `0x31` |
| `lsu_rdma_gather` | `a0, a1` | `0x7b,0x85,0x05,0x04` | `0x0405857b` | 0 | `0x01` |
| `dte_gather` | `a0, zero, a1` | `0x7b,0x85,0x05,0x80` | `0x8005857b` | 0 | `0x20` |
| `set_mem_alloc` | `t6` | `0x7b,0x70,0xf0,0xb9` | `0xb9f0707b` | 7 | `0x2e` |
| `mv128.s.x` | `sr0, a0, a1` | `0x7b,0xa0,0xa5,0x3a` | `0x3aa5a07b` | 2 | `0x0e` |
| `and.ss` | `sr0, sr0, sr0` | `0x7b,0x20,0x00,0x82` | `0x8200207b` | 2 | `0x20` |
| `or.ss` | `sr0, sr0, sr0` | `0x7b,0x20,0x00,0x86` | `0x8600207b` | 2 | `0x21` |
| `xor.ss` | `sr0, sr0, sr0` | `0x7b,0x20,0x00,0x8a` | `0x8a00207b` | 2 | `0x22` |
| `nand.ss` | `sr0, sr0, sr0` | `0x7b,0x20,0x00,0x96` | `0x9600207b` | 2 | `0x25` |
| `vl2de32.v` | `v10, (a1)` | `0x7b,0xb5,0x05,0x1e` | `0x1e05b57b` | 3 | `0x07` |
| `vs2de32.v` | `v10, (a1)` | `0x7b,0xc5,0x05,0x1e` | `0x1e05c57b` | 4 | `0x07` |
| `vl2de16.v` | `v10, (a1)` | `0x7b,0xb5,0x05,0x0e` | `0x0e05b57b` | 3 | `0x03` |
| `vluxei32.sync.v` | `v10, (a1), v0` | `0x7b,0xb5,0x05,0x26` | `0x2605b57b` | 3 | `0x09` |
| `vsuxei32.sync.v` | `v10, (a1), v0` | `0x7b,0xc5,0x05,0x26` | `0x2605c57b` | 4 | `0x09` |
| `vbit2fp.m` | `v10, v11` | `0x7b,0x25,0xb0,0x36` | `0x36b0257b` | 2 | `0x0d` |
| `vreverse.v` | `v10, v11` | `0x7b,0x25,0xb0,0xda` | `0xdab0257b` | 2 | `0x36` |
| `vslh.vv` | `v10, v9, v8` | `0x7b,0x15,0x94,0xda` | `0xda94157b` | 1 | `0x36` |
| `vslh.vx` | `v10, v8, a0` | `0x7b,0x55,0x85,0xda` | `0xda85557b` | 5 | `0x36` |
| `vfsquare.v` | `v10, v11` | `0x7b,0x25,0xb0,0x02` | `0x02b0257b` | 2 | `0x00` |
| `vpow2.v` | `v10, v11` | `0x7b,0x25,0xb0,0x22` | `0x22b0257b` | 2 | `0x08` |
| `vlog2.v` | `v10, v11` | `0x7b,0x15,0xb4,0x4e` | `0x4eb4157b` | 1 | `0x13` |
| `vsin.v` | `v10, v11` | `0x7b,0x95,0xb4,0x4e` | `0x4eb4957b` | 1 | `0x13` |
| `vcos.v` | `v10, v11` | `0x7b,0x15,0xb5,0x4e` | `0x4eb5157b` | 1 | `0x13` |
| `sort_sort` | `a0, zero, a1` | `0x7b,0x85,0x05,0x28` | `0x2805857b` | 0 | `0x0a` |

Operand shapes in the `.td`: `$vd, $vs2, $vs1` (SR `.ss`), `$rd, $rs2, $rs1`
(NE/MMA/conv/DMA/sort/send-recv/cache), `$rd, $rs1` (LSU/RDMA/WDMA), `$rd, $imm,
$rs1` (l0/l1 flush-invalidate-prefetch), `$imm`, `$rd`, `$vd, $rs1` (2D +
whole-register), `$vd, $vs2` (`vmvNr.v`), `$vd, $vs2$vm` / `$vd, $rs1, $rs2$vm`.

## Confirmed discrepancies

### A. `npu_opcode.h` patterns the vendor toolchain does not implement (9)

31 `npu_opcode.h` patterns have no `.td`/census counterpart; each was probed
with `-mcpu=tsm-x82 -disassemble` under 10 `vm`/operand permutations (310
probes, `wt-forkenc/npu_variant_probe.py` → `variant_probe.txt`). 22 are
genuinely implemented; these **9 have 0/10 accepted variants**:

| npu name | pattern (match/mask) | why it cannot be genuine |
| --- | --- | --- |
| `l0.exclusive.invalid` | `0x4000607b`/`0xfe00707f` | no `.td`/census instruction at `f6=0x00,f3=6` |
| `l1.exclusivev.invalid` | `0xc000607b`/`0xfe00707f` | no `.td`/census instruction at `f6=0x30,f3=6` |
| `vmfge.bv.vv` | `0x7c00107b`/`0xfc00707f` | RVV has no `vmfge.vv`; `f6=0x1f,f3=1` is unassigned in TX8 |
| `vmfgt.bv.vv` | `0x7400107b`/`0xfc00707f` | RVV has no `vmfgt.vv`; `f6=0x1d,f3=1` is unassigned in TX8 |
| `vmsgt.u.vv` | `0xa400107b`/`0xfc00707f` | RVV has no `vmsgtu.vv`; TX8 `f6=0x29,f3=1` is unassigned |
| `vmsgt.vv` | `0xa000107b`/`0xfc00707f` | RVV has no `vmsgt.vv` (only `.vx`); TX8 `f6=0x28,f3=1` is unassigned |
| `vsetvl.keepvl` | `0x9400707b`/`0xfe00707f` | same |
| `vsetvl.ns` | `0x9000707b`/`0xfe00707f` | the fork models only `vsetvli` (`.td`) — no `vsetvl` def |
| `vsetvl.vlmax` | `0x9200707b`/`0xfe00707f` | same |

### B. Fork self-inconsistency (assembler vs disassembler vs .td)

* **11 mnemonics make the vendor assembler `abort()`**: `vfmacc.vf`,
  `vfmacc.vv`, `vfmadd.vf`, `vfmadd.vv`, `vfnmacc.vf`, `vfmsac.vf`,
  `vfmv.f.s`, `vfredosum.vs`, `vfwadd.vv`, `vfwredosum.vs`, `vfdiv.vv`
  (`LLVM ERROR: Unsupported RVV instruction …`). This is the cause of the
  reproducible `-O2` backend crash in `RISC-V Assembly Printer`. Reproduce:
  `echo 'vfmacc.vf v10, fa0, v8' | llvm-toolchain/bin/llvm-mc -triple=riscv64
  -mcpu=tsm-x82 -show-encoding`.
* **`vnsrl.wi` only accepts shift amount 0**: `vnsrl.wi v10, v11, 5` aborts
  because the `.td` folds `bits[19:12]` into a constant.
* **44 mnemonics the vendor disassembler prints but the vendor assembler
  refuses** (`class = vendor-cannot-reassemble`): the `vl*/vs*` segment and
  fault-first families (`vle16ff`, `vle32ff`, `vlseg3/5/7e16/32[ff]`,
  `vlsseg*`, `vluxseg*`, `vsseg*`, `vssseg*`, `vsuxseg*`). Their `0x7B` words
  exist in the decoder table but there is **no way to emit them** from
  assembly; the compiler never produces them either.
* **TableGen operand order bug**: `XTsm_VMERGE_VVM` prints
  `"$vd, $vs1, $vs2, v0"` while the vendor assembler's syntax is
  `vmerge.vvm vd, vs2, vs1, v0`. Same for `XTsm_VMERGE_VVM_P1`,
  `XTsm_VMERGE_VXM` and the other `.vvm`/`.vxm` defs. Anything that
  round-trips through the `.td` asm string will print operands swapped.
* **`census_full.tsv` has a 6-name header over 7 columns** (the `operands`
  column is unlabelled); parse it positionally.

### C. The recovered-docs examples are decode-only artefacts

`xtsm82s_isa.tsv`'s `example_encoding` was produced by `.4byte` + `objdump`
with all-zero operands, so it often lands on the `_P1` *decode-only* alternate
rather than the assembler-canonical word:

| mnemonic | recovered-docs example | vendor assembler emits |
| --- | --- | --- |
| `vs4r.v` | `0x8280407b` | `0x9280407b` (`vs4r.v v0, (zero)`) |
| `vcpop.m` | `0x4008207b` (vs1=16) | `0x42b8257b` |

Also, `_P1` defs are *shadowed*: e.g. `XTsm_VS1R_V_P1` (`0x0280407b`)
decodes as `vs1r.v` only while `rs1=0`; with `rs1=11` the same pattern
(`0x0285857b`) is decoded as `send_sync`, whose `.td` mask is a superset.
A decoder must resolve `0x02?0?07b` in favour of `send_sync`.

## Do **not** trust

| source | verdict |
| --- | --- |
| `gemini-cmodel/docs/TX82_RISCV_OFFICIAL_ISA_MANUAL.md` | **Discredited.** Claims 184 "official" entries with a `funct7` column that contradicts the real encodings (the real instructions use `funct6`+`vm` in `31:25`, not a `funct7`). Do not use it for anything. |
| `docs/compiler/fork-source/recovered-docs/*` | Cross-check only. Its `example_encoding` is a `.4byte`+`objdump` artefact (see §C), `base_mattr_decoding` is `<unknown>` for all 397 rows, and its `fields.tsv` register masks include whole-register fields that are not really operand slots. Never the sole authority for a bit. |
| `cmodel/.../npu_opcode.h` | Good for `MATCH_`/`MASK_` of the ~339 entries the fork also has, but it also declares 9 patterns the fork does not implement (§A) and uses its own names (`vid`, `vsetvli.vlmax`, `vcvt.fp32tobf16`). Name-matching against mnemonics fails; match by mask. |
| `wt-forks/out/census_full.tsv` | Reliable words, unreliable *shape*: the `operands` column is not always the tuple encoded in the same row, and 44 rows carry `-` for the 0x7B word. It is a *disassembler* census: a word decoding is not proof it can be assembled. |
| `RISCVInstrInfoXTsm82s.td` | Authoritative for bits and the only source for the ~24 mnemonics nothing else has, but it is auto-generated from the recovery, has the `vmerge` operand-order bug (§B), one `vnsrl.wi` constant folded over the immediate, and 18 `_P1` decode-only defs. |

## Reproduce everything

```
cd wt-forkenc
python3 parse_td.py td.json            # .td -> 399 defs / 2489 Inst{} assignments
python3 build_sources.py sources.json  # +census +npu +recovered +1 stock llvm-mc pass
python3 remap_validate.py > remap_validate.out ; echo $?   # 0 = 150/150
python3 rvv2ct_table.py >/dev/null; python3 npu_variant_probe.py  # binary map + probe
python3 build_table.py; python3 gen_reports.py   # -> the TSV + both markdown files
```
