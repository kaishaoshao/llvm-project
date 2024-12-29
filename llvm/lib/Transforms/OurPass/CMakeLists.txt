llvm_add_library( LLVMOurPass MODULE
  OurPass.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )