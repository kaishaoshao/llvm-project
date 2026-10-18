#!/usr/bin/env python3
"""Golden-IR A/B over the *cached* vendor post-`simt2simd` modules.

`tools/postsimt2simd_ab.py` re-runs the vendor for every cell, which is slow and
needs the vendor binary.  The same modules (and the vendor `.s` they came with)
are already archived verbatim by that tool under `recon/postsimt2simd/`:

    <cell>.postsimt.ll   vendor module dumped with -mllvm -print-after=simt2simd
    <cell>.vendor.s      the vendor's asm for the same compilation

This tool only replays step 2 of that harness -- feed the archived module to our
clang and compare the normalised `.s` -- so it is deterministic (no vendor run,
no ASLR) and costs a few seconds per cell.  It is the per-cell oracle for the
ISel/pseudo layer: `OURS-REJECTED-IR` means our backend aborts on an intrinsic,
`IDENTICAL` means our whole downstream pipeline reproduces the vendor for a
module our IR layer already got right.

Usage:
  tools/golden_ir_ab.py                       # every cached cell
  tools/golden_ir_ab.py --contains vslh
  tools/golden_ir_ab.py --cells mem_l1_size_1,mem_l1_size_128
  tools/golden_ir_ab.py --cc build/bin/clang-21 --verbose
"""
import argparse
import concurrent.futures as cf
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path("/home/shaokai/Desktop/work/tx82")
WT = ROOT / "wt-raisa-r2"
CACHE = WT / "recon/postsimt2simd"
OUT = WT / "recon/golden_ir_ab"
CC1 = ["-cc1", "-triple", "riscv64-unknown-raisa", "-target-cpu", "tsm-x82",
       "-target-feature", "+xtsm82s", "-fraisa-is-device", "-O2"]


def run(cmd, timeout=90):
    pre = "ulimit -c 0; ulimit -f 4194304; "
    p = subprocess.run(["/bin/bash", "-c", pre + "timeout -s KILL %d " % timeout +
                        " ".join(shlex.quote(c) for c in cmd)],
                       capture_output=True, text=True, cwd=str(ROOT))
    return p.returncode, p.stdout + p.stderr


def norm(path):
    if not os.path.exists(path):
        return None
    out = []
    for line in Path(path).read_text(errors="replace").split("\n"):
        if re.match(r"^\s*\.(ident|file|section|attribute|globl?)", line):
            continue
        s = re.sub(r"#.*$", "", line).rstrip()
        if s:
            out.append(s)
    return out


def one(cc, cell, verbose):
    ll = CACHE / (cell + ".postsimt.ll")
    vs = CACHE / (cell + ".vendor.s")
    if not ll.exists():
        return cell, "NO-CACHE", ""
    if not vs.exists():
        return cell, "NO-VENDOR-ASM", ""
    OUT.mkdir(parents=True, exist_ok=True)
    ours = OUT / (cell + ".ours.s")
    if ours.exists():
        ours.unlink()
    rc, log = run([cc] + CC1 + ["-x", "ir", "-S", str(ll), "-o", str(ours)], 90)
    if rc != 0:
        first = next((l for l in log.splitlines()
                      if "error" in l or "Cannot select" in l), "")
        return cell, "OURS-REJECTED-IR", first.strip()[:110]
    a, b = norm(str(vs)), norm(str(ours))
    if a is None or b is None:
        return cell, "MISSING-ASM", ""
    if a == b:
        return cell, "IDENTICAL", ""
    d = [l for l in a if l not in b][:1] + [l for l in b if l not in a][:1]
    return cell, "DIFF", " | ".join(x[:52] for x in d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=str(WT / "build/bin/clang-21"))
    ap.add_argument("--cells", default="")
    ap.add_argument("--contains", default="",
                    help="only cells whose cached module mentions this string")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--baseline", default="",
                    help="previous results.tsv: report the per-cell transition")
    a = ap.parse_args()
    # `run()` executes with cwd=ROOT, so a path relative to the work tree must
    # be made absolute here (a bare relative --cc silently makes every cell fail
    # with rc=127 and be reported as OURS-REJECTED-IR).
    if not os.path.isabs(a.cc):
        a.cc = str((WT / a.cc).resolve())

    if a.cells:
        cells = [c for c in a.cells.split(",") if c]
    else:
        cells = sorted(p.name[:-len(".postsimt.ll")]
                       for p in CACHE.glob("*.postsimt.ll"))
        if a.contains:
            cells = [c for c in cells
                     if a.contains in (CACHE / (c + ".postsimt.ll")).read_text(
                         errors="replace")]
    prev = {}
    if a.baseline and os.path.exists(a.baseline):
        for line in Path(a.baseline).read_text().split("\n")[1:]:
            f = line.split("\t")
            if len(f) > 1 and f[0]:
                prev[f[0]] = f[1]

    res = []
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for out in pool.map(lambda c: one(a.cc, c, a.verbose), cells):
            res.append(out)
    res.sort()

    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / "results.tsv", "w") as f:
        f.write("cell\tgolden_ir\tprevious\tnote\n")
        for cell, verdict, note in res:
            f.write("%s\t%s\t%s\t%s\n" % (cell, verdict, prev.get(cell, "-"), note))

    from collections import Counter
    for cell, verdict, note in res:
        if verdict != "IDENTICAL" or a.verbose:
            delta = ""
            if cell in prev and prev[cell] != verdict:
                delta = "  [%s -> %s]" % (prev[cell], verdict)
            print("   %-40s %-18s%s %s" % (cell, verdict, delta, note))
    print()
    for k, v in Counter(x[1] for x in res).most_common():
        print("   %-22s %d" % (k, v))
    moved = [(c, prev.get(c), v) for c, v, _ in res
             if c in prev and prev[c] != v]
    if prev:
        print("\n   transitions vs baseline: %d" % len(moved))
        for c, p, v in moved:
            print("     %-40s %s -> %s" % (c, p, v))


if __name__ == "__main__":
    sys.exit(main())
