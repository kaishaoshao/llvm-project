/*===---- __clang_raisa_mma.h - Device-side RAISA mma support --------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __CLANG_RAISA_MMA_H__
#define __CLANG_RAISA_MMA_H__

#include "__clang_raisa_ne.h"

namespace raisa {
namespace mma {

#define MMA_ENCODE_MISC(sp, a_trans, b_trans)                                  \
  ((static_cast<uint64_t>(sp)) | (static_cast<uint64_t>(a_trans) << 1) |       \
   (static_cast<uint64_t>(b_trans) << 2))

#define MAXTRIX_DESC_TYPE_A_B_DENSE(T)                                         \
  template <int M, int N, int K>                                               \
  struct matrix_desc<matrix_a, M, N, K, T>                                     \
      : public matrix_desc_l0a<matrix_a, M, N, K, T, matrix_b_dense> {};       \
  template <int M, int N, int K>                                               \
  struct matrix_desc<matrix_b, M, N, K, T>                                     \
      : public matrix_desc_l0w<matrix_b, M, N, K, T, matrix_b_dense> {};       \
  template <int M, int N, int K>                                               \
  struct matrix_desc<matrix_trans_a, M, N, K, T>                               \
      : public matrix_desc_l0a<matrix_trans_a, M, N, K, T, matrix_b_dense> {}; \
  template <int M, int N, int K>                                               \
  struct matrix_desc<matrix_trans_b, M, N, K, T>                               \
      : public matrix_desc_l0w<matrix_trans_b, M, N, K, T, matrix_b_dense> {};

#define MAXTRIX_DESC_TYPE_A_B(T)                                               \
  MAXTRIX_DESC_TYPE_A_B_DENSE(T)                                               \
  template <int M, int N, int K>                                               \
  struct matrix_desc<matrix_a, M, N, K, T, matrix_b_sparse>                    \
      : public matrix_desc_l0a<matrix_a, M, N, K, T, matrix_b_sparse> {};      \
  template <int M, int N, int K>                                               \
  struct matrix_desc<matrix_b, M, N, K, T, matrix_b_sparse>                    \
      : public matrix_desc_sparse<M, N, K, T> {};

#define MAXTRIX_DESC_TYPE_ACC(T)                                               \
  template <int M, int N, int K>                                               \
  struct matrix_desc<accumulator, M, N, K, T>                                  \
      : public matrix_desc_l0a<accumulator, M, N, K, T, matrix_b_dense> {};

///
/// Matrix Use
///
enum {
  matrix_a = 0,   // K-Major
  matrix_b,       // K-Major
  matrix_trans_a, // M-Major
  matrix_trans_b, // N-Major
  accumulator,    // N-Major
};

///
/// Sparse
///
enum {
  matrix_b_dense = 0,
  matrix_b_sparse,
};

///
/// MMA Op
///
enum {
  op_gemm = 0,
  op_gemm_psum,
  op_mma_first,
  op_mma_first_psum,
  op_mma,
  op_mma_last
};

template <uint32_t elem_size, uint32_t h, uint32_t c, ne_tensor_location_t loc,
          bool trans>
static constexpr uint32_t get_aligned_scales_size() {
  return trans
             ? get_ne_tensor_aligned_size({1, 1, (h + 31) / 32, c}, elem_size,
                                          loc, false, 8u)
             : get_ne_tensor_aligned_scales_size({1, 1, h, c}, elem_size, loc);
}

template <int Use, int M, int N, int K, typename T, int SP>
struct matrix_desc_l0a {
  static constexpr ne_tensor_location_t tensor_location =
      NE_TENSOR_LOCATION_L0A;
  enum {
    elem_size = ne_type_info<T>::elem_size,
    ne_prec = ne_type_info<T>::ne_prec,
  };

  static constexpr uint32_t get_aligned_matrix_size() {
    static_assert(M >= 1 && M <= 2048, "Invalid M");
    static_assert(N >= 1 && N <= 2048, "Invalid N");
    static_assert(K >= 1 && K <= 2048, "Invalid K");

    if constexpr (SP == matrix_b_sparse) {
      // If matrix b is sparse, then the matrix a resides in two consecutive
      // banks splited by K dimension(smem points to the first bank) with same
      // start address.
      //
      // For fp16/bf16:
      // [MK] -> bank0: K'[MK32]   K' = Align(K, 64) / 64
      //         bank1: K'[MK32]   each MK32 aligned to 2048 bits
      //
      // The aligned_elems here refers to the elements in one bank.
      static_assert(Use == matrix_a,
                    "Invalid use for left matrix with a sparse right matrix");

      return get_ne_tensor_aligned_elems_size({1, 1, M, K}, elem_size,
                                              tensor_location, true);
    } else {
      // For fp16/bf16:
      // [MK] -> K'[MK64], K''[MK32]    K'  = Align(K, 32) / 64
      //                                K'' = Align(K, 32) % 64 ? 1 : 0
      // The K dimension here refers to:
      //     K for matrix_a,
      //     M for matrix_trans_a,
      //     N for accumulator
      //
      // For scales of MXFP8:
      // [MK] -> S'[MS4], S''[MS2]
      // [KM] -> S'[KsS128], S''[KsS64]   Ks = (K + 63) / 64
      constexpr uint32_t h = (Use == matrix_a || Use == accumulator) ? M : K;
      constexpr uint32_t c =
          (Use == matrix_a) ? K : ((Use == matrix_trans_a) ? M : N);

      if constexpr (ne_prec == NE_PREC_MXFP4 || ne_prec == NE_PREC_MXFP8E5 ||
                    ne_prec == NE_PREC_MXFP8E4) {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location) +
               get_aligned_scales_size<elem_size, h, c, tensor_location,
                                       Use == matrix_trans_a>();
      } else {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location);
      }
    }
  }

  enum {
    base_align = get_ne_tensor_base_align(tensor_location),
    aligned_size = get_aligned_matrix_size() >> 3,
    aligned_elems = get_aligned_matrix_size() / elem_size,
  };

  __L0Ashared__ T *smem;
};

template <int Use, int M, int N, int K, typename T, int SP>
struct matrix_desc_l0w {
  static constexpr ne_tensor_location_t tensor_location =
      NE_TENSOR_LOCATION_L0W;
  enum {
    elem_size = ne_type_info<T>::elem_size,
    ne_prec = ne_type_info<T>::ne_prec,
  };

  static constexpr uint32_t get_aligned_matrix_size() {
    static_assert(M >= 1 && M <= 2048, "Invalid M");
    static_assert(N >= 1 && N <= 2048, "Invalid N");
    static_assert(K >= 1 && K <= 2048, "Invalid K");

    if constexpr (SP == matrix_b_sparse) {
      // The non-zero elements/sparse index for sparse matrix b reside in two
      // memory with a separate pointer for index(see matrix_desc_sparse)
      //
      // For fp16/bf16 non-zero elements:
      // [NK] -> K'[NK32]   K' = (Align(K, 64) / 2) / 32
      //                    each NK32 aligned to 4096 bits
      static_assert(Use == matrix_b, "Invalid use for right sparse matrix");

      return get_ne_tensor_aligned_elems_size({1, 1, N, K}, elem_size,
                                              tensor_location, true);
    } else {
      // For fp16/bf16:
      // [NK] -> K'[NK64], K''[MK32]    K'  = Align(K, 32) / 64
      //                                K'' = Align(K, 32) % 64 ? 1 : 0
      // The K dimension here refers to:
      //     K for matrix_b,
      //     N for matrix_trans_b,
      //
      // For scales of MXFP8:
      // [NK] -> S'[NS4], S''[NS2]
      // [KN] -> S'[KsS128], S''[KsS64]   Ks = (K + 63) / 64
      static_assert(Use == matrix_b || Use == matrix_trans_b,
                    "Invalid use for right dense matrix");
      constexpr uint32_t h = (Use == matrix_b) ? N : K;
      constexpr uint32_t c = (Use == matrix_b) ? K : N;

      if constexpr (ne_prec == NE_PREC_MXFP4 || ne_prec == NE_PREC_MXFP8E5 ||
                    ne_prec == NE_PREC_MXFP8E4) {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location) +
               get_aligned_scales_size<elem_size, h, c, tensor_location,
                                       Use == matrix_trans_b>();
      } else {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location);
      }
    }
  }

  enum {
    base_align = get_ne_tensor_base_align(tensor_location),
    aligned_size = get_aligned_matrix_size() >> 3,
    aligned_elems = get_aligned_matrix_size() / elem_size,
  };

  __L0Wshared__ T *smem;
};

template <int M, int N, int K, typename T>
struct matrix_desc_sparse
    : public matrix_desc_l0w<matrix_b, M, N, K, T, matrix_b_sparse> {
  using base = matrix_desc_l0w<matrix_b, M, N, K, T, matrix_b_sparse>;
  enum {
    // For fp16/bf16:
    // [NK] -> K'[NK32]   K' = (Align(K, 64) / 2) / 32
    //                    each NK32 aligned to 4096 bits
    //                    each index is 2 bits
    index_size = get_ne_sparse_tensor_aligned_indexes_size({1, 1, N, K},
                                                           base::elem_size),
  };

  __L0Wshared__ void *index_smem;
};

/// An overloaded struct containing a matrix descriptor owned by all threads in
/// the warp.
/// The matrix resides in shared memory and its layout is determined by template
/// arguments
/// 1 <= M/N/K <= 2048
template <int Use, int M, int N, int K, typename T, int SP = matrix_b_dense>
struct matrix_desc;

///
/// All Supported Matrix Type
///
/// A/B: INT8
MAXTRIX_DESC_TYPE_A_B(signed char)
/// A/B: FP8
MAXTRIX_DESC_TYPE_A_B(f8e4m3)
MAXTRIX_DESC_TYPE_A_B(f8e5m2)
/// A/B: MXFP8
MAXTRIX_DESC_TYPE_A_B_DENSE(mxfp8e4)
MAXTRIX_DESC_TYPE_A_B_DENSE(mxfp8e5)
/// A/B: FP4
MAXTRIX_DESC_TYPE_A_B(fp4)
/// A/B: MXFP4
MAXTRIX_DESC_TYPE_A_B_DENSE(mxfp4)
/// A/B: BF16
MAXTRIX_DESC_TYPE_A_B(__bf16)
/// A/B: FP16
MAXTRIX_DESC_TYPE_A_B(_Float16)
/// A/B: TF32
MAXTRIX_DESC_TYPE_A_B_DENSE(tf32)
/// C/D Types
MAXTRIX_DESC_TYPE_ACC(__bf16)
MAXTRIX_DESC_TYPE_ACC(_Float16)
MAXTRIX_DESC_TYPE_ACC(int)
MAXTRIX_DESC_TYPE_ACC(float)

/// @brief Dense matrix loading from GMEM to L0W(through L1) or L0A where the
/// matrix layout meets the requirement of MMA instructions.
/// @param desc The matrix tile shape, layout(row/col major)
/// @param src  The matrix address in GMEM
/// @param ldm  The major dimension stride in elments, must be the multiple of
///             64 bytes(hardware restriction)
/// @param crd0 The top-left coordinate of the matrix tile is (crd0, crd1)
/// @param crd1
/// @param l1_temp Used for matrix_b loading, GMEM -> L1 -> L0W
template <int Use, int M, int N, int K, typename T>
__RAISA_NE_DEVICE_DECL__ static void
load_matrix_async(const matrix_desc<Use, M, N, K, T> &desc, const T *src,
                  unsigned ldm, unsigned crd0 = 0, unsigned crd1 = 0,
                  __L1shared__ T *l1_temp = nullptr) {
  using m_type = matrix_desc<Use, M, N, K, T>;

  constexpr uint32_t h =
      (Use == matrix_a || Use == accumulator) ? M : ((Use == matrix_b) ? N : K);
  constexpr uint32_t c = (Use == matrix_a || Use == matrix_b)
                             ? K
                             : ((Use == matrix_trans_a) ? M : N);
  constexpr bool use_l1_temp = Use == matrix_b || Use == matrix_trans_b;

  constexpr uint32_t elem_size = m_type::elem_size;
  constexpr uint32_t max_c_align = 1024u / elem_size;
  constexpr uint32_t aligned_c = NE_ALIGN(c, max_c_align >> 1);
  constexpr uint32_t num_batch = aligned_c / max_c_align;
  uint64_t offset_in_elems = (Use == matrix_trans_a || Use == matrix_trans_b)
                                 ? (crd1 * ldm + crd0)
                                 : (crd0 * ldm + crd1);

  uint64_t src_addr =
      reinterpret_cast<uint64_t>(src) + ((offset_in_elems * elem_size) >> 3);
  uint64_t dst_addr = reinterpret_cast<uint64_t>(desc.smem);
  if constexpr (use_l1_temp) {
    dst_addr = reinterpret_cast<uint64_t>(l1_temp);
  }

  // Load first part
  if constexpr (num_batch) {
    constexpr uint32_t segment_size = (max_c_align * elem_size) >> 3;
    static_assert((num_batch == 1) ||
                      (((segment_size * h) % m_type::base_align) == 0),
                  "TODO: Using DteGatherScatterG2L");

    constexpr uint64_t itr0 = h;
    uint64_t stride0 = (ldm * elem_size) >> 3;
    constexpr uint64_t itr1 = num_batch;
    constexpr uint64_t stride1 = segment_size;
    constexpr uint64_t data_len = segment_size * itr0 * itr1;

    // FIXME: using h * stride0 to describe src length is not accurate
    __builtin_riscv_dte_set_length(data_len, 0, h * stride0);
    __builtin_riscv_dte_set_rdma_src_itera_stride(1, itr1 | (stride1 << 32),
                                                  itr0 | (stride0 << 32));
    __builtin_riscv_dte_set_rdma_misc(0, data_len, segment_size);
    __builtin_riscv_dte_gather(dst_addr, 0, src_addr);

    src_addr += num_batch * segment_size;
    dst_addr += data_len;
  }

  // Load second part
  if constexpr (aligned_c % max_c_align) {
    constexpr uint32_t segment_size = ((max_c_align >> 1) * elem_size) >> 3;
    constexpr uint64_t itr0 = h;
    uint64_t stride0 = (ldm * elem_size) >> 3;
    constexpr uint64_t data_len = segment_size * itr0;

    // FIXME: using h * stride0 to describe src length is not accurate
    __builtin_riscv_dte_set_length(data_len, 0, h * stride0);
    __builtin_riscv_dte_set_rdma_src_itera_stride(1, 1, itr0 | (stride0 << 32));
    __builtin_riscv_dte_set_rdma_misc(0, data_len, segment_size);
    __builtin_riscv_dte_gather(dst_addr, 0, src_addr);
  }

  // L1 -> L0W
  if constexpr (use_l1_temp) {
    __builtin_riscv_lsu_set_length(m_type::aligned_size, 0,
                                   m_type::aligned_size);
    __builtin_riscv_lsu_rdma_mem_copy(
        reinterpret_cast<uint64_t>(desc.smem),
        reinterpret_cast<uint64_t>(l1_temp) |
            (static_cast<uint64_t>(m_type::aligned_size) << 40u));
  }
}

/// @brief Dense matrix(N-Major) store from L0A to GMEM
/// @param desc The matrix tile shape
/// @param dst  The matrix address in GMEM
/// @param ldm  The major dimension stride in elments, must be the multiple of
///             64 bytes(hardware restriction)
/// @param crd0 The top-left coordinate of the matrix tile is (crd0, crd1)
/// @param crd1
template <int M, int N, int K, typename T>
__RAISA_NE_DEVICE_DECL__ static void
store_matrix_async(const matrix_desc<accumulator, M, N, K, T> &desc, T *dst,
                   unsigned ldm, unsigned crd0 = 0, unsigned crd1 = 0) {
  using m_type = matrix_desc<accumulator, M, N, K, T>;
  constexpr uint32_t elem_size = m_type::elem_size;
  constexpr uint32_t h = M;
  constexpr uint32_t c = N;
  constexpr uint32_t max_c_align = 1024u / elem_size;
  constexpr uint32_t aligned_c = NE_ALIGN(c, max_c_align >> 1);
  constexpr uint32_t num_batch = aligned_c / max_c_align;

  uint64_t src_addr = reinterpret_cast<uint64_t>(desc.smem);
  uint64_t dst_addr = reinterpret_cast<uint64_t>(dst) +
                      (((crd0 * ldm + crd1) * elem_size) >> 3);
  if constexpr (num_batch) {
    constexpr uint32_t segment_size = (max_c_align * elem_size) >> 3;
    static_assert((num_batch == 1) ||
                      (((segment_size * h) % m_type::base_align) == 0),
                  "TODO: align each batch");

    constexpr uint64_t itr0 = h;
    uint64_t stride0 = (ldm * elem_size) >> 3;
    constexpr uint64_t itr1 = num_batch;
    constexpr uint64_t stride1 = segment_size;
    constexpr uint64_t data_len = segment_size * itr0 * itr1;

    __builtin_riscv_dte_set_length(h * ldm, 0, data_len);
    __builtin_riscv_dte_set_wdma_dst_itera_stride(1, itr1 | (stride1 << 32),
                                                  itr0 | (stride0 << 32));
    __builtin_riscv_dte_set_wdma_misc(0, data_len, segment_size);
    __builtin_riscv_dte_scatter(dst_addr, 0, src_addr);

    src_addr += data_len;
    dst_addr += num_batch * segment_size;
  }

  if constexpr (aligned_c % max_c_align) {
    constexpr uint32_t segment_size =
        ((c - num_batch * max_c_align) * elem_size) >> 3;
    constexpr uint64_t itr0 = h;
    uint64_t dst_stride0 = (ldm * elem_size) >> 3;
    uint64_t src_stride0 = ((max_c_align >> 1) * elem_size) >> 3;
    uint64_t src_data_len = src_stride0 * itr0;

    __builtin_riscv_dte_set_length(h * ldm, 0, src_data_len);
    __builtin_riscv_dte_set_wdma_src_itera_stride(1, 1,
                                                  itr0 | (src_stride0 << 32));
    __builtin_riscv_dte_set_wdma_dst_itera_stride(1, 1,
                                                  itr0 | (dst_stride0 << 32));

    __builtin_riscv_dte_set_wdma_misc(0, src_data_len, segment_size);
    __builtin_riscv_dte_gather_scatter_l2g(dst_addr, 0, src_addr);
  }
}

/// @brief Load MXFP8/MXFP4 scale data from GMEM to the SMEM scale area.
///
/// For MXFP8/MXFP4 formats, SMEM is partitioned as [data area][scale area].
/// load_matrix_async only fills the data area; this function loads the E8M0
/// scales into the reserved scale area using a 1-D contiguous DTE gather.
///
/// For matrix_a: scales go to L0A SMEM at desc->smem + data_aligned_size.
/// For matrix_b: scales go to L1 SMEM at l1_temp + data_aligned_size_l1,
///               then load_matrix_async copies L1->L0W together with data.
///
/// @param desc         The matrix tile descriptor
/// @param src          Source E8M0 scale buffer in GMEM (already offset by
/// crd0)
/// @param actual_rows  Number of scale rows to load (0 = use tile height h).
///                     Used for tail tiles where actual rows < tile height.
/// @param l1_temp      L1 shared memory buffer (required for matrix_b)
template <int Use, int M, int N, int K, typename T>
__RAISA_NE_DEVICE_DECL__ static void
load_mx_scales_async(const matrix_desc<Use, M, N, K, T> &desc, const e8m0 *src,
                     unsigned actual_rows = 0,
                     __L1shared__ e8m0 *l1_temp = nullptr) {
  using m_type = matrix_desc<Use, M, N, K, T>;
  static_assert(m_type::ne_prec == NE_PREC_MXFP8E4 ||
                    m_type::ne_prec == NE_PREC_MXFP8E5 ||
                    m_type::ne_prec == NE_PREC_MXFP4,
                "load_mx_scales_async is only valid for MX types");

  constexpr uint32_t h =
      (Use == matrix_a || Use == accumulator) ? M : ((Use == matrix_b) ? N : K);
  constexpr uint32_t c = (Use == matrix_a || Use == matrix_b)
                             ? K
                             : ((Use == matrix_trans_a) ? M : N);
  constexpr uint32_t es = m_type::elem_size; // bits per element
  constexpr uint32_t scale_cols = c / 32;    // each E8M0 covers 32 elements
  constexpr bool use_l1 = (Use == matrix_b || Use == matrix_trans_b);

  // Compute data area size in bytes to locate the scale area in SMEM.
  // For MX types, elem_size is 8 bits (1 byte), so max_c_align = 1024/8 = 128.
  // L0A base alignment = 256 bytes; L1 = 32 bytes.
  constexpr uint32_t max_c = 1024u / es;  // elements per batch (128 for 1-byte)
  constexpr uint32_t tail_c = max_c >> 1; // 64
  constexpr uint32_t alc = NE_ALIGN(c, tail_c);
  constexpr uint32_t nb = alc / max_c;
  constexpr uint32_t pad = use_l1 ? 32u : 256u; // byte alignment
  constexpr uint32_t data_bytes =
      (nb * NE_ALIGN(h * max_c * (es >> 3), pad) +
       (alc % max_c ? NE_ALIGN(h * tail_c * (es >> 3), pad) : 0));

  uint64_t dst_addr;
  if constexpr (use_l1) {
    dst_addr = reinterpret_cast<uint64_t>(l1_temp) + data_bytes;
  } else {
    dst_addr = reinterpret_cast<uint64_t>(desc.smem) + data_bytes;
  }

  unsigned rows = (actual_rows == 0) ? h : actual_rows;
  unsigned total_bytes = rows * scale_cols;
  uint64_t src_addr = reinterpret_cast<uint64_t>(src);

  __builtin_riscv_dte_set_length(total_bytes, 0, total_bytes);
  __builtin_riscv_dte_set_rdma_src_itera_stride(
      1, 1, 1ULL | ((uint64_t)total_bytes << 32));
  __builtin_riscv_dte_set_rdma_misc(0, total_bytes, total_bytes);
  __builtin_riscv_dte_gather(dst_addr, 0, src_addr);
}

///
/// MMA Functions
///
/// D = A * B + C
template <int Op = op_gemm_psum, int AUse, int BUse, int M, int N, int K,
          typename Ta, typename Tb, typename Tc, typename Td, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_async(const matrix_desc<accumulator, M, N, K, Td> &d,
          const matrix_desc<AUse, M, N, K, Ta, SP> &a,
          const matrix_desc<BUse, M, N, K, Tb, SP> &b,
          const matrix_desc<accumulator, M, N, K, Tc> &c) {
  using a_type = matrix_desc<AUse, M, N, K, Ta, SP>;
  using b_type = matrix_desc<BUse, M, N, K, Tb, SP>;
  using c_type = matrix_desc<accumulator, M, N, K, Tc>;
  using d_type = matrix_desc<accumulator, M, N, K, Td>;

  static_assert(AUse == matrix_a || AUse == matrix_trans_a,
                "Invalid use for left matrix");
  static_assert(BUse == matrix_b || BUse == matrix_trans_b,
                "Invalid use for right matrix");
  static_assert(is_valid_ne_prec_combine<a_type::ne_prec, b_type::ne_prec,
                                         c_type::ne_prec, d_type::ne_prec>(),
                "Invalid matrix type combination");

  constexpr uint64_t prec = NE_ENCODE_PREC(a_type::ne_prec, b_type::ne_prec,
                                           d_type::ne_prec, c_type::ne_prec);
  constexpr uint64_t misc = MMA_ENCODE_MISC((SP == matrix_b_sparse) ? 1 : 0,
                                            (AUse == matrix_trans_a) ? 1 : 0,
                                            (BUse == matrix_trans_b) ? 1 : 0);
  constexpr uint64_t mkn = NE_ENCODE_MKN(M, K, N);

  uint64_t sparse_index_size = 0;
  uint64_t param1 = 0;
  if constexpr (SP == matrix_b_sparse) {
    sparse_index_size = b_type::index_size;
    param1 = reinterpret_cast<uint64_t>(b.index_smem);
  }

  // Config
  __builtin_riscv_ne_set_prec(0, 0, prec);
  __builtin_riscv_ne_set_misc(0, 0, misc);
  __builtin_riscv_ne_set_mkn(0, 0, mkn);
  __builtin_riscv_ne_set_length(
      d_type::aligned_size,
      static_cast<uint64_t>(c_type::aligned_size) | (sparse_index_size << 32),
      static_cast<uint64_t>(a_type::aligned_size) |
          (static_cast<uint64_t>(b_type::aligned_size) << 32));

  uint64_t param0 = (reinterpret_cast<uint64_t>(a.smem) >> 1) |
                    ((reinterpret_cast<uint64_t>(b.smem) >> 1) << 32);
  if constexpr (Op == op_gemm) {
    // d = a * b
    __builtin_riscv_gemm((reinterpret_cast<uint64_t>(d.smem) >> 1), param1,
                         param0);
  } else if constexpr (Op == op_gemm_psum) {
    // d = a * b + c
    __builtin_riscv_gemm_psum(
        (reinterpret_cast<uint64_t>(d.smem) >> 1) |
            ((reinterpret_cast<uint64_t>(c.smem) >> 1) << 32),
        param1, param0);
  } else {
    static_assert(M >= 1 && M <= 64, "Invalid M for small shape mma");
    static_assert(N >= 1 && N <= 64, "Invalid N for small shape mma");
    if constexpr (Op == op_mma_first) {
      // temp = a * b
      __builtin_riscv_mma_first(0, param1, param0);
    } else if constexpr (Op == op_mma_first_psum) {
      // temp = a * b + c
      __builtin_riscv_mma_first_psum(
          (reinterpret_cast<uint64_t>(c.smem) >> 1) << 32, param1, param0);
    } else if constexpr (Op == op_mma) {
      // temp = a * b + temp
      __builtin_riscv_mma(0, param1, param0);
    } else {
      // d = a * b + temp
      static_assert(Op == op_mma_last, "Invalid Op");
      __builtin_riscv_mma_last((reinterpret_cast<uint64_t>(d.smem) >> 1),
                               param1, param0);
    }
  }
}

/// D = A * B
template <int Op = op_gemm, int AUse, int BUse, int M, int N, int K,
          typename Ta, typename Tb, typename Td, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_async(const matrix_desc<accumulator, M, N, K, Td> &d,
          const matrix_desc<AUse, M, N, K, Ta, SP> &a,
          const matrix_desc<BUse, M, N, K, Tb, SP> &b) {
  matrix_desc<accumulator, M, N, K, typename accumulator_type<Ta>::type>
      temp_c = {nullptr};
  mma_async<Op>(d, a, b, temp_c);
}

///
/// Small Shape MMA Functions With M/N <= 64
///
/// The Dst/Acc matrix type must be INT32 or FLOAT(depends on Ta)
///
/// T = A * B
template <int AUse, int BUse, int M, int N, int K, typename Ta, typename Tb,
          int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_first_async(const matrix_desc<AUse, M, N, K, Ta, SP> &a,
                const matrix_desc<BUse, M, N, K, Tb, SP> &b) {
  matrix_desc<accumulator, M, N, K, typename accumulator_type<Ta>::type>
      temp_d = {nullptr};
  mma_async<op_mma_first>(temp_d, a, b);
}

/// T = A * B + C
template <int AUse, int BUse, int M, int N, int K, typename Ta, typename Tb,
          typename Tc, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_first_async(const matrix_desc<AUse, M, N, K, Ta, SP> &a,
                const matrix_desc<BUse, M, N, K, Tb, SP> &b,
                const matrix_desc<accumulator, M, N, K, Tc> &c) {
  matrix_desc<accumulator, M, N, K, typename accumulator_type<Ta>::type>
      temp_d = {nullptr};
  mma_async<op_mma_first_psum>(temp_d, a, b, c);
}

/// T = A * B + T
template <int AUse, int BUse, int M, int N, int K, typename Ta, typename Tb,
          int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_acc_async(const matrix_desc<AUse, M, N, K, Ta, SP> &a,
              const matrix_desc<BUse, M, N, K, Tb, SP> &b) {
  matrix_desc<accumulator, M, N, K, typename accumulator_type<Ta>::type>
      temp_d = {nullptr};
  mma_async<op_mma>(temp_d, a, b);
}

/// D = A * B + T
template <int AUse, int BUse, int M, int N, int K, typename Ta, typename Tb,
          typename Td, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_last_async(const matrix_desc<accumulator, M, N, K, Td> &d,
               const matrix_desc<AUse, M, N, K, Ta, SP> &a,
               const matrix_desc<BUse, M, N, K, Tb, SP> &b) {
  mma_async<op_mma_last>(d, a, b);
}

/****************************************************************/
// Following are dynamic version of mma apis and structs
/****************************************************************/

