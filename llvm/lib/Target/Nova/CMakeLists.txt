tablegen(LLVM NovaGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM NovaGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM NovaGenAsmWriter.inc -gen-asm-writer)

add_public_tablegen_target(NovaCommonTableGen)

add_llvm_component_group(Nova)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)

add_llvm_target(NovaCodeGen
  NovaTargetMachine.cpp
  NovaRegisterInfo.cpp
  NovaSubtarget.cpp
  NovaTargetObjectFile.cpp
  NovaInstrInfo.cpp
  NovaISelLowering.cpp
  NovaAsmPrinter.cpp

  LINK_COMPONENTS
  MC
  TargetParser
  LLVM
  Target

  ADD_TO_COMPONENT
  Nova
)



