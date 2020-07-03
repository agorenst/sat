#include "vsids.h"
#include <cassert>

vsids_t::vsids_t(const cnf_t& cnf, const trail_t& trail)
    : vars(cnf.var_range()), trail(trail) {
  activity.construct(max_variable(cnf));
  std::fill(std::begin(activity), std::end(activity), 0.0);
}

//__attribute__((noinline))
void vsids_t::clause_learned(const clause_t& c) {
  for (literal_t l : c) {
    activity[var(l)] += bump;
  }
  std::for_each(std::begin(activity), std::end(activity),
                [this](float& s) { s *= alpha; });
}

//__attribute__((noinline))
literal_t vsids_t::choose() const {
  variable_t c = 0;
  float a = -1;
  // std::cout << "[VSIDS][TRACE][VERBOSE] Activity: ";
  // for (literal_t l = - activity.max_var; l <= activity.max_var; l++) {
  // if (l == 0) continue;
  // std::cout << l << ": " << activity[l] << "; ";
  //}
  // std::cout << std::endl;
  for (variable_t v : vars) {
    if (trail.varset.get(v)) continue;
    // std::cout << "[VSIDS][TRACE][VERBOSE] Trying " << l << std::endl;
    if (activity[v] >= a) {
      a = activity[v];
      c = v;
    }
  }
  if (!c) return 0;
  return trail.previously_assigned_literal(c);
  // return (c << 1) + 1;
}

float vsids_t::score(literal_t l) const { return activity[var(l)]; }
