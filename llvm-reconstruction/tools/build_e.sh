#!/bin/bash

# 2026-10-17: --force-src 在相对路径下会静默匹配 0 个对象然后 rc=0 —— 必须显式报错。
if grep -q -- '--force-src' <<< "$*" ; then
  case "$*" in
    *--force-src*/*) : ;;
    *) echo "  ✗ --force-src 需要绝对路径（相对路径静默重编 0 个对象）" >&2; exit 2;;
  esac
fi
# Build driver for wt-merge-e (derived build dir).  Mirrors wt-merge/tools/merge_build.sh:
#   1. dump the copy's object->dep table (its .ninja_deps still points at wt-merge),
#   2. re-run CMake (new/removed source files),
#   3. TableGen + every object whose recorded deps mention an edited source,
#   4. compile the objects minibuild cannot see (new files, no dep record),
#   5. re-archive the libraries that received objects and relink bin/clang-21 + bin/llc.
#
# usage: build_e.sh "<comma separated edited srcs>" "[extra objs]"
set -u
ROOT=/home/shaokai/Desktop/work/tx82
WT=$ROOT/wt-merge-e
BUILD=$WT/build
SRC=$WT/src
LOGS=$WT/logs
mkdir -p "$LOGS"
EDITED=${1:-}
NEW_OBJS=${2:-}
ARCHIVES="lib/libLLVMRISCVCodeGen.a lib/libLLVMRISCVDesc.a lib/libLLVMRISCVInfo.a \
          lib/libLLVMRISCVAsmParser.a lib/libLLVMRISCVDisassembler.a"

cd "$BUILD"
echo "== [0/5] machine check $(date) =="
for _ in $(seq 1 720); do
  if [ -e "$ROOT/tools/.budget-stop" ]; then echo "BUDGET STOP"; exit 9; fi
  if [ ! -e "$ROOT/tools/.builds-paused" ] && ! pgrep -x ninja >/dev/null \
     && ! pgrep -x cc1plus >/dev/null && ! pgrep -x c++ >/dev/null; then
    echo "machine free"; break
  fi
  sleep 20
done
free -h | head -2

echo "== [1/5] CMake re-run (source list) =="
nice -n 10 timeout 1800 ninja build.ninja 2>&1 | tail -3

echo "== [2/5] dep table + synthetic entries for the edited sources =="
ninja -t deps > "$LOGS/ninja_deps.txt" 2>/dev/null
wc -l < "$LOGS/ninja_deps.txt"
python3 - "$LOGS/ninja_deps.txt" "$BUILD" "$EDITED" <<'PY'
import os, re, sys
depfile, build, edited = sys.argv[1], sys.argv[2], sys.argv[3]
srcs = [s for s in edited.split(",") if s]
edges = {}
for line in open(os.path.join(build, "build.ninja"), errors="replace"):
    m = re.match(r"^build (\S+\.o): (\S+) (.*)$", line)
    if not m:
        continue
    obj, rule, ins = m.groups()
    if "CXX_COMPILER" not in rule and "C_COMPILER" not in rule:
        continue
    s = [x for x in ins.split() if x.endswith((".cpp", ".c", ".cc", ".cxx"))]
    if len(s) == 1:
        edges[os.path.join(build, obj)] = s[0]
out = []
for src in srcs:
    want = os.path.normpath(src)
    hit = [o for o, s in edges.items() if os.path.normpath(
        s if os.path.isabs(s) else os.path.join(build, s)) == want]
    print("  %-52s -> %d object(s)" % (os.path.basename(src), len(hit)))
    for o in hit:
        out.append("%s: #deps 1, deps mtime 1 (VALID)\n    %s\n" % (o, want))
if out:
    with open(depfile, "a") as f:
        f.write("".join(out))
PY

echo "== [3/5] tablegen + dependent objects =="
nice -n 10 timeout 14400 python3 "$WT/tools/minibuild.py" --steps tblgen,objs --jobs 2 \
  --force-src "$EDITED"
rc=$?
echo "minibuild rc=$rc"
[ "$rc" -ne 0 ] && { echo "!! tablegen/objs failed"; exit 1; }

echo "== [4/5] objects minibuild cannot see =="
for o in ${NEW_OBJS//,/ }; do
  [ -z "$o" ] && continue
  cmd=$(ninja -t commands "$o" | tail -1)
  echo "-- $o"
  nice -n 10 timeout 3600 bash -c "$cmd" 2>&1 | tail -3
  [ "${PIPESTATUS[0]}" -ne 0 ] && { echo "!! $o failed"; exit 1; }
done

echo "== [5/5] archives + link =="
for lib in $ARCHIVES; do
  [ -f "$BUILD/$lib" ] || continue
  cmd=$(ninja -t commands "$lib" | tail -1)
  nice -n 10 timeout 1800 bash -c "$cmd" 2>&1 | tail -1 || echo "!! archive $lib failed"
done
for tgt in bin/clang-21 bin/llc; do
  cmd=$(ninja -t commands "$tgt" | tail -1)
  nice -n 10 timeout 3600 bash -c "$cmd" 2>&1 | tail -1
done
echo "== BUILD E DONE $(date) =="
ls -la --time-style=full-iso "$BUILD/bin/clang-21" "$BUILD/bin/llc"
