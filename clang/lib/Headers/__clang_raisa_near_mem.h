/*===---- __clang_raisa_near_mem.h - RAISA Near Memory --------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __CLANG_RAISA_NEAR_MEM_H__
#define __CLANG_RAISA_NEAR_MEM_H__

#include <cstdint>
#include <cassert>
#include "__clang_raisa_cooperative_groups.h"


#ifndef __RAISA_DEVICE_DECL__
#define __RAISA_DEVICE_DECL__                                                   \
  __device__ __attribute__((always_inline)) __attribute__((no_vector))
#endif

namespace raisa {

// Allocate per-tile address slots: numDies * tilesPerDie int64_t entries.
// `txDeviceLocalMalloc(near.tileAddrs(), bytesPerTile,
//                       near.startDie(), near.numDies())`
// fills the slots; `tileAddrs()` / `tileAddr(i)` then read them back.
class NearMemory {
public:
  NearMemory(int32_t startDie, int32_t numDies, int32_t tilesPerDie = 8)
      : StartDie_(startDie), NumDies_(numDies), TilesPerDie_(tilesPerDie),
        NumTiles_(numDies * tilesPerDie),
        Arr_(NumTiles_ > 0 ? new int64_t[NumTiles_]() : nullptr) {}

  ~NearMemory() { delete[] Arr_; }

  NearMemory(const NearMemory &) = delete;
  NearMemory &operator=(const NearMemory &) = delete;

  NearMemory(NearMemory &&O) noexcept
      : StartDie_(O.StartDie_), NumDies_(O.NumDies_),
        TilesPerDie_(O.TilesPerDie_), NumTiles_(O.NumTiles_), Arr_(O.Arr_) {
    O.Arr_ = nullptr;
    O.NumTiles_ = 0;
  }
  NearMemory &operator=(NearMemory &&O) noexcept {
    if (this != &O) {
      delete[] Arr_;
      StartDie_ = O.StartDie_;
      NumDies_ = O.NumDies_;
      TilesPerDie_ = O.TilesPerDie_;
      NumTiles_ = O.NumTiles_;
      Arr_ = O.Arr_;
      O.Arr_ = nullptr;
      O.NumTiles_ = 0;
    }
    return *this;
  }

  // Raw pointer access (advanced, no bounds check). Pass to
  // txDeviceLocalMalloc / Free as `addrPtr`, or to kernels as the
  // `int64_t* NearAddr` argument.
  int64_t *tileAddrs() { return Arr_; }
  const int64_t *tileAddrs() const { return Arr_; }

  // Named single-tile read (bounds-checked). Returns the EL1 address that
  // runtime wrote at slot `i`. Out-of-range / uninitialized: debug assert,
  // release returns 0. Returns 0 before `txDeviceLocalMalloc` has been
  // called (zero-initialized at construction).
  int64_t tileAddr(int32_t I) const {
    assert(I >= 0 && I < NumTiles_ && Arr_ != nullptr &&
           "raisa::NearMemory::tileAddr index out of range");
    if (I < 0 || I >= NumTiles_ || Arr_ == nullptr)
      return 0;
    return Arr_[I];
  }

  int32_t startDie() const { return StartDie_; }       // == RPCID
  int32_t numDies() const { return NumDies_; }         // == RPCNum
  int32_t tilesPerDie() const { return TilesPerDie_; }
  int32_t numTiles() const { return NumTiles_; }       // numDies*tilesPerDie()

private:
  int32_t StartDie_;
  int32_t NumDies_;
  int32_t TilesPerDie_;
  int32_t NumTiles_;
  int64_t *Arr_;
};

// Kernel-side helpers. Encode the (cluster_rank, block_rank) -> tile mapping
// in one place.
//
// Only meaningful inside a kernel decorated with __cluster_dims__(.).
__RAISA_DEVICE_DECL__ inline int32_t this_near_index() {
  namespace cg = cooperative_groups;
  cg::grid_group Grid = cg::this_grid();
  cg::cluster_group Cluster = cg::this_cluster();
  return Grid.cluster_rank() * Cluster.num_blocks() + Cluster.block_rank();
}

__RAISA_DEVICE_DECL__ inline int64_t
this_near_addr(const int64_t *TileAddrs) {
  // The host-supplied tileAddrs[] lives in device-accessible global memory
  // (raisa_gmem, AS=1 on RISCV).  Re-tag the pointer here so the i64 load is
  // analyzed as uniform (kernel-arg + gmem load).
  typedef const int64_t __attribute__((address_space(1))) *__GmemTileAddrsPtr;
  __GmemTileAddrsPtr Gmem = (__GmemTileAddrsPtr)TileAddrs;
  return Gmem[this_near_index()];
}

template <typename T>
__RAISA_DEVICE_DECL__ inline T *
this_near(const int64_t *TileAddrs) {
  return reinterpret_cast<T *>(this_near_addr(TileAddrs));
}

} // namespace raisa

#endif // __CLANG_RAISA_NEAR_MEM_H__
