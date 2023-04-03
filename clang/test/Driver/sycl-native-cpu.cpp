// RUN: %clangxx -fsycl-device-only -fsycl-native-cpu %s -### 2>&1 | FileCheck %s


// checks that the host and device triple are the same, and that the sycl-host-compilation LLVM option is set
// CHECK: clang{{.*}}"-triple" "[[TRIPLE:.*]]"{{.*}}"-aux-triple" "[[TRIPLE]]"{{.*}}"-mllvm" "-sycl-native-cpu"{{.*}}"-D" "__SYCL_NATIVE_CPU__"
