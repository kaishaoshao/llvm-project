//===-- TX8InsertXFRM.cpp - Insert TX8 xfrm / mem_alloc markers -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TX8 (tsm-x82) replaces the RISC-V `frm` CSR based dynamic rounding mode with
// the architectural `xfrm` register (programmed by `vsetxfrm`), and marks
// every vector memory access with the `mem_alloc` marker register.
//
// Both show up as *implicit* physical register uses only, i.e. they never
// change the encoded bytes of an instruction, but they do constrain the pre-RA
// register allocator: without them `greedy` picks different physical vector
// registers.  They are therefore inserted after all MIR optimizations and
// immediately before `riscv-insert-read-write-csr`.
//
// This pass performs three transformations:
//
//   * every RVV vector load/store gets an implicit use of `$mem_alloc`,
//     placed directly after the explicit operands so that the operand order
//     matches the vendor compiler (implicit `$vl`/`$vtype` uses, which are
//     added later upstream, follow it);
//
//   * the implicit `$frm` use of every floating point instruction whose
//     rounding mode comes from `frm` (TSFlags HasRoundModeOp and not UsesVXRM)
//     is rewritten to `$xfrm`.  When such an instruction has no `$frm` use at
//     all - it carries a static rounding mode immediate - an implicit `$xfrm`
//     use is added instead;
//
//   * for a static rounding mode, the TX8 encoding of that mode is
//     materialised with `ADDI <gpr>, x0, <mode>` + `vsetxfrm <gpr>` in front
//     of the instruction, and the dynamic mode is restored with
//     `vsetxfrm x0` right after it.  Renaming `$frm` to `$xfrm` is what stops
//     the upstream `riscv-insert-read-write-csr` pass from emitting its own
//     CSR based save/restore; that pass is additionally disabled for tsm-x82.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/RISCVBaseInfo.h"
#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "tx8-insert-xfrm"
#define TX8_INSERT_XFRM_NAME "TX8 Insert VSETXFRM"

namespace {

class TX8InsertXFRM : public MachineFunctionPass {
public:
  static char ID;

  TX8InsertXFRM() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return TX8_INSERT_XFRM_NAME; }

private:
  bool addMemAllocMarkers(MachineFunction &MF) const;
  bool insertXFRM(MachineFunction &MF) const;
};

} // end anonymous namespace

char TX8InsertXFRM::ID = 0;

INITIALIZE_PASS(TX8InsertXFRM, DEBUG_TYPE, TX8_INSERT_XFRM_NAME, false, false)

/// Return true if \p MI is an RVV vector memory operation, i.e. an instruction
/// that accesses memory and has a vector register operand together with the
/// SEW/VL operands every generated RVV instruction carries.
static bool isVectorMemOp(const MachineInstr &MI,
                          const MachineRegisterInfo &MRI) {
  if (!MI.mayLoadOrStore())
    return false;

  const MCInstrDesc &Desc = MI.getDesc();
  if (!RISCVII::hasVLOp(Desc.TSFlags) && !RISCVII::hasSEWOp(Desc.TSFlags))
    return false;

  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.getReg().isVirtual())
      continue;
    if (RISCVRegisterInfo::isRVVRegClass(MRI.getRegClass(MO.getReg())))
      return true;
  }
  return false;
}

/// TX8 numbers the rounding modes in `xfrm` differently from the RISC-V `frm`
/// CSR: RDN and RUP are swapped.  RNE (0) and DYN (7) keep their meaning, and
/// a mode of 0 means "nothing to program" for this pass - RNE is already the
/// default and DYN must read `xfrm`, which is only written by the pairs this
/// function guards.
static unsigned mapRoundingMode(unsigned Mode) {
  if (Mode == 2) // RDN in `frm`, RUP in `xfrm`
    return 3;
  if (Mode == 3) // RUP in `frm`, RDN in `xfrm`
    return 2;
  return Mode;
}

/// Emit `vsetxfrm \p Src`.
static void insertVSETXFRM(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator InsertPt,
                           const DebugLoc &DL, const RISCVInstrInfo *TII,
                           Register Src) {
  BuildMI(MBB, InsertPt, DL, TII->get(RISCV::XTsm_VSETXFRM))
      .addReg(Src, RegState::Kill)
      .addDef(RISCV::XFRM, RegState::Implicit);
}

