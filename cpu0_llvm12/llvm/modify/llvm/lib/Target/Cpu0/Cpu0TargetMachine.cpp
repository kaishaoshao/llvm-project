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
#include "llvm/IR/Metadata.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"
#include <string>

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
// 构造函数用于初始化Cpu0TargetMachine类的实例
// 该类负责管理CPU0目标机器的特定属性和行为
// 参数:
// - T: 目标机器的描述
// - TT: 目标三元组，提供有关目标平台的信息
// - CPU: 目标CPU的名称
// - FS: 特定于目标的功能字符串
// - Options: 目标选项，包含一系列编译选项
// - RM: 可选的重定位模型
// - CM: 可选的代码模型
// - OL: 代码生成的优化级别
// - JIT: 是否用于JIT编译的标志
// - isLittle: 是否为小端字节序的标志，默认大端
CpuoTargetMachine::Cpu0TargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     Optional<Reloc::Model> RM, 
                                     Optional<CodeModel::Model> CM,
                                     CodeGenOpt::Level OL, bool JIT, 
                                     bool isLittle) 
    : LLVMTargetMachine(T, computeDataLayout(TT, CPU, Options, isLittle), TT,
                        CPU, FS, Options, getEffectiveCodeModel(JIT, RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
        isLittle(isLittle), TLOF(make_unique<Cpu0TargetObjectFile>()),
        ABI(Cpu0ABI::computeTargetABI()), 
        DefaultSubtarget(TT, CPU, FS, isLittle, *this){
    // 初始化汇编信息，这将在使用llc命令行工具时显示CPU0目标的特性
    // 注意：此行为在3.7版本中可见，但在3.6版本中不可见
    initsAsmInfo();
}

Cpu0TargetMachine::~Cpu0TargetMachine() {}

void Cpu0TargetMachine::anchor() {}

Cpu0elTargetMachine::Cpu0elTargetMachine(const Target &T,const Triple &TT, 
                                         StringRef CPU, StringRef FS,
                                         const TargetOptions &Options,
                                         Optional<Reloc::Model> RM,
                                         Optional<CodeModel::Model> CM,
                                         CodeGenOpt::Level OL, bool JIT)
    : Cpu0TargetMachine(T, TT, CPU, FS, Options, RM, CM, OL, JIT, true) {}

const Cpu0Subtarget* Cpu0TargetMachine::getSubtargetImpl(const Function &F) const {
    std::string CPU = TargetCPU;
    std::string FS = TargetFS;

    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    auto &I = SubtargetMap[CPU + FS];
    if(!I) {
        resetTargetOptions(F);
        I = std::make_unique<Cpu0Subtarget>(TargetTriple, CPU, FS, isLittle, *this);
    }

    return I.get();
}

namespace {
/// Cpu0 Code Generator Pass Configuration Options.
class Cpu0PassConfig : public TargetPassConfig {
public: 
    Cpu0PassConfig(Cpu0TargetMachine &TM, PassManagerBase &PM)
        : TargetPassConfig(TM, PM) {}
    Cpu0TargetMachine &getCpu0TargetMachine() const {
        return getTM<Cpu0TargetMachine>();
    }
    
    const Cpu0Subtarget &getCpu0Subtarget() const {
        return *getCpu0TargetMachine().getSubtargetImpl();
    }

};
}

TargetPassConfig *Cpu0TargetMachine::createPassConfig(PassManagerBase &PM) {
    return new Cpu0PassConfig(*this, PM);
}


              



