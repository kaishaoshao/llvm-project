//===- RISCVSchedTX82.td - TX8 (tsm-x82) Scheduling Model --*- tablegen -*-===//
//
// MECHANICALLY RECONSTRUCTED from the vendor toolchain binary
//   llvm-toolchain/lib/libLLVMRISCVDesc.a(RISCVMCTargetDesc.cpp.o)
// with wt-passes/sched/dump_schedmodel.cpp (public LLVM MC API, -DNDEBUG).
// Every number below is read back out of the vendor TX82ModelSchedClasses[] /
// RISCVWriteProcResTable / RISCVWriteLatencyTable / RISCVReadAdvanceTable,
// or solved from them.  See docs/compiler/TX8_SCHEDMODEL.md and wt-sched/.
//
//===----------------------------------------------------------------------===//

def TX82Model : SchedMachineModel {
  let IssueWidth = 2;
  let MicroOpBufferSize = 0;
  let LoadLatency = 2;
  let HighLatency = 10;
  let MispredictPenalty = 5;
  let PostRAScheduler = false;
  let CompleteModel = false;
}

//===----------------------------------------------------------------------===//
// Processor resources: (Name, NumUnits, BufferSize) byte-exact from the
// vendor TX82ModelProcResources[] table.  TableGen emits ProcResourceUnits
// in record-name order, which is the order the vendor table has.  There are
// no Super/SubUnits relations and every AcquireAtCycle is 0 in the vendor
// model.  `SchedModel` is written out explicitly so that the four resources
// used only by the TX8 tensor instructions keep their vendor index.
//===----------------------------------------------------------------------===//
let SchedModel = TX82Model, BufferSize = 0 in {
def TX82ALU : ProcResource<2>;
def TX82CSR : ProcResource<1>;
def TX82FDIV : ProcResource<1>;
def TX82FMAC : ProcResource<1>;
def TX82FMISC : ProcResource<1>;
def TX82FMV : ProcResource<1>;
def TX82LSU : ProcResource<1>;
def TX82MDU : ProcResource<1>;
}

let SchedModel = TX82Model, BufferSize = 1 in {
def TX82KCoreWB : ProcResource<1>;
def TX82NCCConfig : ProcResource<1>;
def TX82SIMTStack : ProcResource<1>;
def TX82VADD : ProcResource<1>;
def TX82VCMP : ProcResource<1>;
def TX82VCTRL : ProcResource<1>;
def TX82VCVT : ProcResource<1>;
def TX82VDISPATCH : ProcResource<1>;
def TX82VIALU : ProcResource<1>;
def TX82VISPU : ProcResource<1>;
def TX82VLOAD : ProcResource<2>;
def TX82VMISC : ProcResource<1>;
def TX82VMUL : ProcResource<1>;
def TX82VMULTIF : ProcResource<1>;
def TX82VSTORE : ProcResource<1>;
def TX82VSUB : ProcResource<1>;
}

let SchedModel = TX82Model in {

//===----------------------------------------------------------------------===//
// Write resources, recovered one SchedWrite at a time from the vendor table.
// Writes the vendor model does not resource are marked Unsupported; that is
// exactly what makes the same set of sched classes invalid.
//===----------------------------------------------------------------------===//
def : WriteRes<WriteAtomicD, [TX82LSU]> {
  let Latency = 9;
}
def : WriteRes<WriteAtomicLDD, [TX82LSU]> {
  let Latency = 9;
}
def : WriteRes<WriteAtomicLDW, [TX82LSU]> {
  let Latency = 9;
}
def : WriteRes<WriteAtomicSTD, [TX82LSU]> {
  let Latency = 3;
}
def : WriteRes<WriteAtomicSTW, [TX82LSU]> {
  let Latency = 3;
}
def : WriteRes<WriteAtomicW, [TX82LSU]> {
  let Latency = 9;
}
def : WriteRes<WriteBEXT, [TX82ALU]>;
def : WriteRes<WriteBEXTI, [TX82ALU]>;
def : WriteRes<WriteCLMUL, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteCLZ, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteCLZ32, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteCPOP, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteCPOP32, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteCSR, [TX82CSR]>;
def : WriteRes<WriteCTZ, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteCTZ32, [TX82ALU]> {
  let Latency = 3;
}
def : WriteRes<WriteFAdd16, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFAdd32, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFAdd64, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFClass16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFClass32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFClass64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCmp16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCmp32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCmp64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF16ToF32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF16ToF64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF16ToI32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF16ToI64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF32ToF16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF32ToF64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF32ToI32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF32ToI64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF64ToF16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF64ToF32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF64ToI32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtF64ToI64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtI32ToF16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtI32ToF32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtI32ToF64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtI64ToF16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtI64ToF32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFCvtI64ToF64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFDiv16, [TX82FDIV]> {
  let Latency = 12;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteFDiv32, [TX82FDIV]> {
  let Latency = 19;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteFDiv64, [TX82FDIV]> {
  let Latency = 33;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteFLD16, [TX82LSU]> {
  let Latency = 5;
}
def : WriteRes<WriteFLD32, [TX82LSU]> {
  let Latency = 3;
}
def : WriteRes<WriteFLD64, [TX82LSU]> {
  let Latency = 3;
}
def : WriteRes<WriteFMA16, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFMA32, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFMA64, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFMinMax16, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFMinMax32, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFMinMax64, [TX82FMISC]> {
  let Latency = 2;
}
def : WriteRes<WriteFMovF16ToI16, [TX82FMV]>;
def : WriteRes<WriteFMovF32ToI32, [TX82FMV]>;
def : WriteRes<WriteFMovF64ToI64, [TX82FMV]>;
def : WriteRes<WriteFMovI16ToF16, [TX82FMV]>;
def : WriteRes<WriteFMovI32ToF32, [TX82FMV]>;
def : WriteRes<WriteFMovI64ToF64, [TX82FMV]>;
def : WriteRes<WriteFMul16, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFMul32, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFMul64, [TX82FMAC]> {
  let Latency = 4;
}
def : WriteRes<WriteFSGNJ16, [TX82FMV]>;
def : WriteRes<WriteFSGNJ32, [TX82FMV]>;
def : WriteRes<WriteFSGNJ64, [TX82FMV]>;
def : WriteRes<WriteFST16, [TX82LSU]>;
def : WriteRes<WriteFST32, [TX82LSU]>;
def : WriteRes<WriteFST64, [TX82LSU]>;
def : WriteRes<WriteFSqrt16, [TX82FDIV]> {
  let Latency = 11;
  let ReleaseAtCycles = [11];
}
def : WriteRes<WriteFSqrt32, [TX82FDIV]> {
  let Latency = 18;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteFSqrt64, [TX82FDIV]> {
  let Latency = 32;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteIALU, [TX82ALU]>;
def : WriteRes<WriteIALU32, [TX82ALU]>;
def : WriteRes<WriteIDiv, [TX82MDU]> {
  let Latency = 39;
  let ReleaseAtCycles = [39];
}
def : WriteRes<WriteIDiv32, [TX82MDU]> {
  let Latency = 39;
  let ReleaseAtCycles = [39];
}
def : WriteRes<WriteIMinMax, [TX82ALU]>;
def : WriteRes<WriteIMul, [TX82MDU]> {
  let Latency = 3;
}
def : WriteRes<WriteIMul32, [TX82MDU]> {
  let Latency = 3;
}
def : WriteRes<WriteIRem, [TX82MDU]> {
  let Latency = 39;
  let ReleaseAtCycles = [39];
}
def : WriteRes<WriteIRem32, [TX82MDU]> {
  let Latency = 39;
  let ReleaseAtCycles = [39];
}
def : WriteRes<WriteJal, [TX82ALU]>;
def : WriteRes<WriteJalr, [TX82ALU]>;
def : WriteRes<WriteJmp, [TX82ALU]>;
def : WriteRes<WriteLDB, [TX82LSU]> {
  let Latency = 5;
}
def : WriteRes<WriteLDD, [TX82LSU]> {
  let Latency = 3;
}
def : WriteRes<WriteLDH, [TX82LSU]> {
  let Latency = 5;
}
def : WriteRes<WriteLDW, [TX82LSU]> {
  let Latency = 3;
}
def : WriteRes<WriteNop, []>;
def : WriteRes<WriteORCB, [TX82ALU]>;
def : WriteRes<WriteREV8, [TX82ALU]>;
def : WriteRes<WriteRotateImm, [TX82ALU]>;
def : WriteRes<WriteRotateImm32, [TX82ALU]>;
def : WriteRes<WriteRotateReg, [TX82ALU]>;
def : WriteRes<WriteRotateReg32, [TX82ALU]>;
def : WriteRes<WriteSFB, [TX82ALU]> {
  let NumMicroOps = 2;
}
def : WriteRes<WriteSHXADD, [TX82ALU]>;
def : WriteRes<WriteSHXADD32, [TX82ALU]>;
def : WriteRes<WriteSTB, [TX82LSU]>;
def : WriteRes<WriteSTD, [TX82LSU]>;
def : WriteRes<WriteSTH, [TX82LSU]>;
def : WriteRes<WriteSTW, [TX82LSU]>;
def : WriteRes<WriteShiftImm, [TX82ALU]>;
def : WriteRes<WriteShiftImm32, [TX82ALU]>;
def : WriteRes<WriteShiftReg, [TX82ALU]>;
def : WriteRes<WriteShiftReg32, [TX82ALU]>;
def : WriteRes<WriteSingleBit, [TX82ALU]>;
def : WriteRes<WriteSingleBitImm, [TX82ALU]>;
def : WriteRes<WriteVCompressV_M1_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVCompressV_M1_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVCompressV_M1_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVCompressV_M1_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVCompressV_M2_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [35];
}
def : WriteRes<WriteVCompressV_M2_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [35];
}
def : WriteRes<WriteVCompressV_M2_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [35];
}
def : WriteRes<WriteVCompressV_M2_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [35];
}
def : WriteRes<WriteVCompressV_M4_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [67];
}
def : WriteRes<WriteVCompressV_M4_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [67];
}
def : WriteRes<WriteVCompressV_M4_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [67];
}
def : WriteRes<WriteVCompressV_M4_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [67];
}
def : WriteRes<WriteVCompressV_M8_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [131];
}
def : WriteRes<WriteVCompressV_M8_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [131];
}
def : WriteRes<WriteVCompressV_M8_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [131];
}
def : WriteRes<WriteVCompressV_M8_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [131];
}
def : WriteRes<WriteVCompressV_MF2_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [11];
}
def : WriteRes<WriteVCompressV_MF2_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [11];
}
def : WriteRes<WriteVCompressV_MF2_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [11];
}
def : WriteRes<WriteVCompressV_MF4_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVCompressV_MF4_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVCompressV_MF8_E8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVCompressV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [131];
}
def : WriteRes<WriteVFALUF_M1_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFALUF_M1_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFALUF_M1_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFALUF_M2_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFALUF_M2_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFALUF_M2_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFALUF_M4_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFALUF_M4_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFALUF_M4_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFALUF_M8_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUF_M8_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUF_M8_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUF_MF2_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFALUF_MF2_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFALUF_MF4_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVFALUF_WorstCase, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUV_M1_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFALUV_M1_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFALUV_M1_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFALUV_M2_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFALUV_M2_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFALUV_M2_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFALUV_M4_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFALUV_M4_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFALUV_M4_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFALUV_M8_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUV_M8_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUV_M8_E64, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFALUV_MF2_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFALUV_MF2_E32, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFALUV_MF4_E16, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVFALUV_WorstCase, [TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFCmpF_M1, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFCmpF_M2, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFCmpF_M4, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFCmpF_M8, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFCmpF_MF2, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFCmpF_MF4, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFCmpF_WorstCase, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFCmpV_M1, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFCmpV_M2, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFCmpV_M4, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFCmpV_M8, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFCmpV_MF2, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFCmpV_MF4, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFCmpV_WorstCase, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFCvtFToIV_M1, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFCvtFToIV_M2, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFCvtFToIV_M4, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFCvtFToIV_M8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFCvtFToIV_MF2, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFCvtFToIV_MF4, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFCvtFToIV_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFCvtIToFV_M1_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFCvtIToFV_M1_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFCvtIToFV_M1_E64, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFCvtIToFV_M2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFCvtIToFV_M2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFCvtIToFV_M2_E64, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFCvtIToFV_M4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFCvtIToFV_M4_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFCvtIToFV_M4_E64, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFCvtIToFV_M8_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFCvtIToFV_M8_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFCvtIToFV_M8_E64, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFCvtIToFV_MF2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFCvtIToFV_MF2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFCvtIToFV_MF4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFCvtIToFV_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFMergeV_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFMergeV_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFMergeV_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFMergeV_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFMergeV_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFMergeV_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFMergeV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFMinMaxF_M1_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFMinMaxF_M1_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFMinMaxF_M1_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFMinMaxF_M2_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFMinMaxF_M2_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFMinMaxF_M2_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFMinMaxF_M4_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFMinMaxF_M4_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFMinMaxF_M4_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFMinMaxF_M8_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxF_M8_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxF_M8_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxF_MF2_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFMinMaxF_MF2_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFMinMaxF_MF4_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMinMaxF_WorstCase, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxV_M1_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFMinMaxV_M1_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFMinMaxV_M1_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFMinMaxV_M2_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFMinMaxV_M2_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFMinMaxV_M2_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVFMinMaxV_M4_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFMinMaxV_M4_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFMinMaxV_M4_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [24];
}
def : WriteRes<WriteVFMinMaxV_M8_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxV_M8_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxV_M8_E64, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMinMaxV_MF2_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFMinMaxV_MF2_E32, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFMinMaxV_MF4_E16, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMinMaxV_WorstCase, [TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : WriteRes<WriteVFMovV_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFMovV_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFMovV_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFMovV_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFMovV_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFMovV_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFMovV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFMulF_M1_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMulF_M1_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMulF_M1_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMulF_M2_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFMulF_M2_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFMulF_M2_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFMulF_M4_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFMulF_M4_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFMulF_M4_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFMulF_M8_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulF_M8_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulF_M8_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulF_MF2_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFMulF_MF2_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFMulF_MF4_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVFMulF_WorstCase, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulV_M1_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMulV_M1_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMulV_M1_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVFMulV_M2_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFMulV_M2_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFMulV_M2_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFMulV_M4_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFMulV_M4_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFMulV_M4_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVFMulV_M8_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulV_M8_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulV_M8_E64, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFMulV_MF2_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFMulV_MF2_E32, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFMulV_MF4_E16, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVFMulV_WorstCase, [TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVFNCvtFToFV_M1_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFNCvtFToFV_M1_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFNCvtFToFV_M2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFNCvtFToFV_M2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFNCvtFToFV_M4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFNCvtFToFV_M4_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFNCvtFToFV_MF2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFNCvtFToFV_MF2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFNCvtFToFV_MF4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFNCvtFToFV_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFNCvtFToIV_M1, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFNCvtFToIV_M2, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFNCvtFToIV_M4, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFNCvtFToIV_MF2, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFNCvtFToIV_MF4, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFNCvtFToIV_MF8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFNCvtFToIV_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFRecpV_M1_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [15];
}
def : WriteRes<WriteVFRecpV_M1_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [15];
}
def : WriteRes<WriteVFRecpV_M1_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [15];
}
def : WriteRes<WriteVFRecpV_M2_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVFRecpV_M2_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVFRecpV_M2_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVFRecpV_M4_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFRecpV_M4_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFRecpV_M4_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFRecpV_M8_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFRecpV_M8_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFRecpV_M8_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFRecpV_MF2_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFRecpV_MF2_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFRecpV_MF4_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFRecpV_WorstCase, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFRedMinMaxV_From_M1_E16, [TX82VCMP]> {
  let Latency = 29;
  let ReleaseAtCycles = [29];
}
def : WriteRes<WriteVFRedMinMaxV_From_M1_E32, [TX82VCMP]> {
  let Latency = 29;
  let ReleaseAtCycles = [29];
}
def : WriteRes<WriteVFRedMinMaxV_From_M1_E64, [TX82VCMP]> {
  let Latency = 29;
  let ReleaseAtCycles = [29];
}
def : WriteRes<WriteVFRedMinMaxV_From_M2_E16, [TX82VCMP]> {
  let Latency = 33;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVFRedMinMaxV_From_M2_E32, [TX82VCMP]> {
  let Latency = 33;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVFRedMinMaxV_From_M2_E64, [TX82VCMP]> {
  let Latency = 33;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVFRedMinMaxV_From_M4_E16, [TX82VCMP]> {
  let Latency = 41;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVFRedMinMaxV_From_M4_E32, [TX82VCMP]> {
  let Latency = 41;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVFRedMinMaxV_From_M4_E64, [TX82VCMP]> {
  let Latency = 41;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVFRedMinMaxV_From_M8_E16, [TX82VCMP]> {
  let Latency = 57;
  let ReleaseAtCycles = [57];
}
def : WriteRes<WriteVFRedMinMaxV_From_M8_E32, [TX82VCMP]> {
  let Latency = 57;
  let ReleaseAtCycles = [57];
}
def : WriteRes<WriteVFRedMinMaxV_From_M8_E64, [TX82VCMP]> {
  let Latency = 57;
  let ReleaseAtCycles = [57];
}
def : WriteRes<WriteVFRedMinMaxV_From_MF2_E16, [TX82VCMP]> {
  let Latency = 27;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFRedMinMaxV_From_MF2_E32, [TX82VCMP]> {
  let Latency = 27;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFRedMinMaxV_From_MF4_E16, [TX82VCMP]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVFRedMinMaxV_From_WorstCase, [TX82VCMP]> {
  let Latency = 57;
  let ReleaseAtCycles = [57];
}
def : WriteRes<WriteVFRedOV_From_M1_E16, [TX82VADD]> {
  let Latency = 44;
  let ReleaseAtCycles = [44];
}
def : WriteRes<WriteVFRedOV_From_M1_E32, [TX82VADD]> {
  let Latency = 44;
  let ReleaseAtCycles = [44];
}
def : WriteRes<WriteVFRedOV_From_M1_E64, [TX82VADD]> {
  let Latency = 44;
  let ReleaseAtCycles = [44];
}
def : WriteRes<WriteVFRedOV_From_M2_E16, [TX82VADD]> {
  let Latency = 48;
  let ReleaseAtCycles = [48];
}
def : WriteRes<WriteVFRedOV_From_M2_E32, [TX82VADD]> {
  let Latency = 48;
  let ReleaseAtCycles = [48];
}
def : WriteRes<WriteVFRedOV_From_M2_E64, [TX82VADD]> {
  let Latency = 48;
  let ReleaseAtCycles = [48];
}
def : WriteRes<WriteVFRedOV_From_M4_E16, [TX82VADD]> {
  let Latency = 56;
  let ReleaseAtCycles = [56];
}
def : WriteRes<WriteVFRedOV_From_M4_E32, [TX82VADD]> {
  let Latency = 56;
  let ReleaseAtCycles = [56];
}
def : WriteRes<WriteVFRedOV_From_M4_E64, [TX82VADD]> {
  let Latency = 56;
  let ReleaseAtCycles = [56];
}
def : WriteRes<WriteVFRedOV_From_M8_E16, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedOV_From_M8_E32, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedOV_From_M8_E64, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedOV_From_MF2_E16, [TX82VADD]> {
  let Latency = 42;
  let ReleaseAtCycles = [42];
}
def : WriteRes<WriteVFRedOV_From_MF2_E32, [TX82VADD]> {
  let Latency = 42;
  let ReleaseAtCycles = [42];
}
def : WriteRes<WriteVFRedOV_From_MF4_E16, [TX82VADD]> {
  let Latency = 41;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVFRedOV_From_WorstCase, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedV_From_M1_E16, [TX82VADD]> {
  let Latency = 44;
  let ReleaseAtCycles = [44];
}
def : WriteRes<WriteVFRedV_From_M1_E32, [TX82VADD]> {
  let Latency = 44;
  let ReleaseAtCycles = [44];
}
def : WriteRes<WriteVFRedV_From_M1_E64, [TX82VADD]> {
  let Latency = 44;
  let ReleaseAtCycles = [44];
}
def : WriteRes<WriteVFRedV_From_M2_E16, [TX82VADD]> {
  let Latency = 48;
  let ReleaseAtCycles = [48];
}
def : WriteRes<WriteVFRedV_From_M2_E32, [TX82VADD]> {
  let Latency = 48;
  let ReleaseAtCycles = [48];
}
def : WriteRes<WriteVFRedV_From_M2_E64, [TX82VADD]> {
  let Latency = 48;
  let ReleaseAtCycles = [48];
}
def : WriteRes<WriteVFRedV_From_M4_E16, [TX82VADD]> {
  let Latency = 56;
  let ReleaseAtCycles = [56];
}
def : WriteRes<WriteVFRedV_From_M4_E32, [TX82VADD]> {
  let Latency = 56;
  let ReleaseAtCycles = [56];
}
def : WriteRes<WriteVFRedV_From_M4_E64, [TX82VADD]> {
  let Latency = 56;
  let ReleaseAtCycles = [56];
}
def : WriteRes<WriteVFRedV_From_M8_E16, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedV_From_M8_E32, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedV_From_M8_E64, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFRedV_From_MF2_E16, [TX82VADD]> {
  let Latency = 42;
  let ReleaseAtCycles = [42];
}
def : WriteRes<WriteVFRedV_From_MF2_E32, [TX82VADD]> {
  let Latency = 42;
  let ReleaseAtCycles = [42];
}
def : WriteRes<WriteVFRedV_From_MF4_E16, [TX82VADD]> {
  let Latency = 41;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVFRedV_From_WorstCase, [TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : WriteRes<WriteVFSgnjF_M1_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFSgnjF_M1_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFSgnjF_M1_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFSgnjF_M2_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFSgnjF_M2_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFSgnjF_M2_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFSgnjF_M4_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFSgnjF_M4_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFSgnjF_M4_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFSgnjF_M8_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjF_M8_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjF_M8_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjF_MF2_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFSgnjF_MF2_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFSgnjF_MF4_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFSgnjF_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjV_M1_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFSgnjV_M1_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFSgnjV_M1_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFSgnjV_M2_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFSgnjV_M2_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFSgnjV_M2_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFSgnjV_M4_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFSgnjV_M4_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFSgnjV_M4_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFSgnjV_M8_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjV_M8_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjV_M8_E64, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSgnjV_MF2_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFSgnjV_MF2_E32, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFSgnjV_MF4_E16, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFSgnjV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVFSlide1F_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVFSlide1F_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVFSlide1F_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVFSlide1F_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVFSlide1F_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVFSlide1F_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVFSlide1F_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVFSqrtV_M1_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [15];
}
def : WriteRes<WriteVFSqrtV_M1_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [15];
}
def : WriteRes<WriteVFSqrtV_M1_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [15];
}
def : WriteRes<WriteVFSqrtV_M2_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVFSqrtV_M2_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVFSqrtV_M2_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [19];
}
def : WriteRes<WriteVFSqrtV_M4_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFSqrtV_M4_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFSqrtV_M4_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [27];
}
def : WriteRes<WriteVFSqrtV_M8_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFSqrtV_M8_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFSqrtV_M8_E64, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFSqrtV_MF2_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFSqrtV_MF2_E32, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVFSqrtV_MF4_E16, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVFSqrtV_WorstCase, [TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : WriteRes<WriteVFWCvtFToFV_M1_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFWCvtFToFV_M1_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFWCvtFToFV_M2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFWCvtFToFV_M2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFWCvtFToFV_M4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFWCvtFToFV_M4_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFWCvtFToFV_MF2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFWCvtFToFV_MF2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFWCvtFToFV_MF4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFWCvtFToFV_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFWCvtIToFV_M1_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFWCvtIToFV_M1_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFWCvtIToFV_M1_E8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVFWCvtIToFV_M2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFWCvtIToFV_M2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFWCvtIToFV_M2_E8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVFWCvtIToFV_M4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFWCvtIToFV_M4_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFWCvtIToFV_M4_E8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVFWCvtIToFV_MF2_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFWCvtIToFV_MF2_E32, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFWCvtIToFV_MF2_E8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVFWCvtIToFV_MF4_E16, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFWCvtIToFV_MF4_E8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFWCvtIToFV_MF8_E8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVFWCvtIToFV_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIALUI_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVIALUI_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIALUI_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVIALUI_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIALUI_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIALUI_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIALUI_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIALUI_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIALUV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVIALUV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIALUV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVIALUV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIALUV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIALUV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIALUV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIALUV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIALUX_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVIALUX_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIALUX_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVIALUX_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIALUX_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIALUX_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIALUX_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIALUX_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUI_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICALUI_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICALUI_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICALUI_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUI_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICALUI_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUI_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUI_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUMI_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICALUMI_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICALUMI_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICALUMI_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUMI_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICALUMI_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUMI_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUMI_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUMV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICALUMV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICALUMV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICALUMV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUMV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICALUMV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUMV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUMV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUMX_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICALUMX_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICALUMX_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICALUMX_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUMX_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICALUMX_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUMX_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUMX_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICALUV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICALUV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICALUV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICALUV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUX_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICALUX_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICALUX_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICALUX_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICALUX_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICALUX_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUX_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICALUX_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICmpI_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICmpI_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICmpI_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICmpI_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICmpI_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICmpI_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICmpI_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICmpI_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICmpV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICmpV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICmpV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICmpV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICmpV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICmpV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICmpV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICmpV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICmpX_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVICmpX_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVICmpX_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVICmpX_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVICmpX_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVICmpX_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICmpX_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVICmpX_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIDivV_M1_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivV_M1_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivV_M1_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivV_M1_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivV_M2_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivV_M2_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivV_M2_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivV_M2_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivV_M4_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivV_M4_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivV_M4_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivV_M4_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivV_M8_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivV_M8_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivV_M8_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivV_M8_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivV_MF2_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVIDivV_MF2_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVIDivV_MF2_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVIDivV_MF4_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIDivV_MF4_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIDivV_MF8_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIDivV_WorstCase, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivX_M1_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivX_M1_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivX_M1_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivX_M1_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [49];
}
def : WriteRes<WriteVIDivX_M2_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivX_M2_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivX_M2_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivX_M2_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [81];
}
def : WriteRes<WriteVIDivX_M4_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivX_M4_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivX_M4_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivX_M4_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [145];
}
def : WriteRes<WriteVIDivX_M8_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivX_M8_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivX_M8_E64, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivX_M8_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIDivX_MF2_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVIDivX_MF2_E32, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVIDivX_MF2_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [33];
}
def : WriteRes<WriteVIDivX_MF4_E16, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIDivX_MF4_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIDivX_MF8_E8, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIDivX_WorstCase, [TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : WriteRes<WriteVIMergeI_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMergeI_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVIMergeI_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIMergeI_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMergeI_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVIMergeI_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMergeI_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMergeI_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMergeV_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMergeV_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVIMergeV_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIMergeV_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMergeV_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVIMergeV_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMergeV_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMergeV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMergeX_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMergeX_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVIMergeX_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIMergeX_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMergeX_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVIMergeX_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMergeX_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMergeX_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMinMaxV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVIMinMaxV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIMinMaxV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVIMinMaxV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIMinMaxV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMinMaxV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIMinMaxV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIMinMaxV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIMinMaxX_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVIMinMaxX_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIMinMaxX_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVIMinMaxX_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIMinMaxX_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMinMaxX_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIMinMaxX_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIMinMaxX_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVIMovI_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMovI_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVIMovI_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIMovI_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMovI_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVIMovI_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMovI_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMovI_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMovV_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMovV_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVIMovV_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIMovV_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMovV_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVIMovV_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMovV_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMovV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMovX_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMovX_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVIMovX_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVIMovX_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMovX_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVIMovX_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMovX_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVIMovX_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : WriteRes<WriteVIMulV_M1, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIMulV_M2, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [17];
}
def : WriteRes<WriteVIMulV_M4, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIMulV_M8, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVIMulV_MF2, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [11];
}
def : WriteRes<WriteVIMulV_MF4, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMulV_MF8, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMulV_WorstCase, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVIMulX_M1, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVIMulX_M2, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [17];
}
def : WriteRes<WriteVIMulX_M4, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [25];
}
def : WriteRes<WriteVIMulX_M8, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVIMulX_MF2, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [11];
}
def : WriteRes<WriteVIMulX_MF4, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMulX_MF8, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIMulX_WorstCase, [TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : WriteRes<WriteVISlide1X_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVISlide1X_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVISlide1X_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVISlide1X_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVISlide1X_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVISlide1X_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVISlide1X_MF8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVISlide1X_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVIdxV_M1, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIdxV_M2, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIdxV_M4, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteVIdxV_M8, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVIdxV_MF2, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVIdxV_MF4, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVIdxV_MF8, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVIdxV_WorstCase, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVIotaV_M1, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVIotaV_M2, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVIotaV_M4, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteVIotaV_M8, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVIotaV_MF2, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVIotaV_MF4, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVIotaV_MF8, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVIotaV_WorstCase, [TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVLD1R, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVLD2R, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVLD4R, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVLD8R, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDE_M1, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVLDE_M2, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVLDE_M4, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVLDE_M8, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDE_MF2, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVLDE_MF4, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVLDE_MF8, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVLDE_WorstCase, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDS16_M1, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVLDS16_M2, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVLDS16_M4, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVLDS16_M8, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDS16_MF2, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVLDS16_MF4, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVLDS16_WorstCase, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDS32_M1, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVLDS32_M2, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVLDS32_M4, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVLDS32_M8, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDS32_MF2, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVLDS32_WorstCase, [TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVLDUX16_M1, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVLDUX16_M2, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVLDUX16_M4, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteVLDUX16_M8, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVLDUX16_MF2, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVLDUX16_MF4, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVLDUX16_WorstCase, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVLDUX32_M1, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVLDUX32_M2, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVLDUX32_M4, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteVLDUX32_M8, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVLDUX32_MF2, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVLDUX32_WorstCase, [TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVMALUV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVMALUV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVMALUV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVMALUV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVMALUV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMALUV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVMALUV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVMALUV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVMFFSV_M1, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_M2, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_M4, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_M8, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_MF2, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_MF4, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_MF8, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMFFSV_WorstCase, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMPopV_M1, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_M2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_M4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_M8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_MF2, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_MF4, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_MF8, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMPopV_WorstCase, [TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMSFSV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVMSFSV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVMSFSV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVMSFSV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVMSFSV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVMSFSV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVMSFSV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVMSFSV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVMov1V, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVMov2V, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : WriteRes<WriteVMov4V, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : WriteRes<WriteVMov8V, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : WriteRes<WriteVMovFS, [TX82VMISC]> {
  let Latency = 8;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVMovSF, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMovSX, [TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVMovXS, [TX82VMISC]> {
  let Latency = 8;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVNShiftI_M1, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVNShiftI_M2, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [14];
}
def : WriteRes<WriteVNShiftI_M4, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVNShiftI_MF2, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVNShiftI_MF4, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVNShiftI_MF8, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVNShiftI_WorstCase, [TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : WriteRes<WriteVRGatherEI16VV_M1_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherEI16VV_M1_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherEI16VV_M1_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherEI16VV_M1_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherEI16VV_M2_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherEI16VV_M2_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherEI16VV_M2_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherEI16VV_M2_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherEI16VV_M4_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherEI16VV_M4_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherEI16VV_M4_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherEI16VV_M4_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherEI16VV_M8_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherEI16VV_M8_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherEI16VV_M8_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherEI16VV_M8_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherEI16VV_MF2_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherEI16VV_MF2_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherEI16VV_MF2_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherEI16VV_MF4_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherEI16VV_MF4_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherEI16VV_MF8_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherEI16VV_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVI_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherVI_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherVI_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherVI_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVI_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherVI_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVI_MF8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVI_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVV_M1_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherVV_M1_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherVV_M1_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherVV_M1_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherVV_M2_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherVV_M2_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherVV_M2_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherVV_M2_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherVV_M4_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherVV_M4_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherVV_M4_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherVV_M4_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherVV_M8_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVV_M8_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVV_M8_E64, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVV_M8_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVV_MF2_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherVV_MF2_E32, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherVV_MF2_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherVV_MF4_E16, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVV_MF4_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVV_MF8_E8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVV_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVX_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVRGatherVX_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVRGatherVX_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVRGatherVX_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVRGatherVX_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVRGatherVX_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVX_MF8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVRGatherVX_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVSETIVLI, [TX82VDISPATCH]>;
def : WriteRes<WriteVSETVL, [TX82VDISPATCH]>;
def : WriteRes<WriteVSETVLI, [TX82VDISPATCH]>;
def : WriteRes<WriteVST1R, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVST2R, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVST4R, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVST8R, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTE_M1, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVSTE_M2, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVSTE_M4, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVSTE_M8, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTE_MF2, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVSTE_MF4, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVSTE_MF8, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVSTE_WorstCase, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTS16_M1, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVSTS16_M2, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVSTS16_M4, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVSTS16_M8, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTS16_MF2, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVSTS16_MF4, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVSTS16_WorstCase, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTS32_M1, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [4];
}
def : WriteRes<WriteVSTS32_M2, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [8];
}
def : WriteRes<WriteVSTS32_M4, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : WriteRes<WriteVSTS32_M8, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTS32_MF2, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [3];
}
def : WriteRes<WriteVSTS32_WorstCase, [TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : WriteRes<WriteVSTUX16_M1, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVSTUX16_M2, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVSTUX16_M4, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteVSTUX16_M8, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVSTUX16_MF2, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVSTUX16_MF4, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVSTUX16_WorstCase, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVSTUX32_M1, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVSTUX32_M2, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [10];
}
def : WriteRes<WriteVSTUX32_M4, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [18];
}
def : WriteRes<WriteVSTUX32_M8, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVSTUX32_MF2, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [5];
}
def : WriteRes<WriteVSTUX32_WorstCase, [TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVShiftI_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVShiftI_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVShiftI_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVShiftI_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVShiftI_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVShiftI_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVShiftI_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVShiftI_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVShiftV_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVShiftV_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVShiftV_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVShiftV_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVShiftV_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVShiftV_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVShiftV_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVShiftV_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVShiftX_M1, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [9];
}
def : WriteRes<WriteVShiftX_M2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [13];
}
def : WriteRes<WriteVShiftX_M4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [21];
}
def : WriteRes<WriteVShiftX_M8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVShiftX_MF2, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [7];
}
def : WriteRes<WriteVShiftX_MF4, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVShiftX_MF8, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}
def : WriteRes<WriteVShiftX_WorstCase, [TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : WriteRes<WriteVSlideDownX_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVSlideDownX_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVSlideDownX_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVSlideDownX_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVSlideDownX_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVSlideDownX_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVSlideDownX_MF8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVSlideDownX_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVSlideI_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVSlideI_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVSlideI_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVSlideI_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVSlideI_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVSlideI_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVSlideI_MF8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVSlideI_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVSlideUpX_M1, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [50];
}
def : WriteRes<WriteVSlideUpX_M2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [82];
}
def : WriteRes<WriteVSlideUpX_M4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [146];
}
def : WriteRes<WriteVSlideUpX_M8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : WriteRes<WriteVSlideUpX_MF2, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [34];
}
def : WriteRes<WriteVSlideUpX_MF4, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVSlideUpX_MF8, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [26];
}
def : WriteRes<WriteVSlideUpX_WorstCase, [TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}

