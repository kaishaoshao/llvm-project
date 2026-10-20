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
// Coverage today: the single-basic-block elementwise kernels (the `mathdev_*`
// fadd/fmax/fmin/fdiv/min/max/umin/umax/div, the unary math family and the
// `reduce_{sum,fmax,fmin}` sequences), plus the `abi_*` rule that keeps scalar
// arithmetic scalar and splats it at the store.  Anything else (branches, phis,
// `vslh`/`vluxei.sync` addressing, ...) is left untouched on purpose: a partial
// rewrite would be worse than no rewrite.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"

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

/// The integer vector type with the same element width and shape as \p VecTy
/// (`nxv2f32` -> `nxv2i32`).  The vendor's reduction lowering reinterprets the
/// reduced vector through it (`bitcast` + `llvm.riscv.vor.vs`).
static Type *intVecOf(Type *VecTy) {
  auto *VT = cast<VectorType>(VecTy);
  unsigned Bits = VT->getElementType()->getPrimitiveSizeInBits();
  return VectorType::get(IntegerType::get(VT->getContext(), Bits),
                         VT->getElementCount());
}

/// The vendor's reduction destination / scalar-operand type: the RVV **m1**
/// group for the element width, which is at least the lane vector
/// `<vscale x 2 x T>`.  Verbatim from recon/simt2simd:
///   f32   lane `nxv2f32` (m1)  -> `vfred*` vd/vs1 `nxv2f32`
///   f16   lane `nxv2f16` (mf2) -> `vfred*` vd/vs1 `nxv4f16`
///   bf16  lane `nxv2bf16`(mf2) -> `vfred*` vd/vs1 `nxv4bf16`
/// i.e. `elements_per_vscale = 64 / bitwidth` (an m1 group holds VLEN/SEW =
/// 64*vscale/SEW elements), floored at the lane vector's own 2.
static Type *reductionVecOf(Type *VecTy) {
  auto *VT = cast<VectorType>(VecTy);
  unsigned Bits = VT->getElementType()->getPrimitiveSizeInBits();
  uint64_t M1 = Bits ? (64 / Bits) : 0;
  uint64_t N = std::max<uint64_t>(VT->getElementCount().getKnownMinValue(), M1);
  return VectorType::get(VT->getElementType(), ElementCount::getScalable(N));
}

/// How the vendor's pass lowers one consumed scalar intrinsic.
struct MappedOp {
  const char *Base = nullptr;  // null => not mapped by this pass
  bool Unary = false;          // mangled name lists only the result overload
  bool HasPolicy = false;      // extra `i64` policy operand before the AVL
  bool Div = false;            // `raisa.fdiv` -> `vfrec7(policy 7)` + `vfmul`
  enum ReduceKind { NoReduce, ReduceSum, ReduceMax, ReduceMin } Reduce = NoReduce;
};

