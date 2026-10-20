//===-- TX8OptimizeConfigInstrs.cpp - TX8 config instruction optimiser ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Reconstruction of the vendor fork pass `config-instr-opt`
// (`(anonymous namespace)::TX8OptimizeConfigInstrs`, a legacy **FunctionPass**
// registered as `config-instr-opt`, display name "TX8 Optimize Configuration
// Instructions", RAISA triple only).
//
// Provenance of the semantics (all of it is extracted from vendor artefacts,
// nothing is guessed):
//
//  * `ConfigIntrs[]` and `ConfigUseGroups[0..14]` below are the *actual* tables
//    in the vendor object: `docs/compiler/fork-source/backend-objects/
//    TX8OptimizeConfigInstrs.cpp.o` carries the symbols
//    `(anonymous namespace)::ConfigIntrs` (20 x {u32 Intrinsic::ID, u32 use
//    group}) and `ConfigUseGroup{0..14}` (u32 Intrinsic::ID arrays).  Their
//    `.rodata` bytes were dumped with `objdump -s` and the intrinsic ids were
//    resolved to names against the vendor's installed
//    `llvm/include/llvm/IR/IntrinsicEnums.inc` / `IntrinsicsRISCV.h`.
//  * The only string constants in that object are "config-instr-opt",
//    "TX8 Optimize Configuration Instructions" and **"config.opt.preheader"**,
//    which is the name of the basic block the pass creates when a loop has no
//    proper preheader.
//  * Behaviour was pinned with ~60 controlled probes compiled by the vendor
//    clang with `-mllvm -print-before=config-instr-opt
//    -mllvm -print-after=config-instr-opt [-mllvm -print-module-scope]`.
//
// What the pass does, as observed:
//
//  1. **Redundant configuration writes.**  A call to a configuration intrinsic
//     is deleted when the value it writes is provably already the value of that
//     configuration slot at that point:
//       * the slot key is the **intrinsic id** (`ne.set.mkn` and `ne.set.prec`
//         are different slots even though they share use-group 0);
//       * a write of a *different* value to the same slot invalidates, a write
//         of the same value keeps it redundant: `mkn(1,2,3); mkn(4,5,6);
//         mkn(1,2,3)` keeps all three, `mkn(1,2,3); prec(9,9,9); mkn(1,2,3)`
//         deletes the third;
//       * every non-configuration instruction is transparent, including
//         `llvm.riscv.inst.barrier`, plain stores and `asm sideeffect "~{memory}"`
//         (probes w1/w2) -- the analysis only tracks configuration intrinsics;
//       * a path that reaches the call without *any* configuration write to that
//         slot blocks the deletion (the slot could hold anything), which makes
//         the analysis dominance/CFG scoped rather than block local:
//         `entry: mkn(1,2,3); if (a) { prec; mkn(1,2,3); }` deletes the arm call,
//         `if (a) { mkn(1,2,3); } ; mkn(1,2,3)` keeps the second one.
//  2. **Hoisting out of loops.**  When a configuration call inside a loop is
//     loop invariant, is guaranteed to execute and dominates every use of its
//     use-group inside the loop (and the loop body contains no later write to
//     the same slot), it is moved to the loop's preheader -- a new basic block
//     named `config.opt.preheader` is created when the loop predecessor is not a
//     proper preheader.  Only use-group of the *first* configuration call of the
//     loop is considered, and one candidate is taken per slot, in `ConfigIntrs`
//     order.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "config-instr-opt"

