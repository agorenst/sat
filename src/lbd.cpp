#include "lbd.h"
#include "solver.h"

#include <vector>

void lbd_t::install(solver_t &s) {
  // This is when we (may) clean the DB
  s.before_decision.add_listener([&](cnf_t &cnf) {
    if (should_clean(cnf)) {
      auto to_remove = clean();
      // don't remove things on the trail
      auto et = std::remove_if(
          std::begin(to_remove), std::end(to_remove),
          [&](clause_id cid) { return s.trail.uses_clause(cid); });

      // erase those
      while (std::end(to_remove) != et) to_remove.pop_back();

      for (auto cid : to_remove) s.remove_clause(cid);

      cnf.clean_clauses();
    }
  });

  // This is when we note the LBD of the learned clause:
  s.added_clause.add([&](const trail_t &trail, clause_id cid) {
    push_value(s.cnf[cid], trail);
    flush_value(cid);
  });
}

bool lbd_t::should_clean(const cnf_t &cnf) {
  return max_size <= cnf.live_clause_count();
}

bool lbd_t::remove(clause_id cid) {
  auto it = std::find_if(std::begin(worklist), std::end(worklist),
                         [&](lbd_entry &e) { return e.id == cid; });
  if (it != std::end(worklist)) {
    *it = *std::prev(std::end(worklist));
    worklist.pop_back();
    return true;
  }
  return false;
}

size_t lbd_t::compute_value(const clause_t &c, const trail_t &trail) const {
  static std::vector<char> level_present(trail.level() + 1);
  level_present.resize(trail.level() + 1);
  std::fill(std::begin(level_present), std::end(level_present), false);
  size_t value = 0;
  for (literal_t l : c) {
    if (!level_present[trail.level(l)]) {
      level_present[trail.level(l)] = true;
      value++;
    }
  }
  return value;
}

void lbd_t::push_value(const clause_t &c, const trail_t &trail) {
  value_cache = compute_value(c, trail);
  SAT_ASSERT(c.size() == 0 || value_cache != 0);
}
void lbd_t::flush_value(clause_id cid) {
  SAT_ASSERT(value_cache != 0);
  // worklist.push({value_cache, cid});
  worklist.push_back({value_cache, cid});
  // value_cache = 0; this isn't needed, and is actually a problem.
}

lbd_t::lbd_t(const cnf_t &cnf) {
  max_size = static_cast<size_t>(cnf.live_clause_count() * start_growth);
}

clause_set_t lbd_t::clean() {
  clean_worklist();
  size_t target_size = worklist.size() / 2;

  std::sort(std::begin(worklist), std::end(worklist), entry_cmp);
  clause_set_t to_remove;
  std::for_each(std::begin(worklist) + target_size, std::end(worklist),
                [&](auto &e) { to_remove.push_back(e.id); });
  std::sort(std::begin(to_remove), std::end(to_remove));

  worklist.erase(std::begin(worklist) + target_size, std::end(worklist));
  max_size = static_cast<size_t>(max_size * growth);
  return to_remove;
}

void lbd_t::clean_worklist() {
  auto new_end =
      std::remove_if(std::begin(worklist), std::end(worklist),
                     [](const lbd_entry &p) { return !p.id->is_alive; });
  worklist.erase(new_end, std::end(worklist));
}