#define MAXTRIX_DESC_DYN_TYPE_A_B_DENSE(T)                                     \
  template <>                                                                  \
  struct matrix_desc_dyn<matrix_a, T>                                          \
      : public matrix_desc_dyn_l0a<matrix_a, T, matrix_b_dense> {};            \
  template <>                                                                  \
  struct matrix_desc_dyn<matrix_b, T>                                          \
      : public matrix_desc_dyn_l0w<matrix_b, T, matrix_b_dense> {};            \
  template <>                                                                  \
  struct matrix_desc_dyn<matrix_trans_a, T>                                    \
      : public matrix_desc_dyn_l0a<matrix_trans_a, T, matrix_b_dense> {};      \
  template <>                                                                  \
  struct matrix_desc_dyn<matrix_trans_b, T>                                    \
      : public matrix_desc_dyn_l0w<matrix_trans_b, T, matrix_b_dense> {};

#define MAXTRIX_DESC_DYN_TYPE_A_B(T)                                           \
  MAXTRIX_DESC_DYN_TYPE_A_B_DENSE(T)                                           \
  template <>                                                                  \
  struct matrix_desc_dyn<matrix_a, T, matrix_b_sparse>                         \
      : public matrix_desc_dyn_l0a<matrix_a, T, matrix_b_sparse> {};           \
  template <>                                                                  \
  struct matrix_desc_dyn<matrix_b, T, matrix_b_sparse>                         \
      : public matrix_desc_dyn_sparse<T> {};

