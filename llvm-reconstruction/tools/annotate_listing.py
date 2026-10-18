#!/usr/bin/env python3
"""把「厂商工具链反汇编（认识 TX8 私有指令）」与「Ghidra 的结构/调用图」合成逐函数清单。
用法: annotate_listing.py <elf> <outdir> [函数名过滤]
输出: <outdir>/annotated.md  —— 每个函数: 地址/大小/解码后的指令 + 调用标注
说明: 语义读指令用厂商 llvm-objdump（Ghidra 不认 TX8 会截断）；结构用 Ghidra（callgraph.txt）。
"""
import re, subprocess, sys, os, shutil
V = "/home/shaokai/Desktop/work/tx82/llvm-toolchain/bin"
PYCMAKE = None
elf, outdir = sys.argv[1], sys.argv[2]
filt = sys.argv[3] if len(sys.argv) > 3 else ""
os.makedirs(outdir, exist_ok=True)
objdump = os.path.join(V, "llvm-objdump")
assert os.path.exists(objdump), f"厂商 objdump 不存在: {objdump}"   # 防假绿：先确认工具在
# 1) 反汇编（-r 解析重定位，让 .o 的调用目标可见）
dis = subprocess.run([objdump, "-dr", "--no-show-raw-insn", elf],
                     capture_output=True, text=True).stdout
# 2) 符号表（定义 + 未定义，用于把调用目标标注成名字）
nm = subprocess.run([os.path.join(V, "llvm-nm"), "-C", "--defined-only", elf],
                    capture_output=True, text=True).stdout
addr2name = {}
for line in nm.splitlines():
    p = line.split()
    if len(p) >= 3:
        try: addr2name[int(p[0], 16)] = " ".join(p[2:])
        except ValueError: pass
# 3) 解析成函数
funcs, cur = [], None
for line in dis.splitlines():
    m = re.match(r'^([0-9a-f]+) <(.+)>:$', line)
    if m:
        cur = {"addr": int(m.group(1), 16), "name": m.group(2), "insn": []}
        funcs.append(cur); continue
    if cur is not None and line.strip():
        cur["insn"].append(line.rstrip())
# 4) Ghidra 调用图（若已产出）
cg = {}
cgf = os.path.join(outdir, "callgraph.txt")
if os.path.exists(cgf):
    for line in open(cgf):
        if " -> " in line:
            a, b = line.strip().split(" -> ", 1); cg.setdefault(a, []).append(b)
# 5) 输出
out = [f"# {os.path.basename(elf)} —— 厂商反汇编 + Ghidra 结构", "",
       f"- 反汇编: 厂商 llvm-objdump（认识 TX8 私有指令；行数 {len(dis.splitlines())}）",
       f"- 函数数: {len(funcs)}", ""]
for f in funcs:
    if filt and filt not in f["name"]: continue
    out.append(f"## {f['name']} @ {f['addr']:x}  ({len(f['insn'])} 条)")
    calls = sorted({t for i in f["insn"] for t in re.findall(r'<([^>]+)>', i)})
    if calls: out.append("调用的符号: " + ", ".join(calls))
    if cg.get(f["name"]): out.append("Ghidra 调用图: " + ", ".join(sorted(set(cg[f["name"]]))[:12]))
    out.append("```asm"); out += ["  " + i.split("\t", 1)[-1] for i in f["insn"][:400]]; out.append("```"); out.append("")
open(os.path.join(outdir, "annotated.md"), "w").write("\n".join(out))
print(f"写出 {outdir}/annotated.md（{len(funcs)} 函数，过滤='{filt}'）")
