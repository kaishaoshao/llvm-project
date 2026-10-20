/*===----------- raisa_builtin_tma.h - RPU built-in tma ---------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RPU_BUILTIN_TMA_H
#define __RPU_BUILTIN_TMA_H

#include "__clang_raisa_runtime_wrapper.h"
#include "__clang_raisa_dte.h"
#include "__clang_raisa_lsu.h"

namespace raisa {

namespace device {

/*
 * global_to_L1shared/global_to_L0Ashared
 *   DTEGather / DTEGatherScatterG2L / DTEIndexGather
 * L1shared_to_global/L0Ashared_to_global
 *   DTEScatter / DTEGatherScatterL2G / DTEIndexScatter
 *
 * The merged TensorMap carries both the valid (rshape/rstrides) and padded
 * (pshape/pstrides) views: g2l loads copy only the valid elements and step over
 * padding, while with no padding (pshape==rshape) every path degrades to the
 * original behavior.
 */
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) void
cp_async_tensor(TensorMap<T> &dst_tensor_map, TensorMap<T> &src_tensor_map,
                int32_t c0 = 0, Ts... args) {
  return DTE<T>(dst_tensor_map, src_tensor_map, c0, args...);
}

/*
 * L1 Shared –> L0A Shared
 * L1 Shared–> L0W Shared
 * L0A Shared–> L1 Shared
 * L0A Shared–> L0W Shared
 * L1 Shared–> L1 Shared
 * L0A Shared–> L0A Shared
 *
 * L1 -> L0A/L0W copies move only the valid (rshape) elements and step over the
 * alignment padding; with no padding (pshape==rshape) this degrades to the
 * original copy.
 */
template <typename T, typename... Ts>
__device__ __attribute__((__always_inline__)) void
cp_async_shared_tensor(TensorMap<T> &dst_tensor_map,
                       TensorMap<T> &src_tensor_map, int32_t c0 = 0,
                       Ts... args) {
  return LSU<T>(dst_tensor_map, src_tensor_map, c0, args...);
}

template <typename T>
__device__ __attribute__((__always_inline__)) void
tx_l0a_memset(T *dst, T value, uint32_t ENum) {
  int32_t int_value = 0;
  __builtin_memcpy(&int_value, &value, sizeof(T));
  if constexpr (sizeof(T) == 1)
    int_value = int_value | (int_value << 8) | (int_value << 16) | (int_value << 24);
  else if constexpr (sizeof(T) == 2)
    int_value = int_value | (int_value << 16);
  return LsuRdmamemset((int64_t)(dst), int_value, ENum, sizeof(T));
}

template <typename T>
__device__ __attribute__((__always_inline__)) void
tx_l0w_memset(T *dst, T value, uint32_t ENum) {
  int32_t int_value = 0;
  __builtin_memcpy(&int_value, &value, sizeof(T));
  if constexpr (sizeof(T) == 1)
    int_value = int_value | (int_value << 8) | (int_value << 16) | (int_value << 24);
  else if constexpr (sizeof(T) == 2)
    int_value = int_value | (int_value << 16);
  return LsuRdmamemset((int64_t)(dst), int_value, ENum, sizeof(T));
}

template <typename T>
__device__ __attribute__((__always_inline__)) void
tx_l1_memset(T *dst, T value, uint32_t ENum) {
  int32_t int_value = 0;
  __builtin_memcpy(&int_value, &value, sizeof(T));
  if constexpr (sizeof(T) == 1)
    int_value = int_value | (int_value << 8) | (int_value << 16) | (int_value << 24);
  else if constexpr (sizeof(T) == 2)
    int_value = int_value | (int_value << 16);
  return LsuWdmamemset((int64_t)(dst), int_value, ENum, sizeof(T));
}

} // namespace device
} // namespace raisa

#endif /* __RPU_BUILTIN_TMA_H */
