//===--- Raisa.cpp - RAISA Tool and ToolChain Implementations -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Raisa.h"
#include "clang/Basic/Raisa.h"
#include "clang/Basic/TargetID.h"
#include "clang/Config/config.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Distro.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Config/llvm-config.h" // for LLVM_HOST_TRIPLE
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/TargetParser.h"
#include <system_error>

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

void RaisaInstallationDetector::WarnIfUnsupportedVersion() const {}

RaisaInstallationDetector::RaisaInstallationDetector(
    const Driver &D, const llvm::Triple &HostTriple,
    const llvm::opt::ArgList &Args)
    : D(D) {
  struct Candidate {
    std::string Path;
    bool StrictChecking;
    Candidate(std::string Path, bool StrictChecking = false)
        : Path(Path), StrictChecking(StrictChecking) {}
  };
  SmallVector<Candidate, 4> Candidates;

  // The RAISA installation can be named explicitly with --raisa-path.
  if (Args.hasArg(options::OPT_raisa_path_EQ))
    Candidates.emplace_back(
        std::string(Args.getLastArgValue(options::OPT_raisa_path_EQ)));

  // --no-offloadlib disables looking for the device library, but not for the
  // installation itself.
  bool NoRaisaLib =
      !Args.hasFlag(options::OPT_offloadlib, options::OPT_no_offloadlib, true);

  auto &FS = D.getVFS();
  for (const auto &Candidate : Candidates) {
    InstallPath = Candidate.Path;
    if (InstallPath.empty() || !FS.exists(InstallPath))
      continue;

    BinPath = InstallPath;
    BinPath.append("/bin");
    IncludePath = InstallPath;
    IncludePath.append("/include");
    LibDevicePath = InstallPath;
    LibDevicePath.append("/txriscv/libdevice");

    if (!(FS.exists(IncludePath) && FS.exists(BinPath)))
      continue;

    bool CheckLibDevice = (!NoRaisaLib || Candidate.StrictChecking);
    if (CheckLibDevice && !FS.exists(LibDevicePath))
      continue;

    // The fork does not parse any SDK version file: the device compilation is
    // always driven with the RAISA 1.0 SDK version (see
    // RaisaToolChain::addClangTargetOptions) and the version checks
    // (WarnIfUnsupportedVersion / CheckRaisaVersionSupportsArch) are stubs.
    IsValid = true;
    break;
  }
}

void RaisaInstallationDetector::AddRaisaIncludeArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  // Add the raisa_wrappers directory to the system include path.  This lets us
  // wrap the runtime headers that ship with the RAISA SDK.
  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> P(D.ResourceDir);
    llvm::sys::path::append(P, "include");
    llvm::sys::path::append(P, "raisa_wrappers");
    CC1Args.push_back("-internal-isystem");
    CC1Args.push_back(DriverArgs.MakeArgString(P));
  }

  if (!DriverArgs.hasFlag(options::OPT_offload_inc,
                          options::OPT_no_offload_inc, true))
    return;

  CC1Args.push_back("-include");
  CC1Args.push_back("__clang_raisa_runtime_wrapper.h");
}

void RaisaInstallationDetector::CheckRaisaVersionSupportsArch(
    OffloadArch Arch) const {}

void RaisaInstallationDetector::print(raw_ostream &OS) const {
  if (isValid())
    OS << "Found RAISA installation: " << InstallPath << ", version "
       << RaisaVersionToString(Version) << "\n";
}



void TSRPU::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  if (JA.getType() == types::TY_LLVM_BC)
    constructLinkCommand(C, JA, Inputs, Output, Args);
  else
    constructLldCommand(C, JA, Inputs, Output, Args);
}

