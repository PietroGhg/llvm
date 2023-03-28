// RUN: %clangxx -fsycl -fsycl-host-compilation %s -o %t
// RUN: env ONEAPI_DEVICE_SELECTOR="host:*" %t
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
