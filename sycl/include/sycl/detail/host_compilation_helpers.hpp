#include <cstdio>

extern "C" struct _hc_state {
  size_t MGlobal_id[3];
  _hc_state() {
    MGlobal_id[0] = 0;
    MGlobal_id[1] = 0;
    MGlobal_id[2] = 0;
  }
};
