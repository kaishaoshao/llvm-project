//==- BuiltinsRISCV.td - RISC-V Builtin function database ---*- tablegen -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the RISC-V-specific builtin function database.
//
//===----------------------------------------------------------------------===//

include "clang/Basic/BuiltinsBase.td"

class RISCVBuiltin<string prototype, string features = ""> : TargetBuiltin {
  let Spellings = ["__builtin_riscv_" # NAME];
  let Prototype = prototype;
  let Features = features;
}

let Attributes = [NoThrow, Const] in {
//===----------------------------------------------------------------------===//
// Zbb extension.
//===----------------------------------------------------------------------===//
def orc_b_32 : RISCVBuiltin<"unsigned int(unsigned int)", "zbb">;
def orc_b_64 : RISCVBuiltin<"uint64_t(uint64_t)", "zbb,64bit">;
def clz_32 : RISCVBuiltin<"unsigned int(unsigned int)", "zbb|xtheadbb">;
def clz_64 : RISCVBuiltin<"unsigned int(uint64_t)", "zbb|xtheadbb,64bit">;
def ctz_32 : RISCVBuiltin<"unsigned int(unsigned int)", "zbb">;
def ctz_64 : RISCVBuiltin<"unsigned int(uint64_t)", "zbb,64bit">;

//===----------------------------------------------------------------------===//
// Zbc or Zbkc extension.
//===----------------------------------------------------------------------===//
def clmul_32 : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)", "zbc|zbkc">;
def clmul_64 : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)", "zbc|zbkc,64bit">;
def clmulh_32 : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)", "zbc|zbkc,32bit">;
def clmulh_64 : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)", "zbc|zbkc,64bit">;
def clmulr_32 : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)", "zbc,32bit">;
def clmulr_64 : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)", "zbc,64bit">;

//===----------------------------------------------------------------------===//
// Zbkx extension.
//===----------------------------------------------------------------------===//
let Features = "zbkx,32bit" in {
def xperm4_32 : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
def xperm8_32 : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
} // Features = "zbkx,32bit"

let Features = "zbkx,64bit" in {
def xperm4_64 : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
def xperm8_64 : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
} // Features = "zbkx,64bit"

//===----------------------------------------------------------------------===//
// Zbkb extension.
//===----------------------------------------------------------------------===//
def brev8_32 : RISCVBuiltin<"unsigned int(unsigned int)", "zbkb">;
def brev8_64 : RISCVBuiltin<"uint64_t(uint64_t)", "zbkb,64bit">;
def zip_32 : RISCVBuiltin<"unsigned int(unsigned int)", "zbkb,32bit">;
def unzip_32 : RISCVBuiltin<"unsigned int(unsigned int)", "zbkb,32bit">;

//===----------------------------------------------------------------------===//
// Zknd extension.
//===----------------------------------------------------------------------===//
let Features = "zknd,32bit" in {
def aes32dsi : RISCVBuiltin<"unsigned int(unsigned int, unsigned int, _Constant unsigned int)">;
def aes32dsmi : RISCVBuiltin<"unsigned int(unsigned int, unsigned int, _Constant unsigned int)">;
} // Features = "zknd,32bit"

let Features = "zknd,64bit" in {
def aes64ds : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
def aes64dsm : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
def aes64im : RISCVBuiltin<"uint64_t(uint64_t)">;
} // Features = "zknd,64bit"

//===----------------------------------------------------------------------===//
// Zknd & Zkne extension.
//===----------------------------------------------------------------------===//
let Features = "zknd|zkne,64bit" in {
def aes64ks1i : RISCVBuiltin<"uint64_t(uint64_t, _Constant unsigned int)">;
def aes64ks2 : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
} // Features = "zknd|zkne,64bit"

//===----------------------------------------------------------------------===//
// Zkne extension.
//===----------------------------------------------------------------------===//
let Features = "zkne,32bit" in {
def aes32esi : RISCVBuiltin<"unsigned int(unsigned int, unsigned int, _Constant unsigned int)">;
def aes32esmi : RISCVBuiltin<"unsigned int(unsigned int, unsigned int, _Constant unsigned int)">;
} // Features = "zkne,32bit"

let Features = "zkne,64bit" in {
def aes64es : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
def aes64esm : RISCVBuiltin<"uint64_t(uint64_t, uint64_t)">;
} // Features = "zkne,64bit"

//===----------------------------------------------------------------------===//
// Zknh extension.
//===----------------------------------------------------------------------===//
let Features = "zknh" in {
def sha256sig0 : RISCVBuiltin<"unsigned int(unsigned int)">;
def sha256sig1 : RISCVBuiltin<"unsigned int(unsigned int)">;
def sha256sum0 : RISCVBuiltin<"unsigned int(unsigned int)">;
def sha256sum1 : RISCVBuiltin<"unsigned int(unsigned int)">;
} // Features = "zknh"

