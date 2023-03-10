#include "detail/accessor_impl.hpp"
#include "detail/handler_impl.hpp"
#include <iostream>
#include <sycl/detail/host_compilation.hpp>

namespace sycl {
__SYCL_INLINE_VER_NAMESPACE(_V1) {
namespace detail {

HostCompilationArgDesc::HostCompilationArgDesc(const ArgDesc &Arg) {
  if (Arg.MType == kernel_param_kind_t::kind_accessor) {
    auto HostAcc = static_cast<AccessorImplHost *>(Arg.MPtr);
    MPtr = HostAcc->MData;
  } else
    MPtr = Arg.MPtr;
}

std::vector<HostCompilationArgDesc>
processArgsForHostCompilation(const std::vector<ArgDesc> &MArgs) {
  std::vector<HostCompilationArgDesc> res;
  for (auto &arg : MArgs) {
    res.emplace_back(arg);
  }
  return res;
}

void setHostCompilationImpl(std::shared_ptr<handler_impl> &MImpl,
                            std::shared_ptr<HCTask_t> &task) {
  MImpl->MHostCompilationFunct = task;
}

} // namespace detail
} // __SYCL_INLINE_VER_NAMESPACE(_V1)
} // namespace sycl
