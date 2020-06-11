#pragma once
#include <algorithm>
#include <queue>
#include "clause_key_map.h"
#include "cnf.h"
#include "trail.h"
// Literal block distance, for glue clauses
// Keeps track of the metrics, too.

struct lbm_entry {
  size_t score;
  clause_id id;
};
static inline bool entry_cmp(const lbm_entry& e1, const lbm_entry& e2) {
  return e1.score < e2.score;
}

struct lbm_t {
  // Don't erase anything here!
  // std::priority_queue<lbm_entry> worklist;
  std::vector<lbm_entry> worklist;
  size_t last_original_key = 0;

  clause_map_t<size_t> lbm;
  size_t value_cache = 0;

  size_t max_size = 0;
  float growth = 1.2;
  float start_growth = 1.2;

  // We clean whenever we've since doubled in size.
  bool should_clean(const cnf_t& cnf);

  // Given a remove method...
  template <typename R>
  std::vector<clause_id> clean(R remove_clause) {
    size_t target_size = worklist.size() / 2;

    std::sort(std::begin(worklist), std::end(worklist), entry_cmp);
    std::vector<clause_id> to_remove;
    std::for_each(std::begin(worklist) + target_size, std::end(worklist),
                  [&](auto& e) {
                    // remove_clause(e.id);
                    to_remove.push_back(e.id);
                  });

    worklist.erase(std::begin(worklist) + target_size, std::end(worklist));
    max_size *= growth;
    return to_remove;
  }

  size_t compute_value(const clause_t& c, const trail_t& trail) const;

  void push_value(const clause_t& c, const trail_t& trail);
  void flush_value(clause_id cid);

  lbm_t(const cnf_t& cnf);
};
