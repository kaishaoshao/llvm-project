//===-- TX8RegisterAlignment.cpp - TX8 vector register alignment ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Reconstruction of the vendor's `TX8RegisterAlignment` pass (pass name
// `vreg-align`, display name "TX8 Registers Alignment"), which the vendor runs
// right after `twoaddressinstruction` and before `register-coalescer`:
//
//   vendor `-debug-pass=Structure`:
//     ... -twoaddressinstruction -vreg-align -slotindexes -liveintervals
//         -register-coalescer -rename-independent-subregs -machine-scheduler ...
//   vendor `RISCVTargetMachine.cpp.o`:
//     addPreISel(): insertPass(&TwoAddressInstructionPassID,
//                              &TX8RegisterAlignmentPassID)   [if -riscv-vreg-align]
//
// What it does (from the vendor's own MIR, `-print-after=vreg-align` on
// mem_l1_size_1.rc and the standalone `llc -run-pass=vreg-align` replay):
//
//   before:
//     %4:vr   = PseudoVMV_V_X_M1 undef %4, killed %3, -1, 5, 0
//     early-clobber %6:vrm2 = PseudoVSLH_VX_M2 undef %6, killed %4, killed %5,
//                             128, 5, 0
//   after:
//     %4:vr = PseudoVMV_V_X_M1 undef %4, killed %3, -1, 5, 0
//     undef %14.sub_vrm1_0:vrm2 = COPY killed %4
//     early-clobber %6:vrm2 = PseudoVSLH_ALIGNED_VX_M2 undef %6, killed %14,
//                             killed %5, 128, 5, 0
//
// i.e. the operand that the `*_ALIGNED_*` twin demands is one LMUL step wider
// than the plain pseudo's (a vector shift-left-half reads its source as an
// even-aligned register pair), so the pass materialises the widened virtual
// register with a sub-register `COPY` and renames the instruction to its
// `*_ALIGNED_*` twin.  RegisterCoalescer folds that COPY back into the defining
// instruction, which is why the post-coalescer MIR shows
// `undef %14.sub_vrm1_0:vrm2 = PseudoVMV_V_X_M1 ...`.
//
// Selection condition (vendor `IsCTInstrRequiresAlignment` +
// `RISCV::getCTAlignedPseudoInfo`, decompiled in
// tools/re/out/TX8RegisterAlignment/decompiled.c): for the XTsm_VSLH_* base
// opcodes the InstrAlignMap value is >= 2, and the pseudo's VLMul TSFlags field
// then decides: align iff `getLMul(TSFlags) - 1 < 3`, i.e. only LMUL_2, LMUL_4
// and LMUL_8 are rewritten (LMUL_1 and the fractional LMULs are not).
// The widened operand is operand 1 (vs2) and the sub-register index widens it
// by exactly one LMUL step: sub_vrm1_0 (6) for VR->VRM2, sub_vrm2_0 (14) for
// VRM2->VRM4, sub_vrm4_0 (18) for VRM4->VRM8 -- the very constants the vendor's
// decompiled `alignInstrVReg` uses.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "MCTargetDesc/RISCVBaseInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "vreg-align"