let Features = "zknh,32bit" in {
def sha512sig0h : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
def sha512sig0l : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
def sha512sig1h : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
def sha512sig1l : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
def sha512sum0r : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
def sha512sum1r : RISCVBuiltin<"unsigned int(unsigned int, unsigned int)">;
} // Features = "zknh,32bit"

let Features = "zknh,64bit" in {
def sha512sig0 : RISCVBuiltin<"uint64_t(uint64_t)">;
def sha512sig1 : RISCVBuiltin<"uint64_t(uint64_t)">;
def sha512sum0 : RISCVBuiltin<"uint64_t(uint64_t)">;
def sha512sum1 : RISCVBuiltin<"uint64_t(uint64_t)">;
} // Features = "zknh,64bit"

//===----------------------------------------------------------------------===//
// Zksed extension.
//===----------------------------------------------------------------------===//
let Features = "zksed" in {
def sm4ed : RISCVBuiltin<"unsigned int(unsigned int, unsigned int, _Constant unsigned int )">;
def sm4ks : RISCVBuiltin<"unsigned int(unsigned int, unsigned int, _Constant unsigned int)">;
} // Features = "zksed"

//===----------------------------------------------------------------------===//
// Zksh extension.
//===----------------------------------------------------------------------===//
let Features = "zksh" in {
def sm3p0 : RISCVBuiltin<"unsigned int(unsigned int)">;
def sm3p1 : RISCVBuiltin<"unsigned int(unsigned int)">;
} // Features = "zksh"

} // Attributes = [Const, NoThrow]

//===----------------------------------------------------------------------===//
// Zihintntl extension.
//===----------------------------------------------------------------------===//
let Features = "zihintntl", Attributes = [CustomTypeChecking] in {
def ntl_load : RISCVBuiltin<"void(...)">;
def ntl_store : RISCVBuiltin<"void(...)">;
} // Features = "zihintntl", Attributes = [CustomTypeChecking]

//===----------------------------------------------------------------------===//
// Zihintpause extension.
//===----------------------------------------------------------------------===//
let Features = "zihintpause", Attributes = [NoThrow] in
def pause : RISCVBuiltin<"void()">;

//===----------------------------------------------------------------------===//
// XTsm82s extension (RAISA / TX8 NPU).
//===----------------------------------------------------------------------===//

// Builtins for the new thread/cluster identification registers.  These are
// spelled without the usual `__builtin_riscv_` prefix, so the record name is
// the complete spelling.
let Attributes = [NoThrow, Const] in {
class RISCVReadRegBuiltin<string prototype> : TargetBuiltin {
  let Spellings = [NAME];
  let Prototype = prototype;
}
def __riscv_is_explicit_cluster : RISCVReadRegBuiltin<"bool()">;
def __riscv_read_reg_cluster_ctaid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_ctaid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_ctaid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_ctarank : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_nctaid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_nctaid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_nctaid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_cluster_nctarank : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_clusterid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_clusterid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_clusterid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_ctaid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_ctaid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_ctaid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_nclusterid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_nclusterid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_nclusterid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_nctaid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_nctaid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_nctaid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_ntid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_ntid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_ntid_z : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_tid_x : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_tid_y : RISCVReadRegBuiltin<"int()">;
def __riscv_read_reg_tid_z : RISCVReadRegBuiltin<"int()">;
} // Attributes = [NoThrow, Const]