/// The scalar intrinsics the pass knows how to map onto vector intrinsics, with
/// the *shape* of the produced call.  The overload token count follows the
/// intrinsic's overload table, not the argument count (see spec.tsv):
///   unary math  `vcos.nxv2f32.i64`          -> Unary, policy 7
///   `vfrsqrt7.nxv2f32.i64`                  -> Unary, no policy
///   binary      `vfmin.nxv2f32.nxv2f32.i64` -> result + one operand overload
static MappedOp mappedOpFor(Intrinsic::ID ID) {
  MappedOp R;
  switch (ID) {
  case Intrinsic::experimental_constrained_fadd:
    R.Base = "llvm.riscv.vfadd";
    R.HasPolicy = true;  // the RISC-V `frm` encoding, from the metadata
    break;
  case Intrinsic::riscv_raisa_fmax:
    R.Base = "llvm.riscv.vfmax";
    break;
  case Intrinsic::riscv_raisa_fmin:
    R.Base = "llvm.riscv.vfmin";
    break;
  case Intrinsic::riscv_raisa_fdiv:
    R.Div = true;
    break;
  case Intrinsic::riscv_raisa_cos:
    R.Base = "llvm.riscv.vcos";
    R.Unary = true;
    R.HasPolicy = true;
    break;
  case Intrinsic::riscv_raisa_sin:
    R.Base = "llvm.riscv.vsin";
    R.Unary = true;
    R.HasPolicy = true;
    break;
  case Intrinsic::riscv_raisa_log2:
    R.Base = "llvm.riscv.vlog2";
    R.Unary = true;
    R.HasPolicy = true;
    break;
  case Intrinsic::riscv_raisa_frsqrt:
    R.Base = "llvm.riscv.vfrsqrt7";
    R.Unary = true;  // no policy: spec.tsv `vfrsqrt7.nxv2f32.i64:128`
    break;
  case Intrinsic::exp2:
    // `__raisa_exp2_*` is lowered by clang to the standard `llvm.exp2`.
    R.Base = "llvm.riscv.vpow2";
    R.Unary = true;
    R.HasPolicy = true;
    break;
  case Intrinsic::riscv_raisa_max:
    R.Base = "llvm.riscv.vmax";
    break;
  case Intrinsic::riscv_raisa_min:
    R.Base = "llvm.riscv.vmin";
    break;
  case Intrinsic::riscv_raisa_umax:
    R.Base = "llvm.riscv.vmaxu";
    break;
  case Intrinsic::riscv_raisa_umin:
    R.Base = "llvm.riscv.vminu";
    break;
  case Intrinsic::riscv_raisa_div:
    R.Base = "llvm.riscv.vdiv";
    break;
  case Intrinsic::riscv_raisa_reduce_sum:
    R.Reduce = MappedOp::ReduceSum;
    break;
  case Intrinsic::riscv_raisa_reduce_fmax:
    R.Reduce = MappedOp::ReduceMax;
    break;
  case Intrinsic::riscv_raisa_reduce_fmin:
    R.Reduce = MappedOp::ReduceMin;
    break;
  default:
    break;
  }
  return R;
}

