#!/usr/bin/env bash
# Wait for the machine to be free, then recompile the objects whose *source* is
# newer than the object.
#
# Why: `wt-config/build` is derived from another line's build directory, so some
# objects were compiled before patches that are present in `wt-config/src`
# landed (the important one is `RISCVInsertVSETVLI.cpp.o`, which predates patch
# `0009-raisa-bf16-vtype-format-bit`).  `ninja` cannot be used directly (the
# rewritten command lines no longer match `.ninja_log`), and `minibuild.py`
# cannot see objects that minibuild itself built (no `.ninja_deps` entry), so
# this sweeps `build.ninja` for `src mtime > obj mtime` and recompiles exactly
# those, then re-archives and relinks.
set -u
ROOT=/home/shaokai/Desktop/work/tx82
WT="${1:-$ROOT/wt-config}"
LOGS=$WT/logs
mkdir -p "$LOGS"

echo "waiting for ninja/cc1plus/c++ to finish and for tools/.builds-paused to clear"
for _ in $(seq 1 360); do
  if [ ! -e "$ROOT/tools/.builds-paused" ] && ! pgrep -x ninja >/dev/null \
     && ! pgrep -x cc1plus >/dev/null && ! pgrep -x c++ >/dev/null; then
    echo "machine free: $(date)"
    break
  fi
  sleep 20
done

python3 - "$WT" <<'PY' > "$LOGS/stale_objects.txt"
import re, os, sys
WT = sys.argv[1]
B = WT + "/build/"
out = []
for line in open(B + "build.ninja", errors="replace"):
    m = re.match(r"^build (\S+\.o): (\S+) (.*)$", line)
    if not m:
        continue
    obj, rule, ins = m.groups()
    if "CXX_COMPILER" not in rule and "C_COMPILER" not in rule:
        continue
    srcs = [x for x in ins.split() if x.endswith((".cpp", ".c", ".cc", ".cxx"))]
    if len(srcs) != 1:
        continue
    sp = srcs[0] if os.path.isabs(srcs[0]) else WT + "/src/" + srcs[0]
    op = os.path.join(B, obj)
    if os.path.exists(op) and os.path.exists(sp) and os.path.getmtime(sp) > os.path.getmtime(op):
        print(obj)
PY
echo "stale objects:"; sed 's/^/   /' "$LOGS/stale_objects.txt"

while read -r obj; do
  [ -z "$obj" ] && continue
  case "$obj" in
    *CMakeFiles/LLVMRISCVCodeGen.dir/*) lib=lib/libLLVMRISCVCodeGen.a ;;
    *CMakeFiles/LLVMAsmParser.dir/*)    lib=lib/libLLVMAsmParser.a ;;
    *) echo "skip (unknown archive): $obj"; continue ;;
  esac
  echo "== $obj =="
  nice -n 10 timeout -s KILL 2400 bash "$WT/tools/rebuild_one.sh" "$obj" "$lib" 2>&1 | tail -3
done < "$LOGS/stale_objects.txt"
echo "SWEEP DONE $(date)"
