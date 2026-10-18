#!/usr/bin/env python3
"""Surgical LLVM/clang rebuild driver for wt-isel.

Why this exists: wt-isel/build is a *copy* of wt-forkbuild/build whose
source/build paths were rewritten by prep.sh.  Ninja then reports "command line
changed" for every edge (the command text no longer matches the hashes stored in
.ninja_log), so plain `ninja` would recompile all ~3100 edges.  This tool reads
the command lines out of build.ninja instead and runs only what is needed:

  1. regenerate TableGen outputs -- the whole lib/Target/RISCV/RISCVGen*.inc
     set, plus any edge whose command mentions a file given with
     --changed-src (e.g. clang's riscv_vector.td).  llvm-tblgen runs with
     --write-if-changed, so unchanged outputs keep their mtime; content hashes
     are diffed to learn which outputs really changed.
  2. recompile only the objects whose recorded deps mention a changed output.
  3. re-archive every static library that received a fresh object.
  4. relink bin/clang-21 and refresh the bin/clang* symlinks.

Object -> dep information comes from `ninja -t deps` (dumped to
logs/ninja_deps.txt); its paths are absolute and still point at the *old* build
and source directories, so they are normalised on load.

Usage:
  minibuild.py                                  # everything that is needed
  minibuild.py --steps tblgen                   # just regenerate + report
  minibuild.py --steps objs,libs,link           # use logs/changed_inc.json
  minibuild.py --changed-src clang/include/clang/Basic/riscv_vector.td
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = "/home/shaokai/Desktop/work/tx82/wt-raisa-r2"
BUILD = ROOT + "/build"
LOGS = ROOT + "/logs"
DEPS = LOGS + "/ninja_deps.txt"
OLD_BUILD = "/home/shaokai/Desktop/work/tx82/wt-xfrm/build"
OLD_SRC = "/home/shaokai/Desktop/work/tx82/wt-xfrm/src"

TBLGEN_RE = re.compile(r"^lib/Target/RISCV/RISCVGen.*\.inc$")


def parse_ninja(path):
    """Parse build.ninja into a list of {outs, ins, rule, command, desc}."""
    edges = []
    with open(path, "r", errors="replace") as f:
        lines = f.read().split("\n")
    i, n = 0, len(lines)
    while i < n:
        m = re.match(r"^build (.+?): (\S+)(.*)$", lines[i])
        if not m:
            i += 1
            continue
        # ninja syntax:  build <outs> [| <implicit outs>]: <rule> <ins> [| <implicit ins>] [|| <order-only>]
        head = m.group(1)
        rest = m.group(3)
        outs_s = head.split(" | ", 1)[0]
        ins_s = rest.split(" || ", 1)[0].split(" | ", 1)[0]
        outs = [o for o in outs_s.split(" ") if o and not o.startswith("${")]
        ins = [o for o in ins_s.split(" ") if o and not o.startswith("${")]
        rule = m.group(2)
        cmd = desc = None
        j = i + 1
        while j < n and lines[j].startswith("  "):
            s = lines[j].strip()
            if s.startswith("COMMAND = "):
                cmd = s[len("COMMAND = "):]
                k = j + 1
                while k < n and lines[k].startswith("    ") and not lines[k].strip().startswith(
                        ("COMMAND", "DESC", "pool", "restat", "depfile", "deps", "generator")):
                    cmd += "\n" + lines[k]
                    k += 1
                j = k - 1
            elif s.startswith("DESC = "):
                desc = s[len("DESC = "):]
            j += 1
        edges.append({"outs": outs, "ins": ins, "rule": rule,
                      "command": cmd, "desc": desc})
        i = j
    return edges


def norm(p):
    """Rewrite the old absolute build/source prefixes; build-dir paths become
    build-relative so they can be compared with build.ninja output names.

    The recorded deps may name *any* other `wt-*` line's build/src directory
    (this tree was copied/derived more than once), so normalise generically on
    the `/wt-<name>/build/` and `/wt-<name>/src/` markers instead of only the
    one hard-coded ancestor.  Getting this wrong silently yields "objects to
    rebuild: 0" while the generated headers are newer than the objects."""
    m = re.match(r"^.*?/wt-[^/]+/build/(.*)$", p)
    if m:
        return m.group(1)
    m = re.match(r"^.*?/wt-[^/]+/src/(.*)$", p)
    if m:
        return ROOT + "/src/" + m.group(1)
    if p.startswith(BUILD + "/"):
        return p[len(BUILD) + 1:]
    return p


