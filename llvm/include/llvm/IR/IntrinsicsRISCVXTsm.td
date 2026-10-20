//===- IntrinsicsRISCVXTsm.td - TSM intrinsics --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the TSM intrinsics for RISC-V.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "riscv" in {
  def int_riscv_vsetvli_bf   : DefaultAttrsIntrinsic<[llvm_anyint_ty],
                           /* AVL */  [LLVMMatchType<0>,
                           /* VSEW */  LLVMMatchType<0>,
                           /* VLMUL */ LLVMMatchType<0>,
                           /* DT */    LLVMMatchType<0>],
                                      [IntrNoMem,
                                       ImmArg<ArgIndex<1>>,
                                       ImmArg<ArgIndex<2>>]>;
  def int_riscv_vsetvlimax_bf : DefaultAttrsIntrinsic<[llvm_anyint_ty],
                            /* VSEW */ [LLVMMatchType<0>,
                            /* VLMUL */ LLVMMatchType<0>,
                           /* DT */    LLVMMatchType<0>],
                                      [IntrNoMem,
                                       ImmArg<ArgIndex<0>>,
                                       ImmArg<ArgIndex<1>>]>;
}
// Input: (vector_in, vector_in, vector_in(S), frm, vl)
class TX8BinaryVSUnMaskedRoundingMode
    : DefaultAttrsIntrinsic<[llvm_anyvector_ty],
                             [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyvector_ty, llvm_anyint_ty, LLVMMatchType<2>],
                             [ImmArg<ArgIndex<3>>, IntrNoMem]>, RISCVVIntrinsic {
    let VLOperand = 4;
}
// Input: (vector_in, vector_in, vector_in(S), mask, frm, vl, policy)
class TX8BinaryVSMaskedRoundingMode
    : DefaultAttrsIntrinsic<[llvm_anyvector_ty],
                            [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyvector_ty, LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty, LLVMMatchType<2>, LLVMMatchType<2>],
                            [ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<6>>, IntrNoMem]>, RISCVVIntrinsic {
    let VLOperand = 5;
}
// Input: (vector_in, vector_in, vector_in(S), vl)
class TX8BinaryVSUnMasked
    : DefaultAttrsIntrinsic<[llvm_anyvector_ty],
                             [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyvector_ty, llvm_anyint_ty],
                             [IntrNoMem]>, RISCVVIntrinsic {
    let VLOperand = 3;
}
// Input: (vector_in, vector_in, vector_in(S), mask, vl, policy)
class TX8BinaryVSMasked
    : DefaultAttrsIntrinsic<[llvm_anyvector_ty],
                            [LLVMMatchType<0>, LLVMMatchType<0>, llvm_anyvector_ty, LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>, llvm_anyint_ty, LLVMMatchType<2>],
                            [ImmArg<ArgIndex<5>>, IntrNoMem]>, RISCVVIntrinsic {
    let VLOperand = 4;
}

multiclass RISCVBinaryVSRoundingMode {
    def "int_riscv_" # NAME : TX8BinaryVSUnMaskedRoundingMode;
    def "int_riscv_" # NAME # "_mask" : TX8BinaryVSMaskedRoundingMode;
}
multiclass RISCVBinaryVS {
    def "int_riscv_" # NAME : TX8BinaryVSUnMasked;
    def "int_riscv_" # NAME # "_mask" : TX8BinaryVSMasked;
}
multiclass RISCVBinaryVV_VX_VS: RISCVBinaryAAX {
    defm _vs : RISCVBinaryVS;
}