// Unsupported on tsm-x82.
let Unsupported = true in {
def : WriteRes<WriteAtomicB, []>;
def : WriteRes<WriteAtomicH, []>;
def : WriteRes<WriteBREV8, []>;
def : WriteRes<WriteFAdd128, []>;
def : WriteRes<WriteFClass128, []>;
def : WriteRes<WriteFCmp128, []>;
def : WriteRes<WriteFCvtF128ToF32, []>;
def : WriteRes<WriteFCvtF128ToF64, []>;
def : WriteRes<WriteFCvtF128ToI32, []>;
def : WriteRes<WriteFCvtF128ToI64, []>;
def : WriteRes<WriteFCvtF32ToF128, []>;
def : WriteRes<WriteFCvtF64ToF128, []>;
def : WriteRes<WriteFCvtI32ToF128, []>;
def : WriteRes<WriteFCvtI64ToF128, []>;
def : WriteRes<WriteFDiv128, []>;
def : WriteRes<WriteFLD128, []>;
def : WriteRes<WriteFLI128, []>;
def : WriteRes<WriteFLI16, []>;
def : WriteRes<WriteFLI32, []>;
def : WriteRes<WriteFLI64, []>;
def : WriteRes<WriteFMA128, []>;
def : WriteRes<WriteFMinMax128, []>;
def : WriteRes<WriteFMul128, []>;
def : WriteRes<WriteFRoundF128, []>;
def : WriteRes<WriteFRoundF16, []>;
def : WriteRes<WriteFRoundF32, []>;
def : WriteRes<WriteFRoundF64, []>;
def : WriteRes<WriteFSGNJ128, []>;
def : WriteRes<WriteFST128, []>;
def : WriteRes<WriteFSqrt128, []>;
def : WriteRes<WritePACK, []>;
def : WriteRes<WritePACK32, []>;
def : WriteRes<WriteRdVLENB, []>;
def : WriteRes<WriteSF_VFNRClipV_M1, []>;
def : WriteRes<WriteSF_VFNRClipV_M2, []>;
def : WriteRes<WriteSF_VFNRClipV_MF2, []>;
def : WriteRes<WriteSF_VFNRClipV_MF4, []>;
def : WriteRes<WriteSF_VFNRClipV_MF8, []>;
def : WriteRes<WriteSF_VFNRClipV_WorstCase, []>;
def : WriteRes<WriteSF_VFWMACC_QQQ_M1, []>;
def : WriteRes<WriteSF_VFWMACC_QQQ_M2, []>;
def : WriteRes<WriteSF_VFWMACC_QQQ_M4, []>;
def : WriteRes<WriteSF_VFWMACC_QQQ_MF2, []>;
def : WriteRes<WriteSF_VFWMACC_QQQ_MF4, []>;
def : WriteRes<WriteSF_VFWMACC_QQQ_WorstCase, []>;
def : WriteRes<WriteSF_VQMACC_DOD_M1, []>;
def : WriteRes<WriteSF_VQMACC_DOD_M2, []>;
def : WriteRes<WriteSF_VQMACC_DOD_M4, []>;
def : WriteRes<WriteSF_VQMACC_DOD_M8, []>;
def : WriteRes<WriteSF_VQMACC_DOD_WorstCase, []>;
def : WriteRes<WriteSF_VQMACC_QOQ_M1, []>;
def : WriteRes<WriteSF_VQMACC_QOQ_M2, []>;
def : WriteRes<WriteSF_VQMACC_QOQ_M4, []>;
def : WriteRes<WriteSF_VQMACC_QOQ_MF2, []>;
def : WriteRes<WriteSF_VQMACC_QOQ_WorstCase, []>;
def : WriteRes<WriteVAALUV_M1, []>;
def : WriteRes<WriteVAALUV_M2, []>;
def : WriteRes<WriteVAALUV_M4, []>;
def : WriteRes<WriteVAALUV_M8, []>;
def : WriteRes<WriteVAALUV_MF2, []>;
def : WriteRes<WriteVAALUV_MF4, []>;
def : WriteRes<WriteVAALUV_MF8, []>;
def : WriteRes<WriteVAALUV_WorstCase, []>;
def : WriteRes<WriteVAALUX_M1, []>;
def : WriteRes<WriteVAALUX_M2, []>;
def : WriteRes<WriteVAALUX_M4, []>;
def : WriteRes<WriteVAALUX_M8, []>;
def : WriteRes<WriteVAALUX_MF2, []>;
def : WriteRes<WriteVAALUX_MF4, []>;
def : WriteRes<WriteVAALUX_MF8, []>;
def : WriteRes<WriteVAALUX_WorstCase, []>;
def : WriteRes<WriteVAESKF1V_M1, []>;
def : WriteRes<WriteVAESKF1V_M2, []>;
def : WriteRes<WriteVAESKF1V_M4, []>;
def : WriteRes<WriteVAESKF1V_M8, []>;
def : WriteRes<WriteVAESKF1V_MF2, []>;
def : WriteRes<WriteVAESKF1V_WorstCase, []>;
def : WriteRes<WriteVAESKF2V_M1, []>;
def : WriteRes<WriteVAESKF2V_M2, []>;
def : WriteRes<WriteVAESKF2V_M4, []>;
def : WriteRes<WriteVAESKF2V_M8, []>;
def : WriteRes<WriteVAESKF2V_MF2, []>;
def : WriteRes<WriteVAESKF2V_WorstCase, []>;
def : WriteRes<WriteVAESMVV_M1, []>;
def : WriteRes<WriteVAESMVV_M2, []>;
def : WriteRes<WriteVAESMVV_M4, []>;
def : WriteRes<WriteVAESMVV_M8, []>;
def : WriteRes<WriteVAESMVV_MF2, []>;
def : WriteRes<WriteVAESMVV_WorstCase, []>;
def : WriteRes<WriteVAESZV_M1, []>;
def : WriteRes<WriteVAESZV_M2, []>;
def : WriteRes<WriteVAESZV_M4, []>;
def : WriteRes<WriteVAESZV_M8, []>;
def : WriteRes<WriteVAESZV_MF2, []>;
def : WriteRes<WriteVAESZV_WorstCase, []>;
def : WriteRes<WriteVBREV8V_M1, []>;
def : WriteRes<WriteVBREV8V_M2, []>;
def : WriteRes<WriteVBREV8V_M4, []>;
def : WriteRes<WriteVBREV8V_M8, []>;
def : WriteRes<WriteVBREV8V_MF2, []>;
def : WriteRes<WriteVBREV8V_MF4, []>;
def : WriteRes<WriteVBREV8V_MF8, []>;
def : WriteRes<WriteVBREVV_M1, []>;
def : WriteRes<WriteVBREVV_M2, []>;
def : WriteRes<WriteVBREVV_M4, []>;
def : WriteRes<WriteVBREVV_M8, []>;
def : WriteRes<WriteVBREVV_MF2, []>;
def : WriteRes<WriteVBREVV_MF4, []>;
def : WriteRes<WriteVBREVV_MF8, []>;
def : WriteRes<WriteVCLMULV_M1, []>;
def : WriteRes<WriteVCLMULV_M2, []>;
def : WriteRes<WriteVCLMULV_M4, []>;
def : WriteRes<WriteVCLMULV_M8, []>;
def : WriteRes<WriteVCLMULV_MF2, []>;
def : WriteRes<WriteVCLMULV_MF4, []>;
def : WriteRes<WriteVCLMULV_MF8, []>;
def : WriteRes<WriteVCLMULV_WorstCase, []>;
def : WriteRes<WriteVCLMULX_M1, []>;
def : WriteRes<WriteVCLMULX_M2, []>;
def : WriteRes<WriteVCLMULX_M4, []>;
def : WriteRes<WriteVCLMULX_M8, []>;
def : WriteRes<WriteVCLMULX_MF2, []>;
def : WriteRes<WriteVCLMULX_MF4, []>;
def : WriteRes<WriteVCLMULX_MF8, []>;
def : WriteRes<WriteVCLMULX_WorstCase, []>;
def : WriteRes<WriteVCLZV_M1, []>;
def : WriteRes<WriteVCLZV_M2, []>;
def : WriteRes<WriteVCLZV_M4, []>;
def : WriteRes<WriteVCLZV_M8, []>;
def : WriteRes<WriteVCLZV_MF2, []>;
def : WriteRes<WriteVCLZV_MF4, []>;
def : WriteRes<WriteVCLZV_MF8, []>;
def : WriteRes<WriteVCPOPV_M1, []>;
def : WriteRes<WriteVCPOPV_M2, []>;
def : WriteRes<WriteVCPOPV_M4, []>;
def : WriteRes<WriteVCPOPV_M8, []>;
def : WriteRes<WriteVCPOPV_MF2, []>;
def : WriteRes<WriteVCPOPV_MF4, []>;
def : WriteRes<WriteVCPOPV_MF8, []>;
def : WriteRes<WriteVCTZV_M1, []>;
def : WriteRes<WriteVCTZV_M2, []>;
def : WriteRes<WriteVCTZV_M4, []>;
def : WriteRes<WriteVCTZV_M8, []>;
def : WriteRes<WriteVCTZV_MF2, []>;
def : WriteRes<WriteVCTZV_MF4, []>;
def : WriteRes<WriteVCTZV_MF8, []>;
def : WriteRes<WriteVC_FPR16VV_M1, []>;
def : WriteRes<WriteVC_FPR16VV_M2, []>;
def : WriteRes<WriteVC_FPR16VV_M4, []>;
def : WriteRes<WriteVC_FPR16VV_M8, []>;
def : WriteRes<WriteVC_FPR16VV_MF2, []>;
def : WriteRes<WriteVC_FPR16VV_MF4, []>;
def : WriteRes<WriteVC_FPR16VW_M1, []>;
def : WriteRes<WriteVC_FPR16VW_M2, []>;
def : WriteRes<WriteVC_FPR16VW_M4, []>;
def : WriteRes<WriteVC_FPR16VW_M8, []>;
def : WriteRes<WriteVC_FPR16VW_MF2, []>;
def : WriteRes<WriteVC_FPR16VW_MF4, []>;
def : WriteRes<WriteVC_FPR16V_M1, []>;
def : WriteRes<WriteVC_FPR16V_M2, []>;
def : WriteRes<WriteVC_FPR16V_M4, []>;
def : WriteRes<WriteVC_FPR16V_M8, []>;
def : WriteRes<WriteVC_FPR16V_MF2, []>;
def : WriteRes<WriteVC_FPR16V_MF4, []>;
def : WriteRes<WriteVC_FPR32VV_M1, []>;
def : WriteRes<WriteVC_FPR32VV_M2, []>;
def : WriteRes<WriteVC_FPR32VV_M4, []>;
def : WriteRes<WriteVC_FPR32VV_M8, []>;
def : WriteRes<WriteVC_FPR32VV_MF2, []>;
def : WriteRes<WriteVC_FPR32VW_M1, []>;
def : WriteRes<WriteVC_FPR32VW_M2, []>;
def : WriteRes<WriteVC_FPR32VW_M4, []>;
def : WriteRes<WriteVC_FPR32VW_M8, []>;
def : WriteRes<WriteVC_FPR32VW_MF2, []>;
def : WriteRes<WriteVC_FPR32V_M1, []>;
def : WriteRes<WriteVC_FPR32V_M2, []>;
def : WriteRes<WriteVC_FPR32V_M4, []>;
def : WriteRes<WriteVC_FPR32V_M8, []>;
def : WriteRes<WriteVC_FPR32V_MF2, []>;
def : WriteRes<WriteVC_FPR64VV_M1, []>;
def : WriteRes<WriteVC_FPR64VV_M2, []>;
def : WriteRes<WriteVC_FPR64VV_M4, []>;
def : WriteRes<WriteVC_FPR64VV_M8, []>;
def : WriteRes<WriteVC_FPR64V_M1, []>;
def : WriteRes<WriteVC_FPR64V_M2, []>;
def : WriteRes<WriteVC_FPR64V_M4, []>;
def : WriteRes<WriteVC_FPR64V_M8, []>;
def : WriteRes<WriteVC_IVV_M1, []>;
def : WriteRes<WriteVC_IVV_M2, []>;
def : WriteRes<WriteVC_IVV_M4, []>;
def : WriteRes<WriteVC_IVV_M8, []>;
def : WriteRes<WriteVC_IVV_MF2, []>;
def : WriteRes<WriteVC_IVV_MF4, []>;
def : WriteRes<WriteVC_IVV_MF8, []>;
def : WriteRes<WriteVC_IVW_M1, []>;
def : WriteRes<WriteVC_IVW_M2, []>;
def : WriteRes<WriteVC_IVW_M4, []>;
def : WriteRes<WriteVC_IVW_MF2, []>;
def : WriteRes<WriteVC_IVW_MF4, []>;
def : WriteRes<WriteVC_IVW_MF8, []>;
def : WriteRes<WriteVC_IV_M1, []>;
def : WriteRes<WriteVC_IV_M2, []>;
def : WriteRes<WriteVC_IV_M4, []>;
def : WriteRes<WriteVC_IV_M8, []>;
def : WriteRes<WriteVC_IV_MF2, []>;
def : WriteRes<WriteVC_IV_MF4, []>;
def : WriteRes<WriteVC_IV_MF8, []>;
def : WriteRes<WriteVC_I_M1, []>;
def : WriteRes<WriteVC_I_M2, []>;
def : WriteRes<WriteVC_I_M4, []>;
def : WriteRes<WriteVC_I_M8, []>;
def : WriteRes<WriteVC_I_MF2, []>;
def : WriteRes<WriteVC_I_MF4, []>;
def : WriteRes<WriteVC_I_MF8, []>;
def : WriteRes<WriteVC_VVV_M1, []>;
def : WriteRes<WriteVC_VVV_M2, []>;
def : WriteRes<WriteVC_VVV_M4, []>;
def : WriteRes<WriteVC_VVV_M8, []>;
def : WriteRes<WriteVC_VVV_MF2, []>;
def : WriteRes<WriteVC_VVV_MF4, []>;
def : WriteRes<WriteVC_VVV_MF8, []>;
def : WriteRes<WriteVC_VVW_M1, []>;
def : WriteRes<WriteVC_VVW_M2, []>;
def : WriteRes<WriteVC_VVW_M4, []>;
def : WriteRes<WriteVC_VVW_MF2, []>;
def : WriteRes<WriteVC_VVW_MF4, []>;
def : WriteRes<WriteVC_VVW_MF8, []>;
def : WriteRes<WriteVC_VV_M1, []>;
def : WriteRes<WriteVC_VV_M2, []>;
def : WriteRes<WriteVC_VV_M4, []>;
def : WriteRes<WriteVC_VV_M8, []>;
def : WriteRes<WriteVC_VV_MF2, []>;
def : WriteRes<WriteVC_VV_MF4, []>;
def : WriteRes<WriteVC_VV_MF8, []>;
def : WriteRes<WriteVC_V_FPR16VV_M1, []>;
def : WriteRes<WriteVC_V_FPR16VV_M2, []>;
def : WriteRes<WriteVC_V_FPR16VV_M4, []>;
def : WriteRes<WriteVC_V_FPR16VV_M8, []>;
def : WriteRes<WriteVC_V_FPR16VV_MF2, []>;
def : WriteRes<WriteVC_V_FPR16VV_MF4, []>;
def : WriteRes<WriteVC_V_FPR16VW_M1, []>;
def : WriteRes<WriteVC_V_FPR16VW_M2, []>;
def : WriteRes<WriteVC_V_FPR16VW_M4, []>;
def : WriteRes<WriteVC_V_FPR16VW_M8, []>;
def : WriteRes<WriteVC_V_FPR16VW_MF2, []>;
def : WriteRes<WriteVC_V_FPR16VW_MF4, []>;
def : WriteRes<WriteVC_V_FPR16V_M1, []>;
def : WriteRes<WriteVC_V_FPR16V_M2, []>;
def : WriteRes<WriteVC_V_FPR16V_M4, []>;
def : WriteRes<WriteVC_V_FPR16V_M8, []>;
def : WriteRes<WriteVC_V_FPR16V_MF2, []>;
def : WriteRes<WriteVC_V_FPR16V_MF4, []>;
def : WriteRes<WriteVC_V_FPR32VV_M1, []>;
def : WriteRes<WriteVC_V_FPR32VV_M2, []>;
def : WriteRes<WriteVC_V_FPR32VV_M4, []>;
def : WriteRes<WriteVC_V_FPR32VV_M8, []>;
def : WriteRes<WriteVC_V_FPR32VV_MF2, []>;
def : WriteRes<WriteVC_V_FPR32VW_M1, []>;
def : WriteRes<WriteVC_V_FPR32VW_M2, []>;
def : WriteRes<WriteVC_V_FPR32VW_M4, []>;
def : WriteRes<WriteVC_V_FPR32VW_M8, []>;
def : WriteRes<WriteVC_V_FPR32VW_MF2, []>;
def : WriteRes<WriteVC_V_FPR32V_M1, []>;
def : WriteRes<WriteVC_V_FPR32V_M2, []>;
def : WriteRes<WriteVC_V_FPR32V_M4, []>;
def : WriteRes<WriteVC_V_FPR32V_M8, []>;
def : WriteRes<WriteVC_V_FPR32V_MF2, []>;
def : WriteRes<WriteVC_V_FPR64VV_M1, []>;
def : WriteRes<WriteVC_V_FPR64VV_M2, []>;
def : WriteRes<WriteVC_V_FPR64VV_M4, []>;
def : WriteRes<WriteVC_V_FPR64VV_M8, []>;
def : WriteRes<WriteVC_V_FPR64V_M1, []>;
def : WriteRes<WriteVC_V_FPR64V_M2, []>;
def : WriteRes<WriteVC_V_FPR64V_M4, []>;
def : WriteRes<WriteVC_V_FPR64V_M8, []>;
def : WriteRes<WriteVC_V_IVV_M1, []>;
def : WriteRes<WriteVC_V_IVV_M2, []>;
def : WriteRes<WriteVC_V_IVV_M4, []>;
def : WriteRes<WriteVC_V_IVV_M8, []>;
def : WriteRes<WriteVC_V_IVV_MF2, []>;
def : WriteRes<WriteVC_V_IVV_MF4, []>;
def : WriteRes<WriteVC_V_IVV_MF8, []>;
def : WriteRes<WriteVC_V_IVW_M1, []>;
def : WriteRes<WriteVC_V_IVW_M2, []>;
def : WriteRes<WriteVC_V_IVW_M4, []>;
def : WriteRes<WriteVC_V_IVW_MF2, []>;
def : WriteRes<WriteVC_V_IVW_MF4, []>;
def : WriteRes<WriteVC_V_IVW_MF8, []>;
def : WriteRes<WriteVC_V_IV_M1, []>;
def : WriteRes<WriteVC_V_IV_M2, []>;
def : WriteRes<WriteVC_V_IV_M4, []>;
def : WriteRes<WriteVC_V_IV_M8, []>;
def : WriteRes<WriteVC_V_IV_MF2, []>;
def : WriteRes<WriteVC_V_IV_MF4, []>;
def : WriteRes<WriteVC_V_IV_MF8, []>;
def : WriteRes<WriteVC_V_I_M1, []>;
def : WriteRes<WriteVC_V_I_M2, []>;
def : WriteRes<WriteVC_V_I_M4, []>;
def : WriteRes<WriteVC_V_I_M8, []>;
def : WriteRes<WriteVC_V_I_MF2, []>;
def : WriteRes<WriteVC_V_I_MF4, []>;
def : WriteRes<WriteVC_V_I_MF8, []>;
def : WriteRes<WriteVC_V_VVV_M1, []>;
def : WriteRes<WriteVC_V_VVV_M2, []>;
def : WriteRes<WriteVC_V_VVV_M4, []>;
def : WriteRes<WriteVC_V_VVV_M8, []>;
def : WriteRes<WriteVC_V_VVV_MF2, []>;
def : WriteRes<WriteVC_V_VVV_MF4, []>;
def : WriteRes<WriteVC_V_VVV_MF8, []>;
def : WriteRes<WriteVC_V_VVW_M1, []>;
def : WriteRes<WriteVC_V_VVW_M2, []>;
def : WriteRes<WriteVC_V_VVW_M4, []>;
def : WriteRes<WriteVC_V_VVW_MF2, []>;
def : WriteRes<WriteVC_V_VVW_MF4, []>;
def : WriteRes<WriteVC_V_VVW_MF8, []>;
def : WriteRes<WriteVC_V_VV_M1, []>;
def : WriteRes<WriteVC_V_VV_M2, []>;
def : WriteRes<WriteVC_V_VV_M4, []>;
def : WriteRes<WriteVC_V_VV_M8, []>;
def : WriteRes<WriteVC_V_VV_MF2, []>;
def : WriteRes<WriteVC_V_VV_MF4, []>;
def : WriteRes<WriteVC_V_VV_MF8, []>;
def : WriteRes<WriteVC_V_XVV_M1, []>;
def : WriteRes<WriteVC_V_XVV_M2, []>;
def : WriteRes<WriteVC_V_XVV_M4, []>;
def : WriteRes<WriteVC_V_XVV_M8, []>;
def : WriteRes<WriteVC_V_XVV_MF2, []>;
def : WriteRes<WriteVC_V_XVV_MF4, []>;
def : WriteRes<WriteVC_V_XVV_MF8, []>;
def : WriteRes<WriteVC_V_XVW_M1, []>;
def : WriteRes<WriteVC_V_XVW_M2, []>;
def : WriteRes<WriteVC_V_XVW_M4, []>;
def : WriteRes<WriteVC_V_XVW_MF2, []>;
def : WriteRes<WriteVC_V_XVW_MF4, []>;
def : WriteRes<WriteVC_V_XVW_MF8, []>;
def : WriteRes<WriteVC_V_XV_M1, []>;
def : WriteRes<WriteVC_V_XV_M2, []>;
def : WriteRes<WriteVC_V_XV_M4, []>;
def : WriteRes<WriteVC_V_XV_M8, []>;
def : WriteRes<WriteVC_V_XV_MF2, []>;
def : WriteRes<WriteVC_V_XV_MF4, []>;
def : WriteRes<WriteVC_V_XV_MF8, []>;
def : WriteRes<WriteVC_V_X_M1, []>;
def : WriteRes<WriteVC_V_X_M2, []>;
def : WriteRes<WriteVC_V_X_M4, []>;
def : WriteRes<WriteVC_V_X_M8, []>;
def : WriteRes<WriteVC_V_X_MF2, []>;
def : WriteRes<WriteVC_V_X_MF4, []>;
def : WriteRes<WriteVC_V_X_MF8, []>;
def : WriteRes<WriteVC_XVV_M1, []>;
def : WriteRes<WriteVC_XVV_M2, []>;
def : WriteRes<WriteVC_XVV_M4, []>;
def : WriteRes<WriteVC_XVV_M8, []>;
def : WriteRes<WriteVC_XVV_MF2, []>;
def : WriteRes<WriteVC_XVV_MF4, []>;
def : WriteRes<WriteVC_XVV_MF8, []>;
def : WriteRes<WriteVC_XVW_M1, []>;
def : WriteRes<WriteVC_XVW_M2, []>;
def : WriteRes<WriteVC_XVW_M4, []>;
def : WriteRes<WriteVC_XVW_MF2, []>;
def : WriteRes<WriteVC_XVW_MF4, []>;
def : WriteRes<WriteVC_XVW_MF8, []>;
def : WriteRes<WriteVC_XV_M1, []>;
def : WriteRes<WriteVC_XV_M2, []>;
def : WriteRes<WriteVC_XV_M4, []>;
def : WriteRes<WriteVC_XV_M8, []>;
def : WriteRes<WriteVC_XV_MF2, []>;
def : WriteRes<WriteVC_XV_MF4, []>;
def : WriteRes<WriteVC_XV_MF8, []>;
def : WriteRes<WriteVC_X_M1, []>;
def : WriteRes<WriteVC_X_M2, []>;
def : WriteRes<WriteVC_X_M4, []>;
def : WriteRes<WriteVC_X_M8, []>;
def : WriteRes<WriteVC_X_MF2, []>;
def : WriteRes<WriteVC_X_MF4, []>;
def : WriteRes<WriteVC_X_MF8, []>;
def : WriteRes<WriteVExtV_M1, []>;
def : WriteRes<WriteVExtV_M2, []>;
def : WriteRes<WriteVExtV_M4, []>;
def : WriteRes<WriteVExtV_M8, []>;
def : WriteRes<WriteVExtV_MF2, []>;
def : WriteRes<WriteVExtV_MF4, []>;
def : WriteRes<WriteVExtV_WorstCase, []>;
def : WriteRes<WriteVFClassV_M1, []>;
def : WriteRes<WriteVFClassV_M2, []>;
def : WriteRes<WriteVFClassV_M4, []>;
def : WriteRes<WriteVFClassV_M8, []>;
def : WriteRes<WriteVFClassV_MF2, []>;
def : WriteRes<WriteVFClassV_MF4, []>;
def : WriteRes<WriteVFClassV_WorstCase, []>;
def : WriteRes<WriteVFDivF_M1_E16, []>;
def : WriteRes<WriteVFDivF_M1_E32, []>;
def : WriteRes<WriteVFDivF_M1_E64, []>;
def : WriteRes<WriteVFDivF_M2_E16, []>;
def : WriteRes<WriteVFDivF_M2_E32, []>;
def : WriteRes<WriteVFDivF_M2_E64, []>;
def : WriteRes<WriteVFDivF_M4_E16, []>;
def : WriteRes<WriteVFDivF_M4_E32, []>;
def : WriteRes<WriteVFDivF_M4_E64, []>;
def : WriteRes<WriteVFDivF_M8_E16, []>;
def : WriteRes<WriteVFDivF_M8_E32, []>;
def : WriteRes<WriteVFDivF_M8_E64, []>;
def : WriteRes<WriteVFDivF_MF2_E16, []>;
def : WriteRes<WriteVFDivF_MF2_E32, []>;
def : WriteRes<WriteVFDivF_MF4_E16, []>;
def : WriteRes<WriteVFDivF_WorstCase, []>;
def : WriteRes<WriteVFDivV_M1_E16, []>;
def : WriteRes<WriteVFDivV_M1_E32, []>;
def : WriteRes<WriteVFDivV_M1_E64, []>;
def : WriteRes<WriteVFDivV_M2_E16, []>;
def : WriteRes<WriteVFDivV_M2_E32, []>;
def : WriteRes<WriteVFDivV_M2_E64, []>;
def : WriteRes<WriteVFDivV_M4_E16, []>;
def : WriteRes<WriteVFDivV_M4_E32, []>;
def : WriteRes<WriteVFDivV_M4_E64, []>;
def : WriteRes<WriteVFDivV_M8_E16, []>;
def : WriteRes<WriteVFDivV_M8_E32, []>;
def : WriteRes<WriteVFDivV_M8_E64, []>;
def : WriteRes<WriteVFDivV_MF2_E16, []>;
def : WriteRes<WriteVFDivV_MF2_E32, []>;
def : WriteRes<WriteVFDivV_MF4_E16, []>;
def : WriteRes<WriteVFDivV_WorstCase, []>;
def : WriteRes<WriteVFMulAddF_M1_E16, []>;
def : WriteRes<WriteVFMulAddF_M1_E32, []>;
def : WriteRes<WriteVFMulAddF_M1_E64, []>;
def : WriteRes<WriteVFMulAddF_M2_E16, []>;
def : WriteRes<WriteVFMulAddF_M2_E32, []>;
def : WriteRes<WriteVFMulAddF_M2_E64, []>;
def : WriteRes<WriteVFMulAddF_M4_E16, []>;
def : WriteRes<WriteVFMulAddF_M4_E32, []>;
def : WriteRes<WriteVFMulAddF_M4_E64, []>;
def : WriteRes<WriteVFMulAddF_M8_E16, []>;
def : WriteRes<WriteVFMulAddF_M8_E32, []>;
def : WriteRes<WriteVFMulAddF_M8_E64, []>;
def : WriteRes<WriteVFMulAddF_MF2_E16, []>;
def : WriteRes<WriteVFMulAddF_MF2_E32, []>;
def : WriteRes<WriteVFMulAddF_MF4_E16, []>;
def : WriteRes<WriteVFMulAddF_WorstCase, []>;
def : WriteRes<WriteVFMulAddV_M1_E16, []>;
def : WriteRes<WriteVFMulAddV_M1_E32, []>;
def : WriteRes<WriteVFMulAddV_M1_E64, []>;
def : WriteRes<WriteVFMulAddV_M2_E16, []>;
def : WriteRes<WriteVFMulAddV_M2_E32, []>;
def : WriteRes<WriteVFMulAddV_M2_E64, []>;
def : WriteRes<WriteVFMulAddV_M4_E16, []>;
def : WriteRes<WriteVFMulAddV_M4_E32, []>;
def : WriteRes<WriteVFMulAddV_M4_E64, []>;
def : WriteRes<WriteVFMulAddV_M8_E16, []>;
def : WriteRes<WriteVFMulAddV_M8_E32, []>;
def : WriteRes<WriteVFMulAddV_M8_E64, []>;
def : WriteRes<WriteVFMulAddV_MF2_E16, []>;
def : WriteRes<WriteVFMulAddV_MF2_E32, []>;
def : WriteRes<WriteVFMulAddV_MF4_E16, []>;
def : WriteRes<WriteVFMulAddV_WorstCase, []>;
def : WriteRes<WriteVFNCvtIToFV_M1_E16, []>;
def : WriteRes<WriteVFNCvtIToFV_M1_E32, []>;
def : WriteRes<WriteVFNCvtIToFV_M2_E16, []>;
def : WriteRes<WriteVFNCvtIToFV_M2_E32, []>;
def : WriteRes<WriteVFNCvtIToFV_M4_E16, []>;
def : WriteRes<WriteVFNCvtIToFV_M4_E32, []>;
def : WriteRes<WriteVFNCvtIToFV_MF2_E16, []>;
def : WriteRes<WriteVFNCvtIToFV_MF2_E32, []>;
def : WriteRes<WriteVFNCvtIToFV_MF4_E16, []>;
def : WriteRes<WriteVFNCvtIToFV_WorstCase, []>;
def : WriteRes<WriteVFWALUF_M1_E16, []>;
def : WriteRes<WriteVFWALUF_M1_E32, []>;
def : WriteRes<WriteVFWALUF_M2_E16, []>;
def : WriteRes<WriteVFWALUF_M2_E32, []>;
def : WriteRes<WriteVFWALUF_M4_E16, []>;
def : WriteRes<WriteVFWALUF_M4_E32, []>;
def : WriteRes<WriteVFWALUF_MF2_E16, []>;
def : WriteRes<WriteVFWALUF_MF2_E32, []>;
def : WriteRes<WriteVFWALUF_MF4_E16, []>;
def : WriteRes<WriteVFWALUF_WorstCase, []>;
def : WriteRes<WriteVFWALUV_M1_E16, []>;
def : WriteRes<WriteVFWALUV_M1_E32, []>;
def : WriteRes<WriteVFWALUV_M2_E16, []>;
def : WriteRes<WriteVFWALUV_M2_E32, []>;
def : WriteRes<WriteVFWALUV_M4_E16, []>;
def : WriteRes<WriteVFWALUV_M4_E32, []>;
def : WriteRes<WriteVFWALUV_MF2_E16, []>;
def : WriteRes<WriteVFWALUV_MF2_E32, []>;
def : WriteRes<WriteVFWALUV_MF4_E16, []>;
def : WriteRes<WriteVFWALUV_WorstCase, []>;
def : WriteRes<WriteVFWCvtFToIV_M1, []>;
def : WriteRes<WriteVFWCvtFToIV_M2, []>;
def : WriteRes<WriteVFWCvtFToIV_M4, []>;
def : WriteRes<WriteVFWCvtFToIV_MF2, []>;
def : WriteRes<WriteVFWCvtFToIV_MF4, []>;
def : WriteRes<WriteVFWCvtFToIV_WorstCase, []>;
def : WriteRes<WriteVFWMulAddF_M1_E16, []>;
def : WriteRes<WriteVFWMulAddF_M1_E32, []>;
def : WriteRes<WriteVFWMulAddF_M2_E16, []>;
def : WriteRes<WriteVFWMulAddF_M2_E32, []>;
def : WriteRes<WriteVFWMulAddF_M4_E16, []>;
def : WriteRes<WriteVFWMulAddF_M4_E32, []>;
def : WriteRes<WriteVFWMulAddF_MF2_E16, []>;
def : WriteRes<WriteVFWMulAddF_MF2_E32, []>;
def : WriteRes<WriteVFWMulAddF_MF4_E16, []>;
def : WriteRes<WriteVFWMulAddF_WorstCase, []>;
def : WriteRes<WriteVFWMulAddV_M1_E16, []>;
def : WriteRes<WriteVFWMulAddV_M1_E32, []>;
def : WriteRes<WriteVFWMulAddV_M2_E16, []>;
def : WriteRes<WriteVFWMulAddV_M2_E32, []>;
def : WriteRes<WriteVFWMulAddV_M4_E16, []>;
def : WriteRes<WriteVFWMulAddV_M4_E32, []>;
def : WriteRes<WriteVFWMulAddV_MF2_E16, []>;
def : WriteRes<WriteVFWMulAddV_MF2_E32, []>;
def : WriteRes<WriteVFWMulAddV_MF4_E16, []>;
def : WriteRes<WriteVFWMulAddV_WorstCase, []>;
def : WriteRes<WriteVFWMulF_M1_E16, []>;
def : WriteRes<WriteVFWMulF_M1_E32, []>;
def : WriteRes<WriteVFWMulF_M2_E16, []>;
def : WriteRes<WriteVFWMulF_M2_E32, []>;
def : WriteRes<WriteVFWMulF_M4_E16, []>;
def : WriteRes<WriteVFWMulF_M4_E32, []>;
def : WriteRes<WriteVFWMulF_MF2_E16, []>;
def : WriteRes<WriteVFWMulF_MF2_E32, []>;
def : WriteRes<WriteVFWMulF_MF4_E16, []>;
def : WriteRes<WriteVFWMulF_WorstCase, []>;
def : WriteRes<WriteVFWMulV_M1_E16, []>;
def : WriteRes<WriteVFWMulV_M1_E32, []>;
def : WriteRes<WriteVFWMulV_M2_E16, []>;
def : WriteRes<WriteVFWMulV_M2_E32, []>;
def : WriteRes<WriteVFWMulV_M4_E16, []>;
def : WriteRes<WriteVFWMulV_M4_E32, []>;
def : WriteRes<WriteVFWMulV_MF2_E16, []>;
def : WriteRes<WriteVFWMulV_MF2_E32, []>;
def : WriteRes<WriteVFWMulV_MF4_E16, []>;
def : WriteRes<WriteVFWMulV_WorstCase, []>;
def : WriteRes<WriteVFWRedOV_From_M1_E16, []>;
def : WriteRes<WriteVFWRedOV_From_M1_E32, []>;
def : WriteRes<WriteVFWRedOV_From_M2_E16, []>;
def : WriteRes<WriteVFWRedOV_From_M2_E32, []>;
def : WriteRes<WriteVFWRedOV_From_M4_E16, []>;
def : WriteRes<WriteVFWRedOV_From_M4_E32, []>;
def : WriteRes<WriteVFWRedOV_From_M8_E16, []>;
def : WriteRes<WriteVFWRedOV_From_M8_E32, []>;
def : WriteRes<WriteVFWRedOV_From_MF2_E16, []>;
def : WriteRes<WriteVFWRedOV_From_MF2_E32, []>;
def : WriteRes<WriteVFWRedOV_From_MF4_E16, []>;
def : WriteRes<WriteVFWRedOV_From_WorstCase, []>;
def : WriteRes<WriteVFWRedV_From_M1_E16, []>;
def : WriteRes<WriteVFWRedV_From_M1_E32, []>;
def : WriteRes<WriteVFWRedV_From_M2_E16, []>;
def : WriteRes<WriteVFWRedV_From_M2_E32, []>;
def : WriteRes<WriteVFWRedV_From_M4_E16, []>;
def : WriteRes<WriteVFWRedV_From_M4_E32, []>;
def : WriteRes<WriteVFWRedV_From_M8_E16, []>;
def : WriteRes<WriteVFWRedV_From_M8_E32, []>;
def : WriteRes<WriteVFWRedV_From_MF2_E16, []>;
def : WriteRes<WriteVFWRedV_From_MF2_E32, []>;
def : WriteRes<WriteVFWRedV_From_MF4_E16, []>;
def : WriteRes<WriteVFWRedV_From_WorstCase, []>;
def : WriteRes<WriteVGHSHV_M1, []>;
def : WriteRes<WriteVGHSHV_M2, []>;
def : WriteRes<WriteVGHSHV_M4, []>;
def : WriteRes<WriteVGHSHV_M8, []>;
def : WriteRes<WriteVGHSHV_MF2, []>;
def : WriteRes<WriteVGHSHV_WorstCase, []>;
def : WriteRes<WriteVGMULV_M1, []>;
def : WriteRes<WriteVGMULV_M2, []>;
def : WriteRes<WriteVGMULV_M4, []>;
def : WriteRes<WriteVGMULV_M8, []>;
def : WriteRes<WriteVGMULV_MF2, []>;
def : WriteRes<WriteVGMULV_WorstCase, []>;
def : WriteRes<WriteVIMulAddV_M1, []>;
def : WriteRes<WriteVIMulAddV_M2, []>;
def : WriteRes<WriteVIMulAddV_M4, []>;
def : WriteRes<WriteVIMulAddV_M8, []>;
def : WriteRes<WriteVIMulAddV_MF2, []>;
def : WriteRes<WriteVIMulAddV_MF4, []>;
def : WriteRes<WriteVIMulAddV_MF8, []>;
def : WriteRes<WriteVIMulAddV_WorstCase, []>;
def : WriteRes<WriteVIMulAddX_M1, []>;
def : WriteRes<WriteVIMulAddX_M2, []>;
def : WriteRes<WriteVIMulAddX_M4, []>;
def : WriteRes<WriteVIMulAddX_M8, []>;
def : WriteRes<WriteVIMulAddX_MF2, []>;
def : WriteRes<WriteVIMulAddX_MF4, []>;
def : WriteRes<WriteVIMulAddX_MF8, []>;
def : WriteRes<WriteVIMulAddX_WorstCase, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M1_E16, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M1_E32, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M1_E64, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M1_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M2_E16, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M2_E32, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M2_E64, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M2_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M4_E16, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M4_E32, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M4_E64, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M4_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M8_E16, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M8_E32, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M8_E64, []>;
def : WriteRes<WriteVIRedMinMaxV_From_M8_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_MF2_E16, []>;
def : WriteRes<WriteVIRedMinMaxV_From_MF2_E32, []>;
def : WriteRes<WriteVIRedMinMaxV_From_MF2_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_MF4_E16, []>;
def : WriteRes<WriteVIRedMinMaxV_From_MF4_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_MF8_E8, []>;
def : WriteRes<WriteVIRedMinMaxV_From_WorstCase, []>;
def : WriteRes<WriteVIRedV_From_M1_E16, []>;
def : WriteRes<WriteVIRedV_From_M1_E32, []>;
def : WriteRes<WriteVIRedV_From_M1_E64, []>;
def : WriteRes<WriteVIRedV_From_M1_E8, []>;
def : WriteRes<WriteVIRedV_From_M2_E16, []>;
def : WriteRes<WriteVIRedV_From_M2_E32, []>;
def : WriteRes<WriteVIRedV_From_M2_E64, []>;
def : WriteRes<WriteVIRedV_From_M2_E8, []>;
def : WriteRes<WriteVIRedV_From_M4_E16, []>;
def : WriteRes<WriteVIRedV_From_M4_E32, []>;
def : WriteRes<WriteVIRedV_From_M4_E64, []>;
def : WriteRes<WriteVIRedV_From_M4_E8, []>;
def : WriteRes<WriteVIRedV_From_M8_E16, []>;
def : WriteRes<WriteVIRedV_From_M8_E32, []>;
def : WriteRes<WriteVIRedV_From_M8_E64, []>;
def : WriteRes<WriteVIRedV_From_M8_E8, []>;
def : WriteRes<WriteVIRedV_From_MF2_E16, []>;
def : WriteRes<WriteVIRedV_From_MF2_E32, []>;
def : WriteRes<WriteVIRedV_From_MF2_E8, []>;
def : WriteRes<WriteVIRedV_From_MF4_E16, []>;
def : WriteRes<WriteVIRedV_From_MF4_E8, []>;
def : WriteRes<WriteVIRedV_From_MF8_E8, []>;
def : WriteRes<WriteVIRedV_From_WorstCase, []>;
def : WriteRes<WriteVIWALUV_M1, []>;
def : WriteRes<WriteVIWALUV_M2, []>;
def : WriteRes<WriteVIWALUV_M4, []>;
def : WriteRes<WriteVIWALUV_MF2, []>;
def : WriteRes<WriteVIWALUV_MF4, []>;
def : WriteRes<WriteVIWALUV_MF8, []>;
def : WriteRes<WriteVIWALUV_WorstCase, []>;
def : WriteRes<WriteVIWALUX_M1, []>;
def : WriteRes<WriteVIWALUX_M2, []>;
def : WriteRes<WriteVIWALUX_M4, []>;
def : WriteRes<WriteVIWALUX_MF2, []>;
def : WriteRes<WriteVIWALUX_MF4, []>;
def : WriteRes<WriteVIWALUX_MF8, []>;
def : WriteRes<WriteVIWALUX_WorstCase, []>;
def : WriteRes<WriteVIWMulAddV_M1, []>;
def : WriteRes<WriteVIWMulAddV_M2, []>;
def : WriteRes<WriteVIWMulAddV_M4, []>;
def : WriteRes<WriteVIWMulAddV_MF2, []>;
def : WriteRes<WriteVIWMulAddV_MF4, []>;
def : WriteRes<WriteVIWMulAddV_MF8, []>;
def : WriteRes<WriteVIWMulAddV_WorstCase, []>;
def : WriteRes<WriteVIWMulAddX_M1, []>;
def : WriteRes<WriteVIWMulAddX_M2, []>;
def : WriteRes<WriteVIWMulAddX_M4, []>;
def : WriteRes<WriteVIWMulAddX_MF2, []>;
def : WriteRes<WriteVIWMulAddX_MF4, []>;
def : WriteRes<WriteVIWMulAddX_MF8, []>;
def : WriteRes<WriteVIWMulAddX_WorstCase, []>;
def : WriteRes<WriteVIWMulV_M1, []>;
def : WriteRes<WriteVIWMulV_M2, []>;
def : WriteRes<WriteVIWMulV_M4, []>;
def : WriteRes<WriteVIWMulV_MF2, []>;
def : WriteRes<WriteVIWMulV_MF4, []>;
def : WriteRes<WriteVIWMulV_MF8, []>;
def : WriteRes<WriteVIWMulV_WorstCase, []>;
def : WriteRes<WriteVIWMulX_M1, []>;
def : WriteRes<WriteVIWMulX_M2, []>;
def : WriteRes<WriteVIWMulX_M4, []>;
def : WriteRes<WriteVIWMulX_MF2, []>;
def : WriteRes<WriteVIWMulX_MF4, []>;
def : WriteRes<WriteVIWMulX_MF8, []>;
def : WriteRes<WriteVIWMulX_WorstCase, []>;
def : WriteRes<WriteVIWRedV_From_M1_E16, []>;
def : WriteRes<WriteVIWRedV_From_M1_E32, []>;
def : WriteRes<WriteVIWRedV_From_M1_E8, []>;
def : WriteRes<WriteVIWRedV_From_M2_E16, []>;
def : WriteRes<WriteVIWRedV_From_M2_E32, []>;
def : WriteRes<WriteVIWRedV_From_M2_E8, []>;
def : WriteRes<WriteVIWRedV_From_M4_E16, []>;
def : WriteRes<WriteVIWRedV_From_M4_E32, []>;
def : WriteRes<WriteVIWRedV_From_M4_E8, []>;
def : WriteRes<WriteVIWRedV_From_M8_E16, []>;
def : WriteRes<WriteVIWRedV_From_M8_E32, []>;
def : WriteRes<WriteVIWRedV_From_M8_E8, []>;
def : WriteRes<WriteVIWRedV_From_MF2_E16, []>;
def : WriteRes<WriteVIWRedV_From_MF2_E32, []>;
def : WriteRes<WriteVIWRedV_From_MF2_E8, []>;
def : WriteRes<WriteVIWRedV_From_MF4_E16, []>;
def : WriteRes<WriteVIWRedV_From_MF4_E8, []>;
def : WriteRes<WriteVIWRedV_From_MF8_E8, []>;
def : WriteRes<WriteVIWRedV_From_WorstCase, []>;
def : WriteRes<WriteVLDFF_M1, []>;
def : WriteRes<WriteVLDFF_M2, []>;
def : WriteRes<WriteVLDFF_M4, []>;
def : WriteRes<WriteVLDFF_M8, []>;
def : WriteRes<WriteVLDFF_MF2, []>;
def : WriteRes<WriteVLDFF_MF4, []>;
def : WriteRes<WriteVLDFF_MF8, []>;
def : WriteRes<WriteVLDFF_WorstCase, []>;
def : WriteRes<WriteVLDM_M1, []>;
def : WriteRes<WriteVLDM_M2, []>;
def : WriteRes<WriteVLDM_M4, []>;
def : WriteRes<WriteVLDM_M8, []>;
def : WriteRes<WriteVLDM_MF2, []>;
def : WriteRes<WriteVLDM_MF4, []>;
def : WriteRes<WriteVLDM_MF8, []>;
def : WriteRes<WriteVLDM_WorstCase, []>;
def : WriteRes<WriteVLDOX16_M1, []>;
def : WriteRes<WriteVLDOX16_M2, []>;
def : WriteRes<WriteVLDOX16_M4, []>;
def : WriteRes<WriteVLDOX16_M8, []>;
def : WriteRes<WriteVLDOX16_MF2, []>;
def : WriteRes<WriteVLDOX16_MF4, []>;
def : WriteRes<WriteVLDOX16_WorstCase, []>;
def : WriteRes<WriteVLDOX32_M1, []>;
def : WriteRes<WriteVLDOX32_M2, []>;
def : WriteRes<WriteVLDOX32_M4, []>;
def : WriteRes<WriteVLDOX32_M8, []>;
def : WriteRes<WriteVLDOX32_MF2, []>;
def : WriteRes<WriteVLDOX32_WorstCase, []>;
def : WriteRes<WriteVLDOX64_M1, []>;
def : WriteRes<WriteVLDOX64_M2, []>;
def : WriteRes<WriteVLDOX64_M4, []>;
def : WriteRes<WriteVLDOX64_M8, []>;
def : WriteRes<WriteVLDOX64_WorstCase, []>;
def : WriteRes<WriteVLDOX8_M1, []>;
def : WriteRes<WriteVLDOX8_M2, []>;
def : WriteRes<WriteVLDOX8_M4, []>;
def : WriteRes<WriteVLDOX8_M8, []>;
def : WriteRes<WriteVLDOX8_MF2, []>;
def : WriteRes<WriteVLDOX8_MF4, []>;
def : WriteRes<WriteVLDOX8_MF8, []>;
def : WriteRes<WriteVLDOX8_WorstCase, []>;
def : WriteRes<WriteVLDS64_M1, []>;
def : WriteRes<WriteVLDS64_M2, []>;
def : WriteRes<WriteVLDS64_M4, []>;
def : WriteRes<WriteVLDS64_M8, []>;
def : WriteRes<WriteVLDS64_WorstCase, []>;
def : WriteRes<WriteVLDS8_M1, []>;
def : WriteRes<WriteVLDS8_M2, []>;
def : WriteRes<WriteVLDS8_M4, []>;
def : WriteRes<WriteVLDS8_M8, []>;
def : WriteRes<WriteVLDS8_MF2, []>;
def : WriteRes<WriteVLDS8_MF4, []>;
def : WriteRes<WriteVLDS8_MF8, []>;
def : WriteRes<WriteVLDS8_WorstCase, []>;
def : WriteRes<WriteVLDUX64_M1, []>;
def : WriteRes<WriteVLDUX64_M2, []>;
def : WriteRes<WriteVLDUX64_M4, []>;
def : WriteRes<WriteVLDUX64_M8, []>;
def : WriteRes<WriteVLDUX64_WorstCase, []>;
def : WriteRes<WriteVLDUX8_M1, []>;
def : WriteRes<WriteVLDUX8_M2, []>;
def : WriteRes<WriteVLDUX8_M4, []>;
def : WriteRes<WriteVLDUX8_M8, []>;
def : WriteRes<WriteVLDUX8_MF2, []>;
def : WriteRes<WriteVLDUX8_MF4, []>;
def : WriteRes<WriteVLDUX8_MF8, []>;
def : WriteRes<WriteVLDUX8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG2e16_M1, []>;
def : WriteRes<WriteVLOXSEG2e16_M2, []>;
def : WriteRes<WriteVLOXSEG2e16_M4, []>;
def : WriteRes<WriteVLOXSEG2e16_MF2, []>;
def : WriteRes<WriteVLOXSEG2e16_MF4, []>;
def : WriteRes<WriteVLOXSEG2e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG2e32_M1, []>;
def : WriteRes<WriteVLOXSEG2e32_M2, []>;
def : WriteRes<WriteVLOXSEG2e32_M4, []>;
def : WriteRes<WriteVLOXSEG2e32_MF2, []>;
def : WriteRes<WriteVLOXSEG2e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG2e64_M1, []>;
def : WriteRes<WriteVLOXSEG2e64_M2, []>;
def : WriteRes<WriteVLOXSEG2e64_M4, []>;
def : WriteRes<WriteVLOXSEG2e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG2e8_M1, []>;
def : WriteRes<WriteVLOXSEG2e8_M2, []>;
def : WriteRes<WriteVLOXSEG2e8_M4, []>;
def : WriteRes<WriteVLOXSEG2e8_MF2, []>;
def : WriteRes<WriteVLOXSEG2e8_MF4, []>;
def : WriteRes<WriteVLOXSEG2e8_MF8, []>;
def : WriteRes<WriteVLOXSEG2e8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG3e16_M1, []>;
def : WriteRes<WriteVLOXSEG3e16_M2, []>;
def : WriteRes<WriteVLOXSEG3e16_MF2, []>;
def : WriteRes<WriteVLOXSEG3e16_MF4, []>;
def : WriteRes<WriteVLOXSEG3e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG3e32_M1, []>;
def : WriteRes<WriteVLOXSEG3e32_M2, []>;
def : WriteRes<WriteVLOXSEG3e32_MF2, []>;
def : WriteRes<WriteVLOXSEG3e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG3e64_M1, []>;
def : WriteRes<WriteVLOXSEG3e64_M2, []>;
def : WriteRes<WriteVLOXSEG3e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG3e8_M1, []>;
def : WriteRes<WriteVLOXSEG3e8_M2, []>;
def : WriteRes<WriteVLOXSEG3e8_MF2, []>;
def : WriteRes<WriteVLOXSEG3e8_MF4, []>;
def : WriteRes<WriteVLOXSEG3e8_MF8, []>;
def : WriteRes<WriteVLOXSEG3e8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG4e16_M1, []>;
def : WriteRes<WriteVLOXSEG4e16_M2, []>;
def : WriteRes<WriteVLOXSEG4e16_MF2, []>;
def : WriteRes<WriteVLOXSEG4e16_MF4, []>;
def : WriteRes<WriteVLOXSEG4e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG4e32_M1, []>;
def : WriteRes<WriteVLOXSEG4e32_M2, []>;
def : WriteRes<WriteVLOXSEG4e32_MF2, []>;
def : WriteRes<WriteVLOXSEG4e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG4e64_M1, []>;
def : WriteRes<WriteVLOXSEG4e64_M2, []>;
def : WriteRes<WriteVLOXSEG4e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG4e8_M1, []>;
def : WriteRes<WriteVLOXSEG4e8_M2, []>;
def : WriteRes<WriteVLOXSEG4e8_MF2, []>;
def : WriteRes<WriteVLOXSEG4e8_MF4, []>;
def : WriteRes<WriteVLOXSEG4e8_MF8, []>;
def : WriteRes<WriteVLOXSEG4e8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG5e16_M1, []>;
def : WriteRes<WriteVLOXSEG5e16_MF2, []>;
def : WriteRes<WriteVLOXSEG5e16_MF4, []>;
def : WriteRes<WriteVLOXSEG5e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG5e32_M1, []>;
def : WriteRes<WriteVLOXSEG5e32_MF2, []>;
def : WriteRes<WriteVLOXSEG5e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG5e64_M1, []>;
def : WriteRes<WriteVLOXSEG5e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG5e8_M1, []>;
def : WriteRes<WriteVLOXSEG5e8_MF2, []>;
def : WriteRes<WriteVLOXSEG5e8_MF4, []>;
def : WriteRes<WriteVLOXSEG5e8_MF8, []>;
def : WriteRes<WriteVLOXSEG5e8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG6e16_M1, []>;
def : WriteRes<WriteVLOXSEG6e16_MF2, []>;
def : WriteRes<WriteVLOXSEG6e16_MF4, []>;
def : WriteRes<WriteVLOXSEG6e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG6e32_M1, []>;
def : WriteRes<WriteVLOXSEG6e32_MF2, []>;
def : WriteRes<WriteVLOXSEG6e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG6e64_M1, []>;
def : WriteRes<WriteVLOXSEG6e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG6e8_M1, []>;
def : WriteRes<WriteVLOXSEG6e8_MF2, []>;
def : WriteRes<WriteVLOXSEG6e8_MF4, []>;
def : WriteRes<WriteVLOXSEG6e8_MF8, []>;
def : WriteRes<WriteVLOXSEG6e8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG7e16_M1, []>;
def : WriteRes<WriteVLOXSEG7e16_MF2, []>;
def : WriteRes<WriteVLOXSEG7e16_MF4, []>;
def : WriteRes<WriteVLOXSEG7e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG7e32_M1, []>;
def : WriteRes<WriteVLOXSEG7e32_MF2, []>;
def : WriteRes<WriteVLOXSEG7e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG7e64_M1, []>;
def : WriteRes<WriteVLOXSEG7e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG7e8_M1, []>;
def : WriteRes<WriteVLOXSEG7e8_MF2, []>;
def : WriteRes<WriteVLOXSEG7e8_MF4, []>;
def : WriteRes<WriteVLOXSEG7e8_MF8, []>;
def : WriteRes<WriteVLOXSEG7e8_WorstCase, []>;
def : WriteRes<WriteVLOXSEG8e16_M1, []>;
def : WriteRes<WriteVLOXSEG8e16_MF2, []>;
def : WriteRes<WriteVLOXSEG8e16_MF4, []>;
def : WriteRes<WriteVLOXSEG8e16_WorstCase, []>;
def : WriteRes<WriteVLOXSEG8e32_M1, []>;
def : WriteRes<WriteVLOXSEG8e32_MF2, []>;
def : WriteRes<WriteVLOXSEG8e32_WorstCase, []>;
def : WriteRes<WriteVLOXSEG8e64_M1, []>;
def : WriteRes<WriteVLOXSEG8e64_WorstCase, []>;
def : WriteRes<WriteVLOXSEG8e8_M1, []>;
def : WriteRes<WriteVLOXSEG8e8_MF2, []>;
def : WriteRes<WriteVLOXSEG8e8_MF4, []>;
def : WriteRes<WriteVLOXSEG8e8_MF8, []>;
def : WriteRes<WriteVLOXSEG8e8_WorstCase, []>;
def : WriteRes<WriteVLSEG2e16_M1, []>;
def : WriteRes<WriteVLSEG2e16_M2, []>;
def : WriteRes<WriteVLSEG2e16_M4, []>;
def : WriteRes<WriteVLSEG2e16_MF2, []>;
def : WriteRes<WriteVLSEG2e16_MF4, []>;
def : WriteRes<WriteVLSEG2e16_WorstCase, []>;
def : WriteRes<WriteVLSEG2e32_M1, []>;
def : WriteRes<WriteVLSEG2e32_M2, []>;
def : WriteRes<WriteVLSEG2e32_M4, []>;
def : WriteRes<WriteVLSEG2e32_MF2, []>;
def : WriteRes<WriteVLSEG2e32_WorstCase, []>;
def : WriteRes<WriteVLSEG2e64_M1, []>;
def : WriteRes<WriteVLSEG2e64_M2, []>;
def : WriteRes<WriteVLSEG2e64_M4, []>;
def : WriteRes<WriteVLSEG2e64_WorstCase, []>;
def : WriteRes<WriteVLSEG2e8_M1, []>;
def : WriteRes<WriteVLSEG2e8_M2, []>;
def : WriteRes<WriteVLSEG2e8_M4, []>;
def : WriteRes<WriteVLSEG2e8_MF2, []>;
def : WriteRes<WriteVLSEG2e8_MF4, []>;
def : WriteRes<WriteVLSEG2e8_MF8, []>;
def : WriteRes<WriteVLSEG2e8_WorstCase, []>;
def : WriteRes<WriteVLSEG3e16_M1, []>;
def : WriteRes<WriteVLSEG3e16_M2, []>;
def : WriteRes<WriteVLSEG3e16_MF2, []>;
def : WriteRes<WriteVLSEG3e16_MF4, []>;
def : WriteRes<WriteVLSEG3e16_WorstCase, []>;
def : WriteRes<WriteVLSEG3e32_M1, []>;
def : WriteRes<WriteVLSEG3e32_M2, []>;
def : WriteRes<WriteVLSEG3e32_MF2, []>;
def : WriteRes<WriteVLSEG3e32_WorstCase, []>;
def : WriteRes<WriteVLSEG3e64_M1, []>;
def : WriteRes<WriteVLSEG3e64_M2, []>;
def : WriteRes<WriteVLSEG3e64_WorstCase, []>;
def : WriteRes<WriteVLSEG3e8_M1, []>;
def : WriteRes<WriteVLSEG3e8_M2, []>;
def : WriteRes<WriteVLSEG3e8_MF2, []>;
def : WriteRes<WriteVLSEG3e8_MF4, []>;
def : WriteRes<WriteVLSEG3e8_MF8, []>;
def : WriteRes<WriteVLSEG3e8_WorstCase, []>;
def : WriteRes<WriteVLSEG4e16_M1, []>;
def : WriteRes<WriteVLSEG4e16_M2, []>;
def : WriteRes<WriteVLSEG4e16_MF2, []>;
def : WriteRes<WriteVLSEG4e16_MF4, []>;
def : WriteRes<WriteVLSEG4e16_WorstCase, []>;
def : WriteRes<WriteVLSEG4e32_M1, []>;
def : WriteRes<WriteVLSEG4e32_M2, []>;
def : WriteRes<WriteVLSEG4e32_MF2, []>;
def : WriteRes<WriteVLSEG4e32_WorstCase, []>;
def : WriteRes<WriteVLSEG4e64_M1, []>;
def : WriteRes<WriteVLSEG4e64_M2, []>;
def : WriteRes<WriteVLSEG4e64_WorstCase, []>;
def : WriteRes<WriteVLSEG4e8_M1, []>;
def : WriteRes<WriteVLSEG4e8_M2, []>;
def : WriteRes<WriteVLSEG4e8_MF2, []>;
def : WriteRes<WriteVLSEG4e8_MF4, []>;
def : WriteRes<WriteVLSEG4e8_MF8, []>;
def : WriteRes<WriteVLSEG4e8_WorstCase, []>;
def : WriteRes<WriteVLSEG5e16_M1, []>;
def : WriteRes<WriteVLSEG5e16_MF2, []>;
def : WriteRes<WriteVLSEG5e16_MF4, []>;
def : WriteRes<WriteVLSEG5e16_WorstCase, []>;
def : WriteRes<WriteVLSEG5e32_M1, []>;
def : WriteRes<WriteVLSEG5e32_MF2, []>;
def : WriteRes<WriteVLSEG5e32_WorstCase, []>;
def : WriteRes<WriteVLSEG5e64_M1, []>;
def : WriteRes<WriteVLSEG5e64_WorstCase, []>;
def : WriteRes<WriteVLSEG5e8_M1, []>;
def : WriteRes<WriteVLSEG5e8_MF2, []>;
def : WriteRes<WriteVLSEG5e8_MF4, []>;
def : WriteRes<WriteVLSEG5e8_MF8, []>;
def : WriteRes<WriteVLSEG5e8_WorstCase, []>;
def : WriteRes<WriteVLSEG6e16_M1, []>;
def : WriteRes<WriteVLSEG6e16_MF2, []>;
def : WriteRes<WriteVLSEG6e16_MF4, []>;
def : WriteRes<WriteVLSEG6e16_WorstCase, []>;
def : WriteRes<WriteVLSEG6e32_M1, []>;
def : WriteRes<WriteVLSEG6e32_MF2, []>;
def : WriteRes<WriteVLSEG6e32_WorstCase, []>;
def : WriteRes<WriteVLSEG6e64_M1, []>;
def : WriteRes<WriteVLSEG6e64_WorstCase, []>;
def : WriteRes<WriteVLSEG6e8_M1, []>;
def : WriteRes<WriteVLSEG6e8_MF2, []>;
def : WriteRes<WriteVLSEG6e8_MF4, []>;
def : WriteRes<WriteVLSEG6e8_MF8, []>;
def : WriteRes<WriteVLSEG6e8_WorstCase, []>;
def : WriteRes<WriteVLSEG7e16_M1, []>;
def : WriteRes<WriteVLSEG7e16_MF2, []>;
def : WriteRes<WriteVLSEG7e16_MF4, []>;
def : WriteRes<WriteVLSEG7e16_WorstCase, []>;
def : WriteRes<WriteVLSEG7e32_M1, []>;
def : WriteRes<WriteVLSEG7e32_MF2, []>;
def : WriteRes<WriteVLSEG7e32_WorstCase, []>;
def : WriteRes<WriteVLSEG7e64_M1, []>;
def : WriteRes<WriteVLSEG7e64_WorstCase, []>;
def : WriteRes<WriteVLSEG7e8_M1, []>;
def : WriteRes<WriteVLSEG7e8_MF2, []>;
def : WriteRes<WriteVLSEG7e8_MF4, []>;
def : WriteRes<WriteVLSEG7e8_MF8, []>;
def : WriteRes<WriteVLSEG7e8_WorstCase, []>;
def : WriteRes<WriteVLSEG8e16_M1, []>;
def : WriteRes<WriteVLSEG8e16_MF2, []>;
def : WriteRes<WriteVLSEG8e16_MF4, []>;
def : WriteRes<WriteVLSEG8e16_WorstCase, []>;
def : WriteRes<WriteVLSEG8e32_M1, []>;
def : WriteRes<WriteVLSEG8e32_MF2, []>;
def : WriteRes<WriteVLSEG8e32_WorstCase, []>;
def : WriteRes<WriteVLSEG8e64_M1, []>;
def : WriteRes<WriteVLSEG8e64_WorstCase, []>;
def : WriteRes<WriteVLSEG8e8_M1, []>;
def : WriteRes<WriteVLSEG8e8_MF2, []>;
def : WriteRes<WriteVLSEG8e8_MF4, []>;
def : WriteRes<WriteVLSEG8e8_MF8, []>;
def : WriteRes<WriteVLSEG8e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF2e16_M1, []>;
def : WriteRes<WriteVLSEGFF2e16_M2, []>;
def : WriteRes<WriteVLSEGFF2e16_M4, []>;
def : WriteRes<WriteVLSEGFF2e16_MF2, []>;
def : WriteRes<WriteVLSEGFF2e16_MF4, []>;
def : WriteRes<WriteVLSEGFF2e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF2e32_M1, []>;
def : WriteRes<WriteVLSEGFF2e32_M2, []>;
def : WriteRes<WriteVLSEGFF2e32_M4, []>;
def : WriteRes<WriteVLSEGFF2e32_MF2, []>;
def : WriteRes<WriteVLSEGFF2e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF2e64_M1, []>;
def : WriteRes<WriteVLSEGFF2e64_M2, []>;
def : WriteRes<WriteVLSEGFF2e64_M4, []>;
def : WriteRes<WriteVLSEGFF2e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF2e8_M1, []>;
def : WriteRes<WriteVLSEGFF2e8_M2, []>;
def : WriteRes<WriteVLSEGFF2e8_M4, []>;
def : WriteRes<WriteVLSEGFF2e8_MF2, []>;
def : WriteRes<WriteVLSEGFF2e8_MF4, []>;
def : WriteRes<WriteVLSEGFF2e8_MF8, []>;
def : WriteRes<WriteVLSEGFF2e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF3e16_M1, []>;
def : WriteRes<WriteVLSEGFF3e16_M2, []>;
def : WriteRes<WriteVLSEGFF3e16_MF2, []>;
def : WriteRes<WriteVLSEGFF3e16_MF4, []>;
def : WriteRes<WriteVLSEGFF3e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF3e32_M1, []>;
def : WriteRes<WriteVLSEGFF3e32_M2, []>;
def : WriteRes<WriteVLSEGFF3e32_MF2, []>;
def : WriteRes<WriteVLSEGFF3e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF3e64_M1, []>;
def : WriteRes<WriteVLSEGFF3e64_M2, []>;
def : WriteRes<WriteVLSEGFF3e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF3e8_M1, []>;
def : WriteRes<WriteVLSEGFF3e8_M2, []>;
def : WriteRes<WriteVLSEGFF3e8_MF2, []>;
def : WriteRes<WriteVLSEGFF3e8_MF4, []>;
def : WriteRes<WriteVLSEGFF3e8_MF8, []>;
def : WriteRes<WriteVLSEGFF3e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF4e16_M1, []>;
def : WriteRes<WriteVLSEGFF4e16_M2, []>;
def : WriteRes<WriteVLSEGFF4e16_MF2, []>;
def : WriteRes<WriteVLSEGFF4e16_MF4, []>;
def : WriteRes<WriteVLSEGFF4e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF4e32_M1, []>;
def : WriteRes<WriteVLSEGFF4e32_M2, []>;
def : WriteRes<WriteVLSEGFF4e32_MF2, []>;
def : WriteRes<WriteVLSEGFF4e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF4e64_M1, []>;
def : WriteRes<WriteVLSEGFF4e64_M2, []>;
def : WriteRes<WriteVLSEGFF4e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF4e8_M1, []>;
def : WriteRes<WriteVLSEGFF4e8_M2, []>;
def : WriteRes<WriteVLSEGFF4e8_MF2, []>;
def : WriteRes<WriteVLSEGFF4e8_MF4, []>;
def : WriteRes<WriteVLSEGFF4e8_MF8, []>;
def : WriteRes<WriteVLSEGFF4e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF5e16_M1, []>;
def : WriteRes<WriteVLSEGFF5e16_MF2, []>;
def : WriteRes<WriteVLSEGFF5e16_MF4, []>;
def : WriteRes<WriteVLSEGFF5e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF5e32_M1, []>;
def : WriteRes<WriteVLSEGFF5e32_MF2, []>;
def : WriteRes<WriteVLSEGFF5e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF5e64_M1, []>;
def : WriteRes<WriteVLSEGFF5e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF5e8_M1, []>;
def : WriteRes<WriteVLSEGFF5e8_MF2, []>;
def : WriteRes<WriteVLSEGFF5e8_MF4, []>;
def : WriteRes<WriteVLSEGFF5e8_MF8, []>;
def : WriteRes<WriteVLSEGFF5e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF6e16_M1, []>;
def : WriteRes<WriteVLSEGFF6e16_MF2, []>;
def : WriteRes<WriteVLSEGFF6e16_MF4, []>;
def : WriteRes<WriteVLSEGFF6e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF6e32_M1, []>;
def : WriteRes<WriteVLSEGFF6e32_MF2, []>;
def : WriteRes<WriteVLSEGFF6e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF6e64_M1, []>;
def : WriteRes<WriteVLSEGFF6e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF6e8_M1, []>;
def : WriteRes<WriteVLSEGFF6e8_MF2, []>;
def : WriteRes<WriteVLSEGFF6e8_MF4, []>;
def : WriteRes<WriteVLSEGFF6e8_MF8, []>;
def : WriteRes<WriteVLSEGFF6e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF7e16_M1, []>;
def : WriteRes<WriteVLSEGFF7e16_MF2, []>;
def : WriteRes<WriteVLSEGFF7e16_MF4, []>;
def : WriteRes<WriteVLSEGFF7e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF7e32_M1, []>;
def : WriteRes<WriteVLSEGFF7e32_MF2, []>;
def : WriteRes<WriteVLSEGFF7e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF7e64_M1, []>;
def : WriteRes<WriteVLSEGFF7e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF7e8_M1, []>;
def : WriteRes<WriteVLSEGFF7e8_MF2, []>;
def : WriteRes<WriteVLSEGFF7e8_MF4, []>;
def : WriteRes<WriteVLSEGFF7e8_MF8, []>;
def : WriteRes<WriteVLSEGFF7e8_WorstCase, []>;
def : WriteRes<WriteVLSEGFF8e16_M1, []>;
def : WriteRes<WriteVLSEGFF8e16_MF2, []>;
def : WriteRes<WriteVLSEGFF8e16_MF4, []>;
def : WriteRes<WriteVLSEGFF8e16_WorstCase, []>;
def : WriteRes<WriteVLSEGFF8e32_M1, []>;
def : WriteRes<WriteVLSEGFF8e32_MF2, []>;
def : WriteRes<WriteVLSEGFF8e32_WorstCase, []>;
def : WriteRes<WriteVLSEGFF8e64_M1, []>;
def : WriteRes<WriteVLSEGFF8e64_WorstCase, []>;
def : WriteRes<WriteVLSEGFF8e8_M1, []>;
def : WriteRes<WriteVLSEGFF8e8_MF2, []>;
def : WriteRes<WriteVLSEGFF8e8_MF4, []>;
def : WriteRes<WriteVLSEGFF8e8_MF8, []>;
def : WriteRes<WriteVLSEGFF8e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG2e16_M1, []>;
def : WriteRes<WriteVLSSEG2e16_M2, []>;
def : WriteRes<WriteVLSSEG2e16_M4, []>;
def : WriteRes<WriteVLSSEG2e16_MF2, []>;
def : WriteRes<WriteVLSSEG2e16_MF4, []>;
def : WriteRes<WriteVLSSEG2e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG2e32_M1, []>;
def : WriteRes<WriteVLSSEG2e32_M2, []>;
def : WriteRes<WriteVLSSEG2e32_M4, []>;
def : WriteRes<WriteVLSSEG2e32_MF2, []>;
def : WriteRes<WriteVLSSEG2e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG2e64_M1, []>;
def : WriteRes<WriteVLSSEG2e64_M2, []>;
def : WriteRes<WriteVLSSEG2e64_M4, []>;
def : WriteRes<WriteVLSSEG2e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG2e8_M1, []>;
def : WriteRes<WriteVLSSEG2e8_M2, []>;
def : WriteRes<WriteVLSSEG2e8_M4, []>;
def : WriteRes<WriteVLSSEG2e8_MF2, []>;
def : WriteRes<WriteVLSSEG2e8_MF4, []>;
def : WriteRes<WriteVLSSEG2e8_MF8, []>;
def : WriteRes<WriteVLSSEG2e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG3e16_M1, []>;
def : WriteRes<WriteVLSSEG3e16_M2, []>;
def : WriteRes<WriteVLSSEG3e16_MF2, []>;
def : WriteRes<WriteVLSSEG3e16_MF4, []>;
def : WriteRes<WriteVLSSEG3e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG3e32_M1, []>;
def : WriteRes<WriteVLSSEG3e32_M2, []>;
def : WriteRes<WriteVLSSEG3e32_MF2, []>;
def : WriteRes<WriteVLSSEG3e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG3e64_M1, []>;
def : WriteRes<WriteVLSSEG3e64_M2, []>;
def : WriteRes<WriteVLSSEG3e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG3e8_M1, []>;
def : WriteRes<WriteVLSSEG3e8_M2, []>;
def : WriteRes<WriteVLSSEG3e8_MF2, []>;
def : WriteRes<WriteVLSSEG3e8_MF4, []>;
def : WriteRes<WriteVLSSEG3e8_MF8, []>;
def : WriteRes<WriteVLSSEG3e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG4e16_M1, []>;
def : WriteRes<WriteVLSSEG4e16_M2, []>;
def : WriteRes<WriteVLSSEG4e16_MF2, []>;
def : WriteRes<WriteVLSSEG4e16_MF4, []>;
def : WriteRes<WriteVLSSEG4e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG4e32_M1, []>;
def : WriteRes<WriteVLSSEG4e32_M2, []>;
def : WriteRes<WriteVLSSEG4e32_MF2, []>;
def : WriteRes<WriteVLSSEG4e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG4e64_M1, []>;
def : WriteRes<WriteVLSSEG4e64_M2, []>;
def : WriteRes<WriteVLSSEG4e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG4e8_M1, []>;
def : WriteRes<WriteVLSSEG4e8_M2, []>;
def : WriteRes<WriteVLSSEG4e8_MF2, []>;
def : WriteRes<WriteVLSSEG4e8_MF4, []>;
def : WriteRes<WriteVLSSEG4e8_MF8, []>;
def : WriteRes<WriteVLSSEG4e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG5e16_M1, []>;
def : WriteRes<WriteVLSSEG5e16_MF2, []>;
def : WriteRes<WriteVLSSEG5e16_MF4, []>;
def : WriteRes<WriteVLSSEG5e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG5e32_M1, []>;
def : WriteRes<WriteVLSSEG5e32_MF2, []>;
def : WriteRes<WriteVLSSEG5e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG5e64_M1, []>;
def : WriteRes<WriteVLSSEG5e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG5e8_M1, []>;
def : WriteRes<WriteVLSSEG5e8_MF2, []>;
def : WriteRes<WriteVLSSEG5e8_MF4, []>;
def : WriteRes<WriteVLSSEG5e8_MF8, []>;
def : WriteRes<WriteVLSSEG5e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG6e16_M1, []>;
def : WriteRes<WriteVLSSEG6e16_MF2, []>;
def : WriteRes<WriteVLSSEG6e16_MF4, []>;
def : WriteRes<WriteVLSSEG6e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG6e32_M1, []>;
def : WriteRes<WriteVLSSEG6e32_MF2, []>;
def : WriteRes<WriteVLSSEG6e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG6e64_M1, []>;
def : WriteRes<WriteVLSSEG6e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG6e8_M1, []>;
def : WriteRes<WriteVLSSEG6e8_MF2, []>;
def : WriteRes<WriteVLSSEG6e8_MF4, []>;
def : WriteRes<WriteVLSSEG6e8_MF8, []>;
def : WriteRes<WriteVLSSEG6e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG7e16_M1, []>;
def : WriteRes<WriteVLSSEG7e16_MF2, []>;
def : WriteRes<WriteVLSSEG7e16_MF4, []>;
def : WriteRes<WriteVLSSEG7e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG7e32_M1, []>;
def : WriteRes<WriteVLSSEG7e32_MF2, []>;
def : WriteRes<WriteVLSSEG7e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG7e64_M1, []>;
def : WriteRes<WriteVLSSEG7e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG7e8_M1, []>;
def : WriteRes<WriteVLSSEG7e8_MF2, []>;
def : WriteRes<WriteVLSSEG7e8_MF4, []>;
def : WriteRes<WriteVLSSEG7e8_MF8, []>;
def : WriteRes<WriteVLSSEG7e8_WorstCase, []>;
def : WriteRes<WriteVLSSEG8e16_M1, []>;
def : WriteRes<WriteVLSSEG8e16_MF2, []>;
def : WriteRes<WriteVLSSEG8e16_MF4, []>;
def : WriteRes<WriteVLSSEG8e16_WorstCase, []>;
def : WriteRes<WriteVLSSEG8e32_M1, []>;
def : WriteRes<WriteVLSSEG8e32_MF2, []>;
def : WriteRes<WriteVLSSEG8e32_WorstCase, []>;
def : WriteRes<WriteVLSSEG8e64_M1, []>;
def : WriteRes<WriteVLSSEG8e64_WorstCase, []>;
def : WriteRes<WriteVLSSEG8e8_M1, []>;
def : WriteRes<WriteVLSSEG8e8_MF2, []>;
def : WriteRes<WriteVLSSEG8e8_MF4, []>;
def : WriteRes<WriteVLSSEG8e8_MF8, []>;
def : WriteRes<WriteVLSSEG8e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG2e16_M1, []>;
def : WriteRes<WriteVLUXSEG2e16_M2, []>;
def : WriteRes<WriteVLUXSEG2e16_M4, []>;
def : WriteRes<WriteVLUXSEG2e16_MF2, []>;
def : WriteRes<WriteVLUXSEG2e16_MF4, []>;
def : WriteRes<WriteVLUXSEG2e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG2e32_M1, []>;
def : WriteRes<WriteVLUXSEG2e32_M2, []>;
def : WriteRes<WriteVLUXSEG2e32_M4, []>;
def : WriteRes<WriteVLUXSEG2e32_MF2, []>;
def : WriteRes<WriteVLUXSEG2e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG2e64_M1, []>;
def : WriteRes<WriteVLUXSEG2e64_M2, []>;
def : WriteRes<WriteVLUXSEG2e64_M4, []>;
def : WriteRes<WriteVLUXSEG2e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG2e8_M1, []>;
def : WriteRes<WriteVLUXSEG2e8_M2, []>;
def : WriteRes<WriteVLUXSEG2e8_M4, []>;
def : WriteRes<WriteVLUXSEG2e8_MF2, []>;
def : WriteRes<WriteVLUXSEG2e8_MF4, []>;
def : WriteRes<WriteVLUXSEG2e8_MF8, []>;
def : WriteRes<WriteVLUXSEG2e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG3e16_M1, []>;
def : WriteRes<WriteVLUXSEG3e16_M2, []>;
def : WriteRes<WriteVLUXSEG3e16_MF2, []>;
def : WriteRes<WriteVLUXSEG3e16_MF4, []>;
def : WriteRes<WriteVLUXSEG3e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG3e32_M1, []>;
def : WriteRes<WriteVLUXSEG3e32_M2, []>;
def : WriteRes<WriteVLUXSEG3e32_MF2, []>;
def : WriteRes<WriteVLUXSEG3e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG3e64_M1, []>;
def : WriteRes<WriteVLUXSEG3e64_M2, []>;
def : WriteRes<WriteVLUXSEG3e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG3e8_M1, []>;
def : WriteRes<WriteVLUXSEG3e8_M2, []>;
def : WriteRes<WriteVLUXSEG3e8_MF2, []>;
def : WriteRes<WriteVLUXSEG3e8_MF4, []>;
def : WriteRes<WriteVLUXSEG3e8_MF8, []>;
def : WriteRes<WriteVLUXSEG3e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG4e16_M1, []>;
def : WriteRes<WriteVLUXSEG4e16_M2, []>;
def : WriteRes<WriteVLUXSEG4e16_MF2, []>;
def : WriteRes<WriteVLUXSEG4e16_MF4, []>;
def : WriteRes<WriteVLUXSEG4e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG4e32_M1, []>;
def : WriteRes<WriteVLUXSEG4e32_M2, []>;
def : WriteRes<WriteVLUXSEG4e32_MF2, []>;
def : WriteRes<WriteVLUXSEG4e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG4e64_M1, []>;
def : WriteRes<WriteVLUXSEG4e64_M2, []>;
def : WriteRes<WriteVLUXSEG4e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG4e8_M1, []>;
def : WriteRes<WriteVLUXSEG4e8_M2, []>;
def : WriteRes<WriteVLUXSEG4e8_MF2, []>;
def : WriteRes<WriteVLUXSEG4e8_MF4, []>;
def : WriteRes<WriteVLUXSEG4e8_MF8, []>;
def : WriteRes<WriteVLUXSEG4e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG5e16_M1, []>;
def : WriteRes<WriteVLUXSEG5e16_MF2, []>;
def : WriteRes<WriteVLUXSEG5e16_MF4, []>;
def : WriteRes<WriteVLUXSEG5e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG5e32_M1, []>;
def : WriteRes<WriteVLUXSEG5e32_MF2, []>;
def : WriteRes<WriteVLUXSEG5e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG5e64_M1, []>;
def : WriteRes<WriteVLUXSEG5e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG5e8_M1, []>;
def : WriteRes<WriteVLUXSEG5e8_MF2, []>;
def : WriteRes<WriteVLUXSEG5e8_MF4, []>;
def : WriteRes<WriteVLUXSEG5e8_MF8, []>;
def : WriteRes<WriteVLUXSEG5e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG6e16_M1, []>;
def : WriteRes<WriteVLUXSEG6e16_MF2, []>;
def : WriteRes<WriteVLUXSEG6e16_MF4, []>;
def : WriteRes<WriteVLUXSEG6e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG6e32_M1, []>;
def : WriteRes<WriteVLUXSEG6e32_MF2, []>;
def : WriteRes<WriteVLUXSEG6e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG6e64_M1, []>;
def : WriteRes<WriteVLUXSEG6e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG6e8_M1, []>;
def : WriteRes<WriteVLUXSEG6e8_MF2, []>;
def : WriteRes<WriteVLUXSEG6e8_MF4, []>;
def : WriteRes<WriteVLUXSEG6e8_MF8, []>;
def : WriteRes<WriteVLUXSEG6e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG7e16_M1, []>;
def : WriteRes<WriteVLUXSEG7e16_MF2, []>;
def : WriteRes<WriteVLUXSEG7e16_MF4, []>;
def : WriteRes<WriteVLUXSEG7e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG7e32_M1, []>;
def : WriteRes<WriteVLUXSEG7e32_MF2, []>;
def : WriteRes<WriteVLUXSEG7e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG7e64_M1, []>;
def : WriteRes<WriteVLUXSEG7e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG7e8_M1, []>;
def : WriteRes<WriteVLUXSEG7e8_MF2, []>;
def : WriteRes<WriteVLUXSEG7e8_MF4, []>;
def : WriteRes<WriteVLUXSEG7e8_MF8, []>;
def : WriteRes<WriteVLUXSEG7e8_WorstCase, []>;
def : WriteRes<WriteVLUXSEG8e16_M1, []>;
def : WriteRes<WriteVLUXSEG8e16_MF2, []>;
def : WriteRes<WriteVLUXSEG8e16_MF4, []>;
def : WriteRes<WriteVLUXSEG8e16_WorstCase, []>;
def : WriteRes<WriteVLUXSEG8e32_M1, []>;
def : WriteRes<WriteVLUXSEG8e32_MF2, []>;
def : WriteRes<WriteVLUXSEG8e32_WorstCase, []>;
def : WriteRes<WriteVLUXSEG8e64_M1, []>;
def : WriteRes<WriteVLUXSEG8e64_WorstCase, []>;
def : WriteRes<WriteVLUXSEG8e8_M1, []>;
def : WriteRes<WriteVLUXSEG8e8_MF2, []>;
def : WriteRes<WriteVLUXSEG8e8_MF4, []>;
def : WriteRes<WriteVLUXSEG8e8_MF8, []>;
def : WriteRes<WriteVLUXSEG8e8_WorstCase, []>;
def : WriteRes<WriteVNClipI_M1, []>;
def : WriteRes<WriteVNClipI_M2, []>;
def : WriteRes<WriteVNClipI_M4, []>;
def : WriteRes<WriteVNClipI_MF2, []>;
def : WriteRes<WriteVNClipI_MF4, []>;
def : WriteRes<WriteVNClipI_MF8, []>;
def : WriteRes<WriteVNClipI_WorstCase, []>;
def : WriteRes<WriteVNClipV_M1, []>;
def : WriteRes<WriteVNClipV_M2, []>;
def : WriteRes<WriteVNClipV_M4, []>;
def : WriteRes<WriteVNClipV_MF2, []>;
def : WriteRes<WriteVNClipV_MF4, []>;
def : WriteRes<WriteVNClipV_MF8, []>;
def : WriteRes<WriteVNClipV_WorstCase, []>;
def : WriteRes<WriteVNClipX_M1, []>;
def : WriteRes<WriteVNClipX_M2, []>;
def : WriteRes<WriteVNClipX_M4, []>;
def : WriteRes<WriteVNClipX_MF2, []>;
def : WriteRes<WriteVNClipX_MF4, []>;
def : WriteRes<WriteVNClipX_MF8, []>;
def : WriteRes<WriteVNClipX_WorstCase, []>;
def : WriteRes<WriteVNShiftV_M1, []>;
def : WriteRes<WriteVNShiftV_M2, []>;
def : WriteRes<WriteVNShiftV_M4, []>;
def : WriteRes<WriteVNShiftV_MF2, []>;
def : WriteRes<WriteVNShiftV_MF4, []>;
def : WriteRes<WriteVNShiftV_MF8, []>;
def : WriteRes<WriteVNShiftV_WorstCase, []>;
def : WriteRes<WriteVNShiftX_M1, []>;
def : WriteRes<WriteVNShiftX_M2, []>;
def : WriteRes<WriteVNShiftX_M4, []>;
def : WriteRes<WriteVNShiftX_MF2, []>;
def : WriteRes<WriteVNShiftX_MF4, []>;
def : WriteRes<WriteVNShiftX_MF8, []>;
def : WriteRes<WriteVNShiftX_WorstCase, []>;
def : WriteRes<WriteVREV8V_M1, []>;
def : WriteRes<WriteVREV8V_M2, []>;
def : WriteRes<WriteVREV8V_M4, []>;
def : WriteRes<WriteVREV8V_M8, []>;
def : WriteRes<WriteVREV8V_MF2, []>;
def : WriteRes<WriteVREV8V_MF4, []>;
def : WriteRes<WriteVREV8V_MF8, []>;
def : WriteRes<WriteVRotI_M1, []>;
def : WriteRes<WriteVRotI_M2, []>;
def : WriteRes<WriteVRotI_M4, []>;
def : WriteRes<WriteVRotI_M8, []>;
def : WriteRes<WriteVRotI_MF2, []>;
def : WriteRes<WriteVRotI_MF4, []>;
def : WriteRes<WriteVRotI_MF8, []>;
def : WriteRes<WriteVRotI_WorstCase, []>;
def : WriteRes<WriteVRotV_M1, []>;
def : WriteRes<WriteVRotV_M2, []>;
def : WriteRes<WriteVRotV_M4, []>;
def : WriteRes<WriteVRotV_M8, []>;
def : WriteRes<WriteVRotV_MF2, []>;
def : WriteRes<WriteVRotV_MF4, []>;
def : WriteRes<WriteVRotV_MF8, []>;
def : WriteRes<WriteVRotX_M1, []>;
def : WriteRes<WriteVRotX_M2, []>;
def : WriteRes<WriteVRotX_M4, []>;
def : WriteRes<WriteVRotX_M8, []>;
def : WriteRes<WriteVRotX_MF2, []>;
def : WriteRes<WriteVRotX_MF4, []>;
def : WriteRes<WriteVRotX_MF8, []>;
def : WriteRes<WriteVSALUI_M1, []>;
def : WriteRes<WriteVSALUI_M2, []>;
def : WriteRes<WriteVSALUI_M4, []>;
def : WriteRes<WriteVSALUI_M8, []>;
def : WriteRes<WriteVSALUI_MF2, []>;
def : WriteRes<WriteVSALUI_MF4, []>;
def : WriteRes<WriteVSALUI_MF8, []>;
def : WriteRes<WriteVSALUI_WorstCase, []>;
def : WriteRes<WriteVSALUV_M1, []>;
def : WriteRes<WriteVSALUV_M2, []>;
def : WriteRes<WriteVSALUV_M4, []>;
def : WriteRes<WriteVSALUV_M8, []>;
def : WriteRes<WriteVSALUV_MF2, []>;
def : WriteRes<WriteVSALUV_MF4, []>;
def : WriteRes<WriteVSALUV_MF8, []>;
def : WriteRes<WriteVSALUV_WorstCase, []>;
def : WriteRes<WriteVSALUX_M1, []>;
def : WriteRes<WriteVSALUX_M2, []>;
def : WriteRes<WriteVSALUX_M4, []>;
def : WriteRes<WriteVSALUX_M8, []>;
def : WriteRes<WriteVSALUX_MF2, []>;
def : WriteRes<WriteVSALUX_MF4, []>;
def : WriteRes<WriteVSALUX_MF8, []>;
def : WriteRes<WriteVSALUX_WorstCase, []>;
def : WriteRes<WriteVSHA2CHV_M1, []>;
def : WriteRes<WriteVSHA2CHV_M2, []>;
def : WriteRes<WriteVSHA2CHV_M4, []>;
def : WriteRes<WriteVSHA2CHV_M8, []>;
def : WriteRes<WriteVSHA2CHV_MF2, []>;
def : WriteRes<WriteVSHA2CHV_WorstCase, []>;
def : WriteRes<WriteVSHA2CLV_M1, []>;
def : WriteRes<WriteVSHA2CLV_M2, []>;
def : WriteRes<WriteVSHA2CLV_M4, []>;
def : WriteRes<WriteVSHA2CLV_M8, []>;
def : WriteRes<WriteVSHA2CLV_MF2, []>;
def : WriteRes<WriteVSHA2CLV_WorstCase, []>;
def : WriteRes<WriteVSHA2MSV_M1_E32, []>;
def : WriteRes<WriteVSHA2MSV_M1_E64, []>;
def : WriteRes<WriteVSHA2MSV_M2_E32, []>;
def : WriteRes<WriteVSHA2MSV_M2_E64, []>;
def : WriteRes<WriteVSHA2MSV_M4_E32, []>;
def : WriteRes<WriteVSHA2MSV_M4_E64, []>;
def : WriteRes<WriteVSHA2MSV_M8_E32, []>;
def : WriteRes<WriteVSHA2MSV_M8_E64, []>;
def : WriteRes<WriteVSHA2MSV_MF2_E32, []>;
def : WriteRes<WriteVSHA2MSV_WorstCase, []>;
def : WriteRes<WriteVSM3CV_M1, []>;
def : WriteRes<WriteVSM3CV_M2, []>;
def : WriteRes<WriteVSM3CV_M4, []>;
def : WriteRes<WriteVSM3CV_M8, []>;
def : WriteRes<WriteVSM3CV_MF2, []>;
def : WriteRes<WriteVSM3CV_WorstCase, []>;
def : WriteRes<WriteVSM3MEV_M1, []>;
def : WriteRes<WriteVSM3MEV_M2, []>;
def : WriteRes<WriteVSM3MEV_M4, []>;
def : WriteRes<WriteVSM3MEV_M8, []>;
def : WriteRes<WriteVSM3MEV_MF2, []>;
def : WriteRes<WriteVSM3MEV_WorstCase, []>;
def : WriteRes<WriteVSM4KV_M1, []>;
def : WriteRes<WriteVSM4KV_M2, []>;
def : WriteRes<WriteVSM4KV_M4, []>;
def : WriteRes<WriteVSM4KV_M8, []>;
def : WriteRes<WriteVSM4KV_MF2, []>;
def : WriteRes<WriteVSM4KV_WorstCase, []>;
def : WriteRes<WriteVSM4RV_M1, []>;
def : WriteRes<WriteVSM4RV_M2, []>;
def : WriteRes<WriteVSM4RV_M4, []>;
def : WriteRes<WriteVSM4RV_M8, []>;
def : WriteRes<WriteVSM4RV_MF2, []>;
def : WriteRes<WriteVSMulV_M1, []>;
def : WriteRes<WriteVSMulV_M2, []>;
def : WriteRes<WriteVSMulV_M4, []>;
def : WriteRes<WriteVSMulV_M8, []>;
def : WriteRes<WriteVSMulV_MF2, []>;
def : WriteRes<WriteVSMulV_MF4, []>;
def : WriteRes<WriteVSMulV_MF8, []>;
def : WriteRes<WriteVSMulV_WorstCase, []>;
def : WriteRes<WriteVSMulX_M1, []>;
def : WriteRes<WriteVSMulX_M2, []>;
def : WriteRes<WriteVSMulX_M4, []>;
def : WriteRes<WriteVSMulX_M8, []>;
def : WriteRes<WriteVSMulX_MF2, []>;
def : WriteRes<WriteVSMulX_MF4, []>;
def : WriteRes<WriteVSMulX_MF8, []>;
def : WriteRes<WriteVSMulX_WorstCase, []>;
def : WriteRes<WriteVSOXSEG2e16_M1, []>;
def : WriteRes<WriteVSOXSEG2e16_M2, []>;
def : WriteRes<WriteVSOXSEG2e16_M4, []>;
def : WriteRes<WriteVSOXSEG2e16_MF2, []>;
def : WriteRes<WriteVSOXSEG2e16_MF4, []>;
def : WriteRes<WriteVSOXSEG2e16_MF8, []>;
def : WriteRes<WriteVSOXSEG2e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG2e32_M1, []>;
def : WriteRes<WriteVSOXSEG2e32_M2, []>;
def : WriteRes<WriteVSOXSEG2e32_M4, []>;
def : WriteRes<WriteVSOXSEG2e32_MF2, []>;
def : WriteRes<WriteVSOXSEG2e32_MF4, []>;
def : WriteRes<WriteVSOXSEG2e32_MF8, []>;
def : WriteRes<WriteVSOXSEG2e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG2e64_M1, []>;
def : WriteRes<WriteVSOXSEG2e64_M2, []>;
def : WriteRes<WriteVSOXSEG2e64_M4, []>;
def : WriteRes<WriteVSOXSEG2e64_MF2, []>;
def : WriteRes<WriteVSOXSEG2e64_MF4, []>;
def : WriteRes<WriteVSOXSEG2e64_MF8, []>;
def : WriteRes<WriteVSOXSEG2e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG2e8_M1, []>;
def : WriteRes<WriteVSOXSEG2e8_M2, []>;
def : WriteRes<WriteVSOXSEG2e8_M4, []>;
def : WriteRes<WriteVSOXSEG2e8_MF2, []>;
def : WriteRes<WriteVSOXSEG2e8_MF4, []>;
def : WriteRes<WriteVSOXSEG2e8_MF8, []>;
def : WriteRes<WriteVSOXSEG2e8_WorstCase, []>;
def : WriteRes<WriteVSOXSEG3e16_M1, []>;
def : WriteRes<WriteVSOXSEG3e16_M2, []>;
def : WriteRes<WriteVSOXSEG3e16_MF2, []>;
def : WriteRes<WriteVSOXSEG3e16_MF4, []>;
def : WriteRes<WriteVSOXSEG3e16_MF8, []>;
def : WriteRes<WriteVSOXSEG3e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG3e32_M1, []>;
def : WriteRes<WriteVSOXSEG3e32_M2, []>;
def : WriteRes<WriteVSOXSEG3e32_MF2, []>;
def : WriteRes<WriteVSOXSEG3e32_MF4, []>;
def : WriteRes<WriteVSOXSEG3e32_MF8, []>;
def : WriteRes<WriteVSOXSEG3e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG3e64_M1, []>;
def : WriteRes<WriteVSOXSEG3e64_M2, []>;
def : WriteRes<WriteVSOXSEG3e64_MF2, []>;
def : WriteRes<WriteVSOXSEG3e64_MF4, []>;
def : WriteRes<WriteVSOXSEG3e64_MF8, []>;
def : WriteRes<WriteVSOXSEG3e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG3e8_M1, []>;
def : WriteRes<WriteVSOXSEG3e8_M2, []>;
def : WriteRes<WriteVSOXSEG3e8_MF2, []>;
def : WriteRes<WriteVSOXSEG3e8_MF4, []>;
def : WriteRes<WriteVSOXSEG3e8_MF8, []>;
def : WriteRes<WriteVSOXSEG3e8_WorstCase, []>;
def : WriteRes<WriteVSOXSEG4e16_M1, []>;
def : WriteRes<WriteVSOXSEG4e16_M2, []>;
def : WriteRes<WriteVSOXSEG4e16_MF2, []>;
def : WriteRes<WriteVSOXSEG4e16_MF4, []>;
def : WriteRes<WriteVSOXSEG4e16_MF8, []>;
def : WriteRes<WriteVSOXSEG4e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG4e32_M1, []>;
def : WriteRes<WriteVSOXSEG4e32_M2, []>;
def : WriteRes<WriteVSOXSEG4e32_MF2, []>;
def : WriteRes<WriteVSOXSEG4e32_MF4, []>;
def : WriteRes<WriteVSOXSEG4e32_MF8, []>;
def : WriteRes<WriteVSOXSEG4e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG4e64_M1, []>;
def : WriteRes<WriteVSOXSEG4e64_M2, []>;
def : WriteRes<WriteVSOXSEG4e64_MF2, []>;
def : WriteRes<WriteVSOXSEG4e64_MF4, []>;
def : WriteRes<WriteVSOXSEG4e64_MF8, []>;
def : WriteRes<WriteVSOXSEG4e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG4e8_M1, []>;
def : WriteRes<WriteVSOXSEG4e8_M2, []>;
def : WriteRes<WriteVSOXSEG4e8_MF2, []>;
def : WriteRes<WriteVSOXSEG4e8_MF4, []>;
def : WriteRes<WriteVSOXSEG4e8_MF8, []>;
def : WriteRes<WriteVSOXSEG4e8_WorstCase, []>;
def : WriteRes<WriteVSOXSEG5e16_M1, []>;
def : WriteRes<WriteVSOXSEG5e16_MF2, []>;
def : WriteRes<WriteVSOXSEG5e16_MF4, []>;
def : WriteRes<WriteVSOXSEG5e16_MF8, []>;
def : WriteRes<WriteVSOXSEG5e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG5e32_M1, []>;
def : WriteRes<WriteVSOXSEG5e32_MF2, []>;
def : WriteRes<WriteVSOXSEG5e32_MF4, []>;
def : WriteRes<WriteVSOXSEG5e32_MF8, []>;
def : WriteRes<WriteVSOXSEG5e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG5e64_M1, []>;
def : WriteRes<WriteVSOXSEG5e64_MF2, []>;
def : WriteRes<WriteVSOXSEG5e64_MF4, []>;
def : WriteRes<WriteVSOXSEG5e64_MF8, []>;
def : WriteRes<WriteVSOXSEG5e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG5e8_M1, []>;
def : WriteRes<WriteVSOXSEG5e8_MF2, []>;
def : WriteRes<WriteVSOXSEG5e8_MF4, []>;
def : WriteRes<WriteVSOXSEG5e8_MF8, []>;
def : WriteRes<WriteVSOXSEG5e8_WorstCase, []>;
def : WriteRes<WriteVSOXSEG6e16_M1, []>;
def : WriteRes<WriteVSOXSEG6e16_MF2, []>;
def : WriteRes<WriteVSOXSEG6e16_MF4, []>;
def : WriteRes<WriteVSOXSEG6e16_MF8, []>;
def : WriteRes<WriteVSOXSEG6e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG6e32_M1, []>;
def : WriteRes<WriteVSOXSEG6e32_MF2, []>;
def : WriteRes<WriteVSOXSEG6e32_MF4, []>;
def : WriteRes<WriteVSOXSEG6e32_MF8, []>;
def : WriteRes<WriteVSOXSEG6e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG6e64_M1, []>;
def : WriteRes<WriteVSOXSEG6e64_MF2, []>;
def : WriteRes<WriteVSOXSEG6e64_MF4, []>;
def : WriteRes<WriteVSOXSEG6e64_MF8, []>;
def : WriteRes<WriteVSOXSEG6e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG6e8_M1, []>;
def : WriteRes<WriteVSOXSEG6e8_MF2, []>;
def : WriteRes<WriteVSOXSEG6e8_MF4, []>;
def : WriteRes<WriteVSOXSEG6e8_MF8, []>;
def : WriteRes<WriteVSOXSEG6e8_WorstCase, []>;
def : WriteRes<WriteVSOXSEG7e16_M1, []>;
def : WriteRes<WriteVSOXSEG7e16_MF2, []>;
def : WriteRes<WriteVSOXSEG7e16_MF4, []>;
def : WriteRes<WriteVSOXSEG7e16_MF8, []>;
def : WriteRes<WriteVSOXSEG7e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG7e32_M1, []>;
def : WriteRes<WriteVSOXSEG7e32_MF2, []>;
def : WriteRes<WriteVSOXSEG7e32_MF4, []>;
def : WriteRes<WriteVSOXSEG7e32_MF8, []>;
def : WriteRes<WriteVSOXSEG7e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG7e64_M1, []>;
def : WriteRes<WriteVSOXSEG7e64_MF2, []>;
def : WriteRes<WriteVSOXSEG7e64_MF4, []>;
def : WriteRes<WriteVSOXSEG7e64_MF8, []>;
def : WriteRes<WriteVSOXSEG7e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG7e8_M1, []>;
def : WriteRes<WriteVSOXSEG7e8_MF2, []>;
def : WriteRes<WriteVSOXSEG7e8_MF4, []>;
def : WriteRes<WriteVSOXSEG7e8_MF8, []>;
def : WriteRes<WriteVSOXSEG7e8_WorstCase, []>;
def : WriteRes<WriteVSOXSEG8e16_M1, []>;
def : WriteRes<WriteVSOXSEG8e16_MF2, []>;
def : WriteRes<WriteVSOXSEG8e16_MF4, []>;
def : WriteRes<WriteVSOXSEG8e16_MF8, []>;
def : WriteRes<WriteVSOXSEG8e16_WorstCase, []>;
def : WriteRes<WriteVSOXSEG8e32_M1, []>;
def : WriteRes<WriteVSOXSEG8e32_MF2, []>;
def : WriteRes<WriteVSOXSEG8e32_MF4, []>;
def : WriteRes<WriteVSOXSEG8e32_MF8, []>;
def : WriteRes<WriteVSOXSEG8e32_WorstCase, []>;
def : WriteRes<WriteVSOXSEG8e64_M1, []>;
def : WriteRes<WriteVSOXSEG8e64_MF2, []>;
def : WriteRes<WriteVSOXSEG8e64_MF4, []>;
def : WriteRes<WriteVSOXSEG8e64_MF8, []>;
def : WriteRes<WriteVSOXSEG8e64_WorstCase, []>;
def : WriteRes<WriteVSOXSEG8e8_M1, []>;
def : WriteRes<WriteVSOXSEG8e8_MF2, []>;
def : WriteRes<WriteVSOXSEG8e8_MF4, []>;
def : WriteRes<WriteVSOXSEG8e8_MF8, []>;
def : WriteRes<WriteVSOXSEG8e8_WorstCase, []>;
def : WriteRes<WriteVSSEG2e16_M1, []>;
def : WriteRes<WriteVSSEG2e16_M2, []>;
def : WriteRes<WriteVSSEG2e16_M4, []>;
def : WriteRes<WriteVSSEG2e16_MF2, []>;
def : WriteRes<WriteVSSEG2e16_MF4, []>;
def : WriteRes<WriteVSSEG2e16_WorstCase, []>;
def : WriteRes<WriteVSSEG2e32_M1, []>;
def : WriteRes<WriteVSSEG2e32_M2, []>;
def : WriteRes<WriteVSSEG2e32_M4, []>;
def : WriteRes<WriteVSSEG2e32_MF2, []>;
def : WriteRes<WriteVSSEG2e32_WorstCase, []>;
def : WriteRes<WriteVSSEG2e64_M1, []>;
def : WriteRes<WriteVSSEG2e64_M2, []>;
def : WriteRes<WriteVSSEG2e64_M4, []>;
def : WriteRes<WriteVSSEG2e64_WorstCase, []>;
def : WriteRes<WriteVSSEG2e8_M1, []>;
def : WriteRes<WriteVSSEG2e8_M2, []>;
def : WriteRes<WriteVSSEG2e8_M4, []>;
def : WriteRes<WriteVSSEG2e8_MF2, []>;
def : WriteRes<WriteVSSEG2e8_MF4, []>;
def : WriteRes<WriteVSSEG2e8_MF8, []>;
def : WriteRes<WriteVSSEG2e8_WorstCase, []>;
def : WriteRes<WriteVSSEG3e16_M1, []>;
def : WriteRes<WriteVSSEG3e16_M2, []>;
def : WriteRes<WriteVSSEG3e16_MF2, []>;
def : WriteRes<WriteVSSEG3e16_MF4, []>;
def : WriteRes<WriteVSSEG3e16_WorstCase, []>;
def : WriteRes<WriteVSSEG3e32_M1, []>;
def : WriteRes<WriteVSSEG3e32_M2, []>;
def : WriteRes<WriteVSSEG3e32_MF2, []>;
def : WriteRes<WriteVSSEG3e32_WorstCase, []>;
def : WriteRes<WriteVSSEG3e64_M1, []>;
def : WriteRes<WriteVSSEG3e64_M2, []>;
def : WriteRes<WriteVSSEG3e64_WorstCase, []>;
def : WriteRes<WriteVSSEG3e8_M1, []>;
def : WriteRes<WriteVSSEG3e8_M2, []>;
def : WriteRes<WriteVSSEG3e8_MF2, []>;
def : WriteRes<WriteVSSEG3e8_MF4, []>;
def : WriteRes<WriteVSSEG3e8_MF8, []>;
def : WriteRes<WriteVSSEG3e8_WorstCase, []>;
def : WriteRes<WriteVSSEG4e16_M1, []>;
def : WriteRes<WriteVSSEG4e16_M2, []>;
def : WriteRes<WriteVSSEG4e16_MF2, []>;
def : WriteRes<WriteVSSEG4e16_MF4, []>;
def : WriteRes<WriteVSSEG4e16_WorstCase, []>;
def : WriteRes<WriteVSSEG4e32_M1, []>;
def : WriteRes<WriteVSSEG4e32_M2, []>;
def : WriteRes<WriteVSSEG4e32_MF2, []>;
def : WriteRes<WriteVSSEG4e32_WorstCase, []>;
def : WriteRes<WriteVSSEG4e64_M1, []>;
def : WriteRes<WriteVSSEG4e64_M2, []>;
def : WriteRes<WriteVSSEG4e64_WorstCase, []>;
def : WriteRes<WriteVSSEG4e8_M1, []>;
def : WriteRes<WriteVSSEG4e8_M2, []>;
def : WriteRes<WriteVSSEG4e8_MF2, []>;
def : WriteRes<WriteVSSEG4e8_MF4, []>;
def : WriteRes<WriteVSSEG4e8_MF8, []>;
def : WriteRes<WriteVSSEG4e8_WorstCase, []>;
def : WriteRes<WriteVSSEG5e16_M1, []>;
def : WriteRes<WriteVSSEG5e16_MF2, []>;
def : WriteRes<WriteVSSEG5e16_MF4, []>;
def : WriteRes<WriteVSSEG5e16_WorstCase, []>;
def : WriteRes<WriteVSSEG5e32_M1, []>;
def : WriteRes<WriteVSSEG5e32_MF2, []>;
def : WriteRes<WriteVSSEG5e32_WorstCase, []>;
def : WriteRes<WriteVSSEG5e64_M1, []>;
def : WriteRes<WriteVSSEG5e64_WorstCase, []>;
def : WriteRes<WriteVSSEG5e8_M1, []>;
def : WriteRes<WriteVSSEG5e8_MF2, []>;
def : WriteRes<WriteVSSEG5e8_MF4, []>;
def : WriteRes<WriteVSSEG5e8_MF8, []>;
def : WriteRes<WriteVSSEG5e8_WorstCase, []>;
def : WriteRes<WriteVSSEG6e16_M1, []>;
def : WriteRes<WriteVSSEG6e16_MF2, []>;
def : WriteRes<WriteVSSEG6e16_MF4, []>;
def : WriteRes<WriteVSSEG6e16_WorstCase, []>;
def : WriteRes<WriteVSSEG6e32_M1, []>;
def : WriteRes<WriteVSSEG6e32_MF2, []>;
def : WriteRes<WriteVSSEG6e32_WorstCase, []>;
def : WriteRes<WriteVSSEG6e64_M1, []>;
def : WriteRes<WriteVSSEG6e64_WorstCase, []>;
def : WriteRes<WriteVSSEG6e8_M1, []>;
def : WriteRes<WriteVSSEG6e8_MF2, []>;
def : WriteRes<WriteVSSEG6e8_MF4, []>;
def : WriteRes<WriteVSSEG6e8_MF8, []>;
def : WriteRes<WriteVSSEG6e8_WorstCase, []>;
def : WriteRes<WriteVSSEG7e16_M1, []>;
def : WriteRes<WriteVSSEG7e16_MF2, []>;
def : WriteRes<WriteVSSEG7e16_MF4, []>;
def : WriteRes<WriteVSSEG7e16_WorstCase, []>;
def : WriteRes<WriteVSSEG7e32_M1, []>;
def : WriteRes<WriteVSSEG7e32_MF2, []>;
def : WriteRes<WriteVSSEG7e32_WorstCase, []>;
def : WriteRes<WriteVSSEG7e64_M1, []>;
def : WriteRes<WriteVSSEG7e64_WorstCase, []>;
def : WriteRes<WriteVSSEG7e8_M1, []>;
def : WriteRes<WriteVSSEG7e8_MF2, []>;
def : WriteRes<WriteVSSEG7e8_MF4, []>;
def : WriteRes<WriteVSSEG7e8_MF8, []>;
def : WriteRes<WriteVSSEG7e8_WorstCase, []>;
def : WriteRes<WriteVSSEG8e16_M1, []>;
def : WriteRes<WriteVSSEG8e16_MF2, []>;
def : WriteRes<WriteVSSEG8e16_MF4, []>;
def : WriteRes<WriteVSSEG8e16_WorstCase, []>;
def : WriteRes<WriteVSSEG8e32_M1, []>;
def : WriteRes<WriteVSSEG8e32_MF2, []>;
def : WriteRes<WriteVSSEG8e32_WorstCase, []>;
def : WriteRes<WriteVSSEG8e64_M1, []>;
def : WriteRes<WriteVSSEG8e64_WorstCase, []>;
def : WriteRes<WriteVSSEG8e8_M1, []>;
def : WriteRes<WriteVSSEG8e8_MF2, []>;
def : WriteRes<WriteVSSEG8e8_MF4, []>;
def : WriteRes<WriteVSSEG8e8_MF8, []>;
def : WriteRes<WriteVSSEG8e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG2e16_M1, []>;
def : WriteRes<WriteVSSSEG2e16_M2, []>;
def : WriteRes<WriteVSSSEG2e16_M4, []>;
def : WriteRes<WriteVSSSEG2e16_MF2, []>;
def : WriteRes<WriteVSSSEG2e16_MF4, []>;
def : WriteRes<WriteVSSSEG2e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG2e32_M1, []>;
def : WriteRes<WriteVSSSEG2e32_M2, []>;
def : WriteRes<WriteVSSSEG2e32_M4, []>;
def : WriteRes<WriteVSSSEG2e32_MF2, []>;
def : WriteRes<WriteVSSSEG2e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG2e64_M1, []>;
def : WriteRes<WriteVSSSEG2e64_M2, []>;
def : WriteRes<WriteVSSSEG2e64_M4, []>;
def : WriteRes<WriteVSSSEG2e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG2e8_M1, []>;
def : WriteRes<WriteVSSSEG2e8_M2, []>;
def : WriteRes<WriteVSSSEG2e8_M4, []>;
def : WriteRes<WriteVSSSEG2e8_MF2, []>;
def : WriteRes<WriteVSSSEG2e8_MF4, []>;
def : WriteRes<WriteVSSSEG2e8_MF8, []>;
def : WriteRes<WriteVSSSEG2e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG3e16_M1, []>;
def : WriteRes<WriteVSSSEG3e16_M2, []>;
def : WriteRes<WriteVSSSEG3e16_MF2, []>;
def : WriteRes<WriteVSSSEG3e16_MF4, []>;
def : WriteRes<WriteVSSSEG3e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG3e32_M1, []>;
def : WriteRes<WriteVSSSEG3e32_M2, []>;
def : WriteRes<WriteVSSSEG3e32_MF2, []>;
def : WriteRes<WriteVSSSEG3e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG3e64_M1, []>;
def : WriteRes<WriteVSSSEG3e64_M2, []>;
def : WriteRes<WriteVSSSEG3e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG3e8_M1, []>;
def : WriteRes<WriteVSSSEG3e8_M2, []>;
def : WriteRes<WriteVSSSEG3e8_MF2, []>;
def : WriteRes<WriteVSSSEG3e8_MF4, []>;
def : WriteRes<WriteVSSSEG3e8_MF8, []>;
def : WriteRes<WriteVSSSEG3e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG4e16_M1, []>;
def : WriteRes<WriteVSSSEG4e16_M2, []>;
def : WriteRes<WriteVSSSEG4e16_MF2, []>;
def : WriteRes<WriteVSSSEG4e16_MF4, []>;
def : WriteRes<WriteVSSSEG4e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG4e32_M1, []>;
def : WriteRes<WriteVSSSEG4e32_M2, []>;
def : WriteRes<WriteVSSSEG4e32_MF2, []>;
def : WriteRes<WriteVSSSEG4e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG4e64_M1, []>;
def : WriteRes<WriteVSSSEG4e64_M2, []>;
def : WriteRes<WriteVSSSEG4e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG4e8_M1, []>;
def : WriteRes<WriteVSSSEG4e8_M2, []>;
def : WriteRes<WriteVSSSEG4e8_MF2, []>;
def : WriteRes<WriteVSSSEG4e8_MF4, []>;
def : WriteRes<WriteVSSSEG4e8_MF8, []>;
def : WriteRes<WriteVSSSEG4e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG5e16_M1, []>;
def : WriteRes<WriteVSSSEG5e16_MF2, []>;
def : WriteRes<WriteVSSSEG5e16_MF4, []>;
def : WriteRes<WriteVSSSEG5e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG5e32_M1, []>;
def : WriteRes<WriteVSSSEG5e32_MF2, []>;
def : WriteRes<WriteVSSSEG5e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG5e64_M1, []>;
def : WriteRes<WriteVSSSEG5e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG5e8_M1, []>;
def : WriteRes<WriteVSSSEG5e8_MF2, []>;
def : WriteRes<WriteVSSSEG5e8_MF4, []>;
def : WriteRes<WriteVSSSEG5e8_MF8, []>;
def : WriteRes<WriteVSSSEG5e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG6e16_M1, []>;
def : WriteRes<WriteVSSSEG6e16_MF2, []>;
def : WriteRes<WriteVSSSEG6e16_MF4, []>;
def : WriteRes<WriteVSSSEG6e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG6e32_M1, []>;
def : WriteRes<WriteVSSSEG6e32_MF2, []>;
def : WriteRes<WriteVSSSEG6e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG6e64_M1, []>;
def : WriteRes<WriteVSSSEG6e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG6e8_M1, []>;
def : WriteRes<WriteVSSSEG6e8_MF2, []>;
def : WriteRes<WriteVSSSEG6e8_MF4, []>;
def : WriteRes<WriteVSSSEG6e8_MF8, []>;
def : WriteRes<WriteVSSSEG6e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG7e16_M1, []>;
def : WriteRes<WriteVSSSEG7e16_MF2, []>;
def : WriteRes<WriteVSSSEG7e16_MF4, []>;
def : WriteRes<WriteVSSSEG7e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG7e32_M1, []>;
def : WriteRes<WriteVSSSEG7e32_MF2, []>;
def : WriteRes<WriteVSSSEG7e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG7e64_M1, []>;
def : WriteRes<WriteVSSSEG7e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG7e8_M1, []>;
def : WriteRes<WriteVSSSEG7e8_MF2, []>;
def : WriteRes<WriteVSSSEG7e8_MF4, []>;
def : WriteRes<WriteVSSSEG7e8_MF8, []>;
def : WriteRes<WriteVSSSEG7e8_WorstCase, []>;
def : WriteRes<WriteVSSSEG8e16_M1, []>;
def : WriteRes<WriteVSSSEG8e16_MF2, []>;
def : WriteRes<WriteVSSSEG8e16_MF4, []>;
def : WriteRes<WriteVSSSEG8e16_WorstCase, []>;
def : WriteRes<WriteVSSSEG8e32_M1, []>;
def : WriteRes<WriteVSSSEG8e32_MF2, []>;
def : WriteRes<WriteVSSSEG8e32_WorstCase, []>;
def : WriteRes<WriteVSSSEG8e64_M1, []>;
def : WriteRes<WriteVSSSEG8e64_WorstCase, []>;
def : WriteRes<WriteVSSSEG8e8_M1, []>;
def : WriteRes<WriteVSSSEG8e8_MF2, []>;
def : WriteRes<WriteVSSSEG8e8_MF4, []>;
def : WriteRes<WriteVSSSEG8e8_MF8, []>;
def : WriteRes<WriteVSSSEG8e8_WorstCase, []>;
def : WriteRes<WriteVSShiftI_M1, []>;
def : WriteRes<WriteVSShiftI_M2, []>;
def : WriteRes<WriteVSShiftI_M4, []>;
def : WriteRes<WriteVSShiftI_M8, []>;
def : WriteRes<WriteVSShiftI_MF2, []>;
def : WriteRes<WriteVSShiftI_MF4, []>;
def : WriteRes<WriteVSShiftI_MF8, []>;
def : WriteRes<WriteVSShiftI_WorstCase, []>;
def : WriteRes<WriteVSShiftV_M1, []>;
def : WriteRes<WriteVSShiftV_M2, []>;
def : WriteRes<WriteVSShiftV_M4, []>;
def : WriteRes<WriteVSShiftV_M8, []>;
def : WriteRes<WriteVSShiftV_MF2, []>;
def : WriteRes<WriteVSShiftV_MF4, []>;
def : WriteRes<WriteVSShiftV_MF8, []>;
def : WriteRes<WriteVSShiftV_WorstCase, []>;
def : WriteRes<WriteVSShiftX_M1, []>;
def : WriteRes<WriteVSShiftX_M2, []>;
def : WriteRes<WriteVSShiftX_M4, []>;
def : WriteRes<WriteVSShiftX_M8, []>;
def : WriteRes<WriteVSShiftX_MF2, []>;
def : WriteRes<WriteVSShiftX_MF4, []>;
def : WriteRes<WriteVSShiftX_MF8, []>;
def : WriteRes<WriteVSShiftX_WorstCase, []>;
def : WriteRes<WriteVSTM_M1, []>;
def : WriteRes<WriteVSTM_M2, []>;
def : WriteRes<WriteVSTM_M4, []>;
def : WriteRes<WriteVSTM_M8, []>;
def : WriteRes<WriteVSTM_MF2, []>;
def : WriteRes<WriteVSTM_MF4, []>;
def : WriteRes<WriteVSTM_MF8, []>;
def : WriteRes<WriteVSTM_WorstCase, []>;
def : WriteRes<WriteVSTOX16_M1, []>;
def : WriteRes<WriteVSTOX16_M2, []>;
def : WriteRes<WriteVSTOX16_M4, []>;
def : WriteRes<WriteVSTOX16_M8, []>;
def : WriteRes<WriteVSTOX16_MF2, []>;
def : WriteRes<WriteVSTOX16_MF4, []>;
def : WriteRes<WriteVSTOX16_WorstCase, []>;
def : WriteRes<WriteVSTOX32_M1, []>;
def : WriteRes<WriteVSTOX32_M2, []>;
def : WriteRes<WriteVSTOX32_M4, []>;
def : WriteRes<WriteVSTOX32_M8, []>;
def : WriteRes<WriteVSTOX32_MF2, []>;
def : WriteRes<WriteVSTOX32_WorstCase, []>;
def : WriteRes<WriteVSTOX64_M1, []>;
def : WriteRes<WriteVSTOX64_M2, []>;
def : WriteRes<WriteVSTOX64_M4, []>;
def : WriteRes<WriteVSTOX64_M8, []>;
def : WriteRes<WriteVSTOX64_WorstCase, []>;
def : WriteRes<WriteVSTOX8_M1, []>;
def : WriteRes<WriteVSTOX8_M2, []>;
def : WriteRes<WriteVSTOX8_M4, []>;
def : WriteRes<WriteVSTOX8_M8, []>;
def : WriteRes<WriteVSTOX8_MF2, []>;
def : WriteRes<WriteVSTOX8_MF4, []>;
def : WriteRes<WriteVSTOX8_MF8, []>;
def : WriteRes<WriteVSTOX8_WorstCase, []>;
def : WriteRes<WriteVSTS64_M1, []>;
def : WriteRes<WriteVSTS64_M2, []>;
def : WriteRes<WriteVSTS64_M4, []>;
def : WriteRes<WriteVSTS64_M8, []>;
def : WriteRes<WriteVSTS64_WorstCase, []>;
def : WriteRes<WriteVSTS8_M1, []>;
def : WriteRes<WriteVSTS8_M2, []>;
def : WriteRes<WriteVSTS8_M4, []>;
def : WriteRes<WriteVSTS8_M8, []>;
def : WriteRes<WriteVSTS8_MF2, []>;
def : WriteRes<WriteVSTS8_MF4, []>;
def : WriteRes<WriteVSTS8_MF8, []>;
def : WriteRes<WriteVSTS8_WorstCase, []>;
def : WriteRes<WriteVSTUX64_M1, []>;
def : WriteRes<WriteVSTUX64_M2, []>;
def : WriteRes<WriteVSTUX64_M4, []>;
def : WriteRes<WriteVSTUX64_M8, []>;
def : WriteRes<WriteVSTUX64_WorstCase, []>;
def : WriteRes<WriteVSTUX8_M1, []>;
def : WriteRes<WriteVSTUX8_M2, []>;
def : WriteRes<WriteVSTUX8_M4, []>;
def : WriteRes<WriteVSTUX8_M8, []>;
def : WriteRes<WriteVSTUX8_MF2, []>;
def : WriteRes<WriteVSTUX8_MF4, []>;
def : WriteRes<WriteVSTUX8_MF8, []>;
def : WriteRes<WriteVSTUX8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG2e16_M1, []>;
def : WriteRes<WriteVSUXSEG2e16_M2, []>;
def : WriteRes<WriteVSUXSEG2e16_M4, []>;
def : WriteRes<WriteVSUXSEG2e16_MF2, []>;
def : WriteRes<WriteVSUXSEG2e16_MF4, []>;
def : WriteRes<WriteVSUXSEG2e16_MF8, []>;
def : WriteRes<WriteVSUXSEG2e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG2e32_M1, []>;
def : WriteRes<WriteVSUXSEG2e32_M2, []>;
def : WriteRes<WriteVSUXSEG2e32_M4, []>;
def : WriteRes<WriteVSUXSEG2e32_MF2, []>;
def : WriteRes<WriteVSUXSEG2e32_MF4, []>;
def : WriteRes<WriteVSUXSEG2e32_MF8, []>;
def : WriteRes<WriteVSUXSEG2e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG2e64_M1, []>;
def : WriteRes<WriteVSUXSEG2e64_M2, []>;
def : WriteRes<WriteVSUXSEG2e64_M4, []>;
def : WriteRes<WriteVSUXSEG2e64_MF2, []>;
def : WriteRes<WriteVSUXSEG2e64_MF4, []>;
def : WriteRes<WriteVSUXSEG2e64_MF8, []>;
def : WriteRes<WriteVSUXSEG2e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG2e8_M1, []>;
def : WriteRes<WriteVSUXSEG2e8_M2, []>;
def : WriteRes<WriteVSUXSEG2e8_M4, []>;
def : WriteRes<WriteVSUXSEG2e8_MF2, []>;
def : WriteRes<WriteVSUXSEG2e8_MF4, []>;
def : WriteRes<WriteVSUXSEG2e8_MF8, []>;
def : WriteRes<WriteVSUXSEG2e8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG3e16_M1, []>;
def : WriteRes<WriteVSUXSEG3e16_M2, []>;
def : WriteRes<WriteVSUXSEG3e16_MF2, []>;
def : WriteRes<WriteVSUXSEG3e16_MF4, []>;
def : WriteRes<WriteVSUXSEG3e16_MF8, []>;
def : WriteRes<WriteVSUXSEG3e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG3e32_M1, []>;
def : WriteRes<WriteVSUXSEG3e32_M2, []>;
def : WriteRes<WriteVSUXSEG3e32_MF2, []>;
def : WriteRes<WriteVSUXSEG3e32_MF4, []>;
def : WriteRes<WriteVSUXSEG3e32_MF8, []>;
def : WriteRes<WriteVSUXSEG3e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG3e64_M1, []>;
def : WriteRes<WriteVSUXSEG3e64_M2, []>;
def : WriteRes<WriteVSUXSEG3e64_MF2, []>;
def : WriteRes<WriteVSUXSEG3e64_MF4, []>;
def : WriteRes<WriteVSUXSEG3e64_MF8, []>;
def : WriteRes<WriteVSUXSEG3e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG3e8_M1, []>;
def : WriteRes<WriteVSUXSEG3e8_M2, []>;
def : WriteRes<WriteVSUXSEG3e8_MF2, []>;
def : WriteRes<WriteVSUXSEG3e8_MF4, []>;
def : WriteRes<WriteVSUXSEG3e8_MF8, []>;
def : WriteRes<WriteVSUXSEG3e8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG4e16_M1, []>;
def : WriteRes<WriteVSUXSEG4e16_M2, []>;
def : WriteRes<WriteVSUXSEG4e16_MF2, []>;
def : WriteRes<WriteVSUXSEG4e16_MF4, []>;
def : WriteRes<WriteVSUXSEG4e16_MF8, []>;
def : WriteRes<WriteVSUXSEG4e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG4e32_M1, []>;
def : WriteRes<WriteVSUXSEG4e32_M2, []>;
def : WriteRes<WriteVSUXSEG4e32_MF2, []>;
def : WriteRes<WriteVSUXSEG4e32_MF4, []>;
def : WriteRes<WriteVSUXSEG4e32_MF8, []>;
def : WriteRes<WriteVSUXSEG4e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG4e64_M1, []>;
def : WriteRes<WriteVSUXSEG4e64_M2, []>;
def : WriteRes<WriteVSUXSEG4e64_MF2, []>;
def : WriteRes<WriteVSUXSEG4e64_MF4, []>;
def : WriteRes<WriteVSUXSEG4e64_MF8, []>;
def : WriteRes<WriteVSUXSEG4e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG4e8_M1, []>;
def : WriteRes<WriteVSUXSEG4e8_M2, []>;
def : WriteRes<WriteVSUXSEG4e8_MF2, []>;
def : WriteRes<WriteVSUXSEG4e8_MF4, []>;
def : WriteRes<WriteVSUXSEG4e8_MF8, []>;
def : WriteRes<WriteVSUXSEG4e8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG5e16_M1, []>;
def : WriteRes<WriteVSUXSEG5e16_MF2, []>;
def : WriteRes<WriteVSUXSEG5e16_MF4, []>;
def : WriteRes<WriteVSUXSEG5e16_MF8, []>;
def : WriteRes<WriteVSUXSEG5e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG5e32_M1, []>;
def : WriteRes<WriteVSUXSEG5e32_MF2, []>;
def : WriteRes<WriteVSUXSEG5e32_MF4, []>;
def : WriteRes<WriteVSUXSEG5e32_MF8, []>;
def : WriteRes<WriteVSUXSEG5e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG5e64_M1, []>;
def : WriteRes<WriteVSUXSEG5e64_MF2, []>;
def : WriteRes<WriteVSUXSEG5e64_MF4, []>;
def : WriteRes<WriteVSUXSEG5e64_MF8, []>;
def : WriteRes<WriteVSUXSEG5e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG5e8_M1, []>;
def : WriteRes<WriteVSUXSEG5e8_MF2, []>;
def : WriteRes<WriteVSUXSEG5e8_MF4, []>;
def : WriteRes<WriteVSUXSEG5e8_MF8, []>;
def : WriteRes<WriteVSUXSEG5e8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG6e16_M1, []>;
def : WriteRes<WriteVSUXSEG6e16_MF2, []>;
def : WriteRes<WriteVSUXSEG6e16_MF4, []>;
def : WriteRes<WriteVSUXSEG6e16_MF8, []>;
def : WriteRes<WriteVSUXSEG6e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG6e32_M1, []>;
def : WriteRes<WriteVSUXSEG6e32_MF2, []>;
def : WriteRes<WriteVSUXSEG6e32_MF4, []>;
def : WriteRes<WriteVSUXSEG6e32_MF8, []>;
def : WriteRes<WriteVSUXSEG6e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG6e64_M1, []>;
def : WriteRes<WriteVSUXSEG6e64_MF2, []>;
def : WriteRes<WriteVSUXSEG6e64_MF4, []>;
def : WriteRes<WriteVSUXSEG6e64_MF8, []>;
def : WriteRes<WriteVSUXSEG6e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG6e8_M1, []>;
def : WriteRes<WriteVSUXSEG6e8_MF2, []>;
def : WriteRes<WriteVSUXSEG6e8_MF4, []>;
def : WriteRes<WriteVSUXSEG6e8_MF8, []>;
def : WriteRes<WriteVSUXSEG6e8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG7e16_M1, []>;
def : WriteRes<WriteVSUXSEG7e16_MF2, []>;
def : WriteRes<WriteVSUXSEG7e16_MF4, []>;
def : WriteRes<WriteVSUXSEG7e16_MF8, []>;
def : WriteRes<WriteVSUXSEG7e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG7e32_M1, []>;
def : WriteRes<WriteVSUXSEG7e32_MF2, []>;
def : WriteRes<WriteVSUXSEG7e32_MF4, []>;
def : WriteRes<WriteVSUXSEG7e32_MF8, []>;
def : WriteRes<WriteVSUXSEG7e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG7e64_M1, []>;
def : WriteRes<WriteVSUXSEG7e64_MF2, []>;
def : WriteRes<WriteVSUXSEG7e64_MF4, []>;
def : WriteRes<WriteVSUXSEG7e64_MF8, []>;
def : WriteRes<WriteVSUXSEG7e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG7e8_M1, []>;
def : WriteRes<WriteVSUXSEG7e8_MF2, []>;
def : WriteRes<WriteVSUXSEG7e8_MF4, []>;
def : WriteRes<WriteVSUXSEG7e8_MF8, []>;
def : WriteRes<WriteVSUXSEG7e8_WorstCase, []>;
def : WriteRes<WriteVSUXSEG8e16_M1, []>;
def : WriteRes<WriteVSUXSEG8e16_MF2, []>;
def : WriteRes<WriteVSUXSEG8e16_MF4, []>;
def : WriteRes<WriteVSUXSEG8e16_MF8, []>;
def : WriteRes<WriteVSUXSEG8e16_WorstCase, []>;
def : WriteRes<WriteVSUXSEG8e32_M1, []>;
def : WriteRes<WriteVSUXSEG8e32_MF2, []>;
def : WriteRes<WriteVSUXSEG8e32_MF4, []>;
def : WriteRes<WriteVSUXSEG8e32_MF8, []>;
def : WriteRes<WriteVSUXSEG8e32_WorstCase, []>;
def : WriteRes<WriteVSUXSEG8e64_M1, []>;
def : WriteRes<WriteVSUXSEG8e64_MF2, []>;
def : WriteRes<WriteVSUXSEG8e64_MF4, []>;
def : WriteRes<WriteVSUXSEG8e64_MF8, []>;
def : WriteRes<WriteVSUXSEG8e64_WorstCase, []>;
def : WriteRes<WriteVSUXSEG8e8_M1, []>;
def : WriteRes<WriteVSUXSEG8e8_MF2, []>;
def : WriteRes<WriteVSUXSEG8e8_MF4, []>;
def : WriteRes<WriteVSUXSEG8e8_MF8, []>;
def : WriteRes<WriteVSUXSEG8e8_WorstCase, []>;
def : WriteRes<WriteVWSLLI_M1, []>;
def : WriteRes<WriteVWSLLI_M2, []>;
def : WriteRes<WriteVWSLLI_M4, []>;
def : WriteRes<WriteVWSLLI_MF2, []>;
def : WriteRes<WriteVWSLLI_MF4, []>;
def : WriteRes<WriteVWSLLI_MF8, []>;
def : WriteRes<WriteVWSLLV_M1, []>;
def : WriteRes<WriteVWSLLV_M2, []>;
def : WriteRes<WriteVWSLLV_M4, []>;
def : WriteRes<WriteVWSLLV_MF2, []>;
def : WriteRes<WriteVWSLLV_MF4, []>;
def : WriteRes<WriteVWSLLV_MF8, []>;
def : WriteRes<WriteVWSLLX_M1, []>;
def : WriteRes<WriteVWSLLX_M2, []>;
def : WriteRes<WriteVWSLLX_M4, []>;
def : WriteRes<WriteVWSLLX_MF2, []>;
def : WriteRes<WriteVWSLLX_MF4, []>;
def : WriteRes<WriteVWSLLX_MF8, []>;
def : WriteRes<WriteXPERM, []>;
def : WriteRes<WriteZIP, []>;
}

