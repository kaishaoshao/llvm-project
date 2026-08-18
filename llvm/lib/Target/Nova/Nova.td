include "llvm/Target/Target.td"

include "NovaRegisterInfo.td"
include "NovaInstrInfo.td"

def : ProcessorModel<"generic", NoSchedModel, []>;

def Nova: Target {

}
