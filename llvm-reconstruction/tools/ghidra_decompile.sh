#!/bin/bash
# 用法: ghidra_decompile.sh <obj.o> <输出目录> [函数名过滤]
# 无 root；用工作区里的 JDK + Ghidra headless 导反编译伪代码与调用图
set -u
R=/home/shaokai/Desktop/work/tx82/tools/re
export JAVA_HOME=$R/jdk; export PATH=$JAVA_HOME/bin:$PATH
OBJ=$1; OUT=$2; FILTER=${3:-}
mkdir -p "$OUT" "$OUT/proj"
SCRIPT=$R/ExportDecomp.java
cat > $SCRIPT <<'JAVA'
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class ExportDecomp extends GhidraScript {
  public void run() throws Exception {
    String out = System.getenv("DECOMP_OUT");
    String filter = System.getenv("DECOMP_FILTER");
    DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
    PrintWriter pw = new PrintWriter(new FileWriter(out + "/decompiled.c"));
    PrintWriter cg = new PrintWriter(new FileWriter(out + "/callgraph.txt"));
    FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
    while (it.hasNext()) {
      Function f = it.next();
      String n = f.getName();
      if (filter != null && !filter.isEmpty() && !n.contains(filter)) continue;
      pw.println("/* ==== " + n + " @ " + f.getEntryPoint() + " ==== */");
      DecompileResults r = di.decompileFunction(f, 60, monitor);
      if (r != null && r.decompileCompleted()) pw.println(r.getDecompiledFunction().getC());
      else pw.println("// decompile failed");
      pw.println();
      for (Function c : f.getCalledFunctions(monitor)) cg.println(n + " -> " + c.getName());
    }
    pw.close(); cg.close();
  }
}
JAVA
DECOMP_OUT=$OUT DECOMP_FILTER=$FILTER \
  $R/ghidra/support/analyzeHeadless "$OUT/proj" tx82 -import "$OBJ" \
  -scriptPath $R -postScript ExportDecomp.java -deleteProject 2>&1 | tail -5
echo "输出: $OUT/decompiled.c  $OUT/callgraph.txt"
