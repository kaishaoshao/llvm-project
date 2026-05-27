add_llvm_component_group(MiniRiscv)
add_llvm_target(MiniRiscvCodeGen
  MiniRiscvTargetMachine.cpp
)

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)

