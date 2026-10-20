//===-- TX8RegisterRename.cpp - Break vector register anti-dependencies ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TX82 (tsm-x82) vector register rename pass.
//
// The TX82 vector unit is sensitive to write-after-read hazards on the vector
// register file: an instruction that writes the physical vector register an
// earlier instruction read forces the write to be ordered after the read.
// This pass removes such anti-dependencies by renaming the destination
// register of the offending instruction - and every reference to the value it
// produces - to a free register of the same register class.
//
// Reconstructed from the vendor object
// `libLLVMRISCVCodeGen.a(TX8RegisterRename.cpp.o)`.  The rule set below is
// established with the vendor `llc -run-pass=vreg-rename` as an oracle (see
// `logs/re16/vr/`: `vr_ab.py` replays one pass on both compilers and
// `mkprobe3.py` generates liveness-clean probes):
//
//   * Only the vector register classes (VR / VRM2 / VRM4 / VRM8 and their
//     `NoV0` variants, which back masked instructions) are considered.
//   * **Anti-dependency (WAW):** a definition of R is renamed when R - or any
//     register overlapping it - was already defined *earlier in the same
//     machine basic block*.  That is the only trigger: it fires for a dead def
//     and for a def whose only use is a later instruction, and it does *not*
//     need the same instruction to read R.  A block-local live-in value
//     (defined in a predecessor) is never renamed.
//   * **Merge passthru is protected (two ways):**
//       - the def of an instruction whose own tied passthru is a *live*
//         (non-`undef`) read of R is never renamed (`$v8m2 = VSLH undef $v8m2`
//         is renamed, `$v8 = VAND renamable $v8` is not);
//       - the value feeding the live passthru of an instruction must stay in
//         place, so the *next* definition of that register encountered while
//         scanning backwards is skipped as well (one-shot: the protection is
//         consumed by that def).  This is what keeps the
//         `vand` -> `vadd` -> masked `vmul` passthru chain in one register.
//   * **V0 is never renamed away from**: a def that lands in the mask register
//     (`$v0 = PseudoVMSEQ...`, `$v0 = PseudoVAND...`) stays there, although
//     V0 is a perfectly good *target* register.
//   * A definition is also left alone when the value it produces has to stay in
//     place, i.e. when it is live out of the block or when one of its
//     references is an operand that cannot be rewritten (the implicit
//     return-value operand of PseudoRET).
//   * The replacement register comes from a per-class, monotonically advancing
//     cursor over the allocatable registers; the vendor walks the class
//     allocation order back to front (so the low registers are preferred) and a
//     register skipped because it was live is never revisited.
//   * `RestoreAlignedCTInstruction` runs first (see
//     `restoreAlignedCTInstructions`): the post-RA form of a `*_ALIGNED_*`
//     pseudo is the plain pseudo again, with the widened operand narrowing back
//     to its M1 sub-register and an `implicit $align2` marker.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVRegisterInfo.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "tx8-vreg-rename"

static cl::opt<bool> EnableTX8VRegRename("riscv-vreg-rename", cl::Hidden,
                                         cl::init(true),
                                         cl::desc("TX8 vector register "
                                                  "rename"));

static cl::opt<bool>
    EnableRenameOrderOpt("riscv-vreg-rename-order-opt", cl::Hidden,
                         cl::init(false),
                         cl::desc("Optimize for vreg rename order across MBB"));

