//===-- TX8LegalizeEXEC.cpp - Legalize the TX8/SIMT EXEC lane mask --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TX8LegalizeEXEC is the vendor's post-ISel pass that turns the two "EXEC"
// pseudos produced by instruction selection into real instructions and puts
// every vector instruction under the lane mask.  It is the "(B)" EXEC/SIMT-mask
// family's counterpart to TX8LegalizeCTInstruction and it runs *after*
// register allocation, right after `riscv-copyelim`:
//
//   ... -riscv-insert-vsetvli -riscv-dead-defs ... -riscv-copyelim
//       -legalize-exec -removeredundantdebugvalues ...
//
// (verified with `-mllvm -debug-pass=Structure` on the vendor compiler; the
// same name is what makes `-print-before/after=legalize-exec` work).
//
// Three transformations, each of them reproduced from the vendor's own MIR
// (`-mllvm -print-before=legalize-exec` / `-print-after=legalize-exec`) and
// from `-run-pass=legalize-exec` experiments on hand-written MIR:
//
// 1. `PseudoRAISA_DIVERGENT <mask>, $rd`  (llvm.riscv.raisa.divergent, i.e.
//    "set EXEC from the 128-bit mask and return execz") is expanded into
//
//        ISETEXEC  <mask>, implicit-def $exec
//        $rd = MOVEXECZ implicit $exec
//        $rd = Kcore_Ncc_Read killed $rd
//
//    The three instructions are inserted at the *first terminator* of the
//    block, not at the pseudo's own position -- the vendor's MIR puts them
//    after whatever followed the pseudo (`probe_sr` in the reconstruction
//    notes: the `SD` that consumed the old $rd ends up in front of them).
//    The kill/renamable flags are copied from the pseudo's operands, and the
//    extra use operand of Kcore_Ncc_Read is a plain `killed` register use of
//    the same GPR.
//
// 2. Every instruction that implicitly *reads* `$vl` (the fork's marker for
//    "consumes the vector configuration": all RVV pseudos carry
//    `implicit $vl, implicit $vtype`, and the `PseudoVSETVLI*` forms that
//    reuse a configuration carry `implicit $vl`) gets an implicit use of
//    `$exec` added.  This is how the lane mask becomes part of the liveness
//    and scheduling dependencies.  Verified by experiment: an instruction with
//    only `implicit $vtype` does *not* get `$exec`, one with `implicit $vl`
//    does.  (The vendor's register table numbers `$exec` 1 and therefore `$vl`
//    8, which is the constant the decompiled pass tests with
//    `hasRegisterImplicitUseOperand(MI, 8)`.)
//
// 3. Redundant EXEC writes are dropped.  An `ISETEXEC*` in a block that has no
//    instruction reading `$vl` (so nothing can observe the mask) is erased
//    when either it was the block of an expanded `PseudoRAISA_DIVERGENT`
//    (case 1 above already writes EXEC), or the block has exactly one
//    successor and that successor writes EXEC itself.
//
// `ISETEXEC*` instructions whose mask register is live on block entry are
// additionally hoisted to the beginning of their block and their `$exec` def
// loses the `dead` flag, because from that point on the mask is live.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "legalize-exec"
#define TX8_LEGALIZE_EXEC_NAME "TX8 Legalize EXEC"

namespace {

class TX8LegalizeEXEC : public MachineFunctionPass {
public:
  static char ID;

  TX8LegalizeEXEC() : MachineFunctionPass(ID) {
    initializeTX8LegalizeEXECPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return TX8_LEGALIZE_EXEC_NAME; }

private:
  /// Per-block information gathered while walking the function.
  struct BlockInfo {
    /// The last ISETEXEC/ISETEXEC0/ISETEXEC1 of the block, if any.
    MachineInstr *ExecWrite = nullptr;
    /// The last PseudoRAISA_DIVERGENT of the block, if any (the vendor's scan
    /// overwrites its slot, so the last one wins).
    MachineInstr *Divergent = nullptr;
    /// True once this block's Divergent was expanded; the vendor re-uses the
    /// slot for the new ISETEXEC, so a non-null slot is what decides case 3.
    bool ExpandedDivergent = false;
    /// True if some instruction of the block implicitly reads `$vl`, i.e. can
    /// observe the lane mask.
    bool HasVLReader = false;
  };

  bool expandDivergent(MachineBasicBlock &MBB, MachineInstr &Div,
                       const RISCVInstrInfo *TII);
  void addExecUse(MachineFunction &MF, MachineInstr &MI) const;
  void clearDeadExecDef(MachineInstr &MI) const;
};

} // end anonymous namespace

char TX8LegalizeEXEC::ID = 0;

INITIALIZE_PASS(TX8LegalizeEXEC, DEBUG_TYPE, TX8_LEGALIZE_EXEC_NAME, false,
                false)

/// Add an implicit use of `$exec` to \p MI, exactly the operand
/// `MachineOperand::CreateReg(RISCV::EXEC, /*isDef=*/false, /*isImplicit=*/true)`
/// that the vendor's pass builds.
void TX8LegalizeEXEC::addExecUse(MachineFunction &MF, MachineInstr &MI) const {
  MI.addOperand(MF, MachineOperand::CreateReg(RISCV::EXEC, /*isDef=*/false,
                                              /*isImplicit=*/true));
}

/// Clear the `dead` flag of \p MI's implicit `$exec` def; the vendor does the
/// same with a `&= ~IsDeadOrKill` on that operand.
void TX8LegalizeEXEC::clearDeadExecDef(MachineInstr &MI) const {
  for (MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isDef() && MO.isImplicit() &&
        MO.getReg() == RISCV::EXEC)
      MO.setIsDead(false);
}

