mkdir -p build
cd build
cmake -G Ninja                                     \
      -DCMAKE_BUILD_TYPE=Release                   \
      -DLLVM_TARGETS_TO_BUILD="RISCV;MC_RISCV"     \
      -DLLVM_TOOLS_TO_BUILD="llvm-mc"              \
      -DLLVM_ENABLE_TERMINFO=OFF                   \
      ../llvm
ninja -j8  llvm-mc
cd ..