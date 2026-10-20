//===--- SemaCT.cpp - Semantic analysis for RAISA CT intrinsics -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// RAISA / TX8 "CT" (compute-tile) intrinsic checking.
//
// RECONSTRUCTION PROVENANCE -- READ BEFORE TRUSTING THIS FILE
// ---------------------------------------------------------
// This file is a reconstruction of the fork's `clang/lib/Sema/SemaCT.cpp`.
// The vendor ships no source for it, only the object `SemaCT.cpp.o` (31544 B)
// inside `<vendor>/lib/libclangSema.a`.
//
// PROVEN (from the vendor object, by relocation/size/behaviour analysis):
//   * The two entry points and their exact signatures, which already match the
//     byte-identical `clang/include/clang/Sema/SemaRISCV.h` declarations.
//   * The body of `CheckCTBuiltinFunctionCall`'s outer dispatch: a binary search
//     over `BuiltinID` that is `default:` -> `err_tsm_unsupported_ct_intrinsic`
//     and otherwise falls into a set of small shared bodies.  The classification
//     of 1291 distinct builtin IDs was recovered mechanically from the five
//     jump tables in `.rodata` and the surrounding compare/bitmask tests.  The
//     `case` labels below are that classification, spelled with the generated
//     `RISCVVector::BI__*` / `RISCV::BI__*` enumerators.  888 of the 1291 IDs
//     are reproduced here; the remaining ~403 fall through to `default:` (see
//     "NOT PROVEN" below).
//   * The default path's diagnostic: DiagID 0x1456 ==
//     `diag::err_tsm_unsupported_ct_intrinsic` (5006..5206 arithmetic:
//     DIAG_START_SEMA = 2820, so the ID is 2820 + 2386).
//   * The shared-body semantics: `{16, 32}` / `{32}` / `{16}` / `{8}` / `{64}`
//     length lists passed to a `(CallExpr*, QualType, ArrayRef<unsigned>)`
//     validator lambda, `BuiltinConstantArgRange(TheCall, 1|2, 0, 1, true)`,
//     and literal `return true` / `return false`.
//
// INFERRED (not recovered -- flagged at each site):
//   * The body of the `CheckFixedVector` lambda.  Only its call set is proven:
//     `SemaBase::getASTContext`, `ASTContext::getBuiltinVectorTypeInfo`,
//     `ASTContext::getTypeInfo`, one `Diag`, two `getDeviceDeferredDiags`,
//     `Stmt::getBeginLoc` x3, `Stmt::getSourceRange` x2 and a `SmallVector`.
//   * The body of `CheckIndirectionOperand`.  Proven: it tests
//     `TargetInfo::hasFeature("xtsm82s")` first (7-character literal in its own
//     `.str1.1` pool) and emits
//     `err_typecheck_indirection_through_unsupported_pointer`.
//   * The exact argument selected at the ARG*VEC32 sites: the vendor indexes
//     `0x28/0x30/0x38/0x40(%rbp,%rax,8)` with `(%rax = (TheCall[2] >> 3) & 1)`,
//     and that bit could not be attributed to a known `Expr`/`Stmt` field, so
//     the base index is used here.
//   * ~403 jump-table IDs whose targets are distinct small bodies (addresses
//     `0x55c`, `0x560`, `0x564`, ... ) that were not decoded; they currently
//     hit `default:` and are therefore reported as "not supported by CT".
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaRISCV.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/TargetBuiltins.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang;