namespace {

//===----------------------------------------------------------------------===//
// The vendor's tables, verbatim.
//===----------------------------------------------------------------------===//

struct ConfigIntrInfo {
  Intrinsic::ID ID;
  /// Index into ConfigUseGroups: the set of instructions that read the
  /// configuration state this intrinsic writes.
  unsigned UseGroup;
};

static const ConfigIntrInfo ConfigIntrs[] = {
    {Intrinsic::riscv_ne_set_length, 0},
    {Intrinsic::riscv_ne_set_misc, 0},
    {Intrinsic::riscv_ne_set_mkn, 0},
    {Intrinsic::riscv_ne_set_prec, 0},
    {Intrinsic::riscv_ne_set_conv_pad, 1},
    {Intrinsic::riscv_ne_set_conv_swr, 1},
    {Intrinsic::riscv_ne_set_h_oh_ow, 1},
    {Intrinsic::riscv_lsu_set_length, 2},
    {Intrinsic::riscv_lsu_set_rdma_src_itera_stride, 3},
    {Intrinsic::riscv_lsu_set_rdma_dst_itera_stride, 4},
    {Intrinsic::riscv_lsu_set_wdma_src_itera_stride, 5},
    {Intrinsic::riscv_lsu_set_wdma_dst_itera_stride, 6},
    {Intrinsic::riscv_lsu_set_tdma_hwc, 7},
    {Intrinsic::riscv_dte_set_length, 8},
    {Intrinsic::riscv_dte_set_rdma_misc, 9},
    {Intrinsic::riscv_dte_set_rdma_src_itera_stride, 10},
    {Intrinsic::riscv_dte_set_rdma_dst_itera_stride, 11},
    {Intrinsic::riscv_dte_set_wdma_misc, 12},
    {Intrinsic::riscv_dte_set_wdma_src_itera_stride, 13},
    {Intrinsic::riscv_dte_set_wdma_dst_itera_stride, 14},
};

static const Intrinsic::ID ConfigUseGroup0[] = {
    Intrinsic::riscv_gemm,        Intrinsic::riscv_gemm_psum,
    Intrinsic::riscv_mma,         Intrinsic::riscv_mma_first,
    Intrinsic::riscv_mma_first_psum, Intrinsic::riscv_mma_last,
    Intrinsic::riscv_conv,        Intrinsic::riscv_conv_psum,
    Intrinsic::riscv_convbw,      Intrinsic::riscv_convbw_psum};
static const Intrinsic::ID ConfigUseGroup1[] = {
    Intrinsic::riscv_conv, Intrinsic::riscv_conv_psum,
    Intrinsic::riscv_convbw, Intrinsic::riscv_convbw_psum};
static const Intrinsic::ID ConfigUseGroup2[] = {
    Intrinsic::riscv_lsu_rdma_gather,
    Intrinsic::riscv_lsu_rdma_scatter,
    Intrinsic::riscv_lsu_rdma_gather_scatter,
    Intrinsic::riscv_lsu_rdma_index_gather,
    Intrinsic::riscv_lsu_rdma_mem_copy,
    Intrinsic::riscv_lsu_rdma_mem_set,
    Intrinsic::riscv_lsu_wdma_gather,
    Intrinsic::riscv_lsu_wdma_scatter,
    Intrinsic::riscv_lsu_wdma_gather_scatter,
    Intrinsic::riscv_lsu_wdma_index_scatter,
    Intrinsic::riscv_lsu_wdma_mem_copy,
    Intrinsic::riscv_lsu_wdma_mem_set,
    Intrinsic::riscv_lsu_tdma_nhwc2nchw,
    Intrinsic::riscv_lsu_tdma_nchw2nhwc,
    Intrinsic::riscv_lsu_tdma_bank_scatter,
    Intrinsic::riscv_lsu_tdma_mem_copy};
static const Intrinsic::ID ConfigUseGroup3[] = {
    Intrinsic::riscv_lsu_rdma_gather,
    Intrinsic::riscv_lsu_rdma_gather_scatter};
static const Intrinsic::ID ConfigUseGroup4[] = {
    Intrinsic::riscv_lsu_rdma_scatter,
    Intrinsic::riscv_lsu_rdma_gather_scatter};
static const Intrinsic::ID ConfigUseGroup5[] = {
    Intrinsic::riscv_lsu_wdma_gather,
    Intrinsic::riscv_lsu_wdma_gather_scatter};
static const Intrinsic::ID ConfigUseGroup6[] = {
    Intrinsic::riscv_lsu_wdma_scatter,
    Intrinsic::riscv_lsu_wdma_gather_scatter};
static const Intrinsic::ID ConfigUseGroup7[] = {
    Intrinsic::riscv_lsu_tdma_nhwc2nchw,
    Intrinsic::riscv_lsu_tdma_nchw2nhwc};
static const Intrinsic::ID ConfigUseGroup8[] = {
    Intrinsic::riscv_dte_gather,
    Intrinsic::riscv_dte_scatter,
    Intrinsic::riscv_dte_gather_scatter_g2l,
    Intrinsic::riscv_dte_gather_scatter_l2g,
    Intrinsic::riscv_dte_index_gather,
    Intrinsic::riscv_dte_index_scatter};
static const Intrinsic::ID ConfigUseGroup9[] = {
    Intrinsic::riscv_dte_gather,
    Intrinsic::riscv_dte_gather_scatter_g2l,
    Intrinsic::riscv_dte_index_gather};
static const Intrinsic::ID ConfigUseGroup10[] = {
    Intrinsic::riscv_dte_gather,
    Intrinsic::riscv_dte_gather_scatter_g2l};
static const Intrinsic::ID ConfigUseGroup11[] = {
    Intrinsic::riscv_dte_gather_scatter_g2l};
static const Intrinsic::ID ConfigUseGroup12[] = {
    Intrinsic::riscv_dte_scatter,
    Intrinsic::riscv_dte_gather_scatter_l2g,
    Intrinsic::riscv_dte_index_scatter};
static const Intrinsic::ID ConfigUseGroup13[] = {
    Intrinsic::riscv_dte_gather_scatter_l2g};
static const Intrinsic::ID ConfigUseGroup14[] = {
    Intrinsic::riscv_dte_scatter,
    Intrinsic::riscv_dte_gather_scatter_l2g};

static const ArrayRef<Intrinsic::ID> ConfigUseGroups[] = {
    ConfigUseGroup0,  ConfigUseGroup1,  ConfigUseGroup2,  ConfigUseGroup3,
    ConfigUseGroup4,  ConfigUseGroup5,  ConfigUseGroup6,  ConfigUseGroup7,
    ConfigUseGroup8,  ConfigUseGroup9,  ConfigUseGroup10, ConfigUseGroup11,
    ConfigUseGroup12, ConfigUseGroup13, ConfigUseGroup14};

constexpr unsigned NumConfigIntrs = std::size(ConfigIntrs);
constexpr unsigned NumConfigUseGroups = std::size(ConfigUseGroups);

/// Configuration slot of \p ID, or -1 when \p ID is not a configuration
/// intrinsic.
static int configSlotOf(Intrinsic::ID ID) {
  for (unsigned I = 0; I != NumConfigIntrs; ++I)
    if (ConfigIntrs[I].ID == ID)
      return (int)I;
  return -1;
}

/// Is \p ID one of the instructions that read use-group \p Group?
static bool isUseOfGroup(Intrinsic::ID ID, unsigned Group) {
  assert(Group < NumConfigUseGroups && "bad use group");
  for (Intrinsic::ID Use : ConfigUseGroups[Group])
    if (Use == ID)
      return true;
  return false;
}

/// Do the two calls write exactly the same configuration value?  The vendor
/// compares the callee and every argument operand by identity (constants are
/// uniqued, so two structurally identical calls in different blocks compare
/// equal).
static bool sameConfigValue(const CallBase *A, const CallBase *B) {
  if (A->getCalledOperand() != B->getCalledOperand())
    return false;
  if (A->arg_size() != B->arg_size())
    return false;
  for (unsigned I = 0, E = A->arg_size(); I != E; ++I)
    if (A->getArgOperand(I) != B->getArgOperand(I))
      return false;
  return true;
}

/// The configuration call itself, or null.
static CallInst *asConfigCall(Instruction *I) {
  auto *CI = dyn_cast<CallInst>(I);
  if (!CI || CI->isInlineAsm())
    return nullptr;
  Function *Callee = CI->getCalledFunction();
  if (!Callee || configSlotOf(Callee->getIntrinsicID()) < 0)
    return nullptr;
  return CI;
}

//===----------------------------------------------------------------------===//
// Redundant configuration write elimination.
//===----------------------------------------------------------------------===//

/// Per-slot abstract value of the "last configuration write" dataflow.
struct SlotVal {
  enum KindT {
    /// No information yet (unreached block).
    Uninit,
    /// Some path arrives without having written this slot.
    None,
    /// Every arriving path was last written by a call structurally identical to
    /// \p Rep.
    One,
    /// Arriving paths disagree.
    Many
  } Kind = Uninit;
  CallInst *Rep = nullptr;
};

static SlotVal meetSlot(SlotVal A, SlotVal B) {
  if (A.Kind == SlotVal::Uninit)
    return B;
  if (B.Kind == SlotVal::Uninit)
    return A;
  if (A.Kind == SlotVal::None || B.Kind == SlotVal::None)
    return {SlotVal::None, nullptr};
  if (A.Kind == SlotVal::Many || B.Kind == SlotVal::Many)
    return {SlotVal::Many, nullptr};
  if (A.Rep == B.Rep || sameConfigValue(A.Rep, B.Rep))
    return A;
  return {SlotVal::Many, nullptr};
}

//===----------------------------------------------------------------------===//
// The pass.
//===----------------------------------------------------------------------===//

class TX8OptimizeConfigInstrs : public FunctionPass {
public:
  static char ID;