void TSRPU::Linker::constructLinkCommand(Compilation &C, const JobAction &JA,
                                         const InputInfoList &Inputs,
                                         const InputInfo &Output,
                                         const ArgList &Args) const {
  const ToolChain &TC = getToolChain();

  // Link the device bitcode inputs together with llvm-link.
  ArgStringList LlvmLinkArgs{"-r", "-o", Output.getFilename()};
  for (const auto &II : Inputs)
    LlvmLinkArgs.push_back(II.getFilename());

  StringRef TargetID = Args.getLastArgValue(options::OPT_mcpu_EQ);
  AddStaticDeviceLibsLinking(C, *this, JA, Inputs, Args, LlvmLinkArgs, "riscv64",
                             TargetID, /*IsBitCodeSDL=*/true);

  const char *LlvmLink = Args.MakeArgString(TC.GetProgramPath("llvm-link"));
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         LlvmLink, LlvmLinkArgs, Inputs,
                                         Output));
}

void TSRPU::Linker::constructLldCommand(Compilation &C, const JobAction &JA,
                                        const InputInfoList &Inputs,
                                        const InputInfo &Output,
                                        const ArgList &Args) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();

  ArgStringList LldArgs{"-m"};

  // The LTO plugin of lld picks up the RPU target features.
  std::vector<StringRef> Features;
  getTSRPUTargetFeatures(D, TC.getTriple(), Args, Features);
  if (!Features.empty()) {
    std::string MAttrString = "-plugin-opt=-mattr=";
    for (auto OneFeature : unifyTargetFeatures(Features)) {
      MAttrString.append(OneFeature.str());
      if (OneFeature != Features.back())
        MAttrString.append(",");
    }
    LldArgs.push_back(Args.MakeArgString(MAttrString));
  }

  LldArgs.push_back("elf64lriscv");

  // Forward -mllvm=<x> as -plugin-opt=<x>, as lld's LTO plugin expects.
  for (const Arg *A : Args.filtered(options::OPT_mllvm))
    LldArgs.push_back(
        Args.MakeArgString(Twine("-plugin-opt=") + A->getValue(0)));

  if (D.isSaveTempsEnabled())
    LldArgs.push_back("-save-temps");

  addLinkerCompressDebugSectionsOption(TC, Args, LldArgs);

  LldArgs.push_back("--whole-archive");

  // Forward -Xoffload-linker=<x> to the device linker; a wrapped -mllvm option
  // is rewritten as -plugin-opt.
  for (const Arg *A : Args.filtered(options::OPT_Xoffload_linker)) {
    StringRef ArgVal = A->getValue(1);
    auto SplitArg = ArgVal.split("-mllvm=");
    if (!SplitArg.second.empty())
      LldArgs.push_back(
          Args.MakeArgString(Twine("-plugin-opt=") + SplitArg.second));
    else
      LldArgs.push_back(Args.MakeArgString(ArgVal));
    A->claim();
  }

  LldArgs.push_back("-shared");

  if (Arg *A = Args.getLastArg(options::OPT_flto, options::OPT_flto_EQ)) {
    if (A->getOption().matches(options::OPT_flto_EQ) &&
        A->getValue() == StringRef("thin"))
      LldArgs.push_back("-plugin-opt=thinlto");

    // The optimization level matches CompilerInvocation.cpp.
    StringRef OOpt = "2";
    if (Arg *O = Args.getLastArg(options::OPT_O_Group)) {
      if (O->getOption().matches(options::OPT_O4) ||
          O->getOption().matches(options::OPT_Ofast))
        OOpt = "3";
      else if (O->getOption().matches(options::OPT_O))
        OOpt = O->getValue();
      else if (O->getOption().matches(options::OPT_O0))
        OOpt = "0";
    }
    LldArgs.push_back(Args.MakeArgString(Twine("--lto-O") + OOpt));
  }

  LldArgs.push_back("-o");
  LldArgs.push_back(Output.getFilename());

  for (const auto &II : Inputs)
    LldArgs.push_back(II.getFilename());

  StringRef TargetID = Args.getLastArgValue(options::OPT_mcpu_EQ);
  AddStaticDeviceLibsLinking(C, *this, JA, Inputs, Args, LldArgs, "tsm82",
                             TargetID, /*IsBitCodeSDL=*/true);

  LldArgs.push_back("--no-whole-archive");

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::None(),
      Args.MakeArgString(TC.GetProgramPath("ld.lld")), LldArgs, Inputs, Output));
}

