//===-- TX8LowerSharedPass.cpp - TX8 lower shared variables ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Reconstruction of the vendor's `rpu-lower-shared` pass
// (`TX8LowerSharedPass.cpp.o` / class `TX8LowerSharedLegacy`, a ModulePass,
// registered as `rpu-lower-shared`, display name "TX8 Lower Shared variables",
// inserted between `early-cse` and `callbrprepare`, RAISA only).
//
// What it does, established from the **vendor's own** `-print-before/-after=
// rpu-lower-shared` IR dumps (`wt-raisa-r2/recon/rpu_lower_shared/*.dump`, 28
// cells) and from controlled probes (`wt-raisa-r2/probe/*.rc`):
//
//  1. Every kernel-scope `__attribute__((l0ashared|l0wshared|l1shared))`
//     variable reaches this pass as an `internal addrspace(2|3|4) global`.
//     The pass gives each one a concrete absolute address and deletes the
//     global:
//
//       before:  %3 = lshr i64 ptrtoint (ptr addrspacecast (
//                         ptr addrspace(2) @_ZZ...E7scratch to ptr) to i64), 32
//       after:   %3 = addrspacecast ptr addrspace(2) inttoptr (
//                         i64 4311744512 to ptr addrspace(2)) to ptr
//                %4 = ptrtoint ptr %3 to i64
//                %5 = lshr i64 %4, 32
//
//  2. Base address per address space (verbatim from the vendor's pseudocode,
//     `tools/re/out/TX8LowerSharedPass/decompiled.c:490-492`, and confirmed by
//     every golden dump):
//       addrspace(3) L0A shared -> 0x40000000
//       addrspace(4) L0W shared -> 0x40100000
//       everything else (2 = L1 shared) -> 0x101000000
//
//  3. Stride rule (deterministic; verified by hand against every observed
//     layout, including a 3-variable probe):
//       offset = 0
//       for each variable, in order:
//           offset = align_up(offset, align_of(variable))   // its `align N`
//           address = base + offset
//           offset += alloc_size(variable->getValueType())
//
//  4. Constant-expression uses are materialised into instructions *before* the
//     global is replaced (`llvm::RPU::eliminateConstantExprUsesOfShared` in the
//     vendor's call graph).  That is exactly why the two instructions above
//     appear where a folded constant expression used to be: the operand chain
//     is expanded bottom-up (operand instructions first), which reproduces the
//     vendor's value numbering `%3/%4/%5` byte for byte.
//
// KNOWN, DELIBERATE DEVIATION -- the variable *order* is nondeterministic in the
// vendor: it iterates a pointer-keyed container, so the same input compiled
// twice can place two variables at `{0, 0x80}` or `{0x240, 0}`.  Measured 2-3
// distinct layouts per input over 8 runs; deterministic under `setarch -R`, but
// then the order is still not declaration order (see
// `wt-raisa-r2/tools/rpu_nondet.py`).  This reconstruction uses the **module
// declaration order**, which is deterministic and defensible; cells with >= 2
// shared variables in one address space are registered as VENDOR-NONDET (byte
// identity is not a well-defined target for them), while single-variable cells
// are byte-exact.
//
// `-riscv-bank-aware` selects the separate `bank-aware-shared` pass, which runs
// *before* this one (`TX8BankAwareShared.cpp`, which owns the option).  With the
// flag on, the four `*_bank_on` cells reach this pass with their L0A/L0W globals
// already replaced by banked `inttoptr (i32 ...)` constants, so there is nothing
// left for `rpu-lower-shared` to place there.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

#define DEBUG_TYPE "rpu-lower-shared"

namespace {

class TX8LowerSharedLegacy : public ModulePass {
public:
  static char ID;

  TX8LowerSharedLegacy() : ModulePass(ID) {
    initializeTX8LowerSharedLegacyPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "TX8 Lower Shared variables"; }

