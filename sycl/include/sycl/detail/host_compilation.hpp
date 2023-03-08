#pragma once
#include "cg_types.hpp"
#include <functional>
#include <vector>
namespace sycl {
__SYCL_INLINE_VER_NAMESPACE(_V1) {
namespace detail {

using HCTask_t = std::function<void(NDRDescT)>;

class HCTask : public HostKernelBase {
public:
  HCTask(HCTask_t Task) : MTask(Task) {}
  void call(const NDRDescT &NDRDesc, HostProfilingInfo *HPI) override {
    MTask(NDRDesc);
  }
  // Return pointer to the lambda object.
  // Used to extract captured variables.
  char *getPtr() override {
    assert(false && "getPtr called on Host Compilation task");
    return nullptr;
  }

private:
  HCTask_t MTask;
};

class __SYCL_EXPORT HostCompilationArgDesc {
  void *MPtr;

public:
  void *getPtr() const { return MPtr; }
  HostCompilationArgDesc(const ArgDesc &ArgDesc);
};

__SYCL_EXPORT
std::vector<HostCompilationArgDesc>
processArgsForHostCompilation(const std::vector<ArgDesc> &MArgs);

// Helper class to determine wheter or not the KernelInfo struct has 
// the is_host_compilation field, and if it is true or false.
template <typename T, class Enable = void>
struct IsHostCompilation {
  static constexpr bool value = false;
};

template <typename T>
struct IsHostCompilation<T, typename std::enable_if<T::is_host_compilation>::type> {
  static constexpr bool value = T::is_host_compilation;
};

} // namespace detail
} // __SYCL_INLINE_VER_NAMESPACE(_V1)
} // namespace sycl

extern "C" struct _hc_state {
  size_t MGlobal_id[3];
  _hc_state() {
    MGlobal_id[0] = 0;
    MGlobal_id[1] = 0;
    MGlobal_id[2] = 0;
  }
};

#ifdef __SYCL_HOST_COMPILATION__
#ifdef __SYCL_DEVICE_ONLY__
#define __SYCL_HC_ATTRS                                                        \
  __attribute__((weak)) __attribute((alwaysinline))                            \
      [[intel::device_indirectly_callable]]
extern "C" __SYCL_HC_ATTRS size_t _Z16hc_get_global_idmP9_hc_state(
    size_t n, __attribute((address_space(0))) _hc_state *s) {
  return s->MGlobal_id[n];
}
#undef __SYCL_HC_ATTRS
#endif
#endif
