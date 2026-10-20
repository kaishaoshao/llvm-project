//===-- RISCVTargetMachine.cpp - Define TargetMachine for RISC-V ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about RISC-V target spec.
//
//===----------------------------------------------------------------------===//

#include "RISCVTargetMachine.h"
#include "MCTargetDesc/RISCVBaseInfo.h"
#include "RISCV.h"
#include "RISCVMachineFunctionInfo.h"
#include "RISCVTargetObjectFile.h"
#include "RISCVTargetTransformInfo.h"
#include "TargetInfo/RISCVTargetInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/GlobalISel/CSEInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/MIRParser/MIParser.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/MacroFusion.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Vectorize/EVLIndVarSimplify.h"
#include "llvm/Transforms/Vectorize/LoopIdiomVectorize.h"
#include <optional>
using namespace llvm;

static cl::opt<bool> EnableRedundantCopyElimination(
    "riscv-enable-copyelim",
    cl::desc("Enable the redundant copy elimination pass"), cl::init(true),
    cl::Hidden);

// FIXME: Unify control over GlobalMerge.
static cl::opt<cl::boolOrDefault>
    EnableGlobalMerge("riscv-enable-global-merge", cl::Hidden,
                      cl::desc("Enable the global merge pass"));

static cl::opt<bool>
    EnableMachineCombiner("riscv-enable-machine-combiner",
                          cl::desc("Enable the machine combiner pass"),
                          cl::init(true), cl::Hidden);

static cl::opt<unsigned> RVVVectorBitsMaxOpt(
    "riscv-v-vector-bits-max",
    cl::desc("Assume V extension vector registers are at most this big, "
             "with zero meaning no maximum size is assumed."),
    cl::init(0), cl::Hidden);

static cl::opt<int> RVVVectorBitsMinOpt(
    "riscv-v-vector-bits-min",
    cl::desc("Assume V extension vector registers are at least this big, "
             "with zero meaning no minimum size is assumed. A value of -1 "
             "means use Zvl*b extension. This is primarily used to enable "
             "autovectorization with fixed width vectors."),
    cl::init(-1), cl::Hidden);

static cl::opt<bool> EnableRISCVCopyPropagation(
    "riscv-enable-copy-propagation",
    cl::desc("Enable the copy propagation with RISC-V copy instr"),
    cl::init(true), cl::Hidden);

static cl::opt<bool> EnableRISCVDeadRegisterElimination(
    "riscv-enable-dead-defs", cl::Hidden,
    cl::desc("Enable the pass that removes dead"
             " definitions and replaces stores to"
             " them with stores to x0"),
    cl::init(true));

static cl::opt<bool>
    EnableSinkFold("riscv-enable-sink-fold",
                   cl::desc("Enable sinking and folding of instruction copies"),
                   cl::init(true), cl::Hidden);

static cl::opt<bool>
    EnableLoopDataPrefetch("riscv-enable-loop-data-prefetch", cl::Hidden,
                           cl::desc("Enable the loop data prefetch pass"),
                           cl::init(true));

static cl::opt<bool> EnableMISchedLoadStoreClustering(
    "riscv-misched-load-store-clustering", cl::Hidden,
    cl::desc("Enable load and store clustering in the machine scheduler"),
    cl::init(true));

static cl::opt<bool> EnablePostMISchedLoadStoreClustering(
    "riscv-postmisched-load-store-clustering", cl::Hidden,
    cl::desc("Enable PostRA load and store clustering in the machine scheduler"),
    cl::init(true));

static cl::opt<bool>
    EnableVLOptimizer("riscv-enable-vl-optimizer",
                      cl::desc("Enable the RISC-V VL Optimizer pass"),
                      cl::init(true), cl::Hidden);

static cl::opt<bool> DisableVectorMaskMutation(
    "riscv-disable-vector-mask-mutation",
    cl::desc("Disable the vector mask scheduling mutation"), cl::init(false),
    cl::Hidden);

static cl::opt<bool>
    EnableMachinePipeliner("riscv-enable-pipeliner",
                           cl::desc("Enable Machine Pipeliner for RISC-V"),
                           cl::init(false), cl::Hidden);

