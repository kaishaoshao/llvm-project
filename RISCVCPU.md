# RISCVCPU 资料

## 主要文件作用

暂时使用Toy的文件结构

```
tree -L 2
.
├── CMakeLists.txt
├── TargetDesc
│   ├── CMakeLists.txt
│   ├── ToyAsmBackend.cpp
│   ├── ToyAsmBackend.h
│   ├── ToyBaseInfo.h
│   ├── ToyELFObjectWriter.cpp
│   ├── ToyELFObjectWriter.h
│   ├── ToyFixupKinds.h
│   ├── ToyInstPrinter.cpp
│   ├── ToyInstPrinter.h
│   ├── ToyMCCodeEmitter.cpp
│   ├── ToyMCCodeEmitter.h
│   ├── ToyMCExpr.cpp
│   ├── ToyMCExpr.h
│   ├── ToyTargetDesc.cpp
│   ├── ToyTargetDesc.h
│   ├── ToyTargetStreamer.cpp
│   └── ToyTargetStreamer.h
├── TargetInfo
│   ├── CMakeLists.txt
│   └── ToyTargetInfo.cpp
├── ToyAsmPrinter.cpp
├── ToyAsmPrinter.h
├── ToyCallingConv.td
├── ToyDAGToDAGISel.cpp
├── ToyDAGToDAGISel.h
├── ToyFrameLowering.cpp
├── ToyFrameLowering.h
├── ToyInstrFormats.td
├── ToyInstrInfo.cpp
├── ToyInstrInfo.h
├── ToyInstrInfo.td
├── ToyISelLowering.cpp
├── ToyISelLowering.h
├── ToyMCInstLower.cpp
├── ToyMCInstLower.h
├── ToyRegisterInfo.cpp
├── ToyRegisterInfo.h
├── ToyRegisterInfo.td
├── ToySchedule.td
├── ToySubtarget.cpp
├── ToySubtarget.h
├── ToySubtarget.td
├── ToyTargetMachine.cpp
├── ToyTargetMachine.h
├── ToyTargetObjectFile.cpp
├── ToyTargetObjectFile.h
└── Toy.td

```

### **一、顶层文件**

1. **CMakeLists.txt**• **作用**：项目主构建配置，集成LLVM构建系统并组织子模块（如 `add_subdirectory(TargetDesc)`）。
   • **涉及类**：通过CMake宏调用LLVM的 `AddLLVM`和 `HandleLLVMOptions`模块。

### **二、目标描述（TargetDesc/）**

4. **ToyTargetDesc.cpp/.h**• **作用**：定义目标机器的全局描述符（寄存器集、指令集特性），通过 `TargetMachine`类提供访问接口。
   • **涉及类**：`ToyTargetDesc`继承自 `TargetMachine`，包含 `getInstrInfo()`等方法。
5. **ToyAsmBackend.cpp/.h**• **作用**：汇编器后端，处理重定位、代码布局及ELF文件生成。
   • **涉及类**：`MCObjectWriter`和 `MCAsmBackend`的子类。
6. **ToyELFObjectWriter.cpp/.h**• **作用**：生成ELF格式目标文件，定义段布局和符号表结构。
   • **涉及类**：`ELFObjectWriter`的子类。
7. **ToyInstPrinter.cpp/.h**• **作用**：控制汇编指令的文本输出格式。
   • **涉及类**：`MCInstPrinter`的子类，重写 `printInstruction()`方法。
8. **ToyMCCodeEmitter.cpp/.h**• **作用**：将机器指令编码为二进制格式。
   • **涉及类**：`MCCodeEmitter`的子类，实现 `encodeInstruction()`。
9. **ToyMCExpr.cpp/.h**• **作用**：处理目标相关的表达式（如符号地址计算）。
   • **涉及类**：`MCExpr`的子类，用于重定位和符号引用。
10. **ToyFixupKinds.h**
    ◦ **作用**：定义重定位类型（如PC相对偏移、绝对地址）。

---

### **三、目标信息（TargetInfo/）**

11. **ToyTargetInfo.cpp**
    ◦ **作用**：向LLVM注册目标架构，声明目标三元组（Triple）。
    ◦ **涉及类**：通过 `TargetRegistry::RegisterTarget`注册目标。

---

### **四、指令集定义（.td文件）**

