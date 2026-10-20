/*===---- __clang_raisa_libdevice.h - Device-side libdevice -------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __CLANG_RAISA_LIBDEVICE_H__
#define __CLANG_RAISA_LIBDEVICE_H__

// Software implementations of math functions used as libcall fallbacks
// when the hardware intrinsics are lowered via the DAG legalizer for
// uniform (scalar) inputs.  All arithmetic is float-only (no double)
// because hardware lacks the D extension and double runtime functions
// (__truncdfsf2 etc.) are unavailable in the device environment.
//
// __device__ ensures they are compiled for the device side.
// __attribute__((no_vector)) prevents SIMT->SIMD vectorization.


// ---- Internal helpers -------------------------------------------------------

// Type-punning: float <-> uint32_t without undefined behavior.
static __device__ __attribute__((no_vector)) unsigned __raisadev_as_uint(
    float __f) {
  union {
    float __f;
    unsigned __i;
  } __u = {__f};
  return __u.__i;
}

#define GET_FLOAT_WORD(__w, __d) __w = __raisadev_as_uint(__d)

#define FORCE_EVAL(__x)                                                        \
  do {                                                                         \
    volatile float __y = (__x);                                                \
    (void)__y;                                                                 \
  } while (0)

// ---- float constants (all double constants truncated to float) --------------

// pi/2 and its multiples.
#define __RAISADEV_PI2 1.57079637f
#define __RAISADEV_C1PIO2 1.57079637f
#define __RAISADEV_C2PIO2 3.14159274f
#define __RAISADEV_C3PIO2 4.71238899f
#define __RAISADEV_C4PIO2 6.28318548f

// ---- __cosdf: core cos polynomial, |x| <= pi/4 -------------------------------
// Float version of musl __cosdf.  Error ~5e-7 (vs 5e-11 for double).

#define __RAISADEV_C0 (-0.49999997f)
#define __RAISADEV_C1 (0.041666623f)
#define __RAISADEV_C2 (-0.0013886764f)
#define __RAISADEV_C3 (0.00002439045f)

static __device__ __attribute__((no_vector)) float __raisadev_cosdf(float __x) {
  float __z = __x * __x;
  float __w = __z * __z;
  float __r = __RAISADEV_C2 + __z * __RAISADEV_C3;
  return (1.0f + __z * __RAISADEV_C0) + __w * __RAISADEV_C1 +
         (__w * __z) * __r;
}

// ---- __sindf: core sin polynomial, |x| <= pi/4 -------------------------------
// Float version of musl __sindf.  Error ~5e-7 (vs 5e-12 for double).

#define __RAISADEV_S1 (-0.16666666f)
#define __RAISADEV_S2 (0.00833333f)
#define __RAISADEV_S3 (-0.00019839335f)
#define __RAISADEV_S4 (0.0000027183115f)

static __device__ __attribute__((no_vector)) float __raisadev_sindf(float __x) {
  float __z = __x * __x;
  float __w = __z * __z;
  float __r = __RAISADEV_S3 + __z * __RAISADEV_S4;
  float __s = __z * __x;
  return (__x + __s * (__RAISADEV_S1 + __z * __RAISADEV_S2)) + __s * __w * __r;
}

// ---- __rem_pio2f: argument reduction x mod pi/2 -----------------------------
// Returns the quadrant n and sets *y = x - n*pi/2.
// Float-only version using the toint rint() trick (works for |x| < 2^23*pi/2).

#define __RAISADEV_TOINT 12582912.0f  // 1.5f / FLT_EPSILON
#define __RAISADEV_INVPIO2 0.636619772f  // 2/pi
#define __RAISADEV_PIO2 1.57079637f  // pi/2

static __device__ __attribute__((no_vector)) int __raisadev_rem_pio2f(
    float __x, float *__y) {
  union {
    float __f;
    unsigned __i;
  } __u = {__x};
  unsigned __ix = __u.__i & 0x7fffffff;

  // |x| < 2^23*(pi/2): medium size, use specialized rint().
  // For larger |x| the toint trick loses precision; fall back to NaN.
  if (__ix < 0x4b000000) {
    float __fn =
        __x * __RAISADEV_INVPIO2 + __RAISADEV_TOINT - __RAISADEV_TOINT;
    int __n = (int)__fn;
    *__y = __x - __fn * __RAISADEV_PIO2;

    // Correct for rounding errors near quadrant boundaries.
    const float __pio4 = 0.785398185f; // pi/4
    if (*__y < -__pio4) {
      __n--;
      __fn -= 1.0f;
      *__y = __x - __fn * __RAISADEV_PIO2;
    } else if (*__y > __pio4) {
      __n++;
      __fn += 1.0f;
      *__y = __x - __fn * __RAISADEV_PIO2;
    }
    return __n;
  }

  // x is inf or NaN.
  if (__ix >= 0x7f800000) {
    *__y = __x - __x;
    return 0;
  }

  // TODO: large argument reduction via __rem_pio2_large for |x| >= 2^23*(pi/2).
  // Kernel inputs are typically small; this path is not yet implemented.
  *__y = __x - __x; // return NaN
  return 0;
}

// ---- exp2f helpers ---------------------------------------------------------
// Pure-float exp2f, adapted from musl src/math/exp2f.c.
// Algorithm: x = k/N + r, k integer, r in [-1/(2N), 1/(2N)], N=32.
// exp2(x) = 2^(k/N) * 2^r, 2^(k/N) from table, 2^r via cubic polynomial.
// ULP error ~1-2 (vs 0.5 for musl double version).

// unsigned -> float type-punning (reverse of __raisadev_as_uint).
static __device__ __attribute__((no_vector)) float __raisadev_as_float(
    unsigned __i) {
  union {
    unsigned __i;
    float __f;
  } __u = {__i};
  return __u.__f;
}

// 2^(i/32) for i=0..31, as float32 bit patterns.
static __device__ const unsigned __raisadev_exp2f_tab[32] = {
    0x3f800000, 0x3f82cd87, 0x3f85aac3, 0x3f88980f,
    0x3f8b95c2, 0x3f8ea43a, 0x3f91c3d3, 0x3f94f4f0,
    0x3f9837f0, 0x3f9b8d3a, 0x3f9ef532, 0x3fa27043,
    0x3fa5fed7, 0x3fa9a15b, 0x3fad583f, 0x3fb123f6,
    0x3fb504f3, 0x3fb8fbaf, 0x3fbd08a4, 0x3fc12c4d,
    0x3fc5672a, 0x3fc9b9be, 0x3fce248c, 0x3fd2a81e,
    0x3fd744fd, 0x3fdbfbb8, 0x3fe0ccdf, 0x3fe5b907,
    0x3feac0c7, 0x3fefe4ba, 0x3ff5257d, 0x3ffa83b3,
};

// Polynomial for 2^r on [-1/64, 1/64], from musl exp2f_data.c poly[] → float.
// Coefficients as float bit patterns: C0*r^3 + C1*r^2 + C2*r + 1.
#define __RAISADEV_EXP2F_C0U 0x3d6357c2u
#define __RAISADEV_EXP2F_C1U 0x3e75fe73u
#define __RAISADEV_EXP2F_C2U 0x3f317218u

// 0x1.8p23f = 1.5 * 2^23, used with add/sub to implement rintf(x*N).
#define __RAISADEV_EXP2F_MAGIC 0x1.8p23f

// Cubic polynomial for 2^r on [-1/64, 1/64]: C0*r^3 + C1*r^2 + C2*r + 1.
// Split into separate helper to avoid triggering a backend get.pred.mask
// legalization bug on large no_vector functions.
static __device__ __attribute__((no_vector)) float
__raisadev_exp2f_poly(float __r) {
  float __z = __raisadev_as_float(__RAISADEV_EXP2F_C0U) * __r +
              __raisadev_as_float(__RAISADEV_EXP2F_C1U);
  float __r2 = __r * __r;
  float __y = __raisadev_as_float(__RAISADEV_EXP2F_C2U) * __r + 1.0f;
  return __z * __r2 + __y;
}

// ---- cosf ------------------------------------------------------------------

extern "C" {
static __device__ __attribute__((no_vector)) float cosf(float __x) {
  float __y;
  unsigned __ix, __n, __sign;

  GET_FLOAT_WORD(__ix, __x);
  __sign = __ix >> 31;
  __ix &= 0x7fffffff;

  // |x| <= pi/4
  if (__ix <= 0x3f490fda) {
    if (__ix < 0x39800000) { // |x| < 2^-12
      FORCE_EVAL(__x + 0x1p120f);
      return 1.0f;
    }
    return __raisadev_cosdf(__x);
  }

  // |x| <= 5*pi/4
  if (__ix <= 0x407b53d1) {
    if (__ix > 0x4016cbe3) // |x| > 3*pi/4
      return -__raisadev_cosdf(
          __sign ? __x + __RAISADEV_C2PIO2 : __x - __RAISADEV_C2PIO2);
    else {
      if (__sign)
        return __raisadev_sindf(__x + __RAISADEV_C1PIO2);
      else
        return __raisadev_sindf(__RAISADEV_C1PIO2 - __x);
    }
  }

  // |x| <= 9*pi/4
  if (__ix <= 0x40e231d5) {
    if (__ix > 0x40afeddf) // |x| > 7*pi/4
      return __raisadev_cosdf(
          __sign ? __x + __RAISADEV_C4PIO2 : __x - __RAISADEV_C4PIO2);
    else {
      if (__sign)
        return __raisadev_sindf(-__x - __RAISADEV_C3PIO2);
      else
        return __raisadev_sindf(__x - __RAISADEV_C3PIO2);
    }
  }

  // cos(Inf or NaN) is NaN
  if (__ix >= 0x7f800000)
    return __x - __x;

  // General argument reduction
  __n = __raisadev_rem_pio2f(__x, &__y);
  switch (__n & 3) {
  case 0:
    return __raisadev_cosdf(__y);
  case 1:
    return __raisadev_sindf(-__y);
  case 2:
    return -__raisadev_cosdf(__y);
  default:
    return __raisadev_sindf(__y);
  }
}

// ---- sinf ------------------------------------------------------------------

static __device__ __attribute__((no_vector)) float sinf(float __x) {
  float __y;
  unsigned __ix, __n, __sign;

  GET_FLOAT_WORD(__ix, __x);
  __sign = __ix >> 31;
  __ix &= 0x7fffffff;

  // |x| <= pi/4
  if (__ix <= 0x3f490fda) {
    if (__ix < 0x39800000) { // |x| < 2^-12
      // raise inexact if x!=0 and underflow if subnormal
      FORCE_EVAL(__ix < 0x00800000 ? __x / 0x1p120f : __x + 0x1p120f);
      return __x;
    }
    return __raisadev_sindf(__x);
  }

  // |x| <= 5*pi/4
  if (__ix <= 0x407b53d1) {
    if (__ix <= 0x4016cbe3) { // |x| <= 3*pi/4
      if (__sign)
        return -__raisadev_cosdf(__x + __RAISADEV_C1PIO2);
      else
        return __raisadev_cosdf(__x - __RAISADEV_C1PIO2);
    }
    return __raisadev_sindf(__sign ? -(__x + __RAISADEV_C2PIO2)
                                  : -(__x - __RAISADEV_C2PIO2));
  }

  // |x| <= 9*pi/4
  if (__ix <= 0x40e231d5) {
    if (__ix <= 0x40afeddf) { // |x| <= 7*pi/4
      if (__sign)
        return __raisadev_cosdf(__x + __RAISADEV_C3PIO2);
      else
        return -__raisadev_cosdf(__x - __RAISADEV_C3PIO2);
    }
    return __raisadev_sindf(__sign ? __x + __RAISADEV_C4PIO2
                                  : __x - __RAISADEV_C4PIO2);
  }

  // sin(Inf or NaN) is NaN
  if (__ix >= 0x7f800000)
    return __x - __x;

  // General argument reduction
  __n = __raisadev_rem_pio2f(__x, &__y);
  switch (__n & 3) {
  case 0:
    return __raisadev_sindf(__y);
  case 1:
    return __raisadev_cosdf(__y);
  case 2:
    return __raisadev_sindf(-__y);
  default:
    return -__raisadev_cosdf(__y);
  }
}

// ---- log2f -----------------------------------------------------------------
// x = 2^k * z, z in [OFF, 2*OFF].  A 16-entry table reduces z/c close to 1,
// then polynomial on the small remainder.  Ported from musl src/math/log2f.c.

// 1/ln(2) polynomial coefficients (musl log2f_data.c poly, truncated to float)
#define __RAISADEV_LOG2F_A0 (-0x1.712b70p-2f)
#define __RAISADEV_LOG2F_A1 (0x1.ecabf4p-2f)
#define __RAISADEV_LOG2F_A2 (-0x1.71547ap-1f)
#define __RAISADEV_LOG2F_A3 (0x1.715476p0f)

// Table entries: invc and logc for 16 subintervals (musl log2f_data.c tab)
static __device__ const float __raisadev_log2f_invc[16] = {
    0x1.661ec8p+0f, 0x1.571ed4p+0f, 0x1.49539fp+0f, 0x1.3c995bp+0f,
    0x1.30d191p+0f, 0x1.25e228p+0f, 0x1.1bb4a4p+0f, 0x1.12358fp+0f,
    0x1.0953f4p+0f, 0x1p+0f,       0x1.e608d0p-1f, 0x1.ca4b32p-1f,
    0x1.b20366p-1f, 0x1.9c2d16p-1f, 0x1.886e60p-1f, 0x1.767dcfp-1f,
};
static __device__ const float __raisadev_log2f_logc[16] = {
    -0x1.efec66p-2f, -0x1.b0b683p-2f, -0x1.7418b1p-2f, -0x1.39de92p-2f,
    -0x1.01d9bfp-2f, -0x1.97c1d2p-3f, -0x1.2f9e39p-3f, -0x1.960cbcp-4f,
    -0x1.a6f9dbp-5f, 0x0p0f,       0x1.338caap-4f,  0x1.476a95p-3f,
    0x1.e840b4p-3f,  0x1.40645fp-2f, 0x1.88e9c2p-2f,  0x1.ce0a45p-2f,
};

static __device__ __attribute__((no_vector)) float log2f(float __x) {
  union {
    float __f;
    unsigned __i;
  } __u = {__x};
  unsigned __ix = __u.__i;

  // x < 0: NaN.  x == 0: -inf.
  if ((int)__ix <= 0)
    return __ix == 0 ? -1.0f / 0.0f : (__x - __x) / 0.0f;
  // x is inf or NaN
  if (__ix >= 0x7f800000)
    return __x + __x;

  // Normalize subnormal: scale up and adjust exponent
  if (__ix < 0x00800000) {
    __u.__f = __x * 0x1p23f;
    __ix = __u.__i;
    __ix -= 23 << 23;
  }

  // x = 2^k * z, z in [OFF, 2*OFF] where OFF = 0x1.99ap0 (~1.6)
  unsigned __tmp = __ix - 0x3f330000;
  int __i = (__tmp >> 19) & 15;    // 16 subintervals
  int __k = (int)__tmp >> 23;      // signed arithmetic shift → exponent
  unsigned __iz = __ix - (__tmp & 0xff800000);
  __u.__i = __iz;
  float __z = __u.__f;

  // r = z * invc - 1, |r| <= 1/16
  float __invc = __raisadev_log2f_invc[__i];
  float __logc = __raisadev_log2f_logc[__i];
  float __r = __z * __invc - 1.0f;
  float __y0 = __logc + (float)__k;

  // Polynomial: log2(1+r) ≈ r * P(r)
  float __r2 = __r * __r;
  float __y = __RAISADEV_LOG2F_A1 * __r + __RAISADEV_LOG2F_A2;
  __y = __RAISADEV_LOG2F_A0 * __r2 + __y;
  float __p = __RAISADEV_LOG2F_A3 * __r + __y0;
  __y = __y * __r2 + __p;
  return __y;
}

// ---- exp2f -----------------------------------------------------------------

static __device__ __attribute__((no_vector)) float exp2f(float __x) {
  unsigned __abstop = __raisadev_as_uint(__x) >> 20;
  __abstop &= 0x7ff;
  if (__abstop >= 0x430) {
    if (__raisadev_as_uint(__x) == 0xff800000u)
      return 0.0f;
    if (__abstop >= 0x7f8)
      return __x + __x;
    if (__x > 0.0f)
      return 1.0f / 0.0f;
    if (__x <= -150.0f)
      return 0.0f;
  }
  float __rnd = (__x * 32.0f + __RAISADEV_EXP2F_MAGIC) - __RAISADEV_EXP2F_MAGIC;
  int __k = (int)__rnd;
  float __r = __x - (float)__k * (1.0f / 32.0f);
  float __y = __raisadev_exp2f_poly(__r);
  int __i = __k & 31;
  int __j = (__k - __i) / 32;
  unsigned __t = __raisadev_exp2f_tab[__i];
  int __exp = (int)(__t >> 23) + __j;
  if (__exp > 254)
    return 1.0f / 0.0f;
  if (__exp < 1)
    return 0.0f;
  __t = (__t & 0x807fffffu) | ((unsigned)__exp << 23);
  return __y * __raisadev_as_float(__t);
}
} // extern "C"

#endif // __CLANG_RAISA_LIBDEVICE_H__