  bool runOnModule(Module &M) override;

private:
  bool isSharedVariableToLower(const GlobalVariable &GV) const;
  uint64_t baseAddressFor(unsigned AS) const;
  void materializeConstantExprUses(ArrayRef<GlobalVariable *> GVs);
};

} // end anonymous namespace

char TX8LowerSharedLegacy::ID = 0;

INITIALIZE_PASS(TX8LowerSharedLegacy, "rpu-lower-shared",
                "TX8 Lower Shared variables", false, false)

ModulePass *llvm::createTX8LowerSharedLegacyPass() {
  return new TX8LowerSharedLegacy();
}

/// Which globals the pass owns.  In the corpus every such variable is an
/// `internal addrspace(2|3|4) global [N x T] undef, align A` (the name is the
/// kernel-scope mangling `_ZZ<kernel>...E...`).  The vendor has three separate
/// predicates (`RPU::isL{0A,0W,1}SharedVariableToLower`); the observable rule
/// over all 28 golden cells and every probe is the one below.
bool TX8LowerSharedLegacy::isSharedVariableToLower(
    const GlobalVariable &GV) const {
  if (GV.isDeclaration())
    return false;
  unsigned AS = GV.getAddressSpace();
  if (AS != 2 && AS != 3 && AS != 4)
    return false;
  // Kernel-scope variables are the ones the frontend emits as internal/private
  // locals; a non-local shared variable is not statically allocatable.
  if (!GV.hasLocalLinkage())
    return false;
  // Unverified against a dynamic-L0A case (none exists in the corpus):
  // `RPU::isDynamicL0AShared` is not modelled.
  return true;
}

uint64_t TX8LowerSharedLegacy::baseAddressFor(unsigned AS) const {
  // Verbatim from the vendor pseudocode: `pSVar41 = 0x40000000;
  // if ((AS != 3) && (pSVar41 = 0x101000000, AS == 4)) pSVar41 = 0x40100000;`
  if (AS == 3)
    return 0x40000000ull;
  if (AS == 4)
    return 0x40100000ull;
  return 0x101000000ull;
}

/// Expand a constant expression (that transitively uses one of the globals
/// being lowered) into real instructions, bottom-up, in front of
/// `InsertBefore`.  The bottom-up order reproduces the vendor's numbering: the
/// innermost cast is emitted first, the outermost last, immediately before the
/// instruction that used the folded expression.
static Instruction *expandConstantExpr(ConstantExpr *CE,
                                      Instruction *InsertBefore) {
  Instruction *I = CE->getAsInstruction();
  for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i)
    if (auto *OpCE = dyn_cast<ConstantExpr>(I->getOperand(i)))
      I->setOperand(i, expandConstantExpr(OpCE, InsertBefore));
  I->insertBefore(InsertBefore);
  return I;
}

/// True if the constant transitively references any of `GVs`.
static bool referencesAny(const Constant *C,
                          const SmallPtrSetImpl<GlobalVariable *> &GVs) {
  if (auto *GV = dyn_cast<GlobalVariable>(C))
    return GVs.contains(const_cast<GlobalVariable *>(GV));
  for (const Use &U : C->operands())
    if (auto *OpC = dyn_cast<Constant>(U.get()))
      if (referencesAny(OpC, GVs))
        return true;
  return false;
}

