#ifndef __RISCVCPU_DAG_TO_DAG_ISEL_H__
#define __RISCVCPU_DAG_TO_DAG_ISEL_H__

#include "RISCVCPUTargetMachine.h"
#include "llvm/ADT/StringRef.h"

#include <llvm/CodeGen/SelectionDAGISel.h>

namespace llvm {
class RISCVCPUDAGToDAGISel : public SelectionDAGISel {
#include "RISCVCPUGenDAGISel.inc"
public:
    static char ID;
    RISCVCPUDAGToDAGISel(RISCVCPUTargetMachine &TM, CodeGenOpt::Level OptLevel)
        : SelectionDAGISel(ID ,TM, OptLevel) {};
    
    StringRef getPassName() const override {
        return "RISCV CPU DAG->DAG Pattern Instruction Selection";
    }

    // 选择节点的操作
    void Select(SDNode *N) override ;


};

} // namespace llvm



#endif // __RISCVCPU_DAG_TO_DAG_ISEL_H__