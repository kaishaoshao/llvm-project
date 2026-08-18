// 添加 LLVM IR 映射到的 SDNode ret

include "NovaInstrFormats.td"

//==--------- All SD nodes for Nova --------------------===//
def NovaRetSDN : SDNode<"NovaISD""Ret",
                         SDTNone, // 0 result and 0 operands
                         [SDNPHasCHain, SDNVariadic, SDNPOptInGlue]>;
//==---------- End SD Node definitions ----------------===//


// 该节点将被选中执行PseudoRet指令。
//==----------- Nova Instruction Definitions ----------===//
def PseudoRet : PseudoNovaInst<(outs), (ins), "ret"> {
  let isReturn = 1;
  let isTerminator = 1;
}
//==--------- End Nova Instruction Definitions --------===//

// 添加用于选择节点的模式NovaISD::Ret
//==---- All patterns to match SD nodes ----------------==//
def Pat<(NovaRetSDN), (PseudoRet)>;

def callseq_end : SDNode<"ISD::CALLSEQ_END", SDTNone, [
  SDNPHasCHain, SDNPOptInGlue
]>;

def ADJCALLSTACKDOWN : Instruction {
  let OutOperandList = (outs);
  let Namespace = "Nova";
  let InOperandList = (ins);
  let AsmString = "ADJCALLSTACKDOWN";
  let Pattern = [(callseq_end)];
}


def ADJCALLSTACKUP : Instruction {
  let OutOperandList = (outs);
  let Namespace = "Nova";
  let InOperandList = (ins);
  let AsmString = "ADJCALLSTACKUP";
  let Pattern = [(callseq_end)];
}

//==---- end All patterns to match SD nodes -----------==//
