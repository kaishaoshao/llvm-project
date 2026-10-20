//===--- Raisa.cpp - Utilities for compiling RAISA code -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/Raisa.h"

#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/VersionTuple.h"

namespace clang {

struct RaisaVersionMapEntry {
  const char *Name;
  RaisaVersion Version;
  llvm::VersionTuple TVersion;
};
#define RAISA_ENTRY(major, minor)                                              \
  {                                                                            \
    #major "." #minor, RaisaVersion::RAISA_##major##minor,                     \
        llvm::VersionTuple(major, minor)                                       \
  }

static const RaisaVersionMapEntry RaisaNameVersionMap[] = {
    RAISA_ENTRY(1, 0),
    {"", RaisaVersion::NEW, llvm::VersionTuple(std::numeric_limits<int>::max())},
    {"unknown", RaisaVersion::UNKNOWN, {}} // End of list tombstone.
};
#undef RAISA_ENTRY

const char *RaisaVersionToString(RaisaVersion V) {
  for (auto *I = RaisaNameVersionMap; I->Version != RaisaVersion::UNKNOWN; ++I)
    if (I->Version == V)
      return I->Name;

  return RaisaVersionToString(RaisaVersion::UNKNOWN);
}

RaisaVersion RaisaStringToVersion(const llvm::Twine &S) {
  std::string VS = S.str();
  for (auto *I = RaisaNameVersionMap; I->Version != RaisaVersion::UNKNOWN; ++I)
    if (I->Name == VS)
      return I->Version;
  return RaisaVersion::UNKNOWN;
}

RaisaVersion ToRaisaVersion(llvm::VersionTuple Version) {
  for (auto *I = RaisaNameVersionMap; I->Version != RaisaVersion::UNKNOWN; ++I)
    if (I->TVersion == Version)
      return I->Version;
  return RaisaVersion::UNKNOWN;
}

RaisaVersion RaisaMinVersionForOffloadArch(OffloadArch A) {
  if (A == OffloadArch::UNKNOWN)
    return RaisaVersion::UNKNOWN;

  return RaisaVersion::RAISA_10;
}

RaisaVersion RaisaMaxVersionForOffloadArch(OffloadArch A) {
  switch (A) {
  case OffloadArch::UNKNOWN:
    return RaisaVersion::UNKNOWN;
  case OffloadArch::TSM_X82:
    return RaisaVersion::RAISA_10;
  default:
    return RaisaVersion::NEW;
  }
}

bool RaisaFeatureEnabled(llvm::VersionTuple Version, RaisaFeature Feature) {
  return RaisaFeatureEnabled(ToRaisaVersion(Version), Feature);
}

bool RaisaFeatureEnabled(RaisaVersion Version, RaisaFeature Feature) {
  switch (Feature) {
  case RaisaFeature::RAISA_USES_NEW_LAUNCH:
  case RaisaFeature::RAISA_USES_FATBIN_REGISTER_END:
    return Version > RaisaVersion::UNKNOWN;
  }
  llvm_unreachable("Unknown RAISA feature.");
}
} // namespace clang
