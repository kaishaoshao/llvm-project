#!/bin/bash
# Machine-pipeline A/B, fixed (see the parent's A4 report: the previous version
# was silently comparing two *empty* files).
#
# Original design (still correct): replay the **machine** pipeline instead of
# feeding IR back in (the receiving `simt2simd` would re-pack the signature).
#
#   1. vendor clang : -stop-before=<pass>            -> a complete MIR module
#   2. vendor llc   : -start-before=<pass>           -> vendor asm (method check)
#   3. our llc      : -start-before=<pass> on the *mapped* MIR -> our asm
#
# The three bugs fixed here:
#   (i)   `MAPNAME` used BRE parentheses (`\$s([0-9]+)`); GNU sed aborts/does not
#         match -> use `sed -E` and `\$s([0-9]+)`.
#   (ii)  the name map was applied to the *output asm* only.  The vendor MIR
#         carries vendor instruction names (`SET_MEM_ALLOC`, `PseudoVSLH...`,
#         `$s0`, `srf128`, `rpu_kernel`) which our llc cannot parse, so our llc
#         produced nothing.  The map is now applied to the **input MIR** too.
#   (iii) empty-vs-empty was reported as "0 diff".  Both products are asserted
#         non-empty and of a plausible line count.
#
# usage: mir_ab.sh <cell> [pass]        (pass defaults to riscv-insert-vsetvli)
set -u
ROOT=/home/shaokai/Desktop/work/tx82
WT=$ROOT/wt-merge-e
RC=$ROOT/docx/tests/llvm/tx8_blackbox/round2/sources
VCLANG=$ROOT/llvm-toolchain/bin/clang
VLLC=$ROOT/llvm-toolchain/bin/llc
OLLC=$WT/build/bin/llc
CELL=${1:?cell}
PASS=${2:-riscv-insert-vsetvli}
# Scratch must stay inside this tree: `$WT/recon` is a read-only symlink to the
# wt-merge line's recon dir, so writing there would pollute another line's tree.
OUT=$WT/logs/mir_ab
mkdir -p "$OUT"
PRE=$OUT/$CELL.pre.mir

echo "== [1/5] vendor -stop-before=$PASS MIR for $CELL =="
"$VCLANG" -cc1 -triple riscv64-unknown-raisa -target-cpu tsm-x82 \
  -target-feature +xtsm82s -fraisa-is-device -x raisa -O2 -S \
  -mllvm -stop-before=$PASS -o "$PRE" "$RC/$CELL.rc" 2>"$OUT/$CELL.vclang.err"
rc=$?
if [ $rc -ne 0 ] || [ ! -s "$PRE" ]; then
  echo "HARNESS FAILED: vendor clang rc=$rc, MIR $(wc -c <"$PRE" 2>/dev/null) bytes"
  tail -3 "$OUT/$CELL.vclang.err"; exit 2
fi
echo "   $(wc -l <"$PRE") lines"

