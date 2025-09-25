add_llvm_component_group(MC_RISCV)

# set(LLVM_TARGET_DEFINITIONS RISCV.td)

add_llvm_target(NovaCodeGen

  LINK_COMPONENTS
  MC
  TargetParser
  Target

  ADD_TO_COMPONENT
  MC_RISCV
  )


add_subdirectory(AsmParser)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
