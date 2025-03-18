# 定义名为RISCVCPU组件组，方便在LLVM构建系统中对相关组件进行统一管理和构建 
add_llvm_component_group(RISCVCPU)

add_llvm_target(RISCVCPUCodeGen
    RISCVCPUTargetMachine.cpp
)

# 定义目标的基本信息
add_subdirectory(TargetInfo)
# 生成目标机器代码的描述和工具。
add_subdirectory(TargetDesc)