// Fork `vreg-align` (TX8RegisterAlignment.cpp).  The vendor default is true and
// the pass is inserted right after twoaddressinstruction; `=false` removes it
// from the pipeline (the vendor then crashes on device kernels -- see
// docs/compiler/OPT_BASELINE_MATRIX.md).
static cl::opt<bool>
    EnableVRegAlign("riscv-vreg-align",
                    cl::desc("Enable the TX8 vector register alignment pass"),
                    cl::init(true), cl::Hidden);

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeRISCVTarget() {
  RegisterTargetMachine<RISCVTargetMachine> X(getTheRISCV32Target());
  RegisterTargetMachine<RISCVTargetMachine> Y(getTheRISCV64Target());
  auto *PR = PassRegistry::getPassRegistry();
  initializeGlobalISel(*PR);
  initializeRISCVO0PreLegalizerCombinerPass(*PR);
  initializeRISCVPreLegalizerCombinerPass(*PR);
  initializeRISCVPostLegalizerCombinerPass(*PR);
  initializeKCFIPass(*PR);
  initializeRISCVDeadRegisterDefinitionsPass(*PR);
  initializeRISCVLateBranchOptPass(*PR);
  initializeRISCVMakeCompressibleOptPass(*PR);
  initializeRISCVGatherScatterLoweringPass(*PR);
  initializeRISCVCodeGenPreparePass(*PR);
  initializeRISCVPostRAExpandPseudoPass(*PR);
  initializeRISCVMergeBaseOffsetOptPass(*PR);
  initializeRISCVOptWInstrsPass(*PR);
  initializeRISCVFoldMemOffsetPass(*PR);
  initializeRISCVPreRAExpandPseudoPass(*PR);
  initializeRISCVExpandPseudoPass(*PR);
  initializeRISCVVectorPeepholePass(*PR);
  initializeRISCVVLOptimizerPass(*PR);
  initializeTX8InsertXFRMPass(*PR);
  // wt-exec line: the vendor runs `legalize-exec` after `riscv-copyelim`, i.e.
  // in addPostRegAlloc() below.
  initializeTX8LegalizeEXECPass(*PR);
  initializeTX8OptimizeConfigInstrsPass(*PR);
  initializeTX8AnnotateControlFlowLegacyPass(*PR);
  initializeTX8TransformSIMTToSIMDPass(*PR);
  initializeTX8LowerSharedLegacyPass(*PR);
  initializeRISCVVMV0EliminationPass(*PR);
  initializeRISCVInsertVSETVLIPass(*PR);
  initializeRISCVInsertReadWriteCSRPass(*PR);
  initializeRISCVInsertWriteVXRMPass(*PR);
  initializeRISCVDAGToDAGISelLegacyPass(*PR);
  initializeRISCVMoveMergePass(*PR);
  initializeRISCVPushPopOptPass(*PR);
  initializeRISCVIndirectBranchTrackingPass(*PR);
  initializeRISCVLoadStoreOptPass(*PR);
  initializeRISCVExpandAtomicPseudoPass(*PR);
  initializeRISCVRedundantCopyEliminationPass(*PR);
  initializeRISCVAsmPrinterPass(*PR);
  initializeTX8RegisterRenamePass(*PR);
  initializeTX8RegisterAlignmentPass(*PR);
}

static StringRef computeDataLayout(const Triple &TT,
                                   const TargetOptions &Options) {
  StringRef ABIName = Options.MCOptions.getABIName();
  if (TT.isArch64Bit()) {
    if (ABIName == "lp64e")
      return "e-m:e-p:64:64-i64:64-i128:128-n32:64-S64";

    // RAISA device targets: the fork's RPU layout (32-bit GEP index, 32-bit
    // pointers in AS3/AS4, allocas in AS5).  Must match
    // clang/lib/Basic/Targets/RISCV.h exactly.
    if (TT.isRaisa())
      return "e-m:e-p:64:64:64:32-p3:32:32:32:32-p4:32:32:32:32-"
             "i64:64-i128:128-n32:64-S128-A5";

    // RAISA device targets: the fork's RPU layout (32-bit GEP index, 32-bit
    // pointers in AS3/AS4, allocas in AS5).  Must match
    // clang/lib/Basic/Targets/RISCV.h exactly.
    if (TT.isRaisa())
      return "e-m:e-p:64:64:64:32-p3:32:32:32:32-p4:32:32:32:32-"
             "i64:64-i128:128-n32:64-S128-A5";

    return "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128";
  }
  assert(TT.isArch32Bit() && "only RV32 and RV64 are currently supported");

  if (ABIName == "ilp32e")
    return "e-m:e-p:32:32-i64:64-n32-S32";

  return "e-m:e-p:32:32-i64:64-n32-S128";
}

