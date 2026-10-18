#!/bin/bash
# rebuild_one.sh <tree> <绝对 src 路径> [build]
# 2026-10-17 三修：
#  (1) 旧版硬编码别的树 ⇒ 必须显式给 tree，且 src 必须在 tree 下；
#  (2) --force-src 对"依赖记录指向旧树路径（如 riscv-llvm-musl-toolchain/...）"的对象会静默 0 匹配 ⇒
#      这里同时传 --force-objs <basename>.o（按 basename 子串匹配），并断言"重编数 > 0"；
#  (3) .ninja_deps 里没有条目的对象（如 TX8TransformSIMTToSIMD.cpp.o）连 --force-objs 都看不见 ⇒
#      报错时提示用 `ninja -t commands <obj> | tail -1` 取裸命令。
set -u
TREE=${1:?usage: $0 <tree> <abs-src-path> [build]}; SRC=${2:?usage: $0 <tree> <abs-src-path> [build]}
BUILD=${3:-$TREE/build}
case "$SRC" in "$TREE"/*) : ;; *) echo "  ✗ src 不在 tree 下，拒绝（可能写错树）" >&2; exit 2;; esac
[ "${SRC#/}" = "$SRC" ] && { echo "  ✗ src 必须绝对路径" >&2; exit 2; }
OBJ=$(basename "$SRC").o
[ -f "$TREE/logs/changed_inc.json" ] && echo '[]' > "$TREE/logs/changed_inc.json"
echo "  tree=$TREE obj=$OBJ"
OUT=$(python3 "$TREE/tools/minibuild.py" --force-src "$SRC" --force-objs "$OBJ" --steps objs,libs,link 2>&1); RC=$?
echo "$OUT" | tail -6
if [ $RC -ne 0 ] || ! echo "$OUT" | grep -qE 'ok .*'"$OBJ"'|objects? to rebuild: [1-9]|-> [1-9][0-9]* object'; then
  echo "  ⚠ 可能 0 匹配：若真是 0，用下面的裸命令：" >&2
  echo "    cd $BUILD && ninja -t commands lib/Target/RISCV/CMakeFiles/LLVMRISCVCodeGen.dir/$OBJ | tail -1" >&2
  echo "    cd $BUILD && ninja -t commands lib/libLLVMRISCVCodeGen.a | tail -1   # 归档" >&2
  echo "    cd $BUILD && ninja -t commands bin/clang-21 | tail -1                  # 链接" >&2
  exit 3
fi
echo "  ✓ 已重编并重链"
