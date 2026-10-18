#!/bin/bash
set -u
R=/home/shaokai/Desktop/work/tx82/tools/re
B=/home/shaokai/Desktop/work/tx82/docs/compiler/fork-source/backend-objects
L=$R/out/bulk.log
mkdir -p $R/out
echo "=== [$(date -Is)] 批量开始 ===" >> $L
for o in $B/TX8*.cpp.o; do
  n=$(basename "$o" .cpp.o); d=$R/out/$n
  [ -f "$d/decompiled.c" ] && { echo "skip $n（已有）" >> $L; continue; }
  t0=$(date +%s)
  timeout 2700 $R/ghidra_decompile.sh "$o" "$d" >> $L 2>&1
  rc=$?; t1=$(date +%s)
  lines=$(wc -l < "$d/decompiled.c" 2>/dev/null || echo 0)
  fns=$(grep -c '====' "$d/decompiled.c" 2>/dev/null || echo 0)
  echo "$n rc=$rc $((t1-t0))s ${lines}行 ${fns}函数" >> $L
done
echo "=== [$(date -Is)] 批量结束 ===" >> $L