static Reloc::Model getEffectiveRelocModel(const Triple &TT,
                                           std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

RISCVTargetMachine::RISCVTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, computeDataLayout(TT, Options), TT, CPU, FS,
                               Options, getEffectiveRelocModel(TT, RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<RISCVELFTargetObjectFile>()) {
  initAsmInfo();

  // RISC-V supports the MachineOutliner.
  setMachineOutliner(true);
  setSupportsDefaultOutlining(true);

  if (TT.isOSFuchsia() && !TT.isArch64Bit())
    report_fatal_error("Fuchsia is only supported for 64-bit");

  setCFIFixup(true);

  // Vendor fork: the RAISA (SIMT) device target requires structured control
  // flow.  Both sides of a divergent branch execute under an exec mask, so
  // duplicating blocks (tail duplication / tail merging) and splitting
  // critical edges is wrong there.
  //
  // Evidence (vendor object, `libLLVMRISCVCodeGen.a(RISCVTargetMachine.cpp.o)`,
  // `RISCVTargetMachine::RISCVTargetMachine` at +0x16a .. +0x18b):
  //     mov 0x2c(%rbp),%eax        ; eax = Triple::OSType
  //     cmp $0x2a,%eax             ; 0x2a = 42 = Triple::RAISA
  //     jne ...
  //     orb $0x1,0x2b0(%rbx)       ; TargetMachine::RequireStructuredCFG |= 1
  // The vendor object has no such store for any other OS type.
  //
  // Observable consequences, RAISA only:
  //   * TargetPassConfig::addMachineLateOptimization() skips the late
  //     `tailduplication` pass -- the vendor's `-print-after-all` on an `.rc`
  //     cell has no `tailduplication` after `branch-folder`, while its musl
  //     `.c` pipeline does have one.
  //   * MachineBlockPlacement::allowTailDupPlacement() is false: placement
  //     neither duplicates the return block into a predecessor nor tail-merges,
  //     and BranchFolding's tail merge is disabled too.
  //   * MachineBasicBlock::canSplitCriticalEdge() refuses to split edges.
  // The flag is a Triple property, so musl (riscv64-unknown-linux-musl) is
  // unaffected -- confirmed by the vendor's own musl pipeline.
  if (TT.isRaisa())
    setRequiresStructuredCFG(true);
}

const RISCVSubtarget *
RISCVTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU =
      CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  std::string TuneCPU =
      TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
  std::string FS =
      FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  unsigned RVVBitsMin = RVVVectorBitsMinOpt;
  unsigned RVVBitsMax = RVVVectorBitsMaxOpt;

  Attribute VScaleRangeAttr = F.getFnAttribute(Attribute::VScaleRange);
  if (VScaleRangeAttr.isValid()) {
    if (!RVVVectorBitsMinOpt.getNumOccurrences())
      RVVBitsMin = VScaleRangeAttr.getVScaleRangeMin() * RISCV::RVVBitsPerBlock;
    std::optional<unsigned> VScaleMax = VScaleRangeAttr.getVScaleRangeMax();
    if (VScaleMax.has_value() && !RVVVectorBitsMaxOpt.getNumOccurrences())
      RVVBitsMax = *VScaleMax * RISCV::RVVBitsPerBlock;
  }

  // Vendor fork: the RAISA (SIMT) device target has a fixed vector length of
  // 128 lanes of 32 bits = 4096 bits.  Making the minimum and maximum equal
  // makes RISCVSubtarget::getRealVLen() exact, which is what lets
  // RISCVVectorPeephole::convertToVLMAX() turn an AVL equal to VLMAX (128 for
  // e32/m1 and for e16/mf2) into the VLMAX sentinel.  The vendor's MIR for
  // mathdev_raisa_fadd_rn_f carries `-1` in the VL slot of its vle/vfadd/vse
  // pseudos where this reconstruction materialises `ADDI $x0, 128`; with the
  // sentinel in place the vsetvli pass picks the PseudoVSETVLIX0 (AVL = $x0)
  // form the vendor uses.  This overrides -riscv-v-vector-bits-min/max, as the
  // vendor does too (forcing those options to 1024/1024 on the vendor binary
  // still produces the sentinel conversion).
  if (TargetTriple.getOS() == Triple::RAISA) {
    RVVBitsMin = 4096;
    RVVBitsMax = 4096;
  }

  if (RVVBitsMin != -1U) {
    // FIXME: Change to >= 32 when VLEN = 32 is supported.
    assert((RVVBitsMin == 0 || (RVVBitsMin >= 64 && RVVBitsMin <= 65536 &&
                                isPowerOf2_32(RVVBitsMin))) &&
           "V or Zve* extension requires vector length to be in the range of "
           "64 to 65536 and a power 2!");
    assert((RVVBitsMax >= RVVBitsMin || RVVBitsMax == 0) &&
           "Minimum V extension vector length should not be larger than its "
           "maximum!");
  }
  assert((RVVBitsMax == 0 || (RVVBitsMax >= 64 && RVVBitsMax <= 65536 &&
                              isPowerOf2_32(RVVBitsMax))) &&
         "V or Zve* extension requires vector length to be in the range of "
         "64 to 65536 and a power 2!");

  if (RVVBitsMin != -1U) {
    if (RVVBitsMax != 0) {
      RVVBitsMin = std::min(RVVBitsMin, RVVBitsMax);
      RVVBitsMax = std::max(RVVBitsMin, RVVBitsMax);
    }

    RVVBitsMin = llvm::bit_floor(
        (RVVBitsMin < 64 || RVVBitsMin > 65536) ? 0 : RVVBitsMin);
  }
  RVVBitsMax =
      llvm::bit_floor((RVVBitsMax < 64 || RVVBitsMax > 65536) ? 0 : RVVBitsMax);

  SmallString<512> Key;
  raw_svector_ostream(Key) << "RVVMin" << RVVBitsMin << "RVVMax" << RVVBitsMax
                           << CPU << TuneCPU << FS;
  auto &I = SubtargetMap[Key];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    auto ABIName = Options.MCOptions.getABIName();
    if (const MDString *ModuleTargetABI = dyn_cast_or_null<MDString>(
            F.getParent()->getModuleFlag("target-abi"))) {
      auto TargetABI = RISCVABI::getTargetABI(ABIName);
      if (TargetABI != RISCVABI::ABI_Unknown &&
          ModuleTargetABI->getString() != ABIName) {
        report_fatal_error("-target-abi option != target-abi module flag");
      }
      ABIName = ModuleTargetABI->getString();
    }
    I = std::make_unique<RISCVSubtarget>(
        TargetTriple, CPU, TuneCPU, FS, ABIName, RVVBitsMin, RVVBitsMax, *this);
  }
  return I.get();
}

