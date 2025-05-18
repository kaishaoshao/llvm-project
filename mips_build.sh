mkdir -p build
cmake -S llvm -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_BUILD_TESTS=ON \
      -DLLVM_PARALLEL_LINK_JOBS=8 \
      -DLLVM_TARGETS_TO_BUILD='ARM;Mips;Nano' # Nova won't work yet

cmake --build build --target llc 