#define MAXTRIX_DESC_DYN_TYPE_ACC(T)                                           \
  template <>                                                                  \
  struct matrix_desc_dyn<accumulator, T>                                       \
      : public matrix_desc_dyn_l0a<accumulator, T, matrix_b_dense> {};

template <uint32_t elem_size, ne_tensor_location_t loc, bool trans>
static constexpr uint32_t get_aligned_scales_size(uint32_t h, uint32_t c) {
  return trans
             ? get_ne_tensor_aligned_size({1, 1, (h + 31) / 32, c}, elem_size,
                                          loc, false, 8u)
             : get_ne_tensor_aligned_scales_size({1, 1, h, c}, elem_size, loc);
}

template <int Use, typename T, int SP> struct matrix_desc_dyn_l0a {
  static constexpr ne_tensor_location_t tensor_location =
      NE_TENSOR_LOCATION_L0A;
  enum {
    elem_size = ne_type_info<T>::elem_size,
    ne_prec = ne_type_info<T>::ne_prec,
  };

  static constexpr uint32_t get_aligned_matrix_size(uint32_t M, uint32_t N,
                                                    uint32_t K) {
    if constexpr (SP == matrix_b_sparse) {
      // If matrix b is sparse, then the matrix a resides in two consecutive
      // banks splited by K dimension(smem points to the first bank) with same
      // start address.
      //
      // For fp16/bf16:
      // [MK] -> bank0: K'[MK32]   K' = Align(K, 64) / 64
      //         bank1: K'[MK32]   each MK32 aligned to 2048 bits
      //
      // The aligned_elems here refers to the elements in one bank.
      static_assert(Use == matrix_a,
                    "Invalid use for left matrix with a sparse right matrix");

      return get_ne_tensor_aligned_elems_size({1, 1, M, K}, elem_size,
                                              tensor_location, true);
    } else {
      // For fp16/bf16:
      // [MK] -> K'[MK64], K''[MK32]    K'  = Align(K, 32) / 64
      //                                K'' = Align(K, 32) % 64 ? 1 : 0
      // The K dimension here refers to:
      //     K for matrix_a,
      //     M for matrix_trans_a,
      //     N for accumulator
      //
      // For scales of MXFP8:
      // [MK] -> S'[MS4], S''[MS2]
      // [KM] -> S'[KsS128], S''[KsS64]   Ks = (K + 63) / 64
      uint32_t h = (Use == matrix_a || Use == accumulator) ? M : K;
      uint32_t c =
          (Use == matrix_a) ? K : ((Use == matrix_trans_a) ? M : N);

      if constexpr (ne_prec == NE_PREC_MXFP4 || ne_prec == NE_PREC_MXFP8E5 ||
                    ne_prec == NE_PREC_MXFP8E4) {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location) +
               get_aligned_scales_size<elem_size, tensor_location,
                                       Use == matrix_trans_a>(h, c);
      } else {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location);
      }
    }
  }

  constexpr static uint32_t get_aligned_size(uint32_t M, uint32_t N,
                                             uint32_t K) {
    return get_aligned_matrix_size(M, N, K) >> 3;
  }

  constexpr static uint32_t get_aligned_elems(uint32_t M, uint32_t N,
                                              uint32_t K) {
    return get_aligned_matrix_size(M, N, K) / elem_size;
  }

  enum {
    base_align = get_ne_tensor_base_align(tensor_location),
  };

  __L0Ashared__ T *smem;
};

