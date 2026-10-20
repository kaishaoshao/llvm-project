//===----- CGRAISARuntime.cpp - Interface to RAISA Runtimes -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for RAISA code generation.  Concrete
// subclasses of this implement code generation for specific RAISA
// runtime libraries.
//
//===----------------------------------------------------------------------===//

#include "CGRAISARuntime.h"
#include "CGCall.h"
#include "CodeGenFunction.h"
#include "clang/AST/ExprCXX.h"

using namespace clang;
using namespace CodeGen;

CGRAISARuntime::~CGRAISARuntime() {}

RValue CGRAISARuntime::EmitRAISAKernelCallExpr(CodeGenFunction &CGF,
                                             const RAISAKernelCallExpr *E,
                                             ReturnValueSlot ReturnValue,
                                             llvm::CallBase **CallOrInvoke) {
  llvm::BasicBlock *ConfigOKBlock = CGF.createBasicBlock("kcall.configok");
  llvm::BasicBlock *ContBlock = CGF.createBasicBlock("kcall.end");

  CodeGenFunction::ConditionalEvaluation eval(CGF);
  CGF.EmitBranchOnBoolExpr(E->getConfig(), ContBlock, ConfigOKBlock,
                           /*TrueCount=*/0);

  eval.begin(CGF);
  CGF.EmitBlock(ConfigOKBlock);
  CGF.EmitSimpleCallExpr(E, ReturnValue, CallOrInvoke);
  CGF.EmitBranch(ContBlock);

  CGF.EmitBlock(ContBlock);
  eval.end(CGF);

  return RValue::get(nullptr);
}
