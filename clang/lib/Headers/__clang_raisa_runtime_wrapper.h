/*===---- __clang_raisa_runtime_wrapper.h - RAISA runtime support -------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

/*
 * WARNING: This header is intended to be directly -include'd by
 * the compiler and is not supposed to be included by users.
 *
 * RAISA headers are implemented in a way that currently makes it
 * impossible for user code to #include directly when compiling with
 * Clang. They present different view of RAISA-supplied functions
 * depending on where in NVCC's compilation pipeline the headers are
 * included. Neither of these modes provides function definitions with
 * correct attributes, so we use preprocessor to force the headers
 * into a form that Clang can use.
 *
 * Similarly to NVCC which -include's raisa_runtime.h, Clang -include's
 * this file during every RAISA compilation.
 */

#ifndef __CLANG_RAISA_RUNTIME_WRAPPER_H__
#define __CLANG_RAISA_RUNTIME_WRAPPER_H__

#include <stdint.h>

#if defined(__RAISA__) && defined(__clang__)

#ifndef __RAISA_DEVICE_DECL__
#define __RAISA_DEVICE_DECL__                                                   \
  __device__ __attribute__((always_inline)) __attribute__((no_vector))
#endif

#include "__clang_raisa_builtin_vars.h"
#include "__clang_raisa_device_functions.h"
#ifdef NDEBUG
  #define RAISA_ABORT(cond, fmt, ...) ((void)0)
#else
  #define RAISA_ABORT(cond, fmt, ...)                                                 \
  do {                                                                                \
      if (!(cond)) {                                                                  \
          printf("RAISA ABORT: " fmt " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__);\
          KRT_ABORT();                                                                \
      }                                                                               \
  } while (0)
#endif
#endif // __RAISA__
#endif // __CLANG_RAISA_RUNTIME_WRAPPER_H__