multiclass SIMTBinary {
    def "int_riscv_" # NAME : DefaultAttrsIntrinsic<[llvm_v1i128_ty], [llvm_v1i128_ty, llvm_v1i128_ty], [IntrNoMem]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}

let TargetPrefix = "riscv" in {
    defm vfadd_vs : RISCVBinaryVSRoundingMode;
    defm vfsub_vs : RISCVBinaryVSRoundingMode;
    defm vfmul_vs : RISCVBinaryVSRoundingMode;
    defm vfmax_vs : RISCVBinaryVS;
    defm vfmin_vs : RISCVBinaryVS;
    defm vand_vs  : RISCVBinaryVS;
    defm vor_vs   : RISCVBinaryVS;
    defm vxor_vs  : RISCVBinaryVS;
    defm vmaxu_vs : RISCVBinaryVS;
    defm vmax_vs  : RISCVBinaryVS;
    defm vminu_vs : RISCVBinaryVS;
    defm vmin_vs  : RISCVBinaryVS;

    defm vfeq : RISCVBinaryVV_VX_VS;
    defm vfle : RISCVBinaryVV_VX_VS;
    defm vflt : RISCVBinaryVV_VX_VS;
    defm vfne : RISCVBinaryAAX;
    defm vfgt : RISCVBinaryAAX;
    defm vfge : RISCVBinaryAAX;


    defm vfsquare: RISCVUnaryAARoundingMode;
    defm vpow2   : RISCVUnaryAARoundingMode;
    defm vlog2   : RISCVUnaryAARoundingMode;
    defm vsin    : RISCVUnaryAARoundingMode;
    defm vcos    : RISCVUnaryAARoundingMode;

    // Input: (passthru, mask type input, vl)
    def int_riscv_vbit2fp
            : DefaultAttrsIntrinsic<[llvm_anyvector_ty],
                                    [LLVMMatchType<0>,
                                    LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>,
                                    llvm_anyint_ty],
                                    [IntrNoMem]>, RISCVVIntrinsic {
        let VLOperand = 2;
    }

    defm vreverse : RISCVUnaryAA;
    defm vslh : RISCVBinaryABX;

    defm vl2d : RISCVUSLoad;
    defm vs2d : RISCVUSStore;

    defm vluxei_sync : RISCVILoad;
    defm vsuxei_sync : RISCVIStore;

    defm vfwcvtbf16_f_x_v : RISCVConversion;
    defm vfncvtbf16_x_f_w : RISCVConversionRoundingMode;

    defm vfncvte4m3_f_f_w  : RISCVConversionRoundingMode;
    defm vfncvte4m3_f_bf_w : RISCVConversionRoundingMode;
    defm vfwcvte4m3_f_f_v  : RISCVConversion;
    defm vfwcvte4m3_bf_f_v : RISCVConversion;

    defm vfncvte5m2_f_f_w  : RISCVConversionRoundingMode;
    defm vfncvte5m2_f_bf_w : RISCVConversionRoundingMode;
    defm vfwcvte5m2_f_f_v  : RISCVConversion;
    defm vfwcvte5m2_bf_f_v : RISCVConversion;

    defm vfncvtfp4_f_f_w  : RISCVConversionRoundingMode;
    defm vfncvtfp4_f_bf_w : RISCVConversionRoundingMode;
    defm vfwcvtfp4_f_f_v  : RISCVConversion;
    defm vfwcvtfp4_bf_f_v : RISCVConversion;

  def int_riscv_vset2dvload0 :  DefaultAttrsIntrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrNoMem, IntrHasSideEffects]>,
                                ClangBuiltin<"__builtin_riscv_vset2dvload0">;
  def int_riscv_vset2dvload1 :  DefaultAttrsIntrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrNoMem, IntrHasSideEffects]>,
                                ClangBuiltin<"__builtin_riscv_vset2dvload1">;
  def int_riscv_vset2dvstore0 : DefaultAttrsIntrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrNoMem, IntrHasSideEffects]>,
                                ClangBuiltin<"__builtin_riscv_vset2dvstore0">;
  def int_riscv_vset2dvstore1 : DefaultAttrsIntrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrNoMem, IntrHasSideEffects]>,
                                ClangBuiltin<"__builtin_riscv_vset2dvstore1">;

  def int_riscv_set_exec : DefaultAttrsIntrinsic<[], [llvm_v1i128_ty], [IntrInaccessibleMemOnly]>;
  def int_riscv_set_exec0 : DefaultAttrsIntrinsic<[], [], [IntrInaccessibleMemOnly]>;
  def int_riscv_set_exec1 : DefaultAttrsIntrinsic<[], [], [IntrInaccessibleMemOnly]>;

  def int_riscv_set_mem_alloc : DefaultAttrsIntrinsic<[], [llvm_i64_ty], [IntrNoMem, IntrHasSideEffects]>,
                                ClangBuiltin<"__builtin_riscv_set_mem_alloc">;

  def int_riscv_set_barrier : DefaultAttrsIntrinsic<[], [], [IntrNoMem, IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_set_barrier">;

  def int_riscv_mov128_s_x : DefaultAttrsIntrinsic<[llvm_v1i128_ty], [llvm_i64_ty, llvm_i64_ty], [IntrNoMem]>,
                             ClangBuiltin<"__builtin_riscv_mov128_s_x">;
  def int_riscv_mov128_s_v : DefaultAttrsIntrinsic<[llvm_v1i128_ty], [llvm_anyvector_ty], [IntrNoMem]>;
  def int_riscv_mov128_v_s : DefaultAttrsIntrinsic<[llvm_anyvector_ty], [llvm_v1i128_ty], [IntrNoMem]>;

  defm and_s:  SIMTBinary;
  defm or_s:   SIMTBinary;
  defm xor_s:  SIMTBinary;
  defm andn_s: SIMTBinary;
  defm orn_s:  SIMTBinary;
  defm nand_s: SIMTBinary;
  defm nor_s:  SIMTBinary;
  defm xnor_s: SIMTBinary;
}


