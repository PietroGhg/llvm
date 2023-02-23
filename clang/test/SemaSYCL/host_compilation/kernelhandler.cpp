// RUN: %clang_cc1 -fsycl-is-device -Wno-int-to-void-pointer-cast -internal-isystem %S/../Inputs -mllvm -sycl-host-compilation -fsycl-int-header=%t.h -mllvm -hc-header=%t-hc.h -sycl-std=2020 -emit-llvm-bc -mllvm -sycl-opt -fenable-sycl-dae -o %t.bc %s 
// RUN: %clang_cc1 -fsycl-is-device -Wno-int-to-void-pointer-cast -internal-isystem %S/../Inputs -mllvm -sycl-host-compilation -fsycl-int-header=%t-no-dae.h -mllvm -hc-header=%t-no-dae-hc.h -sycl-std=2020 -emit-llvm-bc -mllvm -sycl-opt -o %t.bc %s 
// RUN: FileCheck -input-file=%t.h %s --check-prefix=CHECK-H
// RUN: FileCheck -input-file=%t-hc.h %s --check-prefix=CHECK-HC
// RUN: FileCheck -input-file=%t-no-dae.h %s --check-prefix=CHECK-NO-DAE-H
// RUN: FileCheck -input-file=%t-no-dae-hc.h %s --check-prefix=CHECK-NO-DAE-HC

#include "sycl.hpp"
class Test1;
int main() {
  sycl::queue deviceQueue;
  sycl::accessor<int, 1, sycl::access::mode::write> acc;
  deviceQueue.submit([&](sycl::handler& h){
        
        h.parallel_for<Test1>([=](){
             acc.use(0);
            });
      });
}


// CHECK-H: extern "C" void _Z5Test1subhandler( const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state*);
// CHECK-H: template <> struct KernelInfo<::Test1> {
// CHECK-H-NEXT:   static constexpr bool is_host_compilation = 1;
// CHECK-H-NEXT:   static void HCKernelHandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state* s) {
// CHECK-H-NEXT:     _Z5Test1subhandler(MArgs, s);
// CHECK-H-NEXT:   }



//CHECK-HC: #pragma once
//CHECK-HC-NEXT: #include <sycl/detail/host_compilation.hpp>
//CHECK-HC:extern "C" void _Z5Test1(void *, _hc_state*)
//CHECK-HC:extern "C" void _Z5Test1subhandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state *state) {
//CHECK-HC-NEXT:  void* ptr0 = MArgs[0].getPtr();
//CHECK-HC-NEXT:  _Z5Test1(ptr0, state);
//CHECK-HC-NEXT:};


// CHECK-NO-DAE-H: extern "C" void _Z5Test1subhandler( const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state*);
// CHECK-NO-DAE-H: template <> struct KernelInfo<::Test1> {
// CHECK-NO-DAE-H-NEXT:   static constexpr bool is_host_compilation = 1;
// CHECK-NO-DAE-H-NEXT:   static void HCKernelHandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state* s) {
// CHECK-NO-DAE-H-NEXT:     _Z5Test1subhandler(MArgs, s);
// CHECK-NO-DAE-H-NEXT:   }


//CHECK-NO-DAE-HC: #pragma once
//CHECK-NO-DAE-HC-NEXT: #include <sycl/detail/host_compilation.hpp>
//CHECK-NO-DAE-HC:extern "C" void _Z5Test1(void *, void *, void *, void *, void *, _hc_state*);
//CHECK-NO-DAE-HC:extern "C" void _Z5Test1subhandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state *state) {
//CHECK-NO-DAE-HC-NEXT:  void* ptr0 = MArgs[0].getPtr();
//CHECK-NO-DAE-HC-NEXT:  void* ptr1 = MArgs[1].getPtr();
//CHECK-NO-DAE-HC-NEXT:  void* ptr2 = MArgs[2].getPtr();
//CHECK-NO-DAE-HC-NEXT:  void* ptr3 = MArgs[3].getPtr();
//CHECK-NO-DAE-HC-NEXT:  void* ptr4 = MArgs[4].getPtr();
//CHECK-NO-DAE-HC-NEXT:  _Z5Test1(ptr0, ptr1, ptr2, ptr3, ptr4, state);
//CHECK-NO-DAE-HC-NEXT:};
