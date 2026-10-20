/*===---- raisa_builtin_vars.h - RPU built-in variables ---------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __RPU_BUILTIN_VARS_H
#define __RPU_BUILTIN_VARS_H

struct dim3;

#define __RPU_DEVICE_BUILTIN_INTRINSIC(FIELD, INTRINSIC)                       \
  __declspec(property(get = __fetch_builtin_##FIELD)) unsigned int FIELD;      \
  static inline __attribute__((always_inline))                                 \
  __attribute__((device)) unsigned int __fetch_builtin_##FIELD(void) {         \
    return INTRINSIC;                                                          \
  }

#define __RPU_DEVICE_BUILTIN_FUNCTION(FIELD, FUNCTION)                         \
  __declspec(property(get = __fetch_builtin_##FIELD)) unsigned int FIELD;      \
  static inline __attribute__((always_inline))                                 \
  __attribute__((device)) unsigned int __fetch_builtin_##FIELD(void) {         \
    return FUNCTION;                                                           \
  }

#if __cplusplus >= 201103L
#define __DELETE = delete
#else
#define __DELETE
#endif

// Make sure nobody can create instances of the special variable types.
#define __RPU_DISALLOW_BUILTINVAR_ACCESS(TypeName)                             \
  __attribute__((device)) TypeName() __DELETE;                                 \
  __attribute__((device)) TypeName(const TypeName &) __DELETE;                 \
  __attribute__((device)) void operator=(const TypeName &) const __DELETE;     \
  __attribute__((device)) TypeName *operator&() const __DELETE

struct __raisa_builtin_threadIdx_t {
  __RPU_DEVICE_BUILTIN_INTRINSIC(x, __riscv_read_reg_tid_x());
  __RPU_DEVICE_BUILTIN_INTRINSIC(y, __riscv_read_reg_tid_y());
  __RPU_DEVICE_BUILTIN_INTRINSIC(z, __riscv_read_reg_tid_z());
  __attribute__((device)) operator dim3() const;

private:
  __RPU_DISALLOW_BUILTINVAR_ACCESS(__raisa_builtin_threadIdx_t);
};

struct __raisa_builtin_blockIdx_t {
  __RPU_DEVICE_BUILTIN_INTRINSIC(x, __riscv_read_reg_ctaid_x());
  __RPU_DEVICE_BUILTIN_INTRINSIC(y, __riscv_read_reg_ctaid_y());
  __RPU_DEVICE_BUILTIN_INTRINSIC(z, __riscv_read_reg_ctaid_z());
  __attribute__((device)) operator dim3() const;

private:
  __RPU_DISALLOW_BUILTINVAR_ACCESS(__raisa_builtin_blockIdx_t);
};

static __attribute__((device)) __attribute__((always_inline)) int raisa_ntid_y() { return 1; }
static __attribute__((device)) __attribute__((always_inline)) int raisa_ntid_z() { return 1; }

struct __raisa_builtin_blockDim_t {
  __RPU_DEVICE_BUILTIN_FUNCTION(x, __riscv_read_reg_ntid_x());
  __RPU_DEVICE_BUILTIN_FUNCTION(y, raisa_ntid_y());
  __RPU_DEVICE_BUILTIN_FUNCTION(z, raisa_ntid_z());
  __attribute__((device)) operator dim3() const;

private:
  __RPU_DISALLOW_BUILTINVAR_ACCESS(__raisa_builtin_blockDim_t);
};

struct __raisa_builtin_gridDim_t {
  __RPU_DEVICE_BUILTIN_INTRINSIC(x, __riscv_read_reg_nctaid_x());
  __RPU_DEVICE_BUILTIN_INTRINSIC(y, __riscv_read_reg_nctaid_y());
  __RPU_DEVICE_BUILTIN_INTRINSIC(z, __riscv_read_reg_nctaid_z());
  __attribute__((device)) operator dim3() const;

private:
  __RPU_DISALLOW_BUILTINVAR_ACCESS(__raisa_builtin_gridDim_t);
};

#define __RPU_BUILTINVAR_ATTR                                                  \
  extern const __attribute__((device)) __attribute__((weak))

__RPU_BUILTINVAR_ATTR __raisa_builtin_threadIdx_t threadIdx;
__RPU_BUILTINVAR_ATTR __raisa_builtin_blockIdx_t blockIdx;
__RPU_BUILTINVAR_ATTR __raisa_builtin_blockDim_t blockDim;
__RPU_BUILTINVAR_ATTR __raisa_builtin_gridDim_t gridDim;

#undef __RPU_DEVICE_BUILTIN
#undef __RPU_BUILTINVAR_ATTR
#undef __RPU_DISALLOW_BUILTINVAR_ACCESS
#undef __DELETE

#endif /* __RPU_BUILTIN_VARS_H */