  TX8OptimizeConfigInstrs() : FunctionPass(ID) {
    initializeTX8OptimizeConfigInstrsPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "TX8 Optimize Configuration Instructions";
  }

  /// Same dependency set as the vendor (see its `getAnalysisUsage`).
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesCFG();
  }

  bool runOnFunction(Function &F) override;

private:
  /// Collect the configuration calls whose write is already the value of the
  /// slot on every path that reaches them.
  void collectRedundantWrites(Function &F, SmallPtrSetImpl<CallInst *> &Out);

  /// Plan the loop hoists.  `Redundant` are the calls that will be deleted and
  /// are therefore not hoist candidates.
  void planHoists(Loop &L, const SmallPtrSetImpl<CallInst *> &Redundant,
                  SmallVectorImpl<CallInst *> &Out);
};

} // end anonymous namespace

char TX8OptimizeConfigInstrs::ID = 0;

INITIALIZE_PASS_BEGIN(TX8OptimizeConfigInstrs, "config-instr-opt",
                      "TX8 Optimize Configuration Instructions", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(TX8OptimizeConfigInstrs, "config-instr-opt",
                    "TX8 Optimize Configuration Instructions", false, false)

FunctionPass *llvm::createTX8OptimizeConfigInstrsPass() {
  return new TX8OptimizeConfigInstrs();
}

void TX8OptimizeConfigInstrs::collectRedundantWrites(
    Function &F, SmallPtrSetImpl<CallInst *> &Out) {
  using State = SmallVector<SlotVal, NumConfigIntrs>;

  DenseMap<BasicBlock *, State> In;

  // Forward "reaching configuration definition" dataflow.  Only reachable
  // blocks get an entry, so unreachable code is left alone.  The lattice is
  // finite and monotone (Uninit -> One/None -> None/Many per slot), so the
  // worklist terminates.
  //
  // Phase 1 is the fixpoint only -- *no* decision may be taken here.  A block
  // can be reached (and processed) through one predecessor before another
  // predecessor has contributed its state; deciding early would treat
  // `entry: mkn(1,2,3); if (a) { mkn(1,2,3); }` as redundant, which the vendor
  // does not (`p_branch_one` / `p_x_block_rev` probes).
  BasicBlock *Entry = &F.getEntryBlock();
  In.try_emplace(Entry, NumConfigIntrs, SlotVal{SlotVal::None, nullptr});

  SmallVector<BasicBlock *, 16> Worklist{Entry};
  SmallPtrSet<BasicBlock *, 32> Queued{Entry};

  while (!Worklist.empty()) {
    BasicBlock *BB = Worklist.pop_back_val();
    Queued.erase(BB);

    State Cur = In.find(BB)->second;

    for (Instruction &I : *BB) {
      CallInst *CI = asConfigCall(&I);
      if (CI)
        Cur[configSlotOf(CI->getCalledFunction()->getIntrinsicID())] =
            SlotVal{SlotVal::One, CI};
    }

    for (BasicBlock *Succ : successors(BB)) {
      auto It = In.find(Succ);
      if (It == In.end()) {
        In.try_emplace(Succ, Cur);
        if (Queued.insert(Succ).second)
          Worklist.push_back(Succ);
        continue;
      }
      State &InSucc = It->second;
      bool Changed = false;
      for (unsigned S = 0; S != NumConfigIntrs; ++S) {
        SlotVal Merged = meetSlot(InSucc[S], Cur[S]);
        if (Merged.Kind != InSucc[S].Kind || Merged.Rep != InSucc[S].Rep) {
          InSucc[S] = Merged;
          Changed = true;
        }
      }
      if (Changed && Queued.insert(Succ).second)
        Worklist.push_back(Succ);
    }
  }

  // Phase 2: walk the converged state and record the redundant calls.  Iterate
  // in reverse post-order so that the recorded set does not depend on hash
  // order.
  SmallVector<BasicBlock *, 32> Order;
  for (BasicBlock *BB : post_order(&F))
    Order.push_back(BB);
  std::reverse(Order.begin(), Order.end());
  Order.erase(std::remove_if(Order.begin(), Order.end(),
                             [&](BasicBlock *BB) { return !In.count(BB); }),
              Order.end());

  for (BasicBlock *BB : Order) {
    State Cur = In.find(BB)->second;
    for (Instruction &I : *BB) {
      CallInst *CI = asConfigCall(&I);
      if (!CI)
        continue;
      unsigned Slot =
          (unsigned)configSlotOf(CI->getCalledFunction()->getIntrinsicID());
      const SlotVal &V = Cur[Slot];
      if (V.Kind == SlotVal::One && sameConfigValue(V.Rep, CI))
        Out.insert(CI);
      Cur[Slot] = SlotVal{SlotVal::One, CI};
    }
  }
}

/// Does \p BB live in a sub-loop of \p L rather than in \p L itself?
static bool inSubLoopOf(const Loop &L, const BasicBlock *BB) {
  for (const Loop *Sub : L.getSubLoops())
    if (Sub->contains(BB))
      return true;
  return false;
}

void TX8OptimizeConfigInstrs::planHoists(
    Loop &L, const SmallPtrSetImpl<CallInst *> &Redundant,
    SmallVectorImpl<CallInst *> &Out) {
  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

  // Configuration calls that live in this loop's own blocks (not in a
  // sub-loop), in program order, ignoring the calls that will be deleted.
  SmallVector<CallInst *, 8> OwnConfigs;
  for (BasicBlock *BB : L.blocks()) {
    if (inSubLoopOf(L, BB))
      continue;
    for (Instruction &I : *BB) {
      CallInst *CI = asConfigCall(&I);
      if (CI && !Redundant.count(CI))
        OwnConfigs.push_back(CI);
    }
  }
  if (OwnConfigs.empty())
    return;

  // Only the use-group of the loop's first configuration call takes part.
  unsigned Group =
      ConfigIntrs[configSlotOf(
          OwnConfigs.front()->getCalledFunction()->getIntrinsicID())]
          .UseGroup;

  SimpleLoopSafetyInfo LSI;
  LSI.computeLoopSafetyInfo(&L);

  // Uses of `Group` anywhere in the loop (sub-loops included).
  SmallVector<Instruction *, 8> GroupUses;
  for (BasicBlock *BB : L.blocks())
    for (Instruction &I : *BB) {
      auto *CI = dyn_cast<CallInst>(&I);
      if (!CI || CI->isInlineAsm())
        continue;
      Function *Callee = CI->getCalledFunction();
      if (Callee && isUseOfGroup(Callee->getIntrinsicID(), Group))
        GroupUses.push_back(&I);
    }

  // One candidate per slot, in the vendor's table order.
  for (unsigned S = 0; S != NumConfigIntrs; ++S) {
    if (ConfigIntrs[S].UseGroup != Group)
      continue;

    CallInst *Candidate = nullptr;
    unsigned NumWrites = 0;
    for (BasicBlock *BB : L.blocks())
      for (Instruction &I : *BB) {
        CallInst *CI = asConfigCall(&I);
        if (!CI || Redundant.count(CI))
          continue;
        if (CI->getCalledFunction()->getIntrinsicID() != ConfigIntrs[S].ID)
          continue;
        ++NumWrites;
        if (!Candidate && !inSubLoopOf(L, BB))
          Candidate = CI;
      }
    if (!Candidate || NumWrites != 1)
      continue;

    // Loop invariant, guaranteed to execute, and dominating every use.
    if (!L.hasLoopInvariantOperands(Candidate))
      continue;
    if (!LSI.isGuaranteedToExecute(*Candidate, &DT, &L))
      continue;
    bool DominatesAllUses = true;
    for (Instruction *U : GroupUses) {
      if (U == Candidate)
        continue;
      if (!DT.dominates(Candidate, U)) {
        DominatesAllUses = false;
        break;
      }
    }
    if (!DominatesAllUses)
      continue;

    Out.push_back(Candidate);
  }
}

bool TX8OptimizeConfigInstrs::runOnFunction(Function &F) {
  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  SmallPtrSet<CallInst *, 16> Redundant;
  collectRedundantWrites(F, Redundant);

  // Plan the loop hoists before mutating anything; the analysis above and the
  // planning both read the pre-transformation CFG.
  SmallVector<Loop *, 16> Loops;
  for (Loop *L : LI)
    Loops.push_back(L);
  // Pre-order so that a parent loop's plan is computed on the original IR too.
  for (unsigned I = 0; I != Loops.size(); ++I)
    for (Loop *Sub : Loops[I]->getSubLoops())
      Loops.push_back(Sub);

  struct HoistPlan {
    Loop *L;
    SmallVector<CallInst *, 4> Instrs;
  };
  SmallVector<HoistPlan, 4> Plans;
  for (Loop *L : Loops) {
    HoistPlan P;
    P.L = L;
    planHoists(*L, Redundant, P.Instrs);
    if (!P.Instrs.empty())
      Plans.push_back(std::move(P));
  }

  bool Changed = false;

  for (HoistPlan &P : Plans) {
    Loop *L = P.L;
    BasicBlock *Pred = L->getLoopPredecessor();
    if (!Pred)
      continue;

    BasicBlock *Preheader = Pred;
    if (!Pred->getSingleSuccessor()) {
      // Not a proper preheader: split the Pred -> header edge into a fresh
      // block, exactly like the vendor (`config.opt.preheader`).
      BasicBlock *Header = L->getHeader();
      BasicBlock *New = BasicBlock::Create(Header->getContext(),
                                           "config.opt.preheader",
                                           Header->getParent(), Header);
      BranchInst::Create(Header, New);
      Instruction *TI = Pred->getTerminator();
      bool Redirected = false;
      for (unsigned I = 0, E = TI->getNumSuccessors(); I != E; ++I) {
        if (TI->getSuccessor(I) == Header) {
          TI->setSuccessor(I, New);
          Redirected = true;
          break;
        }
      }
      if (!Redirected) {
        New->eraseFromParent();
        continue;
      }
      updatePhiNodes(Header, Pred, New);
      DT.addNewBlock(New, Pred);
      if (Loop *OuterL = LI.getLoopFor(Pred))
        OuterL->addBasicBlockToLoop(New, LI);
      Preheader = New;
    }

    Instruction *InsertPt = Preheader->getTerminator();
    for (CallInst *CI : P.Instrs)
      CI->moveBefore(InsertPt->getIterator());
    Changed = true;
  }

  if (!Redundant.empty()) {
    for (CallInst *CI : Redundant)
      if (CI->use_empty())
        CI->eraseFromParent();
    Changed = true;
  }

  return Changed;
}
