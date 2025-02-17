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

#ifndef LLVM_LIB_TARGET_CPU0_CPU0TARGETMACHINE_H
#define LLVM_LIB_TARGET_CPU0_CPU0TARGETMACHINE_H
// #include "Cpu0TargetMachine.h"
// #include "Cpu0.h"

// #include "llvm/IR/Attributes.h"
// #include "llvm/IR/Function.h"
// #include "llvm/Support/CodeGen.h"
// #include "llvm/CodeGen/Passes.h"
// #include "llvm/CodeGen/TargetPassConfig.h"
// #include "llvm/Support/TargetRegistry.h"
// #include "llvm/Target/TargetOptions.h"


#include "Cpu0Config.h" // 包含Cpu0配置文件

#include "MCTargetDesc/Cpu0ABIInfo.h" // 包含Cpu0 ABI信息
#include "Cpu0Subtarget.h" // 包含Cpu0子目标信息
#include "llvm/CodeGen/Passes.h" // 包含代码生成相关的头文件
#include "llvm/CodeGen/SelectionDAGISel.h" // 包含选择DAG指令选择器的头文件
#include "llvm/CodeGen/TargetFrameLowering.h" // 包含目标框架降低的头文件
#include "llvm/Support/CodeGen.h" // 包含代码生成支持的头文件
#include "llvm/Target/TargetMachine.h" // 包含目标机器的头文件

namespace llvm {
class formatted_raw_ostream; // 前向声明formatted_raw_ostream类
class Cpu0RegisterInfo; // 前向声明Cpu0RegisterInfo类

// 定义Cpu0TargetMachine类，继承自LLVMTargetMachine
class Cpu0TargetMchine : public LLVMTargetMachine {
    bool isLittle; // 是否为小端模式

    std::unique_ptr<TargetLoweringObjectFile> TLOF; // 独占指针，指向目标降低对象文件

    // 选择的ABI
    Cpu0ABI::ABI TargetABI;
    Cpu0Subtarget DefaultSubtarget; // 默认子目标

    mutable StringMap<std::unique_ptr<Cpu0Subtarget>> SubtargetMap; // 可变的子目标映射

public:
    // 构造函数
    Cpu0TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      Optional<Reloc::Model> RM, CodeModel::Model CM,
                      CodeGenOpt::Level OL, bool JIT, bool isLittle);
    // 析构函数
    ~Cpu0TargetMachine() override;

    // 获取子目标实现
    const Cpu0Subtarget *getSubtargetImpl() const { 
        return &DefaultSubtarget; 
    }

    // 根据函数获取子目标实现
    const Cpu0Subtarget *getSubtargetImpl(const Function &F) const override;

    // 传递管道配置
    TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

    // 获取对象文件降低
    TargetLoweringObjectFile *getObjFileLowering() const override {
        return TLOF.get();
    }

    // 判断是否为小端模式
    bool isLittleEndian() const { return isLittle; }

    // 获取ABI信息
    const Cpu0ABIInfo *getABI() const { return ABI; }

};

// Cpu0TagetMachine - Cpu0小端目标机器
class Cpu0elTargetMachine : public Cpu0TargetMachine {
    virtual void anchor(); // 锚定函数
public:
    // 构造函数
    Cpu0elTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                        StringRef FS, const TargetOptions &Options,
                        Optional<Reloc::Model> RM, CodeModel::Model CM,
                        CodeGenOpt::Level OL, bool JIT);

};

}  // 结束llvm命名空间

#endif // 结束预处理指令

#endif
