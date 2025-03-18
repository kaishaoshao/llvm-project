# 总结

参考：  https://pages.dogdog.run/toolchain/llvm_toy_riscv_backend.html

## ch1 --- 生成识别llc

1. `CMakeLists.txt`：在根目录和 `lib/Target/RISCVCPU `下都有这个文件。通常，CMakeLists.txt是CMake的构建配置文件，用来定义如何编译和链接项目中的各个组件。根目录的CMakeLists.txt应该控制整个项目的构建，而子目录下的则处理该子模块的构建。
2. `include/llvm/TargetParser/Triple.h`：这个看起来像是LLVM中处理目标平台三元组（比如x86_64-unknown-linux-gnu）的头文件。Triple类可能用于解析和处理目标平台的架构、厂商、操作系统等信息。
3. `lib/Target/RISCVCPU `目录下的几个文件和子目录：
   • `CMakeLists.txt`：应该是构建RISCVCPU目标的后端代码的配置。
   • `TargetDesc`和 `TargetInfo`子目录：通常，在LLVM中，TargetInfo提供目标的基本信息（如寄存器、指令集），而TargetDesc处理目标描述，比如如何生成代码、处理ABI等。
   • `RISCVCPUTargetMachine.cpp`：这个文件可能实现了RISCVCPU目标的目标机器（TargetMachine），这是LLVM后端的重要组成部分，负责将LLVM IR转换为目标平台的机器代码。

接下来，需要确认这些目录和文件在LLVM项目中的典型作用。LLVM的后端通常包括目标描述、指令选择、代码生成等部分。每个目标（比如ARM、X86）都有自己的目录，结构类似。因此，`lib/Target/RISCVCPU`可能是一个自定义的LLVM后端，用于某个名为“RISCVCPU”的架构。

Triple.h属于TargetParser模块，负责解析三元组字符串，这在LLVM中用于确定目标平台的属性。这个头文件可能被多个后端共享，用于处理平台相关的逻辑。

TargetInfo和TargetDesc子目录中的代码可能分别定义了RISCVCPU架构的寄存器、指令集、调用约定（TargetInfo）以及如何生成目标代码、处理ELF/MachO/COFF文件格式（TargetDesc）。

RISCVCPUTargetMachine.cpp可能实现了RISCVCPUTargetMachine类，该类继承自LLVMTargetMachine，负责协调整个代码生成流程，包括指定数据布局、ABI信息，以及注册Pass（优化和转换步骤）来生成机器代码。

### **1. 根目录下的 `CMakeLists.txt`**

• **作用**：定义整个LLVM项目的顶层构建规则。
• **关键功能**：
  • 包含全局编译选项（如 `LLVM_ENABLE_PROJECTS`指定子项目）。
  • 配置LLVM核心库、工具链依赖关系和安装路径。
  • 通过 `add_subdirectory(lib/Target/RISCVCPU)`引入自定义目标后端。

---

### **2. `include/llvm/TargetParser/Triple.h`**

• **作用**：处理平台三元组（**Target Triple**）的解析和操作。
• **关键功能**：
  • 解析形如 `arch-vendor-os-environment`的三元组（例如 `riscv64-unknown-linux-gnu`）。
  • 提供接口获取架构（`getArch`）、操作系统（`getOS`）、ABI（`getEnvironment`）等信息。
  • 被LLVM工具链（如 `clang`、`llc`）用于确定目标平台的默认配置。

---

### **3. `lib/Target/RISCVCPU/`目录**

#### **3.1 `CMakeLists.txt`**

• **作用**：定义如何编译 `RISCVCPU` 目标后端的代码。
• **关键内容**：
  • 声明目标名称：`LLVM_TARGET_DEFINITIONS = `RISCVCPU``。
  • 添加源码文件（如 `RISCVCPUTargetMachine.cpp`）。
  • 链接依赖库（如 `LLVMTarget`、`LLVMCodeGen`）。

#### **3.2 `TargetDesc/`和 `TargetInfo/`子目录**

• **`TargetInfo/`**：
  • **作用**：定义目标的基本信息。
  • **关键文件**：
    ◦ `RISCVCPUTargetInfo.cpp`：注册目标名称和描述（`TargetRegistry`）。
• **`TargetDesc/`**：
  • **作用**：生成目标机器代码的描述和工具。
  • **关键文件**：
    ◦ `RISCVCPUMCTargetDesc.cpp`：定义ELF/MachO/COFF等目标文件格式支持。