MachineFunctionInfo *RISCVTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return RISCVMachineFunctionInfo::create<RISCVMachineFunctionInfo>(
      Allocator, F, static_cast<const RISCVSubtarget *>(STI));
}

TargetTransformInfo
RISCVTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<RISCVTTIImpl>(this, F));
}

// A RISC-V hart has a single byte-addressable address space of 2^XLEN bytes
// for all memory accesses, so it is reasonable to assume that an
// implementation has no-op address space casts. If an implementation makes a
// change to this, they can override it here.
bool RISCVTargetMachine::isNoopAddrSpaceCast(unsigned SrcAS,
                                             unsigned DstAS) const {
  return true;
}

ScheduleDAGInstrs *
RISCVTargetMachine::createMachineScheduler(MachineSchedContext *C) const {
  ScheduleDAGMILive *DAG = createSchedLive(C);
  if (EnableMISchedLoadStoreClustering) {
    DAG->addMutation(createLoadClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));
    DAG->addMutation(createStoreClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));
  }

  const RISCVSubtarget &ST = C->MF->getSubtarget<RISCVSubtarget>();
  if (!DisableVectorMaskMutation && ST.hasVInstructions())
    DAG->addMutation(createRISCVVectorMaskDAGMutation(DAG->TRI));

  return DAG;
}

ScheduleDAGInstrs *
RISCVTargetMachine::createPostMachineScheduler(MachineSchedContext *C) const {
  ScheduleDAGMI *DAG = createSchedPostRA(C);
  if (EnablePostMISchedLoadStoreClustering) {
    DAG->addMutation(createLoadClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));
    DAG->addMutation(createStoreClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));
  }

  return DAG;
}

