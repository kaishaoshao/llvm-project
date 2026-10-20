/*===-------------------- raisa_lsu.h - RPU RAISA LSU-------------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RPU_RAISA_LSU_H
#define __RPU_RAISA_LSU_H

namespace raisa {

#define ALIGN(X, A) (((X) + (A)-1) & ~((A)-1))

/*
 * L1 Shared –> L0A Shared
 * L1 Shared –> L0W Shared
 */
__device__ __attribute__((__always_inline__)) static void
LsuRdmaGather(int64_t dst, int64_t src, int32_t segSize, int32_t itera0,
              int32_t stride0, int32_t itera1, int32_t stride1, int32_t itera2,
              int32_t stride2, int32_t srcLen, int32_t dstLen) {
  int64_t p0 = ((int64_t)stride0 << 32) | itera0;
  int64_t p1 = ((int64_t)stride1 << 32) | itera1;
  int64_t p2 = ((int64_t)stride2 << 32) | itera2;
  __builtin_riscv_lsu_set_rdma_src_itera_stride(p2, p1, p0);

  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_rdma_gather(dst, src + ((int64_t)segSize << 40));
}

__device__ __attribute__((__always_inline__)) static void
LsuRdmaScatter(int64_t dst, int64_t src, int32_t segSize, int itera0,
               int stride0, int itera1, int stride1, int itera2, int stride2,
               int32_t srcLen, int32_t dstLen) {
  int64_t p0 = ((int64_t)stride0 << 32) | itera0;
  int64_t p1 = ((int64_t)stride1 << 32) | itera1;
  int64_t p2 = ((int64_t)stride2 << 32) | itera2;
  __builtin_riscv_lsu_set_rdma_dst_itera_stride(p2, p1, p0);

  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_rdma_scatter(dst, src + ((int64_t)segSize << 40));
}

__device__ __attribute__((__always_inline__)) static void
LsuRdmaIndexGather(int64_t dst, int64_t src0, int32_t src1, int32_t segSize,
                   int32_t indexElemNum, int32_t srcLen, int32_t dstLen) {
  int64_t param0 = src0 + ((int64_t)segSize << 40);
  int64_t param1 = src1 + ((int64_t)indexElemNum << 40);
  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_rdma_index_gather(dst, param1, param0);
}

__device__ __attribute__((__always_inline__)) static void
LsuRdmaGatherScatter(int64_t dst, int64_t src, int32_t segSize,
                     int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                     int32_t srcStride1, int32_t srcItera2, int32_t srcStride2,
                     int32_t dstItera0, int32_t dstStride0, int32_t dstItera1,
                     int32_t dstStride1, int32_t dstItera2, int32_t dstStride2,
                     int32_t srcLen, int32_t dstLen) {
  int64_t srcp0 = ((int64_t)srcStride0 << 32) | srcItera0;
  int64_t srcp1 = ((int64_t)srcStride1 << 32) | srcItera1;
  int64_t srcp2 = ((int64_t)srcStride2 << 32) | srcItera2;
  __builtin_riscv_lsu_set_rdma_src_itera_stride(srcp2, srcp1, srcp0);

  int64_t dstp0 = ((int64_t)dstStride0 << 32) | dstItera0;
  int64_t dstp1 = ((int64_t)dstStride1 << 32) | dstItera1;
  int64_t dstp2 = ((int64_t)dstStride2 << 32) | dstItera2;
  __builtin_riscv_lsu_set_rdma_dst_itera_stride(dstp2, dstp1, dstp0);

  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_rdma_gather_scatter(dst, src + ((int64_t)segSize << 40));
}

// Optimized RDMA gather_scatter. Callers always pass the full two-sided
// descriptor plus a mode; when one side is contiguous (no shuffle) the call
// collapses to a cheaper single-sided primitive:
//   lsus  (src contiguous) -> LsuRdmaScatter, driven by the dst descriptor
//   lsug  (dst contiguous) -> LsuRdmaGather,  driven by the src descriptor
//   lsugs (both shuffled)  -> full LsuRdmaGatherScatter
__device__ __attribute__((__always_inline__)) static void
LsuRdmaGatherScatterOpt(int64_t dst, int64_t src, int32_t segSize,
                        int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                        int32_t srcStride1, int32_t srcItera2,
                        int32_t srcStride2, int32_t dstItera0,
                        int32_t dstStride0, int32_t dstItera1,
                        int32_t dstStride1, int32_t dstItera2,
                        int32_t dstStride2, int32_t srcLen, int32_t dstLen,
                        raisa::LSUTy mode) {
  if (mode == raisa::LSUTy::lsus) {
    LsuRdmaScatter(dst, src, segSize, dstItera0, dstStride0, dstItera1,
                   dstStride1, dstItera2, dstStride2, srcLen, dstLen);
  } else if (mode == raisa::LSUTy::lsug) {
    LsuRdmaGather(dst, src, segSize, srcItera0, srcStride0, srcItera1,
                  srcStride1, srcItera2, srcStride2, srcLen, dstLen);
  } else {
    LsuRdmaGatherScatter(dst, src, segSize, srcItera0, srcStride0, srcItera1,
                         srcStride1, srcItera2, srcStride2, dstItera0,
                         dstStride0, dstItera1, dstStride1, dstItera2,
                         dstStride2, srcLen, dstLen);
  }
}

__device__ __attribute__((__always_inline__)) static void
LsuRdmamemcpy(int64_t dst, int64_t src0, int32_t srcLen) {
  __builtin_riscv_lsu_set_length(srcLen, 0, srcLen);
  __builtin_riscv_lsu_rdma_mem_copy(dst, src0 + ((int64_t)srcLen << 40));
}

__device__ __attribute__((__always_inline__)) static void
LsuRdmamemset(int64_t dst, int32_t value, int32_t ENum, int32_t byteSize) {
  int64_t param0 =
      (int64_t)value + ((int64_t)byteSize << 36) + ((int64_t)ENum << 40);
  int64_t dataLength = ENum * byteSize;
  __builtin_riscv_lsu_set_length(dataLength, 0, 0);
  __builtin_riscv_lsu_rdma_mem_set(dst, param0);
}

/*
 * L0A Shared–> L1 Shared
 */
__device__ __attribute__((__always_inline__)) static void
LsuWdmaGather(int64_t dst, int64_t src, int32_t segSize, int32_t itera0,
              int32_t stride0, int32_t itera1, int32_t stride1, int32_t itera2,
              int32_t stride2, int32_t srcLen, int32_t dstLen) {
  int64_t p0 = ((int64_t)stride0 << 32) | itera0;
  int64_t p1 = ((int64_t)stride1 << 32) | itera1;
  int64_t p2 = ((int64_t)stride2 << 32) | itera2;
  __builtin_riscv_lsu_set_wdma_src_itera_stride(p2, p1, p0);

  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_wdma_gather(dst, src + ((int64_t)segSize << 40));
}

