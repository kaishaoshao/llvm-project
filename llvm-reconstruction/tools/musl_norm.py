#!/usr/bin/env python3
"""musl gate, *both calibers*: raw bytes and `.comment`-stripped.

`tools/musl_ab.py` compares raw object bytes.  For a *vendor vs ours* run that
is meaningless: the vendor's `.comment` is
`21.1.0-rc1 (ssh://192.168.100.107:29418/ts-compiler/ts-llvm-project b85caf26...)`
while ours is `(https://github.com/llvm/llvm-project.git cbcb48a88c...)` -- so
every cell necessarily differs.  The meaningful vendor-vs-ours criterion (see
docs/compiler/fork-source/reconstruction/README.md, "基线口径修正") is the
object with `.comment` removed.

This tool therefore drives the same 14 x 4 recipe as `musl_ab.py` and reports

    raw identical .o      / identical failures / outcomes
    stripped identical .o / identical failures / outcomes

where "stripped" is `normcmp.norm_object` (vendor `llvm-objdump -s`, every
section except `.comment`).

  python3 tools/musl_norm.py --a <baseline clang> --b <new clang>

Scratch goes to wt-raisa-be/parity/ab_norm/ (never /tmp).
"""
import argparse
import concurrent.futures as cf
import glob
import hashlib
import os
import shlex
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import normcmp  # noqa: E402

ROOT = Path("/home/shaokai/Desktop/work/tx82")
WT = ROOT / "wt-raisa-be"
SYSROOT = ROOT / "llvm-toolchain/sysroot"
KERNELS = ROOT / "gemini-cmodel/kernels"
KRT = ROOT / "cmodel/include/boards/riscv/andes/include/krt_interface"

BASE = ["--target=riscv64-unknown-linux-musl", "--sysroot=" + str(SYSROOT),
        "-mcpu=tsm-x82", "-mabi=lp64f", "-std=c11", "-fPIC", "-nostdlib",
        "-ffreestanding", "-nostdinc", "-I" + str(KERNELS), "-I" + str(KRT)]
CONFIGS = ["-O0", "-O1", "-O2 -fno-vectorize", "-O2"]


def one(cc, tag, kern, cfg, timeout=120):
    out = WT / "parity" / "ab_norm" / ("%s_%s_%s.o" % (tag, kern, cfg.replace(" ", "")))
    out.parent.mkdir(parents=True, exist_ok=True)
    if out.exists():
        out.unlink()
    cmd = [cc] + BASE + shlex.split(cfg) + ["-c", str(KERNELS / (kern + ".c")),
                                           "-o", str(out)]
    pre = "ulimit -c 0; ulimit -f 262144; "
    p = subprocess.run(["/bin/bash", "-c", pre + "timeout -s KILL %d " % timeout +
                        " ".join(shlex.quote(c) for c in cmd)],
                       capture_output=True, text=True, cwd=str(ROOT))
    raw = hashlib.sha256(out.read_bytes()).hexdigest()[:16] if out.exists() else ""
    nrm = normcmp.norm_object(out) if out.exists() else ""
    return (kern, cfg, p.returncode, raw, nrm, (p.stdout + p.stderr)[:4000])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True, help="baseline clang (e.g. vendor)")
    ap.add_argument("--b", required=True, help="new clang (ours)")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    a.a = str(Path(a.a).resolve())
    a.b = str(Path(a.b).resolve())
    kerns = sorted(os.path.basename(p)[:-2]
                   for p in glob.glob(str(KERNELS / "tx82_*.c")))
    jobs = [(k, c) for k in kerns for c in CONFIGS]
    res = {}
    for side, cc in (("a", a.a), ("b", a.b)):
        with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
            futs = [ex.submit(one, cc, side, k, c) for k, c in jobs]
            for f in cf.as_completed(futs):
                k, c, rc, raw, nrm, err = f.result()
                res[(side, k, c)] = (rc, raw, nrm, err)
    raw_same = nrm_same = fail = 0
    diffs = []
    for k, c in jobs:
        ra, raw_a, nrm_a, ea = res[("a", k, c)]
        rb, raw_b, nrm_b, eb = res[("b", k, c)]
        if ra != 0 and rb != 0 and ea.split("\n")[0] == eb.split("\n")[0]:
            fail += 1
        if ra == 0 and rb == 0 and raw_a == raw_b:
            raw_same += 1
        if ra == 0 and rb == 0 and nrm_a == nrm_b:
            nrm_same += 1
        elif ra == 0 and rb == 0:
            diffs.append((k, c, raw_a, raw_b, nrm_a, nrm_b))
        elif ra == 0 and rb != 0:
            diffs.append((k, c, raw_a, "OURS-FAIL", nrm_a, eb.strip()[:160]))
        elif ra != 0 and rb == 0:
            diffs.append((k, c, "BASE-FAIL", raw_b, ea.strip()[:160], nrm_b))
    print("%s (a) -> %s (b)" % (a.a, a.b))
    print("  raw      : %d/%d identical .o + %d identical failures = %d/%d identical outcomes"
          % (raw_same, len(jobs), fail, raw_same + fail, len(jobs)))
    print("  .comment-stripped: %d/%d identical .o + %d identical failures = %d/%d identical outcomes"
          % (nrm_same, len(jobs), fail, nrm_same + fail, len(jobs)))
    if not a.quiet:
        for k, c, x, y, nx, ny in diffs:
            print("  DIFF %-22s %-18s raw %s/%s  norm %s/%s"
                  % (k, c, x, y, nx, ny))
    return 0


if __name__ == "__main__":
    sys.exit(main())
