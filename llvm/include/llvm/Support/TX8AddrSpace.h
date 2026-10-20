//===---------------- TX8AddrSpace.h -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// RPU address space definition
///
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_RPUADDRSPACE_H
#define LLVM_SUPPORT_RPUADDRSPACE_H

namespace llvm {
namespace RPUAS {

enum AddressSpace : unsigned {
  GENERIC_ADDRESS_SPACE = 0,
  UNIFORM_GENERIC_ADDRESS_SPACE = 1,
  L1SHARED_ADDRESS_SPACE = 2,
  L0ASHARED_ADDRESS_SPACE = 3,
  L0WSHARED_ADDRESS_SPACE = 4,
  PRIVATE_ADDRESS_SPACE = 5,
};

// According to official PTX Writer's Guide, DWARF debug information should
// contain DW_AT_address_class attribute for all variables and parameters.
// It's required for cuda-gdb to be able to properly reflect the memory space
// of variable address. Acceptable address class codes are listed in this enum.
//
// More detailed information:
// https://docs.nvidia.com/cuda/ptx-writers-guide-to-interoperability/index.html#cuda-specific-dwarf-definitions
enum DWARF_AddressSpace : unsigned {
  DWARF_ADDR_code_space = 1,
  DWARF_ADDR_reg_space = 2,
  DWARF_ADDR_sreg_space = 3,
  DWARF_ADDR_generic_space = 4,
  DWARF_ADDR_gmem_space = 5,
  DWARF_ADDR_l1shared_space = 6,
  DWARF_ADDR_l0ashared_space = 7,
  DWARF_ADDR_l0wshared_space = 8,
};

} // end namespace RPUAS
} // end namespace llvm

#endif // LLVM_SUPPORT_RPUADDRSPACE_H