void TSRPU::getTSRPUTargetFeatures(const Driver &D, const llvm::Triple &Triple,
                                   const ArgList &Args,
                                   std::vector<StringRef> &Features) {
  // The RPU target features are derived from the target ID selected with
  // --raisa-rpu-arch (-mcpu/-march), e.g. "tsm-x82" or "tsm-x82:xtsm82s+".
  handleTargetFeaturesGroup(D, Triple, Args, Features,
                            options::OPT_m_tsrpu_Features_Group);

  StringRef RPUArch = Args.getLastArgValue(options::OPT_mcpu_EQ);
  if (RPUArch.empty())
    RPUArch = Args.getLastArgValue(options::OPT_march_EQ);
  if (RPUArch.empty())
    return;

  // Emit a +/- feature for every feature the processor knows about, using the
  // feature map of the requested target ID.
  llvm::StringMap<bool> FeatureMap;
  if (parseTargetID(Triple, RPUArch, &FeatureMap)) {
    for (StringRef Feature :
         getAllPossibleTargetIDFeatures(Triple, RPUArch.split(':').first)) {
      std::string FeatureStr =
          (Twine(FeatureMap.lookup(Feature) ? "+" : "-") + Feature).str();
      Features.push_back(Args.MakeArgString(FeatureStr));
    }
  }
}

TSRPUToolChain::TSRPUToolChain(const Driver &D, const llvm::Triple &Triple,
                               const llvm::Triple &HostTriple,
                               const ArgList &Args)
    : ToolChain(D, Triple, Args), RaisaInstallation(D, HostTriple, Args) {
  if (RaisaInstallation.isValid())
    getProgramPaths().push_back(std::string(RaisaInstallation.getBinPath()));
  // Lookup binaries into the driver directory, this is used to
  // discover the 'tsrpu-arch' executable.
  getProgramPaths().push_back(getDriver().Dir);
}

/// We only need the host triple to locate the RAISA binary utilities, use the
/// system's default triple if not provided.
TSRPUToolChain::TSRPUToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : TSRPUToolChain(D, Triple, llvm::Triple(LLVM_HOST_TRIPLE), Args) {}

llvm::opt::DerivedArgList *
TSRPUToolChain::TranslateArgs(const llvm::opt::DerivedArgList &Args,
                              StringRef BoundArch,
                              Action::OffloadKind OffloadKind) const {
  DerivedArgList *DAL = ToolChain::TranslateArgs(Args, BoundArch, OffloadKind);
  if (!DAL)
    DAL = new DerivedArgList(Args.getBaseArgs());

  const OptTable &Opts = getDriver().getOpts();

  for (Arg *A : Args)
    if (!llvm::is_contained(*DAL, A))
      DAL->append(A);

  if (!DAL->hasArg(options::OPT_march_EQ) && OffloadKind != Action::OFK_None) {
    DAL->AddJoinedArg(nullptr, Opts.getOption(options::OPT_march_EQ),
                      OffloadArchToString(OffloadArch::RaisaDefault));
  } else if (DAL->getLastArgValue(options::OPT_march_EQ) == "generic" &&
             OffloadKind == Action::OFK_None) {
    DAL->eraseArg(options::OPT_march_EQ);
  } else if (DAL->getLastArgValue(options::OPT_march_EQ) == "native") {
    auto RPUsOrErr = getSystemRPUArchs(Args);
    if (!RPUsOrErr) {
      getDriver().Diag(diag::err_drv_undetermined_rpu_arch)
          << getArchName() << llvm::toString(RPUsOrErr.takeError()) << "-march";
    } else {
      DAL->AddJoinedArg(nullptr, Opts.getOption(options::OPT_march_EQ),
                        Args.MakeArgString(RPUsOrErr->front()));
    }
  }

  return DAL;
}

void TSRPUToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs, llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind DeviceOffloadingKind) const {}

bool TSRPUToolChain::supportsDebugInfoOption(const llvm::opt::Arg *A) const {
  const Option &O = A->getOption();
  return (O.matches(options::OPT_gN_Group) &&
          !O.matches(options::OPT_gmodules)) ||
         O.matches(options::OPT_g_Flag) ||
         O.matches(options::OPT_ggdbN_Group) || O.matches(options::OPT_ggdb) ||
         O.matches(options::OPT_gdwarf) || O.matches(options::OPT_gdwarf_2) ||
         O.matches(options::OPT_gdwarf_3) || O.matches(options::OPT_gdwarf_4) ||
         O.matches(options::OPT_gdwarf_5) ||
         O.matches(options::OPT_gcolumn_info);
}


Expected<SmallVector<std::string>>
TSRPUToolChain::getSystemRPUArchs(const ArgList &Args) const {
  // Detect the TSingMicro RPUs available on the system.
  std::string Program;
  if (Arg *A = Args.getLastArg(options::OPT_offload_arch_tool_EQ))
    Program = A->getValue();
  else
    Program = GetProgramPath("tsrpu-arch");

  auto StdoutOrErr = executeToolChainProgram(Program);
  if (!StdoutOrErr)
    return StdoutOrErr.takeError();

  SmallVector<std::string, 1> RPUArchs;
  llvm::StringMap<bool> FeatureMap;
  for (StringRef Arch : llvm::split((*StdoutOrErr)->getBuffer(), "\n")) {
    if (Arch.empty())
      continue;
    // Only keep the architecture names the RAISA target understands.
    if (parseTargetID(getTriple(), Arch, &FeatureMap))
      RPUArchs.push_back(Arch.str());
  }

  if (RPUArchs.empty())
    return llvm::createStringError(std::error_code(),
                                   "No TSingMicro RPU detected in the system");

  return RPUArchs;
}

/// RAISA toolchain.  Our device compiler produces RPU objects and the TSRPU
/// "linker" (tsrpu-link) stitches them into the single offload binary that the
/// host compilation embeds with -fraisa-include-rpubinary.

RaisaToolChain::RaisaToolChain(const Driver &D, const llvm::Triple &Triple,
                             const ToolChain &HostTC, const ArgList &Args)
    : TSRPUToolChain(D, Triple, HostTC.getTriple(), Args), HostTC(HostTC) {}

void RaisaToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs, llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind DeviceOffloadingKind) const {
  HostTC.addClangTargetOptions(DriverArgs, CC1Args, DeviceOffloadingKind);

  assert(DeviceOffloadingKind == Action::OFK_Raisa &&
         "Only RAISA offloading kinds are supported for TSingMicro RPUs.");

  CC1Args.append({"-fraisa-is-device", "-fno-threadsafe-statics"});

  if (!DriverArgs.hasFlag(options::OPT_offloadlib, options::OPT_no_offloadlib,
                          true))
    return;

  // The fork does not detect the SDK version from the installation; device
  // compilations are always driven with the RAISA 1.0 SDK version.
  CC1Args.push_back(DriverArgs.MakeArgString(
      Twine("-target-sdk-version=") +
      RaisaVersionToString(RaisaVersion::RAISA_10)));
}

llvm::DenormalMode RaisaToolChain::getDefaultDenormalModeForType(
    const llvm::opt::ArgList &DriverArgs, const JobAction &JA,
    const llvm::fltSemantics *FPType) const {
  if (JA.getOffloadingDeviceKind() == Action::OFK_Raisa) {
    if (FPType && FPType == &llvm::APFloat::IEEEsingle() &&
        DriverArgs.hasFlag(options::OPT_frpu_flush_denormals_to_zero,
                           options::OPT_fno_rpu_flush_denormals_to_zero, false))
      return llvm::DenormalMode::getPreserveSign();
  }

  assert(JA.getOffloadingDeviceKind() != Action::OFK_Host);
  return llvm::DenormalMode::getIEEE();
}

