//===-- RISCV.h - Top-level interface for RISC-V ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// RISC-V back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCV_H
#define LLVM_LIB_TARGET_RISCV_RISCV_H

#include "MCTargetDesc/RISCVBaseInfo.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class FunctionPass;
class InstructionSelector;
class ModulePass;
class PassRegistry;
class RISCVRegisterBankInfo;
class RISCVSubtarget;
class RISCVTargetMachine;

FunctionPass *createRISCVCodeGenPreparePass();
void initializeRISCVCodeGenPreparePass(PassRegistry &);

FunctionPass *createRISCVDeadRegisterDefinitionsPass();
void initializeRISCVDeadRegisterDefinitionsPass(PassRegistry &);

FunctionPass *createRISCVIndirectBranchTrackingPass();
void initializeRISCVIndirectBranchTrackingPass(PassRegistry &);

FunctionPass *createRISCVLandingPadSetupPass();
void initializeRISCVLandingPadSetupPass(PassRegistry &);

FunctionPass *createRISCVISelDag(RISCVTargetMachine &TM,
                                 CodeGenOptLevel OptLevel);

FunctionPass *createRISCVLateBranchOptPass();
void initializeRISCVLateBranchOptPass(PassRegistry &);

FunctionPass *createRISCVMakeCompressibleOptPass();
void initializeRISCVMakeCompressibleOptPass(PassRegistry &);

FunctionPass *createRISCVGatherScatterLoweringPass();
void initializeRISCVGatherScatterLoweringPass(PassRegistry &);

FunctionPass *createRISCVVectorPeepholePass();
void initializeRISCVVectorPeepholePass(PassRegistry &);

FunctionPass *createRISCVOptWInstrsPass();
void initializeRISCVOptWInstrsPass(PassRegistry &);

FunctionPass *createRISCVFoldMemOffsetPass();
void initializeRISCVFoldMemOffsetPass(PassRegistry &);

FunctionPass *createRISCVMergeBaseOffsetOptPass();
void initializeRISCVMergeBaseOffsetOptPass(PassRegistry &);

FunctionPass *createRISCVExpandPseudoPass();
void initializeRISCVExpandPseudoPass(PassRegistry &);

FunctionPass *createRISCVPreRAExpandPseudoPass();
void initializeRISCVPreRAExpandPseudoPass(PassRegistry &);

FunctionPass *createRISCVExpandAtomicPseudoPass();
void initializeRISCVExpandAtomicPseudoPass(PassRegistry &);

FunctionPass *createRISCVInsertVSETVLIPass();
void initializeRISCVInsertVSETVLIPass(PassRegistry &);
extern char &RISCVInsertVSETVLIID;

FunctionPass *createRISCVPostRAExpandPseudoPass();
void initializeRISCVPostRAExpandPseudoPass(PassRegistry &);
FunctionPass *createRISCVInsertReadWriteCSRPass();
void initializeRISCVInsertReadWriteCSRPass(PassRegistry &);

FunctionPass *createRISCVInsertWriteVXRMPass();
void initializeRISCVInsertWriteVXRMPass(PassRegistry &);

FunctionPass *createRISCVRedundantCopyEliminationPass();
void initializeRISCVRedundantCopyEliminationPass(PassRegistry &);

FunctionPass *createRISCVMoveMergePass();
void initializeRISCVMoveMergePass(PassRegistry &);

FunctionPass *createRISCVPushPopOptimizationPass();
void initializeRISCVPushPopOptPass(PassRegistry &);
FunctionPass *createRISCVLoadStoreOptPass();
void initializeRISCVLoadStoreOptPass(PassRegistry &);

FunctionPass *createRISCVZacasABIFixPass();
void initializeRISCVZacasABIFixPass(PassRegistry &);

InstructionSelector *
createRISCVInstructionSelector(const RISCVTargetMachine &,
                               const RISCVSubtarget &,
                               const RISCVRegisterBankInfo &);
void initializeRISCVDAGToDAGISelLegacyPass(PassRegistry &);

FunctionPass *createRISCVPostLegalizerCombiner();
void initializeRISCVPostLegalizerCombinerPass(PassRegistry &);

FunctionPass *createRISCVO0PreLegalizerCombiner();
void initializeRISCVO0PreLegalizerCombinerPass(PassRegistry &);

FunctionPass *createRISCVPreLegalizerCombiner();
void initializeRISCVPreLegalizerCombinerPass(PassRegistry &);

FunctionPass *createRISCVVLOptimizerPass();
void initializeRISCVVLOptimizerPass(PassRegistry &);

FunctionPass *createRISCVVMV0EliminationPass();
void initializeRISCVVMV0EliminationPass(PassRegistry &);

FunctionPass *createTX8InsertXFRMPass();
void initializeTX8InsertXFRMPass(PassRegistry &);

ModulePass *createTX8TransformSIMTToSIMDPass();
void initializeTX8TransformSIMTToSIMDPass(PassRegistry &);

ModulePass *createTX8LowerSharedLegacyPass();
void initializeTX8LowerSharedLegacyPass(PassRegistry &);

// TX8BankAwareShared.cpp -- fork `bank-aware-shared`: places `l0ashared` (AS3)
// and `l0wshared` (AS4) globals into the tile-memory banks (128 KiB bank
// stride, one bank per variable) and deletes them, before `rpu-lower-shared`.
// Only inserted when `-riscv-bank-aware` is set (`isRISCVBankAware()`).
ModulePass *createTX8BankAwareSharedLegacyPass();
void initializeTX8BankAwareSharedLegacyPass(PassRegistry &);
bool isRISCVBankAware();

FunctionPass *createTX8OptimizeConfigInstrsPass();
void initializeTX8OptimizeConfigInstrsPass(PassRegistry &);

// TX8LegalizeEXEC.cpp -- post-RA legalization of the SIMT EXEC lane mask.
FunctionPass *createTX8LegalizeEXECPass();
void initializeTX8LegalizeEXECPass(PassRegistry &);

// TX8AnnotateControlFlow.cpp -- fork `tx8-annotate-control-flow`: rewrites
// every lane-divergent conditional branch into a divergence query
// (`get.pred.mask` / `and.s` / `raisa.divergent`) and restores the enclosing
// active mask (`set.exec`) where such a region closes.  Runs immediately
// before `simt2simd`, which is the only order that lets it see the
// pre-vectorisation CFG.
FunctionPass *createTX8AnnotateControlFlowLegacyPass();
void initializeTX8AnnotateControlFlowLegacyPass(PassRegistry &);

FunctionPass *createTX8RegisterRenamePass();
void initializeTX8RegisterRenamePass(PassRegistry &);
bool isTX8VRegRenameEnabled();

// TX8RegisterAlignment.cpp -- fork `vreg-align`: widens the operand that an
// `*_ALIGNED_*` pseudo demands and renames the instruction to its twin.  Runs
// after twoaddressinstruction / before register-coalescer (see addPreISel()).
extern char &TX8RegisterAlignmentPassID;
FunctionPass *createTX8RegisterAlignmentPass();
void initializeTX8RegisterAlignmentPass(PassRegistry &);

void initializeRISCVAsmPrinterPass(PassRegistry &);
} // namespace llvm

#endif
