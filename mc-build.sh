mkdir -p build
cd build
cmake -G Ninja                                     \
      -DCMAKE_BUILD_TYPE=Release                   \
      -DLLVM_ENABLE_PROJECTS="clang"               \
      -DLLVM_TARGETS_TO_BUILD="X86;RISCV;MC_RISCV" \
      -DLLVM_ENABLE_TERMINFO=OFF                   \
      ../llvm
ninja -j8  llvm-mc
cd ..