namespace {

/// A physical register class and the cursor into its allocatable register
/// list.  The cursor is carried across basic blocks, like the vendor's
/// `DenseMap<const TargetRegisterClass *, unsigned>` argument to
/// `breakAntiDeps()`.
struct RegClassCursor {
  SmallVector<MCPhysReg, 32> Order;
  unsigned Next = 0;
};

/// One row of the vendor's `InstrAlignMap` inverse: the post-RA form of an
/// `*_ALIGNED_*` pseudo.  OpNo is the operand that `TX8RegisterAlignment`
/// widened; `WideSubReg` maps it back to the narrow value inside it.
struct AlignedPseudoInfo {
  unsigned Aligned;
  unsigned Plain;
  unsigned WideSubReg;
  unsigned OpNo;
};

#define XTsmAlignedVSLHRow(LMUL, SUBREG)                                       \
  {RISCV::PseudoXTsm_VSLH_ALIGNED_VV_##LMUL,                                   \
   RISCV::PseudoXTsm_VSLH_VV_##LMUL, RISCV::SUBREG, 2},                        \
      {RISCV::PseudoXTsm_VSLH_ALIGNED_VX_##LMUL,                               \
       RISCV::PseudoXTsm_VSLH_VX_##LMUL, RISCV::SUBREG, 2}

const AlignedPseudoInfo AlignedPseudos[] = {
    XTsmAlignedVSLHRow(M2, sub_vrm1_0),
    XTsmAlignedVSLHRow(M4, sub_vrm2_0),
    XTsmAlignedVSLHRow(M8, sub_vrm4_0),
};

#undef XTsmAlignedVSLHRow

const AlignedPseudoInfo *lookupAlignedPseudo(unsigned Opcode) {
  for (const AlignedPseudoInfo &Info : AlignedPseudos)
    if (Info.Aligned == Opcode)
      return &Info;
  return nullptr;
}

class VRegReNamer {
public:
  VRegReNamer(MachineFunction &MFi, const RegisterClassInfo &RCI)
      : MF(MFi), MRI(MF.getRegInfo()),
        TII(MF.getSubtarget<RISCVSubtarget>().getInstrInfo()),
        TRI(MF.getSubtarget<RISCVSubtarget>().getRegisterInfo()),
        RegClassInfo(RCI) {}

  bool run(DenseMap<const TargetRegisterClass *, RegClassCursor> &Cursors);

private:
  bool breakAntiDeps(
      MachineBasicBlock &MBB,
      DenseMap<const TargetRegisterClass *, RegClassCursor> &Cursors);

  static bool isVectorRegClass(const TargetRegisterClass *RC) {
    return RC && RISCVRI::isVRegClass(RC->TSFlags);
  }

  /// The vector register classes this pass is allowed to rename within.
  static ArrayRef<const TargetRegisterClass *> vectorRegClasses() {
    static const TargetRegisterClass *const Classes[] = {
        &RISCV::VRRegClass,       &RISCV::VRM2RegClass,
        &RISCV::VRM4RegClass,     &RISCV::VRM8RegClass,
        &RISCV::VRNoV0RegClass,   &RISCV::VRM2NoV0RegClass,
        &RISCV::VRM4NoV0RegClass, &RISCV::VRM8NoV0RegClass};
    return Classes;
  }

  /// Register class containing \p Reg if it is one of the vector classes.
  static const TargetRegisterClass *vectorRegClassOf(MCRegister Reg) {
    for (const TargetRegisterClass *RC : vectorRegClasses())
      if (RC->contains(Reg))
        return RC;
    return nullptr;
  }

  const TargetRegisterClass *getOperandRegClass(const MachineInstr &MI,
                                                unsigned OpIdx) const {
    const MCInstrDesc &Desc = MI.getDesc();
    if (OpIdx < Desc.getNumOperands())
      if (const TargetRegisterClass *RC =
              TII->getRegClass(Desc, OpIdx, TRI, MF))
        if (isVectorRegClass(RC))
          return RC;
    return nullptr;
  }

  bool overlapsAny(MCRegister Reg,
                   ArrayRef<MCRegister> Regs) const {
    for (MCRegister R : Regs)
      if (TRI->regsOverlap(Reg, R))
        return true;
    return false;
  }

  MCRegister findSuitableFreeRegister(
      const TargetRegisterClass *RC, MCRegister AntiDepReg,
      const LivePhysRegs &Live, const SmallVectorImpl<MCRegister> &Forbid,
      DenseMap<const TargetRegisterClass *, RegClassCursor> &Cursors);

  MachineFunction &MF;
  MachineRegisterInfo &MRI;
  const RISCVInstrInfo *TII;
  const RISCVRegisterInfo *TRI;
  const RegisterClassInfo &RegClassInfo;
};

MCRegister VRegReNamer::findSuitableFreeRegister(
    const TargetRegisterClass *RC, MCRegister AntiDepReg,
    const LivePhysRegs &Live, const SmallVectorImpl<MCRegister> &Forbid,
    DenseMap<const TargetRegisterClass *, RegClassCursor> &Cursors) {
  RegClassCursor &Cursor = Cursors[RC];
  if (Cursor.Order.empty()) {
    // The vendor walks the register class allocation order back to front, so
    // that the low registers are preferred (the upstream RISC-V order puts V0
    // last so that the greedy allocator does not needlessly block the high
    // LMUL groups).
    ArrayRef<MCPhysReg> Order = RegClassInfo.getOrder(RC);
    for (const MCPhysReg *I = Order.end(), *E = Order.begin(); I != E;) {
      --I;
      if (MRI.isAllocatable(*I))
        Cursor.Order.push_back(*I);
    }
  }

  for (; Cursor.Next < Cursor.Order.size(); ++Cursor.Next) {
    MCRegister NewReg = Cursor.Order[Cursor.Next];
    // Never replace a register with itself.
    if (NewReg == AntiDepReg)
      continue;
    // Registers still holding a live value are unavailable.  A register that
    // is skipped here stays skipped: the cursor is never rewound.
    if (!Live.available(MRI, NewReg))
      continue;
    if (overlapsAny(NewReg, Forbid))
      continue;
    ++Cursor.Next;
    return NewReg;
  }
  return MCRegister();
}

bool VRegReNamer::breakAntiDeps(
    MachineBasicBlock &MBB,
    DenseMap<const TargetRegisterClass *, RegClassCursor> &Cursors) {
  const unsigned NumRegs = TRI->getNumRegs();

  // Register *units* whose current value must not move elsewhere.  Units, not
  // alias sets: `$v8m2` may be live out without making `$v9` untouchable.
  BitVector KeepUnits(TRI->getNumRegUnits());
  auto keepUnit = [&](MCRegister Reg) {
    for (MCRegUnitIterator UI(Reg, TRI); UI.isValid(); ++UI)
      KeepUnits.set(*UI);
  };
  auto clearKeep = [&](MCRegister Reg) {
    for (MCRegUnitIterator UI(Reg, TRI); UI.isValid(); ++UI)
      KeepUnits.reset(*UI);
  };
  auto isKept = [&](MCRegister Reg) {
    for (MCRegUnitIterator UI(Reg, TRI); UI.isValid(); ++UI)
      if (KeepUnits.test(*UI))
        return true;
    return false;
  };

  // Block live-out.
  for (const MachineBasicBlock *Succ : MBB.successors())
    for (const auto &LI : Succ->liveins())
      keepUnit(LI.PhysReg);

  // Per register, the operands reading the value produced by the next
  // definition encountered while scanning upwards.
  SmallVector<std::pair<MCRegister, SmallVector<MachineOperand *, 4>>, 32> Refs;
  auto refsFor = [&](MCRegister Reg) -> SmallVectorImpl<MachineOperand *> & {
    for (auto &Entry : Refs)
      if (Entry.first == Reg)
        return Entry.second;
    Refs.push_back({Reg, {}});
    return Refs.back().second;
  };
  auto clearRefs = [&](MCRegister Reg) {
    for (auto &Entry : Refs)
      if (Entry.first == Reg) {
        Entry.second.clear();
        for (MCRegAliasIterator AI(Reg, TRI, true); AI.isValid(); ++AI)
          if (*AI != Reg)
            for (auto &Other : Refs)
              if (Other.first == *AI)
                Other.second.clear();
        return;
      }
  };

  // Forward pre-pass: which definitions have an anti-dependency, i.e. which
  // registers (or registers overlapping them) already had a definition earlier
  // in this block?  Purely syntactic, like the vendor's.
  SmallPtrSet<const MachineOperand *, 16> AntiDepDefs;
  {
    // Register *units*, not alias sets: V8 and V9 share the super-register
    // V8M2, but a definition of one says nothing about the other.
    BitVector Defined(TRI->getNumRegUnits());
    for (MachineInstr &MI : MBB) {
      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isDef())
          continue;
        MCRegister Reg = MO.getReg();
        if (!Reg || !vectorRegClassOf(Reg))
          continue;
        bool AliasDefined = false;
        for (MCRegUnitIterator UI(Reg, TRI); UI.isValid(); ++UI)
          if (Defined.test(*UI)) {
            AliasDefined = true;
            break;
          }
        if (AliasDefined) {
          AntiDepDefs.insert(&MO);
          LLVM_DEBUG(dbgs() << "TX8VR-pre: anti-dep def "
                            << printReg(Reg, TRI) << " at " << MI.getOpcode()
                            << "\n");
        }
        for (MCRegUnitIterator UI(Reg, TRI); UI.isValid(); ++UI)
          Defined.set(*UI);
      }
    }
  }

  // Registers whose current value feeds the (live) passthru of a later
  // instruction: that value must stay in place.  One-shot, consumed by the
  // definition that produces it.
  SmallVector<MCRegister, 4> ProtectRegs;
  auto isProtected = [&](MCRegister Reg) {
    return overlapsAny(Reg, ProtectRegs);
  };
  auto addProtect = [&](MCRegister Reg) {
    if (!isProtected(Reg))
      ProtectRegs.push_back(Reg);
  };
  auto dropProtect = [&](MCRegister Reg) {
    ProtectRegs.erase(std::remove_if(ProtectRegs.begin(), ProtectRegs.end(),
                                     [&](MCRegister P) {
                                       return TRI->regsOverlap(Reg, P);
                                     }),
                      ProtectRegs.end());
  };

  LivePhysRegs Live(*TRI);
  Live.addLiveOuts(MBB);
  bool Changed = false;

  for (MachineBasicBlock::reverse_iterator RIt = MBB.rbegin(), RE = MBB.rend();
       RIt != RE; ++RIt) {
    MachineInstr &MI = *RIt;
    if (MI.isDebugInstr() || MI.isKill())
      continue;

    // Tied passthru operands that read a *live* value: the merge value of a
    // masked operation.  Collected from the original operands.
    SmallVector<MCRegister, 4> LivePassthru;
    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isUse() || MO.isUndef())
        continue;
      if (!MO.isTied() || !MI.isRegTiedToDefOperand(MO.getOperandNo()))
        continue;
      if (MO.getReg() && vectorRegClassOf(MO.getReg()))
        LivePassthru.push_back(MO.getReg());
    }

