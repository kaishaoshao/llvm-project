llvm_add_library(LLVMOurPass MODULE
  OurPass.cpp
  
  PLUGIN_TOOL
  opt
  )