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
#include "llvm/IR/Operator.h"
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

/// The address spaces the vendor's pass treats as tile memory: the fork
/// replaced CUDA's `__shared__` with one attribute per space, and
/// `clang/lib/Basic/Targets/RISCV.h` maps raisa_{l1,l0a,l0w}shared onto 2 / 3 /
/// 4 (the same numbers its own `rpu-lower-shared` bases 0x101000000 /
/// 0x40000000 / 0x40100000 on).
static bool isSharedAddressSpace(unsigned AS) {
  return AS == 2 || AS == 3 || AS == 4;
}

/// The tile-memory variable a shared load/store accesses at offset 0, or null.
///
/// The operand is the pointer the frontend produces for `g[0]`, i.e. a
/// constant expression `addrspacecast (ptr addrspace(2|3|4) @g to ptr)`,
/// possibly under the all-zero `getelementptr` the array subscript leaves
/// behind.  Verbatim from the vendor's `-print-before=simt2simd` IR:
///   store volatile i32 %0, ptr addrspacecast (ptr addrspace(2) @_ZZ...scratch to ptr)
/// Only the fully folded offset-0 form is understood: a surviving non-zero GEP
/// would mean a constant byte offset, which no cell in the corpus produces.
static GlobalVariable *sharedGlobalAt0(const Value *Ptr) {
  if (const auto *GEP = dyn_cast<GEPOperator>(Ptr)) {
    for (unsigned i = 1, e = GEP->getNumOperands(); i != e; ++i) {
      const auto *CI = dyn_cast<ConstantInt>(GEP->getOperand(i));
      if (!CI || !CI->isZero())
        return nullptr;
    }
    Ptr = GEP->getPointerOperand();
  }
  if (const auto *CE = dyn_cast<ConstantExpr>(Ptr)) {
    if (CE->getOpcode() != Instruction::AddrSpaceCast)
      return nullptr;
    Ptr = CE->getOperand(0);
  } else if (const auto *ASC = dyn_cast<AddrSpaceCastInst>(Ptr)) {
    Ptr = ASC->getPointerOperand();
  } else {
    return nullptr;
  }
  auto *GV = const_cast<GlobalVariable *>(dyn_cast<GlobalVariable>(Ptr));
  if (!GV || !isSharedAddressSpace(GV->getAddressSpace()))
    return nullptr;
  return GV;
}

/// The lane width of a tile-memory access.  The vendor's pass re-slices the
/// shared traffic to the width of the value the kernel finally writes out
/// (`ElemTy`).  Besides the exactly-equal case it admits one wider shape, the
/// corpus's `unsigned long` variable: its `i64` store becomes **two `i32`
/// scatters** (low half at +0, high half at +4) and its `i64` load is gathered
/// as a single `i32` (the following `trunc` is what selects the low half) --
/// verbatim from the vendor's own `-print-after=simt2simd` on
/// `mem_{l0a,l0w,l1}shared_u64_uniform`.
static bool sharedAccessTyOK(Type *AccessTy, Type *ElemTy) {
  if (AccessTy == ElemTy)
    return true;
  return AccessTy->isIntegerTy(64) && ElemTy->isIntegerTy(32);
}

/// A wider tile-memory load is only understood when every use narrows it to the
/// lane width: the `trunc` is precisely what tells the vendor's pass that the
/// low half is all that is demanded.
static bool onlyUsedAsLaneTrunc(const LoadInst *LI, Type *ElemTy) {
  if (LI->use_empty())
    return false;
  for (const User *U : LI->users()) {
    const auto *TI = dyn_cast<TruncInst>(U);
    if (!TI || TI->getType() != ElemTy)
      return false;
  }
  return true;
}