// NPU control builtins.  All of them are gated on `xtsm82s`.
class RISCVXTsm82sBuiltin<string prototype, string features = "xtsm82s">
    : TargetBuiltin {
  let Spellings = ["__builtin_riscv_" # NAME];
  let Prototype = prototype;
  let Features = features;
}

// RAISA scalar floating point / integer helpers.
class RAISABuiltin<string prototype> : TargetBuiltin {
  let Spellings = ["__raisa_" # NAME];
  let Prototype = prototype;
  let Features = "xtsm82s";
  let Attributes = [NoThrow, Const];
}

// The `_f16` / `_f` / `_bf16` variants share a prototype and a spelling stem.
class RAISAFloatTemplate
    : Template<["__fp16", "float", "__bf16"], ["_f16", "_f", "_bf16"]>;

class RAISATemplateBuiltin<string prototype> : RAISABuiltin<prototype>,
                                               RAISAFloatTemplate;
def and_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def andn_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def conv : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def conv_psum : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def convbw : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def convbw_psum : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def direct_write : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_gather : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_gather_scatter_g2l : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_gather_scatter_l2g : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_index_gather : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_index_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_length : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_rdma_dst_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_rdma_misc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_rdma_src_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_wdma_dst_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_wdma_misc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def dte_set_wdma_src_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def gemm : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def gemm_psum : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def inst_barrier : RISCVXTsm82sBuiltin<"void()">;
def kcore_ncc_read : RISCVXTsm82sBuiltin<"int64_t(int64_t)">;
def kcore_wait : RISCVXTsm82sBuiltin<"int64_t()">;
def l0_flush : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l0_invalid : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l0_set_src_itera : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l0_set_src_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l1_flush : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l1_invalid : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l1_set_src_itera : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l1_set_src_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def l1_sw_prefetch : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_rdma_gather : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_rdma_gather_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_rdma_index_gather : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_rdma_mem_copy : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_rdma_mem_set : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_rdma_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_set_length : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_rdma_dst_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_rdma_hwc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_rdma_src_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_tdma_hwc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_wdma_dst_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_wdma_hwc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_set_wdma_src_itera_stride : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_tdma_bank_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_tdma_mem_copy : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_tdma_nchw2nhwc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_tdma_nhwc2nchw : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_wdma_gather : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_wdma_gather_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_wdma_index_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def lsu_wdma_mem_copy : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_wdma_mem_set : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def lsu_wdma_scatter : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def mma : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def mma_first : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def mma_first_psum : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def mma_last : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def mov128_s_x : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(int64_t, int64_t)">;
def nand_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def ne_set_conv_pad : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def ne_set_conv_swr : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def ne_set_h_oh_ow : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def ne_set_length : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def ne_set_misc : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def ne_set_mkn : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def ne_set_prec : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def nor_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def or_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def orn_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def recv : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def recv_roce : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def send : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def send_roce : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def send_sync : RISCVXTsm82sBuiltin<"void(int64_t)">;
let Attributes = [NoThrow] in
  def set_barrier : RISCVXTsm82sBuiltin<"void()">;
let Attributes = [NoThrow] in
  def set_mem_alloc : RISCVXTsm82sBuiltin<"void(uint64_t)">;
let Attributes = [NoThrow] in
  def vset2dvload0 : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
let Attributes = [NoThrow] in
  def vset2dvload1 : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
let Attributes = [NoThrow] in
  def vset2dvstore0 : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
let Attributes = [NoThrow] in
  def vset2dvstore1 : RISCVXTsm82sBuiltin<"void(int64_t, int64_t)">;
def vsetxfrm : RISCVXTsm82sBuiltin<"void(int64_t)">;
def wait_recv : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def wait_recv_roce : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def wait_send : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def wait_send_roce : RISCVXTsm82sBuiltin<"void(int64_t, int64_t, int64_t)">;
def wait_sync : RISCVXTsm82sBuiltin<"void(int64_t)">;
def xnor_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;
def xor_s : RISCVXTsm82sBuiltin<"_Vector<1, __int128_t>(_Vector<1, __int128_t>, _Vector<1, __int128_t>)">;

def div : RAISABuiltin<"int(int, int)">;
def max : RAISABuiltin<"int(int, int)">;
def min : RAISABuiltin<"int(int, int)">;
def umax : RAISABuiltin<"unsigned int(unsigned int, unsigned int)">;
def umin : RAISABuiltin<"unsigned int(unsigned int, unsigned int)">;

def cos : RAISATemplateBuiltin<"T(T)">;
def exp2 : RAISATemplateBuiltin<"T(T)">;
def fadd_rd : RAISATemplateBuiltin<"T(T, T)">;
def fadd_rn : RAISATemplateBuiltin<"T(T, T)">;
def fadd_ru : RAISATemplateBuiltin<"T(T, T)">;
def fadd_rz : RAISATemplateBuiltin<"T(T, T)">;
def fdiv : RAISATemplateBuiltin<"T(T, T)">;
def fmax : RAISATemplateBuiltin<"T(T, T)">;
def fmin : RAISATemplateBuiltin<"T(T, T)">;
def frsqrt : RAISATemplateBuiltin<"T(T)">;
def log2 : RAISATemplateBuiltin<"T(T)">;
def reduce_fmax : RAISATemplateBuiltin<"T(T)">;
def reduce_fmin : RAISATemplateBuiltin<"T(T)">;
def reduce_sum : RAISATemplateBuiltin<"T(T)">;
def sin : RAISATemplateBuiltin<"T(T)">;

//===----------------------------------------------------------------------===//
// XCV extensions.
//===----------------------------------------------------------------------===//
include "clang/Basic/BuiltinsRISCVXCV.td"
