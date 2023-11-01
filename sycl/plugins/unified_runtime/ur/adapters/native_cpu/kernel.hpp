//===--------------- kernel.hpp - Native CPU Adapter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common.hpp"
#include <sycl/detail/native_cpu.hpp>
#include <ur_api.h>

using nativecpu_kernel_t = void(const sycl::detail::NativeCPUArgDesc *,
                                __nativecpu_state *);
using nativecpu_ptr_t = nativecpu_kernel_t *;
using nativecpu_task_t = std::function<nativecpu_kernel_t>;

struct local_arg_info_t {
  uint32_t argIndex;
  size_t argSize;
  local_arg_info_t(uint32_t argIndex, size_t argSize)
      : argIndex(argIndex), argSize(argSize) {}
};

struct ur_kernel_handle_t_ : RefCounted {

  ur_kernel_handle_t_(const char *name, nativecpu_task_t subhandler)
      : _name{name}, _subhandler{subhandler} {}

  const char *_name;
  nativecpu_task_t _subhandler;
  std::vector<sycl::detail::NativeCPUArgDesc> _args;
  std::vector<local_arg_info_t> _localArgInfo;

  // To be called before enqueing the kernel.
  void updateMemPool(size_t numParallelThreads) {
    // compute requested size.
    size_t reqSize = 0;
    for (auto &entry : _localArgInfo) {
      reqSize += entry.argSize * numParallelThreads;
    }
    if (reqSize == 0 || reqSize == _localMemPoolSize) {
      return;
    }
    // realloc handles nullptr case
    _localMemPool = realloc(_localMemPool, reqSize);
    _localMemPoolSize = reqSize;
  }

  // To be called before executing a work group
  void handleLocalArgs(size_t numParallelThread, size_t threadId) {
    // For each local argument we have size*numthreads
    size_t offset = 0;
    for (auto &entry : _localArgInfo) {
      _args[entry.argIndex].MPtr =
          reinterpret_cast<char *>(_localMemPool) + offset + (entry.argSize * threadId);
      // update offset in the memory pool
      offset += entry.argSize * numParallelThread;
    }
  }

  ~ur_kernel_handle_t_() {
    if (_localMemPool) {
      free(_localMemPool);
    }
  }

private:
  void *_localMemPool = nullptr;
  size_t _localMemPoolSize = 0;
};
