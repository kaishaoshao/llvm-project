# RVV → opcode `0x7B` remap — bit-level rules and mechanical validation

**Status:** derived and validated. `150/150` `remapped-RVV` census rows are
reproduced bit-exactly by the rule set below; see
`wt-forkenc/remap_validate.py` and its output `wt-forkenc/remap_validate.out`.

## 1. What actually changes

With `+xtsm82s` (i.e. `-mcpu=tsm-x82`) the fork re-encodes the whole RVV
instruction space into the RISC-V *custom-3* opcode `0x7B`. Nothing in the
`0x57`/`0x07`/`0x27` space is removed: the vendor assembler still accepts the
stock words (the census column `vendor_base_0x57` equals the stock encoding
for all 194 RVV-class rows), and in `+v`-only mode the fork emits only the
stock form. So the cmodel decoder must accept **both** encodings, and must
not assume a TX8 `0x7B` word is 'RVV with a patched opcode'.

The fork implements the conversion in the binary, not in TableGen. The
vendor `llvm-mc` (a non-stripped build) contains:

```
$ nm -C llvm-toolchain/bin/llvm-mc | grep -i 'tx8\|convertrvv'
00000000000b3270 t convertRVVALUToCTEncoding(unsigned int)
00000000000b3e10 t llvm::TX8MCInfo::tryConvertRVVToCTEncoding(unsigned int)
00000000000b4090 t llvm::TX8MCInfo::tryConvertCTToRVVEncoding(unsigned int)
0000000000695f40 0000000000000380 r RVV2CTFunctMap
0000000000c14a20 0000000000000380 b CT2RVVFunctMap
```

`llvm::TX8MCInfo::tryConvertRVVToCTEncoding` dispatches on the low 7 bits:
`0x57` → `convertRVVALUToCTEncoding`, `0x07` → load path, `0x27` → store path,
and returns the input word unchanged for everything else. Disassembling
`convertRVVALUToCTEncoding` gives the ALU rule verbatim:

```asm
; index = funct3 + 7*funct6      (funct6 = word>>26, funct3 = bits[14:12])
lea    0x5e2cc7(%rip),%r10          # RVV2CTFunctMap
movzbl (%r10,%rax,2),%ecx           # byte0 = new funct3 (0 = no entry)
movzbl 0x1(%r10,%rax,2),%eax        # byte1 = new funct6
test   %cl,%cl ; je  <unsupported RVV instruction>
and    $0x7,%ecx  ; shl $0xc,%ecx   # new funct3
and    $0x3f,%eax ; shl $0x1a,%eax  # new funct6
and    $0x3ff8fff,%esi ; or %ecx,%eax ; or %esi,%eax   # esi = (w&~0x7f)|0x7b
```

i.e. exactly *rule-A + rule-B* below and nothing else: bit 25 (`vm`), the
three 5-bit operand slots and the immediate/`vtypei` fields are copied
through untouched. The fork's own table has **103 non-zero entries** and is
reproduced verbatim in `wt-forkenc/rvv2ct_funct_map.txt` (extracted
read-only from offset `0x695f40`, size `0x380`).

## 2. The rule set

Applied to a stock RVV word `S` (32 bits) in this order:

| rule | operation | rows |
| --- | --- | --- |
| **A** | `bits[6:0] := 0x7B`. Everything else copied from `S`. | 23 |
| **B** | rule-A, then `bits[31:26] := funct6'` and `bits[14:12] := funct3'` from the TX8 function-code table (§3). | 116 |
| **C** | rule-B, then `bit[25] := vm'` because the `.td` freezes the `vm` bit for that instruction (`vmv.x.s`: `1→0`). | 1 |
| **D** | rule-C, then any *further* `.td`-fixed bits outside `6:0 / 14:12 / 31:26 / 25` are written — the `lumop`/`sumop` field `bits[24:20]` of the whole-register load/store family and `bits[19:15]` of the float-convert / `vnsrl.wi` family. | 10 |
| **E** | operand-slot relocation: the RVV encoding keeps an operand in a different 5-bit slot than the `.td` does. Derived mechanically (perturb each operand, ask the stock assembler which bits move). Exactly **one** instruction is affected — `vmv.v.v`, whose single source register moves from `bits[19:15]` (RVV) to `bits[24:20]` (TX8); that row is counted under **D** above because it also changes `funct6`/`funct3`. | 1 |

*No* rule ever rewrites `vd`/`rd` (`bits[11:7]`), `vs1`/`rs1` (`bits[19:15]`),
`vs2`/`rs2` (`bits[24:20]`) or the immediate payload — except for the single
rule-E case — and no rule ever adds, removes or reorders instructions.

### The function-code table is *not* derivable from the RVV funct6/funct3

