// RUN: %clangxx -fsycl-device-only  -fsycl-host-compilation -Xclang -fsycl-int-header=%t.h -Xclang -fsycl-hc-header=%t-hc.h -o %t.bc %s 
// RUN: FileCheck -input-file=%t-hc.h %s 
// Compiling generated main integration header to check correctness, -fsycl option used to find required includes
// RUN: %clangxx -fsycl -c -x c++ %t.h
#include <CL/sycl.hpp>

#include <iostream>

using namespace cl::sycl;

const size_t N = 10;

template <typename T>
class init_a;

template <typename T>
bool test(queue myQueue) {
  {
    buffer<float, 1> a(range<1>{N});
    T test = 42;

    myQueue.submit([&](handler& cgh) {
      auto A = a.get_access<access::mode::write>(cgh);
      cgh.parallel_for<init_a<T>>(range<1>{N}, [=](id<1> index) {
        A[index] = test;
      });
    });

    auto A = a.get_access<access::mode::read>();
    std::cout << "Result:" << std::endl;
    for (size_t i = 0; i < N; i++) {
        if (A[i] != test) {
          std::cout << "ERROR\n";
          return false;
        }
    }
  }

  std::cout << "Good computation!" << std::endl;
  return true;
}

int main() {
  queue q;
  int res1 = test<int>(q);
  int res2 = test<unsigned>(q);
  int res3 = test<float>(q);
  int res4 = test<double>(q);
  if(!(res1 && res2 && res3 && res4)) {
    return 1;
  }
  return 0;
}



// CHECK:extern "C" void _Z6init_aIiE(void *, void *, int, _hc_state *);
// CHECK:inline static void _Z6init_aIiEsubhandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state *state) {
// CHECK-NEXT:  void* arg0 = MArgs[0].getPtr();
// CHECK-NEXT:  void* arg3 = MArgs[3].getPtr();
// CHECK-NEXT:  int arg4 = *(int*)MArgs[4].getPtr();
// CHECK-NEXT:  _Z6init_aIiE(arg0, arg3, arg4, state);
// CHECK-NEXT:};

// CHECK:extern "C" void _Z6init_aIjE(void *, void *, unsigned int, _hc_state *);
// CHECK:inline static void _Z6init_aIjEsubhandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state *state) {
// CHECK-NEXT:  void* arg0 = MArgs[0].getPtr();
// CHECK-NEXT:  void* arg3 = MArgs[3].getPtr();
// CHECK-NEXT:  unsigned int arg4 = *(unsigned int*)MArgs[4].getPtr();
// CHECK-NEXT:  _Z6init_aIjE(arg0, arg3, arg4, state);
// CHECK-NEXT:};

// CHECK:extern "C" void _Z6init_aIfE(void *, void *, float, _hc_state *);
// CHECK:inline static void _Z6init_aIfEsubhandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state *state) {
// CHECK-NEXT:  void* arg0 = MArgs[0].getPtr();
// CHECK-NEXT:  void* arg3 = MArgs[3].getPtr();
// CHECK-NEXT:  float arg4 = *(float*)MArgs[4].getPtr();
// CHECK-NEXT:  _Z6init_aIfE(arg0, arg3, arg4, state);
// CHECK-NEXT:};

// CHECK:extern "C" void _Z6init_aIdE(void *, void *, double, _hc_state *);
// CHECK:inline static void _Z6init_aIdEsubhandler(const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, _hc_state *state) {
// CHECK-NEXT:  void* arg0 = MArgs[0].getPtr();
// CHECK-NEXT:  void* arg3 = MArgs[3].getPtr();
// CHECK-NEXT:  double arg4 = *(double*)MArgs[4].getPtr();
// CHECK-NEXT:  _Z6init_aIdE(arg0, arg3, arg4, state);
// CHECK-NEXT:};

