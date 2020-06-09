#include "lbm.h"
#include <vector>

bool lbm_t::should_clean(const cnf_t& cnf) {
  return max_size <= cnf.live_clause_count();
}

size_t lbm_t::compute_value(const clause_t& c, const trail_t& trail) const {
  std::vector<bool> level_present(trail.level()+1);
  for (literal_t l : c) {
    level_present[trail.level(l)] = true;
  }
  return std::count(std::begin(level_present), std::end(level_present), true);
}

void lbm_t::push_value(const clause_t& c, const trail_t& trail) {
  SAT_ASSERT(value_cache == 0);
  value_cache = compute_value(c, trail);
}
void lbm_t::flush_value(clause_id cid) {
  SAT_ASSERT(value_cache != 0);
  //worklist.push({value_cache, cid});
  worklist.push_back({value_cache, cid});
  value_cache = 0;
}

lbm_t::lbm_t(const cnf_t& cnf) : lbm(cnf.live_clause_count()) {
  max_size = cnf.live_clause_count() * start_growth;
  last_original_key = *std::prev(std::end(cnf));
}