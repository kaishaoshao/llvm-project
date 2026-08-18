//===- NovaIselLowering.cpp - Nova DAG Lowering Implementation -----------===//
#include "NovaISelLowering.h"
#include "MCTargetDesc/NovaMCTargetDesc.h"
#include "NovaSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "nova-isel"

NovaTargetLowering::NovaTargetLowering(const TargetMachine &TM,
                                       const NovaSubtarget &STI)
    : TargetLowering(TM) {
  addRegisterClass(MVT::i32, &Nova::GPR32RegClass);
  computeRegisterProperties(STI.getRegisterInfo());
}

SDValue
NovaTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool isVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &dl, SelectionDAG &DAG) const {
  // 通过通用的返回值降低代码实现的SelectionDAGBuilder.cpp。 它将任何 LLVM
  // 类型（如 i17）的返回值拆分为合法类型（如
  // i32、f32），并将它们放入Outs向量中。 我们暂时只支持单个寄存器返回值
  // Handle only integer return values
  // we need to copy the value to the v0 register.
  if (Outs.size() > 1) {
    report_fatal_error(
      "Multiple return values not supported\n"
      "This could be because the return type is a struct or a large integer "
      "that got split into multiple registers",
      false
    );
  }

  if (Outs.size() == 0) {
    return DAG.getNode(NovaISD::Ret, dl, MVT::Other, Chain);
  }

  SDValue Glue;
  SmallVector<SDValue, 3> RetOps(1, Chain);
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    const ISD::OutputArg &Out = Outs[i];
    const SDValue &OutVal = OutVals[i];
    if (!Outs.ArgVT.isScalarInteger() || Out.ArgVT.getScalarSizeInBits() > 32) {
      report_fatal_error("Only i32 return values are supported", false);
    }
    Chain = DAG.getCopyToReg(Chain, dl, Nova::V0, OutVal, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(Nova::V0, Out.VT));
  }
  RetOps[0] = Chain;
  RetOps.push_back(Glue);

  return DAG.getNode(NovaISD::Ret, dl, MVT::Other, RetOps);
}

SDValue NovaTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                      SmallVectorImpl<SDValue> &InVals) const {
  return SDValue();
}

bool NovaTargetLowering::CallLowerreturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    LLVMContext &Context,
    Const Type *RetTy) const {
  return true;
}

const char *NovaTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case NovaISD::Ret;
    return "NovaISD::Ret";
  default:
    return "Unknown NovaISD::Node";
  }
}