It is tempting to look for a bit-level arithmetic relation between the RVV
function code and the TX8 one. There is none: the map is not affine over
GF(2) (e.g. `f6 00→03`, `02→0e`, `04→2b`, `05→2a`, `06→2d`, `07→2c` is not
linear) and it is not a per-funct3 permutation either (`f6=0x25,f3=0` →
`f6=0x16,f3=1` for `vsll.vv` but `f6=0x25,f3=2` → `f6=0x2e,f3=1` for
`vmul.vv`). TX8 has its own function-code numbering and **a cmodel decoder
must carry the table, not a formula.** The table is reproduced in full in §3
and in `wt-forkenc/rvv2ct_funct_map.txt`.

A second consequence: the TX8 function code is what tells the decoder the
operand *shape*, because `funct3` becomes a small class code:

| TX8 funct3 | meaning (observed) |
| --- | --- |
| `001` | vector-vector (`.vv`), also reductions and `.mm` mask logic |
| `010` | `...vs` / `.vvm` second-source forms (SR operand or mask form) |
| `101` | vector-scalar (`.vx`, `.vs` from a GPR, `.vi`) |
| `011` | vector load |
| `100` | vector store |
| `111` | `vsetvli`/`vsetivli` family |

and `opcode` is always `0x7B`, i.e. `000 000 000 000 0000 0001 111 1011`.
Bit `bit[1:0]=11` (32-bit instruction) and `bit[4:2]=110` are constant.

## 3. The TX8 function-code table (`RVV2CTFunctMap`)

`index = funct3 + 7*funct6` of the **stock RVV** word (only opcode `0x57`
takes this path). A `-` means the fork has no entry and raises
`LLVM ERROR: Unsupported RVV instruction with funct6=… funct3=… vs1=… vs2=… vm=…`.

| RVV funct6 | f3=0 | f3=1 | f3=2 | f3=3 | f3=4 | f3=5 | f3=6 | f3=7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `00` | `03/1` | `00/1` | - | - | `03/5` | - | - | - |
| `01` | - | `01/1` | - | - | - | - | - | - |
| `02` | `0e/1` | `02/1` | - | - | `0e/5` | - | - | - |
| `03` | - | - | - | - | `3c/5` | - | - | - |
| `04` | `2b/1` | `04/1` | - | - | `2b/5` | - | - | - |
| `05` | `2a/1` | `05/1` | - | - | `2a/5` | - | - | - |
| `06` | `2d/1` | `06/1` | - | - | `2d/5` | - | - | - |
| `07` | `2c/1` | `07/1` | - | - | `2c/5` | - | - | - |
| `08` | - | `08/1` | - | - | - | - | - | - |
| `09` | `11/1` | `09/1` | - | - | `11/5` | - | - | - |
| `0a` | `12/1` | `0a/1` | - | - | `12/5` | - | - | - |
| `0b` | `15/1` | - | - | - | `15/5` | - | - | - |
| `0c` | `3b/1` | - | - | `38/5` | `0c/5` | - | - | - |
| `0e` | `3a/1` | - | - | `2f/2` | `2e/2` | - | `32/2` | - |
| `0f` | - | - | - | `2b/2` | `35/2` | - | `33/2` | - |
| `10` | `0b/1` | - | - | - | `0b/5` | - | - | - |
| `11` | `0d/1` | - | - | - | `0d/5` | - | - | - |
| `12` | `0f/1` | - | - | - | `0f/5` | - | - | - |
| `13` | `10/1` | - | - | - | `10/5` | - | - | - |
| `14` | - | - | `14/1` | - | - | - | - | - |
| `17` | - | - | `37/2` | - | - | - | - | - |
| `18` | `1e/1` | `18/1` | `38/2` | - | `1e/5` | - | - | - |
| `19` | `21/1` | `19/1` | `39/2` | - | `21/5` | - | - | - |
| `1a` | `23/1` | - | `3a/2` | - | `23/5` | - | - | - |
| `1b` | `22/1` | `1b/1` | `3b/2` | - | `22/5` | - | - | - |
| `1c` | `26/1` | `1c/1` | `3c/2` | - | `26/5` | - | - | - |
| `1d` | `25/1` | `1d/1` | `3d/2` | - | `25/5` | - | - | - |
| `1e` | `29/1` | - | `3e/2` | - | `29/5` | - | - | - |
| `1f` | `28/1` | `1f/1` | `3f/2` | - | `28/5` | - | - | - |
| `20` | - | - | `33/1` | - | - | - | `33/5` | - |
| `21` | - | - | `32/1` | - | - | - | `32/5` | - |
| `22` | - | - | `35/1` | - | - | - | `35/5` | - |
| `23` | - | - | `34/1` | - | - | - | `34/5` | - |
| `24` | - | `24/1` | `30/1` | - | - | - | `30/5` | - |
| `25` | `16/1` | - | `2e/1` | - | `16/5` | - | `2e/5` | - |
| `26` | - | - | `31/1` | - | - | - | `31/5` | - |
| `27` | - | - | `2f/1` | `27/1` | - | - | `2f/5` | - |
| `28` | `17/1` | - | - | - | `17/5` | - | - | - |
| `29` | `1a/1` | - | - | - | `1a/5` | - | - | - |

