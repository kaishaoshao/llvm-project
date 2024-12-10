#!/bin/bash
# 获取当前脚本的绝对路径
SCRIPT_PATH="$(readlink -f "$0")"
# 获取脚本所在的目录
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

# 判断当前目录下是否有 build 目录，没有则新建
if [ ! -d "$SCRIPT_DIR/build" ]; then
    mkdir "$SCRIPT_DIR/build" || exit
fi

# 配置 CMake 并生成构建文件
cmake -B "$SCRIPT_DIR/build"          \
    -DCMAKE_BUILD_TYPE=Debug          \
    -DBUILD_SHARED_LIBS=ON            \
    -DLLVM_USE_LINKER=lld             \
    -DLLVM_ENABLE_PROJECTS="clang"    \
    -DLLVM_TARGETS_TO_BUILD="RISCV"   \
    -DLLVM_OPTIMIZED_TABLEGEN=ON      \
    -GNinja "$SCRIPT_DIR/llvm"        \
    && ninja -C "$SCRIPT_DIR/build" -j8