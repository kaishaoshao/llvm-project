//===-- TX8MCInfo.h - TX8 CT encoding conversion layer ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TX8 ("TSM X82") does not encode the standard RVV encoding.  Assembly is
// first encoded as standard RVV and then translated into the CT ("custom
// translated") encoding space (opcode 0x7b), and disassembly is translated
// back CT -> RVV before the standard decoder tables are consulted.
//
// This header is a reconstruction; the vendor does not install it.
//
// Vendor behaviour on an unsupported instruction is
// `report_fatal_error(Message, /*GenCrashDiag=*/true)` -- a hard process
// abort, not a normal diagnostic.  That is reproduced on purpose.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_MCTARGETDESC_TX8MCINFO_H
#define LLVM_LIB_TARGET_RISCV_MCTARGETDESC_TX8MCINFO_H

#include <cstdint>

namespace llvm {

class TX8MCInfo {
public:
  /// Translate an assembly-encoded RVV instruction word into its TX8 CT
  /// encoding.  Returns the input unchanged for words the CT space does not
  /// translate.  Aborts the process for the officially unsupported shapes.
  static uint64_t tryConvertRVVToCTEncoding(uint32_t Word);

  /// Translate a TX8 CT instruction word back into the RVV encoding that the
  /// standard disassembler tables decode.  Returns the input unchanged when
  /// there is no translation.
  static uint32_t tryConvertCTToRVVEncoding(uint32_t Word);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_RISCV_MCTARGETDESC_TX8MCINFO_H