template <int Use, typename T, int SP> struct matrix_desc_dyn_l0w {
  static constexpr ne_tensor_location_t tensor_location =
      NE_TENSOR_LOCATION_L0A;
  enum {
    elem_size = ne_type_info<T>::elem_size,
    ne_prec = ne_type_info<T>::ne_prec,
  };

  static constexpr uint32_t get_aligned_matrix_size(uint32_t M, uint32_t N,
                                                    uint32_t K) {
    if constexpr (SP == matrix_b_sparse) {
      // If matrix b is sparse, then the matrix a resides in two consecutive
      // banks splited by K dimension(smem points to the first bank) with same
      // start address.
      //
      // For fp16/bf16:
      // [MK] -> bank0: K'[MK32]   K' = Align(K, 64) / 64
      //         bank1: K'[MK32]   each MK32 aligned to 2048 bits
      //
      // The aligned_elems here refers to the elements in one bank.
      static_assert(Use == matrix_b,
                    "Invalid use for left matrix with a sparse right matrix");

      return get_ne_tensor_aligned_elems_size({1, 1, M, K}, elem_size,
                                              tensor_location, true);
    } else {
      // For fp16/bf16:
      // [MK] -> K'[MK64], K''[MK32]    K'  = Align(K, 32) / 64
      //                                K'' = Align(K, 32) % 64 ? 1 : 0
      // The K dimension here refers to:
      //     K for matrix_a,
      //     M for matrix_trans_a,
      //     N for accumulator
      //
      // For scales of MXFP8:
      // [MK] -> S'[MS4], S''[MS2]
      // [KM] -> S'[KsS128], S''[KsS64]   Ks = (K + 63) / 64
      static_assert(Use == matrix_b || Use == matrix_trans_b,
                    "Invalid use for right dense matrix");
      uint32_t h = (Use == matrix_a || Use == accumulator) ? M : K;
      uint32_t c = (Use == matrix_a) ? K : ((Use == matrix_trans_a) ? M : N);

      if constexpr (ne_prec == NE_PREC_MXFP4 || ne_prec == NE_PREC_MXFP8E5 ||
                    ne_prec == NE_PREC_MXFP8E4) {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location) +
               get_aligned_scales_size<elem_size, tensor_location,
                                       Use == matrix_trans_a>(h, c);
      } else {
        return get_ne_tensor_aligned_elems_size({1, 1, h, c}, elem_size,
                                                tensor_location);
      }
    }
  }

  constexpr static uint32_t get_aligned_size(uint32_t M, uint32_t N,
                                             uint32_t K) {
    return get_aligned_matrix_size(M, N, K) >> 3;
  }

  constexpr static uint32_t get_aligned_elems(uint32_t M, uint32_t N,
                                              uint32_t K) {
    return get_aligned_matrix_size(M, N, K) / elem_size;
  }
  enum {
    base_align = get_ne_tensor_base_align(tensor_location),
  };
  __L0Wshared__ T *smem;
};