namespace {

/// One row of the vendor's InstrAlignMap (keyed by the *pseudo* here, since our
/// table only needs the families that our `RISC-V` TD defines an ALIGNED twin
/// for).  OpNo is the operand that has to be widened: for
/// `VPseudoBinary`-derived pseudos the machine operands are
/// (0 rd, 1 passthru, 2 rs2, 3 rs1, 4 vl, 5 sew, 6 policy), so the widened
/// source (`$rs2`) is operand 2.
struct AlignedPseudoInfo {
  unsigned Plain;
  unsigned Aligned;
  unsigned WideSubReg;
  unsigned OpNo;
};

// `SubReg` is the sub-register index of the narrow (one-LMUL-step-smaller)
// value inside the wide register: sub_vrm1_0 = 6, sub_vrm2_0 = 14,
// sub_vrm4_0 = 18 in both this tree and the vendor's.
#define XTsmAlignedVSLHRow(LMUL, SUBREG)                                       \
  {RISCV::PseudoXTsm_VSLH_VV_##LMUL,                                           \
   RISCV::PseudoXTsm_VSLH_ALIGNED_VV_##LMUL, RISCV::SUBREG, 2},                \
      {RISCV::PseudoXTsm_VSLH_VX_##LMUL,                                       \
       RISCV::PseudoXTsm_VSLH_ALIGNED_VX_##LMUL, RISCV::SUBREG, 2}

const AlignedPseudoInfo AlignedPseudos[] = {
    XTsmAlignedVSLHRow(M2, sub_vrm1_0),
    XTsmAlignedVSLHRow(M4, sub_vrm2_0),
    XTsmAlignedVSLHRow(M8, sub_vrm4_0),
};

#undef XTsmAlignedVSLHRow

const AlignedPseudoInfo *lookupAlignedPseudo(unsigned Opcode) {
  for (const AlignedPseudoInfo &Info : AlignedPseudos)
    if (Info.Plain == Opcode)
      return &Info;
  return nullptr;
}

/// Vendor `IsCTInstrRequiresAlignment`: every entry of our table corresponds to
/// an InstrAlignMap value >= 2 (7 for XTsm_VSLH_VV/VX), for which the rule is
/// `getLMul(TSFlags) - 1 < 3`.
bool requiresAlignment(const MachineInstr &MI) {
  unsigned VLMul =
      static_cast<unsigned>(RISCVII::getLMul(MI.getDesc().TSFlags));
  return VLMul >= static_cast<unsigned>(RISCVVType::LMUL_2) &&
         VLMul <= static_cast<unsigned>(RISCVVType::LMUL_8);
}

class TX8RegisterAlignment : public MachineFunctionPass {
public:
  static char ID;

  TX8RegisterAlignment() : MachineFunctionPass(ID) {
    initializeTX8RegisterAlignmentPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return "TX8 Registers Alignment"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char TX8RegisterAlignment::ID = 0;
char &llvm::TX8RegisterAlignmentPassID = TX8RegisterAlignment::ID;

INITIALIZE_PASS(TX8RegisterAlignment, "vreg-align", "TX8 Registers Alignment",
                false, false)

FunctionPass *llvm::createTX8RegisterAlignmentPass() {
  return new TX8RegisterAlignment();
}

bool TX8RegisterAlignment::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  const RISCVInstrInfo *TII = ST.getInstrInfo();
  const RISCVRegisterInfo *TRI = ST.getRegisterInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      const AlignedPseudoInfo *Info = lookupAlignedPseudo(MI.getOpcode());
      if (!Info || !requiresAlignment(MI))
        continue;

      MachineOperand &Op = MI.getOperand(Info->OpNo);
      if (!Op.isReg() || !Op.getReg().isVirtual())
        continue;

      const MCInstrDesc &AlignedDesc = TII->get(Info->Aligned);
      const TargetRegisterClass *RC =
          TII->getRegClass(AlignedDesc, Info->OpNo, TRI, MF);
      if (!RC)
        continue;

      LLVM_DEBUG(dbgs() << "vreg-align: " << MI);

      // `undef %new.sub = COPY [killed] %old` -- the wide register's upper part
      // stays undefined; only the low sub-register holds the real value.
      Register NewVReg = MRI.createVirtualRegister(RC);
      BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(TargetOpcode::COPY))
          .addReg(NewVReg, RegState::Define | RegState::Undef,
                  Info->WideSubReg)
          .addReg(Op.getReg(), getKillRegState(Op.isKill()));

      Op.setReg(NewVReg);
      Op.setSubReg(0);
      MI.setDesc(AlignedDesc);
      Changed = true;

      LLVM_DEBUG(dbgs() << "        -> " << MI);
    }
  }

  return Changed;
}
