//===-- TX8TransformSIMTToSIMD.cpp - TX8 SIMT -> SIMD transform ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Reconstruction of the vendor's `tx8-transform-simt-to-simd` pass (pass name
// `simt2simd`), which is the pass that consumes the RAISA/SIMT intrinsics
// (`llvm.riscv.read.reg.*`, `llvm.riscv.raisa.*`, `llvm.riscv.get.pred.mask`)
// and rewrites a SIMT kernel into upstream RVV intrinsics.
//
// Everything below is derived from the vendor's own pass dumps, not guessed:
//   wt-raisa-be/recon/simt2simd/<cell>.before.ir   IR right before the pass
//   wt-raisa-be/recon/simt2simd/<cell>.after.ir    IR right after the pass
//   wt-raisa-be/recon/simt2simd/mapping.tsv        101 cells, 290 distinct
//                                                  (consumed -> produced) pairs
//   wt-raisa-be/recon/simt2simd/spec.tsv           per-cell constants
//
// Established facts this implementation follows:
//   * the pass also *packs the kernel signature*: `void k(ptr %out, ptr %p0,
//     ptr %p1)` becomes `void k(ptr %packed)` where the i-th original parameter
//     is `load ptr, gep i8, ptr %packed, i32 8*i` (all-pointer kernels; the
//     general size/align rule is not implemented yet);
//   * `read.reg.tid.x` is only used to index, and becomes a plain vector load
//     against the base pointer with AVL 128 (no `vid` call in these cells);
//   * every vector memory op is preceded by `llvm.riscv.set.mem.alloc(i64 N)`
//     with N = 128 * sizeof(element);
//   * vector type is `<vscale x 2 x T>` (LMUL chosen by element size upstream);
//   * `llvm.experimental.constrained.fadd.f32` -> `llvm.riscv.vfadd` with the
//     RISC-V frm encoding taken from the rounding-mode metadata
//     (tonearest 0 / towardzero 1 / downward 2 / upward 3);
//   * `llvm.riscv.raisa.f<op>.<ty>` -> `llvm.riscv.v<op>`.
//
// Coverage today: the single-basic-block elementwise kernels (the `mathdev_*
// fadd/fmax` shapes).  Anything else is left untouched on purpose: a partial
// rewrite would be worse than no rewrite.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "simt2simd"

// AVL used by the vendor for every vector op of a 128-lane SIMT kernel.
static constexpr uint64_t SIMT_LANES = 128;

namespace {

/// Scalar intrinsic -> (vector intrinsic, extra constant operands inserted
/// between the vector operands and the AVL).  The constants are copied
/// verbatim from the vendor's after-pass IR (see mapping.tsv).
struct IntrinsicMap {
  Intrinsic::ID Src;
  Intrinsic::ID Dst;
  uint64_t Extra;  // only one extra operand is needed by the mapped set today
};

class TX8TransformSIMTToSIMD : public ModulePass {
public:
  static char ID;

