#include <algorithm>
#include "clause_key_map.h"
// Literal block distance, for glue clauses
// Keeps track of the metrics, too.
struct lbm_t {
  // Don't erase anything here!
  size_t last_original_key = 0;

  clause_map_t<size_t> lbm;
  size_t value_cache = 0;

  size_t max_size = 0;
  float growth = 1.2;
  float start_growth = 1.2;

  // We clean whenever we've since doubled in size.
  bool should_clean(const cnf_t& cnf) {
    return max_size <= cnf.live_clause_count();
  }

  std::vector<clause_id> clean(cnf_t& cnf) {
    SAT_ASSERT(std::is_sorted(std::begin(cnf), std::end(cnf)));
    // binary search to find the start of the range.
    auto to_filter =
        std::upper_bound(std::begin(cnf), std::end(cnf), last_original_key);

    SAT_ASSERT(to_filter != std::end(cnf));
    SAT_ASSERT(*to_filter > last_original_key);

    std::sort(to_filter, std::end(cnf),
              [&](clause_id a, clause_id b) { return lbm[a] < lbm[b]; });
#if 0
    std::cerr << "Remove sequence LBM:" << std::endl;
    std::for_each(to_filter, std::end(cnf), [&](clause_id k) {
                                              std::cerr << lbm[k] << " ";
                                            });
#endif

    auto amount_to_remove = std::distance(to_filter, std::end(cnf)) / 2;
    auto to_remove = to_filter + amount_to_remove;
    std::vector<clause_id> clauses_to_remove;
    clauses_to_remove.insert(std::end(clauses_to_remove), to_remove,
                             std::end(cnf));

    std::sort(to_filter, std::end(cnf));
    SAT_ASSERT(std::is_sorted(std::begin(cnf), std::end(cnf)));

    max_size *= growth;

    SAT_ASSERT(std::all_of(std::begin(clauses_to_remove),
                           std::end(clauses_to_remove),
                           [&](clause_id cid) { return contains(cnf, cid); }));

#if 0
    std::cerr << "REMOVING: ";
    std::for_each(std::begin(clauses_to_remove), std::end(clauses_to_remove), [&](clause_id k) {
                                                                                std::cerr << lbm[k] << " ";
                                                                              });
#endif

    return clauses_to_remove;
  }

  size_t compute_value(const clause_t& c, const trail_t& trail) const {
    std::vector<size_t> levels;
    for (literal_t l : c) {
      levels.push_back(trail.level(l));
    }
    std::sort(std::begin(levels), std::end(levels));
    auto ut = std::unique(std::begin(levels), std::end(levels));
    return std::distance(std::begin(levels), ut);
  }

  void push_value(const clause_t& c, const trail_t& trail) {
    SAT_ASSERT(value_cache == 0);
    value_cache = compute_value(c, trail);
  }
  void flush_value(clause_id cid) {
    SAT_ASSERT(value_cache != 0);
    lbm[cid] = value_cache;
    value_cache = 0;
  }

  void insert(const clause_t& c, clause_id cid, const trail_t& trail) {
    size_t v = compute_value(c, trail);
    lbm[cid] = v;
  }

  size_t get(clause_id cid) { return lbm[cid]; }

  lbm_t(const cnf_t& cnf) : lbm(cnf.live_clause_count()) {
    max_size = cnf.live_clause_count() * start_growth;
    last_original_key = *std::prev(std::end(cnf));
  }
};
