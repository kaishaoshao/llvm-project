# 定义一个名为 MipsNano 的组件组，用于逻辑上组织相关的代码生成模块
add_llvm_component_group(Nano)

#
add_llvm_target(NanoCodeGen # 生成的代码目标名称，对应一个静态或动态库，负责 MIPS Nano 架构的代码生成逻辑
    NanoTargetMachine.cpp


    LINK_COMPONENTS
    # 处理机器码的生成与反汇编，提供指令编码、节（Section）管理等底层功能
    MC
    # 解析目标架构名称（如 mipsnano）并映射到对应的 Target 实现
    TargetParser
    # 目标架构的公共接口，定义目标相关的抽象基类（如 TargetMachine标架构的公共接口
    Target

    # 将当前目标 MipsNanoCodeGen 注册到之前定义的 MipsNano 组件组中，
    # 使其他模块可通过组件组名引用其功能
    ADD_TO_COMPONENT
    Nano
)

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