__device__ __attribute__((__always_inline__)) static void
LsuWdmaScatter(int64_t dst, int64_t src, int32_t segSize, int32_t itera0,
               int32_t stride0, int32_t itera1, int32_t stride1, int32_t itera2,
               int32_t stride2, int32_t srcLen, int32_t dstLen) {
  int64_t p0 = ((int64_t)stride0 << 32) | itera0;
  int64_t p1 = ((int64_t)stride1 << 32) | itera1;
  int64_t p2 = ((int64_t)stride2 << 32) | itera2;
  __builtin_riscv_lsu_set_wdma_dst_itera_stride(p2, p1, p0);

  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_wdma_scatter(dst, src + ((int64_t)segSize << 40));
}

__device__ __attribute__((__always_inline__)) static void
LsuWdmaIndexScatter(int64_t dst, int64_t src0, int32_t src1, int32_t segSize,
                    int32_t indexElemNum, int32_t srcLen, int32_t dstLen) {
  int64_t param0 = src0 + ((int64_t)segSize << 40);
  int64_t param1 = src1 + ((int64_t)indexElemNum << 40);
  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_wdma_index_scatter(dst, param1, param0);
}

__device__ __attribute__((__always_inline__)) static void
LsuWdmaGatherScatter(int64_t dst, int64_t src, int32_t segSize,
                     int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                     int32_t srcStride1, int32_t srcItera2, int32_t srcStride2,
                     int32_t dstItera0, int32_t dstStride0, int32_t dstItera1,
                     int32_t dstStride1, int32_t dstItera2, int32_t dstStride2,
                     int32_t srcLen, int32_t dstLen) {
  int64_t srcp0 = ((int64_t)srcStride0 << 32) | srcItera0;
  int64_t srcp1 = ((int64_t)srcStride1 << 32) | srcItera1;
  int64_t srcp2 = ((int64_t)srcStride2 << 32) | srcItera2;
  __builtin_riscv_lsu_set_wdma_src_itera_stride(srcp2, srcp1, srcp0);

  int64_t dstp0 = ((int64_t)dstStride0 << 32) | dstItera0;
  int64_t dstp1 = ((int64_t)dstStride1 << 32) | dstItera1;
  int64_t dstp2 = ((int64_t)dstStride2 << 32) | dstItera2;
  __builtin_riscv_lsu_set_wdma_dst_itera_stride(dstp2, dstp1, dstp0);

  __builtin_riscv_lsu_set_length(dstLen, 0, srcLen);
  __builtin_riscv_lsu_wdma_gather_scatter(dst, src + ((int64_t)segSize << 40));
}

// Optimized WDMA gather_scatter, mirror of LsuRdmaGatherScatterOpt. Callers pass
// the full two-sided descriptor plus a mode; when one side is a single
// contiguous run the call collapses to a cheaper single-sided primitive:
//   lsus  (src contiguous) -> LsuWdmaScatter, driven by the dst descriptor
//   lsug  (dst contiguous) -> LsuWdmaGather,  driven by the src descriptor
//   lsugs (both shuffled)  -> full LsuWdmaGatherScatter
__device__ __attribute__((__always_inline__)) static void
LsuWdmaGatherScatterOpt(int64_t dst, int64_t src, int32_t segSize,
                        int32_t srcItera0, int32_t srcStride0, int32_t srcItera1,
                        int32_t srcStride1, int32_t srcItera2,
                        int32_t srcStride2, int32_t dstItera0,
                        int32_t dstStride0, int32_t dstItera1,
                        int32_t dstStride1, int32_t dstItera2,
                        int32_t dstStride2, int32_t srcLen, int32_t dstLen,
                        raisa::LSUTy mode) {
  if (mode == raisa::LSUTy::lsus) {
    LsuWdmaScatter(dst, src, segSize, dstItera0, dstStride0, dstItera1,
                   dstStride1, dstItera2, dstStride2, srcLen, dstLen);
  } else if (mode == raisa::LSUTy::lsug) {
    LsuWdmaGather(dst, src, segSize, srcItera0, srcStride0, srcItera1,
                  srcStride1, srcItera2, srcStride2, srcLen, dstLen);
  } else {
    LsuWdmaGatherScatter(dst, src, segSize, srcItera0, srcStride0, srcItera1,
                         srcStride1, srcItera2, srcStride2, dstItera0,
                         dstStride0, dstItera1, dstStride1, dstItera2,
                         dstStride2, srcLen, dstLen);
  }
}

__device__ __attribute__((__always_inline__)) static void
LsuWdmamemcpy(int64_t dst, int64_t src, int32_t srcLen) {
  __builtin_riscv_lsu_set_length(srcLen, 0, srcLen);
  __builtin_riscv_lsu_wdma_mem_copy(dst, src + ((int64_t)srcLen << 40));
}

__device__ __attribute__((__always_inline__)) static void
LsuWdmamemset(int64_t dst, int32_t value, int32_t ENum, int32_t byteSize) {
  int64_t param0 =
      (int64_t)value + ((int64_t)byteSize << 36) + ((int64_t)ENum << 40);
  int64_t dataLength = ENum * byteSize;
  __builtin_riscv_lsu_set_length(dataLength, 0, 0);
  __builtin_riscv_lsu_wdma_mem_set(dst, param0);
}

/*
 * L1 Shared–> L1 Shared
 * L0A Shared –> L0W Shared
 */
__device__ __attribute__((__always_inline__)) static void
LsuTdmaMemcpy(int64_t dst, int64_t src, int32_t size) {
  int64_t param0 = ((int64_t)size << 40) | src;
  __builtin_riscv_lsu_set_length(size, 0, size);
  __builtin_riscv_lsu_tdma_mem_copy(dst, param0);
}

/*
 * L0A Shared–> L0A Shared
 */
// CHW -> HWC
__device__ __attribute__((__always_inline__)) static void
LsuTdmaNchw2nhwc(int64_t dst, int64_t src, int32_t byteSize, int32_t size,
                 int32_t H, int32_t W, int32_t C) {
  int64_t param0 = src + ((int64_t)byteSize << 36);
  int64_t param1 = C + (C << 16) + ((int64_t)W << 32) + ((int64_t)H << 48);
  __builtin_riscv_lsu_set_tdma_hwc(0, 0, param1);
  __builtin_riscv_lsu_set_length(size, 0, size);
  __builtin_riscv_lsu_tdma_nchw2nhwc(dst, param0);
}

