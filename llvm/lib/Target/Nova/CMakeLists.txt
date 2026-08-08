add_llvm_component_group(Nova)

add_llvm_target(NovaCodeGen
  MipsNovaTargetMachine.cpp

  LINK_COMPONENTS
  MC
  TargetParser
  Target

  ADD_TO_COMPONENT
  Nova
)