void TX8LowerSharedLegacy::materializeConstantExprUses(
    ArrayRef<GlobalVariable *> GVs) {
  if (GVs.empty())
    return;
  SmallPtrSet<GlobalVariable *, 8> Set(GVs.begin(), GVs.end());

  // 1. Find every *instruction* whose operand is a constant expression that
  //    transitively uses a shared variable.
  SmallPtrSet<Instruction *, 16> Candidates;
  for (GlobalVariable *GV : GVs)
    for (User *U : GV->users()) {
      auto *CE = dyn_cast<ConstantExpr>(U);
      if (!CE)
        continue;
      SmallVector<ConstantExpr *, 8> Stack{CE};
      SmallPtrSet<ConstantExpr *, 8> Seen;
      while (!Stack.empty()) {
        ConstantExpr *Cur = Stack.pop_back_val();
        if (!Seen.insert(Cur).second)
          continue;
        for (User *CU : Cur->users()) {
          if (auto *Nested = dyn_cast<ConstantExpr>(CU))
            Stack.push_back(Nested);
          else if (auto *I = dyn_cast<Instruction>(CU))
            Candidates.insert(I);
        }
      }
    }
  if (Candidates.empty())
    return;

  // 2. Rewrite in program order so the emitted instruction order matches the
  //    vendor's (which walks the function and rewrites operand by operand).
  //    Each use site gets its own expansion: the vendor does not CSE them (see
  //    `mem_l1_size_1`, where the same address is materialised twice as
  //    %3/%4 and %7/%8).
  Module *M = GVs[0]->getParent();
  for (Function &F : *M) {
    if (F.isDeclaration())
      continue;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (!Candidates.contains(&I))
          continue;
        for (unsigned OpNo = 0, e = I.getNumOperands(); OpNo != e; ++OpNo) {
          auto *CE = dyn_cast<ConstantExpr>(I.getOperand(OpNo));
          if (!CE || !referencesAny(CE, Set))
            continue;
          Instruction *InsertBefore = &I;
          if (auto *PHI = dyn_cast<PHINode>(&I))
            InsertBefore = PHI->getIncomingBlock(OpNo)->getTerminator();
          I.setOperand(OpNo, expandConstantExpr(CE, InsertBefore));
        }
      }
    }
  }
}

bool TX8LowerSharedLegacy::runOnModule(Module &M) {
  // The vendor inserts this pass for RAISA only.
  if (!Triple(M.getTargetTriple()).isRaisa())
    return false;

  SmallVector<GlobalVariable *, 8> Lowered;
  for (GlobalVariable &GV : M.globals())
    if (isSharedVariableToLower(GV))
      Lowered.push_back(&GV);
  if (Lowered.empty())
    return false;

  const DataLayout &DL = M.getDataLayout();

  // Addresses: per address space, sequential with natural alignment, in module
  // declaration order (see the VENDOR-NONDET note in the file header).
  DenseMap<GlobalVariable *, uint64_t> Address;
  for (unsigned AS : {2u, 3u, 4u}) {
    uint64_t Offset = 0;
    for (GlobalVariable *GV : Lowered) {
      if (GV->getAddressSpace() != AS)
        continue;
      Offset = alignTo(Offset, GV->getAlign().valueOrOne().value());
      Address[GV] = baseAddressFor(AS) + Offset;
      Offset += DL.getTypeAllocSize(GV->getValueType());
    }
  }

  LLVM_DEBUG(dbgs() << "rpu-lower-shared: " << Lowered.size()
                    << " shared variable(s)\n");

  // Materialise constant-expression uses first, then replace the globals -- in
  // that order the replacement lands inside a plain `addrspacecast`
  // instruction, exactly as in the vendor's dump.
  materializeConstantExprUses(Lowered);

  LLVMContext &Ctx = M.getContext();
  for (GlobalVariable *GV : Lowered) {
    // The `inttoptr` constant is as wide as the *target* address space's
    // pointer (the vendor's `DataLayout::getPointerSpec`): the RAISA data
    // layout makes addrspace(3)/(4) 32-bit (`p3:32:32:32:32-p4:32:32:32:32`)
    // while addrspace(2) stays 64-bit, and every golden dump agrees
    // (`inttoptr (i32 1073741824 to ptr addrspace(3))` for L0A/L0W,
    //  `inttoptr (i64 4311744512 to ptr addrspace(2))` for L1).
    unsigned PtrBits = DL.getPointerSizeInBits(GV->getAddressSpace());
    Constant *Replacement = ConstantExpr::getIntToPtr(
        ConstantInt::get(Type::getIntNTy(Ctx, PtrBits), Address[GV]),
        GV->getType());
    GV->replaceAllUsesWith(Replacement);
    GV->eraseFromParent();
  }
  return true;
}
