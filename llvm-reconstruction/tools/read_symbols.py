#!/usr/bin/env python3
"""缺口符号预读：厂商 llvm-objdump 反汇编（认识 TX8）+ PLT→符号名解析 + Ghidra 结构。
用法: read_symbols.py <kernel.so> <符号名...>
"""
import re, subprocess, sys, os
V="/home/shaokai/Desktop/work/tx82/llvm-toolchain/bin"
so = sys.argv[1]; want = sys.argv[2:]
def sh(a): return subprocess.run(a, capture_output=True, text=True).stdout
# PLT 映射：.plt 表头 32 字节 + 每个 16 字节 stub 对应第 i 个 JUMP_SLOT
sec = sh([f"{V}/llvm-readelf","-S",so])
addr = int(re.search(r'\]\s+\.plt\s+\S+\s+([0-9a-f]+)', sec).group(1), 16)
syms = [l.split()[-3] for l in sh([f"{V}/llvm-readelf","-r",so]).splitlines() if 'JUMP_SLOT' in l]
plt = {addr + 32 + i*16: s for i, s in enumerate(syms)}
dis = sh([f"{V}/llvm-objdump","-dr","--no-show-raw-insn",so])
def resolve(line):
    m = re.search(r'(jal|j)\s+0x([0-9a-f]+)\s+<\.plt\+0x[0-9a-f]+>', line)
    if m:
        t = int(m.group(2),16)
        if t in plt: return line.split('\t')[0] + f"\t{m.group(1)}\t{plt[t]}   <-- 命中 PLT"
    return line
for w in want:
    i0 = dis.find(f"<{w}>:")
    print(f"\n===== {w} =====")
    if i0 < 0: print("  （该内核里没有这个符号）"); continue
    body = dis[i0:].split("\n\n")[0].splitlines()
    for l in body[:60]:
        if l.strip(): print("  " + resolve(l).split("\t",1)[-1])
