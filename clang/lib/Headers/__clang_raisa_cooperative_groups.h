/*===---- __clang_raisa_cooperative_groups.h - cluster cooperative --------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */
#ifndef __CLANG_RAISA_COOPERATIVE_GROUPS_H__
#define __CLANG_RAISA_COOPERATIVE_GROUPS_H__

#define __RAISA_DEVICE_DECL__                                           \
  __device__ __attribute__((always_inline)) __attribute__((no_vector))

namespace cooperative_groups {

class grid_group{
  friend grid_group this_grid();

private:
  grid_group() = default;

  __RAISA_DEVICE_DECL__ dim3 get_block_idx_impl() const {
    return dim3(__get_pid(0), __get_pid(1), __get_pid(2));
  }

  __RAISA_DEVICE_DECL__ dim3 get_grid_idx_impl() const {
    return dim3(__grid_dim_x(), __grid_dim_y(), __grid_dim_z());
  }

public:
  // 3-Dimensions index of the block within the launched grid
  __RAISA_DEVICE_DECL__ dim3 block_index() {
    return get_block_idx_impl();
  }

  // Rank of the calling block within [0, num_blocks)
  __RAISA_DEVICE_DECL__ int block_rank() {
    auto blockIdx = get_block_idx_impl();

    auto gridDim = get_grid_idx_impl();

    return blockIdx.x + blockIdx.y * gridDim.x +
           blockIdx.z * gridDim.x * gridDim.y;
  }

  // Total number of blocks in the group
  __RAISA_DEVICE_DECL__ int num_blocks() {
    auto girdDim = get_grid_idx_impl();
    return gridDim.x * gridDim.y * gridDim.z;
  }

  // 3-Dimensional index of the cluster within the launched grid
  __RAISA_DEVICE_DECL__ dim3 cluster_index() {
    dim3_t idx = krt_grid_cluster_index();
    return dim3{idx.x, idx.y, idx.z};
  }

  // Rank of the calling cluster within [0, num_cluster)
  __RAISA_DEVICE_DECL__ int cluster_rank() {
    return krt_grid_cluster_rank();
  }

  // Total number of clusters in the group
  __RAISA_DEVICE_DECL__ int num_cluster() {
    return krt_grid_num_clusters();
  }

  __RAISA_DEVICE_DECL__ void sync() {
    // assert(gridDim.x * gridDim.y * gridDim.z == 32 
    //        && "block_num must be 32 when calling sync in grid_group");
    krt_barrier_in_chip();
  }
};

__RAISA_DEVICE_DECL__ inline grid_group this_grid() {
  return grid_group{};
}

class cluster_group{
  friend cluster_group this_cluster();

private:
  cluster_group() = default;

public:
  // sync all blocks in the same cluster
  __RAISA_DEVICE_DECL__ static void sync() {
    return krt_cluster_sync();
  };

  // 3-Dimensional index of the calling block within the launched cluster
  __RAISA_DEVICE_DECL__ dim3 block_index() {
    dim3_t idx = krt_cluster_block_index();
    return dim3{idx.x, idx.y, idx.z};
  };

  // Rank of the calling block within [0, num_blocks)
  __RAISA_DEVICE_DECL__ int block_rank() {
    return krt_cluster_block_rank();
  };

  // Total number of blocks in the group
  __RAISA_DEVICE_DECL__ int num_blocks() {
    return krt_cluster_num_blocks();
  };

  // Send data to remote block
  __RAISA_DEVICE_DECL__ void write_shared_memory(uint64_t src_addr, uint64_t dst_addr,
                                                int32_t remote_block, int32_t data_size) {
    return krt_cluster_write_shared_memory(src_addr, dst_addr, remote_block, data_size);
  };

  // Send data to remote block
  __RAISA_DEVICE_DECL__ void send(int32_t remote_blockid,
                                 uint64_t src_addr,
                                 int32_t data_size) {
    krt_cluster_send(remote_blockid, src_addr, data_size);
    krt_cluster_wait_send(remote_blockid, src_addr, data_size);
    return;
  };

  // Receiv data from remote block
  __RAISA_DEVICE_DECL__ void recv(int32_t remote_blockid,
                                 uint64_t dst_addr,
                                 int32_t data_size) {
    krt_cluster_recv(remote_blockid, dst_addr, data_size);
    krt_cluster_wait_recv(remote_blockid, dst_addr, data_size);
    return;
  };

  // send semaphore to remote block to wait sync
  __RAISA_DEVICE_DECL__ void* barrier_arrival() {
    return krt_cluster_barrier_arrival();
  };

  // recv remote block to sync
  __RAISA_DEVICE_DECL__ void barrier_wait(void* token) {
    krt_cluster_barrier_wait(token);
  };
};

__RAISA_DEVICE_DECL__ inline cluster_group this_cluster() {
  return cluster_group{};
}

} // namespace cooperative_groups

#endif