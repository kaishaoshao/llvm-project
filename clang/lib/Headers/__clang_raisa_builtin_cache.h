/*===----------- raisa_builtin_cache.h - RPU built-in cache -----------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RPU_BUILTIN_CACHE_H
#define __RPU_BUILTIN_CACHE_H

namespace raisa {

enum class CacheMode : int32_t {
  normal = 0,
  gatherscatter = 1,
  all = 2,
};

namespace device {

__device__ __attribute__((__always_inline__)) static void
L0CacheInvalid(int64_t addr, CacheMode mode, int32_t datalen, int32_t segSize,
               int32_t Itera0, int32_t Stride0, int32_t Itera1, int32_t Stride1,
               int32_t Itera2, int32_t Stride2) {
  int64_t param0 = ((int64_t)segSize << 32) | (int64_t)(uint32_t)datalen;
  if (mode == CacheMode::gatherscatter) {
    __builtin_riscv_l0_set_src_stride((int64_t)Stride2, (int64_t)Stride1,
                                      (int64_t)Stride0);
    __builtin_riscv_l0_set_src_itera((int64_t)Itera2, (int64_t)Itera1,
                                     (int64_t)Itera0);
    __builtin_riscv_l0_invalid(param0, 1, addr);
  } else if (mode == CacheMode::normal) {
    __builtin_riscv_l0_invalid(param0, 0, addr);
  } else if (mode == CacheMode::all) {
    __builtin_riscv_l0_invalid(param0, 2, addr);
  }
}

__device__ __attribute__((__always_inline__)) static void
L0CacheFlush(int64_t addr, CacheMode mode, int32_t datalen, int32_t segSize,
             int32_t Itera0, int32_t Stride0, int32_t Itera1, int32_t Stride1,
             int32_t Itera2, int32_t Stride2) {
  int64_t param0 = ((int64_t)segSize << 32) | (int64_t)(uint32_t)datalen;
  if (mode == CacheMode::gatherscatter) {
    __builtin_riscv_l0_set_src_stride((int64_t)Stride2, (int64_t)Stride1,
                                      (int64_t)Stride0);
    __builtin_riscv_l0_set_src_itera((int64_t)Itera2, (int64_t)Itera1,
                                     (int64_t)Itera0);
    __builtin_riscv_l0_flush(param0, 1, addr);
  } else if (mode == CacheMode::normal) {
    __builtin_riscv_l0_flush(param0, 0, addr);
  } else if (mode == CacheMode::all) {
    __builtin_riscv_l0_flush(param0, 2, addr);
  }
}

__device__ __attribute__((__always_inline__)) static void
L1CacheInvalid(int64_t addr, CacheMode mode, int32_t datalen, int32_t segSize,
               int32_t Itera0, int32_t Stride0, int32_t Itera1, int32_t Stride1,
               int32_t Itera2, int32_t Stride2) {
  int64_t param0 = ((int64_t)segSize << 32) | (int64_t)(uint32_t)datalen;
  if (mode == CacheMode::gatherscatter) {
    __builtin_riscv_l1_set_src_stride((int64_t)Stride2, (int64_t)Stride1,
                                      (int64_t)Stride0);
    __builtin_riscv_l1_set_src_itera((int64_t)Itera2, (int64_t)Itera1,
                                     (int64_t)Itera0);
    __builtin_riscv_l1_invalid(param0, 1, addr);
  } else if (mode == CacheMode::normal) {
    __builtin_riscv_l1_invalid(param0, 0, addr);
  } else if (mode == CacheMode::all) {
    __builtin_riscv_l1_invalid(param0, 2, addr);
  }
}

__device__ __attribute__((__always_inline__)) static void
L1CacheFlush(int64_t addr, CacheMode mode, int32_t datalen, int32_t segSize,
             int32_t Itera0, int32_t Stride0, int32_t Itera1, int32_t Stride1,
             int32_t Itera2, int32_t Stride2) {
  int64_t param0 = ((int64_t)segSize << 32) | (int64_t)(uint32_t)datalen;
  if (mode == CacheMode::gatherscatter) {
    __builtin_riscv_l1_set_src_stride((int64_t)Stride2, (int64_t)Stride1,
                                      (int64_t)Stride0);
    __builtin_riscv_l1_set_src_itera((int64_t)Itera2, (int64_t)Itera1,
                                     (int64_t)Itera0);
    __builtin_riscv_l1_flush(param0, 1, addr);
  } else if (mode == CacheMode::normal) {
    __builtin_riscv_l1_flush(param0, 0, addr);
  } else if (mode == CacheMode::all) {
    __builtin_riscv_l1_flush(param0, 2, addr);
  }
}

template <typename T>
__device__ __attribute__((__always_inline__)) void
cache_invalid(T* addr, CacheMode cache_mode, int32_t datalen, int32_t segSize = 0,
              int32_t Itera0 = 0, int32_t stride0 = 0, int32_t Itera1 = 0,
              int32_t stride1 = 0, int32_t Itera2 = 0, int32_t stride2 = 0) {

    L0CacheInvalid((int64_t)addr, cache_mode, datalen, segSize, Itera0, stride0, Itera1, stride1, Itera2, stride2);
    L1CacheInvalid((int64_t)addr, cache_mode, datalen, segSize, Itera0, stride0, Itera1, stride1, Itera2, stride2);

}

__device__ __attribute__((__always_inline__)) void
cache_all_invalid() {
    L0CacheInvalid(0, CacheMode::all, 0, 0, 0, 0, 0, 0, 0, 0);
    L1CacheInvalid(0, CacheMode::all, 0, 0, 0, 0, 0, 0, 0, 0);
}

template <typename T>
__device__ __attribute__((__always_inline__)) void
cache_flush(T* addr, CacheMode cache_mode, int32_t datalen, int32_t segSize = 0,
            int32_t Itera0 = 0, int32_t stride0 = 0, int32_t Itera1 = 0,
            int32_t stride1 = 0, int32_t Itera2 = 0, int32_t stride2 = 0) {

    L0CacheFlush((int64_t)addr, cache_mode, datalen, segSize, Itera0, stride0, Itera1, stride1, Itera2, stride2);
    L1CacheFlush((int64_t)addr, cache_mode, datalen, segSize, Itera0, stride0, Itera1, stride1, Itera2, stride2);

}

__device__ __attribute__((__always_inline__)) void
cache_all_flush() {
    L0CacheFlush(0, CacheMode::all, 0, 0, 0, 0, 0, 0, 0, 0);
    L1CacheFlush(0, CacheMode::all, 0, 0, 0, 0, 0, 0, 0, 0);
}
} // namespace device
} // namespace raisa

#endif /* __RPU_BUILTIN_CACHE_H */
