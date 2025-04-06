#ifndef __RISCVCPU_INST_PRINTER_H__
#define __RISCVCPU_INST_PRINTER_H__

#include "llvm/MC/MCInst.h"

#include <llvm/MC/MCRegister.h>
#include <llvm/MC/MCInstPrinter.h> 
#include <llvm/Support/raw_ostream.h>


namespace llvm {

class RISCVCPUInstPrinter : public MCInstPrinter {
public:
    RISCVCPUInstPrinter(MCAsmInfo const &MAI, MCInstrInfo const &MII, 
                       MCRegisterInfo const &MRI)
        : MCInstPrinter(MAI, MII, MRI) {}

    // 
    void printInst(MCInst const *MI, uint64_t Address, StringRef Annot,
                   MCSubtargetInfo const &STI, raw_ostream &O) override;

    void printRegName(raw_ostream &OS, MCRegister reg) const override;

    //
    void printOperand(MCInst const *MI, unsigned OpNo, raw_ostream &O);
    void printInstList(MCInst const *MI, uint64_t Address, unsigned OpNum, raw_ostream &O);


    // define in RISCVCPUGenAsmWriter.inc
    std::pair<char const *, uint64_t> getMnemonic(MCInst const *MI) override;
    void printInstruction(MCInst const *MI, uint64_t Address, raw_ostream &OS);
    static char const *getRegisterName(MCRegister RegNo);

    bool printAliasInstr(MCInst const *MI, uint64_t Address, raw_ostream &OS);
};

} // namespace llvm


#endif // __RISCVCPU_INST_PRINTER_H__