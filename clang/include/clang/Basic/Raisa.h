//===--- Raisa.h - Utilities for compiling RAISA code  ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_RAISA_H
#define LLVM_CLANG_BASIC_RAISA_H

#include "clang/Basic/OffloadArch.h"

#define RAISA_RUNTIME_API_MODULE_LOAD "txRegisterModule"
#define RAISA_RUNTIME_API_MODULE_UNLOAD "txUnregisterModule"
#define RAISA_RUNTIME_API_REGISTER_FUNCTION "txRegisterFunction"
#define RAISA_RUNTIME_API_REGISTER_VARIABLE "txRegisterVariable"
#define RAISA_RUNTIME_API_LAUNCH "txLaunchKernel"
#define RAISA_RUNTIME_API_CLUSTER_LAUNCH "txLaunchClusterKernel"
#define RAISA_RUNTIME_API_PUSH_CONFIG "txPushCallConfiguration"
#define RAISA_RUNTIME_API_POP_CONFIG "txPopCallConfiguration"

namespace llvm {
class StringRef;
class Twine;
class VersionTuple;
} // namespace llvm

namespace clang {

enum class RaisaVersion {
  UNKNOWN,
  RAISA_10,
  NEW = 10000,
};

const char *RaisaVersionToString(RaisaVersion  V);

// Input is "Major.Minor"
RaisaVersion RaisaStringToVersion(const llvm::Twine &S);

enum class RAISAFunctionTarget {
  Device,
  Global,
  Host,
  HostDevice,
  InvalidTarget
};

/// Get the earliest RaisaVersion that supports the given OffloadArch.
RaisaVersion RaisaMinVersionForOffloadArch(OffloadArch A);

/// Get the latest RaisaVersion that supports the given OffloadArch.
RaisaVersion RaisaMaxVersionForOffloadArch(OffloadArch A);

//  Various SDK-dependent features that affect RAISA compilation
enum class RaisaFeature {
  RAISA_USES_NEW_LAUNCH,
  RAISA_USES_FATBIN_REGISTER_END,
};

RaisaVersion ToRaisaVersion(llvm::VersionTuple);
bool RaisaFeatureEnabled(llvm::VersionTuple, RaisaFeature);
bool RaisaFeatureEnabled(RaisaVersion, RaisaFeature);

} // namespace clang

#endif
