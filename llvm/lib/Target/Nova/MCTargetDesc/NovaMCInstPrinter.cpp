#include "NovaMCInstPrinter.h"
#include "NovaInstrInfo.h"
#include "llvm/MC/MCInst.h"

#define DEBUG_TYPE "nova-mcinst-printer"

using namespace llvm;

#define PRINT_ALIAS_INSTR

#include "NovaGenAsmWriter.inc"

using namespace llvm;

#define PRINT_ALIAS_INSTR
#include "NovaGenAsmWriter.inc"

// 需要打印指令的别名，这由函数处理
void NovaInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCStutargetInfo &STI,
                                raw_ostream &O) {
  // check if we have an alias
  if (!printAliasInstr(MI, Address, O)) {
    printInstruction(MI, Address, O);
  }
  printAnnotation(O, Annot);
}

void NovaInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << "$" << StringRef(getRegisterName(Reg)).lower();
}

void NovaInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);

  if (Op.isReg()) {
    printRegName(O, Op.getReg());
    return;
  }

  assert(Op.isExpr() && "unknown operand type");
  Op.getExpr()->print(O, &MAI, true);
}


