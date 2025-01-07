llvm_add_library(SimpleModulePass MODULE
    SimpleModulePass.cpp

    PLUGIN_TOOL
    opt
)