// HWC -> CHW
__device__ __attribute__((__always_inline__)) static void
LsuTdmaNhwc2nchw(int64_t dst, int64_t src, int32_t byteSize, int32_t size,
                 int32_t H, int32_t W, int32_t C) {
  int64_t param0 = src + (byteSize << 36);
  int64_t param1 = C + (C << 16) + ((int64_t)W << 32) + ((int64_t)H << 48);
  __builtin_riscv_lsu_set_tdma_hwc(0, 0, param1);
  __builtin_riscv_lsu_set_length(size, 0, size);
  __builtin_riscv_lsu_tdma_nhwc2nchw(dst, param0);
}

__device__ __attribute__((__always_inline__)) static void
LsuTdmaBankScatter(int64_t dst, int64_t src, int32_t size,
                   int32_t data_piece_size) {
  int64_t param0 =
      src + ((int64_t)data_piece_size << 36) + ((int64_t)size << 40);
  __builtin_riscv_lsu_set_length(size, 0, size);
  __builtin_riscv_lsu_tdma_bank_scatter(dst, param0);
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::LSUDirectionType
LSUDirection(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if ((src_tensor_map.tensor_as == TensorAS::L1shared &&
       dst_tensor_map.tensor_as == TensorAS::L0Ashared) ||
      (src_tensor_map.tensor_as == TensorAS::L1shared &&
       dst_tensor_map.tensor_as == TensorAS::shared)) {
    return raisa::LSUDirectionType::l12l0a;
  } else if ((src_tensor_map.tensor_as == TensorAS::shared &&
              dst_tensor_map.tensor_as == TensorAS::L1shared) ||
             (src_tensor_map.tensor_as == TensorAS::L0Ashared &&
              dst_tensor_map.tensor_as == TensorAS::L1shared)) {
    return raisa::LSUDirectionType::l0a2l1;
  } else if ((src_tensor_map.tensor_as == TensorAS::L1shared &&
              dst_tensor_map.tensor_as == TensorAS::L0Wshared)) {
    return raisa::LSUDirectionType::l12l0w;
  } else if ((src_tensor_map.tensor_as == TensorAS::L1shared &&
              dst_tensor_map.tensor_as == TensorAS::L1shared)) {
    return raisa::LSUDirectionType::l1tol1;
  } else if ((src_tensor_map.tensor_as == TensorAS::L0Ashared &&
              dst_tensor_map.tensor_as == TensorAS::L0Ashared) ||
             (src_tensor_map.tensor_as == TensorAS::shared &&
              dst_tensor_map.tensor_as == TensorAS::shared)) {
    return raisa::LSUDirectionType::l0a2l0a;
  } else if ((src_tensor_map.tensor_as == TensorAS::L0Ashared &&
              dst_tensor_map.tensor_as == TensorAS::L0Wshared) ||
             (src_tensor_map.tensor_as == TensorAS::shared &&
              dst_tensor_map.tensor_as == TensorAS::L0Wshared)) {
    return raisa::LSUDirectionType::l0a2l0w;
  }

  return raisa::LSUDirectionType::unknown;
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::LSUTy
LSUL12L0Type(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if (src_tensor_map.indexElemNum) {
    return raisa::LSUTy::lsuig;
  } else if (!dst_tensor_map.IsContinuous() && !src_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsugs;
  } else if (!dst_tensor_map.IsContinuous() && src_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsus;
  } else if (dst_tensor_map.IsContinuous() && !src_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsug;
  } else if (dst_tensor_map.IsContinuous() && src_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsucp;
  }
  return raisa::LSUTy::unknown;
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::LSUTy
LSUL02L1Type(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if (dst_tensor_map.indexElemNum) {
    return raisa::LSUTy::lsuis;
  } else if (!dst_tensor_map.IsContinuous() && !src_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsugs;
  } else if (src_tensor_map.IsContinuous() && !dst_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsus;
  } else if (!src_tensor_map.IsContinuous() && dst_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsug;
  } else if (src_tensor_map.IsContinuous() && dst_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsucp;
  }
  return raisa::LSUTy::unknown;
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::LSUTy
LSUL12L1Type(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if (src_tensor_map.IsContinuous() && dst_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsucp;
  }
  return raisa::LSUTy::unknown;
}

template <typename T>
__device__ __attribute__((__always_inline__)) static raisa::LSUTy
LSUL0A2L0WType(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map) {
  if (src_tensor_map.IsContinuous() && dst_tensor_map.IsContinuous()) {
    return raisa::LSUTy::lsucp;
  }
  return raisa::LSUTy::unknown;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2CLSUL12L0(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
            int32_t c0, Ts... args) {
  if (LSUL12L0Type(dst_tensor_map, src_tensor_map) == raisa::LSUTy::lsucp) {
    int32_t offset = GetOffset(src_tensor_map, c0, args...);
    return LsuRdmamemcpy(dst_tensor_map.GetAddr(),
                         src_tensor_map.GetAddr() + offset,
                         src_tensor_map.getDataByteSize());
  } else if (LSUL12L0Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsug) {
    int32_t offset = GetOffset(src_tensor_map, c0, args...);
    return LsuRdmaGather(
        dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
        dst_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
        src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
        src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
        src_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else if (LSUL12L0Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsus) {
    int32_t offset = GetOffset(src_tensor_map, c0, args...);
    return LsuRdmaScatter(
        dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
        src_tensor_map.getSegmentByteSize(), dst_tensor_map.getIteras0(),
        dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
        dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
        dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else if (LSUL12L0Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsugs) {
    int32_t offset = GetOffset(src_tensor_map, c0, args...);
    return LsuRdmaGatherScatter(
        dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
        src_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
        src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
        src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
        src_tensor_map.getStrides2(), dst_tensor_map.getIteras0(),
        dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
        dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
        dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else if (LSUL12L0Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsuig) { // indexGather is only for l12l0a
    int32_t offset = GetOffset(src_tensor_map, c0, args...);
    return LsuRdmaIndexGather(
        dst_tensor_map.GetAddr(), src_tensor_map.GetAddr() + offset,
        *src_tensor_map.indexAddr, src_tensor_map.getSegmentByteSize(),
        *src_tensor_map.indexElemNum, src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else {

  }
  return;
}

template <typename T>
__device__ __attribute__((__always_inline__)) static int64_t
LSUAlignOffset(TensorMap<T> &tensor_map, const uint32_t max_c_align) {
  int64_t offset = tensor_map.TypeSize() * max_c_align *
                   tensor_map.GetRealDimSize() /
                   tensor_map.GetRealInnerDimSize();
  if (tensor_map.tensorRank == 3) {
    offset = offset / tensor_map.rshape[0];
  }
  // dst_addr_i should be aligned by 256B when to l0a
  if (tensor_map.tensor_as == TensorAS::L0Ashared ||
      tensor_map.tensor_as == TensorAS::shared) {
    offset = ALIGN(offset, 256);
  }
  // dst_addr_i should be aligned by 512B when to l0w
  if (tensor_map.tensor_as == TensorAS::L0Wshared) {
    offset = ALIGN(offset, 512);
  }
  if (tensor_map.tensorRank == 3) {
    offset = offset * tensor_map.rshape[0];
  }
  return offset;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2CLSUL02L1(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
            int32_t c0, Ts... args) {
  if (LSUL02L1Type(dst_tensor_map, src_tensor_map) == raisa::LSUTy::lsucp) {
    int32_t offset = GetOffset(dst_tensor_map, c0, args...);
    return LsuWdmamemcpy(dst_tensor_map.GetAddr() + offset,
                         src_tensor_map.GetAddr(),
                         src_tensor_map.getDataByteSize());
  } else if (LSUL02L1Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsug) {
    int32_t offset = GetOffset(dst_tensor_map, c0, args...);
    return LsuWdmaGather(
        dst_tensor_map.GetAddr() + offset, src_tensor_map.GetAddr(),
        dst_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
        src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
        src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
        src_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else if (LSUL02L1Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsus) {
    int32_t offset = GetOffset(dst_tensor_map, c0, args...);
    return LsuWdmaScatter(
        dst_tensor_map.GetAddr() + offset, src_tensor_map.GetAddr(),
        src_tensor_map.getSegmentByteSize(), dst_tensor_map.getIteras0(),
        dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
        dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
        dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else if (LSUL02L1Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsugs) {
    int32_t offset = GetOffset(dst_tensor_map, c0, args...);
    return LsuWdmaGatherScatter(
        dst_tensor_map.GetAddr() + offset, src_tensor_map.GetAddr(),
        src_tensor_map.getSegmentByteSize(), src_tensor_map.getIteras0(),
        src_tensor_map.getStrides0(), src_tensor_map.getIteras1(),
        src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
        src_tensor_map.getStrides2(), dst_tensor_map.getIteras0(),
        dst_tensor_map.getStrides0(), dst_tensor_map.getIteras1(),
        dst_tensor_map.getStrides1(), dst_tensor_map.getIteras2(),
        dst_tensor_map.getStrides2(), src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else if (LSUL02L1Type(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsuis) {
    int32_t offset = GetOffset(dst_tensor_map, c0, args...);
    return LsuWdmaIndexScatter(
        dst_tensor_map.GetAddr() + offset, src_tensor_map.GetAddr(),
        *dst_tensor_map.indexAddr, src_tensor_map.getSegmentByteSize(),
        *dst_tensor_map.indexElemNum, src_tensor_map.getDataByteSize(),
        dst_tensor_map.getDataByteSize());
  } else {

  }
  return;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2ALSUL02L1(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
            int32_t c0, Ts... args) {

  int32_t offset = GetOffset(dst_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr();
  int64_t dst_addr = dst_tensor_map.GetAddr() + offset;

  const uint32_t max_c_align = 1024u / src_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(
      src_tensor_map.rshape[src_tensor_map.tensorRank - 1], max_c_align >> 1);
  uint32_t align_num = aligned_c / max_c_align;
  int64_t dst_offset = LSUAlignOffset(dst_tensor_map, max_c_align);

  int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
  // valid element count of the compact inner dim
  int32_t valid_c = src_tensor_map.rshape[src_tensor_map.tensorRank - 1];
  // Merge the full blocks into one call (block index on the unused itera0:
  // compact src steps by one block, align dst by dst_offset). Every full block
  // has the same segSize.
  if (align_num >= 1) {
    // No tail + real-continuous compact src -> single contiguous run, so it
    // degrades to a pure scatter driven by the dst descriptor. Guard on
    // valid_c (not aligned_c): when valid_c is not a multiple of max_c_align
    // the compact src row stride (valid_c) differs from align_num*segSize, so
    // the full block is not a single run and must stay gather_scatter.
    bool src_one_run =
        src_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
    raisa::LSUTy mode = src_one_run ? raisa::LSUTy::lsus : raisa::LSUTy::lsugs;
    LsuWdmaGatherScatterOpt(
        dst_addr, src_addr, segSize, align_num,
        src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
        src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
        src_tensor_map.getStrides2(), align_num, (int32_t)dst_offset,
        dst_tensor_map.getIteras1(), dst_tensor_map.TypeSize() * max_c_align,
        dst_tensor_map.getIteras2(),
        dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
        src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
        mode);
  }

  if (aligned_c % max_c_align) {
    int32_t segSize = src_tensor_map.TypeSize() * (max_c_align >> 1);
    int64_t src_addr_tail =
        src_addr + align_num * src_tensor_map.TypeSize() * max_c_align;
    int64_t dst_addr_tail = dst_addr + align_num * dst_offset;
    LsuWdmaGatherScatter(
        dst_addr_tail, src_addr_tail, segSize, 1,
        src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
        src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
        src_tensor_map.getIteras2(), src_tensor_map.getStrides2(), 1,
        dst_tensor_map.TypeSize() * (max_c_align >> 1),
        dst_tensor_map.getIteras1(),
        dst_tensor_map.TypeSize() * (max_c_align >> 1),
        dst_tensor_map.getIteras2(),
        dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() *
            (max_c_align >> 1),
        src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
  }
  return;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2CLSUL02L1(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
            int32_t c0, Ts... args) {

  int32_t offset = GetOffset(dst_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr();
  int64_t dst_addr = dst_tensor_map.GetAddr() + offset;

  const uint32_t max_c_align = 1024u / src_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(
      src_tensor_map.rshape[src_tensor_map.tensorRank - 1], max_c_align >> 1);

  int64_t src_offset = LSUAlignOffset(src_tensor_map, max_c_align);
  int64_t src_tail_offset = LSUAlignOffset(src_tensor_map, max_c_align >> 1);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = src_tensor_map.rshape[src_tensor_map.tensorRank - 1];

  // rank<=2: merge full blocks (block index on itera0; align src steps by
  // src_offset, compact dst by one block). Tail keeps its per-case src row
  // stride: max_c_align (trimmed Case A) / max_c_align>>1 (separate Case B).
  if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
      // No tail + real-continuous compact dst -> single contiguous run, so it
      // degrades to a pure gather driven by the src descriptor.
      bool dst_one_run =
          dst_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
      raisa::LSUTy mode =
          dst_one_run ? raisa::LSUTy::lsug : raisa::LSUTy::lsugs;
      LsuWdmaGatherScatterOpt(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          n_full, dst_tensor_map.TypeSize() * max_c_align,
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          mode);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = src_tensor_map.TypeSize() * align_size;
      int64_t dst_addr_tail =
          dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      uint32_t row = (aligned_c % max_c_align) ? (max_c_align >> 1) : max_c_align;
      LsuWdmaGatherScatter(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * row, src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * row, src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * row, 1,
          dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
    }
    return;
  }

  // rank-3: fold the channel-block loop (itera0) and the rshape[0] loop
  // (itera2) into the descriptor; rshape[1] stays on itera1. Case B uses src
  // row stride max_c_align>>1 and src_tail_offset for the rshape[0] step.
  if (src_tensor_map.tensorRank == 3) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
      // No tail + real-continuous compact dst -> single contiguous run in the
      // (block, rshape[1], rshape[0]) order, so it degrades to a pure gather.
      bool dst_one_run =
          dst_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
      raisa::LSUTy mode =
          dst_one_run ? raisa::LSUTy::lsug : raisa::LSUTy::lsugs;
      LsuWdmaGatherScatterOpt(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.rshape[0],
          (int32_t)(src_offset / src_tensor_map.rshape[0]), n_full,
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras1(),
          dst_tensor_map.getStrides1(), dst_tensor_map.rshape[0],
          dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          mode);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = src_tensor_map.TypeSize() * align_size;
      int64_t dst_addr_tail =
          dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      bool caseB = aligned_c % max_c_align;
      uint32_t row = caseB ? (max_c_align >> 1) : max_c_align;
      int32_t j_src = caseB
                          ? (int32_t)(src_tail_offset / src_tensor_map.rshape[0])
                          : (int32_t)(src_offset / src_tensor_map.rshape[0]);
      LsuWdmaGatherScatter(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * row, src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * row, src_tensor_map.rshape[0], j_src, 1,
          dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.rshape[0],
          dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
    }
  }
  return;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2ALSUL02L1(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
            int32_t c0, Ts... args) {
  if (LSUL02L1Type(dst_tensor_map, src_tensor_map) == raisa::LSUTy::lsucp) {
    int32_t offset = GetOffset(dst_tensor_map, c0, args...);
    int64_t src_addr = src_tensor_map.GetAddr();
    int64_t dst_addr = dst_tensor_map.GetAddr() + offset;

    const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
    uint32_t aligned_c = ALIGN(
        dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1], max_c_align >> 1);
    uint32_t align_num = aligned_c / max_c_align;

    int64_t src_offset = LSUAlignOffset(src_tensor_map, max_c_align);
    int64_t src_tail_offset = LSUAlignOffset(src_tensor_map, max_c_align >> 1);
    int64_t dst_offset = LSUAlignOffset(dst_tensor_map, max_c_align);
    int64_t dst_tail_offset = LSUAlignOffset(dst_tensor_map, max_c_align >> 1);
    int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
    // rank<=2: merge the align_num full blocks into one call (block index on
    // itera0; both sides align, so src steps by src_offset and dst by dst_offset,
    // each possibly with a padding gap the hardware honors).
    if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2) {
      if (align_num >= 1) {
        LsuWdmaGatherScatter(
            dst_addr, src_addr, segSize, align_num, (int32_t)src_offset,
            src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
            align_num, (int32_t)dst_offset, dst_tensor_map.getIteras1(),
            dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
      if (aligned_c % max_c_align) {
        int32_t tSegSize = dst_tensor_map.TypeSize() * (max_c_align >> 1);
        int64_t dst_addr_tail = dst_addr + align_num * dst_offset;
        int64_t src_addr_tail = src_addr + align_num * src_offset;
        LsuWdmaGatherScatter(
            dst_addr_tail, src_addr_tail, tSegSize, 1,
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
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
      return;
    }

    // rank-3: fold the channel-block loop (itera0) and the rshape[0] loop
    // (itera2) into the descriptor; rshape[1] stays on itera1. Both sides align,
    // so the rshape[0] step is *_offset / rshape[0] on each side.
    if (src_tensor_map.tensorRank == 3) {
      if (align_num >= 1) {
        LsuWdmaGatherScatter(
            dst_addr, src_addr, segSize, align_num, (int32_t)src_offset,
            src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.rshape[0],
            (int32_t)(src_offset / src_tensor_map.rshape[0]), align_num,
            (int32_t)dst_offset, dst_tensor_map.getIteras1(),
            dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.rshape[0],
            (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
      if (aligned_c % max_c_align) {
        int32_t tSegSize = dst_tensor_map.TypeSize() * (max_c_align >> 1);
        int64_t dst_addr_tail = dst_addr + align_num * dst_offset;
        int64_t src_addr_tail = src_addr + align_num * src_offset;
        LsuWdmaGatherScatter(
            dst_addr_tail, src_addr_tail, tSegSize, 1,
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
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
    }
  }
  return;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2CLSUL012L01(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
              int32_t c0, Ts... args) {
  return LsuTdmaMemcpy(dst_tensor_map.GetAddr(), src_tensor_map.GetAddr(),
                       src_tensor_map.getDataByteSize());
}

// l1 align to l1 align
// l0a align to l0w align
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2ALSUL012L01(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
              int32_t c0, Ts... args) {
  if (LSUL12L1Type(dst_tensor_map, src_tensor_map) == raisa::LSUTy::lsucp) {
    int64_t src_addr = src_tensor_map.GetAddr();
    int64_t dst_addr = dst_tensor_map.GetAddr();

    const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
    uint32_t aligned_c = ALIGN(
        dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1], max_c_align >> 1);

    int64_t src_offset = LSUAlignOffset(src_tensor_map, max_c_align);
    int64_t src_tail_offset = LSUAlignOffset(src_tensor_map, max_c_align >> 1);

    int64_t dst_offset = LSUAlignOffset(dst_tensor_map, max_c_align);
    int64_t dst_tail_offset = LSUAlignOffset(dst_tensor_map, max_c_align >> 1);

    uint32_t align_num = aligned_c / max_c_align;
    // align_num * max_c_align
    for (uint32_t i = 0; i < align_num; ++i) {
      int64_t dst_addr_i = dst_addr + i * dst_offset;
      int64_t src_addr_i = src_addr + i * src_offset;
      int32_t size = dst_tensor_map.getIteras2() * dst_tensor_map.getIteras1() *
                     dst_tensor_map.TypeSize() * max_c_align;
      LsuTdmaMemcpy(dst_addr_i, src_addr_i, size);
    }

    if (aligned_c % max_c_align) {
      int64_t dst_addr_tail = dst_addr + align_num * dst_offset;
      int64_t src_addr_tail = src_addr + align_num * src_offset;
      int32_t size = dst_tensor_map.getIteras2() * dst_tensor_map.getIteras1() *
                     dst_tensor_map.TypeSize() * (max_c_align >> 1);
      LsuTdmaMemcpy(dst_addr_tail, src_addr_tail, size);
    }
  } else if (LSUL0A2L0WType(dst_tensor_map, src_tensor_map) ==
             raisa::LSUTy::lsucp) {
    int64_t src_addr = src_tensor_map.GetAddr();
    int64_t dst_addr = dst_tensor_map.GetAddr();

    const uint32_t max_c_align = 1024u / dst_tensor_map.BitSize();
    uint32_t aligned_c = ALIGN(
        dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1], max_c_align >> 1);
    uint32_t align_num = aligned_c / max_c_align;

    int64_t src_offset = LSUAlignOffset(src_tensor_map, max_c_align);
    int64_t src_tail_offset = LSUAlignOffset(src_tensor_map, max_c_align >> 1);

    int64_t dst_offset = LSUAlignOffset(dst_tensor_map, max_c_align);
    int64_t dst_tail_offset = LSUAlignOffset(dst_tensor_map, max_c_align >> 1);

    int32_t size = dst_tensor_map.getIteras2() * dst_tensor_map.getIteras1() *
                   dst_tensor_map.TypeSize() * max_c_align;
    // align_num * max_c_align
    for (uint32_t i = 0; i < align_num; ++i) {
      if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2) {
        int64_t src_addr_i = src_addr + i * src_offset;
        int64_t dst_addr_i = dst_addr + i * dst_offset;
        LsuTdmaMemcpy(dst_addr_i, src_addr_i, size);
      } else if (src_tensor_map.tensorRank == 3) {
        for (uint32_t j = 0; j < src_tensor_map.rshape[0]; ++j) {
          int64_t src_addr_i = src_addr + i * src_offset +
                               j * src_offset / src_tensor_map.rshape[0];
          int64_t dst_addr_i = dst_addr + i * dst_offset +
                               j * dst_offset / dst_tensor_map.rshape[0];
          LsuTdmaMemcpy(dst_addr_i, src_addr_i, size);
        }
      } else {
      }
    }

    if (aligned_c % max_c_align) {
      int32_t size = dst_tensor_map.getIteras2() * dst_tensor_map.getIteras1() *
                     dst_tensor_map.TypeSize() * (max_c_align >> 1);
      if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2) {
        int64_t src_addr_tail = src_addr + align_num * src_offset;
        int64_t dst_addr_tail = dst_addr + align_num * dst_offset;

        LsuTdmaMemcpy(dst_addr_tail, src_addr_tail, size);
      } else if (src_tensor_map.tensorRank == 3) {
        for (uint32_t j = 0; j < src_tensor_map.rshape[0]; ++j) {
          int64_t src_addr_tail = src_addr + align_num * src_offset +
                                  j * src_tail_offset / src_tensor_map.rshape[0];
          int64_t dst_addr_tail = dst_addr + align_num * dst_offset +
                                  j * dst_tail_offset / dst_tensor_map.rshape[0];
          LsuTdmaMemcpy(dst_addr_tail, src_addr_tail, size);
        }
      } else {
      }
    }
  } else {

  }
  return;
}

// L1 compact -> L0A/L0W align (l12l0). The trailing block only moves the valid
// elements (rshape inner count), so the compact source is never over-read past
// the logical tensor. With no padding this matches the original align tiling.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
C2ALSUL12L0(TensorMap<T> &dst_tensor_map,
                 TensorMap<T> &src_tensor_map, int32_t c0, Ts... args) {
  int32_t offset = GetOffset(src_tensor_map, c0, args...);
  int64_t src_addr = src_tensor_map.GetAddr() + offset;
  int64_t dst_addr = dst_tensor_map.GetAddr();
  const uint32_t max_c_align = 1024u / src_tensor_map.BitSize();
  uint32_t aligned_c = ALIGN(
      src_tensor_map.rshape[src_tensor_map.tensorRank - 1], max_c_align >> 1);

  uint32_t dst_inner_shape = dst_tensor_map.pshape[dst_tensor_map.tensorRank -1];
  if (!dst_tensor_map.HasPadDim())
    dst_inner_shape = (max_c_align >> 1);

  int64_t dst_offset = LSUAlignOffset(dst_tensor_map, max_c_align);
  int64_t dst_tail_offset = LSUAlignOffset(dst_tensor_map, max_c_align >> 1);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = src_tensor_map.rshape[src_tensor_map.tensorRank - 1];

  // rank<=2: merge the full max_c_align blocks into one call (block index on the
  // unused itera0 level: compact src steps by one block, align dst by dst_offset).
  if (dst_tensor_map.tensorRank == 1 || dst_tensor_map.tensorRank == 2) {
    uint32_t n_full = valid_c / max_c_align;

    if (n_full >= 1) {
      int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
      // Degrade to a pure scatter only when the source is a single contiguous
      // run in the folded descriptor's enumeration order. That needs both:
      //   - IsRealContinuous(): the valid data sits on the padded grid with no
      //     extra gaps (no internal padding to step over), and
      //   - no tail (valid_c % max_c_align == 0): otherwise the full-block
      //     region skips each row's tail columns and is no longer contiguous.
      bool src_one_run =
          src_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
      raisa::LSUTy mode = src_one_run ? raisa::LSUTy::lsus : raisa::LSUTy::lsugs;
      LsuRdmaGatherScatterOpt(
          dst_addr, src_addr, segSize, n_full,
          src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), src_tensor_map.getIteras2(),
          src_tensor_map.getStrides2(), n_full, (int32_t)dst_offset,
          dst_tensor_map.getIteras1(), max_c_align * dst_tensor_map.TypeSize(),
          dst_tensor_map.getIteras2(),
          dst_tensor_map.getIteras1() * max_c_align * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          mode);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = src_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail =
          src_addr + n_full * src_tensor_map.TypeSize() * max_c_align;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: dst row stride uses the padded inner shape
        LsuRdmaGatherScatter(
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
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      } else {
        // trimmed-last-block layout: dst row stride stays at max_c_align
        LsuRdmaGatherScatter(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
            src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
            src_tensor_map.getIteras2(), src_tensor_map.getStrides2(), 1,
            max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.getIteras1(),
            max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * max_c_align *
                dst_tensor_map.TypeSize(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
    }
    return;
  }

  // rank-3 L0A/L0W: fold the channel-block loop (itera0) and the rshape[0] loop
  // (itera2) into the descriptor; rshape[1] stays on itera1. Compact src steps
  // by pstrides[0] over rshape[0]; align dst by dst_offset/rshape[0]. The partial
  // tail keeps its per-case dst row stride: max_c_align (trimmed Case A, with
  // dst_offset step) or dst_inner_shape (separate Case B, with dst_tail_offset).
  if (dst_tensor_map.tensorRank == 3) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = src_tensor_map.TypeSize() * max_c_align;
      // No tail + real-continuous compact src -> single contiguous run in the
      // (block, rshape[1], rshape[0]) enumeration order, so it degrades to a
      // pure scatter driven by the dst descriptor.
      bool src_one_run =
          src_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
      raisa::LSUTy mode = src_one_run ? raisa::LSUTy::lsus : raisa::LSUTy::lsugs;
      LsuRdmaGatherScatterOpt(
          dst_addr, src_addr, segSize, n_full,
          src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
          src_tensor_map.getStrides1(), dst_tensor_map.rshape[0],
          src_tensor_map.pstrides[0] * src_tensor_map.TypeSize(), n_full,
          (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          max_c_align * dst_tensor_map.TypeSize(), dst_tensor_map.rshape[0],
          (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          mode);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = src_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail =
          src_addr + n_full * src_tensor_map.TypeSize() * max_c_align;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      bool caseB = aligned_c % max_c_align;
      uint32_t dst_row = caseB ? dst_inner_shape : max_c_align;
      int32_t j_dst = caseB
                          ? (int32_t)(dst_tail_offset / dst_tensor_map.rshape[0])
                          : (int32_t)(dst_offset / dst_tensor_map.rshape[0]);
      LsuRdmaGatherScatter(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.getIteras0() * src_tensor_map.getStrides0(),
          src_tensor_map.getIteras1(), src_tensor_map.getStrides1(),
          dst_tensor_map.rshape[0],
          src_tensor_map.pstrides[0] * src_tensor_map.TypeSize(), 1,
          dst_row * dst_tensor_map.TypeSize(), dst_tensor_map.getIteras1(),
          dst_row * dst_tensor_map.TypeSize(), dst_tensor_map.rshape[0], j_dst,
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
    }
  }
  return;
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2CLSUL12L0(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
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

  int64_t src_offset = LSUAlignOffset(src_tensor_map, max_c_align);
  int64_t src_tail_offset = LSUAlignOffset(src_tensor_map, max_c_align >> 1);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1];

  // rank<=2: merge the full max_c_align blocks into one call (block index on the
  // unused itera0 level: align src steps by src_offset, compact dst by one
  // block).
  if (src_tensor_map.tensorRank == 1 || src_tensor_map.tensorRank == 2) {
    uint32_t n_full = valid_c / max_c_align;

    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      bool dst_one_run =
          dst_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
      raisa::LSUTy mode = dst_one_run ? raisa::LSUTy::lsug : raisa::LSUTy::lsugs;
      LsuRdmaGatherScatterOpt(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          n_full, dst_tensor_map.TypeSize() * max_c_align,
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          mode);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = dst_tensor_map.TypeSize() * align_size;
      int64_t dst_addr_tail =
          dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: src row stride uses the padded inner shape
        LsuRdmaGatherScatter(
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
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      } else {
        // trimmed-last-block layout: src row stride stays at max_c_align
        LsuRdmaGatherScatter(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() *
                max_c_align,
            1, dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
            dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
            dst_tensor_map.getIteras2(), dst_tensor_map.getStrides2(),
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
    }
    return;
  }

  // rank-3: fold the channel-block loop (itera0) and the rshape[0] loop (itera2)
  // into the descriptor; rshape[1] stays on itera1. Align src steps by
  // src_offset/rshape[0], compact dst by rstrides[0]. The partial tail keeps its
  // per-case src row stride: max_c_align (trimmed Case A, src_offset step) or
  // src_inner_shape (separate Case B, src_tail_offset step).
  if (src_tensor_map.tensorRank == 3) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      // No tail + real-continuous compact dst -> single contiguous run in the
      // (block, rshape[1], rshape[0]) enumeration order, so it degrades to a
      // pure gather driven by the src descriptor.
      bool dst_one_run =
          dst_tensor_map.IsRealContinuous() && (valid_c % max_c_align == 0);
      raisa::LSUTy mode = dst_one_run ? raisa::LSUTy::lsug : raisa::LSUTy::lsugs;
      LsuRdmaGatherScatterOpt(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.rshape[0],
          (int32_t)(src_offset / src_tensor_map.rshape[0]), n_full,
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras1(),
          dst_tensor_map.getStrides1(), src_tensor_map.rshape[0],
          dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize(),
          mode);
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = dst_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      int64_t dst_addr_tail =
          dst_addr + n_full * dst_tensor_map.TypeSize() * max_c_align;
      bool caseB = aligned_c % max_c_align;
      uint32_t src_row = caseB ? src_inner_shape : max_c_align;
      int32_t j_src = caseB
                          ? (int32_t)(src_tail_offset / src_tensor_map.rshape[0])
                          : (int32_t)(src_offset / src_tensor_map.rshape[0]);
      LsuRdmaGatherScatter(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * src_row, src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * src_row, src_tensor_map.rshape[0], j_src, 1,
          dst_tensor_map.getIteras0() * dst_tensor_map.getStrides0(),
          dst_tensor_map.getIteras1(), dst_tensor_map.getStrides1(),
          src_tensor_map.rshape[0],
          dst_tensor_map.rstrides[0] * dst_tensor_map.TypeSize(),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
    }
  }
  return;
}

// L1 align -> L0A/L0W align (l12l0). The trailing block only moves the valid
// elements (rshape inner count), so the aligned source padding is never
// read/copied. With no padding this matches the original align-to-align path.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
A2ALSUL12L0(TensorMap<T> &dst_tensor_map,
                 TensorMap<T> &src_tensor_map, int32_t c0, Ts... args) {
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

  int64_t dst_offset = LSUAlignOffset(dst_tensor_map, max_c_align);
  int64_t dst_tail_offset = LSUAlignOffset(dst_tensor_map, max_c_align >> 1);
  int64_t src_offset = LSUAlignOffset(src_tensor_map, max_c_align);
  int64_t src_tail_offset = LSUAlignOffset(src_tensor_map, max_c_align >> 1);

  // valid element count of the inner dim (unpadded logical c)
  int32_t valid_c = dst_tensor_map.rshape[dst_tensor_map.tensorRank - 1];

  // rank<=2: merge the full max_c_align blocks into one call (block index on the
  // unused itera0 level;
  if (dst_tensor_map.tensorRank == 1 || dst_tensor_map.tensorRank == 2) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      LsuRdmaGatherScatter(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getIteras2(),
          src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
          n_full, (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras2(),
          dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
    }
    if (valid_c % max_c_align) {
      int32_t align_size = valid_c - n_full * max_c_align;
      int32_t segSize = dst_tensor_map.TypeSize() * align_size;
      int64_t src_addr_tail = src_addr + n_full * src_offset;
      int64_t dst_addr_tail = dst_addr + n_full * dst_offset;
      if (aligned_c % max_c_align) {
        // separate-tail layout: row stride uses the padded inner shape
        LsuRdmaGatherScatter(
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
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      } else {
        // trimmed-last-block layout: row stride stays at max_c_align
        LsuRdmaGatherScatter(
            dst_addr_tail, src_addr_tail, segSize, 1,
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras1(),
            src_tensor_map.TypeSize() * max_c_align, src_tensor_map.getIteras2(),
            src_tensor_map.getIteras1() * src_tensor_map.TypeSize() * max_c_align,
            1, dst_tensor_map.TypeSize() * max_c_align,
            dst_tensor_map.getIteras1(),
            dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.getIteras2(),
            dst_tensor_map.getIteras1() * dst_tensor_map.TypeSize() * max_c_align,
            src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
      }
    }
    return;
  }

  // rank-3: fold the channel-block loop (itera0) and the rshape[0] loop (itera2)
  // into the descriptor; rshape[1] stays on itera1. Both sides align, so the
  // rshape[0] step is *_offset / rshape[0]. The partial tail keeps its per-case
  // row stride: max_c_align (trimmed Case A, with *_offset step) or *_inner_shape
  // (separate Case B, with *_tail_offset step).
  if (dst_tensor_map.tensorRank == 3) {
    uint32_t n_full = valid_c / max_c_align;
    if (n_full >= 1) {
      int32_t segSize = dst_tensor_map.TypeSize() * max_c_align;
      LsuRdmaGatherScatter(
          dst_addr, src_addr, segSize, n_full, (int32_t)src_offset,
          src_tensor_map.getIteras1(), src_tensor_map.TypeSize() * max_c_align,
          src_tensor_map.rshape[0],
          (int32_t)(src_offset / src_tensor_map.rshape[0]), n_full,
          (int32_t)dst_offset, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * max_c_align, dst_tensor_map.rshape[0],
          (int32_t)(dst_offset / dst_tensor_map.rshape[0]),
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
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
      LsuRdmaGatherScatter(
          dst_addr_tail, src_addr_tail, segSize, 1,
          src_tensor_map.TypeSize() * src_row, src_tensor_map.getIteras1(),
          src_tensor_map.TypeSize() * src_row, src_tensor_map.rshape[0], j_src, 1,
          dst_tensor_map.TypeSize() * dst_row, dst_tensor_map.getIteras1(),
          dst_tensor_map.TypeSize() * dst_row, dst_tensor_map.rshape[0], j_dst,
          src_tensor_map.getDataByteSize(), dst_tensor_map.getDataByteSize());
    }
  }
  return;
}

// L1 -> L0A/L0W copy that only moves the valid elements (rshape) of the
// innermost dimension; with no padding (pshape==rshape) it degrades to a dense
// copy / the original align tiling.
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
LSUL12L0(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
         int32_t c0, Ts... args) {
  auto DLTransformTy = DataLayoutTransformType(dst_tensor_map, src_tensor_map);
  switch (DLTransformTy) {
  case raisa::DataLayoutTransformTy::compact2compact: {
    // C2CLSUL12L0 supports all LSU types; pass the physical view.
    TensorMap<T> dphys = PhysView(dst_tensor_map);
    TensorMap<T> sphys = PhysView(src_tensor_map);
    return C2CLSUL12L0(dphys, sphys, c0, args...);
  }
  case raisa::DataLayoutTransformTy::compact2align:
    return C2ALSUL12L0(dst_tensor_map, src_tensor_map, c0, args...);
  case raisa::DataLayoutTransformTy::align2compact: {
    return A2CLSUL12L0(dst_tensor_map, src_tensor_map, c0, args...);
  }
  case raisa::DataLayoutTransformTy::align2align:
    return A2ALSUL12L0(dst_tensor_map, src_tensor_map, c0, args...);
  default:
    return;
  }
}

template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static void
LSU(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map, int32_t c0,
    Ts... args) {
  raisa::LSUDirectionType lsuDrt = LSUDirection(dst_tensor_map, src_tensor_map);
  auto DLTransformTy = DataLayoutTransformType(dst_tensor_map, src_tensor_map);
  switch (lsuDrt) {
  case raisa::LSUDirectionType::l12l0a:
  case raisa::LSUDirectionType::l12l0w:
    return LSUL12L0(dst_tensor_map, src_tensor_map, c0, args...);
  case raisa::LSUDirectionType::l0a2l1: {
    switch (DLTransformTy) {
    case raisa::DataLayoutTransformTy::compact2compact:
      return C2CLSUL02L1(dst_tensor_map, src_tensor_map, c0, args...);
      break;
    case raisa::DataLayoutTransformTy::compact2align:
      return C2ALSUL02L1(dst_tensor_map, src_tensor_map, c0, args...);
      break;
    case raisa::DataLayoutTransformTy::align2compact:
      return A2CLSUL02L1(dst_tensor_map, src_tensor_map, c0, args...);
      break;
    case raisa::DataLayoutTransformTy::align2align:
      return A2ALSUL02L1(dst_tensor_map, src_tensor_map, c0, args...);
      break;
    default:
      return;
    }
  }
  case raisa::LSUDirectionType::l1tol1:
  case raisa::LSUDirectionType::l0a2l0w: {
    switch (DLTransformTy) {
    case raisa::DataLayoutTransformTy::compact2compact:
      return C2CLSUL012L01(dst_tensor_map, src_tensor_map, c0, args...);
      break;
    case raisa::DataLayoutTransformTy::align2align:
      return A2ALSUL012L01(dst_tensor_map, src_tensor_map, c0, args...);
      break;
    default:
      return;
    }
  }
  default:
    RAISA_ABORT(lsuDrt != raisa::LSUDirectionType::unknown, "LSU unsupported direction: src_as=%d dst_as=%d",
                (int)src_tensor_map.tensor_as,
                (int)dst_tensor_map.tensor_as);
    return;
  }
}

} // namespace raisa

#endif /* __RPU_RAISA_LSU_H */