# ---------------------------------------------------------------------------
# Name map (verdict ③: names are aligned here, not in the .td).  Applied to the
# input MIR (so our llc can parse the vendor's module) *and* to both asm
# products, so the diff shows real code-generation differences only.
# ---------------------------------------------------------------------------
MAP='
s/\bPseudoMOV128_S_V_B32\b/PseudoXTsm_MV128_S_V/g
s/\bMOV128_S_V\b/XTsm_MV128_S_V/g
s/\bMOV128_S_X\b/XTsm_MV128_S_X/g
s/\bKcore_Ncc_Read\b/XTsm_KCORE_NCC_READ/g
s/\bNOR_S\b/XTsm_NOR_SS/g
s/\bAND_S\b/XTsm_AND_SS/g
s/\bOR_S\b/XTsm_OR_SS/g
s/\bXOR_S\b/XTsm_XOR_SS/g
s/\bANDN_S\b/XTsm_ANDN_SS/g
s/\bORN_S\b/XTsm_ORN_SS/g
s/\bNAND_S\b/XTsm_NAND_SS/g
s/\bXNOR_S\b/XTsm_XNOR_SS/g
s/\bISETEXEC\b/XTsm_ISETEXEC/g
s/\bISETEXEC0\b/XTsm_ISETEXEC0/g
s/\bISETEXEC1\b/XTsm_ISETEXEC1/g
s/\bMOVEXECZ\b/XTsm_MOVEXECZ/g
s/\bVSET_SIMT\b/XTsm_VSETSIMT/g
s/\bSET_MEM_ALLOC\b/XTsm_SET_MEM_ALLOC/g
s/\bPseudo(VLUXEI32|VLUXEI64|VSUXEI32|VSUXEI64)_SYNC_V/PseudoXTsm_\1_SYNC_V/g
s/\bPseudoVSLH/PseudoXTsm_VSLH/g
s/\bsrf128\b/sr/g
s/\$s([0-9]+)/\$sr\1/g
s/rpu_kernel void/cc 124 void/g'

# `_SYNC_` pseudos: this tree records (vs2/data, index, base) while the
# vendor records (vs2/data, base, index) -- a registered reconstruction
# difference compensated in RISCVISelDAGToDAG; swap them back (see
# tools/mir_sync_swap.py).
map_mir()  { sed -E -e "$MAP" "$1" | python3 "$WT/tools/mir_sync_swap.py" /dev/stdin; }
norm()     { sed -e 's/#.*//' -e '/\.ident/d' -e 's/[[:space:]]*$//' "$1"; }
norm_v()   { norm "$1" | sed -E -e "$MAP"; }

echo "== [2/5] mapping the MIR into our spelling =="
OURMIR=$OUT/$CELL.ours.mir
map_mir "$PRE" > "$OURMIR"
echo "   $(wc -l <"$OURMIR") lines"

echo "== [3/5] vendor llc replay (method check) =="
"$VLLC" -mtriple=riscv64-unknown-raisa -mcpu=tsm-x82 -mattr=+xtsm82s \
  -start-before=$PASS -o "$OUT/$CELL.vllc.s" "$PRE" 2>"$OUT/$CELL.vllc.err"
vrc=$?
echo "   rc=$vrc  $(wc -l <"$OUT/$CELL.vllc.s" 2>/dev/null) lines"

echo "== [4/5] our llc replay =="
"$OLLC" -mtriple=riscv64-unknown-raisa -mcpu=tsm-x82 -mattr=+xtsm82s \
  -start-before=$PASS -o "$OUT/$CELL.olls.s" "$OURMIR" 2>"$OUT/$CELL.ollc.err"
orc=$?
echo "   rc=$orc  $(wc -l <"$OUT/$CELL.olls.s" 2>/dev/null) lines"
if [ $orc -ne 0 ] || [ ! -s "$OUT/$CELL.olls.s" ]; then
  echo "HARNESS FAILED: our llc rc=$orc, $(wc -c <"$OUT/$CELL.olls.s" 2>/dev/null) bytes"
  tail -3 "$OUT/$CELL.ollc.err"
  echo "== [5/5] diff lines: N/A (harness failed) =="
  exit 3
fi
if [ $vrc -ne 0 ] || [ ! -s "$OUT/$CELL.vllc.s" ]; then
  echo "HARNESS FAILED: vendor llc rc=$vrc"; tail -3 "$OUT/$CELL.vllc.err"
  echo "== [5/5] diff lines: N/A (harness failed) =="
  exit 4
fi

echo "== [5/5] vendor llc vs our llc (normalised) =="
diff <(norm_v "$OUT/$CELL.vllc.s") <(norm "$OUT/$CELL.olls.s") > "$OUT/$CELL.diff" || true
head -40 "$OUT/$CELL.diff"
echo "== diff lines: $(grep -c '^[<>]' "$OUT/$CELL.diff") =="
echo "== DONE =="