template <typename T>
struct matrix_desc_dyn_sparse
    : public matrix_desc_dyn_l0w<matrix_b, T, matrix_b_sparse> {
  using base = matrix_desc_dyn_l0w<matrix_b, T, matrix_b_sparse>;

  static constexpr uint32_t get_index_size(uint32_t M, uint32_t N, uint32_t K) {
    return get_ne_sparse_tensor_aligned_indexes_size({1, 1, N, K},
                                                     base::elem_size);
  }

  __L0Wshared__ void *index_smem;
};

/// An overloaded struct containing a matrix descriptor owned by all threads in
/// the warp.
/// The matrix resides in shared memory and its layout is determined by template
/// arguments
/// 1 <= M/N/K <= 2048
template <int Use, typename T, int SP = matrix_b_dense> struct matrix_desc_dyn;

///
/// All Supported Matrix Type
///
/// A/B: INT8
MAXTRIX_DESC_DYN_TYPE_A_B(signed char)
/// A/B: FP8
MAXTRIX_DESC_DYN_TYPE_A_B(f8e4m3)
MAXTRIX_DESC_DYN_TYPE_A_B(f8e5m2)
/// A/B: MXFP8
MAXTRIX_DESC_DYN_TYPE_A_B_DENSE(mxfp8e4)
MAXTRIX_DESC_DYN_TYPE_A_B_DENSE(mxfp8e5)
/// A/B: FP4
MAXTRIX_DESC_DYN_TYPE_A_B(fp4)
/// A/B: MXFP4
MAXTRIX_DESC_DYN_TYPE_A_B_DENSE(mxfp4)
/// A/B: BF16
MAXTRIX_DESC_DYN_TYPE_A_B(__bf16)
/// A/B: FP16
MAXTRIX_DESC_DYN_TYPE_A_B(_Float16)
/// A/B: TF32
MAXTRIX_DESC_DYN_TYPE_A_B_DENSE(tf32)
/// C/D Types
MAXTRIX_DESC_DYN_TYPE_ACC(__bf16)
MAXTRIX_DESC_DYN_TYPE_ACC(_Float16)
MAXTRIX_DESC_DYN_TYPE_ACC(int)
MAXTRIX_DESC_DYN_TYPE_ACC(float)