12. **ToyInstrFormats.td**◦ **作用**：定义指令的二进制编码格式（如操作码、寄存器字段）。
    ◦ **涉及类**：通过TableGen生成 `Instruction`类的子类。
13. **ToyInstrInfo.td**◦ **作用**：声明指令的操作数、编码规则及语义，匹配LLVM IR到目标指令。
    ◦ **涉及类**：`TargetInstrInfo`的子类，定义指令模式（Pattern）。
14. **ToyRegisterInfo.td**◦ **作用**：定义寄存器的名称、类别及分配策略。
    ◦ **涉及类**：`RegisterClass`和 `TargetRegisterInfo`的子类。
15. **ToyCallingConv.td**◦ **作用**：指定函数调用时的参数传递规则（如寄存器/栈传递）。
16. **ToySchedule.td**
    ◦ **作用**：描述流水线调度策略（指令延迟、吞吐量）。
    ◦ **涉及类**：`SchedMachineModel`的子类。

---

### **五、代码生成核心**

17. **ToyISelLowering.cpp/.h**◦ **作用**：将LLVM IR降级为目标指令的中间表示（DAG节点）。
    ◦ **涉及类**：`TargetLowering`的子类，实现 `LowerOperation()`方法。
18. **ToyDAGToDAGISel.cpp/.h**◦ **作用**：从DAG到目标指令的选择逻辑，实现模式匹配。
    ◦ **涉及类**：`SelectionDAGISel`的子类，重写 `Select()`方法。
19. **ToyAsmPrinter.cpp/.h**◦ **作用**：生成汇编代码，处理函数/变量的输出格式。
    ◦ **涉及类**：`AsmPrinter`的子类，实现 `emitInstruction()`。
20. **ToyMCInstLower.cpp/.h**
    ◦ **作用**：将LLVM机器指令转换为MCInst对象（用于二进制编码）。
    ◦ **涉及类**：`MCInstLowering`的子类。

---

### **六、目标机器配置**

21. **ToyTargetMachine.cpp/.h**◦ **作用**：目标机器的入口类，协调优化与代码生成流程。
    ◦ **涉及类**：继承自 `LLVMTargetMachine`，管理 `Subtarget`和 `DataLayout`。
22. **ToySubtarget.cpp/.h**◦ **作用**：定义处理器子目标特性（如扩展指令支持）。
    ◦ **涉及类**：`TargetSubtargetInfo`的子类，包含调度模型和指令集标志。
23. **ToyFrameLowering.cpp/.h**
    ◦ **作用**：处理栈帧布局（如局部变量分配和过程调用）。
    ◦ **涉及类**：`TargetFrameLowering`的子类，实现 `emitPrologue()`。

---

### **七、辅助文件**

24. **ToyTargetObjectFile.cpp/.h**◦ **作用**：定义目标文件格式细节（如段布局和符号重定位）。
    ◦ **涉及类**：`TargetLoweringObjectFile`的子类。
25. **ToyBaseInfo.h**
    ◦ **作用**：声明目标架构的公共常量（如寄存器编号、指令编码掩码）。

## 缩写参数解释

在支持LLVM后端时一些参数使用了约定俗成的缩写，不好理解意思

| 缩写 | 全称                            | 作用                                         |
| ---- | ------------------------------- | -------------------------------------------- |
| MAI  | Memory Access Information       | 提供有关汇编语言的信息，如注释规则和指令格式 |
| MII  | Machine Instruction Information | 提供有关指令集的信息，包括指令的操作码和格式 |
| MPI  | Machine Register Information    | 提供有关寄存器的信息，包括寄存器的名称和编号 |
|      |                                 |                                              |
|      |                                 |                                              |
|      |                                 |                                              |


## 实现步骤

参照sunway提交记录

### ch1

### ch2

### ch3

### ch4

### ch5

### ch6

### ch7

这里对于codegen文件的修改主要看使用的pass

### ch8

### ch9

### ch10

### ch11

### ch12

### ch13

#### ch14

### ch15

### ch16

### ch17

### ch18

### ch19

### ch20

### ch21

### ch22

### ch23

### ch24

### ch25

### ch26

### ch27

### ch28

### ch29

### ch30

### ch31

### ch32

### ch33

### ch34

### ch35

### ch36

### ch37

### ch38

### ch39

### ch40

### ch41

### ch42

### ch43

### ch44

### ch45

### ch46

### ch47

### ch48

### ch49
