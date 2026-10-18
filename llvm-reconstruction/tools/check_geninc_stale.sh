#!/bin/bash
# 检查 <tree>/src/lib/Target/RISCV 下有没有 .o 比 RISCVGenInstrInfo.inc 旧 —— 这类"旧对象+新枚举"
# 不会表现为"pass 没跑"，而是 AsmPrinter 断言/垃圾指令（2026-10-17 由 control-flow 线发现）。
# 用法: check_geninc_stale.sh <tree> [build]
set -u
TREE=${1:?usage: $0 <tree> [build]}; BUILD=${2:-$TREE/build}
INC=$(find "$BUILD" -name 'RISCVGenInstrInfo.inc' 2>/dev/null | head -1)
[ -z "$INC" ] && { echo "  (no RISCVGenInstrInfo.inc under $BUILD)"; exit 0; }
OLD=$(find "$BUILD/lib/Target/RISCV" -name '*.o' ! -newer "$INC" 2>/dev/null)
if [ -n "$OLD" ]; then
  echo "  ⚠ 有 $(echo "$OLD" | wc -l) 个对象比 $(basename $INC) 旧（opcode-enum 错位风险）："
  echo "$OLD" | sed 's|.*/llvm/lib/Target/RISCV/|    |' | head -12
  exit 1
fi
echo "  ✓ 无对象比 RISCVGenInstrInfo.inc 旧"