/// `trunc` of a value this pass vectorised down to the lane width: the vendor
/// works at lane width, so the truncation is dropped (`ToVec[trunc]` becomes
/// the gathered lane vector itself).
static bool isLaneTrunc(const Instruction *I, Type *ElemTy,
                        const SmallPtrSetImpl<Value *> &Vectorised) {
  return I->getOpcode() == Instruction::Trunc && I->getType() == ElemTy &&
         Vectorised.count(const_cast<Value *>(I->getOperand(0)));
}

/// The vector counterpart of a plain integer binary op the vendor vectorises
/// with one *scalar* operand: `v<op>.<lane>.<scalar>.i64(undef, %vec, %scalar,
/// 128)`.  The corpus attests `and`/`mul`/`add` (the `control_*` family:
/// `and i32 %tid, 1` -> `vand`, `mul i32 %v, 7` -> `vmul`, `add i32 %v, 3` ->
/// `vadd`); every other opcode is left untransformed rather than guessed.
static const char *vectorBinOpName(unsigned Opcode) {
  switch (Opcode) {
  case Instruction::Add: return "vadd";
  case Instruction::Mul: return "vmul";
  case Instruction::And: return "vand";
  default:               return nullptr;
  }
}

/// The vector counterpart of the `icmp` predicates the corpus uses.  Both take
/// the compared vector plus one scalar and produce the lane mask
/// `<vscale x 2 x i1>` (`control_divergent`: `icmp eq i32 %v, 0` -> `vmseq`;
/// `control_nested`: `icmp ult i32 %tid, %n` -> `vmsltu`).
static const char *vectorCmpName(CmpInst::Predicate P) {
  switch (P) {
  case CmpInst::ICMP_EQ:  return "vmseq";
  case CmpInst::ICMP_ULT: return "vmsltu";
  default:                return nullptr;
  }
}

/// `op %vec, %scalar` where `%vec` is a vectorised lane value and `%scalar` is
/// a constant (the `vi` shape).  The `vx` shape, where the scalar is a uniform
/// *value*, is a different attested case (`abi_two_pointers`) and is handled by
/// `isVectorScalarAdd` below.
static bool isVectorConstOp(const Instruction *I, Type *ElemTy,
                            const SmallPtrSetImpl<Value *> &Vectorised) {
  if (!vectorBinOpName(I->getOpcode()) || I->getType() != ElemTy)
    return false;
  return Vectorised.count(const_cast<Value *>(I->getOperand(0))) &&
         isa<ConstantInt>(I->getOperand(1));
}

/// `icmp <pred> %vec, %scalar`: the vendor turns it into the `vms*` family,
/// whose result is a lane mask.  The scalar may be a constant
/// (`icmp eq %v, 0`) or a uniform value (`icmp ult %tid, %n`, where `%n` is
/// unpacked from the parameter block).
static bool isLaneCmp(const Instruction *I,
                      const SmallPtrSetImpl<Value *> &Vectorised,
                      const SmallPtrSetImpl<Value *> &Uniform) {
  const auto *CI = dyn_cast<ICmpInst>(I);
  if (!CI || !vectorCmpName(CI->getPredicate()))
    return false;
  const Value *RHS = CI->getOperand(1);
  return Vectorised.count(const_cast<Value *>(CI->getOperand(0))) &&
         (isa<ConstantInt>(RHS) || Uniform.count(const_cast<Value *>(RHS)));
}

/// `select i1 %cond, %a, %b` where `%cond` is a lane mask this pass produced:
/// the vendor emits `vmerge(undef, %b, %a, %cond, 128)` -- RVV's `vmerge` keeps
/// the *second* operand where the mask is set, so the false value comes first.
static bool isLaneSelect(const Instruction *I, Type *ElemTy,
                         const SmallPtrSetImpl<Value *> &Vectorised) {
  const auto *SI = dyn_cast<SelectInst>(I);
  if (!SI || SI->getType() != ElemTy)
    return false;
  return Vectorised.count(const_cast<Value *>(SI->getCondition()));
}

