clang ./riscvcpu_test/test.c -c -emit-llvm -O0 -o /tmp/test.bc
llvm-dis /tmp/test.bc
cat /tmp/test.ll

./build/bin/llc /tmp/test.bc -march=riscvcpu

# 查看所使用的pass
# ./build/bin/llc -debug-pass=Structure /tmp/test.ll -march=riscvcpu -o /tmp/test.s