bool SemaRISCV::CheckCTBuiltinFunctionCall(const TargetInfo &TI,
                                           unsigned BuiltinID,
                                           CallExpr *TheCall) {
  const ASTContext &Context = getASTContext();

  // INFERRED BODY -- see the provenance note above.  `NumElts` is the list of
  // fixed element counts the vendor's shared bodies pass (8/16/32/64 or
  // {16, 32}); a type is accepted iff it is a vector whose element count is in
  // that list.  The proven facts are the existence of the lambda, its
  // `(CallExpr *, QualType, ArrayRef<unsigned>)` signature, the
  // `ArrayRef<unsigned>` literals at the call sites, and that it emits exactly
  // one diagnostic -- `err_tsm_unsupported_ct_intrinsic_with_type`.
  auto CheckFixedVector = [this, &Context](CallExpr *Call, QualType Ty,
                                 ArrayRef<unsigned> NumElts) -> bool {
    bool Supported = false;
    if (const auto *VT = Ty->getAs<VectorType>()) {
      Supported = llvm::is_contained(NumElts, VT->getNumElements());
    } else if (const auto *BT = Ty->getAs<BuiltinType>()) {
      if (BT->isVectorType()) {
        ASTContext::BuiltinVectorTypeInfo Info =
            Context.getBuiltinVectorTypeInfo(BT);
        Supported = llvm::is_contained(NumElts, Info.EC.getKnownMinValue());
      }
    }
    if (!Supported)
      Diag(Call->getBeginLoc(),
           diag::err_tsm_unsupported_ct_intrinsic_with_type)
          << Ty;
    return Supported;
  };

  (void)TI;

  switch (BuiltinID) {
  default:
    // PROVEN: the default path builds a valid SourceLocation (falling back to
    // the callee / first argument) and emits DiagID 0x1456 with the call's
    // source range.
    Diag(TheCall->getBeginLoc(), diag::err_tsm_unsupported_ct_intrinsic)
        << TheCall->getSourceRange();
    return false;
  case RISCVVector::BI__builtin_rvv_vadd_vv:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tu:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_m:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tum:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tumu:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {32});
  case RISCVVector::BI__builtin_rvv_vle16_v:
  case RISCVVector::BI__builtin_rvv_vle16_v_tu:
  case RISCVVector::BI__builtin_rvv_vle16_v_m:
  case RISCVVector::BI__builtin_rvv_vle16_v_tum:
  case RISCVVector::BI__builtin_rvv_vle16_v_tumu:
  case RISCVVector::BI__builtin_rvv_vle16_v_mu:
  case RISCVVector::BI__builtin_rvv_vle32_v:
  case RISCVVector::BI__builtin_rvv_vle32_v_tu:
  case RISCVVector::BI__builtin_rvv_vle32_v_m:
  case RISCVVector::BI__builtin_rvv_vle32_v_tum:
  case RISCVVector::BI__builtin_rvv_vle32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vle32_v_mu:
    return true;
  case RISCVVector::BI__builtin_rvv_vse16_v:
  case RISCVVector::BI__builtin_rvv_vse16_v_m:
  case RISCVVector::BI__builtin_rvv_vse32_v:
  case RISCVVector::BI__builtin_rvv_vse32_v_m:
    return true;
  case RISCVVector::BI__builtin_rvv_vlse16_v:
  case RISCVVector::BI__builtin_rvv_vlse16_v_tu:
  case RISCVVector::BI__builtin_rvv_vlse16_v_m:
  case RISCVVector::BI__builtin_rvv_vlse16_v_tum:
  case RISCVVector::BI__builtin_rvv_vlse16_v_tumu:
  case RISCVVector::BI__builtin_rvv_vlse16_v_mu:
  case RISCVVector::BI__builtin_rvv_vlse32_v:
  case RISCVVector::BI__builtin_rvv_vlse32_v_tu:
  case RISCVVector::BI__builtin_rvv_vlse32_v_m:
  case RISCVVector::BI__builtin_rvv_vlse32_v_tum:
  case RISCVVector::BI__builtin_rvv_vlse32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vlse32_v_mu:
    return true;
  case RISCVVector::BI__builtin_rvv_vsse16_v:
  case RISCVVector::BI__builtin_rvv_vsse16_v_m:
  case RISCVVector::BI__builtin_rvv_vsse32_v:
  case RISCVVector::BI__builtin_rvv_vsse32_v_m:
    return true;
  case RISCVVector::BI__builtin_rvv_vluxei32_v:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_tu:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_m:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_tum:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vloxei32_v:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_tu:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_m:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_tum:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_mu:
  case RISCVVector::BI__builtin_rvv_vloxei64_v:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_tu:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_m:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_tum:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_tumu:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vsuxei32_v:
    return CheckFixedVector(TheCall, TheCall->getArg(2)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vsuxei32_v_m:
    return CheckFixedVector(TheCall, TheCall->getArg(3)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vsoxei32_v:
    return CheckFixedVector(TheCall, TheCall->getArg(2)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vsoxei32_v_m:
    return CheckFixedVector(TheCall, TheCall->getArg(3)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vsoxei64_v:
    return CheckFixedVector(TheCall, TheCall->getArg(2)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vsoxei64_v_m:
    return CheckFixedVector(TheCall, TheCall->getArg(3)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vadd_vx:
  case RISCVVector::BI__builtin_rvv_vadd_vx_tu:
  case RISCVVector::BI__builtin_rvv_vadd_vx_m:
  case RISCVVector::BI__builtin_rvv_vadd_vx_tum:
  case RISCVVector::BI__builtin_rvv_vadd_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vadd_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsub_vv:
  case RISCVVector::BI__builtin_rvv_vsub_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsub_vv_m:
  case RISCVVector::BI__builtin_rvv_vsub_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsub_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsub_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsub_vx:
  case RISCVVector::BI__builtin_rvv_vsub_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsub_vx_m:
  case RISCVVector::BI__builtin_rvv_vsub_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsub_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsub_vx_mu:
  case RISCVVector::BI__builtin_rvv_vrsub_vx:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_tu:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_m:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_tum:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vadc_vvm:
  case RISCVVector::BI__builtin_rvv_vadc_vvm_tu:
  case RISCVVector::BI__builtin_rvv_vadc_vxm:
  case RISCVVector::BI__builtin_rvv_vadc_vxm_tu:
  case RISCVVector::BI__builtin_rvv_vsbc_vvm:
  case RISCVVector::BI__builtin_rvv_vsbc_vvm_tu:
  case RISCVVector::BI__builtin_rvv_vsbc_vxm:
  case RISCVVector::BI__builtin_rvv_vsbc_vxm_tu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmadc_vvm:
  case RISCVVector::BI__builtin_rvv_vmadc_vxm:
  case RISCVVector::BI__builtin_rvv_vmadc_vv:
  case RISCVVector::BI__builtin_rvv_vmadc_vx:
  case RISCVVector::BI__builtin_rvv_vmsbc_vvm:
  case RISCVVector::BI__builtin_rvv_vmsbc_vxm:
  case RISCVVector::BI__builtin_rvv_vmsbc_vv:
  case RISCVVector::BI__builtin_rvv_vmsbc_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vand_vv:
  case RISCVVector::BI__builtin_rvv_vand_vv_tu:
  case RISCVVector::BI__builtin_rvv_vand_vv_m:
  case RISCVVector::BI__builtin_rvv_vand_vv_tum:
  case RISCVVector::BI__builtin_rvv_vand_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vand_vv_mu:
  case RISCVVector::BI__builtin_rvv_vand_vx:
  case RISCVVector::BI__builtin_rvv_vand_vx_tu:
  case RISCVVector::BI__builtin_rvv_vand_vx_m:
  case RISCVVector::BI__builtin_rvv_vand_vx_tum:
  case RISCVVector::BI__builtin_rvv_vand_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vand_vx_mu:
  case RISCVVector::BI__builtin_rvv_vxor_vv:
  case RISCVVector::BI__builtin_rvv_vxor_vv_tu:
  case RISCVVector::BI__builtin_rvv_vxor_vv_m:
  case RISCVVector::BI__builtin_rvv_vxor_vv_tum:
  case RISCVVector::BI__builtin_rvv_vxor_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vxor_vv_mu:
  case RISCVVector::BI__builtin_rvv_vxor_vx:
  case RISCVVector::BI__builtin_rvv_vxor_vx_tu:
  case RISCVVector::BI__builtin_rvv_vxor_vx_m:
  case RISCVVector::BI__builtin_rvv_vxor_vx_tum:
  case RISCVVector::BI__builtin_rvv_vxor_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vxor_vx_mu:
  case RISCVVector::BI__builtin_rvv_vor_vv:
  case RISCVVector::BI__builtin_rvv_vor_vv_tu:
  case RISCVVector::BI__builtin_rvv_vor_vv_m:
  case RISCVVector::BI__builtin_rvv_vor_vv_tum:
  case RISCVVector::BI__builtin_rvv_vor_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vor_vv_mu:
  case RISCVVector::BI__builtin_rvv_vor_vx:
  case RISCVVector::BI__builtin_rvv_vor_vx_tu:
  case RISCVVector::BI__builtin_rvv_vor_vx_m:
  case RISCVVector::BI__builtin_rvv_vor_vx_tum:
  case RISCVVector::BI__builtin_rvv_vor_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vor_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsll_vv:
  case RISCVVector::BI__builtin_rvv_vsll_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsll_vv_m:
  case RISCVVector::BI__builtin_rvv_vsll_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsll_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsll_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsll_vx:
  case RISCVVector::BI__builtin_rvv_vsll_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsll_vx_m:
  case RISCVVector::BI__builtin_rvv_vsll_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsll_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsll_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsrl_vv:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_m:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsrl_vx:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_m:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsra_vv:
  case RISCVVector::BI__builtin_rvv_vsra_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsra_vv_m:
  case RISCVVector::BI__builtin_rvv_vsra_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsra_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsra_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsra_vx:
  case RISCVVector::BI__builtin_rvv_vsra_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsra_vx_m:
  case RISCVVector::BI__builtin_rvv_vsra_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsra_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsra_vx_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmseq_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmseq_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmseq_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmseq_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsne_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsne_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsne_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsne_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsltu_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsltu_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsltu_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsltu_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmslt_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmslt_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmslt_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmslt_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsleu_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsleu_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsleu_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsleu_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsle_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsle_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsle_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsle_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgtu_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgtu_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgtu_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgtu_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgt_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgt_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgt_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgt_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgeu_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgeu_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgeu_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsgeu_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsge_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsge_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsge_vx:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmsge_vx_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vminu_vv:
  case RISCVVector::BI__builtin_rvv_vminu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vminu_vv_m:
  case RISCVVector::BI__builtin_rvv_vminu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vminu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vminu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vminu_vx:
  case RISCVVector::BI__builtin_rvv_vminu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vminu_vx_m:
  case RISCVVector::BI__builtin_rvv_vminu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vminu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vminu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmin_vv:
  case RISCVVector::BI__builtin_rvv_vmin_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmin_vv_m:
  case RISCVVector::BI__builtin_rvv_vmin_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmin_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmin_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmin_vx:
  case RISCVVector::BI__builtin_rvv_vmin_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmin_vx_m:
  case RISCVVector::BI__builtin_rvv_vmin_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmin_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmin_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmax_vv:
  case RISCVVector::BI__builtin_rvv_vmax_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmax_vv_m:
  case RISCVVector::BI__builtin_rvv_vmax_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmax_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmax_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmax_vx:
  case RISCVVector::BI__builtin_rvv_vmax_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmax_vx_m:
  case RISCVVector::BI__builtin_rvv_vmax_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmax_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmax_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmul_vv:
  case RISCVVector::BI__builtin_rvv_vmul_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmul_vv_m:
  case RISCVVector::BI__builtin_rvv_vmul_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmul_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmul_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmul_vx:
  case RISCVVector::BI__builtin_rvv_vmul_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmul_vx_m:
  case RISCVVector::BI__builtin_rvv_vmul_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmul_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmul_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmulh_vv:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_m:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmulh_vx:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_m:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vdivu_vv:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_m:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vdivu_vx:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_m:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vdiv_vv:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_tu:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_m:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_tum:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_mu:
  case RISCVVector::BI__builtin_rvv_vdiv_vx:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_tu:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_m:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_tum:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_mu:
  case RISCVVector::BI__builtin_rvv_vremu_vv:
  case RISCVVector::BI__builtin_rvv_vremu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vremu_vv_m:
  case RISCVVector::BI__builtin_rvv_vremu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vremu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vremu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vremu_vx:
  case RISCVVector::BI__builtin_rvv_vremu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vremu_vx_m:
  case RISCVVector::BI__builtin_rvv_vremu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vremu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vremu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vrem_vv:
  case RISCVVector::BI__builtin_rvv_vrem_vv_tu:
  case RISCVVector::BI__builtin_rvv_vrem_vv_m:
  case RISCVVector::BI__builtin_rvv_vrem_vv_tum:
  case RISCVVector::BI__builtin_rvv_vrem_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vrem_vv_mu:
  case RISCVVector::BI__builtin_rvv_vrem_vx:
  case RISCVVector::BI__builtin_rvv_vrem_vx_tu:
  case RISCVVector::BI__builtin_rvv_vrem_vx_m:
  case RISCVVector::BI__builtin_rvv_vrem_vx_tum:
  case RISCVVector::BI__builtin_rvv_vrem_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vrem_vx_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmerge_vvm:
  case RISCVVector::BI__builtin_rvv_vmerge_vvm_tu:
  case RISCVVector::BI__builtin_rvv_vmerge_vxm:
  case RISCVVector::BI__builtin_rvv_vmerge_vxm_tu:
  case RISCVVector::BI__builtin_rvv_vmv_v_v:
  case RISCVVector::BI__builtin_rvv_vmv_v_v_tu:
  case RISCVVector::BI__builtin_rvv_vmv_v_x:
  case RISCVVector::BI__builtin_rvv_vmv_v_x_tu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfadd_vv:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_m:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfmul_vv:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_tu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_m:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_tum:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_mu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_tu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_m:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_tum:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_mu:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tu:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_m:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tum:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_mu:
  case RISCVVector::BI__builtin_rvv_vfmin_vv:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_m:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfmin_vf:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_m:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfmax_vv:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_m:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfmax_vf:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_m:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfabs_v:
  case RISCVVector::BI__builtin_rvv_vfabs_v_tu:
  case RISCVVector::BI__builtin_rvv_vfabs_v_m:
  case RISCVVector::BI__builtin_rvv_vfabs_v_tum:
  case RISCVVector::BI__builtin_rvv_vfabs_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfabs_v_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfeq_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfeq_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfeq_vf:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfeq_vf_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfne_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfne_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfne_vf:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfne_vf_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmflt_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmflt_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmflt_vf:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmflt_vf_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfle_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfle_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfle_vf:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfle_vf_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfgt_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfgt_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfgt_vf:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfgt_vf_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfge_vv:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfge_vv_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfge_vf:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vmfge_vf_m:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfmerge_vfm:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_rm:
    return false;
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16});
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {32});
  case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_m:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tum:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_mu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_tu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_tum:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_mu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16});
  case RISCVVector::BI__builtin_rvv_vfredmax_vs:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs_m:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs_m:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm_m:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm_tum:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfwredosum_vs:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vrgather_vx_tum:
    return false;
  case RISCVVector::BI__builtin_rvv_vaesdf_vs:
  case RISCVVector::BI__builtin_rvv_vaesdf_vs_tu:
  case RISCVVector::BI__builtin_rvv_vaesdm_vs:
  case RISCVVector::BI__builtin_rvv_vaesdm_vs_tu:
  case RISCVVector::BI__builtin_rvv_vaesef_vs:
  case RISCVVector::BI__builtin_rvv_vaesef_vs_tu:
  case RISCVVector::BI__builtin_rvv_vaesem_vs:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vlenb:
  case RISCVVector::BI__builtin_rvv_vlm_v:
  case RISCVVector::BI__builtin_rvv_vlmul_ext_v:
  case RISCVVector::BI__builtin_rvv_vlmul_trunc_v:
  case RISCVVector::BI__builtin_rvv_vmand_mm:
  case RISCVVector::BI__builtin_rvv_vmandn_mm:
  case RISCVVector::BI__builtin_rvv_vmclr_m:
  case RISCVVector::BI__builtin_rvv_vmnand_mm:
  case RISCVVector::BI__builtin_rvv_vmnor_mm:
  case RISCVVector::BI__builtin_rvv_vmor_mm:
  case RISCVVector::BI__builtin_rvv_vmorn_mm:
  case RISCVVector::BI__builtin_rvv_vmsbf_m:
  case RISCVVector::BI__builtin_rvv_vmsbf_m_m:
  case RISCVVector::BI__builtin_rvv_vmsbf_m_mu:
  case RISCVVector::BI__builtin_rvv_vmset_m:
  case RISCVVector::BI__builtin_rvv_vmsif_m:
  case RISCVVector::BI__builtin_rvv_vmsif_m_m:
  case RISCVVector::BI__builtin_rvv_vmsif_m_mu:
  case RISCVVector::BI__builtin_rvv_vmsof_m:
  case RISCVVector::BI__builtin_rvv_vmsof_m_m:
  case RISCVVector::BI__builtin_rvv_vmsof_m_mu:
  case RISCVVector::BI__builtin_rvv_vmxnor_mm:
  case RISCVVector::BI__builtin_rvv_vmxor_mm:
  case RISCVVector::BI__builtin_rvv_vreinterpret_v:
  case RISCVVector::BI__builtin_rvv_vsetvli:
  case RISCVVector::BI__builtin_rvv_vsetvlimax:
  case RISCVVector::BI__builtin_rvv_vsext_vf2:
  case RISCVVector::BI__builtin_rvv_vsext_vf2_tu:
  case RISCVVector::BI__builtin_rvv_vsext_vf2_m:
  case RISCVVector::BI__builtin_rvv_vsext_vf2_tum:
  case RISCVVector::BI__builtin_rvv_vsext_vf2_tumu:
  case RISCVVector::BI__builtin_rvv_vsext_vf2_mu:
  case RISCVVector::BI__builtin_rvv_vsext_vf4:
  case RISCVVector::BI__builtin_rvv_vsext_vf4_tu:
  case RISCVVector::BI__builtin_rvv_vsext_vf4_m:
  case RISCVVector::BI__builtin_rvv_vsext_vf4_tum:
  case RISCVVector::BI__builtin_rvv_vsext_vf4_tumu:
  case RISCVVector::BI__builtin_rvv_vsext_vf4_mu:
  case RISCVVector::BI__builtin_rvv_vsext_vf8:
  case RISCVVector::BI__builtin_rvv_vsext_vf8_tu:
  case RISCVVector::BI__builtin_rvv_vsext_vf8_m:
  case RISCVVector::BI__builtin_rvv_vsext_vf8_tum:
  case RISCVVector::BI__builtin_rvv_vsext_vf8_tumu:
  case RISCVVector::BI__builtin_rvv_vsext_vf8_mu:
  case RISCVVector::BI__builtin_rvv_vsm_v:
  case RISCVVector::BI__builtin_rvv_vsm4r_vs:
  case RISCVVector::BI__builtin_rvv_vsm4r_vs_tu:
  case RISCVVector::BI__builtin_rvv_vzext_vf2:
  case RISCVVector::BI__builtin_rvv_vzext_vf2_tu:
  case RISCVVector::BI__builtin_rvv_vzext_vf2_m:
  case RISCVVector::BI__builtin_rvv_vzext_vf2_tum:
  case RISCVVector::BI__builtin_rvv_vzext_vf2_tumu:
  case RISCVVector::BI__builtin_rvv_vzext_vf2_mu:
  case RISCVVector::BI__builtin_rvv_vzext_vf4:
  case RISCVVector::BI__builtin_rvv_vzext_vf4_tu:
  case RISCVVector::BI__builtin_rvv_vzext_vf4_m:
  case RISCVVector::BI__builtin_rvv_vzext_vf4_tum:
  case RISCVVector::BI__builtin_rvv_vzext_vf4_tumu:
  case RISCVVector::BI__builtin_rvv_vzext_vf4_mu:
  case RISCVVector::BI__builtin_rvv_vzext_vf8:
  case RISCVVector::BI__builtin_rvv_vzext_vf8_tu:
  case RISCVVector::BI__builtin_rvv_vzext_vf8_m:
  case RISCVVector::BI__builtin_rvv_vzext_vf8_tum:
  case RISCVVector::BI__builtin_rvv_vzext_vf8_tumu:
  case RISCVVector::BI__builtin_rvv_vzext_vf8_mu:
  case RISCV::BI__riscv_is_explicit_cluster:
  case RISCV::BI__riscv_read_reg_cluster_ctaid_x:
  case RISCV::BI__riscv_read_reg_cluster_ctaid_y:
  case RISCV::BI__riscv_read_reg_cluster_ctaid_z:
  case RISCV::BI__riscv_read_reg_cluster_ctarank:
  case RISCV::BI__riscv_read_reg_cluster_nctaid_x:
  case RISCV::BI__riscv_read_reg_cluster_nctaid_y:
  case RISCV::BI__riscv_read_reg_cluster_nctaid_z:
  case RISCV::BI__riscv_read_reg_cluster_nctarank:
  case RISCV::BI__riscv_read_reg_clusterid_x:
  case RISCV::BI__riscv_read_reg_clusterid_y:
  case RISCV::BI__riscv_read_reg_clusterid_z:
  case RISCV::BI__riscv_read_reg_ctaid_x:
  case RISCV::BI__riscv_read_reg_ctaid_y:
  case RISCV::BI__riscv_read_reg_ctaid_z:
  case RISCV::BI__riscv_read_reg_nclusterid_x:
  case RISCV::BI__riscv_read_reg_nclusterid_y:
  case RISCV::BI__riscv_read_reg_nclusterid_z:
  case RISCV::BI__riscv_read_reg_nctaid_x:
  case RISCV::BI__riscv_read_reg_nctaid_y:
  case RISCV::BI__riscv_read_reg_nctaid_z:
  case RISCV::BI__riscv_read_reg_ntid_x:
  case RISCV::BI__riscv_read_reg_ntid_y:
  case RISCV::BI__riscv_read_reg_ntid_z:
  case RISCV::BI__riscv_read_reg_tid_x:
  case RISCV::BI__riscv_read_reg_tid_y:
  case RISCV::BI__riscv_read_reg_tid_z:
  case RISCV::BI__builtin_riscv_aes32dsi:
  case RISCV::BI__builtin_riscv_aes32dsmi:
  case RISCV::BI__builtin_riscv_aes32esi:
  case RISCV::BI__builtin_riscv_aes32esmi:
  case RISCV::BI__builtin_riscv_aes64ds:
  case RISCV::BI__builtin_riscv_aes64dsm:
  case RISCV::BI__builtin_riscv_aes64es:
  case RISCV::BI__builtin_riscv_aes64esm:
  case RISCV::BI__builtin_riscv_aes64im:
  case RISCV::BI__builtin_riscv_aes64ks1i:
  case RISCV::BI__builtin_riscv_aes64ks2:
  case RISCV::BI__builtin_riscv_cv_alu_addN:
  case RISCV::BI__builtin_riscv_cv_alu_addRN:
  case RISCV::BI__builtin_riscv_cv_alu_adduN:
  case RISCV::BI__builtin_riscv_cv_alu_adduRN:
  case RISCV::BI__builtin_riscv_cv_alu_clip:
  case RISCV::BI__builtin_riscv_cv_alu_clipu:
  case RISCV::BI__builtin_riscv_cv_alu_extbs:
  case RISCV::BI__builtin_riscv_cv_alu_extbz:
  case RISCV::BI__builtin_riscv_cv_alu_exths:
  case RISCV::BI__builtin_riscv_cv_alu_exthz:
  case RISCV::BI__builtin_riscv_cv_alu_sle:
  case RISCV::BI__builtin_riscv_cv_alu_sleu:
  case RISCV::BI__builtin_riscv_cv_alu_subN:
  case RISCV::BI__builtin_riscv_cv_alu_subRN:
  case RISCV::BI__builtin_riscv_cv_alu_subuN:
  case RISCV::BI__builtin_riscv_cv_alu_subuRN:
  case RISCV::BI__builtin_riscv_and_s:
  case RISCV::BI__builtin_riscv_andn_s:
  case RISCV::BI__builtin_riscv_brev8_32:
  case RISCV::BI__builtin_riscv_brev8_64:
  case RISCV::BI__builtin_riscv_clmul_32:
  case RISCV::BI__builtin_riscv_clmul_64:
  case RISCV::BI__builtin_riscv_clmulh_32:
  case RISCV::BI__builtin_riscv_clmulh_64:
  case RISCV::BI__builtin_riscv_clmulr_32:
  case RISCV::BI__builtin_riscv_clmulr_64:
  case RISCV::BI__builtin_riscv_clz_32:
  case RISCV::BI__builtin_riscv_clz_64:
  case RISCV::BI__builtin_riscv_conv:
  case RISCV::BI__builtin_riscv_conv_psum:
  case RISCV::BI__builtin_riscv_convbw:
  case RISCV::BI__builtin_riscv_convbw_psum:
  case RISCV::BI__raisa_cos_f16:
  case RISCV::BI__raisa_cos_f:
  case RISCV::BI__raisa_cos_bf16:
  case RISCV::BI__builtin_riscv_ctz_32:
  case RISCV::BI__builtin_riscv_ctz_64:
  case RISCV::BI__builtin_riscv_direct_write:
  case RISCV::BI__raisa_div:
  case RISCV::BI__builtin_riscv_dte_gather:
  case RISCV::BI__builtin_riscv_dte_gather_scatter_g2l:
  case RISCV::BI__builtin_riscv_dte_gather_scatter_l2g:
  case RISCV::BI__builtin_riscv_dte_index_gather:
  case RISCV::BI__builtin_riscv_dte_index_scatter:
  case RISCV::BI__builtin_riscv_dte_scatter:
  case RISCV::BI__builtin_riscv_dte_set_length:
  case RISCV::BI__builtin_riscv_dte_set_rdma_dst_itera_stride:
  case RISCV::BI__builtin_riscv_dte_set_rdma_misc:
  case RISCV::BI__builtin_riscv_dte_set_rdma_src_itera_stride:
  case RISCV::BI__builtin_riscv_dte_set_wdma_dst_itera_stride:
  case RISCV::BI__builtin_riscv_dte_set_wdma_misc:
  case RISCV::BI__builtin_riscv_dte_set_wdma_src_itera_stride:
  case RISCV::BI__raisa_exp2_f16:
  case RISCV::BI__raisa_exp2_f:
  case RISCV::BI__raisa_exp2_bf16:
  case RISCV::BI__raisa_fadd_rd_f16:
  case RISCV::BI__raisa_fadd_rd_f:
  case RISCV::BI__raisa_fadd_rd_bf16:
  case RISCV::BI__raisa_fadd_rn_f16:
  case RISCV::BI__raisa_fadd_rn_f:
  case RISCV::BI__raisa_fadd_rn_bf16:
  case RISCV::BI__raisa_fadd_ru_f16:
  case RISCV::BI__raisa_fadd_ru_f:
  case RISCV::BI__raisa_fadd_ru_bf16:
  case RISCV::BI__raisa_fadd_rz_f16:
  case RISCV::BI__raisa_fadd_rz_f:
  case RISCV::BI__raisa_fadd_rz_bf16:
  case RISCV::BI__raisa_fdiv_f16:
  case RISCV::BI__raisa_fdiv_f:
  case RISCV::BI__raisa_fdiv_bf16:
  case RISCV::BI__raisa_fmax_f16:
  case RISCV::BI__raisa_fmax_f:
  case RISCV::BI__raisa_fmax_bf16:
    return CheckFixedVector(TheCall, TheCall->getType(), {16, 32});
  case RISCV::BI__raisa_fmin_f16:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  case RISCV::BI__raisa_fmin_bf16:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  case RISCV::BI__builtin_riscv_gemm:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  case RISCV::BI__builtin_riscv_inst_barrier:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  case RISCV::BI__builtin_riscv_l0_invalid:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  case RISCV::BI__builtin_riscv_l0_set_src_stride:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  case RISCV::BI__builtin_riscv_l1_set_src_stride:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  case RISCV::BI__raisa_log2_f16:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  case RISCV::BI__builtin_riscv_lsu_rdma_gather_scatter:
    return false;
  case RISCV::BI__builtin_riscv_lsu_rdma_mem_copy:
    return false;
  case RISCV::BI__builtin_riscv_lsu_set_rdma_dst_itera_stride:
    return false;
  case RISCV::BI__builtin_riscv_lsu_set_rdma_src_itera_stride:
    return false;
  case RISCV::BI__builtin_riscv_lsu_set_wdma_src_itera_stride:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  case RISCV::BI__builtin_riscv_lsu_tdma_mem_copy:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  case RISCV::BI__builtin_riscv_lsu_wdma_gather_scatter:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  case RISCV::BI__builtin_riscv_lsu_wdma_mem_copy:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  case RISCV::BI__raisa_min:
    return false;
  case RISCV::BI__builtin_riscv_mma_first:
    return false;
  case RISCV::BI__builtin_riscv_nand_s:
    return false;
  case RISCV::BI__builtin_riscv_ne_set_conv_swr:
    return false;
  case RISCV::BI__builtin_riscv_ne_set_mkn:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  case RISCV::BI__builtin_riscv_nor_s:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  case RISCV::BI__builtin_riscv_orc_b_32:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  case RISCV::BI__builtin_riscv_orn_s:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  case RISCV::BI__raisa_reduce_fmax_f16:
    return true;
  case RISCV::BI__raisa_reduce_fmax_bf16:
    return true;
  case RISCV::BI__raisa_reduce_sum_f16:
    return true;
  case RISCV::BI__raisa_reduce_sum_bf16:
    return true;
  case RISCV::BI__builtin_riscv_set_barrier:
  case RISCV::BI__builtin_riscv_set_mem_alloc:
  case RISCV::BI__builtin_riscv_sha256sig0:
  case RISCV::BI__builtin_riscv_sha256sig1:
  case RISCV::BI__builtin_riscv_sha256sum0:
  case RISCV::BI__builtin_riscv_sha256sum1:
  case RISCV::BI__builtin_riscv_sha512sig0:
  case RISCV::BI__builtin_riscv_sha512sig0h:
  case RISCV::BI__builtin_riscv_sha512sig0l:
  case RISCV::BI__builtin_riscv_sha512sig1:
  case RISCV::BI__builtin_riscv_sha512sig1h:
  case RISCV::BI__builtin_riscv_sha512sig1l:
  case RISCV::BI__builtin_riscv_sha512sum0:
  case RISCV::BI__builtin_riscv_sha512sum0r:
  case RISCV::BI__builtin_riscv_sha512sum1:
  case RISCV::BI__builtin_riscv_sha512sum1r:
  case RISCV::BI__raisa_sin_f16:
  case RISCV::BI__raisa_sin_f:
  case RISCV::BI__raisa_sin_bf16:
  case RISCV::BI__builtin_riscv_sm3p0:
  case RISCV::BI__builtin_riscv_sm3p1:
  case RISCV::BI__builtin_riscv_sm4ed:
  case RISCV::BI__builtin_riscv_sm4ks:
  case RISCV::BI__raisa_umax:
  case RISCV::BI__raisa_umin:
  case RISCV::BI__builtin_riscv_unzip_32:
  case RISCV::BI__builtin_riscv_vset2dvload0:
  case RISCV::BI__builtin_riscv_vset2dvload1:
  case RISCV::BI__builtin_riscv_vset2dvstore0:
  case RISCV::BI__builtin_riscv_vset2dvstore1:
  case RISCV::BI__builtin_riscv_vsetxfrm:
  case RISCV::BI__builtin_riscv_wait_recv:
  case RISCV::BI__builtin_riscv_wait_recv_roce:
  case RISCV::BI__builtin_riscv_wait_send:
  case RISCV::BI__builtin_riscv_wait_send_roce:
  case RISCV::BI__builtin_riscv_wait_sync:
    return true;
  }
}

// INFERRED BODY -- see the provenance note above.  Proven: the 7-character
// `xtsm82s` feature test up front (with an early return when the feature is
// absent), a `dyn_cast`-style vtable compare on the operand, and one
// `err_typecheck_indirection_through_unsupported_pointer` diagnostic.
void SemaRISCV::CheckIndirectionOperand(const TargetInfo &TI, Expr *Op,
                                        QualType Ty, SourceLocation OpLoc) {
  if (!TI.hasFeature("xtsm82s"))
    return;

  QualType Pointee = Ty->getPointeeType();
  if (!Pointee.isNull() && Pointee->isVectorType())
    return;

  Diag(OpLoc, diag::err_typecheck_indirection_through_unsupported_pointer)
      << Ty;
}
