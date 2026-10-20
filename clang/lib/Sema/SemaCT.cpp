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
//     of all non-default builtin IDs in the CT range was recovered
//     mechanically from the five jump tables in `.rodata` and the surrounding
//     compare/bitmask tests.  The `case` labels below are that classification,
//     spelled with the generated `RISCVVector::BI__*` / `RISCV::BI__*`
//     enumerators.  (The first reconstruction pass only decoded 888 of them and
//     sent the remaining 403 to `default:`, which made the RAISA front-end
//     reject builtins the vendor accepts; see `wt-raisa/tools/gen_semact.py`.)
//   * The default path's diagnostic: DiagID 0x1456 ==
//     `diag::err_tsm_unsupported_ct_intrinsic` (DIAG_START_SEMA = 2821, so the
//     ID is 2821 + 2385; verified by the neighbouring 0x1457 ==
//     `err_tsm_unsupported_ct_intrinsic_with_type` emitted by the validator).
//   * The shared-body semantics: `{16, 32}` / `{32}` / `{16}` / `{8}` / `{64}`
//     length lists passed to a `(CallExpr*, QualType, ArrayRef<unsigned>)`
//     validator lambda, `BuiltinConstantArgRange(TheCall, 1|2, 0, 1, true)`,
//     and literal `return true` / `return false`.
//
// INFERRED (not recovered -- flagged at each site):
//   * The body of `CheckFixedVector` is now recovered from the vendor object's
//     outlined lambda (see the comment at its definition); its guard predicate
//     is written as `BuiltinType::isRVVVLSBuiltinType()` because every QualType
//     the vendor ever passes there is an RVV vector or mask builtin type.
//   * The body of `CheckIndirectionOperand`.  Proven: it tests
//     `TargetInfo::hasFeature("xtsm82s")` first (7-character literal in its own
//     `.str1.1` pool) and emits
//     `err_typecheck_indirection_through_unsupported_pointer`.
//   * The exact argument selected at the ARG*VEC sites: the vendor indexes
//     `0x28/0x30/0x38/0x40(%rbp,%rax,8)` with `(%rax = NumPreArgs)`, i.e.
//     `TheCall->getArg(0..3)`; `CallExpr::OffsetToTrailingObjects == 0x20`, so
//     the base index is correct for every real CT call (NumPreArgs == 0).
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

  // RECOVERED BODY.  The vendor's shared validator takes
  // `(CallExpr *, QualType, ArrayRef<unsigned>)`, calls
  // `ASTContext::getBuiltinVectorTypeInfo` and then
  // `ASTContext::getTypeInfo(Info.ElementType)` exactly once (both relocations
  // are adjacent at the very top of the outlined lambda in `SemaCT.cpp.o`), and
  // compares the *result* against every element of the list before emitting at
  // most one `err_tsm_unsupported_ct_intrinsic_with_type` (DiagID 0x1457).
  //
  // The list therefore holds **element widths in bits**, not element counts.
  // Verified against the vendor clang (RAISA triple, `-fsyntax-only`):
  //   __riscv_vadd_vv_i8m1  -> "this intrinsic with type '__rvv_int8m1_t' is
  //                             not supported by CT"          (8-bit element)
  //   __riscv_vadd_vv_i64m1 -> rejected                        (64-bit element)
  //   __riscv_vadd_vv_i16m4 / _i16mf4 / _i32m8 -> accepted
  // Element counts cannot be what is compared: i16mf4, i16m4 and i32m8 have
  // wildly different counts yet are all accepted by the same {16, 32} list.
  //
  // The return value is inverted with respect to the naive reading: the vendor
  // returns `false` ("not handled here, keep going") when the width *is*
  // supported, and returns the diagnostic builder (i.e. `true`) otherwise.
  auto CheckFixedVector = [this, &Context](CallExpr *Call, QualType Ty,
                                 ArrayRef<unsigned> ElemWidths) -> bool {
    bool Supported = false;
    if (const auto *BT = Ty->getAs<BuiltinType>()) {
      if (BT->isRVVVLSBuiltinType()) {
        ASTContext::BuiltinVectorTypeInfo Info =
            Context.getBuiltinVectorTypeInfo(BT);
        Supported = llvm::is_contained(
            ElemWidths, Context.getTypeInfo(Info.ElementType).Width);
      }
    }
    if (Supported)
      return false;
    return Diag(Call->getBeginLoc(),
                diag::err_tsm_unsupported_ct_intrinsic_with_type)
           << Ty;
  };

  (void)TI;

  switch (BuiltinID) {
  default:
    // PROVEN: the default path builds a valid SourceLocation (falling back to
    // the callee / first argument) and emits DiagID 0x1456 ==
    // diag::err_tsm_unsupported_ct_intrinsic with the call's source range.
    Diag(TheCall->getBeginLoc(), diag::err_tsm_unsupported_ct_intrinsic)
        << TheCall->getSourceRange();
    return false;
  // ---- CheckFixedVector(arg0, {16,32})  (42 IDs) ----
  case RISCVVector::BI__builtin_rvv_vfmv_f_s:
  case RISCVVector::BI__builtin_rvv_vmadc_vv:
  case RISCVVector::BI__builtin_rvv_vmadc_vvm:
  case RISCVVector::BI__builtin_rvv_vmadc_vx:
  case RISCVVector::BI__builtin_rvv_vmadc_vxm:
  case RISCVVector::BI__builtin_rvv_vmfeq_vf:
  case RISCVVector::BI__builtin_rvv_vmfeq_vv:
  case RISCVVector::BI__builtin_rvv_vmfge_vf:
  case RISCVVector::BI__builtin_rvv_vmfge_vv:
  case RISCVVector::BI__builtin_rvv_vmfgt_vf:
  case RISCVVector::BI__builtin_rvv_vmfgt_vv:
  case RISCVVector::BI__builtin_rvv_vmfle_vf:
  case RISCVVector::BI__builtin_rvv_vmfle_vv:
  case RISCVVector::BI__builtin_rvv_vmflt_vf:
  case RISCVVector::BI__builtin_rvv_vmflt_vv:
  case RISCVVector::BI__builtin_rvv_vmfne_vf:
  case RISCVVector::BI__builtin_rvv_vmfne_vv:
  case RISCVVector::BI__builtin_rvv_vmsbc_vv:
  case RISCVVector::BI__builtin_rvv_vmsbc_vvm:
  case RISCVVector::BI__builtin_rvv_vmsbc_vx:
  case RISCVVector::BI__builtin_rvv_vmsbc_vxm:
  case RISCVVector::BI__builtin_rvv_vmseq_vv:
  case RISCVVector::BI__builtin_rvv_vmseq_vx:
  case RISCVVector::BI__builtin_rvv_vmsge_vv:
  case RISCVVector::BI__builtin_rvv_vmsge_vx:
  case RISCVVector::BI__builtin_rvv_vmsgeu_vv:
  case RISCVVector::BI__builtin_rvv_vmsgeu_vx:
  case RISCVVector::BI__builtin_rvv_vmsgt_vv:
  case RISCVVector::BI__builtin_rvv_vmsgt_vx:
  case RISCVVector::BI__builtin_rvv_vmsgtu_vv:
  case RISCVVector::BI__builtin_rvv_vmsgtu_vx:
  case RISCVVector::BI__builtin_rvv_vmsle_vv:
  case RISCVVector::BI__builtin_rvv_vmsle_vx:
  case RISCVVector::BI__builtin_rvv_vmsleu_vv:
  case RISCVVector::BI__builtin_rvv_vmsleu_vx:
  case RISCVVector::BI__builtin_rvv_vmslt_vv:
  case RISCVVector::BI__builtin_rvv_vmslt_vx:
  case RISCVVector::BI__builtin_rvv_vmsltu_vv:
  case RISCVVector::BI__builtin_rvv_vmsltu_vx:
  case RISCVVector::BI__builtin_rvv_vmsne_vv:
  case RISCVVector::BI__builtin_rvv_vmsne_vx:
  case RISCVVector::BI__builtin_rvv_vmv_x_s:
    return CheckFixedVector(TheCall, TheCall->getArg(0)->getType(), {16,32});
  // ---- CheckFixedVector(arg1, {16,32})  (33 IDs) ----
  case RISCVVector::BI__builtin_rvv_vmfeq_vf_m:
  case RISCVVector::BI__builtin_rvv_vmfeq_vv_m:
  case RISCVVector::BI__builtin_rvv_vmfge_vf_m:
  case RISCVVector::BI__builtin_rvv_vmfge_vv_m:
  case RISCVVector::BI__builtin_rvv_vmfgt_vf_m:
  case RISCVVector::BI__builtin_rvv_vmfgt_vv_m:
  case RISCVVector::BI__builtin_rvv_vmfle_vf_m:
  case RISCVVector::BI__builtin_rvv_vmfle_vv_m:
  case RISCVVector::BI__builtin_rvv_vmflt_vf_m:
  case RISCVVector::BI__builtin_rvv_vmflt_vv_m:
  case RISCVVector::BI__builtin_rvv_vmfne_vf_m:
  case RISCVVector::BI__builtin_rvv_vmfne_vv_m:
  case RISCVVector::BI__builtin_rvv_vmseq_vv_m:
  case RISCVVector::BI__builtin_rvv_vmseq_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsge_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsge_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsgeu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsgeu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsgt_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsgt_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsgtu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsgtu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsle_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsle_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsleu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsleu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmslt_vv_m:
  case RISCVVector::BI__builtin_rvv_vmslt_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsltu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsltu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmsne_vv_m:
  case RISCVVector::BI__builtin_rvv_vmsne_vx_m:
  case RISCVVector::BI__builtin_rvv_vs2d_v:
    return CheckFixedVector(TheCall, TheCall->getArg(1)->getType(), {16,32});
  // ---- CheckFixedVector(arg2, {16,32})  (7 IDs) ----
  case RISCVVector::BI__builtin_rvv_vs2d_v_m:
  case RISCVVector::BI__builtin_rvv_vsoxei32_v:
  case RISCVVector::BI__builtin_rvv_vsoxei64_v:
  case RISCVVector::BI__builtin_rvv_vsuxei32_sync_v:
  case RISCVVector::BI__builtin_rvv_vsuxei32_v:
  case RISCVVector::BI__builtin_rvv_vsuxei64_sync_v:
  case RISCVVector::BI__builtin_rvv_vsuxei64_v:
    return CheckFixedVector(TheCall, TheCall->getArg(2)->getType(), {16,32});
  // ---- CheckFixedVector(arg3, {16,32})  (6 IDs) ----
  case RISCVVector::BI__builtin_rvv_vsoxei32_v_m:
  case RISCVVector::BI__builtin_rvv_vsoxei64_v_m:
  case RISCVVector::BI__builtin_rvv_vsuxei32_sync_v_m:
  case RISCVVector::BI__builtin_rvv_vsuxei32_v_m:
  case RISCVVector::BI__builtin_rvv_vsuxei64_sync_v_m:
  case RISCVVector::BI__builtin_rvv_vsuxei64_v_m:
    return CheckFixedVector(TheCall, TheCall->getArg(3)->getType(), {16,32});
  // ---- CheckFixedVector(call, {16})  (36 IDs) ----
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_mu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tu:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tum:
  case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_mu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_tu:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_tum:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_f_f_w_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_x_v:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_x_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_x_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_x_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_x_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_x_v_tumu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16});
  // ---- CheckFixedVector(call, {16,32})  (844 IDs) ----
  case RISCVVector::BI__builtin_rvv_vadc_vvm:
  case RISCVVector::BI__builtin_rvv_vadc_vvm_tu:
  case RISCVVector::BI__builtin_rvv_vadc_vxm:
  case RISCVVector::BI__builtin_rvv_vadc_vxm_tu:
  case RISCVVector::BI__builtin_rvv_vadd_vv:
  case RISCVVector::BI__builtin_rvv_vadd_vv_m:
  case RISCVVector::BI__builtin_rvv_vadd_vv_mu:
  case RISCVVector::BI__builtin_rvv_vadd_vv_tu:
  case RISCVVector::BI__builtin_rvv_vadd_vv_tum:
  case RISCVVector::BI__builtin_rvv_vadd_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vadd_vx:
  case RISCVVector::BI__builtin_rvv_vadd_vx_m:
  case RISCVVector::BI__builtin_rvv_vadd_vx_mu:
  case RISCVVector::BI__builtin_rvv_vadd_vx_tu:
  case RISCVVector::BI__builtin_rvv_vadd_vx_tum:
  case RISCVVector::BI__builtin_rvv_vadd_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vand_vs:
  case RISCVVector::BI__builtin_rvv_vand_vs_m:
  case RISCVVector::BI__builtin_rvv_vand_vs_mu:
  case RISCVVector::BI__builtin_rvv_vand_vs_tu:
  case RISCVVector::BI__builtin_rvv_vand_vs_tum:
  case RISCVVector::BI__builtin_rvv_vand_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vand_vv:
  case RISCVVector::BI__builtin_rvv_vand_vv_m:
  case RISCVVector::BI__builtin_rvv_vand_vv_mu:
  case RISCVVector::BI__builtin_rvv_vand_vv_tu:
  case RISCVVector::BI__builtin_rvv_vand_vv_tum:
  case RISCVVector::BI__builtin_rvv_vand_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vand_vx:
  case RISCVVector::BI__builtin_rvv_vand_vx_m:
  case RISCVVector::BI__builtin_rvv_vand_vx_mu:
  case RISCVVector::BI__builtin_rvv_vand_vx_tu:
  case RISCVVector::BI__builtin_rvv_vand_vx_tum:
  case RISCVVector::BI__builtin_rvv_vand_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vbit2fp_m:
  case RISCVVector::BI__builtin_rvv_vbit2fp_m_tu:
  case RISCVVector::BI__builtin_rvv_vcompress_vm:
  case RISCVVector::BI__builtin_rvv_vcompress_vm_tu:
  case RISCVVector::BI__builtin_rvv_vcos_v:
  case RISCVVector::BI__builtin_rvv_vcos_v_m:
  case RISCVVector::BI__builtin_rvv_vcos_v_mu:
  case RISCVVector::BI__builtin_rvv_vcos_v_rm:
  case RISCVVector::BI__builtin_rvv_vcos_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vcos_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vcos_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vcos_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vcos_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vcos_v_tu:
  case RISCVVector::BI__builtin_rvv_vcos_v_tum:
  case RISCVVector::BI__builtin_rvv_vcos_v_tumu:
  case RISCVVector::BI__builtin_rvv_vdiv_vv:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_m:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_mu:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_tu:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_tum:
  case RISCVVector::BI__builtin_rvv_vdiv_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vdiv_vx:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_m:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_mu:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_tu:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_tum:
  case RISCVVector::BI__builtin_rvv_vdiv_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vdivu_vv:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_m:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vdivu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vdivu_vx:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_m:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vdivu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vfabs_v:
  case RISCVVector::BI__builtin_rvv_vfabs_v_m:
  case RISCVVector::BI__builtin_rvv_vfabs_v_mu:
  case RISCVVector::BI__builtin_rvv_vfabs_v_tu:
  case RISCVVector::BI__builtin_rvv_vfabs_v_tum:
  case RISCVVector::BI__builtin_rvv_vfabs_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vs:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_rm:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_rm_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_m:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfadd_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfeq_vf:
  case RISCVVector::BI__builtin_rvv_vfeq_vf_m:
  case RISCVVector::BI__builtin_rvv_vfeq_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfeq_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfeq_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfeq_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfeq_vs:
  case RISCVVector::BI__builtin_rvv_vfeq_vs_m:
  case RISCVVector::BI__builtin_rvv_vfeq_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfeq_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfeq_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfeq_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfeq_vv:
  case RISCVVector::BI__builtin_rvv_vfeq_vv_m:
  case RISCVVector::BI__builtin_rvv_vfeq_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfeq_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfeq_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfeq_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfge_vf:
  case RISCVVector::BI__builtin_rvv_vfge_vf_m:
  case RISCVVector::BI__builtin_rvv_vfge_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfge_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfge_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfge_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfge_vv:
  case RISCVVector::BI__builtin_rvv_vfge_vv_m:
  case RISCVVector::BI__builtin_rvv_vfge_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfge_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfge_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfge_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfgt_vf:
  case RISCVVector::BI__builtin_rvv_vfgt_vf_m:
  case RISCVVector::BI__builtin_rvv_vfgt_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfgt_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfgt_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfgt_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfgt_vv:
  case RISCVVector::BI__builtin_rvv_vfgt_vv_m:
  case RISCVVector::BI__builtin_rvv_vfgt_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfgt_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfgt_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfgt_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfle_vf:
  case RISCVVector::BI__builtin_rvv_vfle_vf_m:
  case RISCVVector::BI__builtin_rvv_vfle_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfle_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfle_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfle_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfle_vs:
  case RISCVVector::BI__builtin_rvv_vfle_vs_m:
  case RISCVVector::BI__builtin_rvv_vfle_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfle_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfle_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfle_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfle_vv:
  case RISCVVector::BI__builtin_rvv_vfle_vv_m:
  case RISCVVector::BI__builtin_rvv_vfle_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfle_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfle_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfle_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vflt_vf:
  case RISCVVector::BI__builtin_rvv_vflt_vf_m:
  case RISCVVector::BI__builtin_rvv_vflt_vf_mu:
  case RISCVVector::BI__builtin_rvv_vflt_vf_tu:
  case RISCVVector::BI__builtin_rvv_vflt_vf_tum:
  case RISCVVector::BI__builtin_rvv_vflt_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vflt_vs:
  case RISCVVector::BI__builtin_rvv_vflt_vs_m:
  case RISCVVector::BI__builtin_rvv_vflt_vs_mu:
  case RISCVVector::BI__builtin_rvv_vflt_vs_tu:
  case RISCVVector::BI__builtin_rvv_vflt_vs_tum:
  case RISCVVector::BI__builtin_rvv_vflt_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vflt_vv:
  case RISCVVector::BI__builtin_rvv_vflt_vv_m:
  case RISCVVector::BI__builtin_rvv_vflt_vv_mu:
  case RISCVVector::BI__builtin_rvv_vflt_vv_tu:
  case RISCVVector::BI__builtin_rvv_vflt_vv_tum:
  case RISCVVector::BI__builtin_rvv_vflt_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmax_vf:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_m:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfmax_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfmax_vs:
  case RISCVVector::BI__builtin_rvv_vfmax_vs_m:
  case RISCVVector::BI__builtin_rvv_vfmax_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfmax_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfmax_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfmax_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfmax_vv:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_m:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfmax_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmerge_vfm:
  case RISCVVector::BI__builtin_rvv_vfmerge_vfm_tu:
  case RISCVVector::BI__builtin_rvv_vfmin_vf:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_m:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfmin_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfmin_vs:
  case RISCVVector::BI__builtin_rvv_vfmin_vs_m:
  case RISCVVector::BI__builtin_rvv_vfmin_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfmin_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfmin_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfmin_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfmin_vv:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_m:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfmin_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vs:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_rm:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_rm_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_m:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfmul_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfmv_s_f:
  case RISCVVector::BI__builtin_rvv_vfmv_s_f_tu:
  case RISCVVector::BI__builtin_rvv_vfmv_s_x:
  case RISCVVector::BI__builtin_rvv_vfmv_s_x_tu:
  case RISCVVector::BI__builtin_rvv_vfmv_v_f:
  case RISCVVector::BI__builtin_rvv_vfmv_v_f_tu:
  case RISCVVector::BI__builtin_rvv_vfne_vf:
  case RISCVVector::BI__builtin_rvv_vfne_vf_m:
  case RISCVVector::BI__builtin_rvv_vfne_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfne_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfne_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfne_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfne_vv:
  case RISCVVector::BI__builtin_rvv_vfne_vv_m:
  case RISCVVector::BI__builtin_rvv_vfne_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfne_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfne_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfne_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfneg_v:
  case RISCVVector::BI__builtin_rvv_vfneg_v_m:
  case RISCVVector::BI__builtin_rvv_vfneg_v_mu:
  case RISCVVector::BI__builtin_rvv_vfneg_v_tu:
  case RISCVVector::BI__builtin_rvv_vfneg_v_tum:
  case RISCVVector::BI__builtin_rvv_vfneg_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_m:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_mu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_tu:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_tum:
  case RISCVVector::BI__builtin_rvv_vfrec7_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs_m:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfredmax_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs_m:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfredmin_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_m:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm_m:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfredusum_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_m:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_mu:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tu:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tum:
  case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_m:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfrsub_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vfslide1down_vf:
  case RISCVVector::BI__builtin_rvv_vfslide1down_vf_m:
  case RISCVVector::BI__builtin_rvv_vfslide1down_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfslide1up_vf:
  case RISCVVector::BI__builtin_rvv_vfslide1up_vf_m:
  case RISCVVector::BI__builtin_rvv_vfslide1up_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_m:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_mu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_tu:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_tum:
  case RISCVVector::BI__builtin_rvv_vfsqrt_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfsquare_v:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_m:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_mu:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_rm:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_tu:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_tum:
  case RISCVVector::BI__builtin_rvv_vfsquare_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vf_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vs:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_rm:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_m:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_tu:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_tum:
  case RISCVVector::BI__builtin_rvv_vfsub_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vid_v:
  case RISCVVector::BI__builtin_rvv_vid_v_m:
  case RISCVVector::BI__builtin_rvv_vid_v_mu:
  case RISCVVector::BI__builtin_rvv_vid_v_tu:
  case RISCVVector::BI__builtin_rvv_vid_v_tum:
  case RISCVVector::BI__builtin_rvv_vid_v_tumu:
  case RISCVVector::BI__builtin_rvv_viota_m:
  case RISCVVector::BI__builtin_rvv_viota_m_m:
  case RISCVVector::BI__builtin_rvv_viota_m_mu:
  case RISCVVector::BI__builtin_rvv_viota_m_tu:
  case RISCVVector::BI__builtin_rvv_viota_m_tum:
  case RISCVVector::BI__builtin_rvv_viota_m_tumu:
  case RISCVVector::BI__builtin_rvv_vl2d_v:
  case RISCVVector::BI__builtin_rvv_vl2d_v_m:
  case RISCVVector::BI__builtin_rvv_vl2d_v_mu:
  case RISCVVector::BI__builtin_rvv_vl2d_v_tu:
  case RISCVVector::BI__builtin_rvv_vl2d_v_tum:
  case RISCVVector::BI__builtin_rvv_vl2d_v_tumu:
  case RISCVVector::BI__builtin_rvv_vlog2_v:
  case RISCVVector::BI__builtin_rvv_vlog2_v_m:
  case RISCVVector::BI__builtin_rvv_vlog2_v_mu:
  case RISCVVector::BI__builtin_rvv_vlog2_v_rm:
  case RISCVVector::BI__builtin_rvv_vlog2_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vlog2_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vlog2_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vlog2_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vlog2_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vlog2_v_tu:
  case RISCVVector::BI__builtin_rvv_vlog2_v_tum:
  case RISCVVector::BI__builtin_rvv_vlog2_v_tumu:
  case RISCVVector::BI__builtin_rvv_vloxei32_v:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_m:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_mu:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_tu:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_tum:
  case RISCVVector::BI__builtin_rvv_vloxei32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vloxei64_v:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_m:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_mu:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_tu:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_tum:
  case RISCVVector::BI__builtin_rvv_vloxei64_v_tumu:
  case RISCVVector::BI__builtin_rvv_vluxei32_sync_v:
  case RISCVVector::BI__builtin_rvv_vluxei32_sync_v_m:
  case RISCVVector::BI__builtin_rvv_vluxei32_sync_v_mu:
  case RISCVVector::BI__builtin_rvv_vluxei32_sync_v_tu:
  case RISCVVector::BI__builtin_rvv_vluxei32_sync_v_tum:
  case RISCVVector::BI__builtin_rvv_vluxei32_sync_v_tumu:
  case RISCVVector::BI__builtin_rvv_vluxei32_v:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_m:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_mu:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_tu:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_tum:
  case RISCVVector::BI__builtin_rvv_vluxei32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vluxei64_sync_v:
  case RISCVVector::BI__builtin_rvv_vluxei64_sync_v_m:
  case RISCVVector::BI__builtin_rvv_vluxei64_sync_v_mu:
  case RISCVVector::BI__builtin_rvv_vluxei64_sync_v_tu:
  case RISCVVector::BI__builtin_rvv_vluxei64_sync_v_tum:
  case RISCVVector::BI__builtin_rvv_vluxei64_sync_v_tumu:
  case RISCVVector::BI__builtin_rvv_vluxei64_v:
  case RISCVVector::BI__builtin_rvv_vluxei64_v_m:
  case RISCVVector::BI__builtin_rvv_vluxei64_v_mu:
  case RISCVVector::BI__builtin_rvv_vluxei64_v_tu:
  case RISCVVector::BI__builtin_rvv_vluxei64_v_tum:
  case RISCVVector::BI__builtin_rvv_vluxei64_v_tumu:
  case RISCVVector::BI__builtin_rvv_vmax_vs:
  case RISCVVector::BI__builtin_rvv_vmax_vs_m:
  case RISCVVector::BI__builtin_rvv_vmax_vs_mu:
  case RISCVVector::BI__builtin_rvv_vmax_vs_tu:
  case RISCVVector::BI__builtin_rvv_vmax_vs_tum:
  case RISCVVector::BI__builtin_rvv_vmax_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vmax_vv:
  case RISCVVector::BI__builtin_rvv_vmax_vv_m:
  case RISCVVector::BI__builtin_rvv_vmax_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmax_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmax_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmax_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmax_vx:
  case RISCVVector::BI__builtin_rvv_vmax_vx_m:
  case RISCVVector::BI__builtin_rvv_vmax_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmax_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmax_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmax_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vs:
  case RISCVVector::BI__builtin_rvv_vmaxu_vs_m:
  case RISCVVector::BI__builtin_rvv_vmaxu_vs_mu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vs_tu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vs_tum:
  case RISCVVector::BI__builtin_rvv_vmaxu_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmaxu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmaxu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmerge_vvm:
  case RISCVVector::BI__builtin_rvv_vmerge_vvm_tu:
  case RISCVVector::BI__builtin_rvv_vmerge_vxm:
  case RISCVVector::BI__builtin_rvv_vmerge_vxm_tu:
  case RISCVVector::BI__builtin_rvv_vmin_vs:
  case RISCVVector::BI__builtin_rvv_vmin_vs_m:
  case RISCVVector::BI__builtin_rvv_vmin_vs_mu:
  case RISCVVector::BI__builtin_rvv_vmin_vs_tu:
  case RISCVVector::BI__builtin_rvv_vmin_vs_tum:
  case RISCVVector::BI__builtin_rvv_vmin_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vmin_vv:
  case RISCVVector::BI__builtin_rvv_vmin_vv_m:
  case RISCVVector::BI__builtin_rvv_vmin_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmin_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmin_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmin_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmin_vx:
  case RISCVVector::BI__builtin_rvv_vmin_vx_m:
  case RISCVVector::BI__builtin_rvv_vmin_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmin_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmin_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmin_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vminu_vs:
  case RISCVVector::BI__builtin_rvv_vminu_vs_m:
  case RISCVVector::BI__builtin_rvv_vminu_vs_mu:
  case RISCVVector::BI__builtin_rvv_vminu_vs_tu:
  case RISCVVector::BI__builtin_rvv_vminu_vs_tum:
  case RISCVVector::BI__builtin_rvv_vminu_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vminu_vv:
  case RISCVVector::BI__builtin_rvv_vminu_vv_m:
  case RISCVVector::BI__builtin_rvv_vminu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vminu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vminu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vminu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vminu_vx:
  case RISCVVector::BI__builtin_rvv_vminu_vx_m:
  case RISCVVector::BI__builtin_rvv_vminu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vminu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vminu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vminu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmul_vv:
  case RISCVVector::BI__builtin_rvv_vmul_vv_m:
  case RISCVVector::BI__builtin_rvv_vmul_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmul_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmul_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmul_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmul_vx:
  case RISCVVector::BI__builtin_rvv_vmul_vx_m:
  case RISCVVector::BI__builtin_rvv_vmul_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmul_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmul_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmul_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmulh_vv:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_m:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmulh_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmulh_vx:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_m:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmulh_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_m:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vmulhu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_m:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vmulhu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vmv_s_x:
  case RISCVVector::BI__builtin_rvv_vmv_s_x_tu:
  case RISCVVector::BI__builtin_rvv_vmv_v_v:
  case RISCVVector::BI__builtin_rvv_vmv_v_v_tu:
  case RISCVVector::BI__builtin_rvv_vmv_v_x:
  case RISCVVector::BI__builtin_rvv_vmv_v_x_tu:
  case RISCVVector::BI__builtin_rvv_vneg_v:
  case RISCVVector::BI__builtin_rvv_vneg_v_m:
  case RISCVVector::BI__builtin_rvv_vneg_v_mu:
  case RISCVVector::BI__builtin_rvv_vneg_v_tu:
  case RISCVVector::BI__builtin_rvv_vneg_v_tum:
  case RISCVVector::BI__builtin_rvv_vneg_v_tumu:
  case RISCVVector::BI__builtin_rvv_vnot_v:
  case RISCVVector::BI__builtin_rvv_vnot_v_m:
  case RISCVVector::BI__builtin_rvv_vnot_v_mu:
  case RISCVVector::BI__builtin_rvv_vnot_v_tu:
  case RISCVVector::BI__builtin_rvv_vnot_v_tum:
  case RISCVVector::BI__builtin_rvv_vnot_v_tumu:
  case RISCVVector::BI__builtin_rvv_vor_vs:
  case RISCVVector::BI__builtin_rvv_vor_vs_m:
  case RISCVVector::BI__builtin_rvv_vor_vs_mu:
  case RISCVVector::BI__builtin_rvv_vor_vs_tu:
  case RISCVVector::BI__builtin_rvv_vor_vs_tum:
  case RISCVVector::BI__builtin_rvv_vor_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vor_vv:
  case RISCVVector::BI__builtin_rvv_vor_vv_m:
  case RISCVVector::BI__builtin_rvv_vor_vv_mu:
  case RISCVVector::BI__builtin_rvv_vor_vv_tu:
  case RISCVVector::BI__builtin_rvv_vor_vv_tum:
  case RISCVVector::BI__builtin_rvv_vor_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vor_vx:
  case RISCVVector::BI__builtin_rvv_vor_vx_m:
  case RISCVVector::BI__builtin_rvv_vor_vx_mu:
  case RISCVVector::BI__builtin_rvv_vor_vx_tu:
  case RISCVVector::BI__builtin_rvv_vor_vx_tum:
  case RISCVVector::BI__builtin_rvv_vor_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vpow2_v:
  case RISCVVector::BI__builtin_rvv_vpow2_v_m:
  case RISCVVector::BI__builtin_rvv_vpow2_v_mu:
  case RISCVVector::BI__builtin_rvv_vpow2_v_rm:
  case RISCVVector::BI__builtin_rvv_vpow2_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vpow2_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vpow2_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vpow2_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vpow2_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vpow2_v_tu:
  case RISCVVector::BI__builtin_rvv_vpow2_v_tum:
  case RISCVVector::BI__builtin_rvv_vpow2_v_tumu:
  case RISCVVector::BI__builtin_rvv_vrem_vv:
  case RISCVVector::BI__builtin_rvv_vrem_vv_m:
  case RISCVVector::BI__builtin_rvv_vrem_vv_mu:
  case RISCVVector::BI__builtin_rvv_vrem_vv_tu:
  case RISCVVector::BI__builtin_rvv_vrem_vv_tum:
  case RISCVVector::BI__builtin_rvv_vrem_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vrem_vx:
  case RISCVVector::BI__builtin_rvv_vrem_vx_m:
  case RISCVVector::BI__builtin_rvv_vrem_vx_mu:
  case RISCVVector::BI__builtin_rvv_vrem_vx_tu:
  case RISCVVector::BI__builtin_rvv_vrem_vx_tum:
  case RISCVVector::BI__builtin_rvv_vrem_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vremu_vv:
  case RISCVVector::BI__builtin_rvv_vremu_vv_m:
  case RISCVVector::BI__builtin_rvv_vremu_vv_mu:
  case RISCVVector::BI__builtin_rvv_vremu_vv_tu:
  case RISCVVector::BI__builtin_rvv_vremu_vv_tum:
  case RISCVVector::BI__builtin_rvv_vremu_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vremu_vx:
  case RISCVVector::BI__builtin_rvv_vremu_vx_m:
  case RISCVVector::BI__builtin_rvv_vremu_vx_mu:
  case RISCVVector::BI__builtin_rvv_vremu_vx_tu:
  case RISCVVector::BI__builtin_rvv_vremu_vx_tum:
  case RISCVVector::BI__builtin_rvv_vremu_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vreverse_v:
  case RISCVVector::BI__builtin_rvv_vreverse_v_m:
  case RISCVVector::BI__builtin_rvv_vreverse_v_mu:
  case RISCVVector::BI__builtin_rvv_vreverse_v_tu:
  case RISCVVector::BI__builtin_rvv_vreverse_v_tum:
  case RISCVVector::BI__builtin_rvv_vreverse_v_tumu:
  case RISCVVector::BI__builtin_rvv_vrgather_vv:
  case RISCVVector::BI__builtin_rvv_vrgather_vv_m:
  case RISCVVector::BI__builtin_rvv_vrgather_vv_mu:
  case RISCVVector::BI__builtin_rvv_vrgather_vv_tu:
  case RISCVVector::BI__builtin_rvv_vrgather_vv_tum:
  case RISCVVector::BI__builtin_rvv_vrgather_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vrgather_vx:
  case RISCVVector::BI__builtin_rvv_vrgather_vx_m:
  case RISCVVector::BI__builtin_rvv_vrgather_vx_mu:
  case RISCVVector::BI__builtin_rvv_vrgather_vx_tu:
  case RISCVVector::BI__builtin_rvv_vrgather_vx_tum:
  case RISCVVector::BI__builtin_rvv_vrgather_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vrgatherei16_vv:
  case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_m:
  case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_mu:
  case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tu:
  case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tum:
  case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vrsub_vx:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_m:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_mu:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_tu:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_tum:
  case RISCVVector::BI__builtin_rvv_vrsub_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsbc_vvm:
  case RISCVVector::BI__builtin_rvv_vsbc_vvm_tu:
  case RISCVVector::BI__builtin_rvv_vsbc_vxm:
  case RISCVVector::BI__builtin_rvv_vsbc_vxm_tu:
  case RISCVVector::BI__builtin_rvv_vsin_v:
  case RISCVVector::BI__builtin_rvv_vsin_v_m:
  case RISCVVector::BI__builtin_rvv_vsin_v_mu:
  case RISCVVector::BI__builtin_rvv_vsin_v_rm:
  case RISCVVector::BI__builtin_rvv_vsin_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vsin_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vsin_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vsin_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vsin_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vsin_v_tu:
  case RISCVVector::BI__builtin_rvv_vsin_v_tum:
  case RISCVVector::BI__builtin_rvv_vsin_v_tumu:
  case RISCVVector::BI__builtin_rvv_vslide1down_vx:
  case RISCVVector::BI__builtin_rvv_vslide1down_vx_m:
  case RISCVVector::BI__builtin_rvv_vslide1down_vx_mu:
  case RISCVVector::BI__builtin_rvv_vslide1down_vx_tu:
  case RISCVVector::BI__builtin_rvv_vslide1down_vx_tum:
  case RISCVVector::BI__builtin_rvv_vslide1down_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vslide1up_vx:
  case RISCVVector::BI__builtin_rvv_vslide1up_vx_m:
  case RISCVVector::BI__builtin_rvv_vslide1up_vx_mu:
  case RISCVVector::BI__builtin_rvv_vslide1up_vx_tu:
  case RISCVVector::BI__builtin_rvv_vslide1up_vx_tum:
  case RISCVVector::BI__builtin_rvv_vslide1up_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vslidedown_vx:
  case RISCVVector::BI__builtin_rvv_vslidedown_vx_m:
  case RISCVVector::BI__builtin_rvv_vslidedown_vx_mu:
  case RISCVVector::BI__builtin_rvv_vslidedown_vx_tu:
  case RISCVVector::BI__builtin_rvv_vslidedown_vx_tum:
  case RISCVVector::BI__builtin_rvv_vslidedown_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vslideup_vx:
  case RISCVVector::BI__builtin_rvv_vslideup_vx_m:
  case RISCVVector::BI__builtin_rvv_vslideup_vx_mu:
  case RISCVVector::BI__builtin_rvv_vslideup_vx_tu:
  case RISCVVector::BI__builtin_rvv_vslideup_vx_tum:
  case RISCVVector::BI__builtin_rvv_vslideup_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsll_vv:
  case RISCVVector::BI__builtin_rvv_vsll_vv_m:
  case RISCVVector::BI__builtin_rvv_vsll_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsll_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsll_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsll_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsll_vx:
  case RISCVVector::BI__builtin_rvv_vsll_vx_m:
  case RISCVVector::BI__builtin_rvv_vsll_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsll_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsll_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsll_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsra_vv:
  case RISCVVector::BI__builtin_rvv_vsra_vv_m:
  case RISCVVector::BI__builtin_rvv_vsra_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsra_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsra_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsra_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsra_vx:
  case RISCVVector::BI__builtin_rvv_vsra_vx_m:
  case RISCVVector::BI__builtin_rvv_vsra_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsra_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsra_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsra_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsrl_vv:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_m:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsrl_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsrl_vx:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_m:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsrl_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vsub_vv:
  case RISCVVector::BI__builtin_rvv_vsub_vv_m:
  case RISCVVector::BI__builtin_rvv_vsub_vv_mu:
  case RISCVVector::BI__builtin_rvv_vsub_vv_tu:
  case RISCVVector::BI__builtin_rvv_vsub_vv_tum:
  case RISCVVector::BI__builtin_rvv_vsub_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vsub_vx:
  case RISCVVector::BI__builtin_rvv_vsub_vx_m:
  case RISCVVector::BI__builtin_rvv_vsub_vx_mu:
  case RISCVVector::BI__builtin_rvv_vsub_vx_tu:
  case RISCVVector::BI__builtin_rvv_vsub_vx_tum:
  case RISCVVector::BI__builtin_rvv_vsub_vx_tumu:
  case RISCVVector::BI__builtin_rvv_vxor_vs:
  case RISCVVector::BI__builtin_rvv_vxor_vs_m:
  case RISCVVector::BI__builtin_rvv_vxor_vs_mu:
  case RISCVVector::BI__builtin_rvv_vxor_vs_tu:
  case RISCVVector::BI__builtin_rvv_vxor_vs_tum:
  case RISCVVector::BI__builtin_rvv_vxor_vs_tumu:
  case RISCVVector::BI__builtin_rvv_vxor_vv:
  case RISCVVector::BI__builtin_rvv_vxor_vv_m:
  case RISCVVector::BI__builtin_rvv_vxor_vv_mu:
  case RISCVVector::BI__builtin_rvv_vxor_vv_tu:
  case RISCVVector::BI__builtin_rvv_vxor_vv_tum:
  case RISCVVector::BI__builtin_rvv_vxor_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vxor_vx:
  case RISCVVector::BI__builtin_rvv_vxor_vx_m:
  case RISCVVector::BI__builtin_rvv_vxor_vx_mu:
  case RISCVVector::BI__builtin_rvv_vxor_vx_tu:
  case RISCVVector::BI__builtin_rvv_vxor_vx_tum:
  case RISCVVector::BI__builtin_rvv_vxor_vx_tumu:
    return CheckFixedVector(TheCall, TheCall->getType(), {16,32});
  // ---- CheckFixedVector(call, {32})  (78 IDs) ----
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_rm:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_rm:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_rm:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_rm:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_rm_m:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_rm_mu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_rm_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_rm_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_rm_tumu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvtbf16_f_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_m:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_mu:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tu:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tum:
  case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tumu:
    return CheckFixedVector(TheCall, TheCall->getType(), {32});
  // ---- CheckFixedVector(call, {64})  (12 IDs) ----
  case RISCVVector::BI__builtin_rvv_vslh_vv:
  case RISCVVector::BI__builtin_rvv_vslh_vv_m:
  case RISCVVector::BI__builtin_rvv_vslh_vv_mu:
  case RISCVVector::BI__builtin_rvv_vslh_vv_tu:
  case RISCVVector::BI__builtin_rvv_vslh_vv_tum:
  case RISCVVector::BI__builtin_rvv_vslh_vv_tumu:
  case RISCVVector::BI__builtin_rvv_vslh_vx:
  case RISCVVector::BI__builtin_rvv_vslh_vx_m:
  case RISCVVector::BI__builtin_rvv_vslh_vx_mu:
  case RISCVVector::BI__builtin_rvv_vslh_vx_tu:
  case RISCVVector::BI__builtin_rvv_vslh_vx_tum:
  case RISCVVector::BI__builtin_rvv_vslh_vx_tumu:
    return CheckFixedVector(TheCall, TheCall->getType(), {64});
  // ---- CheckFixedVector(call, {8})  (10 IDs) ----
  case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_x_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_x_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_x_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvtbf16_x_f_w_rm_m:
    return CheckFixedVector(TheCall, TheCall->getType(), {8});
  // ---- BuiltinConstantArgRange(RANGE(1), 0, 1)  (6 IDs) ----
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_bf_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_bf_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_f_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_bf_w_rm:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_f_w_rm:
    return SemaRef.BuiltinConstantArgRange(TheCall, 1, 0, 1, true);
  // ---- BuiltinConstantArgRange(RANGE(2), 0, 1)  (6 IDs) ----
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_bf_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_bf_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_f_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_bf_w_rm_m:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_f_w_rm_m:
    return SemaRef.BuiltinConstantArgRange(TheCall, 2, 0, 1, true);
  // ---- RET(0)  (114 IDs) ----
  case RISCVVector::BI__builtin_rvv_mov128_s_v:
  case RISCVVector::BI__builtin_rvv_mov128_v_s:
  case RISCVVector::BI__builtin_rvv_vcpop_m_m:
  case RISCVVector::BI__builtin_rvv_vcreate_v:
  case RISCVVector::BI__builtin_rvv_vfirst_m:
  case RISCVVector::BI__builtin_rvv_vfirst_m_m:
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_bf_w:
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_bf_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvte4m3_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_bf_w:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_bf_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvte5m2_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_bf_w:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_bf_w_m:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_f_w:
  case RISCVVector::BI__builtin_rvv_vfncvtfp4_f_f_w_m:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_bf_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_bf_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_bf_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_bf_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_bf_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_bf_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_f_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_f_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_f_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_f_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_f_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvte4m3_f_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_bf_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_bf_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_bf_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_bf_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_bf_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_bf_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_f_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_f_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_f_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_f_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_f_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvte5m2_f_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_bf_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_bf_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_bf_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_bf_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_bf_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_bf_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_f_f_v:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_f_f_v_m:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_f_f_v_mu:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_f_f_v_tu:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_f_f_v_tum:
  case RISCVVector::BI__builtin_rvv_vfwcvtfp4_f_f_v_tumu:
  case RISCVVector::BI__builtin_rvv_vget_v:
  case RISCVVector::BI__builtin_rvv_vle16_v:
  case RISCVVector::BI__builtin_rvv_vle16_v_m:
  case RISCVVector::BI__builtin_rvv_vle16_v_mu:
  case RISCVVector::BI__builtin_rvv_vle16_v_tu:
  case RISCVVector::BI__builtin_rvv_vle16_v_tum:
  case RISCVVector::BI__builtin_rvv_vle16_v_tumu:
  case RISCVVector::BI__builtin_rvv_vle32_v:
  case RISCVVector::BI__builtin_rvv_vle32_v_m:
  case RISCVVector::BI__builtin_rvv_vle32_v_mu:
  case RISCVVector::BI__builtin_rvv_vle32_v_tu:
  case RISCVVector::BI__builtin_rvv_vle32_v_tum:
  case RISCVVector::BI__builtin_rvv_vle32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vlmul_ext_v:
  case RISCVVector::BI__builtin_rvv_vlmul_trunc_v:
  case RISCVVector::BI__builtin_rvv_vlse16_v:
  case RISCVVector::BI__builtin_rvv_vlse16_v_m:
  case RISCVVector::BI__builtin_rvv_vlse16_v_mu:
  case RISCVVector::BI__builtin_rvv_vlse16_v_tu:
  case RISCVVector::BI__builtin_rvv_vlse16_v_tum:
  case RISCVVector::BI__builtin_rvv_vlse16_v_tumu:
  case RISCVVector::BI__builtin_rvv_vlse32_v:
  case RISCVVector::BI__builtin_rvv_vlse32_v_m:
  case RISCVVector::BI__builtin_rvv_vlse32_v_mu:
  case RISCVVector::BI__builtin_rvv_vlse32_v_tu:
  case RISCVVector::BI__builtin_rvv_vlse32_v_tum:
  case RISCVVector::BI__builtin_rvv_vlse32_v_tumu:
  case RISCVVector::BI__builtin_rvv_vmand_mm:
  case RISCVVector::BI__builtin_rvv_vmandn_mm:
  case RISCVVector::BI__builtin_rvv_vmclr_m:
  case RISCVVector::BI__builtin_rvv_vmmv_m:
  case RISCVVector::BI__builtin_rvv_vmnand_mm:
  case RISCVVector::BI__builtin_rvv_vmnor_mm:
  case RISCVVector::BI__builtin_rvv_vmnot_m:
  case RISCVVector::BI__builtin_rvv_vmor_mm:
  case RISCVVector::BI__builtin_rvv_vmorn_mm:
  case RISCVVector::BI__builtin_rvv_vmsbf_m:
  case RISCVVector::BI__builtin_rvv_vmsbf_m_m:
  case RISCVVector::BI__builtin_rvv_vmset_m:
  case RISCVVector::BI__builtin_rvv_vmsif_m:
  case RISCVVector::BI__builtin_rvv_vmsif_m_m:
  case RISCVVector::BI__builtin_rvv_vmsof_m:
  case RISCVVector::BI__builtin_rvv_vmsof_m_m:
  case RISCVVector::BI__builtin_rvv_vmxnor_mm:
  case RISCVVector::BI__builtin_rvv_vmxor_mm:
  case RISCVVector::BI__builtin_rvv_vreinterpret_v:
  case RISCVVector::BI__builtin_rvv_vse16_v:
  case RISCVVector::BI__builtin_rvv_vse16_v_m:
  case RISCVVector::BI__builtin_rvv_vse32_v:
  case RISCVVector::BI__builtin_rvv_vse32_v_m:
  case RISCVVector::BI__builtin_rvv_vset_v:
  case RISCVVector::BI__builtin_rvv_vsetvli:
  case RISCVVector::BI__builtin_rvv_vsetvli_bf:
  case RISCVVector::BI__builtin_rvv_vsetvlimax:
  case RISCVVector::BI__builtin_rvv_vsetvlimax_bf:
  case RISCVVector::BI__builtin_rvv_vsse16_v:
  case RISCVVector::BI__builtin_rvv_vsse16_v_m:
  case RISCVVector::BI__builtin_rvv_vsse32_v:
  case RISCVVector::BI__builtin_rvv_vsse32_v_m:
  case RISCVVector::BI__builtin_rvv_vundefined:
    return false;
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
