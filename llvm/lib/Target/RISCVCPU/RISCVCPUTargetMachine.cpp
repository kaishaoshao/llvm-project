#include "RISCVCPUTargetMachine.h"
#include "RISCVCPUDAGToDAGISel.h"

#include <llvm/MC/TargetRegistry.h>
#include <llvm/CodeGen/TargetPassConfig.h>

using namespace llvm;

extern Target TheRISCVCPUTarget; // 声明全局的 RISC-V 目标对象

/// RISC-V 目标初始化函数（由 LLVM 自动调用）
/// 作用：向 LLVM 目标注册表注册 RISC-V 目标机器[1,4](@ref)
extern "C" void LLVMInitializeRISCVCPUTarget() {
    // 将 RISCVCPUTargetMachine 类与全局目标对象绑定
    RegisterTargetMachine<RISCVCPUTargetMachine> X(TheRISCVCPUTarget);
}

/// RISC-V 目标机器构造函数
/// 参数说明：
///   T: LLVM 目标描述对象
///   TT: 三元组（如 riscv32-unknown-elf）
///   CPU: CPU 型号（如 rv32imac）
///   FS: 特性字符串（如 +m,+a,+c）
///   Options: 目标选项（如线程模型、ABI 等）
///   RM: 重定位模型（默认为静态 Reloc::Static）
///   CM: 代码模型（默认为小模型 CodeModel::Small）
///   OL: 优化级别（如 O0/O2/O3）
RISCVCPUTargetMachine::RISCVCPUTargetMachine(
    Target const &T, Triple const &TT, StringRef CPU, StringRef FS,
    TargetOptions const &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, 
        // 数据布局字符串（定义硬件特性）[4](@ref)
        "e-m:m-p:32:32-i8:8:32-i16:16:32-i64:64-n32-S64", 
        TT, CPU, FS, Options, 
        Reloc::Static,   // 默认静态重定位（适合裸机环境）
        CodeModel::Small,// 小代码模型（代码/数据地址在32位范围内）
        OL) {
    initAsmInfo(); // 初始化汇编器信息（如指令格式、寄存器名称等）[2](@ref)
}

/// RISC-V 编译流程配置类（继承自 TargetPassConfig）
class RISCVCPUPassConfig : public TargetPassConfig {
public:
    RISCVCPUPassConfig(RISCVCPUTargetMachine &TM, PassManagerBase &PM)
        : TargetPassConfig(TM, PM) {}
    
    /// 添加指令选择阶段（核心编译阶段）
    bool addInstSelector() override {
        // 创建 DAG 到 DAG 的指令选择器[2](@ref)
        // 作用：将 LLVM IR 的 SelectionDAG 转换为 RISC-V 机器指令的 DAG
        addPass(new RISCVCPUDAGToDAGISel(
            getTM<RISCVCPUTargetMachine>(), // 获取目标机器实例
            getOptLevel()));                 // 传递优化级别（影响指令选择策略）
        return false;
    }
};

/// 创建 Pass 配置对象（由 LLVM 编译流程调用）
TargetPassConfig* RISCVCPUTargetMachine::createPassConfig(PassManagerBase &PM) {
    // 返回自定义的 Pass 配置，控制编译流水线阶段
    return new RISCVCPUPassConfig(*this, PM);
}