    // Snapshot the defined registers before any rewriting: the cleanup below
    // has to clear the state of the *original* registers.
    SmallVector<MCRegister, 4> DefRegs;
    SmallVector<MCRegister, 2> RenamedNew;
    for (MachineOperand &MO : MI.operands())
      if (MO.isReg() && MO.isDef() && MO.getReg())
        DefRegs.push_back(MO.getReg());

    // --- definitions ----------------------------------------------------
    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isDef())
        continue;
      MCRegister Reg = MO.getReg();
      if (!Reg || !MRI.isAllocatable(Reg))
        continue;
      if (!vectorRegClassOf(Reg))
        continue;
      // Only an anti-dependency (WAW) is renamed.
      bool IsAnti = AntiDepDefs.count(&MO);
      LLVM_DEBUG(dbgs() << "TX8VR: def " << printReg(Reg, TRI)
                        << " opc=" << MI.getOpcode() << " anti=" << IsAnti
                        << " prot=" << isProtected(Reg) << "\n");
      if (!IsAnti)
        continue;
      // The mask register keeps its values.
      if (TRI->regsOverlap(Reg, RISCV::V0))
        continue;
      // A live passthru of this very instruction has to keep reading `Reg`.
      if (overlapsAny(Reg, LivePassthru))
        continue;
      // A live passthru of a later instruction consumes the protection.
      if (isProtected(Reg)) {
        dropProtect(Reg);
        continue;
      }
      // The value currently in Reg has to stay there.
      if (isKept(Reg)) {
        LLVM_DEBUG(dbgs() << "TX8VR:   keep (live-out)\n");
        continue;
      }

      const TargetRegisterClass *RC = getOperandRegClass(MI, MO.getOperandNo());
      if (!RC)
        RC = vectorRegClassOf(Reg);
      if (!RC) {
        LLVM_DEBUG(dbgs() << "TX8VR:   no reg class\n");
        continue;
      }
      LLVM_DEBUG(dbgs() << "TX8VR:   RC=" << TRI->getRegClassName(RC) << "\n");

      SmallVector<MCRegister, 4> Forbid;
      for (MachineOperand &Other : MI.operands())
        if (Other.isReg() && Other.isDef() && Other.getReg() != Reg)
          Forbid.push_back(Other.getReg());

      MCRegister NewReg =
          findSuitableFreeRegister(RC, Reg, Live, Forbid, Cursors);
      LLVM_DEBUG({
        dbgs() << "TX8VR:   -> free reg ";
        if (NewReg)
          dbgs() << printReg(NewReg, TRI);
        else
          dbgs() << "none";
        dbgs() << "\n";
      });
      if (!NewReg)
        continue;

      // Renaming an operand also drops its "killed"/"renamable" annotations:
      // the operand no longer refers to the value it described.
      auto renameOp = [](MachineOperand &O, MCRegister R) {
        O.setReg(R);
        if (O.isUse())
          O.setIsKill(false);
        O.setIsRenamable(false);
      };

      renameOp(MO, NewReg);
      const unsigned OpNo = MO.getOperandNo();
      for (unsigned I = 0, E = MI.getNumOperands(); I != E; ++I) {
        MachineOperand &Other = MI.getOperand(I);
        if (!Other.isReg() || !Other.isUse() || Other.getReg() != Reg)
          continue;
        if (Other.isTied() && MI.findTiedOperandIdx(I) == OpNo)
          renameOp(Other, NewReg);
      }
      for (auto &Entry : Refs)
        if (Entry.first == Reg)
          for (MachineOperand *Ref : Entry.second)
            renameOp(*Ref, NewReg);
      // The new register inherits the renamed value; any stale reference list
      // for it must not survive.
      for (MCRegAliasIterator AI(NewReg, TRI, true); AI.isValid(); ++AI)
        clearRefs(*AI);

      // The value that used to live in Reg now lives in NewReg: the vendor
      // moves the liveness state across.
      Live.removeReg(Reg);
      Live.addReg(NewReg);
      RenamedNew.push_back(NewReg);

      Changed = true;
    }

    // Every definition starts a new value: drop the references and the
    // "must stay in place" state of the previous value.
    for (MCRegister Reg : DefRegs) {
      clearRefs(Reg);
      clearKeep(Reg);
      for (MCRegAliasIterator AI(Reg, TRI, true); AI.isValid(); ++AI)
        clearRefs(*AI);
    }

    // --- uses -----------------------------------------------------------
    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isUse())
        continue;
      MCRegister Reg = MO.getReg();
      if (!Reg || !vectorRegClassOf(Reg))
        continue;
      // The operand tied to a definition (the passthru of an RVV pseudo)
      // belongs to that definition, not to the value defined above it.
      if (MO.isTied() && MI.isRegTiedToDefOperand(MO.getOperandNo()))
        continue;
      refsFor(Reg).push_back(&MO);
      if (MO.isImplicit())
        keepUnit(Reg);
    }

    // --- liveness -------------------------------------------------------
    for (MachineOperand &MO : reverse(MI.operands())) {
      if (!MO.isReg())
        continue;
      MCRegister Reg = MO.getReg();
      if (!Reg)
        continue;
      if (MO.isDef()) {
        // A renamed definition keeps its (still live) value in the new
        // register.
        if (llvm::is_contained(RenamedNew, Reg))
          continue;
        Live.removeReg(Reg);
      } else if (MO.isUse()) {
        Live.addReg(Reg);
      }
    }

    // --- merge passthru protection --------------------------------------
    for (MCRegister Reg : LivePassthru)
      addProtect(Reg);
  }
  return Changed;
}

