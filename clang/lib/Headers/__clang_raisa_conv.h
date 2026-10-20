/*===---- __clang_raisa_conv.h - Device-side RAISA conv support --------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __CLANG_RAISA_CONV_H__
#define __CLANG_RAISA_CONV_H__
#include "__clang_raisa_ne.h"

#define CONV_ENCODE_H_OH_OW(H, OH, OW)                                         \
  ((static_cast<uint64_t>(H)) | ((static_cast<uint64_t>(OH)) << 16u) |         \
   ((static_cast<uint64_t>(OW)) << 32u))

#define CONV_ENCODE_SWR_PARAM0(Kx, Ky, Sx, Sy)                                 \
  ((static_cast<uint64_t>(Kx)) | (static_cast<uint64_t>(Ky) << 16u) |          \
   (static_cast<uint64_t>(Sx) << 32u) | (static_cast<uint64_t>(Sy) << 48u))

#define CONV_ENCODE_SWR_PARAM1(Dx, Dy)                                         \
  (static_cast<uint64_t>(Dx) | (static_cast<uint64_t>(Dy) << 16u))

#define CONV_ENCODE_PAD_PARAM0(r, l, b, t, ur, ul, ub, ut)                     \
  ((static_cast<uint64_t>(r)) | (static_cast<uint64_t>(l) << 8) |              \
   (static_cast<uint64_t>(b) << 16) | (static_cast<uint64_t>(t) << 24) |       \
   (static_cast<uint64_t>(ur) << 32) | (static_cast<uint64_t>(ul) << 40) |     \
   (static_cast<uint64_t>(ub) << 48) | (static_cast<uint64_t>(ut) << 56))

#define CONV_ENCODE_PAD_PARAM1(pd, pds)                                        \
  (static_cast<uint64_t>(pd) | (static_cast<uint64_t>(pds) << 32))

#define CONV_ENCODE_MISC_PARAM0(sp, w_trans, dilated, rotate)                  \
  (static_cast<uint64_t>(sp) | (static_cast<uint64_t>(w_trans) << 2) |         \
   (static_cast<uint64_t>(dilated) << 3) |                                     \
   (static_cast<uint64_t>(rotate) << 4))

#define CONV_ENCODE_MISC_PARAM1(insert_zero_x, insert_zero_y)                  \
  (static_cast<uint64_t>(insert_zero_x) |                                      \
   (static_cast<uint64_t>(insert_zero_y) << 16))

#define CONV_TENSOR_FILTER_DENSE(T)                                            \
  template <>                                                                  \
  struct tensor_descriptor<T, false, NE_TENSOR_LOCATION_L0A>                   \
      : public tensor_descriptor_base<T, false, NE_TENSOR_LOCATION_L0A> {};    \
  template <>                                                                  \
  struct filter_descriptor<T, false, NE_TENSOR_LOCATION_L0W>                   \
      : public filter_descriptor_base<T, false, NE_TENSOR_LOCATION_L0W> {};

#define CONV_TENSOR_FILTER(T)                                                  \
  CONV_TENSOR_FILTER_DENSE(T)                                                  \
  template <>                                                                  \
  struct tensor_descriptor<T, true, NE_TENSOR_LOCATION_L0A>                    \
      : public tensor_descriptor_base<T, true, NE_TENSOR_LOCATION_L0A> {};     \
  template <>                                                                  \
  struct filter_descriptor<T, true, NE_TENSOR_LOCATION_L0W>                    \
      : public sparse_filter_descriptor_base<T> {};

#define CONV_BW_TENSOR_FILTER(T)                                               \
  template <>                                                                  \
  struct tensor_descriptor<T, false, NE_TENSOR_LOCATION_L0W>                   \
      : public tensor_descriptor_base<T, false, NE_TENSOR_LOCATION_L0W> {};    \
  template <>                                                                  \
  struct filter_descriptor<T, false, NE_TENSOR_LOCATION_L0A>                   \
      : public filter_descriptor_base<T, false, NE_TENSOR_LOCATION_L0A> {};

namespace raisa {
namespace conv {

/// [HWC]/[OHOWF]
template <typename T, bool sparse_weight = false,
          ne_tensor_location_t loc = NE_TENSOR_LOCATION_L0A>
struct tensor_descriptor;

/// [KyKxFC]
template <typename T, bool sparse = false,
          ne_tensor_location_t loc = NE_TENSOR_LOCATION_L0W>
struct filter_descriptor;

/// [HWC]/[OHOWF]
template <typename T, bool sparse, ne_tensor_location_t loc>
struct tensor_descriptor_base {
  enum {
    base_align = get_ne_tensor_base_align(loc),
  };

  uint16_t h; // oh
  uint16_t w; // ow
  uint16_t c; // f

  __RAISA_NE_HOST_DEVICE_DECL__
  constexpr uint32_t get_aligned_size() const {
    constexpr uint32_t elem_size = ne_type_info<T>::elem_size;
    constexpr uint32_t ne_prec = ne_type_info<T>::ne_prec;

    uint32_t size = get_ne_tensor_aligned_elems_size(
        {1, h, w, c}, ne_type_info<T>::elem_size, loc, sparse);
    if constexpr (ne_prec == NE_PREC_MXFP4 || ne_prec == NE_PREC_MXFP8E5 ||
                  ne_prec == NE_PREC_MXFP8E4) {
      size += get_ne_tensor_aligned_scales_size(
          {1, h, w, c}, ne_type_info<T>::elem_size, loc);
    }

    return size >> 3;
  }
};

/// [KyKxFC]
template <typename T, bool sparse, ne_tensor_location_t loc>
struct filter_descriptor_base {
  enum {
    base_align = get_ne_tensor_base_align(loc),
  };

  uint16_t f;
  uint16_t ky;
  uint16_t kx;
  uint16_t c;

  __RAISA_NE_HOST_DEVICE_DECL__
  constexpr uint32_t get_aligned_size() const {
    constexpr uint32_t elem_size = ne_type_info<T>::elem_size;
    constexpr uint32_t ne_prec = ne_type_info<T>::ne_prec;

    uint32_t size = get_ne_tensor_aligned_elems_size(
        {ky, kx, f, c}, ne_type_info<T>::elem_size, loc, sparse);
    if constexpr (ne_prec == NE_PREC_MXFP4 || ne_prec == NE_PREC_MXFP8E5 ||
                  ne_prec == NE_PREC_MXFP8E4) {
      size += get_ne_tensor_aligned_scales_size(
          {ky, kx, f, c}, ne_type_info<T>::elem_size, loc);
    }

    return size >> 3;
  }
};

template <typename T>
struct sparse_filter_descriptor_base
    : public filter_descriptor_base<T, false, NE_TENSOR_LOCATION_L0W> {
  __RAISA_NE_HOST_DEVICE_DECL__
  constexpr uint32_t get_aligned_indexes_size() const {
    return get_ne_sparse_tensor_aligned_indexes_size(
        {this->ky, this->kx, this->f, this->c}, ne_type_info<T>::elem_size);
  }
};

///
/// All Supported Tensor/Filter Type
///
CONV_TENSOR_FILTER(signed char)
CONV_TENSOR_FILTER(f8e4m3)
CONV_TENSOR_FILTER(f8e5m2)
CONV_TENSOR_FILTER(fp4)
CONV_TENSOR_FILTER(__bf16)
CONV_TENSOR_FILTER(_Float16)

CONV_TENSOR_FILTER_DENSE(mxfp8e4)
CONV_TENSOR_FILTER_DENSE(mxfp8e5)
CONV_TENSOR_FILTER_DENSE(mxfp4)
CONV_TENSOR_FILTER_DENSE(tf32)
CONV_TENSOR_FILTER_DENSE(int)
CONV_TENSOR_FILTER_DENSE(float)

CONV_BW_TENSOR_FILTER(_Float16)
CONV_BW_TENSOR_FILTER(__bf16)
CONV_BW_TENSOR_FILTER(tf32)
CONV_BW_TENSOR_FILTER(float)

//
// Forward:           Y =  X Conv W  + PSUM
// Backward(Data):   dX = dY Conv W  + PSUM
// Backward(Filter): dW =  X Conv dY + PSUM
//
// clang-format off
//
//                     Forward                      Backward(Data)                     Backward(Filter)
//                   X         W                 dY                 W                X                 dY
// stride:                    stride                                1                                dilation
// dilation:                  dilation                              dilation                         stride
// padding:        padding               dilation*(k-1)-padding                    padding
// insert_zero:                              stride-1
//
// clang-format on
struct conv2d_descriptor {
  struct stride_dilation_t {
    uint8_t x;
    uint8_t y;
  };
  struct padding_unpadding_t {
    uint8_t top;
    uint8_t bottom;
    uint8_t left;
    uint8_t right;
  };

  // The arguments have different meaning for forward/backward conv2d
  stride_dilation_t stride;
  stride_dilation_t dilation;
  padding_unpadding_t padding;

  constexpr conv2d_descriptor() : stride{1, 1}, dilation{1, 1}, padding{0} {}

  /// Compute the shape of Y for X Conv W
  template <typename Ty, typename Tx, typename Tw, bool sparse>
  __RAISA_NE_HOST_DEVICE_DECL__ constexpr tensor_descriptor<Ty>
  calculate_forward_output_dim(
      const tensor_descriptor<Tx, sparse> &x_desc,
      const filter_descriptor<Tw, sparse> &w_desc) const {
    uint16_t oh = ((x_desc.h + padding.top + padding.bottom -
                    dilation.y * (w_desc.ky - 1) - 1) /
                   stride.y) +
                  1;
    uint16_t ow = ((x_desc.w + padding.left + padding.right -
                    dilation.x * (w_desc.kx - 1) - 1) /
                   stride.x) +
                  1;
    uint16_t f = w_desc.f;
    return {oh, ow, f};
  }

  /// Y =  X Conv W(sparse)  + PSUM
  template <bool FW = true, bool PSUM = true, typename Tx, typename Tw,
            typename Tpsum, typename Ty, bool SP>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  forward(const tensor_descriptor<Ty>         &y_desc,
          const tensor_descriptor<Tx, SP>     &x_desc,
          const filter_descriptor<Tw, SP>     &w_desc,
          const tensor_descriptor<Tpsum>      &psum_desc, 
          __L0Ashared__ void                  *y_data,
          __L0Ashared__ const void            *x_data, 
          __L0Wshared__ const void            *w_data,
          __L0Wshared__ const void            *w_sparse_index_data,
          __L0Ashared__ const void            *psum_data,
          uint32_t                             padding_value = 0) const {
    // clang-format on
    static_assert(is_valid_ne_prec_combine<ne_type_info<Tx>::ne_prec,
                                           ne_type_info<Tw>::ne_prec,
                                           ne_type_info<Tpsum>::ne_prec,
                                           ne_type_info<Ty>::ne_prec>(),
                  "Invalid tensor/filter type combination");

    static_assert(FW || !SP,
                  "Sparse with conv2d backward data is not supported");

    uint16_t right_pad = padding.right;
    uint16_t left_pad = padding.left;
    uint16_t bot_pad = padding.bottom;
    uint16_t top_pad = padding.top;
    if constexpr (!FW) {
      right_pad = dilation.x * (w_desc.kx - 1) - right_pad;
      left_pad = dilation.x * (w_desc.kx - 1) - left_pad;
      bot_pad = dilation.y * (w_desc.ky - 1) - bot_pad;
      top_pad = dilation.y * (w_desc.ky - 1) - top_pad;
    }

    constexpr uint64_t prec =
        NE_ENCODE_PREC(ne_type_info<Tx>::ne_prec, ne_type_info<Tw>::ne_prec,
                       ne_type_info<Ty>::ne_prec, ne_type_info<Tpsum>::ne_prec);
    uint64_t wcf = NE_ENCODE_MKN(x_desc.w, x_desc.c, y_desc.c);
    uint64_t h_oh_ow = CONV_ENCODE_H_OH_OW(x_desc.h, y_desc.h, y_desc.w);

    uint64_t swr_param0 =
        CONV_ENCODE_SWR_PARAM0(w_desc.kx, w_desc.ky, (FW ? (stride.x) : 1),
                               (FW ? (stride.y) : 1));

    uint64_t swr_param1 =
        CONV_ENCODE_SWR_PARAM1(dilation.x, dilation.y);
    uint64_t pad_param0 = CONV_ENCODE_PAD_PARAM0(right_pad, left_pad, bot_pad,
                                                 top_pad, 0, 0, 0, 0);
    uint64_t pad_param1 = CONV_ENCODE_PAD_PARAM1(padding_value, 0);
    uint64_t misc_param0 = CONV_ENCODE_MISC_PARAM0(
        (SP ? 1 : 0), (FW ? 0 : 1),
        ((dilation.x > 1 || dilation.y > 1) ? 1 : 0), (FW ? 0 : 1));
    uint64_t misc_param1 = CONV_ENCODE_MISC_PARAM1((FW ? 0 : (stride.x - 1)),
                                                   (FW ? 0 : (stride.y - 1)));

    uint32_t sparse_index_size = 0;
    uint32_t conv_param1 = 0;
    if constexpr (SP) {
      sparse_index_size = w_desc.get_aligned_indexes_size();
      conv_param1 = reinterpret_cast<uint64_t>(w_sparse_index_data);
    }

    // Config
    __builtin_riscv_ne_set_prec(0, 0, prec);
    __builtin_riscv_ne_set_mkn(0, 0, wcf);
    __builtin_riscv_ne_set_h_oh_ow(0, 0, h_oh_ow);
    __builtin_riscv_ne_set_conv_swr(0, swr_param1, swr_param0);
    __builtin_riscv_ne_set_conv_pad(0, pad_param1, pad_param0);
    __builtin_riscv_ne_set_misc(0, misc_param1, misc_param0);
    __builtin_riscv_ne_set_length(
        y_desc.get_aligned_size(),
        psum_desc.get_aligned_size() | ((int64_t)(sparse_index_size) << 32),
        x_desc.get_aligned_size() | ((int64_t)w_desc.get_aligned_size() << 32));

    // Conv
    uint64_t conv_param0 = (reinterpret_cast<uint64_t>(x_data) >> 1) |
                           ((reinterpret_cast<uint64_t>(w_data) >> 1) << 32);
    if constexpr (PSUM) {
      uint64_t conv_param2 =
          (reinterpret_cast<uint64_t>(y_data) >> 1) |
          ((reinterpret_cast<uint64_t>(psum_data) >> 1) << 32);
      __builtin_riscv_conv_psum(conv_param2, conv_param1, conv_param0);
    } else {
      uint64_t conv_param2 = reinterpret_cast<uint64_t>(y_data) >> 1;
      __builtin_riscv_conv(conv_param2, conv_param1, conv_param0);
    }
  }

  /// Y =  X Conv W(dense)  + PSUM
  template <typename Tx, typename Tw, typename Tpsum, typename Ty>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  forward(const tensor_descriptor<Ty>         &y_desc,
          const tensor_descriptor<Tx>         &x_desc,
          const filter_descriptor<Tw>         &w_desc,
          const tensor_descriptor<Tpsum>      &psum_desc, 
          __L0Ashared__ void                  *y_data,
          __L0Ashared__ const void            *x_data, 
          __L0Wshared__ const void            *w_data,
          __L0Ashared__ const void            *psum_data,
          uint32_t                             padding_value = 0) const {
    // clang-format on
    forward(y_desc, x_desc, w_desc, psum_desc, y_data, x_data, w_data, nullptr,
            psum_data, padding_value);
  }

  /// Y =  X Conv W(sparse)
  template <typename Tx, typename Tw, typename Ty, bool SP>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  forward(const tensor_descriptor<Ty>         &y_desc,
          const tensor_descriptor<Tx, SP>     &x_desc,
          const filter_descriptor<Tw, SP>     &w_desc,
          __L0Ashared__ void                  *y_data,
          __L0Ashared__ const void            *x_data, 
          __L0Wshared__ const void            *w_data,
          __L0Wshared__ const void            *w_sparse_index_data,
          uint32_t                             padding_value = 0) const {
    // clang-format on
    const tensor_descriptor<typename accumulator_type<Tx>::type> temp_psum = {
        y_desc.h, y_desc.w, y_desc.c};
    forward<true, false>(y_desc, x_desc, w_desc, temp_psum, y_data, x_data,
                         w_data, w_sparse_index_data, nullptr, padding_value);
  }

  /// Y =  X Conv W(dense)
  template <typename Tx, typename Tw, typename Ty>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  forward(const tensor_descriptor<Ty>         &y_desc,
          const tensor_descriptor<Tx>         &x_desc,
          const filter_descriptor<Tw>         &w_desc,
          __L0Ashared__ void                  *y_data,
          __L0Ashared__ const void            *x_data, 
          __L0Wshared__ const void            *w_data,
          uint32_t                             padding_value = 0) const {
    // clang-format on
    const tensor_descriptor<typename accumulator_type<Tx>::type> temp_psum = {
        y_desc.h, y_desc.w, y_desc.c};
    forward<true, false>(y_desc, x_desc, w_desc, temp_psum, y_data, x_data,
                         w_data, nullptr, nullptr, padding_value);
  }

  /// Compute the shape of dX for dY Conv W
  template <typename Tx, typename Ty, typename Tw>
  __RAISA_NE_HOST_DEVICE_DECL__ constexpr tensor_descriptor<Tx>
  calculate_backward_data_output_dim(
      const tensor_descriptor<Ty> &dy_desc,
      const filter_descriptor<Tw> &w_desc) const {
    uint16_t h = (dy_desc.h - 1) * stride.y - (padding.top + padding.bottom) +
                 dilation.y * (w_desc.ky - 1) + 1;
    uint16_t w = (dy_desc.w - 1) * stride.x - (padding.left + padding.right) +
                 dilation.x * (w_desc.kx - 1) + 1;
    uint16_t c = w_desc.c;
    return {h, w, c};
  }

  /// dX = dY Conv W(dense)  + PSUM
  template <typename Tx, typename Tw, typename Tpsum, typename Ty>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  backward_data(const tensor_descriptor<Tx>         &dx_desc,
                const tensor_descriptor<Ty>         &dy_desc,
                const filter_descriptor<Tw>         &w_desc,
                const tensor_descriptor<Tpsum>      &psum_desc, 
                __L0Ashared__ void                  *dx_data,
                __L0Ashared__ const void            *dy_data, 
                __L0Wshared__ const void            *w_data,
                __L0Ashared__ const void            *psum_data,
                uint32_t                             padding_value = 0) const {
    // clang-format on
    forward<false, false>(dx_desc, dy_desc, w_desc, psum_desc, dx_data, dy_data,
                          w_data, nullptr, psum_data, padding_value);
  }

  /// dX = dY Conv W
  template <typename Tx, typename Tw, typename Ty>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  backward_data(const tensor_descriptor<Tx>         &dx_desc, // [H,W,C]
                const tensor_descriptor<Ty>         &dy_desc, // [OH,OW,F]
                const filter_descriptor<Tw>         &w_desc,
                __L0Ashared__ void                  *dx_data,
                __L0Ashared__ const void            *dy_data, 
                __L0Wshared__ const void            *w_data,
                uint32_t                             padding_value = 0) const {
    // clang-format on
    const tensor_descriptor<typename accumulator_type<Ty>::type> temp_psum = {
        dx_desc.h, dx_desc.w, dx_desc.c};
    forward<false, false>(dx_desc, dy_desc, w_desc, temp_psum, dx_data, dy_data,
                          w_data, nullptr, nullptr, padding_value);
  }

  /// Compute the shape of dW for dY Conv X
  template <typename Tw, typename Ty, typename Tx>
  __RAISA_NE_HOST_DEVICE_DECL__ constexpr filter_descriptor<
      Tw, false, NE_TENSOR_LOCATION_L0A>
  calculate_backward_filter_output_dim(
      const tensor_descriptor<Tx, false, NE_TENSOR_LOCATION_L0W> &x_desc,
      const tensor_descriptor<Ty, false, NE_TENSOR_LOCATION_L0A> &dy_desc)
      const {
    uint16_t f = dy_desc.c;
    uint16_t ky = ((x_desc.h + padding.top + padding.bottom -
                    stride.y * (dy_desc.h - 1) - 1) /
                   dilation.y) +
                  1;
    uint16_t kx = ((x_desc.w + padding.left + padding.right -
                    stride.x * (dy_desc.w - 1) - 1) /
                   dilation.x) +
                  1;
    uint16_t c = x_desc.c;
    return {f, ky, kx, c};
  }

  /// dW =  dY Conv X(dense) + PSUM
  template <bool PSUM = true, typename Tw, typename Ty, typename Tx,
            typename Tpsum>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  backward_filter(const filter_descriptor<Tw,    false, NE_TENSOR_LOCATION_L0A> &dw_desc,
                  const tensor_descriptor<Ty,    false, NE_TENSOR_LOCATION_L0A> &dy_desc,
                  const tensor_descriptor<Tx,    false, NE_TENSOR_LOCATION_L0W> &x_desc,
                  const filter_descriptor<Tpsum, false, NE_TENSOR_LOCATION_L0A> &psum_desc, 
                  __L0Ashared__ void                                            *dw_data,
                  __L0Ashared__ const void                                      *dy_data,
                  __L0Wshared__ const void                                      *x_data, 
                  __L0Ashared__ const void                                      *psum_data,
                  uint32_t                                                       padding_value = 0) const {
    // clang-format on
    static_assert(is_valid_conv_bw_prec_combine<ne_type_info<Tx>::ne_prec, //
                                                ne_type_info<Ty>::ne_prec,
                                                ne_type_info<Tpsum>::ne_prec,
                                                ne_type_info<Tw>::ne_prec>(),
                  "Invalid tensor/filter type combination");

    constexpr uint64_t prec =
        NE_ENCODE_PREC(ne_type_info<Ty>::ne_prec, ne_type_info<Tx>::ne_prec,
                       ne_type_info<Tw>::ne_prec, ne_type_info<Tpsum>::ne_prec);

    uint64_t wcf = NE_ENCODE_MKN(x_desc.w, x_desc.c, dy_desc.c);
    uint64_t h_oh_ow = CONV_ENCODE_H_OH_OW(x_desc.h, dy_desc.h, dy_desc.w);
    uint64_t swr_param0 = CONV_ENCODE_SWR_PARAM0(
        dw_desc.kx, dw_desc.ky, dilation.x, dilation.y);
    uint64_t swr_param1 = CONV_ENCODE_SWR_PARAM1(stride.x, stride.y);
    uint64_t pad_param0 = CONV_ENCODE_PAD_PARAM0(
        padding.right, padding.left, padding.bottom, padding.top, 0, 0, 0, 0);
    uint64_t pad_param1 = CONV_ENCODE_PAD_PARAM1(padding_value, 0);
    uint64_t misc_param0 = CONV_ENCODE_MISC_PARAM0(
        0, 0, ((stride.x > 1 || stride.y > 1) ? 1 : 0), 0);

    // Config
    __builtin_riscv_ne_set_prec(0, 0, prec);
    __builtin_riscv_ne_set_mkn(0, 0, wcf);
    __builtin_riscv_ne_set_h_oh_ow(0, 0, h_oh_ow);
    __builtin_riscv_ne_set_conv_swr(0, swr_param1, swr_param0);
    __builtin_riscv_ne_set_conv_pad(0, pad_param1, pad_param0);
    __builtin_riscv_ne_set_misc(0, 0, misc_param0);
    __builtin_riscv_ne_set_length(
        dw_desc.get_aligned_size(), psum_desc.get_aligned_size(),
        dy_desc.get_aligned_size() | ((int64_t)x_desc.get_aligned_size() << 32));

    // ConvBW
    uint64_t conv_param0 = (reinterpret_cast<uint64_t>(dy_data) >> 1) |
                           ((reinterpret_cast<uint64_t>(x_data) >> 1) << 32);
    if constexpr (PSUM) {
      uint64_t conv_param2 =
          (reinterpret_cast<uint64_t>(dw_data) >> 1) |
          ((reinterpret_cast<uint64_t>(psum_data) >> 1) << 32);
      __builtin_riscv_convbw_psum(conv_param2, 0, conv_param0);
    } else {
      uint64_t conv_param2 = reinterpret_cast<uint64_t>(dw_data) >> 1;
      __builtin_riscv_convbw(conv_param2, 0, conv_param0);
    }
  }

  /// dW =  dY Conv X(dense)
  template <typename Tw, typename Ty, typename Tx>
  __RAISA_NE_DEVICE_DECL__ void
  // clang-format off
  backward_filter(const filter_descriptor<Tw, false, NE_TENSOR_LOCATION_L0A> &dw_desc,
                  const tensor_descriptor<Ty, false, NE_TENSOR_LOCATION_L0A> &dy_desc,
                  const tensor_descriptor<Tx, false, NE_TENSOR_LOCATION_L0W> &x_desc,
                  __L0Ashared__ void                                         *dw_data,
                  __L0Ashared__ const void                                   *dy_data,
                  __L0Wshared__ const void                                   *x_data, 
                  uint32_t                                                    padding_value = 0) const {
    // clang-format on
    const filter_descriptor<typename accumulator_type<Tx>::type, false,
                            NE_TENSOR_LOCATION_L0A>
        temp_psum = {dw_desc.f, dw_desc.ky, dw_desc.kx, dw_desc.c};
    backward_filter<false>(dw_desc, dy_desc, x_desc, temp_psum, dw_data, dy_data, x_data,
                           nullptr, padding_value);
  }
};

} // namespace conv
} // namespace raisa

#endif