Entries for `funct3=7` never reach this table (the dispatcher sends them to
the `vsetvli` path); `f6=0x10` (`vcpop.m`/`vfirst.m`/`vmv.x.s`/`vmv.s.x`)
and `f6=0x2c` are handled by special-case code in the same function, and the
vector load/store families by the `0x07`/`0x27` paths.

## 4. Mechanical validation

```
$ cd wt-forkenc
$ python3 build_sources.py sources.json     # ingest + one stock llvm-mc pass
$ python3 remap_validate.py > remap_validate.out ; echo $?
0
$ tail -5 remap_validate.out
rows=150  match=150  mismatch=0  no-rule=0  no-stock=0
rule histogram: rule-A=23, rule-B=116, rule-C=1, rule-D=10
RESULT: 100% match -- 150/150 remapped-RVV rows reproduced exactly.
```

Rule source: the `.td` only (`XTsm82sInst` fixed bits). Stock side: the stock
LLVM 21.1.0 baseline, `wt-forks/stock-bin/llvm-mc -triple=riscv64
 -mattr=+v,+zbb,+zfh,+zve32f,+zve64x,+zvl64b`. Expected side: the census 0x7B
word. Exit status is 0 only when all 150 rows match, so the claim is
re-checkable by a single command.

### Rule-E derivation (also mechanical)

The unmoved-operand claim is not assumed. Every operand of every census
assembly text is perturbed to flip all five of its register-index bits, and
the stock assembler reports which instruction bits move; that measured RVV
slot is compared with the field the `.td` assigns to the same operand.
Result:

```
#          ecall markers keep rejected probes aligned)
#   operand probes: 405, measured: 392, unmeasurable: 13
#   vfabs.v        XTsm_VFSGNJX_VV (alias) AMBIGUOUS: unmatched RVV slots [[15, 16, 17, 18, 19, 20, 21, 22, 23, 24]], free .td vars ['vm', 'vs1', 'vs2']
#   vfneg.v        XTsm_VFSGNJN_VV (alias) AMBIGUOUS: unmatched RVV slots [[15, 16, 17, 18, 19, 20, 21, 22, 23, 24]], free .td vars ['vm', 'vs1', 'vs2']
#   vmmv.m         XTsm_VMAND_MM (alias) AMBIGUOUS: unmatched RVV slots [[15, 16, 17, 18, 19, 20, 21, 22, 23, 24]], free .td vars ['vs1', 'vs2']
#   vmnot.m        XTsm_VMNAND_MM (alias) AMBIGUOUS: unmatched RVV slots [[15, 16, 17, 18, 19, 20, 21, 22, 23, 24]], free .td vars ['vs1', 'vs2']
#   vmv.v.v        XTsm_VMV_V_V         operand#1: RVV bits [15, 16, 17, 18, 19] -> .td vs2 bits [20, 21, 22, 23, 24]
#   vnsrl.wi       XTsm_VNSRL_WI        AMBIGUOUS: unmatched RVV slots [[15, 16]], free .td vars ['vm']
#   vrgather.vi    XTsm_VRGATHER_VI     AMBIGUOUS: unmatched RVV slots [[15, 17]], free .td vars ['imm', 'vm']
#   vsetvli        XTsm_VSETVLI         AMBIGUOUS: unmatched RVV slots [[20, 22, 23, 24, 25, 26, 27, 28, 29]], free .td vars ['vtypei']
#   vslidedown.vi  XTsm_VSLIDEDOWN_VI   AMBIGUOUS: unmatched RVV slots [[15, 17]], free .td vars ['imm', 'vm']
#   vslideup.vi    XTsm_VSLIDEUP_VI     AMBIGUOUS: unmatched RVV slots [[15, 17]], free .td vars ['imm', 'vm']
#   unmeasurable operand probes (stock assembler rejected the variant;
#   no RVV slot could be measured for these):
#     vadc.vvm               operand#3 v0       -> vadc.vvm v10, v0, v11, v31
#     vadc.vxm               operand#3 v0       -> vadc.vxm v10, v0, a1, v31
#     vl2re16.v              operand#0 v10      -> vl2re16.v v21, (a1)
#     vl2re32.v              operand#0 v10      -> vl2re32.v v21, (a1)
#     vmadc.vvm              operand#3 v0       -> vmadc.vvm v10, v0, v11, v31
#     vmadc.vxm              operand#3 v0       -> vmadc.vxm v10, v0, a1, v31
#     vmerge.vvm             operand#3 v0       -> vmerge.vvm v10, v0, v11, v31
#     vmerge.vxm             operand#3 v0       -> vmerge.vxm v10, v0, a1, v31
#     vmsbc.vvm              operand#3 v0       -> vmsbc.vvm v10, v0, v11, v31
#     vmsbc.vxm              operand#3 v0       -> vmsbc.vxm v10, v0, a1, v31
#     vs2r.v                 operand#0 v10      -> vs2r.v v21, (a1)
#     vsbc.vvm               operand#3 v0       -> vsbc.vvm v10, v0, v11, v31
#     vsbc.vxm               operand#3 v0       -> vsbc.vxm v10, v0, a1, v31
```