/// Does \p V depend on a value this pass vectorises?  A conditional branch must
/// stay a *uniform* branch, so this decides whether control flow is still
/// scalar.  The vendor reaches that shape through its own
/// `tx8-annotate-control-flow` pass, which rewrites a lane-dependent branch into
/// `get.pred.mask` + `and.s` + `raisa.divergent` *before* `simt2simd`; this tree
/// does not have that pass, so a kernel whose branch depends on a lane value is
/// out of scope (the function is left untouched rather than turned into a
/// branch on a vector value).
static bool dependsOnVectorised(const Value *V,
                                const SmallPtrSetImpl<Value *> &Vectorised) {
  if (Vectorised.count(const_cast<Value *>(V)))
    return true;
  const auto *I = dyn_cast<Instruction>(V);
  if (!I || !(isa<ICmpInst>(I) || isa<BinaryOperator>(I) || isa<CastInst>(I) ||
              isa<SelectInst>(I)))
    return false;
  for (const Value *Op : I->operands())
    if (dependsOnVectorised(Op, Vectorised))
      return true;
  return false;
}

/// RAISA intrinsics the vendor's pass leaves untouched: they are consumed by
/// `legalize-exec`/ISel further down, not by this pass.  Verbatim from the
/// `control_*` dumps (`raisa.get.kernel.entry.exec`, `mov128.s.v`, `nor.s`,
/// `and.s`, `set.exec`, `raisa.divergent`).
static bool isKeptScalarIntrinsic(Intrinsic::ID ID) {
  switch (ID) {
  case Intrinsic::riscv_raisa_get_kernel_entry_exec:
  case Intrinsic::riscv_mov128_s_v:
  case Intrinsic::riscv_and_s:
  case Intrinsic::riscv_or_s:
  case Intrinsic::riscv_xor_s:
  case Intrinsic::riscv_andn_s:
  case Intrinsic::riscv_orn_s:
  case Intrinsic::riscv_nand_s:
  case Intrinsic::riscv_nor_s:
  case Intrinsic::riscv_xnor_s:
  case Intrinsic::riscv_set_exec:
  case Intrinsic::riscv_set_exec0:
  case Intrinsic::riscv_set_exec1:
  case Intrinsic::riscv_raisa_divergent:
  case Intrinsic::riscv_kcore_ncc_read:
    return true;
  default:
    return false;
  }
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
  if (!isa<BinaryOperator>(I) && !isa<CastInst>(I) && !isa<ExtractValueInst>(I) &&
      !isa<CmpInst>(I))
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
  Module &M = *F.getParent();
  LLVMContext &C = F.getContext();
  Type *Int32Ty = Type::getInt32Ty(C);
  Type *Int64Ty = Type::getInt64Ty(C);
  PointerType *PtrTy = PointerType::get(C, 0);

  // ---- locate the tid.x call -------------------------------------------
  // The kernels are rewritten basic block by basic block (the vendor's pass
  // keeps the CFG: `control_nested`'s branch on `raisa.divergent` survives
  // verbatim while both of its arms are vectorised), so the two values that
  // drive the transform are looked up function-wide.
  CallInst *Tid = nullptr;
  for (BasicBlock &BB : F) {
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
    if (Tid)
      break;
  }
  if (!Tid)
    return false;

  // ---- element type, from the first tid-indexed load/store -------------
  Type *ElemTy = nullptr;
  for (BasicBlock &BB : F) {
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
    if (ElemTy)
      break;
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
  for (BasicBlock &BB : F) {
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
      // A tile-memory load becomes a gather against the address vector the
      // pass builds for that variable (`vluxei.sync`), with the lane width of
      // the value the kernel finally writes out.  A wider load is admitted
      // only when every use narrows it back to that width.
      if (sharedGlobalAt0(LI->getPointerOperand()) &&
          sharedAccessTyOK(LI->getType(), ElemTy) &&
          (LI->getType() == ElemTy || onlyUsedAsLaneTrunc(LI, ElemTy))) {
        WillMap.insert(LI);
        Vectorised.insert(LI);
        continue;
      }
      return false;
    }
    if (auto *SI = dyn_cast<StoreInst>(&I)) {
      if (indexedByTid(SI->getPointerOperand(), Tid) &&
          WillMap.count(SI->getValueOperand()))
        continue;
      // The tile-memory counterparts of the two cases above: the stored value
      // is either a per-lane value (a `vid`, a vectorised load) or a constant
      // the pass splats with `vmv.v.x`.  The vendor splats the constant
      // `scratch[0] = 7` of `mem_*shared_u{32}_uniform` and the `vid` of
      // `mem_l1_size_*`; a *uniform scalar expression* stored to tile memory
      // has no evidence, so it stays untransformed.  A value wider than one
      // lane must be a constant (that is the only wider shape attested).
      if (sharedGlobalAt0(SI->getPointerOperand())) {
        Value *V = SI->getValueOperand();
        if (sharedAccessTyOK(V->getType(), ElemTy) &&
            (V->getType() == ElemTy
                 ? (Vectorised.count(V) || isa<ConstantInt>(V))
                 : isa<ConstantInt>(V)))
          continue;
      }
      return false;
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
      if (Callee->getIntrinsicID() == Intrinsic::riscv_get_pred_mask) {
        // `get.pred.mask(i1 %cmp)` becomes `mov128.s.v` of the *lane mask* the
        // comparison turned into (`control_nested`: `%cmp = icmp ult %tid, %n`
        // -> `vmsltu` -> `mov128.s.v`), i.e. a 128-bit scalar mask either way.
        if (CI->arg_size() != 1 ||
            !Vectorised.count(CI->getArgOperand(0)))
          return false;
        WillMap.insert(CI);
        Vectorised.insert(CI);
        continue;
      }
      if (isKeptScalarIntrinsic(Callee->getIntrinsicID())) {
        // Left verbatim; every operand must already be a value this pass keeps
        // (a constant, an argument, or something it maps without changing the
        // 128-bit-mask shape).
        for (Value *A : CI->args())
          if (!isa<Constant>(A) && !WillMap.count(A))
            return false;
        WillMap.insert(CI);
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
    if (isa<ReturnInst>(I) || isa<BranchInst>(I) || isa<GetElementPtrInst>(I) ||
        isa<BitCastInst>(I))
      continue;
    // `trunc` of a wider tile-memory gather down to the lane width: the vendor
    // works at lane width, so the truncation disappears.
    if (isLaneTrunc(&I, ElemTy, Vectorised)) {
      WillMap.insert(&I);
      Vectorised.insert(&I);
      continue;
    }
    // `control_*`: arithmetic against a constant becomes a vector op with a
    // scalar operand, a comparison against one becomes a lane mask, and a
    // `select` on that mask becomes `vmerge`.
    if (isVectorConstOp(&I, ElemTy, Vectorised)) {
      WillMap.insert(&I);
      Vectorised.insert(&I);
      continue;
    }
    if (isLaneCmp(&I, Vectorised, Uniform)) {
      WillMap.insert(&I);
      Vectorised.insert(&I);
      continue;
    }
    if (isLaneSelect(&I, ElemTy, Vectorised) &&
        Vectorised.count(I.getOperand(1)) && Vectorised.count(I.getOperand(2))) {
      WillMap.insert(&I);
      Vectorised.insert(&I);
      continue;
    }
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
  }

  // Control flow has to stay uniform (see dependsOnVectorised): this runs
  // after the whole function has been classified, so a branch depending on a
  // value vectorised in *another* block is caught too.
  for (BasicBlock &BB : F)
    for (Instruction &I : BB) {
      const auto *BR = dyn_cast<BranchInst>(&I);
      if (BR && BR->isConditional() &&
          dependsOnVectorised(BR->getCondition(), Vectorised))
        return false;
    }

  for (BasicBlock &BB : F) {
   for (Instruction &I : BB) {
    if (isa<LoadInst>(I)) {
      if (isPackedParamLoad(cast<LoadInst>(&I)))
        continue;  // pass-created parameter unpacking
      if (sharedGlobalAt0(I.getOperand(0)))
        continue;  // tile-memory gather (checked in the first loop)
      if (!indexedByTid(I.getOperand(0), Tid) ||
          cast<LoadInst>(I).getType() != ElemTy)
        return false;
      continue;
    }
    if (isa<StoreInst>(I)) {
      if (sharedGlobalAt0(I.getOperand(1)))
        continue;  // tile-memory scatter (checked in the first loop)
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
          !lookupReadReg(Callee->getIntrinsicID()) &&
          Callee->getIntrinsicID() != Intrinsic::riscv_get_pred_mask &&
          !isKeptScalarIntrinsic(Callee->getIntrinsicID()))
        return false;
      continue;
    }
    if (isa<ReturnInst>(I) || isa<BranchInst>(I) || isa<GetElementPtrInst>(I) ||
        isa<BitCastInst>(I))
      continue;
    if (isLaneTrunc(&I, ElemTy, Vectorised))
      continue;
    if (isVectorConstOp(&I, ElemTy, Vectorised))
      continue;
    if (isLaneCmp(&I, Vectorised, Uniform))
      continue;
    if (isLaneSelect(&I, ElemTy, Vectorised) &&
        Vectorised.count(I.getOperand(1)) && Vectorised.count(I.getOperand(2)))
      continue;
    if (isUniformScalarInst(&I, Uniform))
      continue;
    if (isVectorScalarAdd(&I, ElemTy, Vectorised, Uniform))
      continue;
    // Anything else (branches, phis, ...) is out of scope.
    return false;
   }
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
  // `vmv.v.x.<lane>.i64`: the splat the vendor uses for every scalar value
  // that ends up in a vector (a tid-indexed store of a uniform scalar, and the
  // value half of a tile-memory scatter).
  auto GetVMV = [&](Type *ScalarTy) {
    return declareRVV(M, ("llvm.riscv.vmv.v.x." + VT + ".i64").c_str(),
                      FunctionType::get(VecTy, {VecTy, ScalarTy, Int64Ty},
                                        false));
  };

  // ---- tile-memory (shared) addressing -----------------------------------
  // The vendor's pass turns every access to a `l0ashared`/`l0wshared`/
  // `l1shared` variable into an indexed gather/scatter whose index vector *is*
  // the absolute address of the variable:
  //
  //   %h = lshr i64 ptrtoint (ptr addrspacecast (ptr addrspace(2) @g to ptr)
  //                           to i64), 32
  //   %t = trunc i64 %h to i32
  //   %l = call <vscale x 2 x i32> @llvm.riscv.vmv.v.x.nxv2i32.i64(
  //            <vscale x 2 x i32> undef,
  //            i32 ptrtoint (ptr addrspacecast (ptr addrspace(2) @g to ptr)
  //                          to i32), i64 128)
  //   %i = call <vscale x 2 x i64> @llvm.riscv.vslh.nxv2i64.nxv2i32.i32.i64(
  //            <vscale x 2 x i64> undef, <vscale x 2 x i32> %l, i32 %t, i64 128)
  //   call void @llvm.riscv.vsuxei.sync...(<lane> %v, ptr null, <nxv2i64> %i, i64 128)
  //
  // The `@g` operands are *constant expressions*: `rpu-lower-shared` (which
  // runs after this pass) is what later materialises them into the
  // `addrspacecast(inttoptr(...))` chains and gives each variable its concrete
  // base address.  The chain is built once **per access**, not once per
  // variable: the vendor's own `-print-after=simt2simd` module still has a
  // separate copy for the load of `mem_l1_size_1`, and the single copy visible
  // in `wt-raisa-r2/recon/rpu_lower_shared/*.in.ll` is `early-cse` merging the
  // identical chains in between.
  VectorType *IdxTy = VectorType::get(Int64Ty, ElementCount::getScalable(2));
  // The lane mask type of a vectorised comparison, `<vscale x 2 x i1>`.
  VectorType *MaskTy =
      VectorType::get(Type::getInt1Ty(C), ElementCount::getScalable(2));
  // The 128-bit scalar mask the `control_*` family passes around.
  Type *Mask128Ty = FixedVectorType::get(IntegerType::get(C, 128), 1);
  auto GetVSLH = [&]() {
    return declareRVV(
        M, ("llvm.riscv.vslh.nxv2i64." + VT + ".i32.i64").c_str(),
        FunctionType::get(IdxTy, {IdxTy, VecTy, Int32Ty, Int64Ty}, false));
  };
  auto GetVSUXEI = [&]() {
    return declareRVV(
        M, ("llvm.riscv.vsuxei.sync." + VT + ".p0.nxv2i64.i64").c_str(),
        FunctionType::get(Type::getVoidTy(C), {VecTy, PtrTy, IdxTy, Int64Ty},
                          false));
  };
  auto GetVLUXEI = [&]() {
    return declareRVV(
        M, ("llvm.riscv.vluxei.sync." + VT + ".p0.nxv2i64.i64").c_str(),
        FunctionType::get(VecTy, {VecTy, PtrTy, IdxTy, Int64Ty}, false));
  };
  // Only used by the two-chunk `i64` shape, where the second scatter's index is
  // the low address vector plus the lane size.
  auto GetVADD = [&]() {
    return declareRVV(
        M, ("llvm.riscv.vadd." + VT + ".i32.i64").c_str(),
        FunctionType::get(VecTy, {VecTy, VecTy, Int32Ty, Int64Ty}, false));
  };
  /// The two halves of one access's address chain: the low 32 bits broadcast to
  /// the lanes and the high 32 bits as a scalar (the vendor splits the address
  /// `vslh(low_vec, high)` back together inside the CPU).
  struct SharedAddr {
    Value *Low;
    Value *Hi;
  };
  auto SharedAddressBase = [&](GlobalVariable *GV,
                               Instruction *At) -> SharedAddr {
    Constant *Cast = ConstantExpr::getAddrSpaceCast(GV, PtrTy);
    Constant *Lo = ConstantExpr::getPtrToInt(Cast, Int32Ty);
    Constant *Hi64 = ConstantExpr::getPtrToInt(Cast, Int64Ty);
    IRBuilder<> IB(At);
    Value *Shr = IB.CreateLShr(Hi64, IB.getInt64(32));
    Value *Hi = IB.CreateTrunc(Shr, Int32Ty);
    Value *Splat =
        IB.CreateCall(GetVMV(Int32Ty), {UndefValue::get(VecTy), Lo,
                                        IB.getInt64(SIMT_LANES)});
    return {Splat, Hi};
  };
  /// The index vector of the lane chunk starting at \p ByteOffset: the offset is
  /// added to the *low* half and `vslh`ed against the same high half -- for a
  /// chunk of 0 that is the whole chain the vendor emits for a single-lane
  /// access.
  auto SharedIndex = [&](const SharedAddr &A, uint64_t ByteOffset,
                         Instruction *At) -> Value * {
    IRBuilder<> IB(At);
    Value *Low = A.Low;
    if (ByteOffset)
      Low = IB.CreateCall(GetVADD(), {UndefValue::get(VecTy), Low,
                                      IB.getInt32(ByteOffset),
                                      IB.getInt64(SIMT_LANES)});
    return IB.CreateCall(GetVSLH(), {UndefValue::get(IdxTy), Low, A.Hi,
                                     IB.getInt64(SIMT_LANES)});
  };
  Value *NullBase = ConstantPointerNull::get(PtrTy);

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
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      Work.push_back(&I);

  for (Instruction *I : Work) {
    if (!I->getParent())
      continue;  // already erased by an earlier replacement
    if (auto *LI = dyn_cast<LoadInst>(I)) {
      if (GlobalVariable *GV = sharedGlobalAt0(LI->getPointerOperand())) {
        // A load wider than one lane gathers only the low chunk -- the `trunc`
        // that consumes it is dropped by the lane-trunc case below.
        SharedAddr A = SharedAddressBase(GV, LI);
        Value *Idx = SharedIndex(A, 0, LI);
        IRBuilder<> IB(LI);
        ToVec[LI] = IB.CreateCall(GetVLUXEI(),
                                  {UndefValue::get(VecTy), NullBase, Idx,
                                   IB.getInt64(SIMT_LANES)});
        ToErase.push_back(LI);
        continue;
      }
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
      if (GlobalVariable *GV = sharedGlobalAt0(SI->getPointerOperand())) {
        Value *Val = SI->getValueOperand();
        IRBuilder<> IB(SI);
        uint64_t LaneBytes = M.getDataLayout().getTypeAllocSize(ElemTy);
        if (Val->getType() != ElemTy) {
          // Wider than one lane (`unsigned long`): the vendor writes it as one
          // `i32` scatter per lane chunk, and emits **all** the value splats
          // first, then the address chain, then the scatters.
          auto *C = dyn_cast<ConstantInt>(Val);
          if (!C)
            return false;
          uint64_t Chunks =
              M.getDataLayout().getTypeAllocSize(Val->getType()) / LaneBytes;
          SmallVector<Value *, 2> Stored;
          for (uint64_t i = 0; i != Chunks; ++i) {
            APInt Bits = C->getValue().lshr(i * 32).trunc(32);
            Stored.push_back(IB.CreateCall(
                GetVMV(ElemTy), {UndefValue::get(VecTy),
                                 ConstantInt::get(ElemTy, Bits),
                                 IB.getInt64(SIMT_LANES)}));
          }
          SharedAddr A = SharedAddressBase(GV, SI);
          for (uint64_t i = 0; i != Chunks; ++i) {
            Value *Idx = SharedIndex(A, i * LaneBytes, SI);
            IB.CreateCall(GetVSUXEI(),
                          {Stored[i], NullBase, Idx, IB.getInt64(SIMT_LANES)});
          }
          ToErase.push_back(SI);
          continue;
        }
        // The address chain is built first, then the value the lane stores,
        // then the scatter.
        Value *Idx = SharedIndex(SharedAddressBase(GV, SI), 0, SI);
        Value *Stored = nullptr;
        auto It = ToVec.find(Val);
        if (It != ToVec.end())
          Stored = It->second;
        else if (auto *C = dyn_cast<ConstantInt>(Val))
          Stored = IB.CreateCall(GetVMV(C->getType()),
                                 {UndefValue::get(VecTy), C,
                                  IB.getInt64(SIMT_LANES)});
        else
          Stored = IB.CreateCall(GetVMV(Val->getType()),
                                 {UndefValue::get(VecTy), Val,
                                  IB.getInt64(SIMT_LANES)});
        IB.CreateCall(GetVSUXEI(),
                      {Stored, NullBase, Idx, IB.getInt64(SIMT_LANES)});
        ToErase.push_back(SI);
        continue;
      }
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
    // A `trunc` of a wider tile-memory gather to the lane width: the pass works
    // at lane width, so the truncation is simply dropped and its users see the
    // gathered lane vector (`mem_*shared_u64_uniform`).
    if (isLaneTrunc(I, ElemTy, Vectorised)) {
      auto It = ToVec.find(I->getOperand(0));
      if (It == ToVec.end())
        return false;
      ToVec[I] = It->second;
      ToErase.push_back(I);
      continue;
    }
    // `control_*`: arithmetic against a constant, the comparison that consumes
    // it, and the `select` on the resulting lane mask.
    if (isVectorConstOp(I, ElemTy, Vectorised)) {
      Value *Lane = I->getOperand(0);
      auto It = ToVec.find(Lane);
      const char *ScalarName = intTypeName(ElemTy);
      auto *C = cast<ConstantInt>(I->getOperand(1));
      if (It == ToVec.end() || !ScalarName)
        return false;
      IRBuilder<> IB(I);
      FunctionCallee VOp = declareRVV(
          M, ("llvm.riscv." + std::string(vectorBinOpName(I->getOpcode())) +
              "." + VT + "." + ScalarName + ".i64").c_str(),
          FunctionType::get(VecTy, {VecTy, VecTy, ElemTy, Int64Ty}, false));
      ToVec[I] = IB.CreateCall(VOp, {UndefValue::get(VecTy), It->second, C,
                                     IB.getInt64(SIMT_LANES)});
      ToErase.push_back(I);
      continue;
    }
    if (isLaneCmp(I, Vectorised, Uniform)) {
      auto *Cmp = cast<ICmpInst>(I);
      auto It = ToVec.find(Cmp->getOperand(0));
      if (It == ToVec.end())
        return false;
      IRBuilder<> IB(I);
      FunctionCallee VCmp = declareRVV(
          M, ("llvm.riscv." + std::string(vectorCmpName(Cmp->getPredicate())) +
              "." + VT + "." + intTypeName(ElemTy) + ".i64").c_str(),
          FunctionType::get(MaskTy, {VecTy, ElemTy, Int64Ty}, false));
      ToVec[I] = IB.CreateCall(VCmp, {It->second, Cmp->getOperand(1),
                                      IB.getInt64(SIMT_LANES)});
      ToErase.push_back(I);
      continue;
    }
    if (isLaneSelect(I, ElemTy, Vectorised) &&
        Vectorised.count(I->getOperand(1)) && Vectorised.count(I->getOperand(2))) {
      auto ItT = ToVec.find(I->getOperand(1));
      auto ItF = ToVec.find(I->getOperand(2));
      auto ItC = ToVec.find(I->getOperand(0));
      if (ItT == ToVec.end() || ItF == ToVec.end() || ItC == ToVec.end())
        return false;
      IRBuilder<> IB(I);
      FunctionCallee VMerge = declareRVV(
          M, ("llvm.riscv.vmerge." + VT + "." + VT + ".i64").c_str(),
          FunctionType::get(VecTy, {VecTy, VecTy, VecTy, MaskTy, Int64Ty},
                            false));
      // `vmerge` keeps the *second* operand where the mask is set, so the
      // select's false value comes first -- verbatim from the vendor:
      //   select i1 %c, %mul, %add -> vmerge(undef, %add, %mul, %c, 128)
      ToVec[I] = IB.CreateCall(VMerge, {UndefValue::get(VecTy), ItF->second,
                                        ItT->second, ItC->second,
                                        IB.getInt64(SIMT_LANES)});
      ToErase.push_back(I);
      continue;
    }
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

    if (Src == Intrinsic::riscv_get_pred_mask) {
      // The lane mask is already vectorised; what changes is its *type*: the
      // vendor wraps it in a 128-bit scalar mask with `mov128.s.v`
      // (`control_nested`: `vmsltu` -> `mov128.s.v`).
      auto It = ToVec.find(CI->getArgOperand(0));
      if (It == ToVec.end())
        return false;
      IRBuilder<> IB(CI);
      FunctionCallee Mov = declareRVV(
          M, "llvm.riscv.mov128.s.v.nxv2i1",
          FunctionType::get(Mask128Ty, {MaskTy}, false));
      CallInst *New = IB.CreateCall(Mov, {It->second});
      CI->replaceAllUsesWith(New);
      ToErase.push_back(CI);
      continue;
    }
    if (isKeptScalarIntrinsic(Src))
      continue;  // left verbatim (its operands were replaced in place)

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
        IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());
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
    if (I->getParent())
      I->eraseFromParent();
  }
  for (BasicBlock &BB : F)
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
