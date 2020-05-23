#include "trail.h"

void trail_t::construct(size_t max_var) {
  size = max_var+1;
  mem = std::make_unique<action_t[]>(size);
  varset = std::make_unique<bool[]>(size);
  next_index = 0;
}
