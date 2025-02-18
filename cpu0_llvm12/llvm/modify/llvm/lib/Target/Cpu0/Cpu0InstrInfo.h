//===-- Cpu0InstrInfo.h - Cpu0 Instruction Information ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Cpu0 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CPU0_CPU0INSTRINFO_H
#define LLVM_LIB_TARGET_CPU0_CPU0INSTRINFO_H

namespace llvm {

class Cpu0InstrInfo : public Cpu0GenInstrInfo {
    virtual void anchor();

protected:
    const Cpu0Subtarget &Subtarget;

public:
    explicit Cpu0InstrInfo(const Cpu0Subtarget &STI);

    static const Cpu0InstrInfo *create(Cpu0Subtarget &STI);

    /// getRegisterInfo - TargetInstrInfo is a superset of MRegister info.  As
    /// such, whenever a client has an instance of instruction info, it should
    /// always be able to get register info as well (through this method).
    /// 该注释解释getRegisterInfo方法的功能：TargetInstrInfo是MRegisterInfo的超集，
    /// 因此，只要客户端拥有指令信息的实例，就应该能够通过此方法获取寄存器信息。
    virtual const Cpu0RegisterInfo &getRegisterInfo() const = 0;

   
    unsigned GetInstSizeInBytes(const MachineInstr &MI) const;

};

const Cpu0InstrInfo *createCpu0SEInstrInfo(const Cpu0Subtarget &STI);

}


#endif // LLVM_LIB_TARGET_CPU0_CPU0INSTRINFO_H