add_llvm_target(LLVMNovaTargetInfo
  NovaTargetInfo.cpp

  LINK_COMPONENTS
  MC
  TargetParser
  LLVM
  Target

  ADD_TO_COMPONENT
  Nova
)
