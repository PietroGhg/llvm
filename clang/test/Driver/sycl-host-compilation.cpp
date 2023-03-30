// RUN: %clangxx -fsycl-device-only -fsycl-host-compilation %s -### 2>&1 | FileCheck %s


// checks that the host and device triple are the same, and that the sycl-host-compilation LLVM option is set
// CHECK: clang{{.*}}"-triple" "[[TRIPLE:.*]]"{{.*}}"-aux-triple" "[[TRIPLE]]"{{.*}}"-mllvm" "-sycl-host-compilation"{{.*}}"-D" "__SYCL_HOST_COMPILATION__"