The residual `AMBIGUOUS` entries are all *not* relocations:
* `vfabs.v`, `vfneg.v`, `vmmv.m`, `vmnot.m` — an `InstAlias` whose single
  source operand feeds **two** `.td` fields (`vs2` and `vs1`), so the measured
  slot spans 10 bits and cannot equal one 5-bit field.
* `vsetvli` — the measured slot is the 11-bit `vtypei` field,
  `bits[30:20]`; partial perturbation only moves part of it.
* `vrgather.vi`, `vslidedown.vi`, `vslideup.vi`, `vnsrl.wi` — the `.td` has
  folded the RVV immediate slot `bits[19:15]` into a *constant* field
  (`vnsrl.wi`: `bits[19:12]=0b10100010`), so there is no free `.td` variable
  to relocate into. This is a real property: TX8 `vnsrl.wi` only has an
  encoding for shift amount 0 — the vendor assembler **aborts** on
  `vnsrl.wi v10, v11, 5` (`LLVM ERROR: Unsupported RVV instruction with
  funct6=0b101100 funct3=0b011 vs1=0b00101 vs2=0b01011 vm=0b1`).

### The 13 operand probes that could not be measured

12 are the *implicit mask operand* `v0` (printed by the disassembler but not
encoded) and 1 (`vl2re16.v`/`vl2re32.v`/`vs2r.v`) is a whole-register group
whose 10 is not group-aligned, so the stock assembler rejects the perturbed
text. Neither can hide a relocation.

## 5. Every `remapped-RVV` row: stock RVV word vs TX8 `0x7B` word

`stock` = `wt-forks/stock-bin/llvm-mc -show-encoding`; `0x7B` = census
`vendor_xtsm82s` column; `pred` = the rule set applied to `stock`.