#### **3.3 `RISCVCPUTargetMachine.cpp`**

• **作用**：实现 `RISCVCPU`目标的核心机器模型。
• **关键功能**：
  • 继承 `LLVMTargetMachine`，定义数据布局（`DataLayout`）和ABI规则。
  • 注册代码生成流程（如添加 `Pass`：指令选择、寄存器分配、代码优化）。
  • 配置目标特定的选项（如启用或禁用某些指令扩展）。

---

### **4. 模块交互关系**

| 模块                      | 依赖关系                             | 输出产物                      |
| ------------------------- | ------------------------------------ | ----------------------------- |
| `TargetParser`          | 被 `clang`、`llvm-mc`等工具使用  | 解析平台三元组                |
| `TargetInfo`            | 依赖 `LLVMSupport`、`LLVMTarget` | 注册目标到LLVM全局目标列表    |
| `TargetDesc`            | 依赖 `LLVMMC`、`LLVMCodeGen`     | 生成目标代码和汇编器/反汇编器 |
| `RISCVCPUTargetMachine` | 依赖 `TargetInfo`、`TargetDesc`  | 生成 `llc`可识别的目标后端  |

### 5. 编译与测试

```shell
 mkdir build
    pushd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
          -DLLVM_TARGETS_TO_BUILD=RISCVCPU -DLLVM_ENABLE_PROJECTS="clang" \
          -DLLVM_OPTIMIZED_TABLEGEN=On -DLLVM_PARALLEL_LINK_JOBS=1 -G "Ninja" ../llvm
    ninja llc
    popd
```

```
$ ./build/bin/llc --version
LLVM (http://llvm.org/):
  LLVM version 17.0.0git
  DEBUG build with assertions.
  Default target: 
  Host CPU: goldmont

  Registered Targets:
    riscvcpu - The RISCVCPU backend
```

```
 clang toy_test/test.c -c -emit-llvm -O0 -o /tmp/test.bc
llvm-dis /tmp/test.bc
cat /tmp/test.ll
./build/bin/llc /tmp/test.bc -march=toy

```

```
/build/bin/llc  /tmp/test.bc -march=riscvcpu
llc: /home/shaokai/Desktop/work/llvm-project-riscv/llvm/tools/llc/llc.cpp:575: auto compileModule(char **, LLVMContext &)::(anonymous class)::operator()(StringRef, StringRef) const: Assertion `Target && "Could not allocate target machine!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.      Program arguments: ./build/bin/llc /tmp/test.bc -march=riscvcpu
```

报错原因：

```
/* NOTE: 由于 LLVMInitializeToyTarget 没有实现, 导致
 * TheTarget.TargetMachineCtorFn 没有定义, TheTarget->createTargetMachine 返回
 * NULL */
Target = std::unique_ptr<TargetMachine>(TheTarget->createTargetMachine(
    TheTriple.getTriple(), CPUStr, FeaturesStr, Options, RM,
    codegen::getExplicitCodeModel(), OLvl));
assert(Target && "Could not allocate target machine!");
```



## **扩展阅读**

• **LLVM官方文档**：[Writing an LLVM Backend](https://llvm.org/docs/WritingAnLLVMBackend.html)

```
$> ./build/bin/llc --version
LLVM (http://llvm.org/):
  LLVM version 15.0.0git
  DEBUG build with assertions.
  Default target: x86_64-unknown-linux-gnu
  Host CPU: skylake

  Registered Targets:
    toy - Toy RISC-V backend

$> clang toy_test/test.c -c -emit-llvm -O0 -o /tmp/test.bc
$> ./build/bin/llc /tmp/test.bc -march=toy

llc: /home/sunway/source/llvm-toy/llvm/tools/llc/llc.cpp:559: auto
compileModule(char **, llvm::LLVMContext &)::(anonymous
class)::operator()(llvm::StringRef) const: Assertion `Target && "Could not
allocate target machine!"' failed.
```

报错的原因是:

```
/* NOTE: 由于 LLVMInitializeRISCVTarget 没有实现, 导致
 * TheTarget.TargetMachineCtorFn 没有定义, TheTarget->createTargetMachine 返回
 * NULL */
Target = std::unique_ptr<TargetMachine>(TheTarget->createTargetMachine(
    TheTriple.getTriple(), CPUStr, FeaturesStr, Options, RM,
    codegen::getExplicitCodeModel(), OLvl));
assert(Target && "Could not allocate target machine!");
```