void RaisaToolChain::AddRaisaIncludeArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  RaisaInstallation.AddRaisaIncludeArgs(DriverArgs, CC1Args);
}

std::string RaisaToolChain::getInputFilename(const InputInfo &Input) const {
  // Only object files are changed, for example assembly files keep their .s
  // extensions. If the user requested device-only compilation don't change it.
  if (Input.getType() != types::TY_Object || getDriver().offloadDeviceOnly())
    return ToolChain::getInputFilename(Input);

  return ToolChain::getInputFilename(Input);
}

llvm::opt::DerivedArgList *
RaisaToolChain::TranslateArgs(const llvm::opt::DerivedArgList &Args,
                              StringRef BoundArch,
                              Action::OffloadKind DeviceOffloadKind) const {
  DerivedArgList *DAL =
      HostTC.TranslateArgs(Args, BoundArch, DeviceOffloadKind);
  if (!DAL)
    DAL = new DerivedArgList(Args.getBaseArgs());

  const OptTable &Opts = getDriver().getOpts();

  for (Arg *A : Args)
    if (!llvm::is_contained(*DAL, A))
      DAL->append(A);

  if (!BoundArch.empty()) {
    DAL->eraseArg(options::OPT_march_EQ);
    DAL->AddJoinedArg(nullptr, Opts.getOption(options::OPT_mcpu_EQ), BoundArch);
  }
  return DAL;
}

Tool *TSRPUToolChain::buildLinker() const {
  return new tools::TSRPU::Linker(*this);
}

Tool *RaisaToolChain::buildLinker() const {
  return new tools::TSRPU::Linker(*this);
}

void RaisaToolChain::addClangWarningOptions(ArgStringList &CC1Args) const {
  HostTC.addClangWarningOptions(CC1Args);
}

ToolChain::CXXStdlibType
RaisaToolChain::GetCXXStdlibType(const ArgList &Args) const {
  return HostTC.GetCXXStdlibType(Args);
}

void RaisaToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                              ArgStringList &CC1Args) const {
  HostTC.AddClangSystemIncludeArgs(DriverArgs, CC1Args);

  if (DriverArgs.hasFlag(options::OPT_offload_inc, options::OPT_no_offload_inc,
                         true) &&
      RaisaInstallation.isValid())
    CC1Args.append(
        {"-internal-isystem",
         DriverArgs.MakeArgString(RaisaInstallation.getIncludePath())});
}

void RaisaToolChain::AddClangCXXStdlibIncludeArgs(const ArgList &Args,
                                                 ArgStringList &CC1Args) const {
  HostTC.AddClangCXXStdlibIncludeArgs(Args, CC1Args);
}

void RaisaToolChain::AddIAMCUIncludeArgs(const ArgList &Args,
                                        ArgStringList &CC1Args) const {
  HostTC.AddIAMCUIncludeArgs(Args, CC1Args);
}

SanitizerMask RaisaToolChain::getSupportedSanitizers() const {
  // The RaisaToolChain only supports sanitizers in the sense that it allows
  // sanitizer arguments on the command line if they are supported by the host
  // toolchain. The RaisaToolChain will actually ignore any command line
  // arguments for any of these "supported" sanitizers. That means that no
  // sanitization of device code is actually supported at this time.
  //
  // This behavior is necessary because the host and device toolchains
  // invocations often share the command line, so the device toolchain must
  // tolerate flags meant only for the host toolchain.
  return HostTC.getSupportedSanitizers();
}

VersionTuple RaisaToolChain::computeMSVCVersion(const Driver *D,
                                               const ArgList &Args) const {
  return HostTC.computeMSVCVersion(D, Args);
}
