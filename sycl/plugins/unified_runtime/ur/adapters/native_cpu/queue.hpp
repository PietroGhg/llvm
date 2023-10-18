//===----------- queue.hpp - Native CPU Adapter ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once
#include "threadpool.hpp"

struct ur_queue_handle_t_ {
  native_cpu::threadpool_t tp;

  ur_queue_handle_t_() {
    tp.start();
  }
  
  ~ur_queue_handle_t_() {
    tp.stop();
  }
};
