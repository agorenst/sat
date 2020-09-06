#include "acids.h"
acids_t::acids_t(const cnf_t &cnf, const trail_t &trail)
    : cnf(cnf), trail(trail), vr(cnf.var_range()) {
  conflict_index = 0;
  variable_t max_var = max_variable(cnf);
  score.construct(max_var);
  std::fill(std::begin(score), std::end(score), 0);
}

void acids_t::clause_learned(const clause_t &c) {
  conflict_index++;
  for (literal_t l : c) {
    variable_t v = var(l);
    size_t s = score[v];
    // maybe do something less overflow-y?
    score[v] = (s + conflict_index) / 2;
  }
}

literal_t acids_t::choose() {
  size_t s = 0;
  variable_t r = 0;
  for (variable_t v : vr) {
    literal_t l = lit(v);
    if (!trail.literal_unassigned(l)) continue;
    if (r == 0) {
      r = v;
      s = score[v];
      continue;
    }
    if (s < score[v]) {
      r = v;
      s = score[v];
    }
  }
  if (!r) return 0;
  return trail.previously_assigned_literal(r);
}
