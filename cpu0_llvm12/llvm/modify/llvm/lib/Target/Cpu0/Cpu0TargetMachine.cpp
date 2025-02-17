//===-- Cpu0TargetMachine.cpp - Define TargetMachine for Cpu0 -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements the info about Cpu0 target spec.
//
//===----------------------------------------------------------------------===//

#include "Cpu0TargetMachine.h"
#include "Cpu0.h"

// #include "llvm/IR/Attributes.h"
// #include "llvm/IR/Function.h"
// #include "llvm/Support/CodeGen.h"
// #include "llvm/CodeGen/Passes.h"
// #include "llvm/CodeGen/TargetPassConfig.h"
// #include "llvm/Support/TargetRegistry.h"
// #include "llvm/Target/TargetOptions.h"

#include "Cpu0Subtarget.h"
#include "Cpu0TargetObjectFile.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

#define DEBUG_TYPE "cpu0"



extern "C" void LLVMInitializeCpu0Target() {
    // Register the target.
    //- Big endian Target Machine
    RegisterTargetMachine<Cpu0TargetMachine> X(TheCpu0Target);
    // - Little endian Target Machine
    RegisterTargetMachine<Cpu0TargetMachine> Y(TheCpu0elTarget);
}

static std::string computeDataLayoutString(const Triple &TT, StringRef CPU,
                                           const TargetOptions &Options,
                                           bool isLittle) {
    std::string Ret = "";
    // There are both little and big endian Cpu0.
    if (isLittle)
        Ret += "e";
    else
        Ret += "E";
    
    Ret += "-m:m";  // 机器模式

    // Pointers are 32 bits some ABIs
    Ret += "-p:32:32";

    // 8 and 16 bit integers only need to have natural alignment, but try to 
    // align them to 32 bits. 64 bit integers have natural alignment.

    Ret += "-i8:8:32-i16:16:32-i64:64"; // 整数对齐规则

    // 32 bit registers are always available and the stack is at least 64 aligned.
    Ret += "-n32-S64";  // 添加寄存器和栈对齐规则

    return Ret;
}

// 获取有效的重定位模型
static Reloc::Model getEffectiveCodeModel(bool JIT,
                                        Optional<Reloc::Model> RM){
    if (!RM.hasValue())
        return Reloc::Static;
    return *RM;
}

// DataLayout --> Big-endian, 32-bit pointer/ABI/alignment
// The stack is always 8 byte aligned
// On function prologue, the stack is created by decrementing
// its pointer. Once decremented, all references are done with positive
// offset from the stack/frame pointer, using StackGrowsUp enables
// an easier handling.
// Using CodeModel::Large enables different CALL behavior.
CpuoTargetMachine::Cpu0TargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     Optional<Reloc::Model> RM, 
                                     Optional<CodeModel::Model> CM,
                                     CodeGenOpt::Level OL, bool JIT, 
                                     bool isLittle) 
// 默认大端
    : LLVMTargetMachine(T, computeDataLayout(TT, CPU, Options, isLittle), TT,
                        CPU, FS, Options, getEffectiveCodeModel(JIT, RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
        isLittle(isLittle), TLOF(make_unique<Cpu0TargetObjectFile>()),
        ABI(Cpu0ABI::computeTargetABI()), 
        DefaultSubtarget(TT, CPU, FS, isLittle, *this){
    // initAsmInfo will display featurea by llc -march=cpu0 -mcpu=help on 3.7 but not on 3.6
    initsAsmInfo();
}

Cpu0TargetMachine::~Cpu0TargetMachine() {}

viod Cpu0TargetMachine::anchor() {}



              