/// @brief Dense matrix loading from GMEM to L0W(through L1) or L0A where the
/// matrix layout meets the requirement of MMA instructions.
/// @param desc The matrix tile shape, layout(row/col major)
/// @param src  The matrix address in GMEM
/// @param ldm  The major dimension stride in elments, must be the multiple of
///             64 bytes(hardware restriction)
/// @param crd0 The top-left coordinate of the matrix tile is (crd0, crd1)
/// @param crd1
/// @param l1_temp Used for matrix_b loading, GMEM -> L1 -> L0W
template <int Use, typename T>
__RAISA_NE_DEVICE_DECL__ static void
load_matrix_async_dyn(const matrix_desc_dyn<Use, T> &desc, const T *src,
                      unsigned ldm, uint32_t M, uint32_t N, uint32_t K,
                      unsigned crd0 = 0, unsigned crd1 = 0,
                      __L1shared__ T *l1_temp = nullptr) {
  using m_type = matrix_desc_dyn<Use, T>;

  uint32_t h =
      (Use == matrix_a || Use == accumulator) ? M : ((Use == matrix_b) ? N : K);
  uint32_t c = (Use == matrix_a || Use == matrix_b)
                   ? K
                   : ((Use == matrix_trans_a) ? M : N);
  constexpr bool use_l1_temp = Use == matrix_b || Use == matrix_trans_b;

  constexpr uint32_t elem_size = m_type::elem_size;
  constexpr uint32_t max_c_align = 1024u / elem_size;
  uint32_t aligned_c = NE_ALIGN(c, max_c_align >> 1);
  uint32_t num_batch = aligned_c / max_c_align;
  uint64_t offset_in_elems = (Use == matrix_trans_a || Use == matrix_trans_b)
                                 ? (crd1 * ldm + crd0)
                                 : (crd0 * ldm + crd1);

  uint64_t src_addr =
      reinterpret_cast<uint64_t>(src) + ((offset_in_elems * elem_size) >> 3);
  uint64_t dst_addr = reinterpret_cast<uint64_t>(desc.smem);
  if constexpr (use_l1_temp) {
    dst_addr = reinterpret_cast<uint64_t>(l1_temp);
  }

  // Load first part
  if (num_batch) {
    constexpr uint32_t segment_size = (max_c_align * elem_size) >> 3;
    // static_assert((num_batch == 1) ||
    //                   (((segment_size * h) % m_type::base_align) == 0),
    //               "TODO: Using DteGatherScatterG2L");

    uint64_t itr0 = h;
    uint64_t stride0 = (ldm * elem_size) >> 3;
    uint64_t itr1 = num_batch;
    constexpr uint64_t stride1 = segment_size;
    uint64_t data_len = segment_size * itr0 * itr1;

    // FIXME: using h * stride0 to describe src length is not accurate
    __builtin_riscv_dte_set_length(data_len, 0, h * stride0);
    __builtin_riscv_dte_set_rdma_src_itera_stride(1, itr1 | (stride1 << 32),
                                                  itr0 | (stride0 << 32));
    __builtin_riscv_dte_set_rdma_misc(0, data_len, segment_size);
    __builtin_riscv_dte_gather(dst_addr, 0, src_addr);

    src_addr += num_batch * segment_size;
    dst_addr += data_len;
  }

  // Load second part
  if (aligned_c % max_c_align) {
    constexpr uint32_t segment_size = ((max_c_align >> 1) * elem_size) >> 3;
    uint64_t itr0 = h;
    uint64_t stride0 = (ldm * elem_size) >> 3;
    uint64_t data_len = segment_size * itr0;

    // FIXME: using h * stride0 to describe src length is not accurate
    __builtin_riscv_dte_set_length(data_len, 0, h * stride0);
    __builtin_riscv_dte_set_rdma_src_itera_stride(1, 1, itr0 | (stride0 << 32));
    __builtin_riscv_dte_set_rdma_misc(0, data_len, segment_size);
    __builtin_riscv_dte_gather(dst_addr, 0, src_addr);
  }

  // L1 -> L0W
  if constexpr (use_l1_temp) {
    __builtin_riscv_lsu_set_length(m_type::get_aligned_size(M, N, K), 0,
                                   m_type::get_aligned_size(M, N, K));
    __builtin_riscv_lsu_rdma_mem_copy(
        reinterpret_cast<uint64_t>(desc.smem),
        reinterpret_cast<uint64_t>(l1_temp) |
            (static_cast<uint64_t>(m_type::get_aligned_size(M, N, K)) << 40u));
  }
}

