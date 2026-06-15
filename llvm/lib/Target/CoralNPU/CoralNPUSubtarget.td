def Feature32Bit :
  SubtargetFeature<"32bit", "Is32bit", "ture", "CoralNPU 32Bit support">;

def Feature64Bit :
  SubtargetFeature<"64bit", "Is32bit", "false", "CoralNPU 64Bit support">;

def Is32Bit : Predicate<"Subtarget->is32Bit()">;
def Is64Bit : Predicate<"!Subtarget->is32Bit()">;

defvar Coral32Bit = DefaultMode;
def Coral64Bit : HwMode<"+64bit", [Is64Bit]>;

def : ProcessorModel<"generic", NoSchedModel, []>;
def : ProcessorModel<"generic-coralnpu64", NoSchedModel, [Feature64Bit]>;
