#include "lbm.h"
#include <vector>

bool lbm_t::should_clean(const cnf_t& cnf) {
  return max_size <= cnf.live_clause_count();
}

size_t lbm_t::compute_value(const clause_t& c, const trail_t& trail) const {
  static std::vector<bool> level_present(trail.level() + 1);
  level_present.resize(trail.level() + 1);
  std::fill(std::begin(level_present), std::end(level_present), false);
  for (literal_t l : c) {
    level_present[trail.level(l)] = true;
  }
  auto value =
      std::count(std::begin(level_present), std::end(level_present), true);
  return value;
}

void lbm_t::push_value(const clause_t& c, const trail_t& trail) {
  value_cache = compute_value(c, trail);
  SAT_ASSERT(c.size() == 0 || value_cache != 0);
}
void lbm_t::flush_value(clause_id cid) {
  SAT_ASSERT(value_cache != 0);
  // worklist.push({value_cache, cid});
  worklist.push_back({value_cache, cid});
  // value_cache = 0; this isn't needed, and is actually a problem.
}

lbm_t::lbm_t(const cnf_t& cnf) : lbm(cnf.live_clause_count()) {
  max_size = cnf.live_clause_count() * start_growth;
  last_original_key = *std::prev(std::end(cnf));
}

clause_set_t lbm_t::clean() {
  size_t target_size = worklist.size() / 2;

  std::sort(std::begin(worklist), std::end(worklist), entry_cmp);
  clause_set_t to_remove;
  std::for_each(std::begin(worklist) + target_size, std::end(worklist),
                [&](auto& e) {
                  to_remove.push_back(e.id);
                });
  std::sort(std::begin(to_remove), std::end(to_remove));

  worklist.erase(std::begin(worklist) + target_size, std::end(worklist));
  max_size *= growth;
  return to_remove;
}
