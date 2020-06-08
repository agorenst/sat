#pragma once
#include "trail.h"
#include "cnf.h"
#include <algorithm>
#include "clause_key_map.h"
#include <queue>
// Literal block distance, for glue clauses
// Keeps track of the metrics, too.

struct lbm_entry {
  size_t score;
  clause_id id;
};
static bool operator<(const lbm_entry& e1, const lbm_entry& e2) {
  return e1.score < e2.score;
}

struct lbm_t {
  // Don't erase anything here!
  //std::priority_queue<lbm_entry> worklist;
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
  template<typename R>
  void clean(R clause_remover) {
    size_t target_size = worklist.size() / 2;

    std::sort(std::begin(worklist), std::end(worklist));
    std::for_each(std::begin(worklist)+target_size, std::end(worklist), [&](auto& e) {
      clause_remover(e.id);
    });
    worklist.erase(std::begin(worklist)+target_size, std::end(worklist));
    /*
    while (worklist.size() > target_size) {
      lbm_entry e = worklist.top(); worklist.pop();
      clause_remover(e.id);
    }
    */
    max_size *= growth;
  }

  size_t compute_value(const clause_t& c, const trail_t& trail) const;

  void push_value(const clause_t& c, const trail_t& trail);
  void flush_value(clause_id cid);

  lbm_t(const cnf_t& cnf);
};
