add_llvm_component_library(LLVMNovaDesc
  NovaMCTargetDesc.cpp
  NovaMCAsmInfo.cpp
  NovaMCInstPrinter.cpp

  LINK_COMPONENTS
  MC
  TargetParser
  LLVM
  Target

  ADD_TO_COMPONENT
  Nova
)