//===----------------------------------------------------------------------===//
// Read advances.  Every SchedRead referenced by a TX8 instruction needs one;
// only the rotate reads are non-zero in the vendor table.
//===----------------------------------------------------------------------===//
def : ReadAdvance<ReadAtomicBA, 0>;
def : ReadAdvance<ReadAtomicBD, 0>;
def : ReadAdvance<ReadAtomicDA, 0>;
def : ReadAdvance<ReadAtomicDD, 0>;
def : ReadAdvance<ReadAtomicHA, 0>;
def : ReadAdvance<ReadAtomicHD, 0>;
def : ReadAdvance<ReadAtomicLDD, 0>;
def : ReadAdvance<ReadAtomicLDW, 0>;
def : ReadAdvance<ReadAtomicSTD, 0>;
def : ReadAdvance<ReadAtomicSTW, 0>;
def : ReadAdvance<ReadAtomicWA, 0>;
def : ReadAdvance<ReadAtomicWD, 0>;
def : ReadAdvance<ReadBREV8, 0>;
def : ReadAdvance<ReadCLMUL, 0>;
def : ReadAdvance<ReadCLZ, 0>;
def : ReadAdvance<ReadCLZ32, 0>;
def : ReadAdvance<ReadCPOP, 0>;
def : ReadAdvance<ReadCPOP32, 0>;
def : ReadAdvance<ReadCSR, 0>;
def : ReadAdvance<ReadCTZ, 0>;
def : ReadAdvance<ReadCTZ32, 0>;
def : ReadAdvance<ReadFAdd128, 0>;
def : ReadAdvance<ReadFAdd16, 0>;
def : ReadAdvance<ReadFAdd32, 0>;
def : ReadAdvance<ReadFAdd64, 0>;
def : ReadAdvance<ReadFClass128, 0>;
def : ReadAdvance<ReadFClass16, 0>;
def : ReadAdvance<ReadFClass32, 0>;
def : ReadAdvance<ReadFClass64, 0>;
def : ReadAdvance<ReadFCmp128, 0>;
def : ReadAdvance<ReadFCmp16, 0>;
def : ReadAdvance<ReadFCmp32, 0>;
def : ReadAdvance<ReadFCmp64, 0>;
def : ReadAdvance<ReadFCvtF128ToF32, 0>;
def : ReadAdvance<ReadFCvtF128ToF64, 0>;
def : ReadAdvance<ReadFCvtF128ToI32, 0>;
def : ReadAdvance<ReadFCvtF128ToI64, 0>;
def : ReadAdvance<ReadFCvtF16ToF32, 0>;
def : ReadAdvance<ReadFCvtF16ToF64, 0>;
def : ReadAdvance<ReadFCvtF16ToI32, 0>;
def : ReadAdvance<ReadFCvtF16ToI64, 0>;
def : ReadAdvance<ReadFCvtF32ToF128, 0>;
def : ReadAdvance<ReadFCvtF32ToF16, 0>;
def : ReadAdvance<ReadFCvtF32ToF64, 0>;
def : ReadAdvance<ReadFCvtF32ToI32, 0>;
def : ReadAdvance<ReadFCvtF32ToI64, 0>;
def : ReadAdvance<ReadFCvtF64ToF128, 0>;
def : ReadAdvance<ReadFCvtF64ToF16, 0>;
def : ReadAdvance<ReadFCvtF64ToF32, 0>;
def : ReadAdvance<ReadFCvtF64ToI32, 0>;
def : ReadAdvance<ReadFCvtF64ToI64, 0>;
def : ReadAdvance<ReadFCvtI32ToF128, 0>;
def : ReadAdvance<ReadFCvtI32ToF16, 0>;
def : ReadAdvance<ReadFCvtI32ToF32, 0>;
def : ReadAdvance<ReadFCvtI32ToF64, 0>;
def : ReadAdvance<ReadFCvtI64ToF128, 0>;
def : ReadAdvance<ReadFCvtI64ToF16, 0>;
def : ReadAdvance<ReadFCvtI64ToF32, 0>;
def : ReadAdvance<ReadFCvtI64ToF64, 0>;
def : ReadAdvance<ReadFDiv128, 0>;
def : ReadAdvance<ReadFDiv16, 0>;
def : ReadAdvance<ReadFDiv32, 0>;
def : ReadAdvance<ReadFDiv64, 0>;
def : ReadAdvance<ReadFMA128, 0>;
def : ReadAdvance<ReadFMA128Addend, 0>;
def : ReadAdvance<ReadFMA16, 0>;
def : ReadAdvance<ReadFMA16Addend, 0>;
def : ReadAdvance<ReadFMA32, 0>;
def : ReadAdvance<ReadFMA32Addend, 0>;
def : ReadAdvance<ReadFMA64, 0>;
def : ReadAdvance<ReadFMA64Addend, 0>;
def : ReadAdvance<ReadFMemBase, 0>;
def : ReadAdvance<ReadFMinMax128, 0>;
def : ReadAdvance<ReadFMinMax16, 0>;
def : ReadAdvance<ReadFMinMax32, 0>;
def : ReadAdvance<ReadFMinMax64, 0>;
def : ReadAdvance<ReadFMovF16ToI16, 0>;
def : ReadAdvance<ReadFMovF32ToI32, 0>;
def : ReadAdvance<ReadFMovF64ToI64, 0>;
def : ReadAdvance<ReadFMovI16ToF16, 0>;
def : ReadAdvance<ReadFMovI32ToF32, 0>;
def : ReadAdvance<ReadFMovI64ToF64, 0>;
def : ReadAdvance<ReadFMul128, 0>;
def : ReadAdvance<ReadFMul16, 0>;
def : ReadAdvance<ReadFMul32, 0>;
def : ReadAdvance<ReadFMul64, 0>;
def : ReadAdvance<ReadFRoundF128, 0>;
def : ReadAdvance<ReadFRoundF16, 0>;
def : ReadAdvance<ReadFRoundF32, 0>;
def : ReadAdvance<ReadFRoundF64, 0>;
def : ReadAdvance<ReadFSGNJ128, 0>;
def : ReadAdvance<ReadFSGNJ16, 0>;
def : ReadAdvance<ReadFSGNJ32, 0>;
def : ReadAdvance<ReadFSGNJ64, 0>;
def : ReadAdvance<ReadFSqrt128, 0>;
def : ReadAdvance<ReadFSqrt16, 0>;
def : ReadAdvance<ReadFSqrt32, 0>;
def : ReadAdvance<ReadFSqrt64, 0>;
def : ReadAdvance<ReadFStoreData, 0>;
def : ReadAdvance<ReadIALU, 0>;
def : ReadAdvance<ReadIALU32, 0>;
def : ReadAdvance<ReadIDiv, 0>;
def : ReadAdvance<ReadIDiv32, 0>;
def : ReadAdvance<ReadIMinMax, 0>;
def : ReadAdvance<ReadIMul, 0>;
def : ReadAdvance<ReadIMul32, 0>;
def : ReadAdvance<ReadIRem, 0>;
def : ReadAdvance<ReadIRem32, 0>;
def : ReadAdvance<ReadJalr, 0>;
def : ReadAdvance<ReadJmp, 0>;
def : ReadAdvance<ReadMemBase, 0>;
def : ReadAdvance<ReadORCB, 0>;
def : ReadAdvance<ReadPACK, 0>;
def : ReadAdvance<ReadPACK32, 0>;
def : ReadAdvance<ReadREV8, 0>;
def : ReadAdvance<ReadRotateImm, 1>;
def : ReadAdvance<ReadRotateImm32, 1>;
def : ReadAdvance<ReadRotateReg, 1>;
def : ReadAdvance<ReadRotateReg32, 1>;
def : ReadAdvance<ReadSFBALU, 0>;
def : ReadAdvance<ReadSFBJmp, 0>;
def : ReadAdvance<ReadSF_VFNRClipF_M1, 0>;
def : ReadAdvance<ReadSF_VFNRClipF_M2, 0>;
def : ReadAdvance<ReadSF_VFNRClipF_MF2, 0>;
def : ReadAdvance<ReadSF_VFNRClipF_MF4, 0>;
def : ReadAdvance<ReadSF_VFNRClipF_MF8, 0>;
def : ReadAdvance<ReadSF_VFNRClipF_WorstCase, 0>;
def : ReadAdvance<ReadSF_VFNRClipV_M1, 0>;
def : ReadAdvance<ReadSF_VFNRClipV_M2, 0>;
def : ReadAdvance<ReadSF_VFNRClipV_MF2, 0>;
def : ReadAdvance<ReadSF_VFNRClipV_MF4, 0>;
def : ReadAdvance<ReadSF_VFNRClipV_MF8, 0>;
def : ReadAdvance<ReadSF_VFNRClipV_WorstCase, 0>;
def : ReadAdvance<ReadSF_VFWMACC_QQQ_M1, 0>;
def : ReadAdvance<ReadSF_VFWMACC_QQQ_M2, 0>;
def : ReadAdvance<ReadSF_VFWMACC_QQQ_M4, 0>;
def : ReadAdvance<ReadSF_VFWMACC_QQQ_MF2, 0>;
def : ReadAdvance<ReadSF_VFWMACC_QQQ_MF4, 0>;
def : ReadAdvance<ReadSF_VFWMACC_QQQ_WorstCase, 0>;
def : ReadAdvance<ReadSF_VQMACC_DOD_M1, 0>;
def : ReadAdvance<ReadSF_VQMACC_DOD_M2, 0>;
def : ReadAdvance<ReadSF_VQMACC_DOD_M4, 0>;
def : ReadAdvance<ReadSF_VQMACC_DOD_M8, 0>;
def : ReadAdvance<ReadSF_VQMACC_DOD_WorstCase, 0>;
def : ReadAdvance<ReadSF_VQMACC_QOQ_M1, 0>;
def : ReadAdvance<ReadSF_VQMACC_QOQ_M2, 0>;
def : ReadAdvance<ReadSF_VQMACC_QOQ_M4, 0>;
def : ReadAdvance<ReadSF_VQMACC_QOQ_MF2, 0>;
def : ReadAdvance<ReadSF_VQMACC_QOQ_WorstCase, 0>;
def : ReadAdvance<ReadSHXADD, 0>;
def : ReadAdvance<ReadSHXADD32, 0>;
def : ReadAdvance<ReadShiftImm, 0>;
def : ReadAdvance<ReadShiftImm32, 0>;
def : ReadAdvance<ReadShiftReg, 0>;
def : ReadAdvance<ReadShiftReg32, 0>;
def : ReadAdvance<ReadSingleBit, 0>;
def : ReadAdvance<ReadSingleBitImm, 0>;
def : ReadAdvance<ReadStoreData, 0>;
def : ReadAdvance<ReadVAALUV_M1, 0>;
def : ReadAdvance<ReadVAALUV_M2, 0>;
def : ReadAdvance<ReadVAALUV_M4, 0>;
def : ReadAdvance<ReadVAALUV_M8, 0>;
def : ReadAdvance<ReadVAALUV_MF2, 0>;
def : ReadAdvance<ReadVAALUV_MF4, 0>;
def : ReadAdvance<ReadVAALUV_MF8, 0>;
def : ReadAdvance<ReadVAALUV_WorstCase, 0>;
def : ReadAdvance<ReadVAALUX_M1, 0>;
def : ReadAdvance<ReadVAALUX_M2, 0>;
def : ReadAdvance<ReadVAALUX_M4, 0>;
def : ReadAdvance<ReadVAALUX_M8, 0>;
def : ReadAdvance<ReadVAALUX_MF2, 0>;
def : ReadAdvance<ReadVAALUX_MF4, 0>;
def : ReadAdvance<ReadVAALUX_MF8, 0>;
def : ReadAdvance<ReadVAALUX_WorstCase, 0>;
def : ReadAdvance<ReadVAESKF1V_M1, 0>;
def : ReadAdvance<ReadVAESKF1V_M2, 0>;
def : ReadAdvance<ReadVAESKF1V_M4, 0>;
def : ReadAdvance<ReadVAESKF1V_M8, 0>;
def : ReadAdvance<ReadVAESKF1V_MF2, 0>;
def : ReadAdvance<ReadVAESKF1V_WorstCase, 0>;
def : ReadAdvance<ReadVAESKF2V_M1, 0>;
def : ReadAdvance<ReadVAESKF2V_M2, 0>;
def : ReadAdvance<ReadVAESKF2V_M4, 0>;
def : ReadAdvance<ReadVAESKF2V_M8, 0>;
def : ReadAdvance<ReadVAESKF2V_MF2, 0>;
def : ReadAdvance<ReadVAESKF2V_WorstCase, 0>;
def : ReadAdvance<ReadVAESMVV_M1, 0>;
def : ReadAdvance<ReadVAESMVV_M2, 0>;
def : ReadAdvance<ReadVAESMVV_M4, 0>;
def : ReadAdvance<ReadVAESMVV_M8, 0>;
def : ReadAdvance<ReadVAESMVV_MF2, 0>;
def : ReadAdvance<ReadVAESMVV_WorstCase, 0>;
def : ReadAdvance<ReadVAESZV_M1, 0>;
def : ReadAdvance<ReadVAESZV_M2, 0>;
def : ReadAdvance<ReadVAESZV_M4, 0>;
def : ReadAdvance<ReadVAESZV_M8, 0>;
def : ReadAdvance<ReadVAESZV_MF2, 0>;
def : ReadAdvance<ReadVAESZV_WorstCase, 0>;
def : ReadAdvance<ReadVBREV8V_M1, 0>;
def : ReadAdvance<ReadVBREV8V_M2, 0>;
def : ReadAdvance<ReadVBREV8V_M4, 0>;
def : ReadAdvance<ReadVBREV8V_M8, 0>;
def : ReadAdvance<ReadVBREV8V_MF2, 0>;
def : ReadAdvance<ReadVBREV8V_MF4, 0>;
def : ReadAdvance<ReadVBREV8V_MF8, 0>;
def : ReadAdvance<ReadVBREVV_M1, 0>;
def : ReadAdvance<ReadVBREVV_M2, 0>;
def : ReadAdvance<ReadVBREVV_M4, 0>;
def : ReadAdvance<ReadVBREVV_M8, 0>;
def : ReadAdvance<ReadVBREVV_MF2, 0>;
def : ReadAdvance<ReadVBREVV_MF4, 0>;
def : ReadAdvance<ReadVBREVV_MF8, 0>;
def : ReadAdvance<ReadVCLMULV_M1, 0>;
def : ReadAdvance<ReadVCLMULV_M2, 0>;
def : ReadAdvance<ReadVCLMULV_M4, 0>;
def : ReadAdvance<ReadVCLMULV_M8, 0>;
def : ReadAdvance<ReadVCLMULV_MF2, 0>;
def : ReadAdvance<ReadVCLMULV_MF4, 0>;
def : ReadAdvance<ReadVCLMULV_MF8, 0>;
def : ReadAdvance<ReadVCLMULV_WorstCase, 0>;
def : ReadAdvance<ReadVCLMULX_M1, 0>;
def : ReadAdvance<ReadVCLMULX_M2, 0>;
def : ReadAdvance<ReadVCLMULX_M4, 0>;
def : ReadAdvance<ReadVCLMULX_M8, 0>;
def : ReadAdvance<ReadVCLMULX_MF2, 0>;
def : ReadAdvance<ReadVCLMULX_MF4, 0>;
def : ReadAdvance<ReadVCLMULX_MF8, 0>;
def : ReadAdvance<ReadVCLMULX_WorstCase, 0>;
def : ReadAdvance<ReadVCLZV_M1, 0>;
def : ReadAdvance<ReadVCLZV_M2, 0>;
def : ReadAdvance<ReadVCLZV_M4, 0>;
def : ReadAdvance<ReadVCLZV_M8, 0>;
def : ReadAdvance<ReadVCLZV_MF2, 0>;
def : ReadAdvance<ReadVCLZV_MF4, 0>;
def : ReadAdvance<ReadVCLZV_MF8, 0>;
def : ReadAdvance<ReadVCPOPV_M1, 0>;
def : ReadAdvance<ReadVCPOPV_M2, 0>;
def : ReadAdvance<ReadVCPOPV_M4, 0>;
def : ReadAdvance<ReadVCPOPV_M8, 0>;
def : ReadAdvance<ReadVCPOPV_MF2, 0>;
def : ReadAdvance<ReadVCPOPV_MF4, 0>;
def : ReadAdvance<ReadVCPOPV_MF8, 0>;
def : ReadAdvance<ReadVCTZV_M1, 0>;
def : ReadAdvance<ReadVCTZV_M2, 0>;
def : ReadAdvance<ReadVCTZV_M4, 0>;
def : ReadAdvance<ReadVCTZV_M8, 0>;
def : ReadAdvance<ReadVCTZV_MF2, 0>;
def : ReadAdvance<ReadVCTZV_MF4, 0>;
def : ReadAdvance<ReadVCTZV_MF8, 0>;
def : ReadAdvance<ReadVCompressV_M1_E16, 0>;
def : ReadAdvance<ReadVCompressV_M1_E32, 0>;
def : ReadAdvance<ReadVCompressV_M1_E64, 0>;
def : ReadAdvance<ReadVCompressV_M1_E8, 0>;
def : ReadAdvance<ReadVCompressV_M2_E16, 0>;
def : ReadAdvance<ReadVCompressV_M2_E32, 0>;
def : ReadAdvance<ReadVCompressV_M2_E64, 0>;
def : ReadAdvance<ReadVCompressV_M2_E8, 0>;
def : ReadAdvance<ReadVCompressV_M4_E16, 0>;
def : ReadAdvance<ReadVCompressV_M4_E32, 0>;
def : ReadAdvance<ReadVCompressV_M4_E64, 0>;
def : ReadAdvance<ReadVCompressV_M4_E8, 0>;
def : ReadAdvance<ReadVCompressV_M8_E16, 0>;
def : ReadAdvance<ReadVCompressV_M8_E32, 0>;
def : ReadAdvance<ReadVCompressV_M8_E64, 0>;
def : ReadAdvance<ReadVCompressV_M8_E8, 0>;
def : ReadAdvance<ReadVCompressV_MF2_E16, 0>;
def : ReadAdvance<ReadVCompressV_MF2_E32, 0>;
def : ReadAdvance<ReadVCompressV_MF2_E8, 0>;
def : ReadAdvance<ReadVCompressV_MF4_E16, 0>;
def : ReadAdvance<ReadVCompressV_MF4_E8, 0>;
def : ReadAdvance<ReadVCompressV_MF8_E8, 0>;
def : ReadAdvance<ReadVCompressV_WorstCase, 0>;
def : ReadAdvance<ReadVExtV_M1, 0>;
def : ReadAdvance<ReadVExtV_M2, 0>;
def : ReadAdvance<ReadVExtV_M4, 0>;
def : ReadAdvance<ReadVExtV_M8, 0>;
def : ReadAdvance<ReadVExtV_MF2, 0>;
def : ReadAdvance<ReadVExtV_MF4, 0>;
def : ReadAdvance<ReadVExtV_WorstCase, 0>;
def : ReadAdvance<ReadVFALUF_M1_E16, 0>;
def : ReadAdvance<ReadVFALUF_M1_E32, 0>;
def : ReadAdvance<ReadVFALUF_M1_E64, 0>;
def : ReadAdvance<ReadVFALUF_M2_E16, 0>;
def : ReadAdvance<ReadVFALUF_M2_E32, 0>;
def : ReadAdvance<ReadVFALUF_M2_E64, 0>;
def : ReadAdvance<ReadVFALUF_M4_E16, 0>;
def : ReadAdvance<ReadVFALUF_M4_E32, 0>;
def : ReadAdvance<ReadVFALUF_M4_E64, 0>;
def : ReadAdvance<ReadVFALUF_M8_E16, 0>;
def : ReadAdvance<ReadVFALUF_M8_E32, 0>;
def : ReadAdvance<ReadVFALUF_M8_E64, 0>;
def : ReadAdvance<ReadVFALUF_MF2_E16, 0>;
def : ReadAdvance<ReadVFALUF_MF2_E32, 0>;
def : ReadAdvance<ReadVFALUF_MF4_E16, 0>;
def : ReadAdvance<ReadVFALUF_WorstCase, 0>;
def : ReadAdvance<ReadVFALUV_M1_E16, 0>;
def : ReadAdvance<ReadVFALUV_M1_E32, 0>;
def : ReadAdvance<ReadVFALUV_M1_E64, 0>;
def : ReadAdvance<ReadVFALUV_M2_E16, 0>;
def : ReadAdvance<ReadVFALUV_M2_E32, 0>;
def : ReadAdvance<ReadVFALUV_M2_E64, 0>;
def : ReadAdvance<ReadVFALUV_M4_E16, 0>;
def : ReadAdvance<ReadVFALUV_M4_E32, 0>;
def : ReadAdvance<ReadVFALUV_M4_E64, 0>;
def : ReadAdvance<ReadVFALUV_M8_E16, 0>;
def : ReadAdvance<ReadVFALUV_M8_E32, 0>;
def : ReadAdvance<ReadVFALUV_M8_E64, 0>;
def : ReadAdvance<ReadVFALUV_MF2_E16, 0>;
def : ReadAdvance<ReadVFALUV_MF2_E32, 0>;
def : ReadAdvance<ReadVFALUV_MF4_E16, 0>;
def : ReadAdvance<ReadVFALUV_WorstCase, 0>;
def : ReadAdvance<ReadVFClassV_M1, 0>;
def : ReadAdvance<ReadVFClassV_M2, 0>;
def : ReadAdvance<ReadVFClassV_M4, 0>;
def : ReadAdvance<ReadVFClassV_M8, 0>;
def : ReadAdvance<ReadVFClassV_MF2, 0>;
def : ReadAdvance<ReadVFClassV_MF4, 0>;
def : ReadAdvance<ReadVFClassV_WorstCase, 0>;
def : ReadAdvance<ReadVFCmpF_M1, 0>;
def : ReadAdvance<ReadVFCmpF_M2, 0>;
def : ReadAdvance<ReadVFCmpF_M4, 0>;
def : ReadAdvance<ReadVFCmpF_M8, 0>;
def : ReadAdvance<ReadVFCmpF_MF2, 0>;
def : ReadAdvance<ReadVFCmpF_MF4, 0>;
def : ReadAdvance<ReadVFCmpF_WorstCase, 0>;
def : ReadAdvance<ReadVFCmpV_M1, 0>;
def : ReadAdvance<ReadVFCmpV_M2, 0>;
def : ReadAdvance<ReadVFCmpV_M4, 0>;
def : ReadAdvance<ReadVFCmpV_M8, 0>;
def : ReadAdvance<ReadVFCmpV_MF2, 0>;
def : ReadAdvance<ReadVFCmpV_MF4, 0>;
def : ReadAdvance<ReadVFCmpV_WorstCase, 0>;
def : ReadAdvance<ReadVFCvtFToIV_M1, 0>;
def : ReadAdvance<ReadVFCvtFToIV_M2, 0>;
def : ReadAdvance<ReadVFCvtFToIV_M4, 0>;
def : ReadAdvance<ReadVFCvtFToIV_M8, 0>;
def : ReadAdvance<ReadVFCvtFToIV_MF2, 0>;
def : ReadAdvance<ReadVFCvtFToIV_MF4, 0>;
def : ReadAdvance<ReadVFCvtFToIV_WorstCase, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M1_E16, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M1_E32, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M1_E64, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M2_E16, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M2_E32, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M2_E64, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M4_E16, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M4_E32, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M4_E64, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M8_E16, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M8_E32, 0>;
def : ReadAdvance<ReadVFCvtIToFV_M8_E64, 0>;
def : ReadAdvance<ReadVFCvtIToFV_MF2_E16, 0>;
def : ReadAdvance<ReadVFCvtIToFV_MF2_E32, 0>;
def : ReadAdvance<ReadVFCvtIToFV_MF4_E16, 0>;
def : ReadAdvance<ReadVFCvtIToFV_WorstCase, 0>;
def : ReadAdvance<ReadVFDivF_M1_E16, 0>;
def : ReadAdvance<ReadVFDivF_M1_E32, 0>;
def : ReadAdvance<ReadVFDivF_M1_E64, 0>;
def : ReadAdvance<ReadVFDivF_M2_E16, 0>;
def : ReadAdvance<ReadVFDivF_M2_E32, 0>;
def : ReadAdvance<ReadVFDivF_M2_E64, 0>;
def : ReadAdvance<ReadVFDivF_M4_E16, 0>;
def : ReadAdvance<ReadVFDivF_M4_E32, 0>;
def : ReadAdvance<ReadVFDivF_M4_E64, 0>;
def : ReadAdvance<ReadVFDivF_M8_E16, 0>;
def : ReadAdvance<ReadVFDivF_M8_E32, 0>;
def : ReadAdvance<ReadVFDivF_M8_E64, 0>;
def : ReadAdvance<ReadVFDivF_MF2_E16, 0>;
def : ReadAdvance<ReadVFDivF_MF2_E32, 0>;
def : ReadAdvance<ReadVFDivF_MF4_E16, 0>;
def : ReadAdvance<ReadVFDivF_WorstCase, 0>;
def : ReadAdvance<ReadVFDivV_M1_E16, 0>;
def : ReadAdvance<ReadVFDivV_M1_E32, 0>;
def : ReadAdvance<ReadVFDivV_M1_E64, 0>;
def : ReadAdvance<ReadVFDivV_M2_E16, 0>;
def : ReadAdvance<ReadVFDivV_M2_E32, 0>;
def : ReadAdvance<ReadVFDivV_M2_E64, 0>;
def : ReadAdvance<ReadVFDivV_M4_E16, 0>;
def : ReadAdvance<ReadVFDivV_M4_E32, 0>;
def : ReadAdvance<ReadVFDivV_M4_E64, 0>;
def : ReadAdvance<ReadVFDivV_M8_E16, 0>;
def : ReadAdvance<ReadVFDivV_M8_E32, 0>;
def : ReadAdvance<ReadVFDivV_M8_E64, 0>;
def : ReadAdvance<ReadVFDivV_MF2_E16, 0>;
def : ReadAdvance<ReadVFDivV_MF2_E32, 0>;
def : ReadAdvance<ReadVFDivV_MF4_E16, 0>;
def : ReadAdvance<ReadVFDivV_WorstCase, 0>;
def : ReadAdvance<ReadVFMergeF_M1, 0>;
def : ReadAdvance<ReadVFMergeF_M2, 0>;
def : ReadAdvance<ReadVFMergeF_M4, 0>;
def : ReadAdvance<ReadVFMergeF_M8, 0>;
def : ReadAdvance<ReadVFMergeF_MF2, 0>;
def : ReadAdvance<ReadVFMergeF_MF4, 0>;
def : ReadAdvance<ReadVFMergeF_WorstCase, 0>;
def : ReadAdvance<ReadVFMergeV_M1, 0>;
def : ReadAdvance<ReadVFMergeV_M2, 0>;
def : ReadAdvance<ReadVFMergeV_M4, 0>;
def : ReadAdvance<ReadVFMergeV_M8, 0>;
def : ReadAdvance<ReadVFMergeV_MF2, 0>;
def : ReadAdvance<ReadVFMergeV_MF4, 0>;
def : ReadAdvance<ReadVFMergeV_WorstCase, 0>;
def : ReadAdvance<ReadVFMinMaxF_M1_E16, 0>;
def : ReadAdvance<ReadVFMinMaxF_M1_E32, 0>;
def : ReadAdvance<ReadVFMinMaxF_M1_E64, 0>;
def : ReadAdvance<ReadVFMinMaxF_M2_E16, 0>;
def : ReadAdvance<ReadVFMinMaxF_M2_E32, 0>;
def : ReadAdvance<ReadVFMinMaxF_M2_E64, 0>;
def : ReadAdvance<ReadVFMinMaxF_M4_E16, 0>;
def : ReadAdvance<ReadVFMinMaxF_M4_E32, 0>;
def : ReadAdvance<ReadVFMinMaxF_M4_E64, 0>;
def : ReadAdvance<ReadVFMinMaxF_M8_E16, 0>;
def : ReadAdvance<ReadVFMinMaxF_M8_E32, 0>;
def : ReadAdvance<ReadVFMinMaxF_M8_E64, 0>;
def : ReadAdvance<ReadVFMinMaxF_MF2_E16, 0>;
def : ReadAdvance<ReadVFMinMaxF_MF2_E32, 0>;
def : ReadAdvance<ReadVFMinMaxF_MF4_E16, 0>;
def : ReadAdvance<ReadVFMinMaxF_WorstCase, 0>;
def : ReadAdvance<ReadVFMinMaxV_M1_E16, 0>;
def : ReadAdvance<ReadVFMinMaxV_M1_E32, 0>;
def : ReadAdvance<ReadVFMinMaxV_M1_E64, 0>;
def : ReadAdvance<ReadVFMinMaxV_M2_E16, 0>;
def : ReadAdvance<ReadVFMinMaxV_M2_E32, 0>;
def : ReadAdvance<ReadVFMinMaxV_M2_E64, 0>;
def : ReadAdvance<ReadVFMinMaxV_M4_E16, 0>;
def : ReadAdvance<ReadVFMinMaxV_M4_E32, 0>;
def : ReadAdvance<ReadVFMinMaxV_M4_E64, 0>;
def : ReadAdvance<ReadVFMinMaxV_M8_E16, 0>;
def : ReadAdvance<ReadVFMinMaxV_M8_E32, 0>;
def : ReadAdvance<ReadVFMinMaxV_M8_E64, 0>;
def : ReadAdvance<ReadVFMinMaxV_MF2_E16, 0>;
def : ReadAdvance<ReadVFMinMaxV_MF2_E32, 0>;
def : ReadAdvance<ReadVFMinMaxV_MF4_E16, 0>;
def : ReadAdvance<ReadVFMinMaxV_WorstCase, 0>;
def : ReadAdvance<ReadVFMovF_M1, 0>;
def : ReadAdvance<ReadVFMovF_M2, 0>;
def : ReadAdvance<ReadVFMovF_M4, 0>;
def : ReadAdvance<ReadVFMovF_M8, 0>;
def : ReadAdvance<ReadVFMovF_MF2, 0>;
def : ReadAdvance<ReadVFMovF_MF4, 0>;
def : ReadAdvance<ReadVFMovF_WorstCase, 0>;
def : ReadAdvance<ReadVFMulAddF_M1_E16, 0>;
def : ReadAdvance<ReadVFMulAddF_M1_E32, 0>;
def : ReadAdvance<ReadVFMulAddF_M1_E64, 0>;
def : ReadAdvance<ReadVFMulAddF_M2_E16, 0>;
def : ReadAdvance<ReadVFMulAddF_M2_E32, 0>;
def : ReadAdvance<ReadVFMulAddF_M2_E64, 0>;
def : ReadAdvance<ReadVFMulAddF_M4_E16, 0>;
def : ReadAdvance<ReadVFMulAddF_M4_E32, 0>;
def : ReadAdvance<ReadVFMulAddF_M4_E64, 0>;
def : ReadAdvance<ReadVFMulAddF_M8_E16, 0>;
def : ReadAdvance<ReadVFMulAddF_M8_E32, 0>;
def : ReadAdvance<ReadVFMulAddF_M8_E64, 0>;
def : ReadAdvance<ReadVFMulAddF_MF2_E16, 0>;
def : ReadAdvance<ReadVFMulAddF_MF2_E32, 0>;
def : ReadAdvance<ReadVFMulAddF_MF4_E16, 0>;
def : ReadAdvance<ReadVFMulAddF_WorstCase, 0>;
def : ReadAdvance<ReadVFMulAddV_M1_E16, 0>;
def : ReadAdvance<ReadVFMulAddV_M1_E32, 0>;
def : ReadAdvance<ReadVFMulAddV_M1_E64, 0>;
def : ReadAdvance<ReadVFMulAddV_M2_E16, 0>;
def : ReadAdvance<ReadVFMulAddV_M2_E32, 0>;
def : ReadAdvance<ReadVFMulAddV_M2_E64, 0>;
def : ReadAdvance<ReadVFMulAddV_M4_E16, 0>;
def : ReadAdvance<ReadVFMulAddV_M4_E32, 0>;
def : ReadAdvance<ReadVFMulAddV_M4_E64, 0>;
def : ReadAdvance<ReadVFMulAddV_M8_E16, 0>;
def : ReadAdvance<ReadVFMulAddV_M8_E32, 0>;
def : ReadAdvance<ReadVFMulAddV_M8_E64, 0>;
def : ReadAdvance<ReadVFMulAddV_MF2_E16, 0>;
def : ReadAdvance<ReadVFMulAddV_MF2_E32, 0>;
def : ReadAdvance<ReadVFMulAddV_MF4_E16, 0>;
def : ReadAdvance<ReadVFMulAddV_WorstCase, 0>;
def : ReadAdvance<ReadVFMulF_M1_E16, 0>;
def : ReadAdvance<ReadVFMulF_M1_E32, 0>;
def : ReadAdvance<ReadVFMulF_M1_E64, 0>;
def : ReadAdvance<ReadVFMulF_M2_E16, 0>;
def : ReadAdvance<ReadVFMulF_M2_E32, 0>;
def : ReadAdvance<ReadVFMulF_M2_E64, 0>;
def : ReadAdvance<ReadVFMulF_M4_E16, 0>;
def : ReadAdvance<ReadVFMulF_M4_E32, 0>;
def : ReadAdvance<ReadVFMulF_M4_E64, 0>;
def : ReadAdvance<ReadVFMulF_M8_E16, 0>;
def : ReadAdvance<ReadVFMulF_M8_E32, 0>;
def : ReadAdvance<ReadVFMulF_M8_E64, 0>;
def : ReadAdvance<ReadVFMulF_MF2_E16, 0>;
def : ReadAdvance<ReadVFMulF_MF2_E32, 0>;
def : ReadAdvance<ReadVFMulF_MF4_E16, 0>;
def : ReadAdvance<ReadVFMulF_WorstCase, 0>;
def : ReadAdvance<ReadVFMulV_M1_E16, 0>;
def : ReadAdvance<ReadVFMulV_M1_E32, 0>;
def : ReadAdvance<ReadVFMulV_M1_E64, 0>;
def : ReadAdvance<ReadVFMulV_M2_E16, 0>;
def : ReadAdvance<ReadVFMulV_M2_E32, 0>;
def : ReadAdvance<ReadVFMulV_M2_E64, 0>;
def : ReadAdvance<ReadVFMulV_M4_E16, 0>;
def : ReadAdvance<ReadVFMulV_M4_E32, 0>;
def : ReadAdvance<ReadVFMulV_M4_E64, 0>;
def : ReadAdvance<ReadVFMulV_M8_E16, 0>;
def : ReadAdvance<ReadVFMulV_M8_E32, 0>;
def : ReadAdvance<ReadVFMulV_M8_E64, 0>;
def : ReadAdvance<ReadVFMulV_MF2_E16, 0>;
def : ReadAdvance<ReadVFMulV_MF2_E32, 0>;
def : ReadAdvance<ReadVFMulV_MF4_E16, 0>;
def : ReadAdvance<ReadVFMulV_WorstCase, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_M1_E16, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_M1_E32, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_M2_E16, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_M2_E32, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_M4_E16, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_M4_E32, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_MF2_E16, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_MF2_E32, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_MF4_E16, 0>;
def : ReadAdvance<ReadVFNCvtFToFV_WorstCase, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_M1, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_M2, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_M4, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_MF2, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_MF4, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_MF8, 0>;
def : ReadAdvance<ReadVFNCvtFToIV_WorstCase, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_M1_E16, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_M1_E32, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_M2_E16, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_M2_E32, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_M4_E16, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_M4_E32, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_MF2_E16, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_MF2_E32, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_MF4_E16, 0>;
def : ReadAdvance<ReadVFNCvtIToFV_WorstCase, 0>;
def : ReadAdvance<ReadVFRecpV_M1_E16, 0>;
def : ReadAdvance<ReadVFRecpV_M1_E32, 0>;
def : ReadAdvance<ReadVFRecpV_M1_E64, 0>;
def : ReadAdvance<ReadVFRecpV_M2_E16, 0>;
def : ReadAdvance<ReadVFRecpV_M2_E32, 0>;
def : ReadAdvance<ReadVFRecpV_M2_E64, 0>;
def : ReadAdvance<ReadVFRecpV_M4_E16, 0>;
def : ReadAdvance<ReadVFRecpV_M4_E32, 0>;
def : ReadAdvance<ReadVFRecpV_M4_E64, 0>;
def : ReadAdvance<ReadVFRecpV_M8_E16, 0>;
def : ReadAdvance<ReadVFRecpV_M8_E32, 0>;
def : ReadAdvance<ReadVFRecpV_M8_E64, 0>;
def : ReadAdvance<ReadVFRecpV_MF2_E16, 0>;
def : ReadAdvance<ReadVFRecpV_MF2_E32, 0>;
def : ReadAdvance<ReadVFRecpV_MF4_E16, 0>;
def : ReadAdvance<ReadVFRecpV_WorstCase, 0>;
def : ReadAdvance<ReadVFRedOV, 0>;
def : ReadAdvance<ReadVFRedOV0, 0>;
def : ReadAdvance<ReadVFRedV, 0>;
def : ReadAdvance<ReadVFRedV0, 0>;
def : ReadAdvance<ReadVFSgnjF_M1_E16, 0>;
def : ReadAdvance<ReadVFSgnjF_M1_E32, 0>;
def : ReadAdvance<ReadVFSgnjF_M1_E64, 0>;
def : ReadAdvance<ReadVFSgnjF_M2_E16, 0>;
def : ReadAdvance<ReadVFSgnjF_M2_E32, 0>;
def : ReadAdvance<ReadVFSgnjF_M2_E64, 0>;
def : ReadAdvance<ReadVFSgnjF_M4_E16, 0>;
def : ReadAdvance<ReadVFSgnjF_M4_E32, 0>;
def : ReadAdvance<ReadVFSgnjF_M4_E64, 0>;
def : ReadAdvance<ReadVFSgnjF_M8_E16, 0>;
def : ReadAdvance<ReadVFSgnjF_M8_E32, 0>;
def : ReadAdvance<ReadVFSgnjF_M8_E64, 0>;
def : ReadAdvance<ReadVFSgnjF_MF2_E16, 0>;
def : ReadAdvance<ReadVFSgnjF_MF2_E32, 0>;
def : ReadAdvance<ReadVFSgnjF_MF4_E16, 0>;
def : ReadAdvance<ReadVFSgnjF_WorstCase, 0>;
def : ReadAdvance<ReadVFSgnjV_M1_E16, 0>;
def : ReadAdvance<ReadVFSgnjV_M1_E32, 0>;
def : ReadAdvance<ReadVFSgnjV_M1_E64, 0>;
def : ReadAdvance<ReadVFSgnjV_M2_E16, 0>;
def : ReadAdvance<ReadVFSgnjV_M2_E32, 0>;
def : ReadAdvance<ReadVFSgnjV_M2_E64, 0>;
def : ReadAdvance<ReadVFSgnjV_M4_E16, 0>;
def : ReadAdvance<ReadVFSgnjV_M4_E32, 0>;
def : ReadAdvance<ReadVFSgnjV_M4_E64, 0>;
def : ReadAdvance<ReadVFSgnjV_M8_E16, 0>;
def : ReadAdvance<ReadVFSgnjV_M8_E32, 0>;
def : ReadAdvance<ReadVFSgnjV_M8_E64, 0>;
def : ReadAdvance<ReadVFSgnjV_MF2_E16, 0>;
def : ReadAdvance<ReadVFSgnjV_MF2_E32, 0>;
def : ReadAdvance<ReadVFSgnjV_MF4_E16, 0>;
def : ReadAdvance<ReadVFSgnjV_WorstCase, 0>;
def : ReadAdvance<ReadVFSlideF_M1, 0>;
def : ReadAdvance<ReadVFSlideF_M2, 0>;
def : ReadAdvance<ReadVFSlideF_M4, 0>;
def : ReadAdvance<ReadVFSlideF_M8, 0>;
def : ReadAdvance<ReadVFSlideF_MF2, 0>;
def : ReadAdvance<ReadVFSlideF_MF4, 0>;
def : ReadAdvance<ReadVFSlideF_WorstCase, 0>;
def : ReadAdvance<ReadVFSlideV_M1, 0>;
def : ReadAdvance<ReadVFSlideV_M2, 0>;
def : ReadAdvance<ReadVFSlideV_M4, 0>;
def : ReadAdvance<ReadVFSlideV_M8, 0>;
def : ReadAdvance<ReadVFSlideV_MF2, 0>;
def : ReadAdvance<ReadVFSlideV_MF4, 0>;
def : ReadAdvance<ReadVFSlideV_WorstCase, 0>;
def : ReadAdvance<ReadVFSqrtV_M1_E16, 0>;
def : ReadAdvance<ReadVFSqrtV_M1_E32, 0>;
def : ReadAdvance<ReadVFSqrtV_M1_E64, 0>;
def : ReadAdvance<ReadVFSqrtV_M2_E16, 0>;
def : ReadAdvance<ReadVFSqrtV_M2_E32, 0>;
def : ReadAdvance<ReadVFSqrtV_M2_E64, 0>;
def : ReadAdvance<ReadVFSqrtV_M4_E16, 0>;
def : ReadAdvance<ReadVFSqrtV_M4_E32, 0>;
def : ReadAdvance<ReadVFSqrtV_M4_E64, 0>;
def : ReadAdvance<ReadVFSqrtV_M8_E16, 0>;
def : ReadAdvance<ReadVFSqrtV_M8_E32, 0>;
def : ReadAdvance<ReadVFSqrtV_M8_E64, 0>;
def : ReadAdvance<ReadVFSqrtV_MF2_E16, 0>;
def : ReadAdvance<ReadVFSqrtV_MF2_E32, 0>;
def : ReadAdvance<ReadVFSqrtV_MF4_E16, 0>;
def : ReadAdvance<ReadVFSqrtV_WorstCase, 0>;
def : ReadAdvance<ReadVFWALUF_M1_E16, 0>;
def : ReadAdvance<ReadVFWALUF_M1_E32, 0>;
def : ReadAdvance<ReadVFWALUF_M2_E16, 0>;
def : ReadAdvance<ReadVFWALUF_M2_E32, 0>;
def : ReadAdvance<ReadVFWALUF_M4_E16, 0>;
def : ReadAdvance<ReadVFWALUF_M4_E32, 0>;
def : ReadAdvance<ReadVFWALUF_MF2_E16, 0>;
def : ReadAdvance<ReadVFWALUF_MF2_E32, 0>;
def : ReadAdvance<ReadVFWALUF_MF4_E16, 0>;
def : ReadAdvance<ReadVFWALUF_WorstCase, 0>;
def : ReadAdvance<ReadVFWALUV_M1_E16, 0>;
def : ReadAdvance<ReadVFWALUV_M1_E32, 0>;
def : ReadAdvance<ReadVFWALUV_M2_E16, 0>;
def : ReadAdvance<ReadVFWALUV_M2_E32, 0>;
def : ReadAdvance<ReadVFWALUV_M4_E16, 0>;
def : ReadAdvance<ReadVFWALUV_M4_E32, 0>;
def : ReadAdvance<ReadVFWALUV_MF2_E16, 0>;
def : ReadAdvance<ReadVFWALUV_MF2_E32, 0>;
def : ReadAdvance<ReadVFWALUV_MF4_E16, 0>;
def : ReadAdvance<ReadVFWALUV_WorstCase, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_M1_E16, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_M1_E32, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_M2_E16, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_M2_E32, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_M4_E16, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_M4_E32, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_MF2_E16, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_MF2_E32, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_MF4_E16, 0>;
def : ReadAdvance<ReadVFWCvtFToFV_WorstCase, 0>;
def : ReadAdvance<ReadVFWCvtFToIV_M1, 0>;
def : ReadAdvance<ReadVFWCvtFToIV_M2, 0>;
def : ReadAdvance<ReadVFWCvtFToIV_M4, 0>;
def : ReadAdvance<ReadVFWCvtFToIV_MF2, 0>;
def : ReadAdvance<ReadVFWCvtFToIV_MF4, 0>;
def : ReadAdvance<ReadVFWCvtFToIV_WorstCase, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M1_E16, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M1_E32, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M1_E8, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M2_E16, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M2_E32, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M2_E8, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M4_E16, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M4_E32, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_M4_E8, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_MF2_E16, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_MF2_E32, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_MF2_E8, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_MF4_E16, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_MF4_E8, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_MF8_E8, 0>;
def : ReadAdvance<ReadVFWCvtIToFV_WorstCase, 0>;
def : ReadAdvance<ReadVFWMulAddF_M1_E16, 0>;
def : ReadAdvance<ReadVFWMulAddF_M1_E32, 0>;
def : ReadAdvance<ReadVFWMulAddF_M2_E16, 0>;
def : ReadAdvance<ReadVFWMulAddF_M2_E32, 0>;
def : ReadAdvance<ReadVFWMulAddF_M4_E16, 0>;
def : ReadAdvance<ReadVFWMulAddF_M4_E32, 0>;
def : ReadAdvance<ReadVFWMulAddF_MF2_E16, 0>;
def : ReadAdvance<ReadVFWMulAddF_MF2_E32, 0>;
def : ReadAdvance<ReadVFWMulAddF_MF4_E16, 0>;
def : ReadAdvance<ReadVFWMulAddF_WorstCase, 0>;
def : ReadAdvance<ReadVFWMulAddV_M1_E16, 0>;
def : ReadAdvance<ReadVFWMulAddV_M1_E32, 0>;
def : ReadAdvance<ReadVFWMulAddV_M2_E16, 0>;
def : ReadAdvance<ReadVFWMulAddV_M2_E32, 0>;
def : ReadAdvance<ReadVFWMulAddV_M4_E16, 0>;
def : ReadAdvance<ReadVFWMulAddV_M4_E32, 0>;
def : ReadAdvance<ReadVFWMulAddV_MF2_E16, 0>;
def : ReadAdvance<ReadVFWMulAddV_MF2_E32, 0>;
def : ReadAdvance<ReadVFWMulAddV_MF4_E16, 0>;
def : ReadAdvance<ReadVFWMulAddV_WorstCase, 0>;
def : ReadAdvance<ReadVFWMulF_M1_E16, 0>;
def : ReadAdvance<ReadVFWMulF_M1_E32, 0>;
def : ReadAdvance<ReadVFWMulF_M2_E16, 0>;
def : ReadAdvance<ReadVFWMulF_M2_E32, 0>;
def : ReadAdvance<ReadVFWMulF_M4_E16, 0>;
def : ReadAdvance<ReadVFWMulF_M4_E32, 0>;
def : ReadAdvance<ReadVFWMulF_MF2_E16, 0>;
def : ReadAdvance<ReadVFWMulF_MF2_E32, 0>;
def : ReadAdvance<ReadVFWMulF_MF4_E16, 0>;
def : ReadAdvance<ReadVFWMulF_WorstCase, 0>;
def : ReadAdvance<ReadVFWMulV_M1_E16, 0>;
def : ReadAdvance<ReadVFWMulV_M1_E32, 0>;
def : ReadAdvance<ReadVFWMulV_M2_E16, 0>;
def : ReadAdvance<ReadVFWMulV_M2_E32, 0>;
def : ReadAdvance<ReadVFWMulV_M4_E16, 0>;
def : ReadAdvance<ReadVFWMulV_M4_E32, 0>;
def : ReadAdvance<ReadVFWMulV_MF2_E16, 0>;
def : ReadAdvance<ReadVFWMulV_MF2_E32, 0>;
def : ReadAdvance<ReadVFWMulV_MF4_E16, 0>;
def : ReadAdvance<ReadVFWMulV_WorstCase, 0>;
def : ReadAdvance<ReadVFWRedOV, 0>;
def : ReadAdvance<ReadVFWRedOV0, 0>;
def : ReadAdvance<ReadVFWRedV, 0>;
def : ReadAdvance<ReadVFWRedV0, 0>;
def : ReadAdvance<ReadVGHSHV_M1, 0>;
def : ReadAdvance<ReadVGHSHV_M2, 0>;
def : ReadAdvance<ReadVGHSHV_M4, 0>;
def : ReadAdvance<ReadVGHSHV_M8, 0>;
def : ReadAdvance<ReadVGHSHV_MF2, 0>;
def : ReadAdvance<ReadVGHSHV_WorstCase, 0>;
def : ReadAdvance<ReadVGMULV_M1, 0>;
def : ReadAdvance<ReadVGMULV_M2, 0>;
def : ReadAdvance<ReadVGMULV_M4, 0>;
def : ReadAdvance<ReadVGMULV_M8, 0>;
def : ReadAdvance<ReadVGMULV_MF2, 0>;
def : ReadAdvance<ReadVGMULV_WorstCase, 0>;
def : ReadAdvance<ReadVIALUV_M1, 0>;
def : ReadAdvance<ReadVIALUV_M2, 0>;
def : ReadAdvance<ReadVIALUV_M4, 0>;
def : ReadAdvance<ReadVIALUV_M8, 0>;
def : ReadAdvance<ReadVIALUV_MF2, 0>;
def : ReadAdvance<ReadVIALUV_MF4, 0>;
def : ReadAdvance<ReadVIALUV_MF8, 0>;
def : ReadAdvance<ReadVIALUV_WorstCase, 0>;
def : ReadAdvance<ReadVIALUX_M1, 0>;
def : ReadAdvance<ReadVIALUX_M2, 0>;
def : ReadAdvance<ReadVIALUX_M4, 0>;
def : ReadAdvance<ReadVIALUX_M8, 0>;
def : ReadAdvance<ReadVIALUX_MF2, 0>;
def : ReadAdvance<ReadVIALUX_MF4, 0>;
def : ReadAdvance<ReadVIALUX_MF8, 0>;
def : ReadAdvance<ReadVIALUX_WorstCase, 0>;
def : ReadAdvance<ReadVICALUV_M1, 0>;
def : ReadAdvance<ReadVICALUV_M2, 0>;
def : ReadAdvance<ReadVICALUV_M4, 0>;
def : ReadAdvance<ReadVICALUV_M8, 0>;
def : ReadAdvance<ReadVICALUV_MF2, 0>;
def : ReadAdvance<ReadVICALUV_MF4, 0>;
def : ReadAdvance<ReadVICALUV_MF8, 0>;
def : ReadAdvance<ReadVICALUV_WorstCase, 0>;
def : ReadAdvance<ReadVICALUX_M1, 0>;
def : ReadAdvance<ReadVICALUX_M2, 0>;
def : ReadAdvance<ReadVICALUX_M4, 0>;
def : ReadAdvance<ReadVICALUX_M8, 0>;
def : ReadAdvance<ReadVICALUX_MF2, 0>;
def : ReadAdvance<ReadVICALUX_MF4, 0>;
def : ReadAdvance<ReadVICALUX_MF8, 0>;
def : ReadAdvance<ReadVICALUX_WorstCase, 0>;
def : ReadAdvance<ReadVICmpV_M1, 0>;
def : ReadAdvance<ReadVICmpV_M2, 0>;
def : ReadAdvance<ReadVICmpV_M4, 0>;
def : ReadAdvance<ReadVICmpV_M8, 0>;
def : ReadAdvance<ReadVICmpV_MF2, 0>;
def : ReadAdvance<ReadVICmpV_MF4, 0>;
def : ReadAdvance<ReadVICmpV_MF8, 0>;
def : ReadAdvance<ReadVICmpV_WorstCase, 0>;
def : ReadAdvance<ReadVICmpX_M1, 0>;
def : ReadAdvance<ReadVICmpX_M2, 0>;
def : ReadAdvance<ReadVICmpX_M4, 0>;
def : ReadAdvance<ReadVICmpX_M8, 0>;
def : ReadAdvance<ReadVICmpX_MF2, 0>;
def : ReadAdvance<ReadVICmpX_MF4, 0>;
def : ReadAdvance<ReadVICmpX_MF8, 0>;
def : ReadAdvance<ReadVICmpX_WorstCase, 0>;
def : ReadAdvance<ReadVIDivV_M1_E16, 0>;
def : ReadAdvance<ReadVIDivV_M1_E32, 0>;
def : ReadAdvance<ReadVIDivV_M1_E64, 0>;
def : ReadAdvance<ReadVIDivV_M1_E8, 0>;
def : ReadAdvance<ReadVIDivV_M2_E16, 0>;
def : ReadAdvance<ReadVIDivV_M2_E32, 0>;
def : ReadAdvance<ReadVIDivV_M2_E64, 0>;
def : ReadAdvance<ReadVIDivV_M2_E8, 0>;
def : ReadAdvance<ReadVIDivV_M4_E16, 0>;
def : ReadAdvance<ReadVIDivV_M4_E32, 0>;
def : ReadAdvance<ReadVIDivV_M4_E64, 0>;
def : ReadAdvance<ReadVIDivV_M4_E8, 0>;
def : ReadAdvance<ReadVIDivV_M8_E16, 0>;
def : ReadAdvance<ReadVIDivV_M8_E32, 0>;
def : ReadAdvance<ReadVIDivV_M8_E64, 0>;
def : ReadAdvance<ReadVIDivV_M8_E8, 0>;
def : ReadAdvance<ReadVIDivV_MF2_E16, 0>;
def : ReadAdvance<ReadVIDivV_MF2_E32, 0>;
def : ReadAdvance<ReadVIDivV_MF2_E8, 0>;
def : ReadAdvance<ReadVIDivV_MF4_E16, 0>;
def : ReadAdvance<ReadVIDivV_MF4_E8, 0>;
def : ReadAdvance<ReadVIDivV_MF8_E8, 0>;
def : ReadAdvance<ReadVIDivV_WorstCase, 0>;
def : ReadAdvance<ReadVIDivX_M1_E16, 0>;
def : ReadAdvance<ReadVIDivX_M1_E32, 0>;
def : ReadAdvance<ReadVIDivX_M1_E64, 0>;
def : ReadAdvance<ReadVIDivX_M1_E8, 0>;
def : ReadAdvance<ReadVIDivX_M2_E16, 0>;
def : ReadAdvance<ReadVIDivX_M2_E32, 0>;
def : ReadAdvance<ReadVIDivX_M2_E64, 0>;
def : ReadAdvance<ReadVIDivX_M2_E8, 0>;
def : ReadAdvance<ReadVIDivX_M4_E16, 0>;
def : ReadAdvance<ReadVIDivX_M4_E32, 0>;
def : ReadAdvance<ReadVIDivX_M4_E64, 0>;
def : ReadAdvance<ReadVIDivX_M4_E8, 0>;
def : ReadAdvance<ReadVIDivX_M8_E16, 0>;
def : ReadAdvance<ReadVIDivX_M8_E32, 0>;
def : ReadAdvance<ReadVIDivX_M8_E64, 0>;
def : ReadAdvance<ReadVIDivX_M8_E8, 0>;
def : ReadAdvance<ReadVIDivX_MF2_E16, 0>;
def : ReadAdvance<ReadVIDivX_MF2_E32, 0>;
def : ReadAdvance<ReadVIDivX_MF2_E8, 0>;
def : ReadAdvance<ReadVIDivX_MF4_E16, 0>;
def : ReadAdvance<ReadVIDivX_MF4_E8, 0>;
def : ReadAdvance<ReadVIDivX_MF8_E8, 0>;
def : ReadAdvance<ReadVIDivX_WorstCase, 0>;
def : ReadAdvance<ReadVIMergeV_M1, 0>;
def : ReadAdvance<ReadVIMergeV_M2, 0>;
def : ReadAdvance<ReadVIMergeV_M4, 0>;
def : ReadAdvance<ReadVIMergeV_M8, 0>;
def : ReadAdvance<ReadVIMergeV_MF2, 0>;
def : ReadAdvance<ReadVIMergeV_MF4, 0>;
def : ReadAdvance<ReadVIMergeV_MF8, 0>;
def : ReadAdvance<ReadVIMergeV_WorstCase, 0>;
def : ReadAdvance<ReadVIMergeX_M1, 0>;
def : ReadAdvance<ReadVIMergeX_M2, 0>;
def : ReadAdvance<ReadVIMergeX_M4, 0>;
def : ReadAdvance<ReadVIMergeX_M8, 0>;
def : ReadAdvance<ReadVIMergeX_MF2, 0>;
def : ReadAdvance<ReadVIMergeX_MF4, 0>;
def : ReadAdvance<ReadVIMergeX_MF8, 0>;
def : ReadAdvance<ReadVIMergeX_WorstCase, 0>;
def : ReadAdvance<ReadVIMinMaxV_M1, 0>;
def : ReadAdvance<ReadVIMinMaxV_M2, 0>;
def : ReadAdvance<ReadVIMinMaxV_M4, 0>;
def : ReadAdvance<ReadVIMinMaxV_M8, 0>;
def : ReadAdvance<ReadVIMinMaxV_MF2, 0>;
def : ReadAdvance<ReadVIMinMaxV_MF4, 0>;
def : ReadAdvance<ReadVIMinMaxV_MF8, 0>;
def : ReadAdvance<ReadVIMinMaxV_WorstCase, 0>;
def : ReadAdvance<ReadVIMinMaxX_M1, 0>;
def : ReadAdvance<ReadVIMinMaxX_M2, 0>;
def : ReadAdvance<ReadVIMinMaxX_M4, 0>;
def : ReadAdvance<ReadVIMinMaxX_M8, 0>;
def : ReadAdvance<ReadVIMinMaxX_MF2, 0>;
def : ReadAdvance<ReadVIMinMaxX_MF4, 0>;
def : ReadAdvance<ReadVIMinMaxX_MF8, 0>;
def : ReadAdvance<ReadVIMinMaxX_WorstCase, 0>;
def : ReadAdvance<ReadVIMovV_M1, 0>;
def : ReadAdvance<ReadVIMovV_M2, 0>;
def : ReadAdvance<ReadVIMovV_M4, 0>;
def : ReadAdvance<ReadVIMovV_M8, 0>;
def : ReadAdvance<ReadVIMovV_MF2, 0>;
def : ReadAdvance<ReadVIMovV_MF4, 0>;
def : ReadAdvance<ReadVIMovV_MF8, 0>;
def : ReadAdvance<ReadVIMovV_WorstCase, 0>;
def : ReadAdvance<ReadVIMovX_M1, 0>;
def : ReadAdvance<ReadVIMovX_M2, 0>;
def : ReadAdvance<ReadVIMovX_M4, 0>;
def : ReadAdvance<ReadVIMovX_M8, 0>;
def : ReadAdvance<ReadVIMovX_MF2, 0>;
def : ReadAdvance<ReadVIMovX_MF4, 0>;
def : ReadAdvance<ReadVIMovX_MF8, 0>;
def : ReadAdvance<ReadVIMovX_WorstCase, 0>;
def : ReadAdvance<ReadVIMulAddV_M1, 0>;
def : ReadAdvance<ReadVIMulAddV_M2, 0>;
def : ReadAdvance<ReadVIMulAddV_M4, 0>;
def : ReadAdvance<ReadVIMulAddV_M8, 0>;
def : ReadAdvance<ReadVIMulAddV_MF2, 0>;
def : ReadAdvance<ReadVIMulAddV_MF4, 0>;
def : ReadAdvance<ReadVIMulAddV_MF8, 0>;
def : ReadAdvance<ReadVIMulAddV_WorstCase, 0>;
def : ReadAdvance<ReadVIMulAddX_M1, 0>;
def : ReadAdvance<ReadVIMulAddX_M2, 0>;
def : ReadAdvance<ReadVIMulAddX_M4, 0>;
def : ReadAdvance<ReadVIMulAddX_M8, 0>;
def : ReadAdvance<ReadVIMulAddX_MF2, 0>;
def : ReadAdvance<ReadVIMulAddX_MF4, 0>;
def : ReadAdvance<ReadVIMulAddX_MF8, 0>;
def : ReadAdvance<ReadVIMulAddX_WorstCase, 0>;
def : ReadAdvance<ReadVIMulV_M1, 0>;
def : ReadAdvance<ReadVIMulV_M2, 0>;
def : ReadAdvance<ReadVIMulV_M4, 0>;
def : ReadAdvance<ReadVIMulV_M8, 0>;
def : ReadAdvance<ReadVIMulV_MF2, 0>;
def : ReadAdvance<ReadVIMulV_MF4, 0>;
def : ReadAdvance<ReadVIMulV_MF8, 0>;
def : ReadAdvance<ReadVIMulV_WorstCase, 0>;
def : ReadAdvance<ReadVIMulX_M1, 0>;
def : ReadAdvance<ReadVIMulX_M2, 0>;
def : ReadAdvance<ReadVIMulX_M4, 0>;
def : ReadAdvance<ReadVIMulX_M8, 0>;
def : ReadAdvance<ReadVIMulX_MF2, 0>;
def : ReadAdvance<ReadVIMulX_MF4, 0>;
def : ReadAdvance<ReadVIMulX_MF8, 0>;
def : ReadAdvance<ReadVIMulX_WorstCase, 0>;
def : ReadAdvance<ReadVIRedV, 0>;
def : ReadAdvance<ReadVIRedV0, 0>;
def : ReadAdvance<ReadVISlideV_M1, 0>;
def : ReadAdvance<ReadVISlideV_M2, 0>;
def : ReadAdvance<ReadVISlideV_M4, 0>;
def : ReadAdvance<ReadVISlideV_M8, 0>;
def : ReadAdvance<ReadVISlideV_MF2, 0>;
def : ReadAdvance<ReadVISlideV_MF4, 0>;
def : ReadAdvance<ReadVISlideV_MF8, 0>;
def : ReadAdvance<ReadVISlideV_WorstCase, 0>;
def : ReadAdvance<ReadVISlideX_M1, 0>;
def : ReadAdvance<ReadVISlideX_M2, 0>;
def : ReadAdvance<ReadVISlideX_M4, 0>;
def : ReadAdvance<ReadVISlideX_M8, 0>;
def : ReadAdvance<ReadVISlideX_MF2, 0>;
def : ReadAdvance<ReadVISlideX_MF4, 0>;
def : ReadAdvance<ReadVISlideX_MF8, 0>;
def : ReadAdvance<ReadVISlideX_WorstCase, 0>;
def : ReadAdvance<ReadVIWALUV_M1, 0>;
def : ReadAdvance<ReadVIWALUV_M2, 0>;
def : ReadAdvance<ReadVIWALUV_M4, 0>;
def : ReadAdvance<ReadVIWALUV_MF2, 0>;
def : ReadAdvance<ReadVIWALUV_MF4, 0>;
def : ReadAdvance<ReadVIWALUV_MF8, 0>;
def : ReadAdvance<ReadVIWALUV_WorstCase, 0>;
def : ReadAdvance<ReadVIWALUX_M1, 0>;
def : ReadAdvance<ReadVIWALUX_M2, 0>;
def : ReadAdvance<ReadVIWALUX_M4, 0>;
def : ReadAdvance<ReadVIWALUX_MF2, 0>;
def : ReadAdvance<ReadVIWALUX_MF4, 0>;
def : ReadAdvance<ReadVIWALUX_MF8, 0>;
def : ReadAdvance<ReadVIWALUX_WorstCase, 0>;
def : ReadAdvance<ReadVIWMulAddV_M1, 0>;
def : ReadAdvance<ReadVIWMulAddV_M2, 0>;
def : ReadAdvance<ReadVIWMulAddV_M4, 0>;
def : ReadAdvance<ReadVIWMulAddV_MF2, 0>;
def : ReadAdvance<ReadVIWMulAddV_MF4, 0>;
def : ReadAdvance<ReadVIWMulAddV_MF8, 0>;
def : ReadAdvance<ReadVIWMulAddV_WorstCase, 0>;
def : ReadAdvance<ReadVIWMulAddX_M1, 0>;
def : ReadAdvance<ReadVIWMulAddX_M2, 0>;
def : ReadAdvance<ReadVIWMulAddX_M4, 0>;
def : ReadAdvance<ReadVIWMulAddX_MF2, 0>;
def : ReadAdvance<ReadVIWMulAddX_MF4, 0>;
def : ReadAdvance<ReadVIWMulAddX_MF8, 0>;
def : ReadAdvance<ReadVIWMulAddX_WorstCase, 0>;
def : ReadAdvance<ReadVIWMulV_M1, 0>;
def : ReadAdvance<ReadVIWMulV_M2, 0>;
def : ReadAdvance<ReadVIWMulV_M4, 0>;
def : ReadAdvance<ReadVIWMulV_MF2, 0>;
def : ReadAdvance<ReadVIWMulV_MF4, 0>;
def : ReadAdvance<ReadVIWMulV_MF8, 0>;
def : ReadAdvance<ReadVIWMulV_WorstCase, 0>;
def : ReadAdvance<ReadVIWMulX_M1, 0>;
def : ReadAdvance<ReadVIWMulX_M2, 0>;
def : ReadAdvance<ReadVIWMulX_M4, 0>;
def : ReadAdvance<ReadVIWMulX_MF2, 0>;
def : ReadAdvance<ReadVIWMulX_MF4, 0>;
def : ReadAdvance<ReadVIWMulX_MF8, 0>;
def : ReadAdvance<ReadVIWMulX_WorstCase, 0>;
def : ReadAdvance<ReadVIWRedV, 0>;
def : ReadAdvance<ReadVIWRedV0, 0>;
def : ReadAdvance<ReadVIotaV_M1, 0>;
def : ReadAdvance<ReadVIotaV_M2, 0>;
def : ReadAdvance<ReadVIotaV_M4, 0>;
def : ReadAdvance<ReadVIotaV_M8, 0>;
def : ReadAdvance<ReadVIotaV_MF2, 0>;
def : ReadAdvance<ReadVIotaV_MF4, 0>;
def : ReadAdvance<ReadVIotaV_MF8, 0>;
def : ReadAdvance<ReadVIotaV_WorstCase, 0>;
def : ReadAdvance<ReadVLDOXV_M1, 0>;
def : ReadAdvance<ReadVLDOXV_M2, 0>;
def : ReadAdvance<ReadVLDOXV_M4, 0>;
def : ReadAdvance<ReadVLDOXV_M8, 0>;
def : ReadAdvance<ReadVLDOXV_MF2, 0>;
def : ReadAdvance<ReadVLDOXV_MF4, 0>;
def : ReadAdvance<ReadVLDOXV_MF8, 0>;
def : ReadAdvance<ReadVLDOXV_WorstCase, 0>;
def : ReadAdvance<ReadVLDSX, 0>;
def : ReadAdvance<ReadVLDUXV_M1, 0>;
def : ReadAdvance<ReadVLDUXV_M2, 0>;
def : ReadAdvance<ReadVLDUXV_M4, 0>;
def : ReadAdvance<ReadVLDUXV_M8, 0>;
def : ReadAdvance<ReadVLDUXV_MF2, 0>;
def : ReadAdvance<ReadVLDUXV_MF4, 0>;
def : ReadAdvance<ReadVLDUXV_MF8, 0>;
def : ReadAdvance<ReadVLDUXV_WorstCase, 0>;
def : ReadAdvance<ReadVLDX, 0>;
def : ReadAdvance<ReadVMALUV_M1, 0>;
def : ReadAdvance<ReadVMALUV_M2, 0>;
def : ReadAdvance<ReadVMALUV_M4, 0>;
def : ReadAdvance<ReadVMALUV_M8, 0>;
def : ReadAdvance<ReadVMALUV_MF2, 0>;
def : ReadAdvance<ReadVMALUV_MF4, 0>;
def : ReadAdvance<ReadVMALUV_MF8, 0>;
def : ReadAdvance<ReadVMALUV_WorstCase, 0>;
def : ReadAdvance<ReadVMFFSV_M1, 0>;
def : ReadAdvance<ReadVMFFSV_M2, 0>;
def : ReadAdvance<ReadVMFFSV_M4, 0>;
def : ReadAdvance<ReadVMFFSV_M8, 0>;
def : ReadAdvance<ReadVMFFSV_MF2, 0>;
def : ReadAdvance<ReadVMFFSV_MF4, 0>;
def : ReadAdvance<ReadVMFFSV_MF8, 0>;
def : ReadAdvance<ReadVMFFSV_WorstCase, 0>;
def : ReadAdvance<ReadVMPopV_M1, 0>;
def : ReadAdvance<ReadVMPopV_M2, 0>;
def : ReadAdvance<ReadVMPopV_M4, 0>;
def : ReadAdvance<ReadVMPopV_M8, 0>;
def : ReadAdvance<ReadVMPopV_MF2, 0>;
def : ReadAdvance<ReadVMPopV_MF4, 0>;
def : ReadAdvance<ReadVMPopV_MF8, 0>;
def : ReadAdvance<ReadVMPopV_WorstCase, 0>;
def : ReadAdvance<ReadVMSFSV_M1, 0>;
def : ReadAdvance<ReadVMSFSV_M2, 0>;
def : ReadAdvance<ReadVMSFSV_M4, 0>;
def : ReadAdvance<ReadVMSFSV_M8, 0>;
def : ReadAdvance<ReadVMSFSV_MF2, 0>;
def : ReadAdvance<ReadVMSFSV_MF4, 0>;
def : ReadAdvance<ReadVMSFSV_MF8, 0>;
def : ReadAdvance<ReadVMSFSV_WorstCase, 0>;
def : ReadAdvance<ReadVMask, 0>;
def : ReadAdvance<ReadVMov1V, 0>;
def : ReadAdvance<ReadVMov2V, 0>;
def : ReadAdvance<ReadVMov4V, 0>;
def : ReadAdvance<ReadVMov8V, 0>;
def : ReadAdvance<ReadVMovFS, 0>;
def : ReadAdvance<ReadVMovSF_F, 0>;
def : ReadAdvance<ReadVMovSF_V, 0>;
def : ReadAdvance<ReadVMovSX_V, 0>;
def : ReadAdvance<ReadVMovSX_X, 0>;
def : ReadAdvance<ReadVMovXS, 0>;
def : ReadAdvance<ReadVNClipV_M1, 0>;
def : ReadAdvance<ReadVNClipV_M2, 0>;
def : ReadAdvance<ReadVNClipV_M4, 0>;
def : ReadAdvance<ReadVNClipV_MF2, 0>;
def : ReadAdvance<ReadVNClipV_MF4, 0>;
def : ReadAdvance<ReadVNClipV_MF8, 0>;
def : ReadAdvance<ReadVNClipV_WorstCase, 0>;
def : ReadAdvance<ReadVNClipX_M1, 0>;
def : ReadAdvance<ReadVNClipX_M2, 0>;
def : ReadAdvance<ReadVNClipX_M4, 0>;
def : ReadAdvance<ReadVNClipX_MF2, 0>;
def : ReadAdvance<ReadVNClipX_MF4, 0>;
def : ReadAdvance<ReadVNClipX_MF8, 0>;
def : ReadAdvance<ReadVNClipX_WorstCase, 0>;
def : ReadAdvance<ReadVNShiftV_M1, 0>;
def : ReadAdvance<ReadVNShiftV_M2, 0>;
def : ReadAdvance<ReadVNShiftV_M4, 0>;
def : ReadAdvance<ReadVNShiftV_MF2, 0>;
def : ReadAdvance<ReadVNShiftV_MF4, 0>;
def : ReadAdvance<ReadVNShiftV_MF8, 0>;
def : ReadAdvance<ReadVNShiftV_WorstCase, 0>;
def : ReadAdvance<ReadVNShiftX_M1, 0>;
def : ReadAdvance<ReadVNShiftX_M2, 0>;
def : ReadAdvance<ReadVNShiftX_M4, 0>;
def : ReadAdvance<ReadVNShiftX_MF2, 0>;
def : ReadAdvance<ReadVNShiftX_MF4, 0>;
def : ReadAdvance<ReadVNShiftX_MF8, 0>;
def : ReadAdvance<ReadVNShiftX_WorstCase, 0>;
def : ReadAdvance<ReadVPassthru_M1, 0>;
def : ReadAdvance<ReadVPassthru_M1_E16, 0>;
def : ReadAdvance<ReadVPassthru_M1_E32, 0>;
def : ReadAdvance<ReadVPassthru_M1_E64, 0>;
def : ReadAdvance<ReadVPassthru_M1_E8, 0>;
def : ReadAdvance<ReadVPassthru_M2, 0>;
def : ReadAdvance<ReadVPassthru_M2_E16, 0>;
def : ReadAdvance<ReadVPassthru_M2_E32, 0>;
def : ReadAdvance<ReadVPassthru_M2_E64, 0>;
def : ReadAdvance<ReadVPassthru_M2_E8, 0>;
def : ReadAdvance<ReadVPassthru_M4, 0>;
def : ReadAdvance<ReadVPassthru_M4_E16, 0>;
def : ReadAdvance<ReadVPassthru_M4_E32, 0>;
def : ReadAdvance<ReadVPassthru_M4_E64, 0>;
def : ReadAdvance<ReadVPassthru_M4_E8, 0>;
def : ReadAdvance<ReadVPassthru_M8, 0>;
def : ReadAdvance<ReadVPassthru_M8_E16, 0>;
def : ReadAdvance<ReadVPassthru_M8_E32, 0>;
def : ReadAdvance<ReadVPassthru_M8_E64, 0>;
def : ReadAdvance<ReadVPassthru_M8_E8, 0>;
def : ReadAdvance<ReadVPassthru_MF2, 0>;
def : ReadAdvance<ReadVPassthru_MF2_E16, 0>;
def : ReadAdvance<ReadVPassthru_MF2_E32, 0>;
def : ReadAdvance<ReadVPassthru_MF2_E8, 0>;
def : ReadAdvance<ReadVPassthru_MF4, 0>;
def : ReadAdvance<ReadVPassthru_MF4_E16, 0>;
def : ReadAdvance<ReadVPassthru_MF4_E8, 0>;
def : ReadAdvance<ReadVPassthru_MF8, 0>;
def : ReadAdvance<ReadVPassthru_MF8_E8, 0>;
def : ReadAdvance<ReadVPassthru_WorstCase, 0>;
def : ReadAdvance<ReadVREV8V_M1, 0>;
def : ReadAdvance<ReadVREV8V_M2, 0>;
def : ReadAdvance<ReadVREV8V_M4, 0>;
def : ReadAdvance<ReadVREV8V_M8, 0>;
def : ReadAdvance<ReadVREV8V_MF2, 0>;
def : ReadAdvance<ReadVREV8V_MF4, 0>;
def : ReadAdvance<ReadVREV8V_MF8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M1_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M1_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M1_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M1_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M2_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M2_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M2_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M2_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M4_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M4_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M4_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M4_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M8_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M8_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M8_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_M8_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_MF2_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_MF2_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_MF2_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_MF4_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_MF4_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_MF8_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_data_WorstCase, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M1_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M1_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M1_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M1_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M2_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M2_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M2_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M2_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M4_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M4_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M4_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M4_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M8_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M8_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M8_E64, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_M8_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_MF2_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_MF2_E32, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_MF2_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_MF4_E16, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_MF4_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_MF8_E8, 0>;
def : ReadAdvance<ReadVRGatherEI16VV_index_WorstCase, 0>;
def : ReadAdvance<ReadVRGatherVI_data_M1, 0>;
def : ReadAdvance<ReadVRGatherVI_data_M2, 0>;
def : ReadAdvance<ReadVRGatherVI_data_M4, 0>;
def : ReadAdvance<ReadVRGatherVI_data_M8, 0>;
def : ReadAdvance<ReadVRGatherVI_data_MF2, 0>;
def : ReadAdvance<ReadVRGatherVI_data_MF4, 0>;
def : ReadAdvance<ReadVRGatherVI_data_MF8, 0>;
def : ReadAdvance<ReadVRGatherVI_data_WorstCase, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M1_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M1_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M1_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M1_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M2_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M2_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M2_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M2_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M4_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M4_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M4_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M4_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M8_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M8_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M8_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_data_M8_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_MF2_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_data_MF2_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_data_MF2_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_MF4_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_data_MF4_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_MF8_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_data_WorstCase, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M1_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M1_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M1_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M1_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M2_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M2_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M2_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M2_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M4_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M4_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M4_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M4_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M8_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M8_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M8_E64, 0>;
def : ReadAdvance<ReadVRGatherVV_index_M8_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_MF2_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_index_MF2_E32, 0>;
def : ReadAdvance<ReadVRGatherVV_index_MF2_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_MF4_E16, 0>;
def : ReadAdvance<ReadVRGatherVV_index_MF4_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_MF8_E8, 0>;
def : ReadAdvance<ReadVRGatherVV_index_WorstCase, 0>;
def : ReadAdvance<ReadVRGatherVX_data_M1, 0>;
def : ReadAdvance<ReadVRGatherVX_data_M2, 0>;
def : ReadAdvance<ReadVRGatherVX_data_M4, 0>;
def : ReadAdvance<ReadVRGatherVX_data_M8, 0>;
def : ReadAdvance<ReadVRGatherVX_data_MF2, 0>;
def : ReadAdvance<ReadVRGatherVX_data_MF4, 0>;
def : ReadAdvance<ReadVRGatherVX_data_MF8, 0>;
def : ReadAdvance<ReadVRGatherVX_data_WorstCase, 0>;
def : ReadAdvance<ReadVRGatherVX_index_M1, 0>;
def : ReadAdvance<ReadVRGatherVX_index_M2, 0>;
def : ReadAdvance<ReadVRGatherVX_index_M4, 0>;
def : ReadAdvance<ReadVRGatherVX_index_M8, 0>;
def : ReadAdvance<ReadVRGatherVX_index_MF2, 0>;
def : ReadAdvance<ReadVRGatherVX_index_MF4, 0>;
def : ReadAdvance<ReadVRGatherVX_index_MF8, 0>;
def : ReadAdvance<ReadVRGatherVX_index_WorstCase, 0>;
def : ReadAdvance<ReadVRotV_M1, 0>;
def : ReadAdvance<ReadVRotV_M2, 0>;
def : ReadAdvance<ReadVRotV_M4, 0>;
def : ReadAdvance<ReadVRotV_M8, 0>;
def : ReadAdvance<ReadVRotV_MF2, 0>;
def : ReadAdvance<ReadVRotV_MF4, 0>;
def : ReadAdvance<ReadVRotV_MF8, 0>;
def : ReadAdvance<ReadVRotV_WorstCase, 0>;
def : ReadAdvance<ReadVRotX_M1, 0>;
def : ReadAdvance<ReadVRotX_M2, 0>;
def : ReadAdvance<ReadVRotX_M4, 0>;
def : ReadAdvance<ReadVRotX_M8, 0>;
def : ReadAdvance<ReadVRotX_MF2, 0>;
def : ReadAdvance<ReadVRotX_MF4, 0>;
def : ReadAdvance<ReadVRotX_MF8, 0>;
def : ReadAdvance<ReadVSALUV_M1, 0>;
def : ReadAdvance<ReadVSALUV_M2, 0>;
def : ReadAdvance<ReadVSALUV_M4, 0>;
def : ReadAdvance<ReadVSALUV_M8, 0>;
def : ReadAdvance<ReadVSALUV_MF2, 0>;
def : ReadAdvance<ReadVSALUV_MF4, 0>;
def : ReadAdvance<ReadVSALUV_MF8, 0>;
def : ReadAdvance<ReadVSALUV_WorstCase, 0>;
def : ReadAdvance<ReadVSALUX_M1, 0>;
def : ReadAdvance<ReadVSALUX_M2, 0>;
def : ReadAdvance<ReadVSALUX_M4, 0>;
def : ReadAdvance<ReadVSALUX_M8, 0>;
def : ReadAdvance<ReadVSALUX_MF2, 0>;
def : ReadAdvance<ReadVSALUX_MF4, 0>;
def : ReadAdvance<ReadVSALUX_MF8, 0>;
def : ReadAdvance<ReadVSALUX_WorstCase, 0>;
def : ReadAdvance<ReadVSETVL, 0>;
def : ReadAdvance<ReadVSETVLI, 0>;
def : ReadAdvance<ReadVSHA2CHV_M1, 0>;
def : ReadAdvance<ReadVSHA2CHV_M2, 0>;
def : ReadAdvance<ReadVSHA2CHV_M4, 0>;
def : ReadAdvance<ReadVSHA2CHV_M8, 0>;
def : ReadAdvance<ReadVSHA2CHV_MF2, 0>;
def : ReadAdvance<ReadVSHA2CHV_WorstCase, 0>;
def : ReadAdvance<ReadVSHA2CLV_M1, 0>;
def : ReadAdvance<ReadVSHA2CLV_M2, 0>;
def : ReadAdvance<ReadVSHA2CLV_M4, 0>;
def : ReadAdvance<ReadVSHA2CLV_M8, 0>;
def : ReadAdvance<ReadVSHA2CLV_MF2, 0>;
def : ReadAdvance<ReadVSHA2CLV_WorstCase, 0>;
def : ReadAdvance<ReadVSHA2MSV_M1_E32, 0>;
def : ReadAdvance<ReadVSHA2MSV_M1_E64, 0>;
def : ReadAdvance<ReadVSHA2MSV_M2_E32, 0>;
def : ReadAdvance<ReadVSHA2MSV_M2_E64, 0>;
def : ReadAdvance<ReadVSHA2MSV_M4_E32, 0>;
def : ReadAdvance<ReadVSHA2MSV_M4_E64, 0>;
def : ReadAdvance<ReadVSHA2MSV_M8_E32, 0>;
def : ReadAdvance<ReadVSHA2MSV_M8_E64, 0>;
def : ReadAdvance<ReadVSHA2MSV_MF2_E32, 0>;
def : ReadAdvance<ReadVSHA2MSV_WorstCase, 0>;
def : ReadAdvance<ReadVSM3CV_M1, 0>;
def : ReadAdvance<ReadVSM3CV_M2, 0>;
def : ReadAdvance<ReadVSM3CV_M4, 0>;
def : ReadAdvance<ReadVSM3CV_M8, 0>;
def : ReadAdvance<ReadVSM3CV_MF2, 0>;
def : ReadAdvance<ReadVSM3CV_WorstCase, 0>;
def : ReadAdvance<ReadVSM3MEV_M1, 0>;
def : ReadAdvance<ReadVSM3MEV_M2, 0>;
def : ReadAdvance<ReadVSM3MEV_M4, 0>;
def : ReadAdvance<ReadVSM3MEV_M8, 0>;
def : ReadAdvance<ReadVSM3MEV_MF2, 0>;
def : ReadAdvance<ReadVSM3MEV_WorstCase, 0>;
def : ReadAdvance<ReadVSM4KV_M1, 0>;
def : ReadAdvance<ReadVSM4KV_M2, 0>;
def : ReadAdvance<ReadVSM4KV_M4, 0>;
def : ReadAdvance<ReadVSM4KV_M8, 0>;
def : ReadAdvance<ReadVSM4KV_MF2, 0>;
def : ReadAdvance<ReadVSM4KV_WorstCase, 0>;
def : ReadAdvance<ReadVSM4RV_M1, 0>;
def : ReadAdvance<ReadVSM4RV_M2, 0>;
def : ReadAdvance<ReadVSM4RV_M4, 0>;
def : ReadAdvance<ReadVSM4RV_M8, 0>;
def : ReadAdvance<ReadVSM4RV_MF2, 0>;
def : ReadAdvance<ReadVSMulV_M1, 0>;
def : ReadAdvance<ReadVSMulV_M2, 0>;
def : ReadAdvance<ReadVSMulV_M4, 0>;
def : ReadAdvance<ReadVSMulV_M8, 0>;
def : ReadAdvance<ReadVSMulV_MF2, 0>;
def : ReadAdvance<ReadVSMulV_MF4, 0>;
def : ReadAdvance<ReadVSMulV_MF8, 0>;
def : ReadAdvance<ReadVSMulV_WorstCase, 0>;
def : ReadAdvance<ReadVSMulX_M1, 0>;
def : ReadAdvance<ReadVSMulX_M2, 0>;
def : ReadAdvance<ReadVSMulX_M4, 0>;
def : ReadAdvance<ReadVSMulX_M8, 0>;
def : ReadAdvance<ReadVSMulX_MF2, 0>;
def : ReadAdvance<ReadVSMulX_MF4, 0>;
def : ReadAdvance<ReadVSMulX_MF8, 0>;
def : ReadAdvance<ReadVSMulX_WorstCase, 0>;
def : ReadAdvance<ReadVSShiftV_M1, 0>;
def : ReadAdvance<ReadVSShiftV_M2, 0>;
def : ReadAdvance<ReadVSShiftV_M4, 0>;
def : ReadAdvance<ReadVSShiftV_M8, 0>;
def : ReadAdvance<ReadVSShiftV_MF2, 0>;
def : ReadAdvance<ReadVSShiftV_MF4, 0>;
def : ReadAdvance<ReadVSShiftV_MF8, 0>;
def : ReadAdvance<ReadVSShiftV_WorstCase, 0>;
def : ReadAdvance<ReadVSShiftX_M1, 0>;
def : ReadAdvance<ReadVSShiftX_M2, 0>;
def : ReadAdvance<ReadVSShiftX_M4, 0>;
def : ReadAdvance<ReadVSShiftX_M8, 0>;
def : ReadAdvance<ReadVSShiftX_MF2, 0>;
def : ReadAdvance<ReadVSShiftX_MF4, 0>;
def : ReadAdvance<ReadVSShiftX_MF8, 0>;
def : ReadAdvance<ReadVSShiftX_WorstCase, 0>;
def : ReadAdvance<ReadVST1R, 0>;
def : ReadAdvance<ReadVST2R, 0>;
def : ReadAdvance<ReadVST4R, 0>;
def : ReadAdvance<ReadVST8R, 0>;
def : ReadAdvance<ReadVSTEV_M1, 0>;
def : ReadAdvance<ReadVSTEV_M2, 0>;
def : ReadAdvance<ReadVSTEV_M4, 0>;
def : ReadAdvance<ReadVSTEV_M8, 0>;
def : ReadAdvance<ReadVSTEV_MF2, 0>;
def : ReadAdvance<ReadVSTEV_MF4, 0>;
def : ReadAdvance<ReadVSTEV_MF8, 0>;
def : ReadAdvance<ReadVSTEV_WorstCase, 0>;
def : ReadAdvance<ReadVSTM_WorstCase, 0>;
def : ReadAdvance<ReadVSTOX16_M1, 0>;
def : ReadAdvance<ReadVSTOX16_M2, 0>;
def : ReadAdvance<ReadVSTOX16_M4, 0>;
def : ReadAdvance<ReadVSTOX16_M8, 0>;
def : ReadAdvance<ReadVSTOX16_MF2, 0>;
def : ReadAdvance<ReadVSTOX16_MF4, 0>;
def : ReadAdvance<ReadVSTOX16_MF8, 0>;
def : ReadAdvance<ReadVSTOX16_WorstCase, 0>;
def : ReadAdvance<ReadVSTOX32_M1, 0>;
def : ReadAdvance<ReadVSTOX32_M2, 0>;
def : ReadAdvance<ReadVSTOX32_M4, 0>;
def : ReadAdvance<ReadVSTOX32_M8, 0>;
def : ReadAdvance<ReadVSTOX32_MF2, 0>;
def : ReadAdvance<ReadVSTOX32_MF4, 0>;
def : ReadAdvance<ReadVSTOX32_MF8, 0>;
def : ReadAdvance<ReadVSTOX32_WorstCase, 0>;
def : ReadAdvance<ReadVSTOX64_M1, 0>;
def : ReadAdvance<ReadVSTOX64_M2, 0>;
def : ReadAdvance<ReadVSTOX64_M4, 0>;
def : ReadAdvance<ReadVSTOX64_M8, 0>;
def : ReadAdvance<ReadVSTOX64_MF2, 0>;
def : ReadAdvance<ReadVSTOX64_MF4, 0>;
def : ReadAdvance<ReadVSTOX64_MF8, 0>;
def : ReadAdvance<ReadVSTOX64_WorstCase, 0>;
def : ReadAdvance<ReadVSTOX8_M1, 0>;
def : ReadAdvance<ReadVSTOX8_M2, 0>;
def : ReadAdvance<ReadVSTOX8_M4, 0>;
def : ReadAdvance<ReadVSTOX8_M8, 0>;
def : ReadAdvance<ReadVSTOX8_MF2, 0>;
def : ReadAdvance<ReadVSTOX8_MF4, 0>;
def : ReadAdvance<ReadVSTOX8_MF8, 0>;
def : ReadAdvance<ReadVSTOX8_WorstCase, 0>;
def : ReadAdvance<ReadVSTOXV_M1, 0>;
def : ReadAdvance<ReadVSTOXV_M2, 0>;
def : ReadAdvance<ReadVSTOXV_M4, 0>;
def : ReadAdvance<ReadVSTOXV_M8, 0>;
def : ReadAdvance<ReadVSTOXV_MF2, 0>;
def : ReadAdvance<ReadVSTOXV_MF4, 0>;
def : ReadAdvance<ReadVSTOXV_MF8, 0>;
def : ReadAdvance<ReadVSTOXV_WorstCase, 0>;
def : ReadAdvance<ReadVSTS16V_M1, 0>;
def : ReadAdvance<ReadVSTS16V_M2, 0>;
def : ReadAdvance<ReadVSTS16V_M4, 0>;
def : ReadAdvance<ReadVSTS16V_M8, 0>;
def : ReadAdvance<ReadVSTS16V_MF2, 0>;
def : ReadAdvance<ReadVSTS16V_MF4, 0>;
def : ReadAdvance<ReadVSTS16V_WorstCase, 0>;
def : ReadAdvance<ReadVSTS32V_M1, 0>;
def : ReadAdvance<ReadVSTS32V_M2, 0>;
def : ReadAdvance<ReadVSTS32V_M4, 0>;
def : ReadAdvance<ReadVSTS32V_M8, 0>;
def : ReadAdvance<ReadVSTS32V_MF2, 0>;
def : ReadAdvance<ReadVSTS32V_WorstCase, 0>;
def : ReadAdvance<ReadVSTS64V_M1, 0>;
def : ReadAdvance<ReadVSTS64V_M2, 0>;
def : ReadAdvance<ReadVSTS64V_M4, 0>;
def : ReadAdvance<ReadVSTS64V_M8, 0>;
def : ReadAdvance<ReadVSTS64V_WorstCase, 0>;
def : ReadAdvance<ReadVSTS8V_M1, 0>;
def : ReadAdvance<ReadVSTS8V_M2, 0>;
def : ReadAdvance<ReadVSTS8V_M4, 0>;
def : ReadAdvance<ReadVSTS8V_M8, 0>;
def : ReadAdvance<ReadVSTS8V_MF2, 0>;
def : ReadAdvance<ReadVSTS8V_MF4, 0>;
def : ReadAdvance<ReadVSTS8V_MF8, 0>;
def : ReadAdvance<ReadVSTS8V_WorstCase, 0>;
def : ReadAdvance<ReadVSTSX, 0>;
def : ReadAdvance<ReadVSTUX16_M1, 0>;
def : ReadAdvance<ReadVSTUX16_M2, 0>;
def : ReadAdvance<ReadVSTUX16_M4, 0>;
def : ReadAdvance<ReadVSTUX16_M8, 0>;
def : ReadAdvance<ReadVSTUX16_MF2, 0>;
def : ReadAdvance<ReadVSTUX16_MF4, 0>;
def : ReadAdvance<ReadVSTUX16_MF8, 0>;
def : ReadAdvance<ReadVSTUX16_WorstCase, 0>;
def : ReadAdvance<ReadVSTUX32_M1, 0>;
def : ReadAdvance<ReadVSTUX32_M2, 0>;
def : ReadAdvance<ReadVSTUX32_M4, 0>;
def : ReadAdvance<ReadVSTUX32_M8, 0>;
def : ReadAdvance<ReadVSTUX32_MF2, 0>;
def : ReadAdvance<ReadVSTUX32_MF4, 0>;
def : ReadAdvance<ReadVSTUX32_MF8, 0>;
def : ReadAdvance<ReadVSTUX32_WorstCase, 0>;
def : ReadAdvance<ReadVSTUX64_M1, 0>;
def : ReadAdvance<ReadVSTUX64_M2, 0>;
def : ReadAdvance<ReadVSTUX64_M4, 0>;
def : ReadAdvance<ReadVSTUX64_M8, 0>;
def : ReadAdvance<ReadVSTUX64_MF2, 0>;
def : ReadAdvance<ReadVSTUX64_MF4, 0>;
def : ReadAdvance<ReadVSTUX64_MF8, 0>;
def : ReadAdvance<ReadVSTUX64_WorstCase, 0>;
def : ReadAdvance<ReadVSTUX8_M1, 0>;
def : ReadAdvance<ReadVSTUX8_M2, 0>;
def : ReadAdvance<ReadVSTUX8_M4, 0>;
def : ReadAdvance<ReadVSTUX8_M8, 0>;
def : ReadAdvance<ReadVSTUX8_MF2, 0>;
def : ReadAdvance<ReadVSTUX8_MF4, 0>;
def : ReadAdvance<ReadVSTUX8_MF8, 0>;
def : ReadAdvance<ReadVSTUX8_WorstCase, 0>;
def : ReadAdvance<ReadVSTUXV_M1, 0>;
def : ReadAdvance<ReadVSTUXV_M2, 0>;
def : ReadAdvance<ReadVSTUXV_M4, 0>;
def : ReadAdvance<ReadVSTUXV_M8, 0>;
def : ReadAdvance<ReadVSTUXV_MF2, 0>;
def : ReadAdvance<ReadVSTUXV_MF4, 0>;
def : ReadAdvance<ReadVSTUXV_MF8, 0>;
def : ReadAdvance<ReadVSTUXV_WorstCase, 0>;
def : ReadAdvance<ReadVSTX, 0>;
def : ReadAdvance<ReadVShiftV_M1, 0>;
def : ReadAdvance<ReadVShiftV_M2, 0>;
def : ReadAdvance<ReadVShiftV_M4, 0>;
def : ReadAdvance<ReadVShiftV_M8, 0>;
def : ReadAdvance<ReadVShiftV_MF2, 0>;
def : ReadAdvance<ReadVShiftV_MF4, 0>;
def : ReadAdvance<ReadVShiftV_MF8, 0>;
def : ReadAdvance<ReadVShiftV_WorstCase, 0>;
def : ReadAdvance<ReadVShiftX_M1, 0>;
def : ReadAdvance<ReadVShiftX_M2, 0>;
def : ReadAdvance<ReadVShiftX_M4, 0>;
def : ReadAdvance<ReadVShiftX_M8, 0>;
def : ReadAdvance<ReadVShiftX_MF2, 0>;
def : ReadAdvance<ReadVShiftX_MF4, 0>;
def : ReadAdvance<ReadVShiftX_MF8, 0>;
def : ReadAdvance<ReadVShiftX_WorstCase, 0>;
def : ReadAdvance<ReadVWSLLV_M1, 0>;
def : ReadAdvance<ReadVWSLLV_M2, 0>;
def : ReadAdvance<ReadVWSLLV_M4, 0>;
def : ReadAdvance<ReadVWSLLV_MF2, 0>;
def : ReadAdvance<ReadVWSLLV_MF4, 0>;
def : ReadAdvance<ReadVWSLLV_MF8, 0>;
def : ReadAdvance<ReadVWSLLX_M1, 0>;
def : ReadAdvance<ReadVWSLLX_M2, 0>;
def : ReadAdvance<ReadVWSLLX_M4, 0>;
def : ReadAdvance<ReadVWSLLX_MF2, 0>;
def : ReadAdvance<ReadVWSLLX_MF4, 0>;
def : ReadAdvance<ReadVWSLLX_MF8, 0>;
def : ReadAdvance<ReadXPERM, 0>;
def : ReadAdvance<ReadZIP, 0>;

