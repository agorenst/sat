#include "vsids.h"
#include <cassert>

vsids_t::vsids_t(const cnf_t& cnf, const trail_t& trail)
    : activity(cnf),
      lits(cnf.lit_range()),
      trail(trail) {
  std::fill(std::begin(activity), std::end(activity), 0.0);
  // std::fill(std::begin(polarity), std::end(polarity), false);
}

void vsids_t::clause_learned(const clause_t& c) {
  for (literal_t l : c) {
    activity[neg(l)] += bump;
    activity[l] += bump;
  }
  std::for_each(std::begin(activity), std::end(activity),
                [this](float& s) { s *= alpha; });
}

literal_t vsids_t::choose() const {
  literal_t c = 0;
  float v = -1;
  // std::cout << "[VSIDS][TRACE][VERBOSE] Activity: ";
  // for (literal_t l = - activity.max_var; l <= activity.max_var; l++) {
  // if (l == 0) continue;
  // std::cout << l << ": " << activity[l] << "; ";
  //}
  // std::cout << std::endl;
  for (literal_t l : lits) {
    if (!trail.literal_unassigned(l)) continue;
    // std::cout << "[VSIDS][TRACE][VERBOSE] Trying " << l << std::endl;
    if (activity[l] > v) {
      v = activity[l];
      c = l;
    }
  }
  return c;
}
