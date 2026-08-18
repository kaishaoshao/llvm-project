#ifndef LLVM_LIB_TARGET_NOVA_ASMPRINTER_H
#define LLVM_LIB_TARGET_NOVA_ASMPRINTER_H

#include "Nova.h"
#include "NovaSubtarget.h"
#include "NovaTargetInfo.h"
#include "NovaTargetMachine.h"
#include "MCTargetDesc/NovaMCInstPrinter.h"

#define DEBUG_TYPE "nova-asm-printer"

using namespace llvm;

namespace {
class NovaAsmPrinter : public AsmPrinter {
public:
  NovaAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  void emitInstruction(const MachineInstr *MI) override;

  // lower the MachineInstr to MCInst
  void lowerInstruction(const MachineInstr &MI, MCInst &Inst);

  // bool lowerPseudoInstExpansion(const MachineInstr *MI, MCInst &Inst);

private:
  MCOperand NovaAsmPrinter::lowerSymbolOperand(const MachineOperand &MO,
                                               MCSymbol *Sym);
};
} // namespace

#endif // LLVM_LIB_TARGET_NOVA_ASMPRINTER_H
