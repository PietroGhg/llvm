#pragma once
#include "cg_types.hpp"
#include "host_compilation_helpers.hpp"
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

} // namespace detail
} // __SYCL_INLINE_VER_NAMESPACE(_V1)
} // namespace sycl
