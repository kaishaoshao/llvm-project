/*===---- __clang_raisa_math.h - Device-side CUDA math support --------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __CLANG_RAISA_MATH_H__
#define __CLANG_RAISA_MATH_H__

#ifndef __RAISA__
#error "This file is for RAISA compilation only."
#endif

// __DEVICE__ is a helper macro with common set of attributes for the wrappers
// we implement in this file. We need static in order to avoid emitting unused
// functions and __forceinline__ helps inlining these wrappers at -O1.
#pragma push_macro("__DEVICE__")

#define __DEVICE__ __device__ inline __attribute__((always_inline, nothrow))

// Generates the float / half / __bf16 variants for unary operations,
// e.g. __RAISA_UNARY(__sin, __raisa_sin) defines __sinf / __sinf16 / __sinbf16.
#define __RAISA_UNARY(FN, BUILTIN)                                              \
  __DEVICE__ float FN##f(float __a) { return BUILTIN##_f(__a); }               \
  __DEVICE__ half FN##f16(half __a) { return BUILTIN##_f16(__a); }             \
  __DEVICE__ __bf16 FN##bf16(__bf16 __a) { return BUILTIN##_bf16(__a); }

__RAISA_UNARY(__exp2, __raisa_exp2)
__RAISA_UNARY(__rsqrt, __raisa_frsqrt)
__RAISA_UNARY(__log2, __raisa_log2)
__RAISA_UNARY(__sin, __raisa_sin)
__RAISA_UNARY(__cos, __raisa_cos)

#undef __RAISA_UNARY

// Generates the float / half / __bf16 variants for binary operations,
// e.g. __RAISA_BINARY(__max, __raisa_fmax) defines __maxf / __maxf16 / __maxbf16.
#define __RAISA_BINARY(FN, BUILTIN)                                             \
  __DEVICE__ float FN##f(float __a, float __b) { return BUILTIN##_f(__a, __b); }    \
  __DEVICE__ half FN##f16(half __a, half __b) { return BUILTIN##_f16(__a, __b); }   \
  __DEVICE__ __bf16 FN##bf16(__bf16 __a, __bf16 __b) { return BUILTIN##_bf16(__a, __b); }

__RAISA_BINARY(__max, __raisa_fmax)
__RAISA_BINARY(__min, __raisa_fmin)
__RAISA_BINARY(__div, __raisa_fdiv)

#undef __RAISA_BINARY

__DEVICE__ int __max(int __a, int __b) { return __raisa_max(__a, __b); }
__DEVICE__ int __min(int __a, int __b) { return __raisa_min(__a, __b); }

__DEVICE__ unsigned int __umax(unsigned int __a, unsigned int __b) {
  return __raisa_umax(__a, __b);
}

__DEVICE__ unsigned int __umin(unsigned int __a, unsigned int __b) {
  return __raisa_umin(__a, __b);
}

__DEVICE__ int __div(int __a, int __b) { return __raisa_div(__a, __b); }

__DEVICE__ float __expf(float __a) {
  const float __log2_e = 1.442695;
  float a = __log2_e * __a;
  return __raisa_exp2_f(a);
}

__DEVICE__ half __expf16(half __a) {
  const half __log2_e = 1.442695;
  half a = __log2_e * __a;
  return __raisa_exp2_f16(a);
}

__DEVICE__ __bf16 __expbf16(__bf16 __a) {
  const __bf16 __log2_e = 1.442695;
  __bf16 a = __log2_e * __a;
  return __raisa_exp2_bf16(a);
}

__DEVICE__ float __logf(float __a) {
  const float LN2 = 0.6931471805599453f;
  return __log2f(__a) * LN2;
}

__DEVICE__ float __powf(float __a, float __b) {
  return __exp2f(__b * __log2f(__a));
}

__DEVICE__ half __powf16(half __a, half __b) {
  return __exp2f16(__b * __log2f16(__a));
}

__DEVICE__ __bf16 __powbf16(__bf16 __a, __bf16 __b) {
  return __exp2bf16(__b * __log2bf16(__a));
}

#pragma pop_macro("__DEVICE__")

#include "__clang_raisa_intrinsics.h"
#include "__clang_raisa_libdevice.h"

#endif // __CLANG_RAISA_MATH_H__