namespace {

class RVVRegisterRegAlloc : public RegisterRegAllocBase<RVVRegisterRegAlloc> {
public:
  RVVRegisterRegAlloc(const char *N, const char *D, FunctionPassCtor C)
      : RegisterRegAllocBase(N, D, C) {}
};

static bool onlyAllocateRVVReg(const TargetRegisterInfo &TRI,
                               const MachineRegisterInfo &MRI,
                               const Register Reg) {
  const TargetRegisterClass *RC = MRI.getRegClass(Reg);
  return RISCVRegisterInfo::isRVVRegClass(RC);
}

static FunctionPass *useDefaultRegisterAllocator() { return nullptr; }

static llvm::once_flag InitializeDefaultRVVRegisterAllocatorFlag;

/// -riscv-rvv-regalloc=<fast|basic|greedy> command line option.
/// This option could designate the rvv register allocator only.
/// For example: -riscv-rvv-regalloc=basic
static cl::opt<RVVRegisterRegAlloc::FunctionPassCtor, false,
               RegisterPassParser<RVVRegisterRegAlloc>>
    RVVRegAlloc("riscv-rvv-regalloc", cl::Hidden,
                cl::init(&useDefaultRegisterAllocator),
                cl::desc("Register allocator to use for RVV register."));

static void initializeDefaultRVVRegisterAllocatorOnce() {
  RegisterRegAlloc::FunctionPassCtor Ctor = RVVRegisterRegAlloc::getDefault();

  if (!Ctor) {
    Ctor = RVVRegAlloc;
    RVVRegisterRegAlloc::setDefault(RVVRegAlloc);
  }
}

static FunctionPass *createBasicRVVRegisterAllocator() {
  return createBasicRegisterAllocator(onlyAllocateRVVReg);
}

static FunctionPass *createGreedyRVVRegisterAllocator() {
  return createGreedyRegisterAllocator(onlyAllocateRVVReg);
}

static FunctionPass *createFastRVVRegisterAllocator() {
  return createFastRegisterAllocator(onlyAllocateRVVReg, false);
}

static RVVRegisterRegAlloc basicRegAllocRVVReg("basic",
                                               "basic register allocator",
                                               createBasicRVVRegisterAllocator);
static RVVRegisterRegAlloc
    greedyRegAllocRVVReg("greedy", "greedy register allocator",
                         createGreedyRVVRegisterAllocator);

static RVVRegisterRegAlloc fastRegAllocRVVReg("fast", "fast register allocator",
                                              createFastRVVRegisterAllocator);

class RISCVPassConfig : public TargetPassConfig {
public:
  RISCVPassConfig(RISCVTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {
    if (TM.getOptLevel() != CodeGenOptLevel::None)
      substitutePass(&PostRASchedulerID, &PostMachineSchedulerID);
    setEnableSinkAndFold(EnableSinkFold);
    EnableLoopTermFold = true;
  }

  RISCVTargetMachine &getRISCVTargetMachine() const {
    return getTM<RISCVTargetMachine>();
  }

  void addIRPasses() override;
  bool addPreISel() override;
  void addCodeGenPrepare() override;
  bool addInstSelector() override;
  bool addIRTranslator() override;
  void addPreLegalizeMachineIR() override;
  bool addLegalizeMachineIR() override;
  void addPreRegBankSelect() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
  void addPreEmitPass() override;
  void addPreEmitPass2() override;
  void addPreSched2() override;
  void addMachineSSAOptimization() override;
  FunctionPass *createRVVRegAllocPass(bool Optimized);
  bool addRegAssignAndRewriteFast() override;
  bool addRegAssignAndRewriteOptimized() override;
  void addPreRegAlloc() override;
  void addPostRegAlloc() override;
  void addFastRegAlloc() override;

  std::unique_ptr<CSEConfigBase> getCSEConfig() const override;
};
} // namespace

TargetPassConfig *RISCVTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new RISCVPassConfig(*this, PM);
}

std::unique_ptr<CSEConfigBase> RISCVPassConfig::getCSEConfig() const {
  return getStandardCSEConfigForOpt(TM->getOptLevel());
}

FunctionPass *RISCVPassConfig::createRVVRegAllocPass(bool Optimized) {
  // Initialize the global default.
  llvm::call_once(InitializeDefaultRVVRegisterAllocatorFlag,
                  initializeDefaultRVVRegisterAllocatorOnce);

  RegisterRegAlloc::FunctionPassCtor Ctor = RVVRegisterRegAlloc::getDefault();
  if (Ctor != useDefaultRegisterAllocator)
    return Ctor();

  if (Optimized)
    return createGreedyRVVRegisterAllocator();

  return createFastRVVRegisterAllocator();
}

bool RISCVPassConfig::addRegAssignAndRewriteFast() {
  addPass(createRVVRegAllocPass(false));
  addPass(createRISCVInsertVSETVLIPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None &&
      EnableRISCVDeadRegisterElimination)
    addPass(createRISCVDeadRegisterDefinitionsPass());
  return TargetPassConfig::addRegAssignAndRewriteFast();
}

bool RISCVPassConfig::addRegAssignAndRewriteOptimized() {
  addPass(createRVVRegAllocPass(true));
  addPass(createVirtRegRewriter(false));
  addPass(createRISCVInsertVSETVLIPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None &&
      EnableRISCVDeadRegisterElimination)
    addPass(createRISCVDeadRegisterDefinitionsPass());
  return TargetPassConfig::addRegAssignAndRewriteOptimized();
}

