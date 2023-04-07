#pragma once
#include "cg_types.hpp"
#include <functional>
#include <memory>
#include <vector>
namespace sycl {
__SYCL_INLINE_VER_NAMESPACE(_V1) {
namespace detail {

using NativeCPUTask_t = std::function<void(NDRDescT)>;

class NativeCPUTask : public HostKernelBase {
public:
  NativeCPUTask(std::shared_ptr<NativeCPUTask_t> Task) : MTask(Task) {}
  void call(const NDRDescT &NDRDesc, HostProfilingInfo *HPI) override {
    (*MTask)(NDRDesc);
  }
  // Return pointer to the lambda object.
  // Used to extract captured variables.
  char *getPtr() override {
    assert(false && "getPtr called on Native CPU task");
    return nullptr;
  }

private:
  std::shared_ptr<NativeCPUTask_t> MTask;
};

class __SYCL_EXPORT NativeCPUArgDesc {
  void *MPtr;

public:
  void *getPtr() const { return MPtr; }
  NativeCPUArgDesc(const ArgDesc &ArgDesc);
};

__SYCL_EXPORT
std::vector<NativeCPUArgDesc>
processArgsForNativeCPU(const std::vector<ArgDesc> &MArgs);

// Helper class to determine wheter or not the KernelInfo struct has
// the is_native_cpu field, and if it is true or false.
template <typename T, class Enable = void> struct is_native_cpu {
  static constexpr bool value = false;
};

template <typename T>
struct is_native_cpu<T, typename std::enable_if_t<T::is_native_cpu>> {
  static constexpr bool value = T::is_native_cpu;
};

template <typename T>
inline constexpr bool is_native_cpu_v = is_native_cpu<T>::value;

} // namespace detail
} // __SYCL_INLINE_VER_NAMESPACE(_V1)
} // namespace sycl

struct nativecpu_state {
  size_t MGlobal_id[3];
  nativecpu_state() {
    MGlobal_id[0] = 0;
    MGlobal_id[1] = 0;
    MGlobal_id[2] = 0;
  }
};

#ifdef __SYCL_NATIVE_CPU__
#ifdef __SYCL_DEVICE_ONLY__
#define __SYCL_HC_ATTRS                                                        \
  __attribute__((weak)) __attribute((alwaysinline))                            \
      [[intel::device_indirectly_callable]]
extern "C" __SYCL_HC_ATTRS size_t _Z13get_global_idmP15nativecpu_state(
    size_t n, __attribute((address_space(0))) nativecpu_state *s) {
  return s->MGlobal_id[n];
}
#undef __SYCL_HC_ATTRS
#endif
#endif
