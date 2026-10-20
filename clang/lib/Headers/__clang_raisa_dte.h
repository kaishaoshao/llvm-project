/*===-------------------- raisa_dte.h - RPU RAISA DTE -----------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===----------------------------------------------------------------------===
 */

#ifndef __RPU_RAISA_DTE_H
#define __RPU_RAISA_DTE_H

namespace raisa {

#define ALIGN(X, A) (((X) + (A)-1) & ~((A)-1))

/*
 * gmem/g3d to L1shared/L0AShared/L0WShared
 */
// segSize: inner dimension size
// srcLen: valid data size for src
// dstLen: valid data size for dst
// src must be 32Bytes aligned for g3d
// dst must be 32Bytes aligned for L1
__device__ __attribute__((__always_inline__)) static void
DTEGather(int64_t dst, int64_t src, int32_t segSize, int32_t itera0,
          int32_t stride0, int32_t itera1, int32_t stride1, int32_t itera2,
          int32_t stride2, int32_t srcLen, int32_t dstLen) {
  // RDMA
  int64_t param0 = (int64_t)segSize + ((int64_t)0 << 32);
  // datalen = segSize * itera0 * itera1 * itera2
  int64_t param1 = (int64_t)(segSize * itera0 * itera1 * itera2);
  __builtin_riscv_dte_set_rdma_misc(0, param1, param0);

  int64_t p0 = ((int64_t)stride0 << 32) | itera0;
  int64_t p1 = ((int64_t)stride1 << 32) | itera1;
  int64_t p2 = ((int64_t)stride2 << 32) | itera2;
  __builtin_riscv_dte_set_rdma_src_itera_stride(p2, p1, p0);

  __builtin_riscv_dte_set_length(dstLen, 0, srcLen);
  __builtin_riscv_dte_gather(dst, 0, src);
}

__device__ __attribute__((__always_inline__)) static void
DTEGatherScatterG2L(int64_t dst, int64_t src, int32_t segSize,
                    int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                    int32_t srcStride1, int32_t srcItera2, int32_t srcStride2,
                    int32_t dstItera0, int32_t dstStride0, int32_t dstItera1,
                    int32_t dstStride1, int32_t dstItera2, int32_t dstStride2,
                    int32_t srcLen, int32_t dstLen, bool srcNoShuffle) {
  // RDMA
  int64_t param0 = (int64_t)segSize + ((int64_t)0 << 32);
  if (srcNoShuffle) { // src is not shuffle
    int64_t param1 = (int64_t)(segSize * dstItera0 * dstItera1 * dstItera2);
    __builtin_riscv_dte_set_rdma_misc(0, param1, param0);
  } else {
    __builtin_riscv_dte_set_rdma_misc(0, 0, param0);
  }

  int64_t srcp0 = ((int64_t)srcStride0 << 32) | srcItera0;
  int64_t srcp1 = ((int64_t)srcStride1 << 32) | srcItera1;
  int64_t srcp2 = ((int64_t)srcStride2 << 32) | srcItera2;
  __builtin_riscv_dte_set_rdma_src_itera_stride(srcp2, srcp1, srcp0);

  int64_t dstp0 = ((int64_t)dstStride0 << 32) | dstItera0;
  int64_t dstp1 = ((int64_t)dstStride1 << 32) | dstItera1;
  int64_t dstp2 = ((int64_t)dstStride2 << 32) | dstItera2;
  __builtin_riscv_dte_set_rdma_dst_itera_stride(dstp2, dstp1, dstp0);

  __builtin_riscv_dte_set_length(dstLen, 0, srcLen);
  __builtin_riscv_dte_gather_scatter_g2l(dst, 0, src);
}

// Optimized g2l gather_scatter. Callers pass the full two-sided descriptor, the
// srcNoShuffle hint, and a mode:
//   dtegors  (dst is a single contiguous run) -> DTEGather, src descriptor
//   dtegands (otherwise)                       -> full gather_scatter
__device__ __attribute__((__always_inline__)) static void
DTEGatherScatterG2LOpt(int64_t dst, int64_t src, int32_t segSize,
                       int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                       int32_t srcStride1, int32_t srcItera2, int32_t srcStride2,
                       int32_t dstItera0, int32_t dstStride0, int32_t dstItera1,
                       int32_t dstStride1, int32_t dstItera2, int32_t dstStride2,
                       int32_t srcLen, int32_t dstLen, bool srcNoShuffle,
                       raisa::DTETy mode) {
  if (mode == raisa::DTETy::dtegors) {
    DTEGather(dst, src, segSize, srcItera0, srcStride0, srcItera1, srcStride1,
              srcItera2, srcStride2, srcLen, dstLen);
  } else {
    DTEGatherScatterG2L(dst, src, segSize, srcItera0, srcStride0, srcItera1,
                        srcStride1, srcItera2, srcStride2, dstItera0, dstStride0,
                        dstItera1, dstStride1, dstItera2, dstStride2, srcLen,
                        dstLen, srcNoShuffle);
  }
}

__device__ __attribute__((__always_inline__)) static void
DTEIndexGather(int64_t dst, int64_t src0, int32_t src1, int32_t segSize,
               int32_t indexElemNum, int32_t srcLen, int32_t dstLen) {
  // RDMA
  int64_t param0 = (int64_t)segSize + ((int64_t)indexElemNum << 32);
  int64_t param1 = (int64_t)(segSize * indexElemNum);
  __builtin_riscv_dte_set_rdma_misc(0, param1, param0);

  __builtin_riscv_dte_set_length(dstLen, 0, srcLen);
  __builtin_riscv_dte_index_gather(dst, src1, src0);
}

/*
 * L1shared/L0AShared to gmem/g3d
 */
// dst must be 32Bytes aligned for g3d
// src must be 32Bytes aligned when l1
__device__ __attribute__((__always_inline__)) static void
DTEScatter(int64_t dst, int64_t src, int32_t segSize, int itera0, int stride0,
           int itera1, int stride1, int itera2, int stride2, int32_t srcLen,
           int32_t dstLen) {
  // WDMA
  int64_t param0 = (int64_t)segSize + ((int64_t)0 << 32);
  // datalen = segSize * itera0 * itera1 * itera2
  int64_t param1 = (int64_t)(segSize * itera0 * itera1 * itera2);
  __builtin_riscv_dte_set_wdma_misc(0, param1, param0);
  int64_t p0 = ((int64_t)stride0 << 32) | itera0;
  int64_t p1 = ((int64_t)stride1 << 32) | itera1;
  int64_t p2 = ((int64_t)stride2 << 32) | itera2;
  __builtin_riscv_dte_set_wdma_dst_itera_stride(p2, p1, p0);

  __builtin_riscv_dte_set_length(dstLen, 0, srcLen);
  __builtin_riscv_dte_scatter(dst, 0, src);
}

__device__ __attribute__((__always_inline__)) static void
DTEGatherScatterL2G(int64_t dst, int64_t src, int32_t segSize,
                    int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                    int32_t srcStride1, int32_t srcItera2, int32_t srcStride2,
                    int32_t dstItera0, int32_t dstStride0, int32_t dstItera1,
                    int32_t dstStride1, int32_t dstItera2, int32_t dstStride2,
                    int32_t srcLen, int32_t dstLen, bool dstNoShuffle) {
  // WDMA
  int64_t param0 = (int64_t)segSize + ((int64_t)0 << 32);

  if (dstNoShuffle) { // dst is not shuffle
    int64_t param1 = (int64_t)(segSize * srcItera0 * srcItera1 * srcItera2);
    __builtin_riscv_dte_set_wdma_misc(0, param1, param0);
  } else {
    __builtin_riscv_dte_set_wdma_misc(0, 0, param0);
  }

  int64_t srcp0 = ((int64_t)srcStride0 << 32) | srcItera0;
  int64_t srcp1 = ((int64_t)srcStride1 << 32) | srcItera1;
  int64_t srcp2 = ((int64_t)srcStride2 << 32) | srcItera2;
  __builtin_riscv_dte_set_wdma_src_itera_stride(srcp2, srcp1, srcp0);

  int64_t dstp0 = ((int64_t)dstStride0 << 32) | dstItera0;
  int64_t dstp1 = ((int64_t)dstStride1 << 32) | dstItera1;
  int64_t dstp2 = ((int64_t)dstStride2 << 32) | dstItera2;
  __builtin_riscv_dte_set_wdma_dst_itera_stride(dstp2, dstp1, dstp0);

  __builtin_riscv_dte_set_length(dstLen, 0, srcLen);
  __builtin_riscv_dte_gather_scatter_l2g(dst, 0, src);
}

// Optimized l2g gather_scatter. Callers pass the full two-sided descriptor, the
// dstNoShuffle hint, and a mode:
//   dtegors  (src is a single contiguous run) -> DTEScatter, dst descriptor
//   dtegands (otherwise)                       -> full gather_scatter
__device__ __attribute__((__always_inline__)) static void
DTEGatherScatterL2GOpt(int64_t dst, int64_t src, int32_t segSize,
                       int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                       int32_t srcStride1, int32_t srcItera2, int32_t srcStride2,
                       int32_t dstItera0, int32_t dstStride0, int32_t dstItera1,
                       int32_t dstStride1, int32_t dstItera2, int32_t dstStride2,
                       int32_t srcLen, int32_t dstLen, bool dstNoShuffle,
                       raisa::DTETy mode) {
  if (mode == raisa::DTETy::dtegors) {
    DTEScatter(dst, src, segSize, dstItera0, dstStride0, dstItera1, dstStride1,
               dstItera2, dstStride2, srcLen, dstLen);
  } else {
    DTEGatherScatterL2G(dst, src, segSize, srcItera0, srcStride0, srcItera1,
                        srcStride1, srcItera2, srcStride2, dstItera0, dstStride0,
                        dstItera1, dstStride1, dstItera2, dstStride2, srcLen,
                        dstLen, dstNoShuffle);
  }
}

__device__ __attribute__((__always_inline__)) static void
DTEIndexScatter(int64_t dst, int64_t src0, int64_t src1, int32_t segSize,
                int32_t indexElemNum, int32_t srcLen, int32_t dstLen) {
  // WDMA
  int64_t param0 = (int64_t)segSize + ((int64_t)indexElemNum << 32);
  int64_t param1 = (int64_t)(segSize * indexElemNum);
  __builtin_riscv_dte_set_wdma_misc(0, param1, param0);

  __builtin_riscv_dte_set_length(dstLen, 0, srcLen);
  __builtin_riscv_dte_index_scatter(dst, src1, src0);
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::DTEDirectionType
DTEDirection(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if ((src_tensor_map.tensor_as == TensorAS::gmem &&
       dst_tensor_map.tensor_as == TensorAS::L1shared) ||
      (src_tensor_map.tensor_as == TensorAS::gmem &&
       dst_tensor_map.tensor_as == TensorAS::L0Ashared) ||
      (src_tensor_map.tensor_as == TensorAS::gmem &&
       dst_tensor_map.tensor_as == TensorAS::shared)) {
    return raisa::DTEDirectionType::g2l;
  } else if ((src_tensor_map.tensor_as == TensorAS::L1shared &&
              dst_tensor_map.tensor_as == TensorAS::gmem) ||
             (src_tensor_map.tensor_as == TensorAS::L0Ashared &&
              dst_tensor_map.tensor_as == TensorAS::gmem) ||
             (src_tensor_map.tensor_as == TensorAS::shared &&
              dst_tensor_map.tensor_as == TensorAS::gmem)) {
    return raisa::DTEDirectionType::l2g;
  }
  return raisa::DTEDirectionType::unknown;
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::DTETy
DTEG2LType(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if (src_tensor_map.indexElemNum) {
    // DTEIndexGather: src has indexElemNum
    return raisa::DTETy::dteigs;
  } else if (dst_tensor_map.IsContinuous()) {
    // DTEGather: dst is continuous
    return raisa::DTETy::dtegors;
  } else if (!dst_tensor_map.IsContinuous()) {
    // DTEGatherScatterG2L: dst is not continuous
    return raisa::DTETy::dtegands;
  }
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::DTETy
DTEL2GType(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if (dst_tensor_map.indexElemNum) {
    // DTEIndexScatter: dst has indexElemNum
    return raisa::DTETy::dteigs;
  } else if (!src_tensor_map.IsContinuous()) {
    // DTEGatherScatterL2G: src has stride (not continuous)
    return raisa::DTETy::dtegands;
  } else if (src_tensor_map.IsContinuous()) {
    // DTEScatter: src does not have stride(continuous)
    return raisa::DTETy::dtegors;
  }
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2CDTE(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map, int32_t c0,
       Ts... args) {
  if (DTEDirection(dst_tensor_map, src_tensor_map) ==
      raisa::DTEDirectionType::g2l) {
    auto type = DTEG2LType(dst_tensor_map, src_tensor_map);
    // DTEGather: dst does not have stride (continuous)
    if (type == raisa::DTETy::dtegors) {
      int32_t offset = GetOffset(src_tensor_map, c0, args...);
      return DTEGather(
          dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
          src_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
          src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
          src_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
          dst_tensor_map.getDataByteSize());
    } else if (type ==
               raisa::DTETy::dtegands) { // DTEGatherScatterG2L: dst has non
                                        // continuous stride (not continuous)
      int32_t offset = GetOffset(src_tensor_map, c0, args...);
      return DTEGatherScatterG2L(
          dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
          dst_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
          src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
          src_tensor_map.getStrides2(), dst_tensor_map.getIteras0(),
          dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
          dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
          dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
          dst_tensor_map.getDataByteSize(), src_tensor_map.IsContinuous());
    } else if (type ==
               raisa::DTETy::dteigs) { // DTEIndexGather: src has indexElemNum
      return DTEIndexGather(
          dst_tensor_map.GetAddr(), src_tensor_map.GetAddr(),
          *src_tensor_map.indexAddr, src_tensor_map.getSegmentByteSize(),
          *src_tensor_map.indexElemNum, src_tensor_map.getDataByteSize(),
          dst_tensor_map.getDataByteSize());
    } else {

    }
  } else if (DTEDirection(dst_tensor_map, src_tensor_map) ==
             raisa::DTEDirectionType::l2g) {
    auto type = DTEL2GType(dst_tensor_map, src_tensor_map);
    // DTEScatter: src does not have stride(continuous)
    if (type == raisa::DTETy::dtegors) {
      int32_t offset = GetOffset(dst_tensor_map, c0, args...);
      return DTEScatter(
          dst_tensor_map.GetAddr() + offset, src_tensor_map.GetAddr(),
          dst_tensor_map.getSegmentByteSize(), dst_tensor_map.getIteras0(),
          dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
          dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
          dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
          dst_tensor_map.getDataByteSize());
    } else if (type ==
               raisa::DTETy::dtegands) { // DTEGatherScatterL2G: src has non
                                        // continuous stride (not continuous)
      int32_t offset = GetOffset(dst_tensor_map, c0, args...);
      return DTEGatherScatterL2G(
          dst_tensor_map.GetAddr() + offset, src_tensor_map.GetAddr(),
          src_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
          src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
          src_tensor_map.getStrides2(), dst_tensor_map.getIteras0(),
          dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
          dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
          dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
          dst_tensor_map.getDataByteSize(), dst_tensor_map.IsContinuous());
    } else if (type ==
               raisa::DTETy::dteigs) { // DTEIndexScatter: dst has indexElemNum
      // src1: index[]
      return DTEIndexScatter(
          dst_tensor_map.GetAddr(), src_tensor_map.GetAddr(),
          *dst_tensor_map.indexAddr, dst_tensor_map.getSegmentByteSize(),
          *dst_tensor_map.indexElemNum, src_tensor_map.getDataByteSize(),
          dst_tensor_map.getDataByteSize());
    } else {

    }
  } else {

    return;
  }
}

// Compact -> compact gmem load (g2l). The contiguous (dtegors) fast path with
// no padding copies only the valid rshape bytes; otherwise the physical view
// (rshape iteras, pstrides) iterates rshape and steps over padding. When there
// is no padding (pshape==rshape) this reproduces the original gather behavior.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2CDTEG2L(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
          int32_t c0, Ts... args) {
  if (DTEG2LType(dst_tensor_map, src_tensor_map) == raisa::DTETy::dtegors) {
    // rshape == pshape means no padding → valid data is physically contiguous
    bool no_padding = true;
    for (int i = 0; i < src_tensor_map.tensorRank; ++i) {
      if (src_tensor_map.rshape[i] != src_tensor_map.pshape[i] ||
          dst_tensor_map.rshape[i] != dst_tensor_map.pshape[i]) {
        no_padding = false;
        break;
      }
    }
    if (no_padding) {
      int32_t offset = GetOffset(src_tensor_map, c0, args...);
      if (src_tensor_map.getSegmentByteSize() == src_tensor_map.getDataByteSize()) {
        return DTEGather(
            dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
            src_tensor_map.getSegmentByteSize(), 1,
            src_tensor_map.getSegmentByteSize(), 1, 1, 1, 1,
            src_tensor_map.getDataByteSize(),
            dst_tensor_map.getDataByteSize());
      } else {
        return DTEGather(
            dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
            src_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
            src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
            src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
            src_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
            dst_tensor_map.getDataByteSize());
      }
    }
    // padding exists: fall through to PhysView strided path
  }
  TensorMap<T> dphys = PhysView(dst_tensor_map);
  TensorMap<T> sphys = PhysView(src_tensor_map);
  return C2CDTE(dphys, sphys, c0, args...);
}

template <typename T>
__device__ __attribute__((__always_inline__)) static int64_t
DTEAlignOffset(TensorMap<T> &tensor_map, const uint32_t max_c_align) {
  int64_t offset = tensor_map.TypeSize() * max_c_align *
                   tensor_map.GetRealDimSize() /
                   tensor_map.GetRealInnerDimSize();
  // dst_addr should be aligned by 256B when to l0a
  if (tensor_map.tensor_as == TensorAS::L0Ashared ||
      tensor_map.tensor_as == TensorAS::shared) {
    if (tensor_map.tensorRank == 3) {
      offset = offset / tensor_map.rshape[0];
    }
    offset = ALIGN(offset, 256);
    if (tensor_map.tensorRank == 3) {
      offset = offset * tensor_map.rshape[0];
    }
  }
  return offset;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2ADTEL2G(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
          int32_t c0, Ts... args) {
  int32_t offset = GetOffset(dst_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr();
  int64_t dst_addr = dst_tensor_map.GetAddr() + offset;

  const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1],
                             max_c_align >> 1);
  uint32_t align_num = aligned_c / max_c_align;

  int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
  int64_t dst_offset = DTEAlignOffset(dst_tensor_map, max_c_align);
  // valid element count of the compact inner dim
  int32_t valid_c = src_tensor_map.rshape[src_tensor_map.tensorRank - 1];
  // Merge the full max_c_align blocks into one call (block index on the unused
  // itera0 level: compact src steps by one block, align dst by dst_offset, which
  // may carry a padding gap the hardware honors). Every full block has the same
  // segSize.
  if (align_num >= 1) {
    // No tail + real-continuous compact src -> single contiguous run, so it
    // degrades to a pure scatter driven by the dst descriptor. Guard on valid_c
    // (not aligned_c): a non-multiple valid_c makes the compact src row stride
    // differ from align_num*segSize, so the full block is not a single run.
    bool src_one_run =
        src_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
    raisa::DTETy mode =
        src_one_run ? raisa::DTETy::dtegors : raisa::DTETy::dtegands;
    DTEGatherScatterL2GOpt(
        dst_addr, src_addr, segSize, align_num,
        src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
        src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
        src_tensor_map.getStrides2(), align_num, (int32_t)dst_offset,
        dst_tensor_map.getIteras1(), dst_tensor_map.TypeSize() * max_c_align,
        dst_tensor_map.getIteras2(),
        dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
        src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
        true, mode);
  }

  int32_t tailSegSize = dst_tensor_map.TypeSize() * (max_c_align >> 1);

  if (aligned_c % max_c_align) {
    int64_t src_addr_tail =
        src_addr + align_num * src_tensor_map.TypeSize() * max_c_align;
    int64_t dst_addr_tail = dst_addr + align_num * dst_offset;

    DTEGatherScatterL2G(
        dst_addr_tail, src_addr_tail, tailSegSize, 1,
        src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
        src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
        src_tensor_map.getIteras2(), src_tensor_map.getStrides2(), 1,
        dst_tensor_map.TypeSize() * (max_c_align >> 1),
        dst_tensor_map.getIteras1(),
        dst_tensor_map.TypeSize() * (max_c_align >> 1),
        dst_tensor_map.getIteras2(),
        dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() *
            (max_c_align >> 1),
        src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
        true);
  }
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2CDTEL2G(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
          int32_t c0, Ts... args) {
  // TODO: offset computed by aligned datalayout
  int32_t offset = GetOffset(dst_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr();
  int64_t dst_addr = dst_tensor_map.GetAddr() + offset;

  const uint32_t max_c_align = 1024u / src_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(src_tensor_map.rshape[src_tensor_map.tensorRank - 1],
                             max_c_align >> 1);

  int64_t src_offset = DTEAlignOffset(src_tensor_map, max_c_align);
  int64_t src_tail_offset = DTEAlignOffset(src_tensor_map, max_c_align >> 1);
  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = src_tensor_map.rshape[src_tensor_map.tensorRank - 1];

  // rank<=2 / L1shared: merge the full max_c_align blocks into one call (block
  // index on the unused itera0 level: align src steps by src_offset, compact dst
  // by one block).
  if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2 ||
      src_tensor_map.tensor_as == TensorAS::L1shared) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterL2G(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          n_full, dst_tensor_map.TypeSize() * max_c_align,
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = src_tensor_map.TypeSize() * align_size;
      int64_t dst_addr_tail =
          dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: src row stride uses (max_c_align >> 1)
        DTEGatherScatterL2G(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * (max_c_align >> 1),
            src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * (max_c_align >> 1),
            src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() *
                (max_c_align >> 1),
            1, dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
            dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
            dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      } else {
        // trimmed-last-block layout: src row stride stays at max_c_align
        DTEGatherScatterL2G(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
            1, dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
            dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
            dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      }
    }
    return;
  }

  // rank-3 L0A/L0W (shared): fold both the channel-block loop (itera0) and the
  // rshape[0] loop (itera2) into the descriptor; rshape[1] stays on itera1. The
  // full blocks collapse to one call, the partial tail to one more (Case B uses
  // src row stride max_c_align>>1 and src_tail_offset for the rshape[0] step;
  // the trimmed Case A keeps max_c_align and src_offset).
  if (src_tensor_map.tensorRank == 3 &&
      (src_tensor_map.tensor_as == TensorAS::L0Ashared ||
       src_tensor_map.tensor_as == TensorAS::shared)) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterL2G(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.rshape[0],
          (int32_t)(src_offset / src_tensor_map.rshape[0]), n_full,
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras1(),
          dst_tensor_map.getStrides1(), dst_tensor_map.rshape[0],
          dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = src_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      int64_t dst_addr_tail =
          dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
      if (aligned_c % max_c_align) {
        DTEGatherScatterL2G(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * (max_c_align >> 1),
            src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * (max_c_align >> 1),
            src_tensor_map.rshape[0],
            (int32_t)(src_tail_offset / src_tensor_map.rshape[0]), 1,
            dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
            dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
            dst_tensor_map.rshape[0],
            dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      } else {
        DTEGatherScatterL2G(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.rshape[0],
            (int32_t)(src_offset / src_tensor_map.rshape[0]), 1,
            dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
            dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
            dst_tensor_map.rshape[0],
            dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      }
    }
  }
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2ADTEL2G(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
          int32_t c0, Ts... args) {
  // TODO: offset computed by aligned datalayout
  int32_t offset = GetOffset(dst_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr();
  int64_t dst_addr = dst_tensor_map.GetAddr() + offset;

  const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1],
                             max_c_align >> 1);
  uint32_t align_num = aligned_c / max_c_align;

  int64_t src_offset = DTEAlignOffset(src_tensor_map, max_c_align);
  int64_t src_tail_offset = DTEAlignOffset(src_tensor_map, max_c_align >> 1);
  int64_t dst_offset = DTEAlignOffset(dst_tensor_map, max_c_align);
  int64_t dst_tail_offset = DTEAlignOffset(dst_tensor_map, max_c_align >> 1);

  // rank<=2 / L1shared: fold the align_num full blocks onto itera0 (both sides
  // align, so src steps by src_offset and dst by dst_offset). dstNoShuffle=true
  // preserves the full datalen the original per-block DTEScatter set.
  if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2 ||
      src_tensor_map.tensor_as == TensorAS::L1shared) {
    if (align_num >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterL2G(
          dst_addr, src_addr, segSize, align_num, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          align_num, (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras2(),
          dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (aligned_c % max_c_align) {
      int32_t tailSegSize = dst_tensor_map.TypeSize() * (max_c_align >> 1);
      int64_t dst_addr_tail = dst_addr + align_num * dst_offset;
      int64_t src_addr_tail = src_addr + align_num * src_offset;
      DTEGatherScatterL2G(
          dst_addr_tail, src_addr_tail, tailSegSize, 1,
          src_tensor_map.TypeSize() * (max_c_align >> 1),
          src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * (max_c_align >> 1),
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() *
              (max_c_align >> 1),
          1, dst_tensor_map.TypeSize() * (max_c_align >> 1),
          dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * (max_c_align >> 1),
          dst_tensor_map.getIteras2(),
          dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() *
              (max_c_align >> 1),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    return;
  }

  // rank-3 L0A/L0W: fold the channel-block loop (itera0) and the rshape[0] loop
  // (itera2) into the descriptor; rshape[1] stays on itera1. Both sides align,
  // so the rshape[0] step is *_offset / rshape[0]. The partial tail uses the
  // half-block row stride (max_c_align>>1) and *_tail_offset for rshape[0].
  if (src_tensor_map.tensorRank == 3 &&
      (src_tensor_map.tensor_as == TensorAS::L0Ashared ||
       src_tensor_map.tensor_as == TensorAS::shared)) {
    if (align_num >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterL2G(
          dst_addr, src_addr, segSize, align_num, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.rshape[0],
          (int32_t)(src_offset / src_tensor_map.rshape[0]), align_num,
          (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.rshape[0],
          (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (aligned_c % max_c_align) {
      int32_t tailSegSize = dst_tensor_map.TypeSize() * (max_c_align >> 1);
      int64_t dst_addr_tail = dst_addr + align_num * dst_offset;
      int64_t src_addr_tail = src_addr + align_num * src_offset;
      DTEGatherScatterL2G(
          dst_addr_tail, src_addr_tail, tailSegSize, 1,
          src_tensor_map.TypeSize() * (max_c_align >> 1),
          src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * (max_c_align >> 1),
          src_tensor_map.rshape[0],
          (int32_t)(src_tail_offset / src_tensor_map.rshape[0]), 1,
          dst_tensor_map.TypeSize() * (max_c_align >> 1),
          dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * (max_c_align >> 1),
          dst_tensor_map.rshape[0],
          (int32_t)(dst_tail_offset / dst_tensor_map.rshape[0]),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
  }
}

// Compact -> align gmem load (g2l). Iteration counts come from rshape,
// addressing from pstrides; each trailing block moves only the valid elements
// (rshape inner count) so the compact source is never over-read past the
// logical tensor. With no padding this matches the original align tiling.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2ADTEG2L(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
               int32_t c0, Ts... args) {
  int32_t offset = GetOffset(src_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr() + offset;
  int64_t dst_addr = dst_tensor_map.GetAddr();

  const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(
      dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1], max_c_align >> 1);
  uint32_t dst_inner_shape = dst_tensor_map.pshape[dst_tensor_map.tensorRank - 1];
  if (!dst_tensor_map.HasPadDim())
    dst_inner_shape = (max_c_align >> 1);

  int64_t dst_offset = DTEAlignOffset(dst_tensor_map, max_c_align);
  int64_t dst_tail_offset = DTEAlignOffset(dst_tensor_map, max_c_align >> 1);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1];

  // rank<=2 / L1shared: classify the inner dim into full max_c_align blocks plus
  // an optional partial tail. All full blocks share one segSize and a constant
  // src/dst block stride.
  if (dst_tensor_map.tensorRank == 1 || dst_tensor_map.tensorRank == 2 ||
      dst_tensor_map.tensor_as == TensorAS::L1shared) {
    uint32_t n_full = valid_c / max_c_align;

    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterG2L(
          dst_addr, src_addr, segSize, n_full,
          src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
          src_tensor_map.getStrides2(), n_full, (int32_t)dst_offset,
          dst_tensor_map.getIteras1(), max_c_align * dst_tensor_map.TypeSize(),
          dst_tensor_map.getIteras2(),
          dst_tensor_map.getIteras1() * max_c_align * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int64_t src_addr_tail =
          src_addr + n_full * src_tensor_map.TypeSize() * max_c_align;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: dst row stride uses the padded inner shape
        int32_t segSize = dst_tensor_map.TypeSize() * align_size;
        if (dst_tensor_map.tensor_as == TensorAS::L1shared)
          segSize = ALIGN(dst_tensor_map.TypeSize() * align_size, 32);
        DTEGatherScatterG2L(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
            src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
            src_tensor_map.getIteras2(), src_tensor_map.getStrides2(), 1,
            (dst_inner_shape) * dst_tensor_map.TypeSize(),
            dst_tensor_map.getIteras1(),
            (dst_inner_shape) * dst_tensor_map.TypeSize(),
            dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * (dst_inner_shape) *
                dst_tensor_map.TypeSize(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      } else {
        // trimmed-last-block layout: dst row stride stays at max_c_align
        if (dst_tensor_map.tensor_as == TensorAS::L1shared)
          align_size = ALIGN(align_size, 32 / dst_tensor_map.TypeSize());
        int32_t segSize = dst_tensor_map.TypeSize() * align_size;
        DTEGatherScatterG2L(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
            src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
            src_tensor_map.getIteras2(), src_tensor_map.getStrides2(), 1,
            max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.getIteras1(),
            max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * max_c_align *
                dst_tensor_map.TypeSize(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      }
    }
    return;
  }

  // rank-3 L0A/L0W (shared): fold both the channel-block loop (itera0) and the
  // rshape[0] loop (itera2) into the descriptor; rshape[1] stays on itera1. The
  // full blocks collapse to one call, the partial tail to one more (each keeps
  // its per-case dst row stride, and the rshape[0] step follows dst_offset for
  // the trimmed block / dst_tail_offset for the separate tail).
  if (dst_tensor_map.tensorRank == 3 &&
      (dst_tensor_map.tensor_as == TensorAS::L0Ashared ||
       dst_tensor_map.tensor_as == TensorAS::shared)) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterG2L(
          dst_addr, src_addr, segSize, n_full,
          src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), src_tensor_map.rshape[0],
          src_tensor_map.pstrides[0] * src_tensor_map.TypeSize(), n_full,
          (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.rshape[0],
          (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = dst_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail =
          src_addr + n_full * src_tensor_map.TypeSize() * max_c_align;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: dst row stride uses the padded inner shape,
        // rshape[0] step uses dst_tail_offset
        DTEGatherScatterG2L(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
            src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
            src_tensor_map.rshape[0],
            src_tensor_map.pstrides[0] * src_tensor_map.TypeSize(), 1,
            (dst_inner_shape) * dst_tensor_map.TypeSize(),
            dst_tensor_map.getIteras1(),
            (dst_inner_shape) * dst_tensor_map.TypeSize(),
            dst_tensor_map.rshape[0],
            (int32_t)(dst_tail_offset / dst_tensor_map.rshape[0]),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      } else {
        // trimmed-last-block layout: dst row stride stays at max_c_align,
        // rshape[0] step uses dst_offset
        DTEGatherScatterG2L(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
            src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
            src_tensor_map.rshape[0],
            src_tensor_map.pstrides[0] * src_tensor_map.TypeSize(), 1,
            max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.getIteras1(),
            max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.rshape[0],
            (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      }
    }
  }
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2CDTEG2L(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
          int32_t c0, Ts... args) {
  // TODO: offset computed by aligned datalayout
  int32_t offset = GetOffset(src_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr() + offset;
  int64_t dst_addr = dst_tensor_map.GetAddr();

  const uint32_t max_c_align = 1024u / src_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(src_tensor_map.rshape[src_tensor_map.tensorRank - 1],
                             max_c_align >> 1);
  uint32_t src_inner_shape = src_tensor_map.pshape[src_tensor_map.tensorRank - 1];
  if (!src_tensor_map.HasPadDim())
    src_inner_shape = (max_c_align >> 1);
  int64_t src_offset = DTEAlignOffset(src_tensor_map, max_c_align);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = src_tensor_map.rshape[src_tensor_map.tensorRank - 1];

  // Merge the full max_c_align blocks into one call (block index on the unused
  // itera0 level: align src steps by src_offset, compact dst by one block).
  uint32_t n_full = valid_c / max_c_align;

  if (n_full >= 1) {
    int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
    // No tail + real-continuous compact dst -> single contiguous run, so it
    // degrades to a pure gather driven by the src descriptor.
    bool dst_one_run =
        dst_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
    raisa::DTETy mode =
        dst_one_run ? raisa::DTETy::dtegors : raisa::DTETy::dtegands;
    DTEGatherScatterG2LOpt(
        dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
        src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
        src_tensor_map.getIteras2(),
        src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
        n_full, dst_tensor_map.TypeSize() * max_c_align,
        dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
        dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
        src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
        true, mode);
  }
  if (valid_c % max_c_align) {
    int32_t align_size = valid_c - n_full * max_c_align;
    int32_t segSize = src_tensor_map.TypeSize() * align_size;
    int64_t dst_addr_tail =
        dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
    int64_t src_addr_tail = src_addr + n_full * src_offset;
    if (aligned_c % max_c_align) {
      // separate-tail layout: src row stride uses the padded inner shape
      DTEGatherScatterG2L(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * (src_inner_shape),
          src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * (src_inner_shape),
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() *
              (src_inner_shape),
          1, dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    } else {
      // trimmed-last-block layout: src row stride stays at max_c_align
      DTEGatherScatterG2L(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          1, dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
  }
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2ADTEG2L(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
               int32_t c0, Ts... args) {
  int32_t offset = GetOffset(src_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr() + offset;
  int64_t dst_addr = dst_tensor_map.GetAddr();
  const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(
      dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1], max_c_align >> 1);
  uint32_t src_inner_shape = src_tensor_map.pshape[src_tensor_map.tensorRank - 1];
  if (!src_tensor_map.HasPadDim())
    src_inner_shape = (max_c_align >> 1);
  uint32_t dst_inner_shape = dst_tensor_map.pshape[dst_tensor_map.tensorRank - 1];
  if (!dst_tensor_map.HasPadDim())
    dst_inner_shape = (max_c_align >> 1);

  int64_t dst_offset = DTEAlignOffset(dst_tensor_map, max_c_align);
  int64_t dst_tail_offset = DTEAlignOffset(dst_tensor_map, max_c_align >> 1);
  int64_t src_offset = DTEAlignOffset(src_tensor_map, max_c_align);
  int64_t src_tail_offset = DTEAlignOffset(src_tensor_map, max_c_align >> 1);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1];

  // rank<=2 / L1shared: fold the full max_c_align blocks onto itera0 (both sides
  // align, so src steps by src_offset and dst by dst_offset).
  if (dst_tensor_map.tensorRank == 1 || dst_tensor_map.tensorRank == 2 ||
      dst_tensor_map.tensor_as == TensorAS::L1shared) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterG2L(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          n_full, (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras2(),
          dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: row stride uses the padded inner shape
        int32_t segSize = dst_tensor_map.TypeSize() * align_size;
        if (dst_tensor_map.tensor_as == TensorAS::L1shared)
          segSize = ALIGN(dst_tensor_map.TypeSize() * align_size, 32);
        DTEGatherScatterG2L(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * (src_inner_shape),
            src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * (src_inner_shape),
            src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() *
                (src_inner_shape),
            1, dst_tensor_map.TypeSize() * (dst_inner_shape),
            dst_tensor_map.getIteras1(),
            dst_tensor_map.TypeSize() * (dst_inner_shape),
            dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() *
                (dst_inner_shape),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      } else {
        // trimmed-last-block layout: row stride stays at max_c_align
        if (dst_tensor_map.tensor_as == TensorAS::L1shared)
          align_size = ALIGN(align_size, 32 / dst_tensor_map.TypeSize());
        int32_t segSize = dst_tensor_map.TypeSize() * align_size;
        DTEGatherScatterG2L(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
            1, dst_tensor_map.TypeSize() * max_c_align,
            dst_tensor_map.getIteras1(),
            dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
            true);
      }
    }
    return;
  }

  // rank-3 L0A/L0W: fold the channel-block loop (itera0) and the rshape[0] loop
  // (itera2) into the descriptor; rshape[1] stays on itera1. Both sides align,
  // so the rshape[0] step is *_offset / rshape[0].
  if (dst_tensor_map.tensorRank == 3 &&
      (dst_tensor_map.tensor_as == TensorAS::L0Ashared ||
       dst_tensor_map.tensor_as == TensorAS::shared)) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      DTEGatherScatterG2L(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.rshape[0],
          (int32_t)(src_offset / src_tensor_map.rshape[0]), n_full,
          (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.rshape[0],
          (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = dst_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      bool caseB = aligned_c % max_c_align;
      uint32_t src_row = caseB ? src_inner_shape : max_c_align;
      uint32_t dst_row = caseB ? dst_inner_shape : max_c_align;
      int32_t j_src = caseB
                          ? (int32_t)(src_tail_offset / src_tensor_map.rshape[0])
                          : (int32_t)(src_offset / src_tensor_map.rshape[0]);
      int32_t j_dst = caseB
                          ? (int32_t)(dst_tail_offset / dst_tensor_map.rshape[0])
                          : (int32_t)(dst_offset / dst_tensor_map.rshape[0]);
      DTEGatherScatterG2L(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * src_row, src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * src_row, src_tensor_map.rshape[0], j_src, 1,
          dst_tensor_map.TypeSize() * dst_row, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * dst_row, dst_tensor_map.rshape[0], j_dst,
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          true);
    }
  }
}

// gmem -> L0A/L1 load (g2l) that only moves the valid elements (rshape) of the
// innermost dimension, so the compact/aligned source padding is never read.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
DTEG2L(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
       int32_t c0, Ts... args) {
  auto DLTransformTy = DataLayoutTransformType(dst_tensor_map, src_tensor_map);
  switch (DLTransformTy) {
  case raisa::DataLayoutTransformTy::compact2compact: {
    // C2CDTEG2L: dtegors + no-padding → fast DTEGather; otherwise PhysView →
    // C2CDTE which handles all DTE types (dtegors, dtegands, dteigs).
    return C2CDTEG2L(dst_tensor_map, src_tensor_map, c0, args...);
  }
  case raisa::DataLayoutTransformTy::compact2align:
    // align paths compute dst addresses via DTEAlignOffset with fixed strides,
    // so the physical (pshape/pstrides) layout must be canonical.
    return C2ADTEG2L(dst_tensor_map, src_tensor_map, c0, args...);
  case raisa::DataLayoutTransformTy::align2compact: {
    return A2CDTEG2L(dst_tensor_map, src_tensor_map, c0, args...);
  }
  case raisa::DataLayoutTransformTy::align2align:
    return A2ADTEG2L(dst_tensor_map, src_tensor_map, c0, args...);
  default:

    return;
  }
}

// Single entry for gmem<->L1/L0A copies. Splits by direction: g2l uses the
// padding-aware valid load (DTEG2L); l2g uses the store path. The merged
// TensorMap carries rshape/pshape (+ rstrides/pstrides); with no padding
// (pshape==rshape) every branch degrades to the original behavior.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
DTE(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map, int32_t c0,
    Ts... args) {
  raisa::DTEDirectionType dir = DTEDirection(dst_tensor_map, src_tensor_map);
  if (dir == raisa::DTEDirectionType::g2l) {
    return DTEG2L(dst_tensor_map, src_tensor_map, c0, args...);
  } else if (dir == raisa::DTEDirectionType::l2g) {
    auto DLTransformTy =
        DataLayoutTransformType(dst_tensor_map, src_tensor_map);
    switch (DLTransformTy) {
    case raisa::DataLayoutTransformTy::compact2compact:
      // C2CDTE routes the l2g direction internally.
      return C2CDTE(dst_tensor_map, src_tensor_map, c0, args...);
    case raisa::DataLayoutTransformTy::compact2align:
      return C2ADTEL2G(dst_tensor_map, src_tensor_map, c0, args...);
    case raisa::DataLayoutTransformTy::align2compact:
      return A2CDTEL2G(dst_tensor_map, src_tensor_map, c0, args...);
    case raisa::DataLayoutTransformTy::align2align:
      return A2ADTEL2G(dst_tensor_map, src_tensor_map, c0, args...);
    default:

      return;
    }
  }
  return;
}

} // namespace raisa

#endif /* __RPU_RAISA_DTE_H */
