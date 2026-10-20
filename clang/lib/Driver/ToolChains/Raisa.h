//===--- Raisa.h - RAISA Tool and ToolChain Implementations -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_RAISA_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_RAISA_H

#include "clang/Basic/Raisa.h"
#include "clang/Driver/Action.h"
#include "clang/Driver/Multilib.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/VersionTuple.h"
#include <bitset>
#include <set>
#include <vector>

namespace clang {
namespace driver {

/// A class to find a viable RAISA installation.
class LLVM_LIBRARY_VISIBILITY RaisaInstallationDetector {
private:
  const Driver &D;
  bool IsValid = false;
  RaisaVersion Version = RaisaVersion::UNKNOWN;

  std::string InstallPath;
  std::string BinPath;
  std::string LibDevicePath;
  std::string IncludePath;
  llvm::StringMap<std::string> LibDeviceMap;

  mutable std::bitset<(int)OffloadArch::LAST> ArchsWithBadVersion;

public:
  RaisaInstallationDetector(const Driver &D, const llvm::Triple &HostTriple,
                            const llvm::opt::ArgList &Args);

  void AddRaisaIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const;

  /// Emit an error if Version does not support the given Arch.
  void CheckRaisaVersionSupportsArch(OffloadArch Arch) const;

  /// Check whether we detected a valid RAISA install.
  bool isValid() const { return IsValid; }

  /// Print information about the detected RAISA installation.
  void print(raw_ostream &OS) const;

  /// Get the detected RAISA install's version.
  RaisaVersion version() const { return Version; }

  /// Get the detected RAISA install path.
  StringRef getInstallPath() const { return InstallPath; }

  /// Get the detected RAISA bin path.
  StringRef getBinPath() const { return BinPath; }

  /// Get the detected RAISA include path.
  StringRef getIncludePath() const { return IncludePath; }

  /// Get the detected RAISA libdevice path.
  StringRef getLibDevicePath() const { return LibDevicePath; }

  /// Get the detected libdevice file for the given RPU architecture.
  std::string getLibDeviceFile(StringRef Gpu) const {
    return LibDeviceMap.lookup(Gpu);
  }

  void WarnIfUnsupportedVersion() const;
};

namespace tools {

namespace TSRPU {

/// Runs the RAISA device linker ("tsrpu-link"), which stitches the device
/// object files produced by tsrpuas into the single offload binary that gets
/// embedded into the host object with -fraisa-include-rpubinary.
class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("TSRPU::Linker", "tsrpu-link", TC) {}

  bool hasIntegratedCPP() const override { return false; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;

private:
  void constructLinkCommand(Compilation &C, const JobAction &JA,
                            const InputInfoList &Inputs,
                            const InputInfo &Output,
                            const llvm::opt::ArgList &Args) const;
  void constructLldCommand(Compilation &C, const JobAction &JA,
                           const InputInfoList &Inputs, const InputInfo &Output,
                           const llvm::opt::ArgList &Args) const;
};

void getTSRPUTargetFeatures(const Driver &D, const llvm::Triple &Triple,
                            const llvm::opt::ArgList &Args,
                            std::vector<StringRef> &Features);

} // end namespace TSRPU

} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY TSRPUToolChain : public ToolChain {
public:
  TSRPUToolChain(const Driver &D, const llvm::Triple &Triple,
                 const llvm::Triple &HostTriple,
                 const llvm::opt::ArgList &Args);
  TSRPUToolChain(const Driver &D, const llvm::Triple &Triple,
                 const llvm::opt::ArgList &Args);

  llvm::opt::DerivedArgList *
  TranslateArgs(const llvm::opt::DerivedArgList &Args, StringRef BoundArch,
                Action::OffloadKind DeviceOffloadKind) const override;
  void
  addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                        llvm::opt::ArgStringList &CC1Args,
                        Action::OffloadKind DeviceOffloadKind) const override;

  bool useIntegratedAs() const override { return true; }
  bool isCrossCompiling() const override { return true; }
  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool HasNativeLLVMSupport() const override { return true; }
  bool isPICDefaultForced() const override { return false; }
  bool SupportsProfiling() const override { return false; }
  bool IsMathErrnoDefault() const override { return false; }

  bool supportsDebugInfoOption(const llvm::opt::Arg *A) const override;

  unsigned GetDefaultDwarfVersion() const override { return 2; }
  unsigned getMaxDwarfVersion() const override { return 2; }

  Expected<SmallVector<std::string>>
  getSystemRPUArchs(const llvm::opt::ArgList &Args) const override;

  /// The detected RAISA installation.
  RaisaInstallationDetector RaisaInstallation;

protected:
  Tool *buildLinker() const override;
};

class LLVM_LIBRARY_VISIBILITY RaisaToolChain : public TSRPUToolChain {
public:
  RaisaToolChain(const Driver &D, const llvm::Triple &Triple,
                 const ToolChain &HostTC, const llvm::opt::ArgList &Args);

  const llvm::Triple *getAuxTriple() const override {
    return &HostTC.getTriple();
  }
  bool HasNativeLLVMSupport() const override { return false; }
  std::string getInputFilename(const InputInfo &Input) const override;
  llvm::opt::DerivedArgList *
  TranslateArgs(const llvm::opt::DerivedArgList &Args, StringRef BoundArch,
                Action::OffloadKind DeviceOffloadKind) const override;
  void
  addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                        llvm::opt::ArgStringList &CC1Args,
                        Action::OffloadKind DeviceOffloadKind) const override;
  llvm::DenormalMode getDefaultDenormalModeForType(
      const llvm::opt::ArgList &DriverArgs, const JobAction &JA,
      const llvm::fltSemantics *FPType = nullptr) const override;
  void AddRaisaIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const override;
  void addClangWarningOptions(llvm::opt::ArgStringList &CC1Args) const override;
  CXXStdlibType GetCXXStdlibType(const llvm::opt::ArgList &Args) const override;
  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void AddClangCXXStdlibIncludeArgs(
      const llvm::opt::ArgList &Args,
      llvm::opt::ArgStringList &CC1Args) const override;
  void AddIAMCUIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const override;
  SanitizerMask getSupportedSanitizers() const override;
  VersionTuple computeMSVCVersion(const Driver *D,
                                  const llvm::opt::ArgList &Args) const override;

  unsigned GetDefaultDwarfVersion() const override { return 2; }

  const ToolChain &HostTC;

protected:
  Tool *buildLinker() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_RAISA_H
