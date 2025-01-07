opt=../build/bin/opt 
clang=../build/bin/clang
ourpass=../build/lib/LLVMOurPass.so

echo "***************find the ourPass**************"
$opt -load $ourpass -help | grep "ourpass"

echo "*********Create hello_world.c to /tmp********"
echo "
#include <stdio.h>

void print_func(){
    printf(\"Hello, world\n\");
} 

int main(){
    print_func();
    return 0;
}
" > /tmp/hello_world.c
cat /tmp/hello_world.c 

echo "*************Create the hello_world.bc***********"
$clang -target x86_64-unknown-linux-gnu -emit-llvm -c /tmp/hello_world.c -o /tmp/hello_world.bc

echo "*************Run ourpass on hello_world.bc***********"
# --mtriple=x86_64-unknown-linux-gnu 
$opt --mtriple=riscv64-unknown-elf  -load-pass-plugin $ourpass -passes=ourpass -time-passes < hello_world.bc > /tmp/null

