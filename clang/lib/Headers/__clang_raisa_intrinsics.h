/*===---- __clang_raisa_intrinsics.h - RAISA device-side intrinsic wrappers ---===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __CLANG_RAISA_INTRINSICS_H__
#define __CLANG_RAISA_INTRINSICS_H__

#ifndef __RAISA__
#error "This file is for RAISA compilation only."
#endif

#define __RAISA_INTRINSICS_DECL__                                              \
  __device__ inline __attribute__((always_inline, nothrow))

__RAISA_INTRINSICS_DECL__ float __reduce_sum(float __val) {
  return __raisa_reduce_sum_f(__val);
}
__RAISA_INTRINSICS_DECL__ half __reduce_sum(half __val) {
  return __raisa_reduce_sum_f16(__val);
}
__RAISA_INTRINSICS_DECL__ __bf16 __reduce_sum(__bf16 __val) {
  return __raisa_reduce_sum_bf16(__val);
}

__RAISA_INTRINSICS_DECL__ float __reduce_fmax(float __val) {
  return __raisa_reduce_fmax_f(__val);
}
__RAISA_INTRINSICS_DECL__ half __reduce_fmax(half __val) {
  return __raisa_reduce_fmax_f16(__val);
}
__RAISA_INTRINSICS_DECL__ __bf16 __reduce_fmax(__bf16 __val) {
  return __raisa_reduce_fmax_bf16(__val);
}

__RAISA_INTRINSICS_DECL__ float __reduce_fmin(float __val) {
  return __raisa_reduce_fmin_f(__val);
}
__RAISA_INTRINSICS_DECL__ half __reduce_fmin(half __val) {
  return __raisa_reduce_fmin_f16(__val);
}
__RAISA_INTRINSICS_DECL__ __bf16 __reduce_fmin(__bf16 __val) {
  return __raisa_reduce_fmin_bf16(__val);
}

#undef __RAISA_INTRINSICS_DECL__

#endif // __CLANG_RAISA_INTRINSICS_H__
