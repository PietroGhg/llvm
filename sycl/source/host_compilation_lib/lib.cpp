#include <sycl/detail/host_compilation_helpers.hpp>

extern "C" size_t _hc_get_global_id(unsigned n, _hc_state *s) {
  return s->MGlobal_id[n];
}
