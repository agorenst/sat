#include "trail.h"

void trail_t::construct(size_t max_var) {
  size = max_var+1;
  next_index = 0;
  dlevel = 0;
  mem = std::make_unique<action_t[]>(size);
  varset = std::make_unique<bool[]>(size);
  varlevel = std::make_unique<size_t[]>(size);
}