multiclass ne_config_intr {
    def "int_riscv_" # NAME : DefaultAttrsIntrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrWriteMem]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass ne_compute_intr {
    def "int_riscv_" # NAME : DefaultAttrsIntrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], []>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass cache_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass comm_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass dte_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass rdma_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass rdma_index_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass wdma_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass wdma_index_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass tdma_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}
multiclass lsu_set_intrinsic {
    def "int_riscv_" # NAME : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects, IntrWriteMem]>,
                              ClangBuiltin<"__builtin_riscv_" # NAME>;
}


let TargetPrefix = "riscv" in {
  defm gemm            : ne_compute_intr;
  defm gemm_psum       : ne_compute_intr;
  defm conv            : ne_compute_intr;
  defm conv_psum       : ne_compute_intr;
  defm convbw          : ne_compute_intr;
  defm convbw_psum     : ne_compute_intr;
  defm mma             : ne_compute_intr;
  defm mma_first       : ne_compute_intr;
  defm mma_first_psum  : ne_compute_intr;
  defm mma_last        : ne_compute_intr;
  defm ne_set_prec     : ne_config_intr;
  defm ne_set_misc     : ne_config_intr;
  defm ne_set_mkn      : ne_config_intr;
  defm ne_set_h_oh_ow  : ne_config_intr;
  defm ne_set_conv_pad : ne_config_intr;
  defm ne_set_conv_swr : ne_config_intr;
  defm ne_set_length   : ne_config_intr;

  defm l0_invalid        : cache_intrinsic;
  defm l0_flush          : cache_intrinsic;
  defm l1_invalid        : cache_intrinsic;
  defm l1_flush          : cache_intrinsic;
  defm l1_sw_prefetch    : cache_intrinsic;
  defm l0_set_src_stride : cache_intrinsic;
  defm l0_set_src_itera  : cache_intrinsic;
  defm l1_set_src_stride : cache_intrinsic;
  defm l1_set_src_itera  : cache_intrinsic;

  defm send                    : comm_intrinsic;
  defm wait_send               : comm_intrinsic;
  defm recv                    : comm_intrinsic;
  defm direct_write            : comm_intrinsic;
  defm wait_recv               : comm_intrinsic;
  defm send_roce               : comm_intrinsic;
  defm wait_send_roce          : comm_intrinsic;
  defm recv_roce               : comm_intrinsic;
  defm wait_recv_roce          : comm_intrinsic;

  def int_riscv_send_sync : Intrinsic<[], [llvm_i64_ty], [IntrHasSideEffects]>,
                            ClangBuiltin<"__builtin_riscv_send_sync">;
  def int_riscv_wait_sync : Intrinsic<[], [llvm_i64_ty], [IntrHasSideEffects]>,
                            ClangBuiltin<"__builtin_riscv_wait_sync">;
  def int_riscv_inst_barrier : Intrinsic<[], [], [IntrHasSideEffects]>,
                               ClangBuiltin<"__builtin_riscv_inst_barrier">;
  def int_riscv_kcore_ncc_read : Intrinsic<[llvm_i64_ty], [llvm_i64_ty], [IntrHasSideEffects]>,
                                 ClangBuiltin<"__builtin_riscv_kcore_ncc_read">;
  def int_riscv_kcore_wait : Intrinsic<[llvm_i64_ty], [], [IntrHasSideEffects]>,
                             ClangBuiltin<"__builtin_riscv_kcore_wait">;

  defm dte_gather             : dte_intrinsic;
  defm dte_scatter            : dte_intrinsic;
  defm dte_gather_scatter_g2l : dte_intrinsic;
  defm dte_gather_scatter_l2g : dte_intrinsic;
  defm dte_index_gather       : dte_intrinsic;
  defm dte_index_scatter      : dte_intrinsic;
  defm dte_set_rdma_misc             : dte_intrinsic;
  defm dte_set_rdma_src_itera_stride : dte_intrinsic;
  defm dte_set_rdma_dst_itera_stride : dte_intrinsic;
  defm dte_set_wdma_misc             : dte_intrinsic;
  defm dte_set_wdma_src_itera_stride : dte_intrinsic;
  defm dte_set_wdma_dst_itera_stride : dte_intrinsic;
  defm dte_set_length                : dte_intrinsic;

  defm lsu_rdma_gather         : rdma_intrinsic;
  defm lsu_rdma_scatter        : rdma_intrinsic;
  defm lsu_rdma_gather_scatter : rdma_intrinsic;
  defm lsu_rdma_mem_set        : rdma_intrinsic;
  defm lsu_rdma_mem_copy       : rdma_intrinsic;
  defm lsu_rdma_index_gather   : rdma_index_intrinsic;

  defm lsu_wdma_gather         : wdma_intrinsic;
  defm lsu_wdma_scatter        : wdma_intrinsic;
  defm lsu_wdma_gather_scatter : wdma_intrinsic;
  defm lsu_wdma_mem_set        : wdma_intrinsic;
  defm lsu_wdma_mem_copy       : wdma_intrinsic;
  defm lsu_wdma_index_scatter  : wdma_index_intrinsic;

  defm lsu_tdma_nchw2nhwc    : tdma_intrinsic;
  defm lsu_tdma_nhwc2nchw    : tdma_intrinsic;
  defm lsu_tdma_mem_copy     : tdma_intrinsic;
  defm lsu_tdma_bank_scatter : tdma_intrinsic;

  defm lsu_set_rdma_src_itera_stride : lsu_set_intrinsic;
  defm lsu_set_rdma_dst_itera_stride : lsu_set_intrinsic;
  defm lsu_set_rdma_hwc              : lsu_set_intrinsic;
  defm lsu_set_wdma_src_itera_stride : lsu_set_intrinsic;
  defm lsu_set_wdma_dst_itera_stride : lsu_set_intrinsic;
  defm lsu_set_wdma_hwc              : lsu_set_intrinsic;
  defm lsu_set_tdma_hwc              : lsu_set_intrinsic;
  defm lsu_set_length                : lsu_set_intrinsic;
}



defvar RPU_MAX_GRID_SIZE_X = 0x7fffffff;
defvar RPU_MAX_GRID_SIZE_Y = 0xffff;
defvar RPU_MAX_GRID_SIZE_Z = 0xffff;

defvar RPU_MAX_BLOCK_SIZE_X = 1024;
defvar RPU_MAX_BLOCK_SIZE_Y = 1024;
defvar RPU_MAX_BLOCK_SIZE_Z = 64;

class RPUBuiltin :
  ClangBuiltin<!strconcat("__", !substr(NAME, !size("int_")))> {
    assert !eq(!substr(NAME, 0, !size("int_riscv_")), "int_riscv_"),
           "RPUBuiltin must be a RPU intrinsic starting with 'int_riscv_'";
}

class RPUTXReadRegIntrinsicNB<list<IntrinsicProperty> properties = []>
  : DefaultAttrsIntrinsic<[llvm_i32_ty], [],
      !listconcat([IntrNoMem, IntrSpeculatable, NoUndef<RetIndex>], properties)>;

class RPUTXReadRegIntrinsic<list<IntrinsicProperty> properties = []>
  : RPUTXReadRegIntrinsicNB<properties>, RPUBuiltin;

multiclass RPUTXReadRegIntrinsic_v4<list<list<IntrinsicProperty>> properties = [[], [], [], []]> {
  assert !eq(!size(properties), 4), "properties must be a list of 4 lists";
  defvar suffixes = ["_x", "_y", "_z", "_w"];
  foreach i = !range(suffixes) in
    def suffixes[i] : RPUTXReadRegIntrinsic<properties[i]>;
}

multiclass RPUTXReadRegIntrinsicNB_v4<list<list<IntrinsicProperty>> properties = [[], [], [], []]> {
  assert !eq(!size(properties), 4), "properties must be a list of 4 lists";
  defvar suffixes = ["_x", "_y", "_z", "_w"];
  foreach i = !range(suffixes) in
    def suffixes[i] : RPUTXReadRegIntrinsicNB<properties[i]>;
}

let TargetPrefix = "riscv" in {

defm int_riscv_read_reg_tid
  : RPUTXReadRegIntrinsic_v4<[[Range<RetIndex, 0, RPU_MAX_BLOCK_SIZE_X>],
                                 [Range<RetIndex, 0, RPU_MAX_BLOCK_SIZE_Y>],
                                 [Range<RetIndex, 0, RPU_MAX_BLOCK_SIZE_Z>],
                                 [Range<RetIndex, 0, 1>]]>;

defm int_riscv_read_reg_ntid
  : RPUTXReadRegIntrinsic_v4<[[Range<RetIndex, 1, !add(RPU_MAX_BLOCK_SIZE_X, 1)>],
                                 [Range<RetIndex, 1, !add(RPU_MAX_BLOCK_SIZE_Y, 1)>],
                                 [Range<RetIndex, 1, !add(RPU_MAX_BLOCK_SIZE_Z, 1)>],
                                 [Range<RetIndex, 0, 1>]]>;

defvar RPU_MAX_GRID_ID_RANGE = [[Range<RetIndex, 0, RPU_MAX_GRID_SIZE_X>],
                                [Range<RetIndex, 0, RPU_MAX_GRID_SIZE_Y>],
                                [Range<RetIndex, 0, RPU_MAX_GRID_SIZE_Z>],
                                [Range<RetIndex, 0, 1>]];

defvar RPU_MAX_GRID_NID_RANGE = [[Range<RetIndex, 1, !add(RPU_MAX_GRID_SIZE_X, 1)>],
                                 [Range<RetIndex, 1, !add(RPU_MAX_GRID_SIZE_Y, 1)>],
                                 [Range<RetIndex, 1, !add(RPU_MAX_GRID_SIZE_Z, 1)>],
                                 [Range<RetIndex, 0, 1>]];

defm int_riscv_read_reg_ctaid
  : RPUTXReadRegIntrinsic_v4<RPU_MAX_GRID_ID_RANGE>;

defm int_riscv_read_reg_nctaid
  : RPUTXReadRegIntrinsic_v4<RPU_MAX_GRID_NID_RANGE>;

defm int_riscv_read_reg_clusterid
  : RPUTXReadRegIntrinsicNB_v4<RPU_MAX_GRID_ID_RANGE>;
defm int_riscv_read_reg_nclusterid
  : RPUTXReadRegIntrinsicNB_v4<RPU_MAX_GRID_NID_RANGE>;
defm int_riscv_read_reg_cluster_ctaid
  : RPUTXReadRegIntrinsicNB_v4<RPU_MAX_GRID_ID_RANGE>;
defm int_riscv_read_reg_cluster_nctaid
  : RPUTXReadRegIntrinsicNB_v4<RPU_MAX_GRID_NID_RANGE>;

def int_riscv_read_reg_cluster_ctarank : RPUTXReadRegIntrinsicNB;
def int_riscv_read_reg_cluster_nctarank : RPUTXReadRegIntrinsicNB;

def int_riscv_is_explicit_cluster
  : DefaultAttrsIntrinsic<[llvm_i1_ty], [],
              [IntrNoMem, IntrSpeculatable, NoUndef<RetIndex>],
              "llvm.riscv.is_explicit_cluster">;

//===----------------------------------------------------------------------===//
// Special Intrinsics for backend internal use only. No frontend
// should emit calls to these.
//===----------------------------------------------------------------------===//
def int_riscv_get_pred_mask : DefaultAttrsIntrinsic<[llvm_v1i128_ty], [llvm_i1_ty], [IntrNoMem]>;

// set exec and return execz
def int_riscv_raisa_divergent: DefaultAttrsIntrinsic<[llvm_i64_ty], [llvm_v1i128_ty], [IntrNoMem, IntrHasSideEffects]>;
def int_riscv_raisa_get_kernel_entry_exec: DefaultAttrsIntrinsic<[llvm_anyvector_ty], [], [IntrNoMem, IntrConvergent]>;

// Similar to vmv_v_x, but it depends on control flow
def int_riscv_raisa_vmv_v_x: DefaultAttrsIntrinsic<[llvm_anyvector_ty], [llvm_i64_ty], [IntrNoMem, IntrHasSideEffects]>;

// Represent unreachable in a divergent region.
def int_riscv_unreachable : Intrinsic<[], [], [IntrConvergent, IntrNoCallback, IntrNoFree]>;

def int_riscv_raisa_reduce_fmax : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem, IntrConvergent]>;
def int_riscv_raisa_reduce_fmin : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem, IntrConvergent]>;
def int_riscv_raisa_reduce_sum  : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem, IntrConvergent]>;

def int_riscv_raisa_frsqrt : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem]>;

def int_riscv_raisa_log2 : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem]>;

def int_riscv_raisa_sin : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem]>;
def int_riscv_raisa_cos : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrNoMem]>;

def int_riscv_raisa_fmax : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>, LLVMMatchType<0>], [IntrNoMem]>;
def int_riscv_raisa_fmin : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>, LLVMMatchType<0>], [IntrNoMem]>;
def int_riscv_raisa_fdiv : DefaultAttrsIntrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>, LLVMMatchType<0>], [IntrNoMem]>;

def int_riscv_raisa_max  : DefaultAttrsIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
def int_riscv_raisa_min  : DefaultAttrsIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
def int_riscv_raisa_umax : DefaultAttrsIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
def int_riscv_raisa_umin : DefaultAttrsIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
def int_riscv_raisa_div  : DefaultAttrsIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;

} // let TargetPrefix = "riscv"
