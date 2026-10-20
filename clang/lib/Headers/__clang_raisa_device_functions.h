/*===---------- raisa_device_functions.h - RPU RAISA DTE ------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===----------------------------------------------------------------------===
 */

#ifndef __CLANG_RAISA_DEVICE_FUNCTIONS_H_
#define __CLANG_RAISA_DEVICE_FUNCTIONS_H_

extern "C" {
#include "krt.h"

__device__ __attribute__((no_vector)) int printf(const char *, ...);
}

__device__ __forceinline__ static uint32_t __laneid() {
  // now the threadIdx.x ranges from 0 to 127, and warpSize is 128
  return threadIdx.x & 0x7f;
}

template <typename T>
__device__ __forceinline__ T __shfl_sync(__uint128_t mask, T var, uint32_t srcLane, uint32_t width = 128) {
  __shared__ T smem[128];
  smem[__laneid()] = var;
  uint32_t groupId = __laneid() / width;

  return smem[srcLane + groupId * width];
}

template <typename T>
__device__ __forceinline__ T __shfl_up_sync(__uint128_t mask, T var, uint32_t delta, uint32_t width = 128) {
  __shared__ T smem[128];
  smem[__laneid()] = var;
  uint32_t groupStart = __laneid() - __laneid() % width;
  uint32_t srcLane = __laneid() >= delta + groupStart ? __laneid() - delta : __laneid();

  return smem[srcLane];
}

template <typename T>
__device__ __forceinline__ T __shfl_down_sync(__uint128_t mask, T var, uint32_t delta, uint32_t width = 128) {
  __shared__ T smem[128];
  smem[__laneid()] = var;
  uint32_t groupEnd = __laneid() + width - __laneid() % width;
  uint32_t srcLane = __laneid() + delta < groupEnd ? __laneid() + delta : __laneid();
  return smem[srcLane];
}

template <typename T>
__device__ __forceinline__ T __shfl_xor_sync(__uint128_t mask, T var, uint32_t laneMask, uint32_t width = 128) {
  __shared__ T smem[128];
  smem[__laneid()] = var;
  uint32_t idxInGroup = __laneid() % width;
  uint32_t groupStart = __laneid() - idxInGroup;
  uint32_t groupSrc = idxInGroup ^ laneMask + groupStart;

  return smem[groupSrc];
}

__device__ __forceinline__ static void __threadfence() {
  // l0_flush + l1_flush to make sure that all mem write
  // operations have done
  __builtin_riscv_l0_flush(0, 2, 0);
  __builtin_riscv_l1_flush(0, 2, 0);
}

__device__ __forceinline__ static void __threadfence_block() {
  // now give an empty function
}

static __device__ __forceinline__ void __syncthreads() {
  // now give an empty function for that there is not tile scheduler
}

// To use for preload and prevent breaking prefetch. declaration only (no body),
// so isSafeAndProfitableToSinkLoad conservatively treats the call as a memory
// barrier.
__attribute__((noinline)) __device__ void __prefetch_barrier();

#endif // __CLANG_RAISA_DEVICE_FUNCTIONS_H_