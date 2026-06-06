add_llvm_component_group(CoralNPU)
add_llvm_target(CoralNPUCodeGen
  CoralNPUTargetMachine.cpp
)

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)

