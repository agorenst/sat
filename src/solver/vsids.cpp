#include "vsids.h"

#include <cassert>

vsids_t::vsids_t(const cnf_t &cnf, const trail_t &trail)
    : cnf(cnf), vars(cnf.var_range()), trail(trail) {
  activity.construct(max_variable(cnf));

  static_activity();
}

void vsids_t::static_activity() {
  clear_activity();
  for (auto cid : cnf) {
    for (auto l : cnf[cid]) {
      activity[var(l)] += bump;
    }
  }
}

void vsids_t::clear_activity() {
  std::fill(std::begin(activity), std::end(activity), 0.0f);
}

INLINESTATE
void vsids_t::clause_learned(const clause_t &c) {
  for (literal_t l : c) {
    activity[var(l)] += bump;
  }
  std::for_each(std::begin(activity), std::end(activity),
                [this](float &s) { s *= alpha; });
}

INLINESTATE
literal_t vsids_t::choose() const {
  variable_t c = 0;
  float a = -1;
  for (variable_t v : vars) {
    if (trail.varset.get(v)) continue;
    if (activity[v] >= a) {
      a = activity[v];
      c = v;
    }
  }
  if (!c) return 0;
  return trail.previously_assigned_literal(c);
}

float vsids_t::score(literal_t l) const { return activity[var(l)]; }
