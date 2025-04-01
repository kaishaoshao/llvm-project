# 定义名为RISCVCPU组件组，方便在LLVM构建系统中对相关组件进行统一管理和构建 
add_llvm_component_group(RISCVCPU)

# 用于在LLVM构建系统中定义目标架构的描述文件
set(LLVM_TARGET_DEFINITIONS RISCVCPU.td)
# 使用TableGen工具生成与目标架构相关的子目标信息文件
tablegen(LLVM RISCVCPUGenSubtargetInfo.inc -gen-subtarget)

# 使用TableGen工具生成与目标架构相关的指令选择信息文件
tablegen(LLVM RISCVCPUGenDAGISel.inc -gen-dag-isel)

# 目标标记为公共的TableGen目标，使其可在其他模块中使用
add_public_tablegen_target(RISCVCPUCommonTableGen)

add_llvm_target(RISCVCPUCodeGen
    RISCVCPUTargetMachine.cpp
    RISCVCPUDAGToDAGISel.cpp
)

# 定义目标的基本信息
add_subdirectory(TargetInfo)
# 生成目标机器代码的描述和工具。
add_subdirectory(TargetDesc)