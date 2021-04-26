// This is for my experiments seeing what SAT solving "looks like" when solving
// PM-to-SAT reductions. We know for such problems that any solution must have
// all variables be true.
#include "positive_only_literal_chooser.h"

positive_only_literal_chooser_t::positive_only_literal_chooser_t(
    variable_t max_var, const trail_t& trail)
    : trail(trail) {
  for (variable_t v : variable_range(max_var)) {
    vars.push_back(v);
  }
}

literal_t positive_only_literal_chooser_t::choose() const {
  for (variable_t v : vars) {
    if (trail.varset.get(v)) continue;
    return lit(v);
  }
  return 0;
}