void RISCVPassConfig::addIRPasses() {
  addPass(createAtomicExpandLegacyPass());
  addPass(createRISCVZacasABIFixPass());

  if (getOptLevel() != CodeGenOptLevel::None) {
    if (EnableLoopDataPrefetch)
      addPass(createLoopDataPrefetchPass());

    addPass(createRISCVGatherScatterLoweringPass());
    addPass(createInterleavedAccessPass());
    addPass(createRISCVCodeGenPreparePass());
  }

  TargetPassConfig::addIRPasses();

  // RAISA device kernels: consume the SIMT intrinsics (`llvm.riscv.read.reg.*`,
  // `llvm.riscv.raisa.*`, `get.pred.mask`) and rewrite them into upstream RVV
  // intrinsics.  Pass name `simt2simd` -- the same name the vendor's pipeline
  // uses, so `-print-before/after=simt2simd` works.  Runs last in the IR
  // pipeline, which is where the vendor's pass sits (its `before` IR is the
  // fully optimised module).
  if (TM->getTargetTriple().isRaisa()) {
    // RAISA device path: drop configuration-instruction writes whose value is
    // already the value of the configuration slot (and hoist loop-invariant
    // ones).  Vendor pass name `config-instr-opt`; the vendor inserts it after
    // the loop passes and *before* `simt2simd`, which `-mllvm
    // -print-before=config-instr-opt -mllvm -print-before=simt2simd` confirms.
    addPass(createTX8OptimizeConfigInstrsPass());
    // The vendor's `-mllvm -debug-pass=Structure` order (verbatim) is
    //   ... -rpu-unify-divergent-exit-nodes -domtree -cycles -fix-irreducible
    //   -loops -unify-loop-exits -postdomtree -domfrontier -regions
    //   -structurizecfg -loops -cycles -uniformity
    //   -tx8-annotate-control-flow -simt2simd -domtree -early-cse
    //   -rpu-lower-shared ...
    // i.e. a verbatim copy of the AMDGPU divergence stack
    // (`AMDGPUUnifyDivergentExitNodes`, `FixIrreducible`, `UnifyLoopExits`,
    // `StructurizeCFG`, `SIAnnotateControlFlow`), with only the annotation pass
    // replaced by the fork's own.
    //
    // `structurizecfg` is *not* optional: `simt2simd` only vectorises kernels
    // whose regions have been structurized into a single exit, and the
    // annotation pass merely *follows* whatever CFG it is handed (a region is
    // closed by emitting `set.exec` at the block that was pushed).  Without it
    // the two `control_*_loop` cells keep their multi-exit CFG, the annotation
    // puts its `set.exec` in the loop body instead of in the vendor's `Flow`
    // blocks, `simt2simd` bails and the surviving `llvm.riscv.read.reg.*` kills
    // ISel ("Don't know how to custom type legalize this intrinsic!").
    // Evidence: vendor `-print-before/after=structurizecfg` vs our
    // `-print-after=tx8-annotate-control-flow` dumps on those two cells.
    //
    // The three passes the vendor runs *before* it are pure no-ops on every
    // multi-block cell of the corpus (vendor `-print-before/after` of
    // `rpu-unify-divergent-exit-nodes`, `fix-irreducible` and
    // `unify-loop-exits` on `control_divergent_loop`, `control_uniform_loop`,
    // `control_nested` and `control_early_return`: every dump is byte-identical
    // to its input), so they stay unported.  (The note that used to be here
    // claimed `structurizecfg` itself was a no-op; that was measured on
    // `control_nested`/`control_early_return` only, whose regions have a single
    // exit to begin with.)
    //
    // Blast radius is structural, not sampled: upstream `StructurizeCFG::run`
    // returns immediately for the top-level region, so a single-basic-block
    // kernel cannot be touched.  Exactly 4 of the 114 `.rc` cells have more
    // than one basic block before `simt2simd` (the two `control_*_loop` cells,
    // `control_nested`, `control_early_return`); the other 110 are single-BB
    // and therefore provably unaffected.
    addPass(createStructurizeCFGPass());
    addPass(createTX8AnnotateControlFlowLegacyPass());
    addPass(createTX8TransformSIMTToSIMDPass());
    // The vendor's pipeline runs `-domtree -early-cse` between `simt2simd` and
    // `rpu-lower-shared` (verbatim from its own `-mllvm
    // -debug-pass=Structure`), and that pass is observable: it folds the
    // all-zero `getelementptr` the signature packing inserts in front of the
    // first parameter load, and merges the identical tile-memory address
    // chains `simt2simd` builds once per access.  Without it our
    // `rpu-lower-shared` input keeps both, which is what made
    // `mem_l1_size_*`'s register allocation differ from the vendor's.
    addPass(createEarlyCSEPass());
  }

  // RAISA device kernels: give every `__attribute__((l0ashared|l0wshared|
  // l1shared))` variable its concrete address and delete the global.  The
  // vendor's `rpu-lower-shared` sits between `early-cse` and `callbrprepare`,
  // i.e. immediately after `simt2simd` in this pipeline.
  if (TM->getTargetTriple().isRaisa())
    addPass(createTX8LowerSharedLegacyPass());
}

