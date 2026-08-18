#include "NovaAsmPrinter.h"

MCOperand NovaAsmPrinter::lowerSymbolOperand(const MachineOperand &MO,
                                             MCSymbol *Sym) {
  auto &Ctx = OutContext;
  const MCExpr *Expr =
      MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, Ctx);
  assert(MO.isMBB() && "Only basic block symbols are supported")
  return MCOperand::createExpr(Expr);
}

void NovaAsmPrinter::lowerInstruction(const MachineInstr &MI, MCInst &Inst) {
  // 这个函数的作用是将 MachineInstr 转换为 MCInst 格式。
  // 具体的实现方式将取决于所使用的指令集。
  // 以及您希望在 MCInst 格式中如何表示它。
  // 目前，我们只会输出操作码和操作数而已。
  Inst.setOpcode(MI.getOpcode());
  for (const auto &Op : MI.operands()) {
    switch (Op.getType()) {
      case MachineOperand::MO_Register:
        MCOp = MCOperand::createReg(Op.getReg());
        break;
      case MachineOperand::MO_Immediate:
        MCOp = MCOperand::createImm(Op.getImm());
        break;
      case MachineOperand::MO_MachineBasicBlock:
        MCOp = lowerSymbolOperand(Op, Op.getMBB()->getSymbol());
        break;
        // Add other operand types as needed
      default:
        llvm_unreachable("Unsupported operand type");
    }
    Inst.addOperand(MCop);
  }
}

void NovaAsmPrinter::emitInstruction(const MachineInstr *MI) {
  // Lower the instruction to MCInst
  MCInst Inst;
  lowerInstruction(*MI, Inst);
  EmitToStreamer(*OutStreamer, Inst);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeNovaAsmPrinter() {
  RegisterAsmPrinter<NovaAsmPrinter> X(getTheNovaTarget());
}