/// @brief Dense matrix(N-Major) store from L0A to GMEM
/// @param desc The matrix tile shape
/// @param dst  The matrix address in GMEM
/// @param ldm  The major dimension stride in elments, must be the multiple of
///             64 bytes(hardware restriction)
/// @param crd0 The top-left coordinate of the matrix tile is (crd0, crd1)
/// @param crd1
template <typename T>
__RAISA_NE_DEVICE_DECL__ static void
store_matrix_async_dyn(const matrix_desc_dyn<accumulator, T> &desc, T *dst,
                       unsigned ldm, uint32_t M, uint32_t N, uint32_t K,
                       unsigned crd0 = 0, unsigned crd1 = 0) {
  using m_type = matrix_desc_dyn<accumulator, T>;
  constexpr uint32_t elem_size = m_type::elem_size;
  uint32_t h = M;
  uint32_t c = N;
  constexpr uint32_t max_c_align = 1024u / elem_size;
  uint32_t aligned_c = NE_ALIGN(c, max_c_align >> 1);
  uint32_t num_batch = aligned_c / max_c_align;

  uint64_t src_addr = reinterpret_cast<uint64_t>(desc.smem);
  uint64_t dst_addr = reinterpret_cast<uint64_t>(dst) +
                      (((crd0 * ldm + crd1) * elem_size) >> 3);
  if (num_batch) {
    constexpr uint32_t segment_size = (max_c_align * elem_size) >> 3;
    // static_assert((num_batch == 1) ||
    //                   (((segment_size * h) % m_type::base_align) == 0),
    //               "TODO: align each batch");

    uint64_t itr0 = h;
    uint64_t stride0 = (ldm * elem_size) >> 3;
    uint64_t itr1 = num_batch;
    constexpr uint64_t stride1 = segment_size;
    uint64_t data_len = segment_size * itr0 * itr1;

    __builtin_riscv_dte_set_length(h * ldm, 0, data_len);
    __builtin_riscv_dte_set_wdma_dst_itera_stride(1, itr1 | (stride1 << 32),
                                                  itr0 | (stride0 << 32));
    __builtin_riscv_dte_set_wdma_misc(0, data_len, segment_size);
    __builtin_riscv_dte_scatter(dst_addr, 0, src_addr);

    src_addr += data_len;
    dst_addr += num_batch * segment_size;
  }

  if (aligned_c % max_c_align) {
    uint32_t segment_size =
        ((c - num_batch * max_c_align) * elem_size) >> 3;
    uint64_t itr0 = h;
    uint64_t dst_stride0 = (ldm * elem_size) >> 3;
    uint64_t src_stride0 = ((max_c_align >> 1) * elem_size) >> 3;
    uint64_t src_data_len = src_stride0 * itr0;

    __builtin_riscv_dte_set_length(h * ldm, 0, src_data_len);
    __builtin_riscv_dte_set_wdma_src_itera_stride(1, 1,
                                                  itr0 | (src_stride0 << 32));
    __builtin_riscv_dte_set_wdma_dst_itera_stride(1, 1,
                                                  itr0 | (dst_stride0 << 32));

    __builtin_riscv_dte_set_wdma_misc(0, src_data_len, segment_size);
    __builtin_riscv_dte_gather_scatter_l2g(dst_addr, 0, src_addr);
  }
}

/// @brief Load MXFP8/MXFP4 scale data from GMEM to the SMEM scale area.
///
/// For MXFP8/MXFP4 formats, SMEM is partitioned as [data area][scale area].
/// load_matrix_async only fills the data area; this function loads the E8M0
/// scales into the reserved scale area using a 1-D contiguous DTE gather.
///
/// For matrix_a: scales go to L0A SMEM at desc->smem + data_aligned_size.
/// For matrix_b: scales go to L1 SMEM at l1_temp + data_aligned_size_l1,
///               then load_matrix_async copies L1->L0W together with data.
///
/// @param desc         The matrix tile descriptor
/// @param src          Source E8M0 scale buffer in GMEM (already offset by
/// crd0)
/// @param actual_rows  Number of scale rows to load (0 = use tile height h).
///                     Used for tail tiles where actual rows < tile height.
/// @param l1_temp      L1 shared memory buffer (required for matrix_b)
template <int Use, typename T>
__RAISA_NE_DEVICE_DECL__ static void
load_mx_scales_async_dyn(const matrix_desc_dyn<Use, T> &desc, const e8m0 *src,
                         uint32_t M, uint32_t N, uint32_t K,
                         unsigned actual_rows = 0,
                         __L1shared__ e8m0 *l1_temp = nullptr) {
  using m_type = matrix_desc_dyn<Use, T>;
  static_assert(m_type::ne_prec == NE_PREC_MXFP8E4 ||
                    m_type::ne_prec == NE_PREC_MXFP8E5 ||
                    m_type::ne_prec == NE_PREC_MXFP4,
                "load_mx_scales_async is only valid for MX types");

  uint32_t h =
      (Use == matrix_a || Use == accumulator) ? M : ((Use == matrix_b) ? N : K);
  uint32_t c = (Use == matrix_a || Use == matrix_b)
                             ? K
                             : ((Use == matrix_trans_a) ? M : N);
  constexpr uint32_t es = m_type::elem_size; // bits per element
  uint32_t scale_cols = c / 32;    // each E8M0 covers 32 elements
  constexpr bool use_l1 = (Use == matrix_b || Use == matrix_trans_b);

  // Compute data area size in bytes to locate the scale area in SMEM.
  // For MX types, elem_size is 8 bits (1 byte), so max_c_align = 1024/8 = 128.
  // L0A base alignment = 256 bytes; L1 = 32 bytes.
  constexpr uint32_t max_c = 1024u / es;  // elements per batch (128 for 1-byte)
  constexpr uint32_t tail_c = max_c >> 1; // 64
  uint32_t alc = NE_ALIGN(c, tail_c);
  uint32_t nb = alc / max_c;
  constexpr uint32_t pad = use_l1 ? 32u : 256u; // byte alignment
  uint32_t data_bytes =
      (nb * NE_ALIGN(h * max_c * (es >> 3), pad) +
       (alc % max_c ? NE_ALIGN(h * tail_c * (es >> 3), pad) : 0));

  uint64_t dst_addr;
  if constexpr (use_l1) {
    dst_addr = reinterpret_cast<uint64_t>(l1_temp) + data_bytes;
  } else {
    dst_addr = reinterpret_cast<uint64_t>(desc.smem) + data_bytes;
  }

  unsigned rows = (actual_rows == 0) ? h : actual_rows;
  unsigned total_bytes = rows * scale_cols;
  uint64_t src_addr = reinterpret_cast<uint64_t>(src);

  __builtin_riscv_dte_set_length(total_bytes, 0, total_bytes);
  __builtin_riscv_dte_set_rdma_src_itera_stride(
      1, 1, 1ULL | ((uint64_t)total_bytes << 32));
  __builtin_riscv_dte_set_rdma_misc(0, total_bytes, total_bytes);
  __builtin_riscv_dte_gather(dst_addr, 0, src_addr);
}