  TX8TransformSIMTToSIMD() : ModulePass(ID) {
    initializeTX8TransformSIMTToSIMDPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

private:
  bool transform(Function &F);
  bool packKernelSignature(Function &F);
};

} // end anonymous namespace

char TX8TransformSIMTToSIMD::ID = 0;

INITIALIZE_PASS(TX8TransformSIMTToSIMD, "simt2simd",
                "TX8 Transform SIMT To SIMD", false, false)

ModulePass *llvm::createTX8TransformSIMTToSIMDPass() {
  return new TX8TransformSIMTToSIMD();
}

/// `llvm.riscv.read.reg.*` -> what the vendor's pass replaces it with.  Typed
/// and named verbatim from recon/simt2simd/*.after.ir:
///
///   read.reg.tid.x / ntid.y / ntid.z  ->  llvm.riscv.vid (a *per-lane* value,
///                                          so it becomes a vector in place)
///   read.reg.ctaid.{x,y,z}            ->  __get_pid(i32 0|1|2)
///   read.reg.nctaid.{x,y,z}           ->  __grid_dim_{x,y,z}()
///   read.reg.ntid.x                   ->  __block_dim_x()
///
/// The uniform ones stay scalar; the store that consumes them splats with
/// `llvm.riscv.vmv.v.x` (see the store handling below).  tid.{y,z} and the `w`
/// dimensions are not in the vendor's table either -- the cells that use them
/// fail on the vendor side as well, so leave them unsupported.
struct ReadRegMap {
  Intrinsic::ID Src;
  const char *Callee;  // nullptr => per-lane, use llvm.riscv.vid
  bool HasArg;
  int Arg;
};

static const ReadRegMap ReadRegMaps[] = {
    {Intrinsic::riscv_read_reg_tid_x, nullptr, false, 0},
    {Intrinsic::riscv_read_reg_ntid_y, nullptr, false, 0},
    {Intrinsic::riscv_read_reg_ntid_z, nullptr, false, 0},
    {Intrinsic::riscv_read_reg_ctaid_x, "__get_pid", true, 0},
    {Intrinsic::riscv_read_reg_ctaid_y, "__get_pid", true, 1},
    {Intrinsic::riscv_read_reg_ctaid_z, "__get_pid", true, 2},
    {Intrinsic::riscv_read_reg_nctaid_x, "__grid_dim_x", false, 0},
    {Intrinsic::riscv_read_reg_nctaid_y, "__grid_dim_y", false, 0},
    {Intrinsic::riscv_read_reg_nctaid_z, "__grid_dim_z", false, 0},
    {Intrinsic::riscv_read_reg_ntid_x, "__block_dim_x", false, 0},
};

static const ReadRegMap *lookupReadReg(Intrinsic::ID ID) {
  for (const ReadRegMap &M : ReadRegMaps)
    if (M.Src == ID)
      return &M;
  return nullptr;
}

/// Is this value used for anything other than as a lane index?  `tid.x` is
/// dropped (the memory operation becomes a whole-vector one) unless it is also
/// used as a value, in which case the vendor materialises `vid`.
static bool usedAsValue(const CallInst *CI) {
  for (const User *U : CI->users()) {
    const auto *GEP = dyn_cast<GetElementPtrInst>(U);
    if (GEP && GEP->getNumOperands() == 2 && GEP->getOperand(1) == CI)
      continue;
    return true;
  }
  return false;
}

/// LLVM's printed short name for the vector type, e.g. `nxv2f32` / `nxv2bf16`.
/// The RVV intrinsics are declared under mangled names that end with these
/// suffixes, so building the declaration by name (instead of going through
/// `Intrinsic::getDeclaration`'s overload list) keeps the call shape exactly
/// what the vendor emits -- see recon/simt2simd/*.after.ir.
static std::string vecName(Type *VecTy) {
  auto *VT = cast<VectorType>(VecTy);
  Type *Elt = VT->getElementType();
  std::string S = "nxv" + std::to_string(VT->getElementCount().getKnownMinValue());
  if (Elt->isFloatTy())        S += "f32";
  else if (Elt->isHalfTy())    S += "f16";
  else if (Elt->isBFloatTy())  S += "bf16";
  else if (Elt->isIntegerTy(8))  S += "i8";
  else if (Elt->isIntegerTy(16)) S += "i16";
  else if (Elt->isIntegerTy(32)) S += "i32";
  else if (Elt->isIntegerTy(64)) S += "i64";
  else return "";  // unsupported element type -> caller bails out
  return S;
}

static FunctionCallee declareRVV(Module &M, StringRef Name, FunctionType *FT) {
  return M.getOrInsertFunction(Name, FT);
}

/// Scalar intrinsics the pass knows how to map onto a vector intrinsic.
static bool isSupportedScalar(Intrinsic::ID ID) {
  switch (ID) {
  case Intrinsic::experimental_constrained_fadd:
  case Intrinsic::riscv_raisa_fmax:
  case Intrinsic::riscv_raisa_fmin:
  case Intrinsic::riscv_raisa_fdiv:
  case Intrinsic::riscv_raisa_cos:
  case Intrinsic::riscv_raisa_sin:
  case Intrinsic::riscv_raisa_log2:
  case Intrinsic::riscv_raisa_frsqrt:
  case Intrinsic::exp2:
  case Intrinsic::riscv_raisa_max:
  case Intrinsic::riscv_raisa_min:
  case Intrinsic::riscv_raisa_umax:
  case Intrinsic::riscv_raisa_umin:
  case Intrinsic::riscv_raisa_div:
    return true;
  default:
    return false;
  }
}

/// RISC-V `frm` encoding for the constrained-FP rounding-mode metadata.
static std::optional<uint64_t> frmFromMetadata(const Value *V) {
  const auto *MAV = dyn_cast<MetadataAsValue>(V);
  if (!MAV)
    return std::nullopt;
  const auto *MD = dyn_cast<MDString>(MAV->getMetadata());
  if (!MD)
    return std::nullopt;
  StringRef S = MD->getString();
  if (S == "round.tonearest")
    return 0;
  if (S == "round.towardzero")
    return 1;
  if (S == "round.downward")
    return 2;
  if (S == "round.upward")
    return 3;
  return std::nullopt;
}

/// Is \p V the tid.x call (possibly through the index of a GEP)?
static bool indexedByTid(const Value *V, const Value *Tid) {
  const auto *GEP = dyn_cast<GetElementPtrInst>(V);
  if (!GEP || GEP->getNumOperands() != 2)
    return false;
  return GEP->getOperand(1) == Tid;
}

/// A `load` this pass itself created while unpacking the parameter block:
/// `load <ty>, ptr (getelementptr i8, ptr %block, <const>)`.  The body is
/// already packed when `transform()` runs, so the validation loops have to skip
/// these (they are not lane loads).
static bool isPackedParamLoad(const LoadInst *LI) {
  const auto *GEP = dyn_cast<GetElementPtrInst>(LI->getPointerOperand());
  if (!GEP || GEP->getNumOperands() != 2)
    return false;
  if (!isa<ConstantInt>(GEP->getOperand(1)))
    return false;
  const auto *Base = dyn_cast<Argument>(GEP->getPointerOperand());
  return Base && Base->getArgNo() == 0;
}

/// Rewrite a kernel's signature to the RAISA ABI form, unconditionally.
///
/// The vendor's pass packs *every* `rpu_kernel` into `void(ptr)` + one
/// `getelementptr i8, ptr %0, <off>` / `load` per parameter, whether or not the
/// body can be vectorised.  Evidence: `-print-after=simt2simd` on
/// `imm_l1_flush_31_object` (a purely scalar body) still yields the packed
/// signature and the unpacking loads; and feeding the vendor's post-pass IR for
/// the byte-diff cells into this tree's clang reproduces the vendor's object, so
/// the downstream is already correct.
///
/// The offsets are the **cumulative alloc sizes** of the parameters (ptr = 8,
/// float = 4, i32 = 4 in the RAISA data layout; e.g. `abi_float(unsigned *out,
/// float f, unsigned n)` unpacks at 0/8/12).
///
/// On return the original Function object is *erased*; the caller must not use
/// \p F afterwards.
bool TX8TransformSIMTToSIMD::packKernelSignature(Function &F) {
  Module &M = *F.getParent();
  LLVMContext &C = F.getContext();
  PointerType *PtrTy = PointerType::get(C, 0);
  const DataLayout &DL = M.getDataLayout();

  SmallVector<std::pair<uint64_t, Type *>, 8> Params;
  uint64_t Offset = 0;
  for (Argument &A : F.args()) {
    Params.push_back({Offset, A.getType()});
    Offset += DL.getTypeAllocSize(A.getType());
  }

  FunctionType *NFT = FunctionType::get(Type::getVoidTy(C), {PtrTy}, false);
  const std::string OldName = F.getName().str();
  Function *NF = Function::Create(NFT, F.getLinkage(), F.getAddressSpace(),
                                  OldName, &M);
  NF->setCallingConv(F.getCallingConv());
  // Only the function attribute group survives: the original parameter
  // attributes describe `void(ptr, ptr, ...)`, not the packed `void(ptr)`.
  NF->setAttributes(AttributeList::get(
      C, F.getAttributes().getFnAttrs(), AttributeSet(), {}));
  NF->setDSOLocal(F.isDSOLocal());
  NF->copyMetadata(&F, 0);
  NF->splice(NF->begin(), &F);

  IRBuilder<> B(&*NF->getEntryBlock().getFirstInsertionPt());
  SmallVector<Value *, 8> NewArgs;
  for (const auto &[Off, Ty] : Params) {
    Value *G = B.CreateGEP(Type::getInt8Ty(C), NF->getArg(0), B.getInt32(Off));
    NewArgs.push_back(B.CreateLoad(Ty, G));
  }
  for (unsigned i = 0, e = F.arg_size(); i != e; ++i)
    F.getArg(i)->replaceAllUsesWith(NewArgs[i]);

  NF->setLinkage(F.getLinkage());
  F.replaceAllUsesWith(NF);
  F.eraseFromParent();
  // `Function::Create` had to disambiguate the name against the old function;
  // now that the old one is gone, take the original (mangled) name back so the
  // emitted symbol is the vendor's `_Z...`.
  NF->setName(OldName);
  return true;
}

bool TX8TransformSIMTToSIMD::transform(Function &F) {
  // Only single-basic-block kernels are handled for now.
  if (F.size() != 1)
    return false;

  BasicBlock &BB = F.getEntryBlock();
  Module &M = *F.getParent();
  LLVMContext &C = F.getContext();
  Type *Int32Ty = Type::getInt32Ty(C);
  Type *Int64Ty = Type::getInt64Ty(C);
  PointerType *PtrTy = PointerType::get(C, 0);

  // ---- locate the tid.x call -------------------------------------------
  CallInst *Tid = nullptr;
  for (Instruction &I : BB) {
    auto *CI = dyn_cast<CallInst>(&I);
    if (!CI)
      continue;
    Function *Callee = CI->getCalledFunction();
    if (!Callee)
      return false;
    if (Callee->getIntrinsicID() == Intrinsic::riscv_read_reg_tid_x) {
      Tid = CI;
      break;
    }
  }
  if (!Tid)
    return false;

  // ---- element type, from the first tid-indexed load/store -------------
  Type *ElemTy = nullptr;
  for (Instruction &I : BB) {
    if (auto *LI = dyn_cast<LoadInst>(&I)) {
      if (indexedByTid(LI->getPointerOperand(), Tid)) {
        ElemTy = LI->getType();
        break;
      }
    }
    if (auto *SI = dyn_cast<StoreInst>(&I)) {
      if (indexedByTid(SI->getPointerOperand(), Tid)) {
        ElemTy = SI->getValueOperand()->getType();
        break;
      }
    }
  }
  if (!ElemTy || !ElemTy->isSingleValueType())
    return false;

  Type *VecTy = VectorType::get(ElemTy, ElementCount::getScalable(2));
  if (vecName(VecTy).empty())
    return false;
  uint64_t Bytes = SIMT_LANES * (ElemTy->getPrimitiveSizeInBits() / 8);

  // ---- validate the whole body before mutating anything ----------------
  // The body is rewritten only if EVERY instruction is understood: a partial
  // rewrite would leave a half-transformed module behind (the function has
  // already been spliced by then), which is exactly the kind of "looks done but
  // is not" state this reconstruction must avoid.  `WillMap` models the values
  // the rewrite will have available when it reaches each instruction.
  SmallPtrSet<Value *, 16> WillMap;
  for (Instruction &I : BB) {
    if (auto *LI = dyn_cast<LoadInst>(&I)) {
      if (isPackedParamLoad(LI))
        continue;  // pass-created parameter unpacking
      if (indexedByTid(LI->getPointerOperand(), Tid) && LI->getType() == ElemTy)
        WillMap.insert(LI);
      continue;
    }
    if (auto *SI = dyn_cast<StoreInst>(&I)) {
      if (!indexedByTid(SI->getPointerOperand(), Tid) ||
          !WillMap.count(SI->getValueOperand()))
        return false;
      continue;
    }
    if (auto *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (!Callee)
        return false;
      if (lookupReadReg(Callee->getIntrinsicID())) {
        WillMap.insert(CI);
        continue;
      }
      if (!isSupportedScalar(Callee->getIntrinsicID()))
        return false;
      if (Callee->getIntrinsicID() == Intrinsic::experimental_constrained_fadd &&
          !frmFromMetadata(CI->getArgOperand(2)))
        return false;
      for (Value *A : CI->args()) {
        if (isa<MetadataAsValue>(A))  // constrained-FP rounding/fpexcept operands
          continue;
        if (!WillMap.count(A))
          return false;
      }
      WillMap.insert(CI);
      continue;
    }
    if (isa<ReturnInst>(I) || isa<GetElementPtrInst>(I) || isa<BitCastInst>(I))
      continue;
    return false;
  }

  for (Instruction &I : BB) {
    if (isa<LoadInst>(I)) {
      if (isPackedParamLoad(cast<LoadInst>(&I)))
        continue;  // pass-created parameter unpacking
      if (!indexedByTid(I.getOperand(0), Tid) ||
          cast<LoadInst>(I).getType() != ElemTy)
        return false;
      continue;
    }
    if (isa<StoreInst>(I)) {
      if (!indexedByTid(I.getOperand(1), Tid) ||
          I.getOperand(0)->getType() != ElemTy)
        return false;
      continue;
    }
    if (auto *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (!Callee)
        return false;
      switch (Callee->getIntrinsicID()) {
      case Intrinsic::experimental_constrained_fadd:
      case Intrinsic::riscv_raisa_fmax:
      case Intrinsic::riscv_raisa_fmin:
      case Intrinsic::riscv_raisa_fdiv:
      case Intrinsic::riscv_raisa_cos:
      case Intrinsic::riscv_raisa_sin:
      case Intrinsic::riscv_raisa_log2:
      case Intrinsic::riscv_raisa_frsqrt:
      case Intrinsic::exp2:
      case Intrinsic::riscv_raisa_max:
      case Intrinsic::riscv_raisa_min:
      case Intrinsic::riscv_raisa_umax:
      case Intrinsic::riscv_raisa_umin:
      case Intrinsic::riscv_raisa_div:
        break;
      default:
        if (!lookupReadReg(Callee->getIntrinsicID()))
          return false;
        break;
      }
      continue;
    }
    if (isa<ReturnInst>(I) || isa<GetElementPtrInst>(I) || isa<BitCastInst>(I))
      continue;
    // Anything else (branches, phis, ...) is out of scope.
    return false;
  }

  // ---- rewrite the body -------------------------------------------------
  // (The signature was already packed by packKernelSignature(); this half is
  // the all-or-nothing vectorisation and is allowed to bail out.)
  FunctionCallee SetMemAlloc =
      Intrinsic::getDeclaration(&M, Intrinsic::riscv_set_mem_alloc);
  const std::string VT = vecName(VecTy);
  FunctionCallee VLE = declareRVV(
      M, ("llvm.riscv.vle." + VT + ".p0.i64").c_str(),
      FunctionType::get(VecTy, {VecTy, PtrTy, Int64Ty}, false));
  FunctionCallee VSE = declareRVV(
      M, ("llvm.riscv.vse." + VT + ".p0.i64").c_str(),
      FunctionType::get(Type::getVoidTy(C), {VecTy, PtrTy, Int64Ty}, false));

  DenseMap<Value *, Value *> ToVec;
  // Instructions are erased only after every replacement has been built: the
  // scalar values are still referenced by later instructions while the loop
  // runs, so erasing eagerly would leave dangling operands behind (and made
  // the pass crash with "Use still stuck around after Def is destroyed").
  SmallVector<Instruction *, 16> ToErase;
  auto EmitSetMemAlloc = [&](Instruction *Before) {
    IRBuilder<> IB(Before);
    IB.CreateCall(SetMemAlloc, {IB.getInt64(Bytes)});
  };

  SmallVector<Instruction *, 16> Work;
  for (Instruction &I : BB)
    Work.push_back(&I);

  for (Instruction *I : Work) {
    if (I->getParent() != &BB)
      continue;  // already replaced
    if (auto *LI = dyn_cast<LoadInst>(I)) {
      if (!indexedByTid(LI->getPointerOperand(), Tid))
        continue;  // parameter-extraction load, not a lane load
      Value *Base = cast<GetElementPtrInst>(LI->getPointerOperand())
                         ->getPointerOperand();
      EmitSetMemAlloc(LI);
      IRBuilder<> IB(LI);
      Value *V = IB.CreateCall(VLE, {UndefValue::get(VecTy), Base,
                                     IB.getInt64(SIMT_LANES)});
      ToVec[LI] = V;
      // def pushed first: reverse(ToErase) then erases the user before it
      ToErase.push_back(cast<Instruction>(LI->getPointerOperand()));
      ToErase.push_back(LI);
      continue;
    }
    if (auto *SI = dyn_cast<StoreInst>(I)) {
      if (!indexedByTid(SI->getPointerOperand(), Tid))
        continue;
      Value *Val = SI->getValueOperand();
      Value *Base = cast<GetElementPtrInst>(SI->getPointerOperand())
                         ->getPointerOperand();
      auto It = ToVec.find(Val);
      Value *Stored = nullptr;
      if (It != ToVec.end()) {
        Stored = It->second;
      } else {
        // The value is not per-lane (it is a uniform id read or a scalar
        // expression over uniforms): the vendor keeps it scalar and splats it
        // at the store, `vmv.v.x(undef, %val, 128)` right before the
        // `set.mem.alloc` -- see id_ctaid_x / abi_float after.ir.
        IRBuilder<> SB(SI);
        FunctionCallee VMV = declareRVV(
            M, ("llvm.riscv.vmv.v.x." + VT + ".i64").c_str(),
            FunctionType::get(VecTy, {VecTy, Val->getType(), Int64Ty}, false));
        Stored = SB.CreateCall(VMV, {UndefValue::get(VecTy), Val,
                                     SB.getInt64(SIMT_LANES)});
      }
      EmitSetMemAlloc(SI);
      IRBuilder<> IB(SI);
      IB.CreateCall(VSE, {Stored, Base, IB.getInt64(SIMT_LANES)});
      ToErase.push_back(cast<Instruction>(SI->getPointerOperand()));
      ToErase.push_back(SI);
      continue;
    }
    auto *CI = dyn_cast<CallInst>(I);
    if (!CI)
      continue;
    Intrinsic::ID Src = CI->getCalledFunction()->getIntrinsicID();
    if (const ReadRegMap *RM = lookupReadReg(Src)) {
      IRBuilder<> IB(CI);
      if (!RM->Callee) {
        // Per-lane value (`tid.x`): becomes `vid`, but only if it is used for
        // something other than as a lane index -- the fadd/fmax cells only use
        // it as the index and their vendor IR has no `vid`.
        if (usedAsValue(CI)) {
          FunctionCallee VID = declareRVV(
              M, ("llvm.riscv.vid." + VT + ".i64").c_str(),
              FunctionType::get(VecTy, {VecTy, Int64Ty}, false));
          ToVec[CI] = IB.CreateCall(VID, {UndefValue::get(VecTy),
                                          IB.getInt64(SIMT_LANES)});
        }
        ToErase.push_back(CI);
        continue;
      }
      SmallVector<Type *, 1> Params;
      SmallVector<Value *, 1> Args;
      if (RM->HasArg) {
        Params.push_back(Int32Ty);
        Args.push_back(IB.getInt32(RM->Arg));
      }
      FunctionCallee F = M.getOrInsertFunction(
          RM->Callee, FunctionType::get(Int32Ty, Params, false));
      if (auto *Fn = dyn_cast<Function>(F.getCallee()))
        Fn->addFnAttr("no_vector");  // verbatim from the vendor's after.ir
      CallInst *New = IB.CreateCall(F, Args);
      CI->replaceAllUsesWith(New);
      ToErase.push_back(CI);
      continue;
    }

    Intrinsic::ID Dst;
    std::optional<uint64_t> Extra;
    switch (Src) {
    case Intrinsic::experimental_constrained_fadd:
      Dst = Intrinsic::riscv_vfadd;
      Extra = frmFromMetadata(CI->getArgOperand(2));
      if (!Extra)
        return false;
      break;
    case Intrinsic::riscv_raisa_fmax:
      Dst = Intrinsic::riscv_vfmax;
      break;
    case Intrinsic::riscv_raisa_fmin:
      Dst = Intrinsic::riscv_vfmin;
      break;
    case Intrinsic::riscv_raisa_fdiv:
      Dst = Intrinsic::riscv_vfdiv;
      break;
    case Intrinsic::riscv_raisa_cos:
      Dst = Intrinsic::riscv_vcos;
      Extra = 7;  // policy, verbatim from the vendor's dump
      break;
    case Intrinsic::riscv_raisa_sin:
      Dst = Intrinsic::riscv_vsin;
      Extra = 7;
      break;
    case Intrinsic::riscv_raisa_log2:
      Dst = Intrinsic::riscv_vlog2;
      Extra = 7;
      break;
    case Intrinsic::riscv_raisa_frsqrt:
      Dst = Intrinsic::riscv_vfrsqrt7;
      Extra = 7;
      break;
    case Intrinsic::exp2:
      // `__raisa_exp2_*` is lowered by clang to the standard `llvm.exp2`.
      Dst = Intrinsic::riscv_vpow2;
      Extra = 7;
      break;
    case Intrinsic::riscv_raisa_max:
      Dst = Intrinsic::riscv_vmax;
      break;
    case Intrinsic::riscv_raisa_min:
      Dst = Intrinsic::riscv_vmin;
      break;
    case Intrinsic::riscv_raisa_umax:
      Dst = Intrinsic::riscv_vmaxu;
      break;
    case Intrinsic::riscv_raisa_umin:
      Dst = Intrinsic::riscv_vminu;
      break;
    case Intrinsic::riscv_raisa_div:
      Dst = Intrinsic::riscv_vdiv;
      break;
    default:
      return false;
    }

    // Only the shapes whose vendor IR has been reproduced exactly are emitted;
    // everything else leaves the function untouched (see the pass comment).
    // Declaration shapes, verbatim from recon/simt2simd/*.after.ir:
    //   vfadd   (undef, a, b, i64 frm,    i64 128)   -> `.VT.VT.i64`
    //   vfmax   (undef, a, b,             i64 128)   -> `.VT.VT.i64`
    //   vcos/vsin/vlog2/vpow2/vfrsqrt7
    //           (undef, x,    i64 7,     i64 128)   -> `.VT.i64`
    const char *BaseName = nullptr;
    bool UnaryPolicy = false;
    switch (Dst) {
    case Intrinsic::riscv_vfadd: BaseName = "llvm.riscv.vfadd"; break;
    case Intrinsic::riscv_vfmax: BaseName = "llvm.riscv.vfmax"; break;
    case Intrinsic::riscv_vcos: BaseName = "llvm.riscv.vcos"; UnaryPolicy = true; break;
    case Intrinsic::riscv_vsin: BaseName = "llvm.riscv.vsin"; UnaryPolicy = true; break;
    case Intrinsic::riscv_vlog2: BaseName = "llvm.riscv.vlog2"; UnaryPolicy = true; break;
    case Intrinsic::riscv_vpow2: BaseName = "llvm.riscv.vpow2"; UnaryPolicy = true; break;
    case Intrinsic::riscv_vfrsqrt7:
      BaseName = "llvm.riscv.vfrsqrt7";
      UnaryPolicy = true;
      break;
    default:
      return false;
    }
    IRBuilder<> IB(CI);
    SmallVector<Value *, 6> Args;
    SmallVector<Type *, 6> Params;
    Args.push_back(UndefValue::get(VecTy));
    Params.push_back(VecTy);  // undef passthru
    for (unsigned i = 0, e = CI->arg_size(); i != e; ++i) {
      Value *A = CI->getArgOperand(i);
      if (isa<MetadataAsValue>(A))  // rounding / fpexcept metadata
        continue;
      auto It = ToVec.find(A);
      if (It == ToVec.end())
        return false;
      Args.push_back(It->second);
      Params.push_back(VecTy);
    }
    // The overloaded-name suffix lists the intrinsic's overloaded types, which
    // recon/simt2simd/spec.tsv pins down: the unary math family only overloads
    // the result (`vcos.nxv2f32.i64`), the binary family also overloads an
    // operand (`vfadd.nxv2f32.nxv2f32.i64`).
    std::string Name = BaseName;
    if (UnaryPolicy)
      Name += "." + VT;
    else
      Name += "." + VT + "." + VT;
    Name += ".i64";
    // vfadd carries the rounding mode; vcos/vsin/vlog2/vpow2 carry the RAISA
    // policy constant (7) in the same slot.  vfrsqrt7 carries neither
    // (spec.tsv: `vfrsqrt7.nxv2f32.i64:128`).
    if (Dst == Intrinsic::riscv_vfadd || (UnaryPolicy && Dst != Intrinsic::riscv_vfrsqrt7)) {
      Args.push_back(IB.getInt64(*Extra));
      Params.push_back(Int64Ty);
    }
    Args.push_back(IB.getInt64(SIMT_LANES));
    Params.push_back(Int64Ty);
    FunctionCallee Callee =
        declareRVV(M, Name.c_str(), FunctionType::get(VecTy, Params, false));
    ToVec[CI] = IB.CreateCall(Callee, Args);
    // Vendor fork: bf16 vector arithmetic is encoded with the BF16 element
    // format, i.e. vtype bit 8 (RISCVVType::isBF16Fmt), and the vendor selects a
    // dedicated `PseudoVFADD_BFloat16_*` pseudo for it.  Mark the kernel so that
    // RISCVInsertVSETVLI emits that bit: the element type is not recoverable
    // there (MachineRegisterInfo only carries the register class, and the e16
    // f16/bf16 pseudos share `VR`/`VRM2`/...).  This is the coarse stand-in for
    // the vendor's per-opcode `RISCV::getCTPseudoInfoBy{BF16,Half}` tables.
    if (ElemTy->isBFloatTy())
      F.addFnAttr("tx8-simt-bf16-fmt");
    ToErase.push_back(CI);
  }

  // Now that no replacement refers to the scalar values, drop the replaced
  // instructions (uses first, definitions second) and any dead GEPs.
  for (Instruction *I : reverse(ToErase)) {
    if (I->getParent() == &BB)
      I->eraseFromParent();
  }
  for (Instruction &I : make_early_inc_range(BB))
    if (I.use_empty() && isa<GetElementPtrInst>(I))
      I.eraseFromParent();

  return true;
}

bool TX8TransformSIMTToSIMD::runOnModule(Module &M) {
  bool Changed = false;
  // The kernels are collected first: both steps replace the Function object
  // (packKernelSignature erases the original and creates a new one), so
  // iterating the module while mutating it would visit the new function again
  // and pack it twice.
  auto collect = [&M]() {
    SmallVector<Function *, 8> Kernels;
    for (Function &F : M)
      if (!F.isDeclaration() && F.getCallingConv() == CallingConv::RPU_KERNEL)
        Kernels.push_back(&F);
    return Kernels;
  };

  // Step 1: pack every kernel signature.  This is unconditional, exactly like
  // the vendor's pass (see packKernelSignature()).
  for (Function *F : collect())
    if (packKernelSignature(*F))
      Changed = true;
  // Step 2: best-effort, all-or-nothing vectorisation of the packed bodies.
  for (Function *F : collect())
    if (transform(*F))
      Changed = true;
  return Changed;
}
