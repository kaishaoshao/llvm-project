// 管具体指令和pattern
include "CoralNPUInstrFormats.td"

def CoralNPUInstrInfo : InstrInfo;

def RET : CoralNPUInst {
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "ret";
  let Pattern = [];
}