bool VRegReNamer::run(
    DenseMap<const TargetRegisterClass *, RegClassCursor> &Cursors) {
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    // The per-class cursor is reset for every basic block (the vendor reuses
    // the same DenseMap storage across blocks, but restarts the search each
    // time).
    Cursors.clear();
    Changed |= breakAntiDeps(MBB, Cursors);
  }
  return Changed;
}

/// Vendor `RestoreAlignedCTInstruction`: the `*_ALIGNED_*` pseudos only exist
/// between `vreg-align` and this pass.  Turn them back into their plain twins,
/// narrow the operand `vreg-align` widened back to its narrow sub-register, and
/// let the value flow like any other M1 operand.  (The vendor also appends an
/// `implicit $align2` marker; this tree has no such register, and the marker is
/// not observable in the emitted assembly.)
bool restoreAlignedCTInstructions(MachineFunction &MF,
                                  const RISCVInstrInfo *TII,
                                  const RISCVRegisterInfo *TRI) {
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      const AlignedPseudoInfo *Info = lookupAlignedPseudo(MI.getOpcode());
      if (!Info)
        continue;
      if (Info->OpNo < MI.getNumOperands()) {
        MachineOperand &MO = MI.getOperand(Info->OpNo);
        if (MO.isReg() && MO.getReg()) {
          MCRegister Narrow = TRI->getSubReg(MO.getReg(), Info->WideSubReg);
          if (Narrow)
            MO.setReg(Narrow);
        }
      }
      MI.setDesc(TII->get(Info->Plain));
      Changed = true;
    }
  }
  return Changed;
}