| mnemonic | stock (`0x57`/`0x07`/`0x27`) | TX8 `0x7B` | rule | `.td` def | pred==0x7B |
| --- | --- | --- | --- | --- | --- |
| `vadc.vvm` | `40058557` | `2c05957b` | B | `XTsm_VADC_VVM` | yes |
| `vadc.vxm` | `4005c557` | `2c05d57b` | B | `XTsm_VADC_VXM` | yes |
| `vadd.vv` | `03f58557` | `0ff5957b` | B | `XTsm_VADD_VV` | yes |
| `vadd.vx` | `03f5c557` | `0ff5d57b` | B | `XTsm_VADD_VX` | yes |
| `vand.vv` | `27f58557` | `47f5957b` | B | `XTsm_VAND_VV` | yes |
| `vand.vx` | `27f5c557` | `47f5d57b` | B | `XTsm_VAND_VX` | yes |
| `vcompress.vm` | `5ff5a557` | `dff5a57b` | B | `XTsm_VCOMPRESS_VM` | yes |
| `vcpop.m` | `42b82557` | `42b8257b` | A | `XTsm_VCPOP_M` | yes |
| `vdiv.vv` | `87f5a557` | `cbf5957b` | B | `XTsm_VDIV_VV` | yes |
| `vdiv.vx` | `87f5e557` | `cbf5d57b` | B | `XTsm_VDIV_VX` | yes |
| `vdivu.vv` | `83f5a557` | `cff5957b` | B | `XTsm_VDIVU_VV` | yes |
| `vdivu.vx` | `83f5e557` | `cff5d57b` | B | `XTsm_VDIVU_VX` | yes |
| `vfabs.v` | `2ab59557` | `2ab5957b` | A | `XTsm_VFSGNJX_VV (alias)` | yes |
| `vfadd.vv` | `03f59557` | `03f5957b` | A | `XTsm_VFADD_VV` | yes |
| `vfcvt.f.x.v` | `4ab19557` | `a2b4257b` | D | `XTsm_VFCVT_F_X_V` | yes |
| `vfcvt.f.xu.v` | `4ab11557` | `a2bba57b` | D | `XTsm_VFCVT_F_XU_V` | yes |
| `vfcvt.x.f.v` | `4ab09557` | `a2b4a57b` | D | `XTsm_VFCVT_X_F_V` | yes |
| `vfcvt.xu.f.v` | `4ab01557` | `a2bc257b` | D | `XTsm_VFCVT_XU_F_V` | yes |
| `vfirst.m` | `42b8a557` | `42b8a57b` | A | `XTsm_VFIRST_M` | yes |
| `vfmax.vv` | `1bf59557` | `1bf5957b` | A | `XTsm_VFMAX_VV` | yes |
| `vfmin.vv` | `13f59557` | `13f5957b` | A | `XTsm_VFMIN_VV` | yes |
| `vfmul.vv` | `93f59557` | `93f5957b` | A | `XTsm_VFMUL_VV` | yes |
| `vfncvt.f.f.w` | `4aba1557` | `a2b0a57b` | D | `XTsm_VFNCVT_F_F_W` | yes |
| `vfncvt.x.f.w` | `4ab89557` | `a2b2a57b` | D | `XTsm_VFNCVT_X_F_W` | yes |
| `vfneg.v` | `26b59557` | `26b5957b` | A | `XTsm_VFSGNJN_VV (alias)` | yes |
| `vfrec7.v` | `4eb29557` | `4eb2957b` | A | `XTsm_VFREC7_V` | yes |
| `vfredmax.vs` | `1ff59557` | `1ff5957b` | A | `XTsm_VFREDMAX_VS` | yes |
| `vfredmin.vs` | `17f59557` | `17f5957b` | A | `XTsm_VFREDMIN_VS` | yes |
| `vfredusum.vs` | `07f59557` | `07f5957b` | A | `XTsm_VFREDUSUM_VS` | yes |
| `vfrsqrt7.v` | `4eb21557` | `4eb2157b` | A | `XTsm_VFRSQRT7_V` | yes |
| `vfsgnj.vv` | `23f59557` | `23f5957b` | A | `XTsm_VFSGNJ_VV` | yes |
| `vfsgnjn.vv` | `27f59557` | `27f5957b` | A | `XTsm_VFSGNJN_VV` | yes |
| `vfsgnjx.vv` | `2bf59557` | `2bf5957b` | A | `XTsm_VFSGNJX_VV` | yes |
| `vfsqrt.v` | `4eb01557` | `4eb0157b` | A | `XTsm_VFSQRT_V` | yes |
| `vfsub.vv` | `0bf59557` | `0bf5957b` | A | `XTsm_VFSUB_VV` | yes |
| `vfwcvt.f.f.v` | `4ab61557` | `a2b0257b` | D | `XTsm_VFWCVT_F_F_V` | yes |
| `vfwcvt.f.x.v` | `4ab59557` | `a2b2257b` | D | `XTsm_VFWCVT_F_X_V` | yes |
| `viota.m` | `52b82557` | `52b8157b` | B | `XTsm_VIOTA_M` | yes |
| `vl1re16.v` | `0285d507` | `0285b57b` | B | `XTsm_VL1RE16_V` | yes |
| `vl1re32.v` | `0285e507` | `1285b57b` | B | `XTsm_VL1RE32_V` | yes |
| `vl2re16.v` | `2285d507` | `4285b57b` | B | `XTsm_VL2RE16_V` | yes |
| `vl2re32.v` | `2285e507` | `5285b57b` | B | `XTsm_VL2RE32_V` | yes |
| `vle16.v` | `0205d507` | `0205b57b` | B | `XTsm_VLE16_V` | yes |
| `vle32.v` | `0205e507` | `1205b57b` | B | `XTsm_VLE32_V` | yes |
| `vlse16.v` | `0bf5d507` | `0bf5b57b` | B | `XTsm_VLSE16_V` | yes |
| `vlse32.v` | `0bf5e507` | `1bf5b57b` | B | `XTsm_VLSE32_V` | yes |
| `vluxei32.v` | `07f5e507` | `07f5b57b` | B | `XTsm_VLUXEI32_V` | yes |
| `vluxei64.v` | `07f5f507` | `17f5b57b` | B | `XTsm_VLUXEI64_V` | yes |
| `vmadc.vv` | `47f58557` | `37f5957b` | B | `XTsm_VMADC_VV` | yes |
| `vmadc.vvm` | `44058557` | `3405957b` | B | `XTsm_VMADC_VVM` | yes |
| `vmadc.vx` | `47f5c557` | `37f5d57b` | B | `XTsm_VMADC_VX` | yes |
| `vmadc.vxm` | `4405c557` | `3405d57b` | B | `XTsm_VMADC_VXM` | yes |
| `vmand.mm` | `67f5a557` | `e7f5a57b` | B | `XTsm_VMAND_MM` | yes |
| `vmandn.mm` | `63f5a557` | `e3f5a57b` | B | `XTsm_VMANDN_MM` | yes |
| `vmax.vv` | `1ff58557` | `b3f5957b` | B | `XTsm_VMAX_VV` | yes |
| `vmax.vx` | `1ff5c557` | `b3f5d57b` | B | `XTsm_VMAX_VX` | yes |
| `vmaxu.vv` | `1bf58557` | `b7f5957b` | B | `XTsm_VMAXU_VV` | yes |
| `vmaxu.vx` | `1bf5c557` | `b7f5d57b` | B | `XTsm_VMAXU_VX` | yes |
| `vmerge.vvm` | `5c058557` | `dc05957b` | B | `XTsm_VMERGE_VVM` | yes |
| `vmerge.vxm` | `5c05c557` | `5005d57b` | B | `XTsm_VMERGE_VXM` | yes |
| `vmfeq.vv` | `63f59557` | `63f5957b` | A | `XTsm_VMFEQ_VV` | yes |
| `vmfle.vv` | `67f59557` | `67f5957b` | A | `XTsm_VMFLE_VV` | yes |
| `vmflt.vv` | `6ff59557` | `6ff5957b` | A | `XTsm_VMFLT_VV` | yes |
| `vmfne.vv` | `73f59557` | `73f5957b` | A | `XTsm_VMFNE_VV` | yes |
| `vmin.vv` | `17f58557` | `abf5957b` | B | `XTsm_VMIN_VV` | yes |
| `vmin.vx` | `17f5c557` | `abf5d57b` | B | `XTsm_VMIN_VX` | yes |
| `vminu.vv` | `13f58557` | `aff5957b` | B | `XTsm_VMINU_VV` | yes |
| `vminu.vx` | `13f5c557` | `aff5d57b` | B | `XTsm_VMINU_VX` | yes |
| `vmmv.m` | `66b5a557` | `e6b5a57b` | B | `XTsm_VMAND_MM (alias)` | yes |
| `vmnand.mm` | `77f5a557` | `f7f5a57b` | B | `XTsm_VMNAND_MM` | yes |
| `vmnor.mm` | `7bf5a557` | `fbf5a57b` | B | `XTsm_VMNOR_MM` | yes |
| `vmnot.m` | `76b5a557` | `f6b5a57b` | B | `XTsm_VMNAND_MM (alias)` | yes |
| `vmor.mm` | `6bf5a557` | `ebf5a57b` | B | `XTsm_VMOR_MM` | yes |
| `vmorn.mm` | `73f5a557` | `f3f5a57b` | B | `XTsm_VMORN_MM` | yes |
| `vmsbc.vv` | `4ff58557` | `43f5957b` | B | `XTsm_VMSBC_VV` | yes |
| `vmsbc.vvm` | `4c058557` | `4005957b` | B | `XTsm_VMSBC_VVM` | yes |
| `vmsbc.vx` | `4ff5c557` | `43f5d57b` | B | `XTsm_VMSBC_VX` | yes |
| `vmsbc.vxm` | `4c05c557` | `4005d57b` | B | `XTsm_VMSBC_VXM` | yes |
| `vmsbf.m` | `52b0a557` | `52b0957b` | B | `XTsm_VMSBF_M` | yes |
| `vmseq.vv` | `63f58557` | `7bf5957b` | B | `XTsm_VMSEQ_VV` | yes |
| `vmseq.vx` | `63f5c557` | `7bf5d57b` | B | `XTsm_VMSEQ_VX` | yes |
| `vmsgt.vx` | `7ff5c557` | `a3f5d57b` | B | `XTsm_VMSGT_VX` | yes |
| `vmsgtu.vx` | `7bf5c557` | `a7f5d57b` | B | `XTsm_VMSGTU_VX` | yes |
| `vmsif.m` | `52b1a557` | `52b1957b` | B | `XTsm_VMSIF_M` | yes |
| `vmsle.vv` | `77f58557` | `97f5957b` | B | `XTsm_VMSLE_VV` | yes |
| `vmsle.vx` | `77f5c557` | `97f5d57b` | B | `XTsm_VMSLE_VX` | yes |
| `vmsleu.vv` | `73f58557` | `9bf5957b` | B | `XTsm_VMSLEU_VV` | yes |
| `vmsleu.vx` | `73f5c557` | `9bf5d57b` | B | `XTsm_VMSLEU_VX` | yes |
| `vmslt.vv` | `6ff58557` | `8bf5957b` | B | `XTsm_VMSLT_VV` | yes |
| `vmslt.vx` | `6ff5c557` | `8bf5d57b` | B | `XTsm_VMSLT_VX` | yes |
| `vmsltu.vv` | `6bf58557` | `8ff5957b` | B | `XTsm_VMSLTU_VV` | yes |
| `vmsltu.vx` | `6bf5c557` | `8ff5d57b` | B | `XTsm_VMSLTU_VX` | yes |
| `vmsne.vv` | `67f58557` | `87f5957b` | B | `XTsm_VMSNE_VV` | yes |
| `vmsne.vx` | `67f5c557` | `87f5d57b` | B | `XTsm_VMSNE_VX` | yes |
| `vmsof.m` | `52b12557` | `52b1157b` | B | `XTsm_VMSOF_M` | yes |
| `vmul.vv` | `97f5a557` | `bbf5957b` | B | `XTsm_VMUL_VV` | yes |
| `vmul.vx` | `97f5e557` | `bbf5d57b` | B | `XTsm_VMUL_VX` | yes |
| `vmulh.vv` | `9ff5a557` | `bff5957b` | B | `XTsm_VMULH_VV` | yes |
| `vmulh.vx` | `9ff5e557` | `bff5d57b` | B | `XTsm_VMULH_VX` | yes |
| `vmulhsu.vv` | `9bf5a557` | `c7f5957b` | B | `XTsm_VMULHSU_VV` | yes |
| `vmulhsu.vx` | `9bf5e557` | `c7f5d57b` | B | `XTsm_VMULHSU_VX` | yes |
| `vmulhu.vv` | `93f5a557` | `c3f5957b` | B | `XTsm_VMULHU_VV` | yes |
| `vmulhu.vx` | `93f5e557` | `c3f5d57b` | B | `XTsm_VMULHU_VX` | yes |
| `vmv.s.x` | `4205e557` | `de05d57b` | B | `XTsm_VMV_S_X` | yes |
| `vmv.v.v` | `5e058557` | `f6b0557b` | D | `XTsm_VMV_V_V` | yes |
| `vmv.v.x` | `5e05c557` | `5205d57b` | B | `XTsm_VMV_V_X` | yes |
| `vmv.x.s` | `42b02557` | `dcb0557b` | C | `XTsm_VMV_X_S` | yes |
| `vmv1r.v` | `9eb03557` | `9eb0157b` | B | `XTsm_VMV1R_V` | yes |
| `vmxnor.mm` | `7ff5a557` | `fff5a57b` | B | `XTsm_VMXNOR_MM` | yes |
| `vmxor.mm` | `6ff5a557` | `eff5a57b` | B | `XTsm_VMXOR_MM` | yes |
| `vneg.v` | `0eb04557` | `f2b0557b` | B | `XTsm_VRSUB_VX (alias)` | yes |
| `vnsrl.wi` | `b2b03557` | `a2ba257b` | D | `XTsm_VNSRL_WI` | yes |
| `vor.vv` | `2bf58557` | `4bf5957b` | B | `XTsm_VOR_VV` | yes |
| `vor.vx` | `2bf5c557` | `4bf5d57b` | B | `XTsm_VOR_VX` | yes |
| `vrem.vv` | `8ff5a557` | `d3f5957b` | B | `XTsm_VREM_VV` | yes |
| `vrem.vx` | `8ff5e557` | `d3f5d57b` | B | `XTsm_VREM_VX` | yes |
| `vremu.vv` | `8bf5a557` | `d7f5957b` | B | `XTsm_VREMU_VV` | yes |
| `vremu.vx` | `8bf5e557` | `d7f5d57b` | B | `XTsm_VREMU_VX` | yes |
| `vrgather.vi` | `33f5b557` | `e3f5d57b` | B | `XTsm_VRGATHER_VI` | yes |
| `vrgather.vv` | `33f58557` | `eff5957b` | B | `XTsm_VRGATHER_VV` | yes |
| `vrgather.vx` | `33f5c557` | `33f5d57b` | B | `XTsm_VRGATHER_VX` | yes |
| `vrgatherei16.vv` | `3bf58557` | `ebf5957b` | B | `XTsm_VRGATHEREI16_VV` | yes |
| `vrsub.vx` | `0ff5c557` | `f3f5d57b` | B | `XTsm_VRSUB_VX` | yes |
| `vs1r.v` | `02858527` | `1285c57b` | B | `XTsm_VS1R_V` | yes |
| `vs2r.v` | `22858527` | `5285c57b` | B | `XTsm_VS2R_V` | yes |
| `vsbc.vvm` | `48058557` | `3c05957b` | B | `XTsm_VSBC_VVM` | yes |
| `vsbc.vxm` | `4805c557` | `3c05d57b` | B | `XTsm_VSBC_VXM` | yes |
| `vse16.v` | `0205d527` | `0205c57b` | B | `XTsm_VSE16_V` | yes |
| `vse32.v` | `0205e527` | `1205c57b` | B | `XTsm_VSE32_V` | yes |
| `vsetvli` | `1ff5f557` | `1ff5f57b` | A | `XTsm_VSETVLI` | yes |
| `vslide1down.vx` | `3ff5e557` | `cff5a57b` | B | `XTsm_VSLIDE1DOWN_VX` | yes |
| `vslide1up.vx` | `3bf5e557` | `cbf5a57b` | B | `XTsm_VSLIDE1UP_VX` | yes |
| `vslidedown.vi` | `3ff5b557` | `aff5a57b` | B | `XTsm_VSLIDEDOWN_VI` | yes |
| `vslidedown.vx` | `3ff5c557` | `d7f5a57b` | B | `XTsm_VSLIDEDOWN_VX` | yes |
| `vslideup.vi` | `3bf5b557` | `bff5a57b` | B | `XTsm_VSLIDEUP_VI` | yes |
| `vslideup.vx` | `3bf5c557` | `bbf5a57b` | B | `XTsm_VSLIDEUP_VX` | yes |
| `vsll.vv` | `97f58557` | `5bf5957b` | B | `XTsm_VSLL_VV` | yes |
| `vsll.vx` | `97f5c557` | `5bf5d57b` | B | `XTsm_VSLL_VX` | yes |
| `vsra.vv` | `a7f58557` | `6bf5957b` | B | `XTsm_VSRA_VV` | yes |
| `vsra.vx` | `a7f5c557` | `6bf5d57b` | B | `XTsm_VSRA_VX` | yes |
| `vsrl.vv` | `a3f58557` | `5ff5957b` | B | `XTsm_VSRL_VV` | yes |
| `vsrl.vx` | `a3f5c557` | `5ff5d57b` | B | `XTsm_VSRL_VX` | yes |
| `vsse16.v` | `0bf5d527` | `0bf5c57b` | B | `XTsm_VSSE16_V` | yes |
| `vsse32.v` | `0bf5e527` | `1bf5c57b` | B | `XTsm_VSSE32_V` | yes |
| `vsub.vv` | `0bf58557` | `3bf5957b` | B | `XTsm_VSUB_VV` | yes |
| `vsub.vx` | `0bf5c557` | `3bf5d57b` | B | `XTsm_VSUB_VX` | yes |
| `vsuxei32.v` | `07f5e527` | `07f5c57b` | B | `XTsm_VSUXEI32_V` | yes |
| `vsuxei64.v` | `07f5f527` | `17f5c57b` | B | `XTsm_VSUXEI64_V` | yes |
| `vxor.vv` | `2ff58557` | `57f5957b` | B | `XTsm_VXOR_VV` | yes |
| `vxor.vx` | `2ff5c557` | `57f5d57b` | B | `XTsm_VXOR_VX` | yes |

