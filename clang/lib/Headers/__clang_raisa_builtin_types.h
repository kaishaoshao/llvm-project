/*===---- raisa_builtin_types.h - RPU built-in variables --------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RPU_BUILTIN_TYPES_H
#define __RPU_BUILTIN_TYPES_H

namespace raisa {

#define TMAMaxAxis 3

#define ALIGN(X, A) (((X) + (A)-1) & ~((A)-1))

enum class DTEDirectionType : short { unknown = -1, g2l = 0, l2g };

enum class DTETy : short { unknown = -1, dtegors = 0, dtegands, dteigs };

enum class LSUDirectionType : short {
  unknown = -1,
  l12l0a = 0,
  l12l0w,
  l0a2l1,
  l1tol1,
  l0a2l0a,
  l0a2l0w
};

enum class LSUL0A2L0ATy : short { unknown = -1, nchw2nhwc = 0, nhwc2nchw };

enum class LSUTy : short {
  unknown = -1,
  lsug = 0,
  lsus = 1,
  lsugs,
  lsuig,
  lsuis,
  lsucp,
  lsumset
};

enum class TensorAS : short {
  unknown = -1,
  gmem = 0,
  L1shared = 2,
  shared = 3,    // shared and L0Ashared alias
  L0Ashared = 3, // shared and L0Ashared alias
  L0Wshared = 4,
};

enum class DataLayout : short {
  unknown = -1,
  compact = 0,
  align,
};

enum class DataLayoutTransformTy : short {
  unknown = -1,
  compact2compact = 0,
  compact2align = 1,
  align2compact,
  align2align
};

// TensorMap carries both a "real" (valid data) view and a "padded" (physical,
// alignment-padded) view in a single description:
//   rshape   : valid data shape
//   pshape   : actual data shape after padding for alignment
//   rstrides : strides of the valid data
//   pstrides : strides of the actual padded data
// Iteration counts (instIteras) are driven by the valid shape, while memory
// strides (instStrides) are driven by the padded physical layout. When pshape /
// pstrides are not provided they default to rshape / rstrides, i.e. no padding,
// which reproduces the original single-view behavior.
template <typename T> class TensorMap {
public:
  __device__ __attribute__((__always_inline__)) TensorMap() = default;

  __device__ __attribute__((__always_inline__))
  TensorMap(T *tensorAddress, int32_t tensorRank = 0, int32_t *rshape = nullptr,
            int32_t *rstrides = nullptr, int32_t tolalSize = 0,
            int32_t segSize = 0, DataLayout dl = DataLayout::compact,
            int32_t *indexAddr = nullptr, int32_t *indexElemNum = nullptr,
            TensorAS as = TensorAS::unknown, int32_t *pshape = nullptr,
            int32_t *pstrides = nullptr)
      : tensor_as(as), tensorRank(tensorRank), tensorAddress(tensorAddress),
        rshape(rshape), rstrides(rstrides), pshape(pshape),
        pstrides(pstrides), segmentSize(segSize),
        tolalSize(tolalSize), indexAddr(indexAddr), indexElemNum(indexElemNum),
        dl(dl) {
    if (!this->pshape) {
      this->pshape = rshape;
      this->pstrides = rstrides;
      this->hasPadDim = false;
    } else {
      this->hasPadDim = true;
    }
// init inst/tdma info
    is_continuous = tensorRank > 0;
    is_real_continuous = tensorRank > 0;
    int32_t _stride_check = is_continuous ? this->pstrides[tensorRank - 1] : 0;
    int32_t _real_stride_check = _stride_check;
#pragma clang loop unroll(full)
    for (int32_t i = tensorRank - 1; i >= 0; --i) {
      instIteras[tensorRank - 1 - i] = rshape[i];
      instStrides[tensorRank - 1 - i] = this->pstrides[i] * TypeSize();
      if (is_continuous && this->pstrides[i] != _stride_check)
        is_continuous = false;
      if (is_real_continuous && this->rstrides[i] != _real_stride_check)
        is_real_continuous = false;
      if (i > 0) {
        _stride_check = _stride_check * this->pshape[i];
        _real_stride_check = _real_stride_check * this->pshape[i];
      }
    }
#pragma clang loop unroll(full)
    for (int32_t i = tensorRank; i < TMAMaxAxis; ++i) {
      instIteras[i] = 1;
      instStrides[i] = tolalSize * TypeSize();
    }
    segmentSize =
        (segmentSize != 0) ? segmentSize : this->pstrides[tensorRank - 1];

    if (as == TensorAS::L0Ashared) {
      tdmaH = tensorRank > 2 ? rshape[tensorRank - 3] : 1;
      tdmaW = tensorRank > 1 ? rshape[tensorRank - 2] : 1;
      tdmaC = tensorRank > 0 ? rshape[tensorRank - 1] : 1;
    }
  }

  __device__ __attribute__((__always_inline__)) int64_t GetAddr() {
    return (int64_t)(tensorAddress);
  }

  __device__ __attribute__((__always_inline__)) int32_t TypeSize() {
    return sizeof(T);
  }

  __device__ __attribute__((__always_inline__)) int32_t BitSize() {
    return sizeof(T) * 8u;
  }

  __device__ __attribute__((__always_inline__)) int32_t GetRealDimSize() {
    int size = 1;
    for (int i = 0; i < tensorRank; ++i) {
      size *= rshape[i];
    }
    return size;
  }

  __device__ __attribute__((__always_inline__)) int32_t GetPadDimSize() {
    int size = 1;
    for (int i = 0; i < tensorRank; ++i) {
      size *= pshape[i];
    }
    return size;
  }

  __device__ __attribute__((__always_inline__)) int32_t GetRealInnerDimSize() {
    // Error Check: tensorRank < 1
    return rshape[tensorRank - 1];
  }

  __device__ __attribute__((__always_inline__)) int32_t GetPadInnerDimSize() {
    // Error Check: tensorRank < 1
    return pshape[tensorRank - 1];
  }

  // Physical continuity: is the padded buffer densely packed?
  // Judged on pshape/pstrides. Used to decide dense-copy vs strided-copy:
  // with padding this is false, forcing the strided path that steps over
  // padding. With no padding (pshape==rshape, pstrides==rstrides) this matches
  // the original continuity test.
  __device__ __attribute__((__always_inline__)) bool IsContinuous() {
    return is_continuous;
  }
  __device__ __attribute__((__always_inline__)) bool IsRealContinuous() {
    return is_real_continuous;
  }

  __device__ __attribute__((__always_inline__)) int32_t getSegmentSize() {
    return segmentSize;
  }
  __device__ __attribute__((__always_inline__)) int32_t getSegmentByteSize() {
    return segmentSize * sizeof(T);
  }
  __device__ __attribute__((__always_inline__)) int32_t getDataSize() {
    return tolalSize;
  }
  __device__ __attribute__((__always_inline__)) int32_t getDataByteSize() {
    return tolalSize * sizeof(T);
  }
  __device__ __attribute__((__always_inline__)) int32_t getIteras0() {
    return instIteras[0];
  }
  __device__ __attribute__((__always_inline__)) int32_t getIteras1() {
    return instIteras[1];
  }
  __device__ __attribute__((__always_inline__)) int32_t getIteras2() {
    return instIteras[2];
  }
  __device__ __attribute__((__always_inline__)) int32_t getStrides0() {
    return instStrides[0];
  }
  __device__ __attribute__((__always_inline__)) int32_t getStrides1() {
    return instStrides[1];
  }
  __device__ __attribute__((__always_inline__)) int32_t getStrides2() {
    return instStrides[2];
  }
  __device__ __attribute__((__always_inline__)) bool HasPadDim() {
    return hasPadDim;
  }

public:
  TensorAS tensor_as = TensorAS::unknown;
  int32_t tensorRank = 1; // the number of dimensions of the array

  T *tensorAddress = nullptr;

  int32_t *rshape = nullptr;   // valid data shape
  int32_t *rstrides = nullptr; // strides of the valid data
  int32_t *pshape = nullptr;   // padded (physical) data shape
  int32_t *pstrides = nullptr; // strides of the padded (physical) data
  bool hasPadDim = false;      // true if pshape/pstrides were explicitly passed
  // precomputed by the constructor
  bool is_continuous = false;
  bool is_real_continuous = false;

  int32_t instIteras[3];
  int32_t instStrides[3]; // 32bytes aligned when dst is N3D/G3D

  int32_t segmentSize =
      0; // the segmentSize of inner dim, 32bytes aligned when dst is N3D/G3D
  int32_t tolalSize = 0; // the datasize including data skipped by stride

  int32_t *indexAddr{nullptr};
  int32_t *indexElemNum{nullptr};
  LSUL0A2L0ATy LSUL0A2L0AT = LSUL0A2L0ATy::unknown;
  DataLayout dl = DataLayout::unknown;

  int tdmaH = 1;
  int tdmaW = 1;
  int tdmaC = 1;
};

template <typename T> class GmemTensorMap : public TensorMap<T> {
public:
  __device__ __attribute__((__always_inline__))
  GmemTensorMap(T *tensorAddress, int32_t tensorRank = 0,
                int32_t *rshape = nullptr, int32_t *rstrides = nullptr,
                int32_t tolalSize = 0, int32_t segSize = 0,
                DataLayout dl = DataLayout::compact,
                int32_t *indexAddr = nullptr, int32_t *indexElemNum = nullptr,
                int32_t *pshape = nullptr, int32_t *pstrides = nullptr)
      : TensorMap<T>(tensorAddress, tensorRank, rshape, rstrides, tolalSize,
                     segSize, dl, indexAddr, indexElemNum, TensorAS::gmem,
                     pshape, pstrides) {}
};

template <typename T> class L1TensorMap : public TensorMap<T> {
public:
  __device__ __attribute__((__always_inline__))
  L1TensorMap(T *tensorAddress, int32_t tensorRank = 0,
              int32_t *rshape = nullptr, int32_t *rstrides = nullptr,
              int32_t tolalSize = 0, int32_t segSize = 0,
              DataLayout dl = DataLayout::compact, int32_t *indexAddr = nullptr,
              int32_t *indexElemNum = nullptr, int32_t *pshape = nullptr,
              int32_t *pstrides = nullptr)
      : TensorMap<T>(tensorAddress, tensorRank, rshape, rstrides, tolalSize,
                     segSize, dl, indexAddr, indexElemNum, TensorAS::L1shared,
                     pshape, pstrides) {}
};

template <typename T> class L0ATensorMap : public TensorMap<T> {
public:
  __device__ __attribute__((__always_inline__))
  L0ATensorMap(T *tensorAddress, int32_t tensorRank = 0,
               int32_t *rshape = nullptr, int32_t *rstrides = nullptr,
               int32_t tolalSize = 0, int32_t segSize = 0,
               DataLayout dl = DataLayout::compact, int32_t *pshape = nullptr,
               int32_t *pstrides = nullptr)
      : TensorMap<T>(tensorAddress, tensorRank, rshape, rstrides, tolalSize,
                     segSize, dl, nullptr, nullptr, TensorAS::L0Ashared, pshape,
                     pstrides) {}
};

template <typename T> class L0WTensorMap : public TensorMap<T> {
public:
  __device__ __attribute__((__always_inline__))
  L0WTensorMap(T *tensorAddress, int32_t tensorRank = 0,
               int32_t *rshape = nullptr, int32_t *rstrides = nullptr,
               int32_t tolalSize = 0, int32_t segSize = 0,
               DataLayout dl = DataLayout::compact, int32_t *pshape = nullptr,
               int32_t *pstrides = nullptr)
      : TensorMap<T>(tensorAddress, tensorRank, rshape, rstrides, tolalSize,
                     segSize, dl, nullptr, nullptr, TensorAS::L0Wshared, pshape,
                     pstrides) {}
};

template <typename T>
__device__ __attribute__((__always_inline__)) raisa::DataLayoutTransformTy
DataLayoutTransformType(TensorMap<T> &dst_tensor_map,
                        TensorMap<T> &src_tensor_map) {
  if (src_tensor_map.dl == DataLayout::compact &&
      dst_tensor_map.dl == DataLayout::compact) {
    return raisa::DataLayoutTransformTy::compact2compact;
  } else if (src_tensor_map.dl == DataLayout::compact &&
             dst_tensor_map.dl == DataLayout::align) {
    return raisa::DataLayoutTransformTy::compact2align;
  } else if (src_tensor_map.dl == DataLayout::align &&
             dst_tensor_map.dl == DataLayout::compact) {
    return raisa::DataLayoutTransformTy::align2compact;
  } else if (src_tensor_map.dl == DataLayout::align &&
             dst_tensor_map.dl == DataLayout::align) {
    return raisa::DataLayoutTransformTy::align2align;
  }
  return raisa::DataLayoutTransformTy::unknown;
}

// Build a physical-view TensorMap (rstrides=pstrides) so the generic copy paths
// iterate the valid rshape extent and step over padding via the physical
// strides. Numerically identical to the source map's instIteras/instStrides;
// returned by value so callers can override tolalSize without touching the
// original. Used for the align2compact valid delegations.
template <typename T>
__device__ __attribute__((__always_inline__)) static TensorMap<T>
PhysView(TensorMap<T> &m) {
  return TensorMap<T>(m.tensorAddress, m.tensorRank, m.rshape, m.pstrides,
                      m.tolalSize, m.segmentSize, m.dl, m.indexAddr,
                      m.indexElemNum, m.tensor_as);
}

} // namespace raisa

// l0a: considering 256B address aligned for K` and K``
// l0w: considering 512B address aligned for K` and K``
__host__ __device__ __attribute__((__always_inline__)) static int32_t
getAlignByteSize(int32_t H, int32_t W, int32_t typeSize,
                 int32_t addrAlignSize = 32) {
  int32_t max_c_align = 1024u / (typeSize * 8);
  int32_t tail_c_align = max_c_align >> 1;
  int32_t aligned_c = ALIGN(W, tail_c_align);
  return (aligned_c / max_c_align) *
             ALIGN(H * max_c_align * typeSize, addrAlignSize) +
         ((aligned_c % max_c_align)
              ? ALIGN(H * (max_c_align >> 1) * typeSize, addrAlignSize)
              : 0);
}

// l0a: considering 256B address aligned for K` and K``
// l0w: considering 512B address aligned for K` and K``
// 32: min align
__host__ __device__ __attribute__((__always_inline__)) static int32_t
getAlignSize(int32_t H, int32_t W, int32_t typeSize,
             int32_t addrAlignSize = 32) {
  return getAlignByteSize(H, W, typeSize, addrAlignSize) / (typeSize);
}

__host__ __device__ __attribute__((__always_inline__)) static int32_t
getAlignMultiSize(int32_t H, int32_t W, int32_t typeSize,
                  int32_t addrAlignSize = 32) {
  int32_t max_c_align = 1024u / (typeSize * 8);
  int32_t tail_c_align = max_c_align >> 1;
  int32_t aligned_c = ALIGN(W, tail_c_align);
  return (aligned_c / max_c_align) *
         ALIGN(H * max_c_align * typeSize, addrAlignSize);
}

__host__ __device__ __attribute__((__always_inline__)) static int32_t
getAlignTailSize(int32_t H, int32_t W, int32_t typeSize,
                 int32_t addrAlignSize = 32) {
  int32_t max_c_align = 1024u / (typeSize * 8);
  int32_t tail_c_align = max_c_align >> 1;
  int32_t aligned_c = ALIGN(W, tail_c_align);
  return (aligned_c % max_c_align)
             ? ALIGN(H * (max_c_align >> 1) * typeSize, addrAlignSize)
             : 0;
}

__host__ __device__ __attribute__((__always_inline__)) static int32_t
getMultiples(int32_t W, int32_t typeSize) {
  int32_t max_c_align = 1024u / (typeSize * 8);
  int32_t tail_c_align = max_c_align >> 1;
  int32_t aligned_c = ALIGN(W, tail_c_align);
  return aligned_c / max_c_align;
}

__host__ __device__ __attribute__((__always_inline__)) static constexpr int32_t
getMaxAlign(int32_t typeSize) {
  return 1024u / (typeSize * 8);
}

__host__ __device__ __attribute__((__always_inline__)) static int32_t
getTailAlign(int32_t W, int32_t typeSize) {
  int32_t max_c_align = 1024u / (typeSize * 8);
  int32_t tail_c_align = max_c_align >> 1;
  int32_t aligned_c = ALIGN(W, tail_c_align);
  return (aligned_c % max_c_align) ? (max_c_align >> 1) : 0;
}

// Physical addressing uses the padded layout (pstrides / rshape).
template <typename T0, typename T1>
__device__ __attribute__((__always_inline__)) static int32_t
ComputeOffset(raisa::TensorMap<T0> &tensor_map, T1 &firstArgs) {
  if (tensor_map.dl == raisa::DataLayout::compact) {
    return firstArgs * tensor_map.pstrides[tensor_map.tensorRank - 1] *
           tensor_map.TypeSize();
  } else if (tensor_map.dl == raisa::DataLayout::align) {
    // align datalayout cannot support tiling inner dim
    return firstArgs * tensor_map.pstrides[tensor_map.tensorRank - 1] *
           tensor_map.TypeSize();
  } else { // TODO: error check
    return 0;
  }
}

template <typename T0, typename T1, typename... Ts>
__device__ __attribute__((__always_inline__)) static int32_t
ComputeOffset(raisa::TensorMap<T0> &tensor_map, T1 &firstArgs, Ts &...args) {
  int argSize = sizeof...(args);
  if (tensor_map.dl == raisa::DataLayout::compact) {
    return firstArgs *
               tensor_map.pstrides[tensor_map.tensorRank - argSize - 1] *
               tensor_map.TypeSize() +
           ComputeOffset(tensor_map, args...);
  } else if (tensor_map.dl == raisa::DataLayout::align) { // offset only for GMEM
    return firstArgs *
               getAlignByteSize(1, tensor_map.rshape[tensor_map.tensorRank - 1],
                                tensor_map.TypeSize()) *
               tensor_map.pstrides[tensor_map.tensorRank - argSize - 1] /
               tensor_map.rshape[tensor_map.tensorRank - 1] +
           ComputeOffset(tensor_map, args...);
  } else { // TODO: error check
  }
}

// Offset: compact offset
// Offset: align offset
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) static int32_t
GetOffset(raisa::TensorMap<T> &tensor_map, int32_t c0, Ts... args) {
  return ComputeOffset<T, int32_t>(tensor_map, c0, args...);
}

#endif /* __RPU_BUILTIN_TYPES_H */
