# 是用于定义一个LLVM库的CMake函数。用于创建和管理LLVM项目中的静态库或共享库
add_llvm_library(LLVMRISCVCPUInfo
    RISCVCPUTargetInfo.cpp
)