///
/// MMA Functions
///
/// D = A * B + C
template <int Op = op_gemm_psum, int AUse, int BUse, typename Ta, typename Tb,
          typename Tc, typename Td, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_async_dyn(const matrix_desc_dyn<accumulator, Td> &d,
              const matrix_desc_dyn<AUse, Ta, SP> &a,
              const matrix_desc_dyn<BUse, Tb, SP> &b,
              const matrix_desc_dyn<accumulator, Tc> &c, uint32_t M, uint32_t N,
              uint32_t K) {
  using a_type = matrix_desc_dyn<AUse, Ta, SP>;
  using b_type = matrix_desc_dyn<BUse, Tb, SP>;
  using c_type = matrix_desc_dyn<accumulator, Tc>;
  using d_type = matrix_desc_dyn<accumulator, Td>;

  static_assert(AUse == matrix_a || AUse == matrix_trans_a,
                "Invalid use for left matrix");
  static_assert(BUse == matrix_b || BUse == matrix_trans_b,
                "Invalid use for right matrix");
  static_assert(is_valid_ne_prec_combine<a_type::ne_prec, b_type::ne_prec,
                                         c_type::ne_prec, d_type::ne_prec>(),
                "Invalid matrix type combination");

  constexpr uint64_t prec = NE_ENCODE_PREC(a_type::ne_prec, b_type::ne_prec,
                                           d_type::ne_prec, c_type::ne_prec);
  constexpr uint64_t misc = MMA_ENCODE_MISC((SP == matrix_b_sparse) ? 1 : 0,
                                            (AUse == matrix_trans_a) ? 1 : 0,
                                            (BUse == matrix_trans_b) ? 1 : 0);
  uint64_t mkn = NE_ENCODE_MKN(M, K, N);
  uint64_t sparse_index_size = 0;
  uint64_t param1 = 0;
  if constexpr (SP == matrix_b_sparse) {
    sparse_index_size = b_type::index_size;
    param1 = reinterpret_cast<uint64_t>(b.index_smem);
  }

  // Config
  __builtin_riscv_ne_set_prec(0, 0, prec);
  __builtin_riscv_ne_set_misc(0, 0, misc);
  __builtin_riscv_ne_set_mkn(0, 0, mkn);
  __builtin_riscv_ne_set_length(
      d_type::get_aligned_size(M, N, K),
      static_cast<uint64_t>(c_type::get_aligned_size(M, N, K)) |
          (sparse_index_size << 32),
      static_cast<uint64_t>(a_type::get_aligned_size(M, N, K)) |
          (static_cast<uint64_t>(b_type::get_aligned_size(M, N, K)) << 32));

  uint64_t param0 = (reinterpret_cast<uint64_t>(a.smem) >> 1) |
                    ((reinterpret_cast<uint64_t>(b.smem) >> 1) << 32);
  if constexpr (Op == op_gemm) {
    // d = a * b
    __builtin_riscv_gemm((reinterpret_cast<uint64_t>(d.smem) >> 1), param1,
                         param0);
  } else if constexpr (Op == op_gemm_psum) {
    // d = a * b + c
    __builtin_riscv_gemm_psum(
        (reinterpret_cast<uint64_t>(d.smem) >> 1) |
            ((reinterpret_cast<uint64_t>(c.smem) >> 1) << 32),
        param1, param0);
  } else {
    // static_assert(M >= 1 && M <= 64, "Invalid M for small shape mma");
    // static_assert(N >= 1 && N <= 64, "Invalid N for small shape mma");
    if constexpr (Op == op_mma_first) {
      // temp = a * b
      __builtin_riscv_mma_first(0, param1, param0);
    } else if constexpr (Op == op_mma_first_psum) {
      // temp = a * b + c
      __builtin_riscv_mma_first_psum(
          (reinterpret_cast<uint64_t>(c.smem) >> 1) << 32, param1, param0);
    } else if constexpr (Op == op_mma) {
      // temp = a * b + temp
      __builtin_riscv_mma(0, param1, param0);
    } else {
      // d = a * b + temp
      static_assert(Op == op_mma_last, "Invalid Op");
      __builtin_riscv_mma_last((reinterpret_cast<uint64_t>(d.smem) >> 1),
                               param1, param0);
    }
  }
}

/// D = A * B
template <int Op = op_gemm, int AUse, int BUse, typename Ta, typename Tb,
          typename Td, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_async_dyn(const matrix_desc_dyn<accumulator, Td> &d,
              const matrix_desc_dyn<AUse, Ta, SP> &a,
              const matrix_desc_dyn<BUse, Tb, SP> &b, uint32_t M, uint32_t N,
              uint32_t K) {
  matrix_desc_dyn<accumulator, typename accumulator_type<Ta>::type> temp_c = {
      nullptr};
  mma_async_dyn<Op>(d, a, b, temp_c, M, N, K);
}

///
/// Small Shape MMA Functions With M/N <= 64
///
/// The Dst/Acc matrix type must be INT32 or FLOAT(depends on Ta)
///
/// T = A * B
template <int AUse, int BUse, typename Ta, typename Tb, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_first_async_dyn(const matrix_desc_dyn<AUse, Ta, SP> &a,
                    const matrix_desc_dyn<BUse, Tb, SP> &b, uint32_t M,
                    uint32_t N, uint32_t K) {
  matrix_desc_dyn<accumulator, typename accumulator_type<Ta>::type> temp_d = {
      nullptr};
  mma_async_dyn<op_mma_first>(temp_d, a, b, M, N, K);
}

/// T = A * B + C
template <int AUse, int BUse, typename Ta, typename Tb, typename Tc, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_first_async_dyn(const matrix_desc_dyn<AUse, Ta, SP> &a,
                    const matrix_desc_dyn<BUse, Tb, SP> &b,
                    const matrix_desc_dyn<accumulator, Tc> &c, uint32_t M,
                    uint32_t N, uint32_t K) {
  matrix_desc_dyn<accumulator, typename accumulator_type<Ta>::type> temp_d = {
      nullptr};
  mma_async_dyn<op_mma_first_psum>(temp_d, a, b, c, M, N, K);
}

/// T = A * B + T
template <int AUse, int BUse, typename Ta, typename Tb, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_acc_async_dyn(const matrix_desc_dyn<AUse, Ta, SP> &a,
                  const matrix_desc_dyn<BUse, Tb, SP> &b, uint32_t M,
                  uint32_t N, uint32_t K) {
  matrix_desc_dyn<accumulator, typename accumulator_type<Ta>::type> temp_d = {
      nullptr};
  mma_async_dyn<op_mma>(temp_d, a, b, M, N, K);
}

/// D = A * B + T
template <int AUse, int BUse, typename Ta, typename Tb, typename Td, int SP>
__RAISA_NE_DEVICE_DECL__ static void
mma_last_async_dyn(const matrix_desc_dyn<accumulator, Td> &d,
                   const matrix_desc_dyn<AUse, Ta, SP> &a,
                   const matrix_desc_dyn<BUse, Tb, SP> &b, uint32_t M,
                   uint32_t N, uint32_t K) {
  mma_async_dyn<op_mma_last>(d, a, b, M, N, K);
}

} // namespace mma
} // namespace raisa

#endif