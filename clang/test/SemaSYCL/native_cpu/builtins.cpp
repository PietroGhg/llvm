// RUN: %clangxx -fsycl-device-only  -D __SYCL_NATIVE_CPU__ -mllvm -sycl-native-cpu -Xclang -fsycl-native-cpu-header=%t-hc.h -sycl-std=2020 -mllvm -sycl-opt -S -emit-llvm  -o - %s | FileCheck %s

#include "sycl.hpp"
class Test1;
class Test2;
class Test3;
int main() {
  sycl::queue deviceQueue;
  sycl::accessor<int, 1, sycl::access::mode::write> acc;
  sycl::range<1> r(1);
  deviceQueue.submit([&](sycl::handler& h){
        
        h.parallel_for<Test1>(r, [=](sycl::id<1> id){
             acc[id[0]] = 42;
            });
      });
  sycl::nd_range<2> r2({1,1},{1,1,});
  deviceQueue.submit([&](sycl::handler& h){
        
        h.parallel_for<Test2>(r2, [=](sycl::id<2> id){
             acc[id[1]] = 42;
            });
      });
  sycl::nd_range<2> r3({1,1},{1,1,},{1,1});
  deviceQueue.submit([&](sycl::handler& h){
        
        h.parallel_for<Test3>(r3, [=](sycl::id<3> id){
             acc[id[2]] = 42;
            });
      });
}

// check that we added the state struct as a function argument, and that we inject the calls to 
// our builtins.
// We disable index flipping for SYCL Native CPU, so id.get_global_id(1) maps to 
// dimension 1 for a 2-D kernel (as opposed to dim 0), etc

// CHECK: @_Z5Test1(i32 addrspace(1)* %0, %"class.sycl::_V1::id"* %1, %struct._hc_state* %2)
// CHECK: call{{.*}}_Z16hc_get_global_idmP9_hc_state(i64 0, %struct._hc_state* %2)

// CHECK: @_Z5Test2(i32 addrspace(1)* %0, %"class.sycl::_V1::id"* %1, %struct._hc_state* %2)
// CHECK: call{{.*}}_Z16hc_get_global_idmP9_hc_state(i64 1, %struct._hc_state* %2)

// CHECK: @_Z5Test3(i32 addrspace(1)* %0, %"class.sycl::_V1::id"* %1, %struct._hc_state* %2)
// CHECK: call{{.*}}_Z16hc_get_global_idmP9_hc_state(i64 2, %struct._hc_state* %2)
