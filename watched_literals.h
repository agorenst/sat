#pragma once
#include <forward_list>
#include <map>
#include <vector>

#include "clause_key_map.h"
#include "cnf.h"
#include "debug.h"
#include "literal_incidence_map.h"
#include "trail.h"
#include "unit_queue.h"

// This is ONLY for binary+ clauses, not unary or empty.
// The main entry points:
//  - watch_clause(cid): add a new clause to be watched
//  - literal_falsed(l): inform the machine that a new literal has been marked
//  false.
//                       This will add more things to the unit-prop queue, or
//                       report a conflict.

struct trace_t;
struct watcher_t {
  literal_t l1 = 0;
  literal_t l2 = 0;
};

struct watched_literals_t {
  cnf_t& cnf;
  trail_t& trail;
  unit_queue_t& units;
  const literal_map_t<trail_t::v_state_t>& litstate;
  literal_map_t<std::vector<std::pair<clause_id, literal_t>>>
      literals_to_watcher;
  clause_map_t<watcher_t> watched_literals;

  bool literal_true(literal_t l) {
    return litstate[l] == trail_t::v_state_t::var_true;
  }
  bool literal_false(literal_t l) {
    return litstate[l] == trail_t::v_state_t::var_false;
  }

  watched_literals_t(cnf_t& cnf, trail_t& t, unit_queue_t& q);
  void watch_clause(clause_id cid);
  void literal_falsed(literal_t l);

  bool clause_watched(clause_id cid);

  void remove_clause(clause_id cid);

  literal_t find_first_watcher(const clause_t& c);
  auto find_second_watcher(clause_t& c, literal_t o);
  auto find_next_watcher(clause_t& c, literal_t o);
  void print_watch_state();
  bool validate_state();
};

std::ostream& operator<<(std::ostream& o, const watched_literals_t& w);
