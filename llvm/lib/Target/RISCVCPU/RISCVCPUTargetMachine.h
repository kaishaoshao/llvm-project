#ifndef __RISCVCPU_TARGET_MACHINE_H__
#define __RISCVCPU_TARGET_MACHINE_H__

#include "llvm/Target/TargetMachine.h"

///
///
/// | 参数	                         |    影响范围	       |  典型值示例
/// | Target const &T	            | 目标架构和名称       | TheRISCVTarget
/// | Triple const &TT              | 目标平台特性	       | riscv64-unknown-linux-gnu
/// | StringRef CPU	                | CPU型号和指令集扩展   | 	generic-rv64
/// | StringRef FS	                | 目标特性字符串	   | +m,+a,+f
/// | TargetOptions const &Options  | 高级代码生成选项	   | TargetOptions()
/// | Reloc::Model RM               | 重定位模型	      | Reloc::Static
/// | CodeModel::Model CM           | 代码模型	          | CodeModel::Small
/// | CodeGenOpt::Level OL          | 优化级别	          | CodeGenOpt::Default
/// | bool JIT	                    | JIT编译标志         | false
///

namespace llvm {
class RISCVCPUTargetMachine : public LLVMTargetMachine {
public:
    /// @brief RISCVCPUTargetMachine 是LLVM中用于表示RISCVCPU目标机器的类，
    ///                 其构造函数参数用于配置目标机器的各种属性 
    /// @param T        表示目标机器的基本描述信息 如 riscvcpu
    /// @param TT       表示目标三元组信息 如 riscv64-unknown-linux-gnu
    /// @param CPU      指定目标CPU的具体型号或特性 如 sifive-u74 
    ///                 用于优化代码生成（如启用特定CPU的指令调度策略
    /// @param FS       指定目标平台的特性字符串（Feature String），控制目标平台的指令集扩展
    ///                 启用或禁用某些目标特性（如+c表示压缩指令，-m表示禁用乘法指令）
    /// @param Options  配置目标机器的高级选项： 是否使用软件浮点模拟 PIE POSIC  控制代码生成的高级行为
    /// @param RM       指定重定位模型（Relocation Model）控制生成的代码是否支持动态链接
    /// @param CM       指定代码模型（Code Model） CodeModel::Small：小代码模型（默认）
    /// @param OL       指定代码生成优化级别  CodeGenOpt::Default：默认优化
    /// @param JIT      指定目标机器是否用于JIT（Just-In-Time）编译
    RISCVCPUTargetMachine(Target const &T, Triple const &TT, 
            StringRef CPU, StringRef FS, TargetOptions const &Options,
            std::optional<Reloc::Model> RM, std::optional<CodeModel::Model> CM,
            CodeGenOpt::Level OL, bool JIT);
    ~RISCVCPUTargetMachine() {};
        
    TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

};

}


#endif // __RISCVCPU_TARGET_MACHINE_H__