bool RISCVPassConfig::addPreISel() {
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    // Add a barrier before instruction selection so that we will not get
    // deleted block address after enabling default outlining. See D99707 for
    // more details.
    addPass(createBarrierNoopPass());
  }

  // RAISA device kernels must keep their tile-memory (`l0ashared`/
  // `l0wshared`/`l1shared`) variables distinct.  The vendor runs the global
  // merge late enough that its `simt2simd` still sees one global per variable
  // and can recognise `addrspacecast (ptr addrspace(2|3|4) @g to ptr)` as an
  // offset-0 tile-memory access; with the merge in place every access is
  // rewritten to `getelementptr @_MergedGlobals, 0, <field>`, which is
  // indistinguishable from a shared global accessed at a non-zero constant
  // offset, so the pass has to bail and ISel then dies on the surviving
  // AS2/AS3/AS4 access (`alloc_*`: `Don't know how to custom type legalize
  // this intrinsic!`).  Verified: `-mllvm -riscv-enable-global-merge=false`
  // restores the vendor's per-variable globals verbatim.
  if (!TM->getTargetTriple().isRaisa() &&
      ((TM->getOptLevel() != CodeGenOptLevel::None &&
        EnableGlobalMerge == cl::BOU_UNSET) ||
       EnableGlobalMerge == cl::BOU_TRUE)) {
    // FIXME: Like AArch64, we disable extern global merging by default due to
    // concerns it might regress some workloads. Unlike AArch64, we don't
    // currently support enabling the pass in an "OnlyOptimizeForSize" mode.
    // Investigating and addressing both items are TODO.
    addPass(createGlobalMergePass(TM, /* MaxOffset */ 2047,
                                  /* OnlyOptimizeForSize */ false,
                                  /* MergeExternalByDefault */ true));
  }

  // Fork `vreg-align`: the vendor inserts TX8RegisterAlignment directly after
  // twoaddressinstruction and before register-coalescer -- verified both in its
  // RISCVTargetMachine.cpp.o relocations
  // (`insertPass(&TwoAddressInstructionPassID, &TX8RegisterAlignmentPassID)`)
  // and in its `-mllvm -debug-pass=Structure` listing
  // (`-twoaddressinstruction -vreg-align -slotindexes -liveintervals
  //   -register-coalescer ...`).
  if (EnableVRegAlign)
    insertPass(&TwoAddressInstructionPassID, &TX8RegisterAlignmentPassID);

  return false;
}

void RISCVPassConfig::addCodeGenPrepare() {
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createTypePromotionLegacyPass());
  TargetPassConfig::addCodeGenPrepare();
}

bool RISCVPassConfig::addInstSelector() {
  addPass(createRISCVISelDag(getRISCVTargetMachine(), getOptLevel()));

  return false;
}

bool RISCVPassConfig::addIRTranslator() {
  addPass(new IRTranslator(getOptLevel()));
  return false;
}

void RISCVPassConfig::addPreLegalizeMachineIR() {
  if (getOptLevel() == CodeGenOptLevel::None) {
    addPass(createRISCVO0PreLegalizerCombiner());
  } else {
    addPass(createRISCVPreLegalizerCombiner());
  }
}

bool RISCVPassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer());
  return false;
}

void RISCVPassConfig::addPreRegBankSelect() {
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createRISCVPostLegalizerCombiner());
}

bool RISCVPassConfig::addRegBankSelect() {
  addPass(new RegBankSelect());
  return false;
}

bool RISCVPassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelect(getOptLevel()));
  return false;
}

void RISCVPassConfig::addPreSched2() {
  addPass(createRISCVPostRAExpandPseudoPass());

  // Emit KCFI checks for indirect calls.
  addPass(createKCFIPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None)
    addPass(createRISCVLoadStoreOptPass());
}

void RISCVPassConfig::addPreEmitPass() {
  // TODO: It would potentially be better to schedule copy propagation after
  // expanding pseudos (in addPreEmitPass2). However, performing copy
  // propagation after the machine outliner (which runs after addPreEmitPass)
  // currently leads to incorrect code-gen, where copies to registers within
  // outlined functions are removed erroneously.
  if (TM->getOptLevel() >= CodeGenOptLevel::Default &&
      EnableRISCVCopyPropagation)
    addPass(createMachineCopyPropagationPass(true));
  if (TM->getOptLevel() >= CodeGenOptLevel::Default)
    addPass(createRISCVLateBranchOptPass());
  // The IndirectBranchTrackingPass inserts lpad and could have changed the
  // basic block alignment. It must be done before Branch Relaxation to
  // prevent the adjusted offset exceeding the branch range.
  addPass(createRISCVIndirectBranchTrackingPass());
  addPass(&BranchRelaxationPassID);
  addPass(createRISCVMakeCompressibleOptPass());
}

