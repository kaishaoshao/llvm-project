//===-- NovaInstrFormats.td - Nova Instruction Formats --------------------===//
// This file contains the instruction formats for the Nova architecture.
//===----------------------------------------------------------------------===//

class NovaInst<dag outs, dag ins, string asmString> : Instruction {
  let Namespace = "Nova";
  let OutOperandList = outs;
  let InOperandList = ins;
  let asmString = asmString;
}

// 我们定义的指令是 LLVM IR 指令映射到的 MachineInstr。
// 理想情况下，这些指令与目标系统的指令集架构 (ISA) 指令相匹配。
// 但有时我们需要一些指令集架构（ISA）之外的额外指令，这些指令被称为“伪指令”。
// 伪指令并非真正的指令，而是用来表示一系列真实指令的。它们的作用是简化指令选择过程，
// 使操作更加便捷。例如，MIPS 后端使用PseudoRet`return` 指令。然
// 后根据 MIPS 版本指令集架构 (ISA) 的不同PseudoRet，打印为 `true`jr或 `false`jalr。
class PseudoNovaInst<dag outs, dag ins, string asmString> : NovaInst<outs, ins, asmString> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// 定义指令
// 在llvm中这些枚举例值并行由TableGen生成，但是我们正在努力实现自动生成
