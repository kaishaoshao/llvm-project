add_llvm_component_group(CoralNPU)

set(LLVM_TARGET_DEFINITIONS CoralNPU.td)

#生成Gen***.inc
tablegen(LLVM CoralNPUGenRegisterInfo.inc -gen-register-info)
# tablegen(LLVM CoralNPUGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM CoralNPUGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(CoralNPUCommonTableGen)

add_llvm_target(CoralNPUCodeGen
  CoralNPUTargetMachine.cpp
)

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
