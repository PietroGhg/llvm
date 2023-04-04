#include "clang/Basic/LangOptions.h"
#include <string>
namespace clang {
inline std::string getNativeCPUHeaderName(const LangOptions &LangOpts) {
  std::string HCName = LangOpts.SYCLNativeCPUHeader;
  if (HCName == "")
    HCName = LangOpts.SYCLIntHeader + ".hc";
  return HCName;
}
} // namespace clang