### Exceptions

**None.** `150/150` rows match, so every `remapped-RVV` row is covered by
rules A–E with the `.td` function code.

Independent confirmation from a *third* source: the fork's own
`RVV2CTFunctMap` table (extracted from the binary) agrees with the `.td` and
the census on all 111 opcode-`0x57` census rows it covers, 0 mismatches
(`python3 wt-forkenc/rvv2ct_table.py`).

## 6. What remains unverified

1. **`vm` semantics.** The remap copies `bit[25]` (or freezes it per `.td`),
   but that only shows *where* the bit goes. Whether TX8 `vm=0` means
   "masked by v0" exactly as RVV does is **not** verified — the fork emits
   `vm=0` only for `vmerge.vvm`/`vadc.vvm`-style forms in our samples, and the
   *only* `vm=0` opcode-`0x7B` words the census observes are the `.vvm`/`.vxm`
   families and `vmv.x.s`.
2. **`vsetvli`/`vtypei` field semantics.** `bits[30:20]` are copied verbatim
   and the `.td` models them as an 11-bit `vtypei` operand, but bit 31 is
   unassigned in the `.td` and the fork's encoder is documented (in the `.td`
   header comment) to "additionally set reserved vtype bits 10/9 when `rd`
   and/or `rs1` is `x0`". That quirk was **not** reproduced or validated here;
   a decoder should treat a TX8 `vsetvli` word as "RVV `vsetvli` with opcode
   `0x7B`" and re-validate the `vtypei` encoding separately.
