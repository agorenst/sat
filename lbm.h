#pragma once
#include "cnf.h"
#include "trail.h"
#include <algorithm>
#include <queue>
// Literal block distance, for glue clauses
// Keeps track of the metrics, too.

struct lbm_entry {
  size_t score;
  clause_id id;
};
static inline bool entry_cmp(const lbm_entry &e1, const lbm_entry &e2) {
  return e1.score < e2.score;
}

struct lbm_t {
  // Don't erase anything here!
  // std::priority_queue<lbm_entry> worklist;
  std::vector<lbm_entry> worklist;

  clause_map_t<size_t> lbm;
  size_t value_cache = 0;

  size_t max_size = 0;
  float growth = 1.05;
  float start_growth = 1.2;
  bool my_erasure = false;

  // We clean whenever we've since doubled in size.
  bool should_clean(const cnf_t &cnf);

  clause_set_t clean();
  void clean_worklist();

  size_t compute_value(const clause_t &c, const trail_t &trail) const;

  void push_value(const clause_t &c, const trail_t &trail);
  void flush_value(clause_id cid);

  lbm_t(const cnf_t &cnf);
};