bool TX8InsertXFRM::addMemAllocMarkers(MachineFunction &MF) const {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (!isVectorMemOp(MI, MRI))
        continue;
      if (MI.readsRegister(RISCV::MEM_ALLOC, /*TRI=*/nullptr))
        continue;

      MachineOperand MO = MachineOperand::CreateReg(
          RISCV::MEM_ALLOC, /*isDef=*/false, /*isImp=*/true);

      // Keep the marker directly after the explicit operands, in front of the
      // implicit `$vl`/`$vtype` uses that the pre-RA passes add, which is
      // where the vendor compiler materializes it as well.
      unsigned NumExplicit = MI.getDesc().getNumOperands();
      if (NumExplicit < MI.getNumOperands())
        MI.insert(&MI.getOperand(NumExplicit), MO);
      else
        MI.addOperand(MF, MO);
      Changed = true;
    }
  }

  return Changed;
}

bool TX8InsertXFRM::insertXFRM(MachineFunction &MF) const {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  const RISCVInstrInfo *TII = ST.getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    // Static rounding mode programmed by the last processed instruction.  0
    // means "no static mode is in effect".
    unsigned PendingMode = 0;
    MachineInstr *SavedMI = nullptr;

    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;

      // Never look inside inline asm or bundles; make sure the dynamic
      // rounding mode is restored before entering them.
      if (MI.isInlineAsm() || MI.isBundle()) {
        if (PendingMode) {
          insertVSETXFRM(MBB, MI, MI.getDebugLoc(), TII, RISCV::X0);
          PendingMode = 0;
        }
        continue;
      }

      // -1 for instructions that do not take a rounding mode, or that take it
      // from `vxrm` (which is not affected by `xfrm`).
      int FrmOpNum = RISCVII::getFRMOpNum(MI.getDesc());
      if (FrmOpNum < 0)
        continue;

      const MachineOperand &FrmOp = MI.getOperand(FrmOpNum);
      unsigned Mode = 0;
      if (FrmOp.isImm() && static_cast<unsigned>(FrmOp.getImm()) <= 6)
        Mode = mapRoundingMode(FrmOp.getImm());

      if (PendingMode != Mode) {
        if (Mode) {
          Register ModeReg = MRI.createVirtualRegister(&RISCV::GPRRegClass);
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(RISCV::ADDI), ModeReg)
              .addReg(RISCV::X0)
              .addImm(Mode);
          insertVSETXFRM(MBB, MI, MI.getDebugLoc(), TII, ModeReg);
        } else {
          insertVSETXFRM(MBB, MI, MI.getDebugLoc(), TII, RISCV::X0);
        }
        PendingMode = Mode;
      }

      // Rewrite the implicit dynamic rounding mode read to the TX8 register.
      int Idx = MI.findRegisterUseOperandIdx(RISCV::FRM, /*TRI=*/nullptr,
                                             /*isKill=*/false);
      if (Idx < 0)
        MI.addOperand(MachineOperand::CreateReg(
            RISCV::XFRM, /*isDef=*/false, /*isImp=*/true));
      else
        MI.getOperand(Idx).setReg(RISCV::XFRM);

      SavedMI = &MI;
      Changed = true;
    }

    // Restore the dynamic rounding mode directly after the instruction that
    // requested a static one.
    if (PendingMode && SavedMI)
      insertVSETXFRM(MBB, std::next(SavedMI->getIterator()),
                     SavedMI->getDebugLoc(), TII, RISCV::X0);
  }

  return Changed;
}

bool TX8InsertXFRM::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  // The `xfrm` register only exists on the TX8 core.
  if (!MF.getSubtarget<RISCVSubtarget>().hasVendorXTsm82s())
    return false;

  bool Changed = false;
  Changed |= addMemAllocMarkers(MF);
  Changed |= insertXFRM(MF);
  return Changed;
}

FunctionPass *llvm::createTX8InsertXFRMPass() { return new TX8InsertXFRM(); }
