// Nano CPU Register Info

class NanoReg<string n> : Register<n> {
    let Namespace = "Nano";
}

class NanoGPRReg<string n> : Register<n> {
    let Namespace = "Nano";
}

let Namespace = "Nano" in {
    def ZERO : NanoReg<"zero"> {
        let isConstant = 1;
    }
    def AT : NanoGPRReg<"at">;
    def V0 : NanoGPRReg<"2">;
    def V1 : NanoGPRReg<"3">;
    def A0 : NanoGPRReg<"4">;
    def A1 : NanoGPRReg<"5">;
    def A2 : NanoGPRReg<"6">;
    def A3 : NanoGPRReg<"7">;
    def T0 : NanoGPRReg<"8">;
    def T1 : NanoGPRReg<"9">;
    def T2 : NanoGPRReg<"10">;
    def T3 : NanoGPRReg<"11">;
    def T4 : NanoGPRReg<"12">;
    def T5 : NanoGPRReg<"13">;
    def T6 : NanoGPRReg<"14">;
    def T7 : NanoGPRReg<"15">;
    def S0 : NanoGPRReg<"16">;
    def S1 : NanoGPRReg<"17">;
    def S2 : NanoGPRReg<"18">;
    def S3 : NanoGPRReg<"19">;
    def S4 : NanoGPRReg<"20">;
    def S5 : NanoGPRReg<"21">;
    def S6 : NanoGPRReg<"22">;
    def S7 : NanoGPRReg<"23">;
    def T8 : NanoGPRReg<"24">;
    def T9 : NanoGPRReg<"25">;
    def K0 : NanoGPRReg<"26">;
    def K1 : NanoGPRReg<"27">;
    def GP : NanoGPRReg<"gp">; // 28
    def SP : NanoGPRReg<"sp">; // 29
    def FP : NanoGPRReg<"fp">; // 30
    def RA : NanoGPRReg<"ra">; // 31

    //== Special Registers ==//
    def PC : NanoReg<"pc">;
}

//=== --------------
// Register Classes
//=== --------------
def GPR32 : RegisterClass<"Nano", // Target Namespace
[i32],  // 32位整数
32,     // 4 bits 对齐
(add
// Reserved Registers
ZERO, AT,
// Return Values and arguments
V0, V1,
A0, A1, A2, A3,
// Caller-saved temporaries (not preserved across calls, clobbered by calls)
T0, T1, T2, T3, T4, T5, T6, T7,
// Callee-saved (Preserved across calls)
T8, T9,
// Kernel registers
K0, K1,
// Global Pointer
GP,
// Stack Pointer
SP,
// Frame Pointer
FP,
// Return Address (mostly used by instruction "jal")
RA)>;

//=== -------------------
// Other Register classes
//=== -------------------
def GPR32ZERO : RegisterClass<"Nano", [i32], 32, (add ZERO)>;

def GPR32NONZERO : RegisterClass<"Nano", [i32], 32, (sub GPR32, GPR32ZERO)>;

//=== --------------------
// Unallocatable registers
//=== --------------------
// 寄存器分配器不会将 $ra 等特殊寄存器分配给虚拟寄存器
let isAllocatable = false in {
    def CPURAReg : RegisterClass<"Nano", [i32], 32, (add RA)>;
    def CPUSPReg : RegisterClass<"Nano", [i32], 32, (add SP)>;

    def GP32 : RegisterClass<"Nano", [i32], (add GP)>;
}


