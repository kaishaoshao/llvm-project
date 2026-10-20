/*===---- __clang_raisa_ne.h - Neural Engine --------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __CLANG_RAISA_NE_H__
#define __CLANG_RAISA_NE_H__

#include <cstdint>

#define __RAISA_NE_DEVICE_DECL__ __device__ __attribute__((always_inline))
#define __RAISA_NE_HOST_DEVICE_DECL__                                           \
  __host__ __device__ __attribute__((always_inline))

#define NE_TENSOR_MAX_DIM (4)

#define NE_ALIGN(X, A) (((X) + (A) - 1) & ~((A) - 1))

#define NE_ENCODE_PREC(src0, src1, dst, psum)                                  \
  ((static_cast<uint64_t>(src0)) | (static_cast<uint64_t>(src1) << 16) |       \
   (static_cast<uint64_t>(dst) << 32) | (static_cast<uint64_t>(psum) << 48))

#define NE_ENCODE_MKN(m, k, n)                                                 \
  ((static_cast<uint64_t>(m)) | (static_cast<uint64_t>(k) << 16) |             \
   (static_cast<uint64_t>(n) << 32))

namespace raisa {

template <typename Tag, typename Type, uint8_t Align = 1>
struct alignas(Align) ValueType {
  Type value;
  __device__ ValueType() = default;
};

///
/// NE Extended Types
///
struct FP8E4 {};
struct FP8E5 {};
struct MXFP8E4 {};
struct MXFP8E5 {};
struct E8M0 {};
// TO Support
struct FP4 {};
struct MXFP4 {};
struct TF32 {};

using f8e4m3 = ValueType<FP8E4, uint8_t>;
using f8e5m2 = ValueType<FP8E5, uint8_t>;
using mxfp8e4 = ValueType<MXFP8E4, uint8_t>;
using mxfp8e5 = ValueType<MXFP8E5, uint8_t>;
using e8m0 = ValueType<E8M0, int8_t>;
// TO Support
using fp4 = ValueType<FP4, uint8_t>;
using mxfp4 = ValueType<MXFP4, uint8_t>;
using tf32 = ValueType<TF32, float, 4>;

///
/// NE Prec
///
enum {
  NE_PREC_INT8 = 0,
  NE_PREC_FP8E4 = 1,
  NE_PREC_FP8E5 = 2,
  NE_PREC_FFP16 = 3,
  NE_PREC_FBF16 = 4,
  NE_PREC_TF32 = 5,
  NE_PREC_INT32 = 6,
  NE_PREC_FP32 = 7,
  NE_PREC_FP4 = 10,
  NE_PREC_MXFP4 = 11,
  NE_PREC_MXFP8E5 = 12,
  NE_PREC_MXFP8E4 = 13,
  NE_PREC_E8M0 = 14
};

///
/// Static type combination checking for GEMM/CONV(forward, backward_data)
///
template <unsigned a, unsigned b, unsigned d>
static constexpr bool is_valid_ne_prec_combine() {
  if constexpr (a == NE_PREC_INT8) {
    return (b == NE_PREC_INT8) && (d == NE_PREC_FFP16 || d == NE_PREC_FBF16 ||
                                   d == NE_PREC_FP32 || d == NE_PREC_INT32);
  } else if constexpr (a == NE_PREC_FP8E4 || a == NE_PREC_FP8E5) {
    return (b == NE_PREC_FP8E4 || b == NE_PREC_FP8E5 || b == NE_PREC_FP4) &&
           (d == NE_PREC_FFP16 || d == NE_PREC_FBF16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_MXFP8E4 || a == NE_PREC_MXFP8E5) {
    return (b == NE_PREC_MXFP8E4 || b == NE_PREC_MXFP8E5) &&
           (d == NE_PREC_FFP16 || d == NE_PREC_FBF16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_MXFP4) {
    return (b == NE_PREC_MXFP4) &&
           (d == NE_PREC_FFP16 || d == NE_PREC_FBF16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_FBF16) {
    return (b == NE_PREC_FBF16) && (d == NE_PREC_FBF16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_FFP16) {
    return (b == NE_PREC_FFP16) && (d == NE_PREC_FFP16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_TF32) {
    return (b == NE_PREC_TF32) && (d == NE_PREC_FP32);
  }
  return false;
}

template <unsigned a, unsigned b, unsigned c, unsigned d>
static constexpr bool is_valid_ne_prec_combine() {
  return is_valid_ne_prec_combine<a, b, d>() &&
         (c == ((a == NE_PREC_INT8) ? NE_PREC_INT32 : NE_PREC_FP32));
}

template <unsigned a, unsigned b, unsigned c, unsigned d>
static constexpr bool is_valid_conv_bw_prec_combine() {
  if constexpr (a == NE_PREC_FBF16) {
    return (b == NE_PREC_FBF16) && (c == NE_PREC_FP32) &&
           (d == NE_PREC_FBF16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_FFP16) {
    return (b == NE_PREC_FFP16) && (c == NE_PREC_FP32) &&
           (d == NE_PREC_FFP16 || d == NE_PREC_FP32);
  } else if constexpr (a == NE_PREC_TF32) {
    return (b == NE_PREC_TF32) && (c == NE_PREC_FP32) && (d == NE_PREC_FP32);
  }
  return false;
}

///
/// NE Supported Type Info
///
template <typename T> struct ne_type_info;
#define NE_TYPE_INFO(T, P, S)                                                  \
  template <> struct ne_type_info<T> {                                         \
    enum {                                                                     \
      ne_prec = P,                                                             \
      elem_size = S,                                                           \
    };                                                                         \
  };

NE_TYPE_INFO(signed char, NE_PREC_INT8, 8)
NE_TYPE_INFO(f8e4m3, NE_PREC_FP8E4, 8)
NE_TYPE_INFO(f8e5m2, NE_PREC_FP8E5, 8)
NE_TYPE_INFO(_Float16, NE_PREC_FFP16, 16)
NE_TYPE_INFO(__bf16, NE_PREC_FBF16, 16)
NE_TYPE_INFO(tf32, NE_PREC_TF32, 32)
NE_TYPE_INFO(int, NE_PREC_INT32, 32)
NE_TYPE_INFO(float, NE_PREC_FP32, 32)
NE_TYPE_INFO(fp4, NE_PREC_FP4, 4)
NE_TYPE_INFO(mxfp4, NE_PREC_MXFP4, 4)
NE_TYPE_INFO(mxfp8e5, NE_PREC_MXFP8E5, 8)
NE_TYPE_INFO(mxfp8e4, NE_PREC_MXFP8E4, 8)
NE_TYPE_INFO(e8m0, NE_PREC_E8M0, 8)

///
/// Accumulator for GEMM/CONV(forward)
///
template <typename Ta> struct accumulator_type {
  typedef float type;
};
template <> struct accumulator_type<signed char> {
  typedef int type;
};

enum ne_tensor_location_t {
  NE_TENSOR_LOCATION_L0A,
  NE_TENSOR_LOCATION_L0W,
};

///
/// Base alignment requirement for each shared memory type
///
__RAISA_NE_HOST_DEVICE_DECL__
static constexpr uint32_t get_ne_tensor_base_align(ne_tensor_location_t loc) {
  // return (loc == NE_TENSOR_LOCATION_L0A) ? 256 : 512;
  return (loc == NE_TENSOR_LOCATION_L0A) ? 256 : ((loc == NE_TENSOR_LOCATION_L0W) ? 512 : 32);
}

///
/// Calculate the required shared memory size to hold the tensor with given
/// element size.
///
/// tensor_elem_size_in_bits is used to calulate the max alignemnt.
/// elem_size_in_bits is used to calulate the finally aligned size and may
/// equals to tensor_elem_size_in_bits.
///
/// sparse indicates this tensor is sparse or it will be used with a sparse
/// tensor(if so, this tensor will reside in two consecutive memory bank and
/// this function return the size for only one bank)
///
__RAISA_NE_HOST_DEVICE_DECL__ static constexpr uint32_t
get_ne_tensor_aligned_size(const uint32_t (&dim)[NE_TENSOR_MAX_DIM],
                           uint32_t tensor_elem_size_in_bits,
                           ne_tensor_location_t loc, bool sparse,
                           uint32_t elem_size_in_bits) {
  const uint32_t max_c_align = 1024u / tensor_elem_size_in_bits;
  const uint32_t base_align = get_ne_tensor_base_align(loc) << 3;
  uint32_t dims = dim[0] * dim[1] * dim[2];
  if (sparse) {
    const uint32_t aligned_c =
        NE_ALIGN(dim[NE_TENSOR_MAX_DIM - 1], max_c_align);
    return (aligned_c / max_c_align) *
           NE_ALIGN(dims * (max_c_align >> 1) * elem_size_in_bits, base_align);
  } else {
    const uint32_t aligned_c =
        NE_ALIGN(dim[NE_TENSOR_MAX_DIM - 1], max_c_align >> 1);
    return (aligned_c / max_c_align) *
               NE_ALIGN(dims * max_c_align * elem_size_in_bits, base_align) +
           ((aligned_c % max_c_align)
                ? NE_ALIGN(dims * (max_c_align >> 1) * elem_size_in_bits,
                           base_align)
                : 0);
  }
}

///
/// Overloaded function for elem_size_in_bits = tensor_elem_size_in_bits
///
__RAISA_NE_HOST_DEVICE_DECL__
static constexpr uint32_t get_ne_tensor_aligned_elems_size(
    const uint32_t (&dim)[NE_TENSOR_MAX_DIM], uint32_t tensor_elem_size_in_bits,
    ne_tensor_location_t loc, bool sparse = false) {
  return get_ne_tensor_aligned_size(dim, tensor_elem_size_in_bits, loc, sparse,
                                    tensor_elem_size_in_bits);
}

///
/// Calculate the aligned indexes size for a sparse tensor which must reside in
/// NE_TENSOR_LOCATION_L0W
///
/// A single index size if always 2 bits either for 4-2 or 8-4 sparsity
///
__RAISA_NE_HOST_DEVICE_DECL__
static constexpr uint32_t get_ne_sparse_tensor_aligned_indexes_size(
    const uint32_t (&dim)[NE_TENSOR_MAX_DIM],
    uint32_t tensor_elem_size_in_bits) {
  return get_ne_tensor_aligned_size(dim, tensor_elem_size_in_bits,
                                    NE_TENSOR_LOCATION_L0W, true, 2u);
}

///
/// Calculate the aligned scales size for tensor with MXFP8/MXFP4 type
///
/// 32 elements shared the same one scale and scale is always E8M0
///
__RAISA_NE_HOST_DEVICE_DECL__
static constexpr uint32_t
get_ne_tensor_aligned_scales_size(const uint32_t (&dim)[NE_TENSOR_MAX_DIM],
                                  uint32_t tensor_elem_size_in_bits,
                                  ne_tensor_location_t loc) {
  const uint32_t max_c_align = 1024u / tensor_elem_size_in_bits;
  const uint32_t base_align = get_ne_tensor_base_align(loc) << 3;
  const uint32_t max_scale_align = max_c_align / 32u;
  const uint32_t aligned_c =
      NE_ALIGN(dim[NE_TENSOR_MAX_DIM - 1], max_c_align >> 1) / 32u;
  const uint32_t aligned_scale = aligned_c;
  uint32_t dims = dim[0] * dim[1] * dim[2];

  return (aligned_scale / max_scale_align) *
             NE_ALIGN(dims * max_scale_align * 8u, base_align) +
         ((aligned_scale % max_scale_align)
              ? NE_ALIGN(dims * (max_scale_align >> 1) * 8u, base_align)
              : 0);
}

} // namespace raisa

#endif