3. **Vector length.** The fork hard-codes VLEN=256 (`li a3,256` instead of
   `csrr a3,vlenb`, FORK_DELTA §2.3), so `vtypei`'s `vlmul`/`vsew` meaning on
   TX8 hardware is unverified.
4. **The `0x07`/`0x27` load/store tables.** Their conversion code was not
   extracted field-by-field; only the 150 census rows (rules B–D) validate
   it. Segment/FF load-store forms (`vle16ff`, `vlseg*`, `vluxseg*`, ...)
   have no assembler witness at all (44 rows, class
   `vendor-cannot-reassemble`).
5. **RVV instructions with no census row.** The census covers 150 of the
   fork's RVV mnemonics; the `.td` names 172 `remapped-RVV` forms. The extra
   22 (`vid.v`, `vs4r.v`, `vs8r.v`, `vl4re16/32.v`, `vl8re16/32.v`,
   `vmv2r/4r/8r.v`, …) are `.td`+recovered-docs only and their function codes
   are confirmed by the binary table where it has an entry.
6. **`npu_opcode.h`'s `CT2RVVFunctMap` inverse.** The binary builds an inverse
   table at runtime (`CT2RVVFunctMap`, `.bss`, initialised by
   `InitializeCT2RVVFunctMap`); it is not present in the file and was not
   reconstructed. The forward table is not injective for the whole
   `funct6/funct3` space (`vcpop.m`/`vfirst.m` both map from `f6=0x10,f3=2`),
   so the inverse is only well defined together with the reserved `vs1` field.