/// Scalar intrinsics the pass knows how to map onto a vector intrinsic.
static bool isSupportedScalar(Intrinsic::ID ID) {
  MappedOp Op = mappedOpFor(ID);
  return Op.Base || Op.Div || Op.Reduce != MappedOp::NoReduce;
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

/// A scalar (uniform) instruction the vendor's pass leaves alone: it produces a
/// single scalar value and every operand is either a constant or a value that
/// stays uniform.  This is the `abi_*` rule -- "keep the scalar arithmetic,
/// splat at the store" -- and it is exactly why a scalar op taking an
/// already-vectorised operand is *not* admitted here (the vendor lowers those
/// to the `vadd.nxv2i32.i32.i64` shape, e.g. `abi_two_pointers`).
static bool isUniformScalarInst(const Instruction *I,
                                const SmallPtrSetImpl<Value *> &Uniform) {
  if (!I->getType()->isSingleValueType() || I->getType()->isVectorTy())
    return false;
  if (!isa<BinaryOperator>(I) && !isa<CastInst>(I) && !isa<ExtractValueInst>(I))
    return false;
  for (const Value *Op : I->operands())
    if (!isa<Constant>(Op) && !Uniform.count(const_cast<Value *>(Op)))
      return false;
  return true;
}

/// The vendor's `vx` shape: a plain integer `add` of one *vectorised* lane value
/// and one *uniform* scalar, which becomes a vector+scalar RVV intrinsic.  This
/// is a different family from the `abi_*` "keep the scalar arithmetic" rule
/// above, and the corpus attests exactly one instance:
///
///   abi_two_pointers:  %add = add i32 %1, %n
///        ->  llvm.riscv.vadd.nxv2i32.i32.i64(undef, %1, %n, i64 128)
///
/// Only that shape is admitted here: `add`, integer, the *scalar element* type
/// (so the overload is the attested `.<i32>.i64`), and the (vector, scalar)
/// operand orientation.  Anything else -- a constant on the right (that would be
/// a `vi` shape, `control_divergent`), a different opcode, a different
/// orientation -- has no evidence in `recon/simt2simd/` and is left alone.
static bool isVectorScalarAdd(const Instruction *I, Type *ElemTy,
                              const SmallPtrSetImpl<Value *> &Vectorised,
                              const SmallPtrSetImpl<Value *> &Uniform) {
  if (I->getOpcode() != Instruction::Add || I->getType() != ElemTy ||
      !ElemTy->isIntegerTy())
    return false;
  return Vectorised.count(const_cast<Value *>(I->getOperand(0))) &&
         Uniform.count(const_cast<Value *>(I->getOperand(1)));
}

/// LLVM's printed name for an integer scalar type, for the `vx` overload suffix.
static const char *intTypeName(Type *Ty) {
  switch (Ty->getIntegerBitWidth()) {
  case 8:  return "i8";
  case 16: return "i16";
  case 32: return "i32";
  case 64: return "i64";
  default: return nullptr;
  }
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
/// float f, unsigned n)` unpacks at 0/8/12), each one aligned to its ABI
/// alignment -- `abi_ints(unsigned *out, unsigned char a, unsigned short b,
/// unsigned int c, unsigned long d)` unpacks at 0/8/10/12/16 (the i16 starts at
/// 10, not 9) and `abi_struct(unsigned *out, Pair pair, unsigned n)` at 0/8/24.
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
    Offset = alignTo(Offset, DL.getABITypeAlign(A.getType()));
    Params.push_back({Offset, A.getType()});
    Offset += DL.getTypeAllocSize(A.getType());
  }

  FunctionType *NFT = FunctionType::get(Type::getVoidTy(C), {PtrTy}, false);
  const std::string OldName = F.getName().str();
  Function *NF = Function::Create(NFT, F.getLinkage(), F.getAddressSpace(),
                                  OldName, &M);
  NF->setCallingConv(F.getCallingConv());
  // The function attribute group survives; the original *parameter* attributes
  // do not (they describe `void(ptr, ptr, ...)`, not the packed `void(ptr)`).
  // The vendor instead stamps one constant set on the packed pointer -- verbatim
  // from all 104 archived `recon/simt2simd/*.after.ir`:
  //   (ptr noalias noundef nonnull readonly captures(none) %0) local_unnamed_addr
  // It is also semantically right for the RAISA ABI: the parameter block is only
  // read, never written or captured.
  AttrBuilder PackedParamAttrs(C);
  PackedParamAttrs.addAttribute(Attribute::NoAlias)
      .addAttribute(Attribute::NonNull)
      .addAttribute(Attribute::ReadOnly)
      .addAttribute(Attribute::NoUndef)
      .addCapturesAttr(CaptureInfo::none());
  NF->setAttributes(AttributeList::get(
      C, F.getAttributes().getFnAttrs(), AttributeSet(),
      {AttributeSet::get(C, PackedParamAttrs)}));
  // `local_unnamed_addr` is part of that same signature and has to be carried
  // over explicitly: `Function::Create` starts from `UnnamedAddr::None`.
  NF->setUnnamedAddr(F.getUnnamedAddr());
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
  // is not" state this reconstruction must avoid.  Three value sets drive the
  // all-or-nothing decision:
  //   WillMap     values that exist after the rewrite (vectorised lanes,
  //               `read.reg` results, mapped intrinsic results, uniforms)
  //   Vectorised  the subset that becomes an RVV vector
  //   Uniform     the subset that stays scalar and is splatted at the store
  SmallPtrSet<Value *, 16> WillMap, Vectorised, Uniform;
  for (Argument &A : F.args())
    Uniform.insert(&A);  // the packed parameter block
  for (Instruction &I : BB) {
    if (auto *LI = dyn_cast<LoadInst>(&I)) {
      if (isPackedParamLoad(LI)) {
        Uniform.insert(LI);  // pass-created parameter unpacking
        continue;
      }
      if (indexedByTid(LI->getPointerOperand(), Tid) && LI->getType() == ElemTy) {
        WillMap.insert(LI);
        Vectorised.insert(LI);
        continue;
      }
      return false;
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
      if (const ReadRegMap *RM = lookupReadReg(Callee->getIntrinsicID())) {
        WillMap.insert(CI);
        // `tid.x` becomes `vid` only when it is used as a value; the uniform
        // ids (`ctaid`/`nctaid`/`ntid.x`) stay scalar and are splatted at the
        // store.  `ntid.{y,z}` are per-lane too but the vendor maps them to
        // `vid` (see ReadRegMaps).
        if (!RM->Callee && usedAsValue(CI))
          Vectorised.insert(CI);
        else
          Uniform.insert(CI);
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
        // A mapped intrinsic consumes per-lane values only: the vendor's IR has
        // a scalar operand (`vadd.nxv2i32.i32.i64`) for the mixed shapes, which
        // is a different family.
        if (!Vectorised.count(A))
          return false;
      }
      WillMap.insert(CI);
      Vectorised.insert(CI);
      continue;
    }
    if (isa<ReturnInst>(I) || isa<GetElementPtrInst>(I) || isa<BitCastInst>(I))
      continue;
    // Scalar (uniform) arithmetic is kept scalar and splatted at the store --
    // as long as none of its operands is being vectorised (`abi_float`,
    // `abi_ints`, `abi_struct`, `abi_pointer_*`).  `abi_two_pointers` is the
    // counter-example: its `add` takes a vectorised load and the vendor lowers
    // it to `vadd.vx`, which is not this rule.
    if (isUniformScalarInst(&I, Uniform)) {
      WillMap.insert(&I);
      Uniform.insert(&I);
      continue;
    }
    // `abi_two_pointers` is the counter-example to that rule: its `add` takes a
    // vectorised load and the vendor lowers it to the vector+scalar
    // `vadd.nxv2i32.i32.i64` shape, so it becomes a per-lane value.
    if (isVectorScalarAdd(&I, ElemTy, Vectorised, Uniform)) {
      WillMap.insert(&I);
      Vectorised.insert(&I);
      continue;
    }
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
      if (!isSupportedScalar(Callee->getIntrinsicID()) &&
          !lookupReadReg(Callee->getIntrinsicID()))
        return false;
      continue;
    }
    if (isa<ReturnInst>(I) || isa<GetElementPtrInst>(I) || isa<BitCastInst>(I))
      continue;
    if (isUniformScalarInst(&I, Uniform))
      continue;
    if (isVectorScalarAdd(&I, ElemTy, Vectorised, Uniform))
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
  // Declared on first use: the vendor's module only carries the declarations the
  // body actually calls (a purely scalar store kernel has no `vle` at all).
  auto GetVLE = [&]() {
    return declareRVV(M, ("llvm.riscv.vle." + VT + ".p0.i64").c_str(),
                      FunctionType::get(VecTy, {VecTy, PtrTy, Int64Ty}, false));
  };
  auto GetVSE = [&]() {
    return declareRVV(
        M, ("llvm.riscv.vse." + VT + ".p0.i64").c_str(),
        FunctionType::get(Type::getVoidTy(C), {VecTy, PtrTy, Int64Ty}, false));
  };

  DenseMap<Value *, Value *> ToVec;
  // The all-zero lane vector the reduction lowering ORs against, materialised
  // lazily at the top of the entry block (see the reduce family below).
  Value *Zero = nullptr;
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
      Value *V = IB.CreateCall(GetVLE(), {UndefValue::get(VecTy), Base,
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
      IB.CreateCall(GetVSE(), {Stored, Base, IB.getInt64(SIMT_LANES)});
      ToErase.push_back(cast<Instruction>(SI->getPointerOperand()));
      ToErase.push_back(SI);
      continue;
    }
    // Uniform scalar arithmetic stays scalar -- but the vendor's pass rebuilds
    // it, so the kept instruction loses the frontend's value name (`%conv`/`%add`
    // print as `%7`/`%8`, feeding the store's `vmv.v.x` splat).  Every
    // identifier-like SSA name is in fact gone from *all* 101 archived
    // `*.after.ir` bodies, so dropping the name is exactly what the vendor
    // prints; the value itself (and its order) is untouched.
    if (isUniformScalarInst(I, Uniform)) {
      I->setName("");
      continue;
    }
    // The vector+scalar `vx` shape (`abi_two_pointers`): the lane operand comes
    // from the map, the uniform one stays a scalar GPR operand, and the call is
    // spelled exactly like the vendor's --
    //   %8 = call <vscale x 2 x i32> @llvm.riscv.vadd.nxv2i32.i32.i64(
    //            <vscale x 2 x i32> undef, <vscale x 2 x i32> %7, i32 %6, i64 128)
    if (isVectorScalarAdd(I, ElemTy, Vectorised, Uniform)) {
      Value *Lane = I->getOperand(0);
      Value *UniformOp = I->getOperand(1);
      auto It = ToVec.find(Lane);
      const char *ScalarName = intTypeName(ElemTy);
      if (It == ToVec.end() || !ScalarName)
        return false;
      IRBuilder<> IB(I);
      Type *ScalarTy = ElemTy;
      FunctionCallee VX = declareRVV(
          M, ("llvm.riscv.vadd." + VT + "." + ScalarName + ".i64").c_str(),
          FunctionType::get(VecTy, {VecTy, VecTy, ScalarTy, Int64Ty}, false));
      ToVec[I] = IB.CreateCall(VX, {UndefValue::get(VecTy), It->second,
                                    UniformOp, IB.getInt64(SIMT_LANES)});
      ToErase.push_back(I);
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

    MappedOp Op = mappedOpFor(Src);
    if (!Op.Base && !Op.Div && Op.Reduce == MappedOp::NoReduce)
      return false;

    auto VecOperand = [&](Value *V) -> Value * {
      auto It = ToVec.find(V);
      return It == ToVec.end() ? nullptr : It->second;
    };
    // `fdiv` is decomposed by the vendor into `vfrec7` on the divisor followed
    // by `vfmul` (both with the RAISA policy constant 7), verbatim from
    // recon/simt2simd/mathdev_raisa_fdiv_f.after.ir:
    //   %9  = call ... @llvm.riscv.vfrec7.nxv2f32.i64(undef, %8, i64 7, i64 128)
    //   %10 = call ... @llvm.riscv.vfmul.nxv2f32.nxv2f32.i64(undef, %7, %9, i64 7, i64 128)
    if (Op.Div) {
      Value *A = VecOperand(CI->getArgOperand(0));
      Value *B = VecOperand(CI->getArgOperand(1));
      if (!A || !B)
        return false;
      IRBuilder<> IB(CI);
      FunctionCallee Rec = declareRVV(
          M, ("llvm.riscv.vfrec7." + VT + ".i64").c_str(),
          FunctionType::get(VecTy, {VecTy, VecTy, Int64Ty, Int64Ty}, false));
      Value *R = IB.CreateCall(Rec, {UndefValue::get(VecTy), B, IB.getInt64(7),
                                     IB.getInt64(SIMT_LANES)});
      FunctionCallee Mul = declareRVV(
          M, ("llvm.riscv.vfmul." + VT + "." + VT + ".i64").c_str(),
          FunctionType::get(VecTy, {VecTy, VecTy, VecTy, Int64Ty, Int64Ty},
                            false));
      ToVec[CI] = IB.CreateCall(Mul, {UndefValue::get(VecTy), A, R,
                                      IB.getInt64(7), IB.getInt64(SIMT_LANES)});
      if (ElemTy->isBFloatTy())
        F.addFnAttr("tx8-simt-bf16-fmt");
      ToErase.push_back(CI);
      continue;
    }

    // `reduce_{sum,fmax,fmin}` become a multi-instruction sequence that reduces
    // the lane vector and then *broadcasts* the reduced value back over the
    // lanes (`vor.vs` against a zero vector).  Shapes verbatim from
    // recon/simt2simd/mathdev_raisa_reduce_{sum,fmax,fmin}_*.after.ir:
    //   sum   vfmv.v.f(undef, 0.0, 128) -> vfredusum(undef, V, seed, 7, 128)
    //   fmax  vfredmax(undef, V, V, 128)          (fmin likewise)
    //   both  bitcast -> vor.vs(undef, zero, x, 128) -> bitcast
    // The reduction's `vd`/`vs1` use the m1 group for the element width (twice
    // the lane vector for the 16-bit formats), so the 16-bit cells widen the
    // seed with the vendor's two `llvm.vector.insert` calls.
    if (Op.Reduce != MappedOp::NoReduce) {
      Value *V = VecOperand(CI->getArgOperand(0));
      if (!V)
        return false;
      Type *RTy = reductionVecOf(VecTy);
      Type *VTI = intVecOf(VecTy);
      Type *RTI = intVecOf(RTy);
      auto Widen = [&](Value *S, Type *STy, Type *DTy, IRBuilder<> &B) -> Value * {
        FunctionCallee Ins = declareRVV(
            M, ("llvm.vector.insert." + vecName(DTy) + "." + vecName(STy))
                   .c_str(),
            FunctionType::get(DTy, {DTy, STy, Int64Ty}, false));
        Value *Lo = B.CreateCall(Ins, {UndefValue::get(DTy), S, B.getInt64(0)});
        // the vendor's second insert pads the high half with `undef` at the
        // sub-vector's element stride (`i64 2` for an `nxv2` source)
        uint64_t Stride =
            cast<VectorType>(STy)->getElementCount().getKnownMinValue();
        return B.CreateCall(Ins,
                            {Lo, UndefValue::get(STy), B.getInt64(Stride)});
      };
      // The zero vector the `vor.vs` passthru uses is materialised at the very
      // top of the entry block -- it is the first instruction of the vendor's
      // after-pass IR, ahead of the parameter unpacking:
      //   %1 = call <vscale x 2 x i32> @llvm.riscv.vmv.v.x.nxv2i32.i64(undef, i32 0, i64 128)
      if (!Zero) {
        IRBuilder<> EB(&*BB.getFirstInsertionPt());
        auto *ZTy = cast<VectorType>(VTI);
        FunctionCallee VMVZero = declareRVV(
            M, ("llvm.riscv.vmv.v.x." + vecName(ZTy) + ".i64").c_str(),
            FunctionType::get(ZTy, {ZTy, ZTy->getElementType(), Int64Ty},
                              false));
        Zero = EB.CreateCall(VMVZero,
                             {UndefValue::get(ZTy),
                              ConstantInt::get(ZTy->getElementType(), 0),
                              EB.getInt64(SIMT_LANES)});
      }
      IRBuilder<> IB(CI);
      Value *Seed = V;
      if (Op.Reduce == MappedOp::ReduceSum) {
        // The sum starts from an explicit `0.0` seed; max/min reuse the vector.
        FunctionCallee VFMV = declareRVV(
            M, ("llvm.riscv.vfmv.v.f." + VT + ".i64").c_str(),
            FunctionType::get(VecTy, {VecTy, ElemTy, Int64Ty}, false));
        Seed = IB.CreateCall(VFMV, {UndefValue::get(VecTy),
                                    ConstantFP::get(ElemTy, 0.0),
                                    IB.getInt64(SIMT_LANES)});
      }
      if (RTy != VecTy)
        Seed = Widen(Seed, VecTy, RTy, IB);
      const char *RedName = Op.Reduce == MappedOp::ReduceSum ? "llvm.riscv.vfredusum"
                            : Op.Reduce == MappedOp::ReduceMax
                                ? "llvm.riscv.vfredmax"
                                : "llvm.riscv.vfredmin";
      std::string RedFull =
          std::string(RedName) + "." + vecName(RTy) + "." + VT + ".i64";
      SmallVector<Type *, 5> RP = {RTy, VecTy, RTy};
      SmallVector<Value *, 5> RA = {UndefValue::get(RTy), V, Seed};
      if (Op.Reduce == MappedOp::ReduceSum) {
        RP.push_back(Int64Ty);  // policy 7, like the unary math family
        RA.push_back(IB.getInt64(7));
      }
      RP.push_back(Int64Ty);
      RA.push_back(IB.getInt64(SIMT_LANES));
      Value *Red = IB.CreateCall(
          declareRVV(M, RedFull.c_str(), FunctionType::get(RTy, RP, false)), RA);
      // The widened zero vector is dead in the vendor's IR (the `vor.vs`
      // passthru is the narrow one) but it is part of the dump, so it is
      // emitted -- and *before* the bitcast, which is where the vendor puts it.
      if (RTy != VecTy)
        (void)Widen(Zero, VTI, RTI, IB);
      Value *RedI = IB.CreateBitCast(Red, RTI);
      std::string OrFull = "llvm.riscv.vor.vs." + vecName(VTI) + "." +
                           vecName(RTI) + ".i64";
      FunctionCallee VOR = declareRVV(
          M, OrFull.c_str(),
          FunctionType::get(VTI, {VTI, VTI, RTI, Int64Ty}, false));
      Value *Or = IB.CreateCall(
          VOR, {UndefValue::get(VTI), Zero, RedI, IB.getInt64(SIMT_LANES)});
      ToVec[CI] = IB.CreateBitCast(Or, VecTy);
      if (ElemTy->isBFloatTy())
        F.addFnAttr("tx8-simt-bf16-fmt");
      ToErase.push_back(CI);
      continue;
    }

    // Unary / binary elementwise shapes.
    SmallVector<Value *, 4> Ops;
    for (unsigned i = 0, e = CI->arg_size(); i != e; ++i) {
      Value *A = CI->getArgOperand(i);
      if (isa<MetadataAsValue>(A))  // rounding / fpexcept metadata
        continue;
      Value *V = VecOperand(A);
      if (!V)
        return false;
      Ops.push_back(V);
    }
    if (Ops.size() != (Op.Unary ? 1u : 2u))
      return false;

    IRBuilder<> IB(CI);
    SmallVector<Value *, 6> Args;
    SmallVector<Type *, 6> Params;
    Args.push_back(UndefValue::get(VecTy));
    Params.push_back(VecTy);  // undef passthru
    for (Value *V : Ops) {
      Args.push_back(V);
      Params.push_back(VecTy);
    }
    // The overloaded-name suffix lists the intrinsic's overloaded types, which
    // recon/simt2simd/spec.tsv pins down: the unary math family only overloads
    // the result (`vcos.nxv2f32.i64`), the binary family also overloads an
    // operand (`vfadd.nxv2f32.nxv2f32.i64`).
    std::string Name = std::string(Op.Base) + "." + VT +
                       (Op.Unary ? "" : ("." + VT)) + ".i64";
    // vfadd carries the rounding mode; vcos/vsin/vlog2/vpow2 carry the RAISA
    // policy constant (7) in the same slot.  vfrsqrt7 carries neither
    // (spec.tsv: `vfrsqrt7.nxv2f32.i64:128`), and the integer/float min/max/div
    // family carries no extra operand at all.
    if (Op.HasPolicy) {
      uint64_t Policy = 7;  // the RAISA SIMT policy constant
      if (Src == Intrinsic::experimental_constrained_fadd) {
        std::optional<uint64_t> Frm = frmFromMetadata(CI->getArgOperand(2));
        if (!Frm)
          return false;
        Policy = *Frm;
      }
      Args.push_back(IB.getInt64(Policy));
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
