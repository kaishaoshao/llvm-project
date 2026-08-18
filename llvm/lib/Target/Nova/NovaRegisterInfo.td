// Nova CPU Registers

class NovaReg<string n> : Register<n> {
  let Namespace = "Nova";
}

class NovaGPRReg<string n> : Register<n> {
  let Namespace = "Nova";
}

let Namespace = "Nova" in {
  def ZERO : NovaGPRReg<"zero"> {
    let isConstant = 1;
  }

  def AT : NovaGPRReg<"at">;
  def V0 : NovaGPRReg<"2">;
  def V1 : NovaGPRReg<"3">;
  def A0 : NovaGPRReg<"4">;
  def A1 : NovaGPRReg<"5">;
  def A2 : NovaGPRReg<"6">;
  def A3 : NovaGPRReg<"7">;
  def T0 : NovaGPRReg<"8">;
  def T1 : NovaGPRReg<"9">;
  def T2 : NovaGPRReg<"10">;
  def T3 : NovaGPRReg<"11">;
  def T4 : NovaGPRReg<"12">;
  def T5 : NovaGPRReg<"13">;
  def T6 : NovaGPRReg<"14">;
  def T7 : NovaGPRReg<"15">;
  def S0 : NovaGPRReg<"16">;
  def S1 : NovaGPRReg<"17">;
  def S2 : NovaGPRReg<"18">;
  def S3 : NovaGPRReg<"19">;
  def S4 : NovaGPRReg<"20">;
  def S5 : NovaGPRReg<"21">;
  def S6 : NovaGPRReg<"22">;
  def S7 : NovaGPRReg<"23">;
  def T8 : NovaGPRReg<"24">;
  def T9 : NovaGPRReg<"25">;
  def K0 : NovaGPRReg<"26">;
  def K1 : NovaGPRReg<"27">;
  def GP : NovaGPRReg<"gp">; // 28
  def SP : NovaGPRReg<"sp">; // 29
  def FP : NovaGPRReg<"sp">; // 20
  def RA : NovaGPRReg<"ra">; // 31

  //== Spcial registers ==//
  //======================//
  def PC : NovaReg<"pc">;
}

//=== -----------------
// Register Classes
//=== -----------------
// namespace, 值类型， 对齐方式4字节
def GPR32 : RegisterClass<"Nova",
  [i32],
  32,
  (add
  // Return values and arguments
  V0, V1,
  A0, A1, A2, A3,
  // Caller-saved temporaries (not peserved across calls )
  T0, T1, T2, T3, T4, T5, T6, T7,
  // caller-saved (preserved across calls)
  S0, S1, S2, S3, S4, S5, S6, S7,
  // More caller-saced temporaries
  T8, T9,
  // kernel registers
  K0, K1,
  // Global pointer
  GP,
  // Stack pointer
  SP,
  // Frmame pointer
  FP,
  // Return address (mostly used by instruction "jal")
  RA)
>;

def GPR32ZERO : RegisterClass<"Nova", [i32], 32, (add ZERO)>;

// 不可分配寄存器：$ra不能由寄存器分配器分配给虚拟寄存器
let isAllocatable = false in {
  def CPURAReg : RegisterClass<"Nova", [i32], 32, (add RA)>;
  def CPUSPReg : RegisterClass<"Nova", [i32], 32, (add SP)>;

  def GP32 : RegisterClass<"Nova", [i32], 32, (add GP)>;
}