def load_object_deps():
    """output path -> set of dep paths (paths normalised to this tree)."""
    deps = {}
    cur = None
    if not os.path.exists(DEPS):
        return deps
    head = re.compile(
        r"^(\S.*?): #deps \d+, deps mtime \d+ \((VALID|STALE|MISSING)\)$")
    with open(DEPS, "r", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                cur = None
                continue
            if line.startswith("    "):
                if cur:
                    deps.setdefault(cur, set()).add(norm(line.strip()))
                continue
            m = head.match(line)
            cur = norm(m.group(1)) if m else None
            if cur:
                deps.setdefault(cur, set())
    return deps


def sha(path):
    try:
        with open(path, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()
    except OSError:
        return None


def run(cmd, label, dry):
    if dry:
        print("DRY %-52s %s" % (label, cmd.split("\n")[0][:110]))
        return
    t0 = time.time()
    p = subprocess.run(["/bin/bash", "-o", "pipefail", "-c", cmd], cwd=BUILD)
    dt = time.time() - t0
    if p.returncode != 0:
        print("!! FAILED rc=%d after %.1fs: %s" % (p.returncode, dt, label))
        sys.exit(p.returncode)
    print("   ok %-58s %6.1fs" % (label, dt))


def ninja_last_cmd(target):
    p = subprocess.run(["ninja", "-t", "commands", target], cwd=BUILD,
                       capture_output=True, text=True)
    if p.returncode != 0:
        print("   !! ninja -t commands %s failed: %s" % (target, p.stderr.strip()[:200]))
        return None
    out = [l for l in p.stdout.split("\n") if l.strip()]
    return out[-1] if out else None


def edge_command(target, by_out):
    """Compile edges inherit their command from the ninja *rule*, so only
    custom commands carry an inline COMMAND.  Fall back to `ninja -t commands`
    (which expands the rule) for everything else."""
    e = by_out.get(target)
    if e and e["command"]:
        return e["command"]
    return ninja_last_cmd(target)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--steps", default="tblgen,objs,libs,link")
    ap.add_argument("--force-objs", default="")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--force-src", default="",
                    help="comma separated edited .cpp/.h files: recompile every "
                         "object whose recorded deps mention them")
    ap.add_argument("--changed-src", default="",
                    help="comma separated extra sources whose tablegen edges rerun")
    args = ap.parse_args()
    steps = set(args.steps.split(","))
    dry = args.dry_run
    os.makedirs(LOGS, exist_ok=True)

    edges = parse_ninja(BUILD + "/build.ninja")
    by_out = {}
    for e in edges:
        for o in e["outs"]:
            by_out.setdefault(o, e)
    print("ninja: %d edges, %d with inline COMMAND" %
          (len(edges), sum(1 for e in edges if e["command"])))

    # ---------- step 1: TableGen -----------------------------------------
    changed_inc = []
    if "tblgen" in steps:
        print("== [1/4] TableGen ==")
        targets = []
        for e in edges:
            if not e["command"]:
                continue
            prim = next((o for o in e["outs"] if TBLGEN_RE.match(o)), None)
            if prim is None:
                for s in args.changed_src.split(","):
                    s = s.strip()
                    if s and s in e["command"] and e["outs"]:
                        prim = e["outs"][0]
                        break
            if prim:
                targets.append((prim, e))
        seen = set()
        for prim, e in targets:
            if prim in seen:
                continue
            seen.add(prim)
            out_abs = os.path.join(BUILD, prim)
            before = sha(out_abs)
            run(e["command"], prim, dry)
            after = sha(out_abs)
            if before != after:
                changed_inc.append(prim)
                print("   *** CHANGED %s" % prim)
        print("changed outputs: %s" % (", ".join(changed_inc) or "none"))
    else:
        p = LOGS + "/changed_inc.json"
        changed_inc = json.load(open(p)) if os.path.exists(p) else []
        print("changed outputs (from logs): %s" % (", ".join(changed_inc) or "none"))
    json.dump(changed_inc, open(LOGS + "/changed_inc.json", "w"))

    objdeps = load_object_deps()
    print("dep table: %d outputs" % len(objdeps))

    # ---------- step 2: objects ------------------------------------------
    force = [s for s in args.force_objs.split(",") if s]
    force_src = [os.path.normpath(s) for s in args.force_src.split(",") if s]
    rebuilt = []
    if "objs" in steps and (changed_inc or force or force_src):
        print("== [2/4] recompile dependent objects ==")
        for obj, ds in sorted(objdeps.items()):
            if not obj.endswith(".o"):
                continue
            hit = any(c in ds for c in changed_inc)
            if not hit and force:
                hit = any(f in os.path.basename(obj) for f in force)
            if not hit and force_src:
                hit = any(any(fs in d for d in ds) for fs in force_src)
            if hit:
                rebuilt.append(obj)
        print("objects to rebuild: %d (jobs=%d)" % (len(rebuilt), args.jobs))

        def one(obj):
            cmd = edge_command(obj, by_out)
            if not cmd:
                print("   !! no command for %s (stale build? skipped)" % obj)
                return
            run(cmd, os.path.basename(obj), dry)

        with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as ex:
            list(ex.map(one, rebuilt))
    else:
        print("== [2/4] nothing to recompile ==")

    # ---------- step 3: archives -----------------------------------------
    if "libs" in steps and rebuilt:
        print("== [3/4] re-archive ==")
        # "<dir>/CMakeFiles/<Target>.dir/<src>.o" -> "lib/<Target>.a"
        libs = set()
        for obj in rebuilt:
            mm = re.search(r"CMakeFiles/([^/]+)\.dir/", obj)
            if mm:
                cand = "lib/lib%s.a" % mm.group(1)
                if cand in by_out:
                    libs.add(cand)
        # cross-check against the archive edges' input lists
        for e in edges:
            for o in e["outs"]:
                if re.match(r"^lib/.*\.a$", o) and any(r in e["ins"] for r in rebuilt):
                    libs.add(o)
        for lib in sorted(libs):
            cmd = ninja_last_cmd(lib)
            if cmd:
                run(cmd, lib, dry)
        if not libs:
            print("   (no archive depends on the rebuilt objects)")
    else:
        print("== [3/4] no archives ==")

    # ---------- step 4: link --------------------------------------------
    if "link" in steps and (rebuilt or "force" in steps):
        print("== [4/4] relink clang ==")
        for tgt, label in (("bin/clang-21", "bin/clang-21"),
                           ("bin/clang", "clang symlinks")):
            cmd = ninja_last_cmd(tgt)
            if cmd:
                run(cmd, label, dry)
    else:
        print("== [4/4] no relink ==")
    print("minibuild done")


if __name__ == "__main__":
    main()
