#ifndef __RISCVCPU_ASM_PRINTER_H__
#define __RISCVCPU_ASM_PRINTER_H__

#include "RISCVCPUTargetMachine.h"
#include <llvm/CodeGen/AsmPrinter.h>
#include <llvm/MC/MCStreamer.h>


namespace llvm {
class RISCVCPUAsmPrinter : public AsmPrinter {
public:
    RISCVCPUAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
        : AsmPrinter(TM, std::move(Streamer)) {}
    /**
     * @brief 获取当前Pass的名称。
     * 
     * 该函数是一个虚函数，用于返回当前Pass的名称。在这个实现中，它返回一个表示
     * "RISCVCPU Assembly Printer"的字符串引用。该函数通常用于调试或日志记录，
     * 以便识别当前正在执行的Pass。
     * 
     * @return StringRef 返回一个字符串引用，表示当前Pass的名称。
     */
    virtual StringRef getPassName() const override {
        return "RISCVCPU Assembly Printer";
    }
};

} // namespace llvm


#endif // __RISCVCPU_ASM_PRINTER_H__