//===-- Cpu0FrameLowering.cpp - Cpu0 Frame Information --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Cpu0 implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "Cpu0FrameLowering.h"

#include "Cpu0InstrInfo.h"
#include "Cpu0MachineFunction.h"
#include "Cpu0Subtarget.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

//- emitPrologue() and emitEpilogue must exist for main(). 

//===----------------------------------------------------------------------===//
//
// Stack Frame Processing methods
// +----------------------------+
//
// The stack is allocated decrementing the stack pointer on
// the first instruction of a function prologue. Once decremented,
// all stack references are done thought a positive offset
// from the stack/frame pointer, so the stack is considering
// to grow up! Otherwise terrible hacks would have to be made
// to get this stack ABI compliant :)
//
//  The stack frame required by the ABI (after call):
//  Offset
//
//  0                 ----------
//  4                 Args to pass
//  .                 saved $GP  (used in PIC)
//  .                 Alloca allocations
//  .                 Local Area
//  .                 CPU "Callee Saved" Registers
//  .                 saved FP
//  .                 saved RA
//  .                 FPU "Callee Saved" Registers
//  StackSize         -----------
//
// Offset - offset from sp after stack allocation on function prologue
//
// The sp is the stack pointer subtracted/added from the stack size
// at the Prologue/Epilogue
//
// References to the previous stack (to obtain arguments) are done
// with offsets that exceeds the stack size: (stacksize+(4*(num_arg-1))
//
// Examples:
// - reference to the actual stack frame
//   for any local area var there is smt like : FI >= 0, StackOffset: 4
//     st REGX, 4(SP)
//
// - reference to previous stack frame
//   suppose there's a load to the 5th arguments : FI < 0, StackOffset: 16.
//   The emitted instruction will be something like:
//     ld REGX, 16+StackSize(SP)
//
// Since the total stack size is unknown on LowerFormalArguments, all
// stack references (ObjectOffset) created to reference the function
// arguments, are negative numbers. This way, on eliminateFrameIndex it's
// possible to detect those references and the offsets are adjusted to
// their real location.
//
//===----------------------------------------------------------------------===//

//- emitPrologue() 和 emitEpilogue() 必须为 main() 函数存在。

//===----------------------------------------------------------------------===//
//
// 栈帧处理方法
// +----------------------------+
//
// 栈的分配是通过在函数序言的第一条指令中递减栈指针来完成的。一旦递减，
// 所有的栈引用都是通过栈指针或帧指针的正偏移量来完成的，因此栈被认为是
// 向上增长的！否则，必须进行一些糟糕的 hack 才能使栈符合 ABI 规范 :)
//
// ABI 所需的栈帧（在调用之后）：
// 偏移量
//
//  0                 ----------
//  4                 要传递的参数
//  .                 保存的 $GP（用于 PIC）
//  .                 Alloca 分配
//  .                 局部变量区域
//  .                 CPU 的“被调用者保存”寄存器
//  .                 保存的 FP
//  .                 保存的 RA
//  .                 FPU 的“被调用者保存”寄存器
//  StackSize         -----------
//
// 偏移量 - 函数序言中栈分配后相对于 sp 的偏移量
//
// sp 是栈指针，在序言/尾声中被减去/加上栈大小
//
// 对前一个栈的引用（以获取参数）是通过超过栈大小的偏移量来完成的：
// (stacksize + (4 * (num_arg - 1))
//
// 示例：
// - 对当前栈帧的引用
//   对于任何局部变量，类似这样：FI >= 0, StackOffset: 4
//     st REGX, 4(SP)
//
// - 对前一个栈帧的引用
//   假设加载第 5 个参数：FI < 0, StackOffset: 16
//   生成的指令可能类似于：
//     ld REGX, 16+StackSize(SP)
//
// 由于在 LowerFormalArguments 时总栈大小未知，所有为引用函数参数创建的
// 栈引用（ObjectOffset）都是负数。这样，在 eliminateFrameIndex 时，
// 可以检测到这些引用，并将偏移量调整到它们的实际位置。
//
//===----------------------------------------------------------------------===//

const Cpu0FrameLowering *Cpu0FrameLowering::create(const Cpu0Subtarget &ST) {
  return llvm::createCpu0SEFrameLowering(ST);
}

// hasFP - Return true if the specified function should have a dedicated frame
// pointer register.  This is true if the function has variable sized allocas,
// if it needs dynamic stack realignment, if frame pointer elimination is
// disabled, or if the frame address is taken.

// hasFP - 如果指定函数应该有一个专用的帧指针寄存器，则返回 true。
// 如果函数包含可变大小的 alloca 分配、需要动态栈对齐、帧指针消除被禁用，
// 或者帧地址被使用，则返回 true。
bool Cpu0FrameLowering::hasFP(const MachineFunction &MF) const{
    const MachineFrameInfo &MFI = MF.getFrameInfo();
    const TargetRegisterInfo &TRI = *STI.getRegisterInfo();

    return MF.getTarget().Options.DisableFramePointerElim(MF) ||
           MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
           TRI->needsStackRealignment(MF);
}