void RISCVPassConfig::addPreEmitPass2() {
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    addPass(createRISCVMoveMergePass());
    // Schedule PushPop Optimization before expansion of Pseudo instruction,
    // ensuring return instruction is detected correctly.
    addPass(createRISCVPushPopOptimizationPass());
  }
  addPass(createRISCVExpandPseudoPass());

  // Schedule the expansion of AMOs at the last possible moment, avoiding the
  // possibility for other passes to break the requirements for forward
  // progress in the LR/SC block.
  addPass(createRISCVExpandAtomicPseudoPass());

  // KCFI indirect call checks are lowered to a bundle.
  addPass(createUnpackMachineBundles([&](const MachineFunction &MF) {
    return MF.getFunction().getParent()->getModuleFlag("kcfi");
  }));

  // TX82: break vector register anti-dependencies.  Only enabled from -O2 on.
  if (isTX8VRegRenameEnabled() &&
      TM->getOptLevel() >= CodeGenOptLevel::Default)
    addPass(createTX8RegisterRenamePass());
}

void RISCVPassConfig::addMachineSSAOptimization() {
  addPass(createRISCVVectorPeepholePass());
  addPass(createRISCVFoldMemOffsetPass());

  TargetPassConfig::addMachineSSAOptimization();

  if (EnableMachineCombiner)
    addPass(&MachineCombinerID);

  if (TM->getTargetTriple().isRISCV64()) {
    addPass(createRISCVOptWInstrsPass());
  }
}

void RISCVPassConfig::addPreRegAlloc() {
  addPass(createRISCVPreRAExpandPseudoPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    addPass(createRISCVMergeBaseOffsetOptPass());
    if (EnableVLOptimizer)
      addPass(createRISCVVLOptimizerPass());
  }

  addPass(createTX8InsertXFRMPass());
  addPass(createRISCVInsertReadWriteCSRPass());
  addPass(createRISCVInsertWriteVXRMPass());
  addPass(createRISCVLandingPadSetupPass());

  if (TM->getOptLevel() != CodeGenOptLevel::None && EnableMachinePipeliner)
    addPass(&MachinePipelinerID);

  addPass(createRISCVVMV0EliminationPass());
}

void RISCVPassConfig::addFastRegAlloc() {
  addPass(&InitUndefID);
  TargetPassConfig::addFastRegAlloc();
}


void RISCVPassConfig::addPostRegAlloc() {
  if (TM->getOptLevel() != CodeGenOptLevel::None &&
      EnableRedundantCopyElimination)
    addPass(createRISCVRedundantCopyEliminationPass());

  // TX82 (wt-exec line): legalize the SIMT EXEC lane mask for RAISA device
  // kernels.  The vendor's pipeline puts `legalize-exec` immediately after
  // `riscv-copyelim` (`-mllvm -debug-pass=Structure`), i.e. after register
  // allocation and after the post-RA copy propagation, and before the
  // post-RA pseudo expansion that lowers the RVV pseudos.
  if (TM->getTargetTriple().isRaisa())
    addPass(createTX8LegalizeEXECPass());
}

void RISCVTargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerLateLoopOptimizationsEPCallback([=](LoopPassManager &LPM,
                                                 OptimizationLevel Level) {
    LPM.addPass(LoopIdiomVectorizePass(LoopIdiomVectorizeStyle::Predicated));
  });

  PB.registerVectorizerEndEPCallback(
      [](FunctionPassManager &FPM, OptimizationLevel Level) {
        if (Level.isOptimizingForSpeed())
          FPM.addPass(createFunctionToLoopPassAdaptor(EVLIndVarSimplifyPass()));
      });
}

yaml::MachineFunctionInfo *
RISCVTargetMachine::createDefaultFuncInfoYAML() const {
  return new yaml::RISCVMachineFunctionInfo();
}

yaml::MachineFunctionInfo *
RISCVTargetMachine::convertFuncInfoToYAML(const MachineFunction &MF) const {
  const auto *MFI = MF.getInfo<RISCVMachineFunctionInfo>();
  return new yaml::RISCVMachineFunctionInfo(*MFI);
}

bool RISCVTargetMachine::parseMachineFunctionInfo(
    const yaml::MachineFunctionInfo &MFI, PerFunctionMIParsingState &PFS,
    SMDiagnostic &Error, SMRange &SourceRange) const {
  const auto &YamlMFI =
      static_cast<const yaml::RISCVMachineFunctionInfo &>(MFI);
  PFS.MF.getInfo<RISCVMachineFunctionInfo>()->initializeBaseYamlFields(YamlMFI);
  return false;
}