//===----------------------------------------------------------------------===//
// Per-opcode overrides (InstRW).  The vendor model splits a few groups of
// instructions that upstream LLVM puts in one sched class - most notably
// vector FP add vs. vector FP sub (TX82VADD latency 6 vs. TX82VSUB
// latency 5).  Each entry below reproduces the vendor sched class of the
// listed opcodes exactly.  Generated by wt-sched/find_overrides.py.
//===----------------------------------------------------------------------===//
def TX82Unsupported : SchedWriteRes<[]> {
  let Unsupported = true;
}

def TX82Ov000_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov000_W0, ReadVPassthru_M1_E16, ReadVFALUV_M1_E16, ReadVFALUV_M1_E16], (instrs PseudoVFSUB_VV_M1_E16)>;
def TX82Ov001_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov001_W0, ReadVPassthru_M1_E16, ReadVFALUV_M1_E16, ReadVFALUV_M1_E16, ReadVMask], (instrs PseudoVFSUB_VV_M1_E16_MASK)>;
def TX82Ov002_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov002_W0, ReadVPassthru_M1_E32, ReadVFALUV_M1_E32, ReadVFALUV_M1_E32], (instrs PseudoVFSUB_VV_M1_E32)>;
def TX82Ov003_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov003_W0, ReadVPassthru_M1_E32, ReadVFALUV_M1_E32, ReadVFALUV_M1_E32, ReadVMask], (instrs PseudoVFSUB_VV_M1_E32_MASK)>;
def TX82Ov004_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov004_W0, ReadVPassthru_M1_E64, ReadVFALUV_M1_E64, ReadVFALUV_M1_E64], (instrs PseudoVFSUB_VV_M1_E64)>;
def TX82Ov005_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov005_W0, ReadVPassthru_M1_E64, ReadVFALUV_M1_E64, ReadVFALUV_M1_E64, ReadVMask], (instrs PseudoVFSUB_VV_M1_E64_MASK)>;
def TX82Ov006_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov006_W0, ReadVPassthru_M2_E16, ReadVFALUV_M2_E16, ReadVFALUV_M2_E16], (instrs PseudoVFSUB_VV_M2_E16)>;
def TX82Ov007_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov007_W0, ReadVPassthru_M2_E16, ReadVFALUV_M2_E16, ReadVFALUV_M2_E16, ReadVMask], (instrs PseudoVFSUB_VV_M2_E16_MASK)>;
def TX82Ov008_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov008_W0, ReadVPassthru_M2_E32, ReadVFALUV_M2_E32, ReadVFALUV_M2_E32], (instrs PseudoVFSUB_VV_M2_E32)>;
def TX82Ov009_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov009_W0, ReadVPassthru_M2_E32, ReadVFALUV_M2_E32, ReadVFALUV_M2_E32, ReadVMask], (instrs PseudoVFSUB_VV_M2_E32_MASK)>;
def TX82Ov010_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov010_W0, ReadVPassthru_M2_E64, ReadVFALUV_M2_E64, ReadVFALUV_M2_E64], (instrs PseudoVFSUB_VV_M2_E64)>;
def TX82Ov011_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov011_W0, ReadVPassthru_M2_E64, ReadVFALUV_M2_E64, ReadVFALUV_M2_E64, ReadVMask], (instrs PseudoVFSUB_VV_M2_E64_MASK)>;
def TX82Ov012_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov012_W0, ReadVPassthru_M4_E16, ReadVFALUV_M4_E16, ReadVFALUV_M4_E16], (instrs PseudoVFSUB_VV_M4_E16)>;
def TX82Ov013_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov013_W0, ReadVPassthru_M4_E16, ReadVFALUV_M4_E16, ReadVFALUV_M4_E16, ReadVMask], (instrs PseudoVFSUB_VV_M4_E16_MASK)>;
def TX82Ov014_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov014_W0, ReadVPassthru_M4_E32, ReadVFALUV_M4_E32, ReadVFALUV_M4_E32], (instrs PseudoVFSUB_VV_M4_E32)>;
def TX82Ov015_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov015_W0, ReadVPassthru_M4_E32, ReadVFALUV_M4_E32, ReadVFALUV_M4_E32, ReadVMask], (instrs PseudoVFSUB_VV_M4_E32_MASK)>;
def TX82Ov016_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov016_W0, ReadVPassthru_M4_E64, ReadVFALUV_M4_E64, ReadVFALUV_M4_E64], (instrs PseudoVFSUB_VV_M4_E64)>;
def TX82Ov017_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov017_W0, ReadVPassthru_M4_E64, ReadVFALUV_M4_E64, ReadVFALUV_M4_E64, ReadVMask], (instrs PseudoVFSUB_VV_M4_E64_MASK)>;
def TX82Ov018_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov018_W0, ReadVPassthru_M8_E16, ReadVFALUV_M8_E16, ReadVFALUV_M8_E16], (instrs PseudoVFSUB_VV_M8_E16)>;
def TX82Ov019_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov019_W0, ReadVPassthru_M8_E16, ReadVFALUV_M8_E16, ReadVFALUV_M8_E16, ReadVMask], (instrs PseudoVFSUB_VV_M8_E16_MASK)>;
def TX82Ov020_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov020_W0, ReadVPassthru_M8_E32, ReadVFALUV_M8_E32, ReadVFALUV_M8_E32], (instrs PseudoVFSUB_VV_M8_E32)>;
def TX82Ov021_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov021_W0, ReadVPassthru_M8_E32, ReadVFALUV_M8_E32, ReadVFALUV_M8_E32, ReadVMask], (instrs PseudoVFSUB_VV_M8_E32_MASK)>;
def TX82Ov022_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov022_W0, ReadVPassthru_M8_E64, ReadVFALUV_M8_E64, ReadVFALUV_M8_E64], (instrs PseudoVFSUB_VV_M8_E64)>;
def TX82Ov023_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov023_W0, ReadVPassthru_M8_E64, ReadVFALUV_M8_E64, ReadVFALUV_M8_E64, ReadVMask], (instrs PseudoVFSUB_VV_M8_E64_MASK)>;
def TX82Ov024_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [6];
}
def : InstRW<[TX82Ov024_W0, ReadVPassthru_MF2_E16, ReadVFALUV_MF2_E16, ReadVFALUV_MF2_E16], (instrs PseudoVFSUB_VV_MF2_E16)>;
def TX82Ov025_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [6];
}
def : InstRW<[TX82Ov025_W0, ReadVPassthru_MF2_E16, ReadVFALUV_MF2_E16, ReadVFALUV_MF2_E16, ReadVMask], (instrs PseudoVFSUB_VV_MF2_E16_MASK)>;
def TX82Ov026_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [6];
}
def : InstRW<[TX82Ov026_W0, ReadVPassthru_MF2_E32, ReadVFALUV_MF2_E32, ReadVFALUV_MF2_E32], (instrs PseudoVFSUB_VV_MF2_E32)>;
def TX82Ov027_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [6];
}
def : InstRW<[TX82Ov027_W0, ReadVPassthru_MF2_E32, ReadVFALUV_MF2_E32, ReadVFALUV_MF2_E32, ReadVMask], (instrs PseudoVFSUB_VV_MF2_E32_MASK)>;
def TX82Ov028_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : InstRW<[TX82Ov028_W0, ReadVPassthru_MF4_E16, ReadVFALUV_MF4_E16, ReadVFALUV_MF4_E16], (instrs PseudoVFSUB_VV_MF4_E16)>;
def TX82Ov029_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : InstRW<[TX82Ov029_W0, ReadVPassthru_MF4_E16, ReadVFALUV_MF4_E16, ReadVFALUV_MF4_E16, ReadVMask], (instrs PseudoVFSUB_VV_MF4_E16_MASK)>;
def TX82Ov030_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov030_W0, ReadIALU], (instrs XTsm_VSETVLI)>;
def TX82Ov031_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov031_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VADD_VV)>;
def TX82Ov032_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov032_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VADD_VX)>;
def TX82Ov033_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov033_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSUB_VV)>;
def TX82Ov034_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov034_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VAND_VV)>;
def TX82Ov035_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov035_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VOR_VV)>;
def TX82Ov036_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov036_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VXOR_VV)>;
def TX82Ov037_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov037_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMUL_VV)>;
def TX82Ov038_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov038_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMUL_VX)>;
def TX82Ov039_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov039_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VDIV_VV)>;
def TX82Ov040_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov040_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VDIVU_VV)>;
def TX82Ov041_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov041_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VREM_VV)>;
def TX82Ov042_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov042_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VREMU_VV)>;
def TX82Ov043_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov043_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLL_VV)>;
def TX82Ov044_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov044_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSRL_VV)>;
def TX82Ov045_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov045_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSRA_VV)>;
def TX82Ov046_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov046_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMIN_VV)>;
def TX82Ov047_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov047_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMAX_VV)>;
def TX82Ov048_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov048_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSEQ_VV)>;
def TX82Ov049_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov049_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLT_VV)>;
def TX82Ov050_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov050_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMERGE_VVM)>;
def TX82Ov051_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov051_W0, ReadIALU], (instrs XTsm_VMV_V_V)>;
def TX82Ov052_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov052_W0, ReadIALU], (instrs XTsm_VMV_V_X)>;
def TX82Ov053_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 8;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov053_W0, ReadIALU], (instrs XTsm_VMV_X_S)>;
def TX82Ov054_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov054_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VRGATHER_VV)>;
def TX82Ov055_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov055_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLIDE1UP_VX)>;
def TX82Ov056_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov056_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLE16_V)>;
def TX82Ov057_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov057_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLE32_V)>;
def TX82Ov058_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov058_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSE16_V)>;
def TX82Ov059_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov059_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSE32_V)>;
def TX82Ov060_W0 : SchedWriteRes<[TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov060_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFADD_VV)>;
def TX82Ov061_W0 : SchedWriteRes<[TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov061_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSUB_VV)>;
def TX82Ov062_W0 : SchedWriteRes<[TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov062_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMUL_VV)>;
def TX82Ov063_W0 : SchedWriteRes<[TX82VADD]> {
  let Latency = 72;
  let ReleaseAtCycles = [72];
}
def : InstRW<[TX82Ov063_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFREDUSUM_VS)>;
def TX82Ov064_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov064_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVT_F_F_W)>;
def TX82Ov065_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov065_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVT_F_F_V)>;
def TX82Ov066_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov066_W0], (instrs XTsm_AND_SS)>;
def TX82Ov067_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov067_W0], (instrs XTsm_ANDN_SS)>;
def TX82Ov068_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov068_W0], (instrs XTsm_OR_SS)>;
def TX82Ov069_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov069_W0], (instrs XTsm_XOR_SS)>;
def TX82Ov070_W0 : SchedWriteRes<[TX82VCTRL]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov070_W0], (instrs XTsm_ISETEXEC)>;
def TX82Ov071_W0 : SchedWriteRes<[TX82VCTRL]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov071_W0], (instrs XTsm_ISETEXEC0)>;
def TX82Ov072_W0 : SchedWriteRes<[TX82VCTRL]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov072_W0], (instrs XTsm_ISETEXEC1)>;
def TX82Ov073_W0 : SchedWriteRes<[TX82KCoreWB]> {
  let Latency = 18;
  let ReleaseAtCycles = [18];
}
def : InstRW<[TX82Ov073_W0], (instrs XTsm_KCORE_NCC_READ)>;
def TX82Ov074_W0 : SchedWriteRes<[TX82KCoreWB]> {
  let Latency = 18;
  let ReleaseAtCycles = [18];
}
def : InstRW<[TX82Ov074_W0], (instrs XTsm_KCORE_WAIT)>;
def TX82Ov075_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov075_W0], (instrs XTsm_MV128_S_V)>;
def TX82Ov076_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov076_W0], (instrs XTsm_MV128_S_X)>;
def TX82Ov077_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov077_W0], (instrs XTsm_MV128_V_S)>;
def TX82Ov078_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov078_W0], (instrs XTsm_NAND_SS)>;
def TX82Ov079_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov079_W0], (instrs XTsm_NOR_SS)>;
def TX82Ov080_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov080_W0], (instrs XTsm_ORN_SS)>;
def TX82Ov081_W0 : SchedWriteRes<[TX82NCCConfig]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov081_W0], (instrs XTsm_SET_MEM_ALLOC)>;
def TX82Ov082_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov082_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VADC_VVM)>;
def TX82Ov083_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov083_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VADC_VXM)>;
def TX82Ov084_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov084_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VAND_VS)>;
def TX82Ov085_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov085_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VAND_VX)>;
def TX82Ov086_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 2;
  let ReleaseAtCycles = [33];
}
def : InstRW<[TX82Ov086_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VBIT2FP_M)>;
def TX82Ov087_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [131];
}
def : InstRW<[TX82Ov087_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VCOMPRESS_VM)>;
def TX82Ov088_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov088_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VCOS_V)>;
def TX82Ov089_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}
def : InstRW<[TX82Ov089_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VCPOP_M)>;
def TX82Ov090_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov090_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VDIV_VX)>;
def TX82Ov091_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov091_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VDIVU_VX)>;
def TX82Ov092_W0 : SchedWriteRes<[TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov092_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFADD_VS)>;
def TX82Ov093_W0 : SchedWriteRes<[TX82VADD]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov093_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFADD_VX)>;
def TX82Ov094_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov094_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFCVT_F_X_V)>;
def TX82Ov095_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov095_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFCVT_F_XU_V)>;
def TX82Ov096_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov096_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFCVT_X_F_V)>;
def TX82Ov097_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov097_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFCVT_XU_F_V)>;
def TX82Ov098_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov098_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFEQ_VS)>;
def TX82Ov099_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov099_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFEQ_VV)>;
def TX82Ov100_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov100_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFEQ_VX)>;
def TX82Ov101_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov101_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFGE_VV)>;
def TX82Ov102_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov102_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFGE_VX)>;
def TX82Ov103_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov103_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFGT_VV)>;
def TX82Ov104_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov104_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFGT_VX)>;
def TX82Ov105_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : InstRW<[TX82Ov105_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFIRST_M)>;
def TX82Ov106_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov106_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFLE_VS)>;
def TX82Ov107_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov107_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFLE_VV)>;
def TX82Ov108_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov108_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFLE_VX)>;
def TX82Ov109_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov109_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFLT_VS)>;
def TX82Ov110_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov110_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFLT_VV)>;
def TX82Ov111_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov111_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFLT_VX)>;
def TX82Ov112_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov112_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMAX_VS)>;
def TX82Ov113_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov113_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMAX_VV)>;
def TX82Ov114_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov114_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMAX_VX)>;
def TX82Ov115_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov115_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMIN_VS)>;
def TX82Ov116_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov116_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMIN_VV)>;
def TX82Ov117_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov117_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMIN_VX)>;
def TX82Ov118_W0 : SchedWriteRes<[TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov118_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMUL_VS)>;
def TX82Ov119_W0 : SchedWriteRes<[TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov119_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFMUL_VX)>;
def TX82Ov120_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov120_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVT_X_F_W)>;
def TX82Ov121_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov121_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTBF16_X_F_W)>;
def TX82Ov122_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov122_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTE4M3_F_BF_W)>;
def TX82Ov123_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov123_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTE4M3_F_F_W)>;
def TX82Ov124_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov124_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTE5M2_F_BF_W)>;
def TX82Ov125_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov125_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTE5M2_F_F_W)>;
def TX82Ov126_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov126_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTFP4_F_BF_W)>;
def TX82Ov127_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov127_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNCVTFP4_F_F_W)>;
def TX82Ov128_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov128_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNE_VV)>;
def TX82Ov129_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov129_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFNE_VX)>;
def TX82Ov130_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov130_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFREC7_V)>;
def TX82Ov131_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 57;
  let ReleaseAtCycles = [57];
}
def : InstRW<[TX82Ov131_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFREDMAX_VS)>;
def TX82Ov132_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 57;
  let ReleaseAtCycles = [57];
}
def : InstRW<[TX82Ov132_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFREDMIN_VS)>;
def TX82Ov133_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov133_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFRSQRT7_V)>;
def TX82Ov134_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov134_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFRSUB_VX)>;
def TX82Ov135_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov135_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSGNJ_VV)>;
def TX82Ov136_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov136_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSGNJ_VX)>;
def TX82Ov137_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov137_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSGNJN_VV)>;
def TX82Ov138_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov138_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSGNJN_VX)>;
def TX82Ov139_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov139_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSGNJX_VV)>;
def TX82Ov140_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov140_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSGNJX_VX)>;
def TX82Ov141_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov141_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSQRT_V)>;
def TX82Ov142_W0 : SchedWriteRes<[TX82VMUL]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov142_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSQUARE_V)>;
def TX82Ov143_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov143_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSUB_VS)>;
def TX82Ov144_W0 : SchedWriteRes<[TX82VSUB]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov144_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFSUB_VX)>;
def TX82Ov145_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov145_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVT_F_X_V)>;
def TX82Ov146_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov146_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTBF16_F_X_V)>;
def TX82Ov147_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov147_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTE4M3_BF_F_V)>;
def TX82Ov148_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov148_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTE4M3_F_F_V)>;
def TX82Ov149_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov149_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTE5M2_BF_F_V)>;
def TX82Ov150_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov150_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTE5M2_F_F_V)>;
def TX82Ov151_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov151_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTFP4_BF_F_V)>;
def TX82Ov152_W0 : SchedWriteRes<[TX82VCVT]> {
  let Latency = 7;
  let ReleaseAtCycles = [22];
}
def : InstRW<[TX82Ov152_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VFWCVTFP4_F_F_V)>;
def TX82Ov153_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [34];
}
def : InstRW<[TX82Ov153_W0, ReadIALU, ReadIALU], (instrs XTsm_VID_V)>;
def TX82Ov154_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 3;
  let ReleaseAtCycles = [34];
}
def : InstRW<[TX82Ov154_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VIOTA_M)>;
def TX82Ov155_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [4];
}
def : InstRW<[TX82Ov155_W0, ReadIALU], (instrs XTsm_VL1RE16_V)>;
def TX82Ov156_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [4];
}
def : InstRW<[TX82Ov156_W0, ReadIALU], (instrs XTsm_VL1RE32_V)>;
def TX82Ov157_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov157_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VL2DE16_V)>;
def TX82Ov158_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov158_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VL2DE32_V)>;
def TX82Ov159_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov159_W0, ReadIALU], (instrs XTsm_VL2RE16_V)>;
def TX82Ov160_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov160_W0, ReadIALU], (instrs XTsm_VL2RE32_V)>;
def TX82Ov161_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [16];
}
def : InstRW<[TX82Ov161_W0, ReadIALU], (instrs XTsm_VL4RE16_V)>;
def TX82Ov162_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [16];
}
def : InstRW<[TX82Ov162_W0, ReadIALU], (instrs XTsm_VL4RE32_V)>;
def TX82Ov163_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov163_W0, ReadIALU], (instrs XTsm_VL8RE16_V)>;
def TX82Ov164_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov164_W0, ReadIALU], (instrs XTsm_VL8RE32_V)>;
def TX82Ov165_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov165_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLOG2_V)>;
def TX82Ov166_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov166_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLSE16_V)>;
def TX82Ov167_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 21;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov167_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLSE32_V)>;
def TX82Ov168_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [34];
}
def : InstRW<[TX82Ov168_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLUXEI32_SYNC_V)>;
def TX82Ov169_W0 : SchedWriteRes<[TX82VLOAD]> {
  let Latency = 23;
  let ReleaseAtCycles = [34];
}
def : InstRW<[TX82Ov169_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VLUXEI32_V)>;
def TX82Ov170_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov170_W0, ReadIALU, ReadIALU], (instrs XTsm_VMADC_VV)>;
def TX82Ov171_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov171_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMADC_VVM)>;
def TX82Ov172_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov172_W0, ReadIALU, ReadIALU], (instrs XTsm_VMADC_VX)>;
def TX82Ov173_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov173_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMADC_VXM)>;
def TX82Ov174_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov174_W0, ReadIALU, ReadIALU], (instrs XTsm_VMAND_MM)>;
def TX82Ov175_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov175_W0, ReadIALU, ReadIALU], (instrs XTsm_VMANDN_MM)>;
def TX82Ov176_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov176_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMAX_VS)>;
def TX82Ov177_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov177_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMAX_VX)>;
def TX82Ov178_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov178_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMAXU_VS)>;
def TX82Ov179_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov179_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMAXU_VV)>;
def TX82Ov180_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov180_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMAXU_VX)>;
def TX82Ov181_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 7;
  let ReleaseAtCycles = [38];
}
def : InstRW<[TX82Ov181_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMERGE_VXM)>;
def TX82Ov182_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov182_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFEQ_VV)>;
def TX82Ov183_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov183_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFEQ_VX)>;
def TX82Ov184_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov184_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFGE_VX)>;
def TX82Ov185_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov185_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFGT_VX)>;
def TX82Ov186_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov186_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFLE_VV)>;
def TX82Ov187_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov187_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFLE_VX)>;
def TX82Ov188_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov188_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFLT_VV)>;
def TX82Ov189_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov189_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFLT_VX)>;
def TX82Ov190_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov190_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFNE_VV)>;
def TX82Ov191_W0 : SchedWriteRes<[TX82VCMP]> {
  let Latency = 9;
  let ReleaseAtCycles = [40];
}
def : InstRW<[TX82Ov191_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMFNE_VX)>;
def TX82Ov192_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov192_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMIN_VS)>;
def TX82Ov193_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov193_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMIN_VX)>;
def TX82Ov194_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov194_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMINU_VS)>;
def TX82Ov195_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov195_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMINU_VV)>;
def TX82Ov196_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov196_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMINU_VX)>;
def TX82Ov197_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov197_W0, ReadIALU, ReadIALU], (instrs XTsm_VMNAND_MM)>;
def TX82Ov198_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov198_W0, ReadIALU, ReadIALU], (instrs XTsm_VMNOR_MM)>;
def TX82Ov199_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov199_W0, ReadIALU, ReadIALU], (instrs XTsm_VMOR_MM)>;
def TX82Ov200_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov200_W0, ReadIALU, ReadIALU], (instrs XTsm_VMORN_MM)>;
def TX82Ov201_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov201_W0, ReadIALU, ReadIALU], (instrs XTsm_VMSBC_VV)>;
def TX82Ov202_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov202_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSBC_VVM)>;
def TX82Ov203_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov203_W0, ReadIALU, ReadIALU], (instrs XTsm_VMSBC_VX)>;
def TX82Ov204_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov204_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSBC_VXM)>;
def TX82Ov205_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov205_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSBF_M)>;
def TX82Ov206_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov206_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSEQ_VX)>;
def TX82Ov207_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov207_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSGT_VX)>;
def TX82Ov208_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov208_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSGTU_VX)>;
def TX82Ov209_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov209_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSIF_M)>;
def TX82Ov210_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov210_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLE_VV)>;
def TX82Ov211_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov211_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLE_VX)>;
def TX82Ov212_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov212_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLEU_VV)>;
def TX82Ov213_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov213_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLEU_VX)>;
def TX82Ov214_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov214_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLT_VX)>;
def TX82Ov215_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov215_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLTU_VV)>;
def TX82Ov216_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov216_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSLTU_VX)>;
def TX82Ov217_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov217_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSNE_VV)>;
def TX82Ov218_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov218_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSNE_VX)>;
def TX82Ov219_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov219_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMSOF_M)>;
def TX82Ov220_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov220_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMULH_VV)>;
def TX82Ov221_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov221_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMULH_VX)>;
def TX82Ov222_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov222_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMULHSU_VV)>;
def TX82Ov223_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov223_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMULHSU_VX)>;
def TX82Ov224_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov224_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMULHU_VV)>;
def TX82Ov225_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [41];
}
def : InstRW<[TX82Ov225_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VMULHU_VX)>;
def TX82Ov226_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [5];
}
def : InstRW<[TX82Ov226_W0, ReadIALU, ReadIALU], (instrs XTsm_VMV_S_X)>;
def TX82Ov227_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov227_W0, ReadIALU], (instrs XTsm_VMV1R_V)>;
def TX82Ov228_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [12];
}
def : InstRW<[TX82Ov228_W0, ReadIALU], (instrs XTsm_VMV2R_V)>;
def TX82Ov229_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [20];
}
def : InstRW<[TX82Ov229_W0, ReadIALU], (instrs XTsm_VMV4R_V)>;
def TX82Ov230_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 5;
  let ReleaseAtCycles = [36];
}
def : InstRW<[TX82Ov230_W0, ReadIALU], (instrs XTsm_VMV8R_V)>;
def TX82Ov231_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov231_W0, ReadIALU, ReadIALU], (instrs XTsm_VMXNOR_MM)>;
def TX82Ov232_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov232_W0, ReadIALU, ReadIALU], (instrs XTsm_VMXOR_MM)>;
def TX82Ov233_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov233_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VOR_VS)>;
def TX82Ov234_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov234_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VOR_VX)>;
def TX82Ov235_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov235_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VPOW2_V)>;
def TX82Ov236_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov236_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VREM_VX)>;
def TX82Ov237_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 25;
  let ReleaseAtCycles = [273];
}
def : InstRW<[TX82Ov237_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VREMU_VX)>;
def TX82Ov238_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 22;
  let ReleaseAtCycles = [270];
}
def : InstRW<[TX82Ov238_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VREVERSE_V)>;
def TX82Ov239_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov239_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VRGATHER_VI)>;
def TX82Ov240_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov240_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VRGATHER_VX)>;
def TX82Ov241_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov241_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VRGATHEREI16_VV)>;
def TX82Ov242_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov242_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VRSUB_VX)>;
def TX82Ov243_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [4];
}
def : InstRW<[TX82Ov243_W0, ReadIALU, ReadIALU], (instrs XTsm_VS1R_V)>;
def TX82Ov244_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov244_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VS2DE16_V)>;
def TX82Ov245_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov245_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VS2DE32_V)>;
def TX82Ov246_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [8];
}
def : InstRW<[TX82Ov246_W0, ReadIALU, ReadIALU], (instrs XTsm_VS2R_V)>;
def TX82Ov247_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [16];
}
def : InstRW<[TX82Ov247_W0, ReadIALU, ReadIALU], (instrs XTsm_VS4R_V)>;
def TX82Ov248_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov248_W0, ReadIALU, ReadIALU], (instrs XTsm_VS8R_V)>;
def TX82Ov249_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov249_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSBC_VVM)>;
def TX82Ov250_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov250_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSBC_VXM)>;
def TX82Ov251_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov251_W0], (instrs XTsm_VSET2DVLOAD0)>;
def TX82Ov252_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov252_W0], (instrs XTsm_VSET2DVLOAD1)>;
def TX82Ov253_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov253_W0], (instrs XTsm_VSET2DVSTORE0)>;
def TX82Ov254_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov254_W0], (instrs XTsm_VSET2DVSTORE1)>;
def TX82Ov255_W0 : SchedWriteRes<[TX82VCTRL]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov255_W0], (instrs XTsm_VSETBARRIER)>;
def TX82Ov256_W0 : SchedWriteRes<[TX82VCTRL]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov256_W0], (instrs XTsm_VSETSIMT)>;
def TX82Ov257_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov257_W0], (instrs XTsm_VSETSIMTMUL)>;
def TX82Ov258_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov258_W0], (instrs XTsm_VSETSTART)>;
def TX82Ov259_W0 : SchedWriteRes<[TX82VDISPATCH]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov259_W0], (instrs XTsm_VSETXFRM)>;
def TX82Ov260_W0 : SchedWriteRes<[TX82VMULTIF]> {
  let Latency = 12;
  let ReleaseAtCycles = [43];
}
def : InstRW<[TX82Ov260_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSIN_V)>;
def TX82Ov261_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [25];
}
def : InstRW<[TX82Ov261_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLH_VV)>;
def TX82Ov262_W0 : SchedWriteRes<[TX82VISPU]> {
  let Latency = 10;
  let ReleaseAtCycles = [25];
}
def : InstRW<[TX82Ov262_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLH_VX)>;
def TX82Ov263_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov263_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLIDE1DOWN_VX)>;
def TX82Ov264_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov264_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLIDEDOWN_VI)>;
def TX82Ov265_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov265_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLIDEDOWN_VX)>;
def TX82Ov266_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov266_W0, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLIDEUP_VI)>;
def TX82Ov267_W0 : SchedWriteRes<[TX82VMISC]> {
  let Latency = 26;
  let ReleaseAtCycles = [274];
}
def : InstRW<[TX82Ov267_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLIDEUP_VX)>;
def TX82Ov268_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov268_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSLL_VX)>;
def TX82Ov269_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov269_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSRA_VX)>;
def TX82Ov270_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov270_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSRL_VX)>;
def TX82Ov271_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov271_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSSE16_V)>;
def TX82Ov272_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 9;
  let ReleaseAtCycles = [32];
}
def : InstRW<[TX82Ov272_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSSE32_V)>;
def TX82Ov273_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov273_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSUB_VX)>;
def TX82Ov274_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [34];
}
def : InstRW<[TX82Ov274_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSUXEI32_SYNC_V)>;
def TX82Ov275_W0 : SchedWriteRes<[TX82VSTORE]> {
  let Latency = 11;
  let ReleaseAtCycles = [34];
}
def : InstRW<[TX82Ov275_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VSUXEI32_V)>;
def TX82Ov276_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov276_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VXOR_VS)>;
def TX82Ov277_W0 : SchedWriteRes<[TX82VIALU]> {
  let Latency = 6;
  let ReleaseAtCycles = [37];
}
def : InstRW<[TX82Ov277_W0, ReadIALU, ReadIALU, ReadIALU, ReadIALU], (instrs XTsm_VXOR_VX)>;
def TX82Ov278_W0 : SchedWriteRes<[TX82SIMTStack]> {
  let Latency = 1;
}
def : InstRW<[TX82Ov278_W0], (instrs XTsm_XNOR_SS)>;

} // SchedModel = TX82Model