class TX8RegisterRename : public MachineFunctionPass {
public:
  static char ID;
  TX8RegisterRename() : MachineFunctionPass(ID) {
    initializeTX8RegisterRenamePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "TX8 Rename Registers"; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char TX8RegisterRename::ID = 0;

INITIALIZE_PASS(TX8RegisterRename, "vreg-rename", "TX8 Rename Registers", false,
                false)

bool TX8RegisterRename::runOnMachineFunction(MachineFunction &MF) {
  const RISCVSubtarget &ST = MF.getSubtarget<RISCVSubtarget>();
  if (!ST.hasVInstructions())
    return false;

  RegisterClassInfo RCI;
  RCI.runOnMachineFunction(MF, false);

  const RISCVInstrInfo *TII = ST.getInstrInfo();
  const RISCVRegisterInfo *TRI = ST.getRegisterInfo();

  bool Changed = restoreAlignedCTInstructions(MF, TII, TRI);

  VRegReNamer Renamer(MF, RCI);
  DenseMap<const TargetRegisterClass *, RegClassCursor> Cursors;
  Changed |= Renamer.run(Cursors);
  return Changed;
}

FunctionPass *llvm::createTX8RegisterRenamePass() {
  return new TX8RegisterRename();
}

bool llvm::isTX8VRegRenameEnabled() { return EnableTX8VRegRename; }
