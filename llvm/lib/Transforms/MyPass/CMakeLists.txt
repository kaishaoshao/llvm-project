add_llvm_component_library(LLVMMyPass
  MyPass.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/MyPass

  LINK_COMPONENTS
  Core
  Support
)