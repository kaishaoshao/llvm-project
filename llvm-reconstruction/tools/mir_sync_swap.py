#!/usr/bin/env python3
"""Swap the (index, base) operands of `_SYNC_` pseudos in a MIR module.

This tree's `XTsm_*_SYNC_V*` records declare `(VR:$vs2, GPRMemZeroOffset:$rs1)`
with the *index first*, because `RISCVAsmPrinter::lowerRISCVVMachineInstrToMCInst`
copies the leading explicit operands positionally onto the base instruction
(see RISCVInstrInfoXTsm82s.td); RISCVISelDAGToDAG.cpp therefore swaps base and
index when it builds them.  The vendor's pseudos keep the upstream
(base, index) order, so replaying the vendor's MIR through our `llc` needs the
two operands swapped back.  Only the 2nd and 3rd printed operands are touched
(store: data, base, index, ...; load: passthru, base, index, ...).

usage: mir_sync_swap.py <file.mir>   (writes the result to stdout)
"""
import re
import sys

src = sys.argv[1]
out = []
with open(src, errors="replace") as f:
    for line in f:
        s = line.rstrip("\n")
        if "_SYNC_V_" in s and "Pseudo" in s:
            m = re.match(r"^(\s*)(.*?)(\s*::.*|\s*)$", s)
            if m:
                indent, body, tail = m.group(1), m.group(2), m.group(3)
                mm = re.match(r"^(.*?=\s*)?(\S+)\s+(.*)$", body)
                if mm:
                    pre = mm.group(1) or ""
                    name = mm.group(2)
                    parts = [p.strip() for p in mm.group(3).split(",")]
                    if len(parts) >= 3:
                        parts[1], parts[2] = parts[2], parts[1]
                    s = indent + pre + name + " " + ", ".join(parts) + tail
        out.append(s)
sys.stdout.write("\n".join(out) + "\n")
