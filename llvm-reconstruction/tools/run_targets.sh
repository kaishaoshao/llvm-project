#!/bin/bash
set -u
R=/home/shaokai/Desktop/work/tx82/tools/re
B=/home/shaokai/Desktop/work/tx82/docs/compiler/fork-source/backend-objects
L=$R/out/targets.log
echo "=== [$(date -Is)] 开始 ===" >> $L
# 1) 待重建：simt2simd（352 KB / 176 函数）
timeout 2700 $R/ghidra_decompile.sh $B/TX8TransformSIMTToSIMD.cpp.o $R/out/simt2simd >> $L 2>&1
echo "simt2simd rc=$? $(wc -l < $R/out/simt2simd/decompiled.c 2>/dev/null) 行" >> $L
# 2) 待重建：lower-shared（54 KB）
timeout 1200 $R/ghidra_decompile.sh $B/TX8LowerSharedPass.cpp.o $R/out/lowershared >> $L 2>&1
echo "lowershared rc=$? $(wc -l < $R/out/lowershared/decompiled.c 2>/dev/null) 行" >> $L
# 3) 交叉验证：已复现的 vreg-rename（71 KB）—— 用它检验"RE 伪代码 vs 我们已有实现"
timeout 1800 $R/ghidra_decompile.sh $B/TX8RegisterRename.cpp.o $R/out/vregrename >> $L 2>&1
echo "vregrename rc=$? $(wc -l < $R/out/vregrename/decompiled.c 2>/dev/null) 行" >> $L
echo "=== [$(date -Is)] 结束 ===" >> $L
