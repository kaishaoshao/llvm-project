#include "RISCVCPUTargetMachine.h"

#include <llvm/MC/TargetRegistry.h>
#include "RISCVCPUDAGToDAGISel.h"
#include <llvm/CodeGen/TargetPassConfig.h>

using namespace llvm;

extern Target TheRISCVCPUTarget;


///  用于初始化RISCV CPU目标
extern "C" void LLVMInitializeRISCVCPUTarget() {
    RegisterTargetMachine<RISCVCPUTargetMachine> X(TheRISCVCPUTarget);
}

RISCVCPUTargetMachine::RISCVCPUTargetMachine(Target const &T, 
        Triple const &TT, StringRef CPU, StringRef FS,
        TargetOptions const &Options, std::optional<Reloc::Model> RM,
        std::optional<CodeModel::Model> CM, CodeGenOpt::Level OL,
        bool JIT) :  // 这里的参数主要是 llc 传递过来的命令行参数 
        LLVMTargetMachine(T, "e-m:m-p:32:32-i8:8:32-i16:16:32-i64:64-n32-S64", 
            TT, CPU, FS, Options, Reloc::Static, CodeModel::Small, OL) {
        initAsmInfo();
}

class RISCVCPUPassConfig : public TargetPassConfig {
public:
    RISCVCPUPassConfig(RISCVCPUTargetMachine &TM, PassManagerBase &PM) :
        TargetPassConfig(TM, PM) {}
    
    bool addInstSelector() override {
        // 添加指令选择器
        addPass(new RISCVCPUDAGToDAGISel(getTM<RISCVCPUTargetMachine>(), getOptLevel()));
        return false;
    }

};


TargetPassConfig *RISCVCPUTargetMachine::createPassConfig(PassManagerBase &PM) {
    return new RISCVCPUPassConfig(*this, PM);
}