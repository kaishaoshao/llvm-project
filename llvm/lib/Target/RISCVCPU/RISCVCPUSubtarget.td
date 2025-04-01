def RISCVCPUInstrInfo : InstrInfo;

def RISCVCPU : Target {
    let InstructionSet =RISCVCPUInstrInfo;
}