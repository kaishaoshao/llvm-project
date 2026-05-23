include "llvm/Target/Target.td"

def FeatureRV32
    : SubtargetFeature<"32bit", "HasRV64", "false", "RV32 support">;
def FeatureRV64
    : SubtargetFeature<"64bit", "HasRV64", "true", "RV64 support">;

def IsRV32 : Predicate<"!Subtarget->hasRV64()">;
def IsRV64 : Predicate<"Subtarget->hasRV64()">;

defvar RV32 = DefaultMode;
def RV64 : HwMode<"+64bit", [IsRV64]>;

include "TOYRISCVRegisterInfo.td"
include "TOYRISCVSchedule.td"
include "TOYRISCVInstrInfo.td"
include "TOYRISCVCallingConv.td"

def TOYRISCVInstrInfo : InstrInfo;

def TOYRISCV : Target {
  let InstructionSet = TOYRISCVInstrInfo;
}

def : ProcessorModel<"cpu-rv32", NoSchedModel, []>;
def : ProcessorModel<"cpu-rv64", NoSchedModel, [FeatureRV64]>;