/// Expand `$rd = PseudoRAISA_DIVERGENT <mask>, implicit-def $exec` at the first
/// terminator of \p MBB.
bool TX8LegalizeEXEC::expandDivergent(MachineBasicBlock &MBB, MachineInstr &Div,
                                      const RISCVInstrInfo *TII) {
  const MachineOperand &DefOp = Div.getOperand(0);
  const MachineOperand &MaskOp = Div.getOperand(1);
  Register Dst = DefOp.getReg();
  Register Mask = MaskOp.getReg();
  DebugLoc DL = Div.getDebugLoc();

  // The GPR result of the pseudo becomes the result of MOVEXECZ and of
  // Kcore_Ncc_Read, keeping the pseudo's operand flags (`renamable`).
  unsigned DstFlags = RegState::Define | getRenamableRegState(DefOp.isRenamable());
  // The mask operand of the new ISETEXEC keeps the pseudo's `killed` and
  // `renamable` flags.
  unsigned MaskFlags =
      getKillRegState(MaskOp.isKill()) | getRenamableRegState(MaskOp.isRenamable());

  MachineBasicBlock::iterator InsertPt = MBB.getFirstTerminator();

  BuildMI(MBB, InsertPt, DL, TII->get(RISCV::XTsm_ISETEXEC))
      .addReg(Mask, MaskFlags);
  BuildMI(MBB, InsertPt, DL, TII->get(RISCV::XTsm_MOVEXECZ))
      .addReg(Dst, DstFlags);
  BuildMI(MBB, InsertPt, DL, TII->get(RISCV::XTsm_KCORE_NCC_READ))
      .addReg(Dst, DstFlags)
      .addReg(Dst, RegState::Kill);

  Div.eraseFromParent();
  return true;
}

bool TX8LegalizeEXEC::runOnMachineFunction(MachineFunction &MF) {
  // The vendor only installs the pass for RAISA device compilation (its
  // argument never shows up in a `riscv64-unknown-elf` pipeline), so the
  // feature check is enough to keep it inert elsewhere.
  if (!MF.getSubtarget<RISCVSubtarget>().hasVendorXTsm82s())
    return false;

  const RISCVInstrInfo *TII = MF.getSubtarget<RISCVSubtarget>().getInstrInfo();

  SmallVector<BlockInfo, 16> Blocks(MF.getNumBlockIDs());
  bool Changed = false;

  //===--------------------------------------------------------------------===//
  // Walk the function once, recording the EXEC-related instructions per block
  // and adding the `implicit $exec` uses.
  //===--------------------------------------------------------------------===//
  for (MachineBasicBlock &MBB : MF) {
    BlockInfo &BI = Blocks[MBB.getNumber()];

    for (MachineInstr &MI : MBB) {
      switch (MI.getOpcode()) {
      case RISCV::XTsm_ISETEXEC:
      case RISCV::XTsm_ISETEXEC0:
      case RISCV::XTsm_ISETEXEC1:
        // Last one wins, like the vendor's loop.
        BI.ExecWrite = &MI;
        continue;
      case RISCV::PseudoRAISA_DIVERGENT:
        BI.Divergent = &MI;
        continue;
      default:
        break;
      }

      if (MI.hasRegisterImplicitUseOperand(RISCV::VL)) {
        addExecUse(MF, MI);
        BI.HasVLReader = true;
        Changed = true;
      }
    }

    //--- Hoist an EXEC write whose mask is live on entry. ------------------//
    MachineInstr *Exec = BI.ExecWrite;
    if (Exec && !MBB.empty() && Exec != &MBB.front()) {
      bool CanHoist = true;
      if (Exec->getOpcode() == RISCV::XTsm_ISETEXEC &&
          Exec->getOperand(0).isReg() &&
          Exec->getOperand(0).getReg().isValid())
        // Moving the write above its own definition would break the mask, so
        // only a mask that is live on entry may be hoisted.
        CanHoist = MBB.isLiveIn(Exec->getOperand(0).getReg());
      if (CanHoist) {
        MBB.splice(MBB.begin(), &MBB, Exec->getIterator());
        clearDeadExecDef(*Exec);
        Changed = true;
      }
    }

    //--- Expand the divergent pseudo. --------------------------------------//
    if (BI.Divergent) {
      expandDivergent(MBB, *BI.Divergent, TII);
      BI.Divergent = nullptr;
      BI.ExpandedDivergent = true;
      Changed = true;
    }
  }

  //===--------------------------------------------------------------------===//
  // Drop redundant EXEC writes.
  //===--------------------------------------------------------------------===//
  for (MachineBasicBlock &MBB : MF) {
    BlockInfo &BI = Blocks[MBB.getNumber()];
    if (!BI.ExecWrite || BI.HasVLReader)
      continue;

    if (!BI.ExpandedDivergent) {
      // Nothing in this block reads the mask; the only case where the write is
      // still redundant is when the single successor writes EXEC itself.
      if (MBB.succ_size() != 1)
        continue;
      MachineBasicBlock *Succ = *MBB.succ_begin();
      if (!Blocks[Succ->getNumber()].ExecWrite)
        continue;
    }

    BI.ExecWrite->eraseFromParent();
    BI.ExecWrite = nullptr;
    Changed = true;
  }

  return Changed;
}

FunctionPass *llvm::createTX8LegalizeEXECPass() {
  return new TX8LegalizeEXEC();
}
