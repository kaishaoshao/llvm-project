add_llvm_component_library(LLVMMC_RISCVAsmParser
  RISCVAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Support
  TargetParser
  MC_RISCVDesc
  MC_RISCVInfo

  ADD_TO_COMPONENT
  MC_RISCV
  )
