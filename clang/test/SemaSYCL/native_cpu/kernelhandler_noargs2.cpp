// cmdline that used to fail in kernel handler emission
// RUN: %clangxx -fsycl-device-only  -D __SYCL_NATIVE_CPU__ -mllvm -sycl-native-cpu -Xclang -fsycl-native-cpu-header=%t-hc.h -sycl-std=2020 -mllvm -sycl-opt -S -emit-llvm  -o - %s
// RUN: FileCheck -input-file=%t-hc.h %s --check-prefix=CHECK-HC
// RUN: %clangxx -fsycl -c -x c++ %t-hc.h
#include "sycl.hpp"

template <typename name, typename Func>
__attribute__((sycl_kernel)) void launch(const Func &kernelFunc) {
  kernelFunc();
}
int main() {
  launch<class TestKernel>([]() {  });
  return 0;
}

//CHECK-HC: #pragma once
//CHECK-HC-NEXT: #include <sycl/detail/native_cpu.hpp>
//CHECK-HC:extern "C" void _ZZ4mainE10TestKernel();
//CHECK-HC:inline static void _ZZ4mainE10TestKernelsubhandler(const std::vector<sycl::detail::NativeCPUArgDesc>& MArgs, _hc_state *state) {
//CHECK-HC-NEXT:  _ZZ4mainE10TestKernel();
//CHECK-HC-NEXT:};